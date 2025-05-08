///\file
///\brief implementation file for psycle::host::ASIOInterface.

#include <psycle/host/detail/project.private.hpp>
#include "ASIOInterface.hpp"
#include "ASIOConfig.hpp"
#include "ConfigStorage.hpp"
#include "MidiInput.hpp"
#include <psycle/helpers/dsp.hpp>
#include <universalis/os/aligned_alloc.hpp>

namespace psycle
{
	namespace host
	{
		#define ASIO_VERSION 2L
		using namespace helpers;
		//#define ALLOW_NON_ASIO

		extern CPsycleApp theApp;

		// note: asio drivers will tell us their preferred settings with : ASIOGetBufferSize
		AudioDriverInfo ASIODriverSettings::info_("ASIO 2.3 Output");
		ASIODriverSettings* ASIOInterface::settings_;
		CCriticalSection ASIOInterface::_lock;
		AsioDrivers ASIOInterface::asioDrivers;
		ASIOInterface::AsioStereoBuffer *ASIOInterface::ASIObuffers(0);
		bool ASIOInterface::_firstrun(true);
		bool ASIOInterface::_supportsOutputReady(false);
		ASIOInterface::PortOut ASIOInterface::_selectedout;
		std::vector<ASIOInterface::PortCapt> ASIOInterface::_selectedins;
		uint32_t ASIOInterface::writePos(0);
		uint32_t ASIOInterface::m_wrapControl(0);

		AUDIODRIVERWORKFN ASIOInterface::_pCallback(0);
		void* ASIOInterface::_pCallbackContext(0);


		ASIODriverSettings::ASIODriverSettings()
		{
			SetDefaultSettings();
		}
		ASIODriverSettings::ASIODriverSettings(const ASIODriverSettings& othersettings)
			: AudioDriverSettings(othersettings)
		{
			SetDefaultSettings();
		}
		ASIODriverSettings& ASIODriverSettings::operator=(const ASIODriverSettings& othersettings)
		{
			AudioDriverSettings::operator=(othersettings);
			return *this;
		}
		bool ASIODriverSettings::operator!=(ASIODriverSettings const &othersettings)
		{
			return
				AudioDriverSettings::operator!=(othersettings) ||
				driverID != othersettings.driverID;
		}
		AudioDriver* ASIODriverSettings::NewDriver()
		{
			return new ASIOInterface(this);
		}

		void ASIODriverSettings::SetDefaultSettings()
		{
			AudioDriverSettings::SetDefaultSettings();
			driverID=0;
		}

		void ASIODriverSettings::Load(ConfigStorage & store)
		{
			driverID = 0;
			if(
				// First, try current path since version 1.8.8
				store.OpenGroup("devices\\asio") ||
				// else, resort to the old path used from versions 1.8.0 to 1.8.6 included
				store.OpenGroup("configuration--1.8\\devices\\asio")
			) {
				unsigned int block;
				unsigned int samples;
				bool ok = store.Read("BufferSize", block);
				ok &= store.Read("DriverID", driverID);
				ok &= store.Read("SamplesPerSec", samples);
				store.CloseGroup();
				if (ok) {
					setBlockFrames(block);
					setSamplesPerSec(samples);
				}
			}
		}
		void ASIODriverSettings::Save(ConfigStorage & store)
		{
			store.CreateGroup("devices\\asio");
			store.Write("BufferSize", blockFrames());
			store.Write("DriverID", driverID);
			store.Write("SamplesPerSec", samplesPerSec());
			store.CloseGroup();
		}

		std::string ASIOInterface::PortEnum::GetName() const
		{
			std::string fullname = _info.name;
			switch(_info.type)
			{
			case ASIOSTInt16LSB:
			case ASIOSTInt32LSB16:		// 32 bit data with 16 bit alignment
			case ASIOSTInt16MSB:
			case ASIOSTInt32MSB16:		// 32 bit data with 16 bit alignment
				fullname = fullname + " : 16 bit";
				break;
			case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment
				fullname = fullname + " : 18 bit";
				break;

			case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment
			case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment
				fullname = fullname + " : 20 bit";
				break;
			case ASIOSTInt24LSB:		// used for 20 bits as well
			case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment
			case ASIOSTInt24MSB:		// used for 20 bits as well
			case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment
				fullname = fullname + " : 24 bit";
				break;
			case ASIOSTInt32LSB:
			case ASIOSTInt32MSB:
				fullname = fullname + ": 32 bit";
				break;
			case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
			case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, Big Endian architecture
				fullname = fullname + ": 32 bit float";
				break;
			case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
			case ASIOSTFloat64MSB: 		// IEEE 754 64 bit double float, Big Endian architecture
				fullname = fullname + ": 64 bit float";
				break;
			default:
				fullname = fullname + ": unsupported!";
				break;
			}
			return fullname;
		}

		void ASIOInterface::Error(const char msg[])
		{
			MessageBox(0, msg, "ASIO 2.2 Output driver", MB_OK | MB_ICONERROR);
		}

		ASIOInterface::ASIOInterface(ASIODriverSettings* settings)
		{
			_initialized = false;
			_running = false;
			_firstrun = true;
			_pCallback = 0;
			settings_ = settings;
		}
		ASIOInterface::~ASIOInterface() throw()
		{
			if(_initialized) Reset();
			//ASIOExit();
			asioDrivers.removeCurrentDriver();
		}
		bool ASIOInterface::SupportsAsio()
		{
			char szNameBuf[MAX_ASIO_DRIVERS][33];
			char* pNameBuf[MAX_ASIO_DRIVERS];
			for(int i(0); i < MAX_ASIO_DRIVERS; ++i) pNameBuf[i] = szNameBuf[i];
			return asioDrivers.getDriverNames((char**)pNameBuf,MAX_ASIO_DRIVERS);
		}
		void ASIOInterface::Initialize(AUDIODRIVERWORKFN pcallback, void* context)
		{
			_pCallbackContext = context;
			_pCallback = pcallback;
			_running = false;
			RefreshAvailablePorts();
			_initialized = true;
		}


		void ASIOInterface::RefreshAvailablePorts()
		{
			bool isPlaying = _running;
			if (isPlaying) Stop();
			char szNameBuf[MAX_ASIO_DRIVERS][33];
			char* pNameBuf[MAX_ASIO_DRIVERS];
			for(int i(0); i < MAX_ASIO_DRIVERS; ++i) pNameBuf[i] = szNameBuf[i];
			int drivers = asioDrivers.getDriverNames((char**)pNameBuf,MAX_ASIO_DRIVERS);
			drivEnum_.resize(0);
			ASIODriverInfo driverInfo;
			for(int i(0) ; i < drivers; ++i)
			{
				#if !defined ALLOW_NON_ASIO
					if(std::strcmp("ASIO DirectX Driver",szNameBuf[i])==0) continue;
					if(std::strcmp("ASIO DirectX Full Duplex Driver",szNameBuf[i])==0) continue;
					if(std::strcmp("ASIO Multimedia Driver",szNameBuf[i])==0) continue;
				#endif
				memset(&driverInfo,0,sizeof(ASIODriverInfo));
				driverInfo.asioVersion=ASIO_VERSION;
				driverInfo.sysRef = theApp.m_pMainWnd->m_hWnd;
				if(asioDrivers.loadDriver(szNameBuf[i]))
				{
					// initialize the driver
					if (ASIOInit(&driverInfo) == ASE_OK)
					{
						DriverEnum driver(szNameBuf[i]);
						TRACE("%s\n",szNameBuf[i]);
						long in,out;
						if(ASIOGetChannels(&in,&out) == ASE_OK)
						{
							// += 2 because we pair them in stereo
							for(int j(0) ; j < out ; j += 2)
							{
								ASIOChannelInfo channelInfo;
								channelInfo.isInput = ASIOFalse;
								channelInfo.channel = j;
								ASIOGetChannelInfo(&channelInfo);
								PortEnum port(j,channelInfo);
								driver.AddOutPort(port);
							}
							// += 2 because we pair them in stereo
							for(int j(0) ; j < in ; j += 2)
							{
								ASIOChannelInfo channelInfo;
								channelInfo.isInput = ASIOTrue;
								channelInfo.channel = j;
								ASIOGetChannelInfo(&channelInfo);
								PortEnum port(j,channelInfo);
								driver.AddInPort(port);
							}
						}
						ASIOGetBufferSize(&driver.minSamples, &driver.maxSamples, &driver.prefSamples, &driver.granularity);
						drivEnum_.push_back(driver);
					}
					asioDrivers.removeCurrentDriver();
				}
			}
			if (isPlaying) Start();
		}
		void ASIOInterface::Reset()
		{
			Stop();
		}

		bool ASIOInterface::Start()
		{
			CSingleLock lock(&_lock, TRUE);
			_firstrun = true;
			if(_running) return true;
			if(_pCallback == 0)
			{
				_running = false;
				return false;
			}
			// BEGIN -  Code 
			asioDrivers.removeCurrentDriver();

			_selectedout = GetOutPortFromidx(settings_->driverID);
			if ( !_selectedout.driver) _selectedout = GetOutPortFromidx(0);
			if ( _selectedout.driver) {
				if(settings_->blockFrames() < _selectedout.driver->minSamples) 
						settings_->setBlockFrames(_selectedout.driver->prefSamples);
				else if(settings_->blockFrames() > _selectedout.driver->maxSamples) 
						settings_->setBlockFrames(_selectedout.driver->prefSamples);
			}


			char bla[128]; strcpy(bla,_selectedout.driver->_name.c_str());
			if(!asioDrivers.loadDriver(bla))
			{
				_running = false;
				return false;
			}
			// initialize the driver
			ASIODriverInfo driverInfo;
			memset(&driverInfo,0,sizeof(ASIODriverInfo));
			driverInfo.asioVersion=ASIO_VERSION;
			driverInfo.sysRef = theApp.m_pMainWnd->m_hWnd;
			if (ASIOInit(&driverInfo) != ASE_OK)
			{
				//ASIOExit();
				asioDrivers.removeCurrentDriver();
				_running = false;
				return false;
			}
			if(ASIOSetSampleRate(settings_->samplesPerSec()) != ASE_OK)
			{
				settings_->setSamplesPerSec(44100);
				if(ASIOSetSampleRate(settings_->samplesPerSec()) != ASE_OK)
				{
					//ASIOExit();
					asioDrivers.removeCurrentDriver();
					_running = false;
					return false;
				}
			}
			if(ASIOOutputReady() == ASE_OK) _supportsOutputReady = true;
			else _supportsOutputReady = false;
			// set up the asioCallback structure and create the ASIO data buffer
			asioCallbacks.bufferSwitch = &bufferSwitch;
			asioCallbacks.sampleRateDidChange = &sampleRateChanged;
			asioCallbacks.asioMessage = &asioMessages;
			asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;
			//////////////////////////////////////////////////////////////////////////
			// Create the buffers to play and record.
			int numbuffers = (int)(1+_selectedins.size())*2;
			ASIOBufferInfo *info = new ASIOBufferInfo[numbuffers];
			int counter=0;
			for (unsigned int i(0); i < _selectedins.size() ; ++i)
			{
				info[counter].isInput = info[counter+1].isInput = _selectedins[i].port->_info.isInput;
				info[counter].channelNum = _selectedins[i].port->_idx;
				info[counter+1].channelNum = _selectedins[i].port->_idx + 1;
				info[counter].buffers[0] = info[counter].buffers[1] = info[counter+1].buffers[0] = info[counter+1].buffers[1] = 0;
				counter+=2;
			}
			info[counter].isInput = info[counter+1].isInput = _selectedout.port->_info.isInput;
			info[counter].channelNum = _selectedout.port->_idx;
			info[counter+1].channelNum = _selectedout.port->_idx + 1;
			info[counter].buffers[0] = info[counter].buffers[1] = info[counter+1].buffers[0] = info[counter+1].buffers[1] = 0;
			// create and activate buffers
			if(ASIOCreateBuffers(info,numbuffers,settings_->blockFrames(),&asioCallbacks) != ASE_OK)
			{
				//ASIOExit();
				asioDrivers.removeCurrentDriver();
				_running = false;
				return false;
			}
			ASIObuffers =  new AsioStereoBuffer[_selectedins.size()+1];
			counter=0;
			unsigned int i(0);
			for (; i < _selectedins.size() ; ++i)
			{
				AsioStereoBuffer buffer(info[counter].buffers,info[counter+1].buffers,_selectedins[i].port->_info.type);
				ASIObuffers[i] = buffer;
				// 2* is a safety measure (Haven't been able to dig out why it crashes if it is exactly the size)
				universalis::os::aligned_memory_alloc(16, _selectedins[i].pleft, 2 * settings_->blockFrames()* sizeof(float) );
				universalis::os::aligned_memory_alloc(16, _selectedins[i].pright, 2 * settings_->blockFrames() *sizeof(float));
				counter+=2;
			}
			AsioStereoBuffer buffer(info[counter].buffers,info[counter+1].buffers,_selectedout.port->_info.type);
			ASIObuffers[i] = buffer;

			ASIOGetLatencies(&_inlatency,&_outlatency);
			if(ASIOStart() != ASE_OK)
			{
				ASIODisposeBuffers();
				//ASIOExit();
				asioDrivers.removeCurrentDriver();
				_running = false;
				return false;
			}
			// END -  CODE
			_running = true;
			writePos = 0;
			m_wrapControl = 0;
			PsycleGlobal::midi().ReSync(); // MIDI IMPLEMENTATION
			delete[] info;
			return true;
		}

		bool ASIOInterface::Stop()
		{
			CSingleLock lock(&_lock, TRUE);
			if(!_running) return true;
			_running = false;
			ASIOStop();
			ASIODisposeBuffers();
			for (unsigned int i(0); i < _selectedins.size() ; ++i)
			{
				universalis::os::aligned_memory_dealloc(_selectedins[i].pleft);
				universalis::os::aligned_memory_dealloc(_selectedins[i].pright);
			}

			delete[] ASIObuffers;
			//ASIOExit();
			asioDrivers.removeCurrentDriver();
			return true;
		}
		void ASIOInterface::GetPlaybackPorts(std::vector<std::string> &ports) const
		{
			ports.resize(0);
			for (unsigned int j=0;j<drivEnum_.size();++j) {
				for (unsigned int i=0;i<drivEnum_[j]._portout.size();++i) {
					std::ostringstream stream;
					stream << drivEnum_[j]._name << " "
						<< drivEnum_[j]._portout[i].GetName();
					ports.push_back(stream.str());
				}
			}
		}

		void ASIOInterface::GetCapturePorts(std::vector<std::string> &ports) const
		{
			ports.resize(0);
			DriverEnum *driver =  _selectedout.driver;
			if (!driver) return;
			for (unsigned int i=0;i<driver->_portin.size();i++) ports.push_back(driver->_portin[i].GetName());
		}
		bool ASIOInterface::AddCapturePort(int idx)
		{
			bool isplaying = _running;
			DriverEnum *driver = _selectedout.driver;
			if ( idx >= driver->_portin.size()) return false;
			if ( idx < _portMapping.size() && _portMapping[idx] != -1) return true;
			if (isplaying) Stop();
			PortCapt port;
			port.driver = driver;
			port.port = &driver->_portin[idx];
			_selectedins.push_back(port);
			if ( _portMapping.size() <= idx) {
				int oldsize = _portMapping.size();
				_portMapping.resize(idx+1);
				for(int i=oldsize;i<_portMapping.size();i++) _portMapping[i]=-1;
			}
			_portMapping[idx]=(int)(_selectedins.size()-1);
			if (isplaying) return Start();

			return true;
		}
		bool ASIOInterface::RemoveCapturePort(int idx)
		{
			bool isplaying = _running;
			int maxSize = 0;
			std::vector<PortCapt> newports;
			DriverEnum *driver = _selectedout.driver;
			if ( idx >= driver->_portin.size() || 
				 idx >= _portMapping.size() || _portMapping[idx] == -1) return false;

			if (isplaying) Stop();
			for (unsigned int i=0;i<_portMapping.size();++i)
			{
				if (i != idx && _portMapping[i] != -1) {
					maxSize=i+1;
					newports.push_back(_selectedins[_portMapping[i]]);
					_portMapping[i]= (int)(newports.size()-1);
				}
			}
			_portMapping[idx] = -1;
			if(maxSize < _portMapping.size()) _portMapping.resize(maxSize);
			_selectedins = newports;
			if (isplaying) Start();
			return true;
		}
		void ASIOInterface::GetReadBuffers(int idx,float **pleft, float **pright,int numsamples)
		{
			if (!_running || idx >=_portMapping.size() || _portMapping[idx] == -1)
			{
				*pleft=0;
				*pright=0;
				return;
			}
			int mpos = _selectedins[_portMapping[idx]].machinepos;
			*pleft=_selectedins[_portMapping[idx]].pleft+mpos;
			*pright=_selectedins[_portMapping[idx]].pright+mpos;
			_selectedins[_portMapping[idx]].machinepos+=numsamples;
		}
		ASIOInterface::DriverEnum ASIOInterface::GetDriverFromidx(int driverID) const
		{
			int counter=0;
			for (unsigned int i(0); i < drivEnum_.size(); ++i)
			{
				if ( driverID < counter+drivEnum_[i]._portout.size())
				{
					return drivEnum_[i];
				}
				counter+=(int)(drivEnum_[i]._portout.size());
			}
			DriverEnum driver;
			return driver;
		}
		ASIOInterface::PortOut ASIOInterface::GetOutPortFromidx(int driverID)
		{
			PortOut port;
			int counter=0;
			for (unsigned int i(0); i < drivEnum_.size(); ++i)
			{
				if ( driverID < counter+drivEnum_[i]._portout.size())
				{
					port.driver = &drivEnum_[i];
					port.port = &drivEnum_[i]._portout[driverID-counter];
					return port;
				}
				counter+=(int)(drivEnum_[i]._portout.size());
			}
			return port;
		}
		int ASIOInterface::GetidxFromOutPort(PortOut&port) const
		{
			int counter=0;
			for (unsigned int i(0); i < drivEnum_.size(); ++i)
			{
				if ( &drivEnum_[i] == port.driver )
				{
					return counter+(port.port->_idx/2);

				}
				counter+=(int)(drivEnum_[i]._portout.size());
			}
			return 0;
		}

		void ASIOInterface::Configure()
		{
			CASIOConfig dlg;
			dlg.pASIO = this;
			dlg.m_driverIndex = settings_->driverID;
			dlg.m_sampleRate = settings_->samplesPerSec();
			dlg.m_bufferSize = settings_->blockFrames();
			if(dlg.DoModal() != IDOK) return;
			Enable(false);
			PortOut port = GetOutPortFromidx(dlg.m_driverIndex);
			bool supported = port.port->IsFormatSupported(port.driver,dlg.m_sampleRate);
			if(!supported) {
				Error("The Format selected is not supported. Keeping the previous configuration");
				return;
			}
			_selectedout = port;
			settings_->driverID = dlg.m_driverIndex;
			settings_->setSamplesPerSec(dlg.m_sampleRate);
			settings_->setBlockFrames(dlg.m_bufferSize);
			Enable(true);
		}

		bool ASIOInterface::Enable(bool e)
		{
			return e ? Start() : Stop();
		}

		uint32_t ASIOInterface::GetPlayPosInSamples()
		{
			if(!_running) return 0;
			return writePos - GetOutputLatencySamples();
		}

		uint32_t ASIOInterface::GetWritePosInSamples() const
		{
			if(!_running) return 0;
			return writePos;
		}

		bool ASIOInterface::PortEnum::IsFormatSupported(DriverEnum* driver, int samplerate) const
		{
			if(!asioDrivers.loadDriver(const_cast<char*>(driver->_name.c_str())))
			{
				return false;
			}
			// initialize the driver
			ASIODriverInfo driverInfo;
			memset(&driverInfo,0,sizeof(ASIODriverInfo));
			driverInfo.asioVersion=ASIO_VERSION;
			driverInfo.sysRef = theApp.m_pMainWnd->m_hWnd;
			if (ASIOInit(&driverInfo) != ASE_OK)
			{
				//ASIOExit();
				asioDrivers.removeCurrentDriver();
				return false;
			}
			ASIOError error = ASIOCanSampleRate(samplerate);
			if (error == ASE_NoClock) {
				asioDrivers.removeCurrentDriver();
				return false;
			}
			else if(error != ASE_OK && ASIOSetSampleRate(samplerate) != ASE_OK)
			{
				asioDrivers.removeCurrentDriver();
				return false;
			}
			asioDrivers.removeCurrentDriver();
			return true;
		}

		void ASIOInterface::ControlPanel(int driverID)
		{
			PortOut pout = GetOutPortFromidx(driverID);
			DriverEnum* newdriver = pout.driver;
			if(_selectedout.driver == newdriver && _running)
			{
				ASIOControlPanel(); //you might want to check wether the ASIOControlPanel() can open
			}
			else
			{
				bool isPlaying = _running;
				if(isPlaying) Stop();
				// load it
				if(asioDrivers.loadDriver(const_cast<char*>(newdriver->_name.c_str())))
				{
					ASIOControlPanel(); // you might want to check wether the ASIOControlPanel() can open
					asioDrivers.removeCurrentDriver();
				}
				if(isPlaying) Start();
			}
		}

		#define SwapDouble(v) SwapLongLong((long long)(v))
		#define SwapFloat(v) SwapLong(long(v))
		#define SwapLongLong(v) ((((v)>>56)&0xFF)|(((v)>>40)&0xFF00)|(((v)>>24)&0xFF0000)|(((v)>>8)&0xFF000000) \
						       | (((v)&0xFF)<<56)|(((v)&0xFF00)<<40)|(((v)&0xFF0000)<<24)|(((v)&0xFF000000)<<8))
		#define SwapLong(v) ((((v)>>24)&0xFF)|(((v)>>8)&0xFF00)|(((v)&0xFF00)<<8)|(((v)&0xFF)<<24)) 
		#define SwapShort(v) ((((v)>>8)&0xFF)|(((v)&0xFF)<<8))

		//ADVICE: Remember that psycle uses the range +32768.f to -32768.f. All conversions are done relative to this.
		ASIOTime *ASIOInterface::bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow)
		{
			// the actual processing callback.
			// Beware that this is normally in a seperate thread, hence be sure that you take care
			// about thread synchronization.
			if(_firstrun)
			{
				universalis::cpu::exceptions::install_handler_in_thread();
				_firstrun = false;
			}
			writePos = timeInfo->timeInfo.samplePosition.lo;
			if (timeInfo->timeInfo.samplePosition.hi != m_wrapControl) {
				m_wrapControl = timeInfo->timeInfo.samplePosition.hi;
				PsycleGlobal::midi().ReSync();	// MIDI IMPLEMENTATION
			}

			const unsigned int _ASIObufferSamples = settings_->blockFrames();
			//////////////////////////////////////////////////////////////////////////
			// Inputs
			unsigned int counter(0);
			for (; counter< _selectedins.size(); ++counter)
			{
				ASIOSampleType dtype =_selectedins[counter].port->_info.type;
				int i(0);
				switch (dtype)
				{
				case ASIOSTInt16LSB:
					{
						short* inl;
						short* inr;
						inl = (short*)ASIObuffers[counter].pleft[index];
						inr = (short*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = *inl;
							_selectedins[counter].pright[i] = *inr;
						}
					}
					break;
				case ASIOSTInt24LSB:		// used for 20 bits as well
					{
						char* inl;
						char* inr;
						inl = (char*)ASIObuffers[counter].pleft[index];
						inr = (char*)ASIObuffers[counter].pright[index];
						int t;
						char* pt = (char*)&t;
						for (i = 0; i < _ASIObufferSamples; i++)
						{
							pt[0] = *inl++;
							pt[1] = *inl++;
							pt[2] = *inl++;
							_selectedins[counter].pleft[i] = t*0.00390625f;

							pt[0] = *inr++;
							pt[1] = *inr++;
							pt[2] = *inr++;
							_selectedins[counter].pright[i] = t*0.00390625f;

						}
					}
					break;
				case ASIOSTInt32LSB:
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = (*inl)*0.0000152587890625;
							_selectedins[counter].pright[i] = (*inr)*0.0000152587890625;
						}
					}
					break;
				case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
					{
						helpers::dsp::MovMul(static_cast<float*>(ASIObuffers[counter].pleft[index]),_selectedins[counter].pleft,_ASIObufferSamples,32768.0f);
						helpers::dsp::MovMul(static_cast<float*>(ASIObuffers[counter].pright[index]),_selectedins[counter].pright,_ASIObufferSamples,32768.0f);
					}
					break;
				case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
					{
						double* inl;
						double* inr;
						inl = (double*)ASIObuffers[counter].pleft[index];
						inr = (double*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = (*inl)*32768.0;
							_selectedins[counter].pright[i] = (*inr)*32768.0;
						}
					}
					break;
					// these are used for 32 bit data buffer, with different alignment of the data inside
					// 32 bit PCI bus systems can more easily used with these
				case ASIOSTInt32LSB16:		// 32 bit data with 16 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = *inl;
							_selectedins[counter].pright[i] = *inr;
						}
					}
					break;
				case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = (*inl)*0.25f;
							_selectedins[counter].pright[i] = (*inr)*0.25f;
						}
					}
					break;
				case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = (*inl)*0.0625f;
							_selectedins[counter].pright[i] = (*inr)*0.0625f;
						}
					}
					break;
				case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = (*inl)*0.00390625f;
							_selectedins[counter].pright[i] = (*inr)*0.00390625f;
						}
					}
					break;
				case ASIOSTInt16MSB:
					{
						short* inl;
						short* inr;
						inl = (short*)ASIObuffers[counter].pleft[index];
						inr = (short*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = SwapShort(*inl);
							_selectedins[counter].pright[i] = SwapShort(*inr);
						}
					}
					break;
				case ASIOSTInt24MSB:		// used for 20 bits as well
					{
						char* inl;
						char* inr;
						inl = (char*)ASIObuffers[counter].pleft[index];
						inr = (char*)ASIObuffers[counter].pright[index];
						int t;
						char* pt = (char*)&t;
						for (i = 0; i < _ASIObufferSamples; i++)
						{
							pt[2] = *inl++;
							pt[1] = *inl++;
							pt[0] = *inl++;
							_selectedins[counter].pleft[i] = t*0.00390625f;

							pt[2] = *inr++;
							pt[1] = *inr++;
							pt[0] = *inr++;
							_selectedins[counter].pright[i] = t*0.00390625f;

						}
					}
					break;
				case ASIOSTInt32MSB:
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							long val = SwapLong(*inl++);
							_selectedins[counter].pleft[i] = val *0.0000152587890625;
							val = SwapLong(*inr++);
							_selectedins[counter].pright[i] = val *0.0000152587890625;
						}
					}
					break;
				case ASIOSTInt32MSB16:		// 32 bit data with 16 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = SwapLong(*inl);
							_selectedins[counter].pright[i] = SwapLong(*inr);
						}
					}
					break;
				case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							long val = SwapLong(*inl);
							_selectedins[counter].pleft[i] = val*0.25f;
							val = SwapLong(*inr);
							_selectedins[counter].pright[i] = val*0.25f;
						}
					}
					break;
				case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							long val = SwapLong(*inl);
							_selectedins[counter].pleft[i] = val*0.0625f;
							val = SwapLong(*inr);
							_selectedins[counter].pright[i] = val*0.0625f;
						}
					}
					break;
				case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment (right aligned)
					{
						long* inl;
						long* inr;
						inl = (long*)ASIObuffers[counter].pleft[index];
						inr = (long*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							long val = SwapLong(*inl);
							_selectedins[counter].pleft[i] = val*0.00390625f;
							val = SwapLong(*inr);
							_selectedins[counter].pright[i] = val*0.00390625f;
						}
					}
					break;
				case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, as found on PowerPC implementation
					{
						float* inl;
						float* inr;
						inl = (float*)ASIObuffers[counter].pleft[index];
						inr = (float*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = SwapFloat(*inl);
							_selectedins[counter].pright[i] = SwapFloat(*inr);
						}
					}
					break;
				case ASIOSTFloat64MSB: 		// IEEE 754 64 bit float, as found on PowerPC implementation
					{
						double* inl;
						double* inr;
						inl = (double*)ASIObuffers[counter].pleft[index];
						inr = (double*)ASIObuffers[counter].pright[index];
						for (i = 0; i < _ASIObufferSamples; i++,inl++,inr++)
						{
							_selectedins[counter].pleft[i] = SwapDouble(*inl);
							_selectedins[counter].pright[i] = SwapDouble(*inr);
						}
					}
					break;
				}
				_selectedins[counter].machinepos=0;
			}

			//////////////////////////////////////////////////////////////////////////
			// Outputs
			float *pBuf = _pCallback(_pCallbackContext, _ASIObufferSamples);
			switch (_selectedout.port->_info.type)
			{
			case ASIOSTInt16LSB:
				{
				int16_t* outl;
				int16_t* outr;
				outl = (int16_t*)ASIObuffers[counter].pleft[index];
				outr = (int16_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = rint_clip<int16_t>(*pBuf++);
					*outr++ = rint_clip<int16_t>(*pBuf++);
					}
				}
				break;
			case ASIOSTInt24LSB:		// used for 20 bits as well
				{
					char* outl;
					char* outr;
					outl = (char*)ASIObuffers[counter].pleft[index];
					outr = (char*)ASIObuffers[counter].pright[index];
					int t;
					char* pt = (char*)&t;
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					t = rint_clip<int, 24>((*pBuf++) * 256.0f);
						*outl++ = pt[0];
						*outl++ = pt[1];
						*outl++ = pt[2];

					t = rint_clip<int, 24>((*pBuf++) * 256.0f);
						*outr++ = pt[0];
						*outr++ = pt[1];
						*outr++ = pt[2];
					}
				}

				break;
			case ASIOSTInt32LSB:
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				// Don't really know why, but the -100 is what made the clipping work correctly.
				int const max((1u << ((sizeof(int32_t) << 3) - 1)) - 100);
				int const min(-max - 1);
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = psycle::helpers::math::rint<int32_t>(psycle::helpers::math::clip(float(min), (*pBuf++) * 65536.0f, float(max)));
					*outr++ = psycle::helpers::math::rint<int32_t>(psycle::helpers::math::clip(float(min), (*pBuf++) * 65536.0f, float(max)));
					//*outl++ = rint_clip<int32_t>((*pBuf++) * 65536.0f);
					//*outr++ = rint_clip<int32_t>((*pBuf++) * 65536.0f);
					}
				}
				break;
			case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				{
					float* outl;
					float* outr;
					outl = (float*)ASIObuffers[counter].pleft[index];
					outr = (float*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = (*pBuf++) / 32768.0f;
					*outr++ = (*pBuf++) / 32768.0f;
					}
				}
				break;
			case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				{
					double* outl;
					double* outr;
					outl = (double*)ASIObuffers[counter].pleft[index];
					outr = (double*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = (*pBuf++) / 32768.0;
					*outr++ = (*pBuf++) / 32768.0;
					}
				}
				break;
				// these are used for 32 bit data buffer, with different alignment of the data inside
				// 32 bit PCI bus systems can more easily used with these
		case ASIOSTInt32LSB16: // 32 bit data with 16 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = rint_clip<int32_t, 16>(*pBuf++);
					*outr++ = rint_clip<int32_t, 16>(*pBuf++);
					}
				}
				break;
			case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = rint_clip<int32_t, 18>((*pBuf++) * 4.0f);
					*outr++ = rint_clip<int32_t, 18>((*pBuf++) * 4.0f);
					}
				}
				break;
			case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = rint_clip<int32_t, 20>((*pBuf++) * 16.0f);
					*outr++ = rint_clip<int32_t, 20>((*pBuf++) * 16.0f);
					}
				}
				break;
			case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = rint_clip<int32_t, 24>((*pBuf++) * 256.0f);
					*outr++ = rint_clip<int32_t, 24>((*pBuf++) * 256.0f);
					}
				}
				break;
			case ASIOSTInt16MSB:
				{
				int16_t* outl;
				int16_t* outr;
				outl = (int16_t*)ASIObuffers[counter].pleft[index];
				outr = (int16_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = SwapShort(rint_clip<int16_t>(*pBuf++));
					*outr++ = SwapShort(rint_clip<int16_t>(*pBuf++));
					}
				}
				break;
			case ASIOSTInt24MSB:		// used for 20 bits as well
				{
					char* outl;
					char* outr;
					outl = (char*)ASIObuffers[counter].pleft[index];
					outr = (char*)ASIObuffers[counter].pright[index];
					int t;
					char* pt = (char*)&t;
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					t = rint_clip<int, 24>((*pBuf++) * 256.0f);
						*outl++ = pt[2];
						*outl++ = pt[1];
						*outl++ = pt[0];

					t = rint_clip<int, 24>((*pBuf++) * 256.0f);
						*outr++ = pt[2];
						*outr++ = pt[1];
						*outr++ = pt[0];
					}
				}
				break;
			case ASIOSTInt32MSB:
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					//See the LSB case above.
					int const max((1u << ((sizeof(int32_t) << 3) - 1)) - 100);
					int const min(-max - 1);
					*outl++ = SwapLong(psycle::helpers::math::rint<int32_t>(psycle::helpers::math::clip(float(min), (*pBuf++) * 65536.0f, float(max))));
					*outr++ = SwapLong(psycle::helpers::math::rint<int32_t>(psycle::helpers::math::clip(float(min), (*pBuf++) * 65536.0f, float(max))));
					//*outl++ = SwapLong(rint_clip<int32_t>((*pBuf++) * 65536.0f));
					//*outr++ = SwapLong(rint_clip<int32_t>((*pBuf++) * 65536.0f));
					}
				}
				break;
			case ASIOSTInt32MSB16:		// 32 bit data with 16 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = SwapLong((rint_clip<int32_t, 16>(*pBuf++)));
					*outr++ = SwapLong((rint_clip<int32_t, 16>(*pBuf++)));
					}
				}
				break;
			case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = SwapLong((rint_clip<int32_t, 18>((*pBuf++) * 4.0f)));
					*outr++ = SwapLong((rint_clip<int32_t, 18>((*pBuf++) * 4.0f)));
					}
				}
				break;
			case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = SwapLong((rint_clip<int32_t, 20>((*pBuf++) * 16.0f)));
					*outr++ = SwapLong((rint_clip<int32_t, 20>((*pBuf++) * 16.0f)));
					}
				}
				break;
			case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment (right aligned)
				{
				int32_t* outl;
				int32_t* outr;
				outl = (int32_t*)ASIObuffers[counter].pleft[index];
				outr = (int32_t*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = SwapLong((rint_clip<int32_t, 24>((*pBuf++) * 256.0f)));
					*outr++ = SwapLong((rint_clip<int32_t, 24>((*pBuf++) * 256.0f)));
					}
				}
				break;
			case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, as found on PowerPC implementation
				{
				float* outl;
				float* outr;
				outl = (float*)ASIObuffers[counter].pleft[index];
				outr = (float*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = SwapFloat((*pBuf++) * 0.00030517578125);
					*outr++ = SwapFloat((*pBuf++) * 0.00030517578125);
					}
				}
				break;
			case ASIOSTFloat64MSB: 		// IEEE 754 64 bit double float, as found on PowerPC implementation
				{
				double* outl;
				double* outr;
				outl = (double*)ASIObuffers[counter].pleft[index];
				outr = (double*)ASIObuffers[counter].pright[index];
				for(int i = 0; i < _ASIObufferSamples; ++i) {
					*outl++ = SwapDouble((*pBuf++) * 0.00030517578125);
					*outr++ = SwapDouble((*pBuf++) * 0.00030517578125);
					}
				}
				break;
			}
			// finally if the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
			if(_supportsOutputReady) ASIOOutputReady();
			return 0;
		}

		void ASIOInterface::bufferSwitch(long index, ASIOBool processNow)
		{
			// the actual processing callback.
			// Beware that this is normally in a seperate thread, hence be sure that you take care
			// about thread synchronization. This is omitted here for simplicity.

			// as this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
			// though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the according flags
			ASIOTime  timeInfo;
			std::memset(&timeInfo, 0, sizeof timeInfo);

			// get the time stamp of the buffer, not necessary if no
			// synchronization to other media is required
			if(ASIOGetSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) == ASE_OK)
				timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

			bufferSwitchTimeInfo (&timeInfo, index, processNow);
		}


		void ASIOInterface::sampleRateChanged(ASIOSampleRate sRate)
		{
			// do whatever you need to do if the sample rate changed
			// usually this only happens during external sync.
			// Audio processing is not stopped by the driver, actual sample rate
			// might not have even changed, maybe only the sample rate status of an
			// AES/EBU or S/PDIF digital input at the audio device.
			// You might have to update time/sample related conversion routines, etc.
			settings_->setSamplesPerSec(sRate);
		}

		long ASIOInterface::asioMessages(long selector, long value, void* message, double* opt)
		{
			// currently the parameters "value", "message" and "opt" are not used.
			long ret = 0;
			switch(selector)
			{
				case kAsioSelectorSupported:
					if
						(
							value == kAsioResetRequest ||
							value == kAsioEngineVersion ||
							value == kAsioResyncRequest ||
							value == kAsioLatenciesChanged ||
							// the following three were added for ASIO 2.0, you don't necessarily have to support them
							value == kAsioSupportsTimeInfo ||
							value == kAsioSupportsTimeCode ||
							value == kAsioSupportsInputMonitor
						)
						ret = 1L;
					break;
				case kAsioResetRequest:
					// defer the task and perform the reset of the driver during the next "safe" situation
					// You cannot reset the driver right now, as this code is called from the driver.
					// Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(), Destruction
					// Afterwards you initialize the driver again.
					//asioDriverInfo.stopped;  // In this sample the processing will just stop
					ret = 1L;
					break;
				case kAsioResyncRequest:
					// This informs the application, that the driver encountered some non fatal data loss.
					// It is used for synchronization purposes of different media.
					// Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
					// Windows Multimedia system, which could loose data because the Mutex was hold too long
					// by another thread.
					// However a driver can issue it in other situations, too.
					ret = 1L;
					break;
				case kAsioLatenciesChanged:
					// This will inform the host application that the drivers were latencies changed.
					// Beware, it this does not mean that the buffer sizes have changed!
					// You might need to update internal delay data.
					ret = 1L;
					break;
				case kAsioEngineVersion:
					// return the supported ASIO version of the host application
					// If a host applications does not implement this selector, ASIO 1.0 is assumed
					// by the driver
					ret = ASIO_VERSION;
					break;
				case kAsioSupportsTimeInfo:
					// informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
					// is supported.
					// For compatibility with ASIO 1.0 drivers the host application should always support
					// the "old" bufferSwitch method, too.
					ret = 1;
					break;
				case kAsioSupportsTimeCode:
					// informs the driver wether application is interested in time code info.
					// If an application does not need to know about time code, the driver has less work
					// to do.
					ret = 0;
					break;
			}
			return ret;
		}
	}
}
