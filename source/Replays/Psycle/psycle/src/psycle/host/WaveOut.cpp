///\file
///\brief implementation file for psycle::host::WaveOut.

#include <psycle/host/detail/project.private.hpp>
#include "WaveOut.hpp"
#include "WaveOutDialog.hpp"
#include "MidiInput.hpp"
#include "ConfigStorage.hpp"

#include <universalis.hpp>
#include <universalis/os/thread_name.hpp>
#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/dsp.hpp>
#include <process.h>
#include "cpu_time_clock.hpp"

namespace psycle
{
	namespace host
	{
		AudioDriverInfo WaveOutSettings::info_("Windows WaveOut MME");
		AudioDriverEvent WaveOut::_event;

		void WaveOut::Error(char const msg[])
		{
			MessageBox(0, msg, "Windows WaveOut MME driver", MB_OK | MB_ICONERROR);
		}
		
		WaveOutSettings::WaveOutSettings()
		{
			SetDefaultSettings();
		}
		WaveOutSettings::WaveOutSettings(const WaveOutSettings& othersettings)
			: AudioDriverSettings(othersettings)
		{
			SetDefaultSettings();
		}
		WaveOutSettings& WaveOutSettings::operator=(const WaveOutSettings& othersettings)
		{
			AudioDriverSettings::operator=(othersettings);
			return *this;
		}
		bool WaveOutSettings::operator!=(WaveOutSettings const &other)
		{
			return AudioDriverSettings::operator!=(other) ||
				deviceID_ != other.deviceID_ ||
				pollSleep_ != other.pollSleep_;
		}
		
		AudioDriver* WaveOutSettings::NewDriver()
		{
			return new WaveOut(this);
		}

		void WaveOutSettings::SetDefaultSettings()
		{
			AudioDriverSettings::SetDefaultSettings();
			deviceID_=0;
			pollSleep_ = 10;
			setBlockCount(6);
			setBlockBytes(4096);
		}
		void WaveOutSettings::Load(ConfigStorage &store)
		{
			if(
				// First, try current path since version 1.8.8
				store.OpenGroup("devices\\mme") ||
				// else, resort to the old path used from versions 1.8.0 to 1.8.6 included
				store.OpenGroup("configuration--1.8\\devices\\mme")
			) {
				store.Read("DeviceID", deviceID_);
				store.Read("PollSleep", pollSleep_);
				unsigned int tmp = samplesPerSec();
				store.Read("SamplesPerSec", tmp);
				setSamplesPerSec(tmp);
				bool dodither = dither();
				store.Read("Dither", dodither);
				setDither(dodither);
				tmp = validBitDepth();
				store.Read("bitDepth", tmp);
				setValidBitDepth(tmp);
				tmp = blockCount();
				store.Read("NumBlocks", tmp);
				setBlockCount(tmp);
				tmp = blockBytes();
				store.Read("BlockSize", tmp);
				setBlockBytes(tmp);
				store.CloseGroup();
			}
		}
		void WaveOutSettings::Save(ConfigStorage &store)
		{
			store.CreateGroup("devices\\mme");
			store.Write("DeviceID", deviceID_);
			store.Write("PollSleep", pollSleep_);
			store.Write("SamplesPerSec", samplesPerSec());
			store.Write("Dither", dither());
			store.Write("bitDepth", validBitDepth());
			store.Write("NumBlocks", blockCount());
			store.Write("BlockSize", blockBytes());
			store.CloseGroup();
		}

		WaveOut::WaveOut(WaveOutSettings* settings)
			: settings_(settings)
			, _initialized(false)
			, _running(false)
			, _pCallback(0)
		{
			_capEnums.resize(0);
			_capPorts.resize(0);
		}

		void WaveOut::Initialize(AUDIODRIVERWORKFN pCallback, void * context)
		{
			_callbackContext = context;
			_pCallback = pCallback;
			_running = false;
			EnumerateCapturePorts();
			_initialized = true;
		}
		void WaveOut::RefreshAvailablePorts()
		{
			EnumeratePlaybackPorts();
			EnumerateCapturePorts();
		}
		void WaveOut::EnumeratePlaybackPorts()
		{
			_playEnums.resize(0);
			for (int i = 0; i < waveOutGetNumDevs(); i++)
			{
				WAVEOUTCAPS caps;
				waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS));
				PortEnums port(caps.szPname, i);
				_playEnums.push_back(port);
			}
		}
		void WaveOut::EnumerateCapturePorts()
		{
			_capEnums.resize(0);
			for (unsigned int i=0; i < waveInGetNumDevs(); ++i)
			{
				WAVEINCAPS caps;
				waveInGetDevCaps(i,&caps,sizeof(WAVEINCAPS));
				PortEnums port(caps.szPname, i);
				_capEnums.push_back(port);
			}
		}

		void WaveOut::Reset()
		{
			if (_running) Stop();
		}

		WaveOut::~WaveOut() throw()
		{
			if(_initialized) Reset();
		}

		bool WaveOut::Start()
		{
//			CSingleLock lock(&_lock, TRUE);
			if(_running) return true;
			if(!_pCallback) return false;

			WAVEFORMATPCMEX format;
			PrepareWaveFormat(format,settings_->numChannels(),settings_->samplesPerSec(),settings_->bitDepth(), settings_->validBitDepth());
			if(::waveOutOpen(&_handle, settings_->deviceID_, reinterpret_cast<LPWAVEFORMATEX>(&format), 0, 0, 0) != MMSYSERR_NOERROR)
			{
				Error("waveOutOpen() failed");
				return false;
			}

			_currentBlock = 0;
			_writePos = 0;
			m_readPosWraps = 0;
			m_lastPlayPos = 0;

			// allocate blocks
			unsigned int _blockSizeBytes = settings_->blockBytes();
			unsigned int _numBlocks = settings_->blockCount();
			for(CBlock *pBlock = _blocks; pBlock < _blocks + _numBlocks; pBlock++)
			{
				pBlock->Handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, _blockSizeBytes);
				pBlock->pData = (byte *)GlobalLock(pBlock->Handle);
			}

			// allocate block headers
			for(CBlock *pBlock = _blocks; pBlock < _blocks + _numBlocks; pBlock++)
			{
				pBlock->HeaderHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR));
				pBlock->pHeader = (WAVEHDR *)GlobalLock(pBlock->HeaderHandle);

				WAVEHDR *ph = pBlock->pHeader;
				ph->lpData = (char *)pBlock->pData;
				ph->dwBufferLength = _blockSizeBytes;
				ph->dwFlags = WHDR_DONE;
				ph->dwLoops = 0;

				pBlock->Prepared = false;
			}

			for (unsigned int i=0; i<_capPorts.size();i++)
				CreateCapturePort(_capPorts[i]);

			_stopPolling = false;
			_event.ResetEvent();
			::_beginthread(PollerThread, 0, this);
			_running = true;
			PsycleGlobal::midi().ReSync();	// MIDI IMPLEMENTATION
			return true;
		}

		bool WaveOut::Stop()
		{
//			CSingleLock lock(&_lock, TRUE);
			if(!_running) return true;
			_stopPolling = true;
			CSingleLock event(&_event, TRUE);
			unsigned int _numBlocks = settings_->blockCount();
			// Once we get here, the PollerThread should have stopped
			if(::waveOutReset(_handle) != MMSYSERR_NOERROR)
			{
				Error("waveOutReset() failed");
				return false;
			}
			for(;;)
			{
				bool alldone = true;
				for(CBlock *pBlock = _blocks; pBlock < _blocks + _numBlocks; pBlock++)
				{
					if((pBlock->pHeader->dwFlags & WHDR_DONE) == 0) alldone = false;
				}
				if(alldone) break;
				::Sleep(10);
			}
			for(CBlock *pBlock = _blocks; pBlock < _blocks + _numBlocks; pBlock++)
			{
				if(pBlock->Prepared)
				{
					if(::waveOutUnprepareHeader(_handle, pBlock->pHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
					{
						Error("waveOutUnprepareHeader() failed");
					}
				}
			}
			if(::waveOutClose(_handle) != MMSYSERR_NOERROR)
			{
				Error("waveOutClose() failed");
				return false;
			}
			for(CBlock *pBlock = _blocks; pBlock < _blocks + _numBlocks; pBlock++)
			{
				::GlobalUnlock(pBlock->Handle);
				::GlobalFree(pBlock->Handle);
				::GlobalUnlock(pBlock->HeaderHandle);
				::GlobalFree(pBlock->HeaderHandle);
			}
			for(unsigned int i=0; i<_capPorts.size(); i++)
			{
				if(_capPorts[i]._handle == NULL)
					continue;
				if(::waveInReset(_capPorts[i]._handle) != MMSYSERR_NOERROR)
				{
					Error("waveInReset() failed");
					return false;
				}
				///\todo: wait until WHDR_DONE like with waveout?
				for(CBlock *pBlock = _capPorts[i]._blocks; pBlock < _capPorts[i]._blocks + _numBlocks; pBlock++)
				{
					if(pBlock->Prepared)
					{
						if(::waveInUnprepareHeader(_capPorts[i]._handle, pBlock->pHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
						{
							Error("waveInUnprepareHeader() failed");
						}
					}
				}
				waveInClose(_capPorts[i]._handle);
				_capPorts[i]._handle = NULL;
				universalis::os::aligned_memory_dealloc(_capPorts[i].pleft);
				universalis::os::aligned_memory_dealloc(_capPorts[i].pright);
				for(CBlock *pBlock = _capPorts[i]._blocks; pBlock < _capPorts[i]._blocks + _numBlocks; pBlock++)
				{
					::GlobalUnlock(pBlock->Handle);
					::GlobalFree(pBlock->Handle);
					::GlobalUnlock(pBlock->HeaderHandle);
					::GlobalFree(pBlock->HeaderHandle);
				}
			}

			_running = false;
			return true;
		}
		void WaveOut::GetPlaybackPorts(std::vector<std::string> &ports) const
		{
			ports.resize(0);
			for (unsigned int i=0;i<_playEnums.size();i++) ports.push_back(_playEnums[i].portname);
		}
		void WaveOut::GetCapturePorts(std::vector<std::string> &ports) const
		{
			ports.resize(0);
			for (unsigned int i=0;i<_capEnums.size();i++) ports.push_back(_capEnums[i].portname);
		}
		bool WaveOut::AddCapturePort(int idx)
		{
			bool isplaying = _running;
			if ( idx >= _capEnums.size()) return false;
			if ( idx < _portMapping.size() && _portMapping[idx] != -1) return true;
			
			if (isplaying) Stop();
			PortCapt port;
			port._idx = idx;
			_capPorts.push_back(port);
			if ( _portMapping.size() <= idx) {
				int oldsize = _portMapping.size();
				_portMapping.resize(idx+1);
				for(int i=oldsize;i<_portMapping.size();i++) _portMapping[i]=-1;
			}
			_portMapping[idx]=(int)(_capPorts.size()-1);
			if (isplaying) return Start();

			return true;
		}
		bool WaveOut::RemoveCapturePort(int idx)
		{
			bool isplaying = _running;
			int maxSize = 0;
			std::vector<PortCapt> newports;
			if ( idx >= _capEnums.size() || 
				 idx >= _portMapping.size() || _portMapping[idx] == -1) return false;

			if (isplaying) Stop();
			for (unsigned int i=0;i<_portMapping.size();++i)
			{
				if (i != idx && _portMapping[i] != -1) {
					maxSize=i+1;
					newports.push_back(_capPorts[_portMapping[i]]);
					_portMapping[i]= (int)(newports.size()-1);
				}
			}
			_portMapping[idx] = -1;
			if(maxSize < _portMapping.size()) _portMapping.resize(maxSize);
			_capPorts = newports;
			if (isplaying) Start();
			return true;
		}
		bool WaveOut::CreateCapturePort(PortCapt &port)
		{
			HRESULT hr;
			//avoid opening a port twice
			if (port._handle) return true;

			WAVEFORMATPCMEX format;
			PrepareWaveFormat(format,settings_->numChannels(),settings_->samplesPerSec(),settings_->bitDepth(), settings_->validBitDepth());
			unsigned int _blockSizeBytes = settings_->blockBytes();
			unsigned int _numBlocks = settings_->blockCount();
			if ((hr = waveInOpen(&port._handle,port._idx,reinterpret_cast<LPWAVEFORMATEX>(&format),NULL,NULL,CALLBACK_NULL)) != MMSYSERR_NOERROR )
			{
				Error(_T("waveInOpen() failed"));
				return false;
			}
			// allocate blocks
			for(CBlock *pBlock = port._blocks; pBlock < port._blocks + _numBlocks; pBlock++)
			{
				pBlock->Handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, _blockSizeBytes);
				pBlock->pData = (byte *)GlobalLock(pBlock->Handle);
			}
			// allocate block headers
			for(CBlock *pBlock = port._blocks; pBlock < port._blocks + _numBlocks; pBlock++)
			{
				pBlock->HeaderHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR));
				pBlock->pHeader = (WAVEHDR *)GlobalLock(pBlock->HeaderHandle);

				WAVEHDR *ph = pBlock->pHeader;
				ph->lpData = (char *)pBlock->pData;
				ph->dwBufferLength = _blockSizeBytes;
				ph->dwFlags = WHDR_DONE;
				ph->dwLoops = 0;

				pBlock->Prepared = false;
			}
			universalis::os::aligned_memory_alloc(16, port.pleft, _blockSizeBytes);
			universalis::os::aligned_memory_alloc(16, port.pright, _blockSizeBytes);
			ZeroMemory(port.pleft, 2*_blockSizeBytes);
			ZeroMemory(port.pright, 2*_blockSizeBytes);
			port._machinepos=0;

			waveInStart(port._handle);

			return true;
		}

		void WaveOut::GetReadBuffers(int idx,float **pleft, float **pright,int numsamples)
		{
			if (!_running || idx >= _portMapping.size() || _portMapping[idx] == -1 
				|| _capPorts[_portMapping[idx]]._handle == NULL)
			{
				*pleft=0;
				*pright=0;
				return;
			}
			int mpos = _capPorts[_portMapping[idx]]._machinepos;
			*pleft=_capPorts[_portMapping[idx]].pleft+mpos;
			*pright=_capPorts[_portMapping[idx]].pright+mpos;
			_capPorts[_portMapping[idx]]._machinepos+=numsamples;
		}

		void WaveOut::PollerThread(void * pWaveOut)
		{
			universalis::os::thread_name thread_name("mme wave out");
			universalis::cpu::exceptions::install_handler_in_thread();
			WaveOut * pThis = reinterpret_cast<WaveOut*>(pWaveOut);
			::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
			unsigned int _numBlocks = pThis->settings_->blockCount();
			while(!pThis->_stopPolling)
			{
				CBlock *pb = pThis->_blocks + pThis->_currentBlock;
				int underruns=0;
				while(pb->pHeader->dwFlags & WHDR_DONE)
				{
					for (unsigned int i =0; i < pThis->_capPorts.size(); i++)
					{
						if(pThis->_capPorts[i]._handle == NULL)
							continue;
						pThis->DoBlocksRecording(pThis->_capPorts[i]);
					}
					pThis->DoBlocks();
					++pb;
					if(pb == pThis->_blocks + _numBlocks) pb = pThis->_blocks;
					if ( pb->pHeader->dwFlags & WHDR_DONE)
					{
						underruns++;
						if ( underruns > _numBlocks )
						{
							// Audio dropout most likely happened
							// (There's a possibility a dropout didn't happen, but the cpu usage
							// is almost at 100%, so we force an exit of the loop for a "Sleep()" call,
							// preventing psycle from being frozen.
							break;
						}
					}
					pThis->_currentBlock = pb - pThis->_blocks;
				}
				::Sleep(pThis->settings_->pollSleep_);
			}
			_event.SetEvent();
			::_endthread();
		}
		void WaveOut::DoBlocksRecording(PortCapt& port)
		{
			CBlock *pb = port._blocks + _currentBlock;

			if(pb->Prepared)
			{
				if(::waveInUnprepareHeader(port._handle, pb->pHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
				{
					Error("waveInUnprepareHeader() failed");
				}
				pb->Prepared = false;
			}

			WAVEHDR *ph = pb->pHeader;
			ph->lpData = (char *)pb->pData;
			ph->dwBufferLength = settings_->blockBytes();
			ph->dwFlags = WHDR_DONE;
			ph->dwLoops = 0;

			if(::waveInPrepareHeader(port._handle, pb->pHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			{
				Error("waveInPrepareHeader() failed");
			}
			pb->Prepared = true;
			waveInAddBuffer(port._handle, pb->pHeader, sizeof(WAVEHDR));
			// Put the audio in our float buffers.
			int numSamples = settings_->blockFrames();
			unsigned int _sampleValidBits = settings_->validBitDepth();
			if (_sampleValidBits == 32) {
				DeinterlaceFloat(reinterpret_cast<float*>(pb->pData), port.pleft,port.pright, numSamples);
			}
			else if (_sampleValidBits == 24) {
				DeQuantize32AndDeinterlace(reinterpret_cast<int*>(pb->pData), port.pleft,port.pright, numSamples);
			}
			else {
				DeQuantize16AndDeinterlace(reinterpret_cast<short int*>(pb->pData), port.pleft,port.pright, numSamples);
			}
			port._machinepos=0;
		}

		void WaveOut::DoBlocks()
		{
			CBlock *pb = _blocks + _currentBlock;
			if(pb->Prepared)
			{
				while (::waveOutUnprepareHeader(_handle, pb->pHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
				{
				}
				pb->Prepared = false;
			}
			int *pBlock = (int *)pb->pData;
			int numSamples = GetBufferSamples();
			unsigned int _sampleValidBits = settings_->validBitDepth();
			float * pFloatBlock = _pCallback(_callbackContext,numSamples);
			if(_sampleValidBits == 32) {
				dsp::MovMul(pFloatBlock, reinterpret_cast<float*>(pBlock), numSamples*2, 1.f/32768.f);
			}
			else if(_sampleValidBits == 24) {
				Quantize24in32Bit(pFloatBlock, pBlock, numSamples);
			}
			else if (_sampleValidBits == 16) {
				if(settings_->dither()) Quantize16WithDither(pFloatBlock, pBlock, numSamples);
				else Quantize16(pFloatBlock, pBlock, numSamples);
			}

			_writePos += numSamples;

			pb->pHeader->lpData = (char *)pb->pData;
			pb->pHeader->dwBufferLength = settings_->blockBytes();
			pb->pHeader->dwFlags = 0;
			pb->pHeader->dwLoops = 0;

			if(::waveOutPrepareHeader(_handle, pb->pHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			{
				Error("waveOutPrepareHeader() failed");
			}
			pb->Prepared = true;

			if(::waveOutWrite(_handle, pb->pHeader, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			{
				Error("waveOutWrite() failed");
			}
		}


		void WaveOut::Configure()
		{
			CWaveOutDialog dlg;
			dlg.m_device = settings_->deviceID_;
			dlg.m_sampleRate = settings_->samplesPerSec();
			dlg.m_bitDepth = settings_->validBitDepth();
			dlg.m_dither = settings_->dither();
			dlg.m_bufNum = settings_->blockCount();
			dlg.m_bufSamples = settings_->blockFrames();
			dlg.waveout = this;

			if(dlg.DoModal() != IDOK) return;
			Enable(false);
			WAVEFORMATPCMEX wf;
			PrepareWaveFormat(wf, 2, dlg.m_sampleRate, (dlg.m_bitDepth==24)?32:dlg.m_bitDepth, dlg.m_bitDepth);
			bool supported = _playEnums[dlg.m_device].IsFormatSupported(wf,false);
			if(!supported) {
				Error("The Format selected is not supported. Keeping the previous configuration");
				return;
			}

			settings_->deviceID_ = dlg.m_device;
			settings_->setSamplesPerSec(dlg.m_sampleRate);
			settings_->setValidBitDepth(dlg.m_bitDepth);
			settings_->setDither(dlg.m_dither);
			settings_->setBlockCount(dlg.m_bufNum);
			settings_->setBlockFrames(dlg.m_bufSamples);
			Enable(true);
		}

		uint32_t WaveOut::GetPlayPosInSamples()
		{
			// WARNING! waveOutGetPosition in TIME_SAMPLES has max of 0x7FFFFF for 16bit stereo signals.
			if(_stopPolling) return 0;
			MMTIME time;
			time.wType = TIME_SAMPLES;
			if(::waveOutGetPosition(_handle, &time, sizeof(MMTIME)) != MMSYSERR_NOERROR)
			{
				Error("waveOutGetPosition() failed");
			}
			if(time.wType != TIME_SAMPLES)
			{
				Error("waveOutGetPosition() doesn't support TIME_SAMPLES");
			}
			
			uint32_t retval = time.u.sample;
			// sample counter wrap around?
			if( m_lastPlayPos > retval)
			{
				m_readPosWraps++;
				if(m_lastPlayPos > _writePos) {
					m_readPosWraps = 0;
					PsycleGlobal::midi().ReSync();	// MIDI IMPLEMENTATION
				}
			}
			m_lastPlayPos = retval;
			return retval + (m_readPosWraps*0x800000);
		}

		uint32_t WaveOut::GetWritePosInSamples() const
		{
			if(_stopPolling) return 0;
			return _writePos;
		}

		bool WaveOut::Enable(bool e)
		{
			return e ? Start() : Stop();
		}

		bool WaveOut::PortEnums::IsFormatSupported(WAVEFORMATEXTENSIBLE& pwfx, bool isInput) const
		{ 
			if(isInput) {
				return MMSYSERR_NOERROR == waveInOpen(
					NULL,                 // ptr can be NULL for query 
					idx,            // the device identifier 
					reinterpret_cast<WAVEFORMATEX*>(&pwfx),                 // defines requested format 
					NULL,                 // no callback 
					NULL,                 // no instance data 
					WAVE_FORMAT_QUERY);  // query only, do not open device 
			}
			else {
				return MMSYSERR_NOERROR == waveOutOpen( 
					NULL,                 // ptr can be NULL for query 
					idx,            // the device identifier 
					reinterpret_cast<WAVEFORMATEX*>(&pwfx),                 // defines requested format 
					NULL,                 // no callback 
					NULL,                 // no instance data 
					WAVE_FORMAT_QUERY);  // query only, do not open device 
			}
		} 
	}
}
