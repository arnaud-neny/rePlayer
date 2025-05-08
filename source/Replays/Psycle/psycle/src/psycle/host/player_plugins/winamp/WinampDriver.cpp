///\file
///\brief implementation file for psycle::host::WinampDriver.

#include <psycle/host/detail/project.private.hpp>
#include "WinampDriver.hpp"
#include <psycle/host/ConfigStorage.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/helpers/math.hpp>

#define WA_STREAM_SIZE 576
// post this to the main window at end of file (after playback has stopped)
#define WM_WA_MPEG_EOF WM_USER+2


namespace psycle
{
	namespace host
	{
		int stream_buffer[WA_STREAM_SIZE*4]; // stereo, and double size (for special dsp plugins)

		AudioDriverInfo WinampSettings::info_("Winamp output plugin");

		void WinampDriver::Error(const TCHAR msg[])
		{
			MessageBox(0, msg, _T("Winamp Psycle Output driver"), MB_OK | MB_ICONERROR);
		}

		WinampSettings::WinampSettings()
		{
			SetDefaultSettings();
		}
		WinampSettings::WinampSettings(const WinampSettings& othersettings)
			: AudioDriverSettings(othersettings)
		{
			SetDefaultSettings();
		}
		WinampSettings& WinampSettings::operator=(const WinampSettings& othersettings)
		{
			AudioDriverSettings::operator=(othersettings);
			return *this;
		}
		bool WinampSettings::operator!=(WinampSettings const &othersettings)
		{
			return
				AudioDriverSettings::operator!=(othersettings);
		}

		AudioDriver* WinampSettings::NewDriver()
		{
			return new WinampDriver(this);
		}

		void WinampSettings::SetDefaultSettings()
		{
			AudioDriverSettings::SetDefaultSettings();
		}
		void WinampSettings::Load(ConfigStorage &store)
		{
			store.OpenGroup("devices\\winamp");
			unsigned int tmp = samplesPerSec();
			store.Read("SamplesPerSec", tmp);
			setSamplesPerSec(tmp);
			bool dodither = dither();
			store.Read("Dither", dodither);
			setDither(dodither);
			tmp = validBitDepth();
			store.Read("BitDepth", tmp);
			setValidBitDepth(tmp);
			store.CloseGroup();
		}
		void WinampSettings::Save(ConfigStorage &store)
		{
			store.CreateGroup("devices\\winamp");
			store.Write("SamplesPerSec", samplesPerSec());
			store.Write("Dither", dither());
			store.Write("BitDepth", validBitDepth());
			store.CloseGroup();
		}



		WinampDriver::WinampDriver(WinampSettings* settings)
			:context(NULL)
			,killDecodeThread(0)
			,thread_handle(INVALID_HANDLE_VALUE)
			,_initialized(false)
			,paused(false)
		{
			//This constructor is meant to host an In_Module itself, instead of 
			//relying on the one given in the other constructor
			///\todo: implement
		}
		WinampDriver::WinampDriver(WinampSettings* settings, In_Module* themod)
			:inmod(themod)
			,settings_(settings)
			,context(NULL)
			,killDecodeThread(0)
			,thread_handle(INVALID_HANDLE_VALUE)
			,_initialized(false)
			,paused(false)
		{
		}
		WinampDriver::~WinampDriver()
		{
			Stop();
		}

		void WinampDriver::Initialize(AUDIODRIVERWORKFN pCallback, void* context)
		{
			pCallback; // unused var
			this->context = context;
			_initialized=true;
		}
		
		void WinampDriver::Pause(bool e)
		{
			if (e) 
			{
				paused=true; inmod->outMod->Pause(1);
			}
			else
			{
				worked=false;
				paused=false; inmod->outMod->Pause(0);
			}
		}
		
		bool WinampDriver::Enable(bool e)
		{
			return  e ? Start() : Stop();
		}

		bool WinampDriver::Start()
		{
			if(Enabled()) return true;

			outputlatency = inmod->outMod->Open(settings_->samplesPerSec(),settings_->numChannels(),settings_->validBitDepth(), -1,-1);
			if (outputlatency < 0)
			{
				return false;
			}
			inmod->outMod->SetVolume(-666);

			unsigned long tmp;
			killDecodeThread=0;
			paused=0; worked=false;
			memset(stream_buffer,0,sizeof(stream_buffer));
			thread_handle = (HANDLE) CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) PlayThread,(void *) this,0,&tmp);
			return thread_handle != INVALID_HANDLE_VALUE;
		}

		bool WinampDriver::Stop()
		{
			if (Enabled())
			{
				killDecodeThread=1;
				if (WaitForSingleObject(thread_handle,INFINITE) == WAIT_TIMEOUT)
				{
					MessageBox(inmod->hMainWindow,"error asking thread to die!\n","error killing decode thread",0);
					TerminateThread(thread_handle,0);
				}
				CloseHandle(thread_handle);
				thread_handle = INVALID_HANDLE_VALUE;
				inmod->outMod->Close();
			}
			return true;
		}
		void WinampDriver::SetSamplesPerSec(int samples) 
		{
			bool isenabled = Enabled();
			if(isenabled) Enable(false);
			settings_->setSamplesPerSec(samples);
			if(isenabled) Enable(true);
		}
		
		std::uint32_t WinampDriver::GetOutputLatencySamples() const
		{
			return GetOutputLatencyMs()*settings_->samplesPerSec()*0.001f;
		}

		DWORD WINAPI __stdcall WinampDriver::PlayThread(void *b)
		{
			WinampDriver& wadriver = *reinterpret_cast<WinampDriver*>(b);
			In_Module* themod = wadriver.inmod;

			float *float_buffer;
			Player* pPlayer = static_cast<Player*>(wadriver.context);
			int samprate = wadriver.settings().samplesPerSec();
			int plug_stream_size = WA_STREAM_SIZE;
			::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
			AudioDriverSettings & settings = wadriver.settings();
			int _sampleValidBits = settings.validBitDepth();
			int sampleSize = settings.frameBytes();
			if(_sampleValidBits ==24) {
				//Correct for bits=24. Psycle uses 24bits in 32bits. winamp uses raw 24bits.
				sampleSize*=3.f/4.f;
			}
			while (!wadriver.killDecodeThread)
			{
				if ( !wadriver.worked)
				{
					if (pPlayer->_playing)
					{
						float_buffer = Player::Work(pPlayer,plug_stream_size);
						if(_sampleValidBits == 32) {
							Quantize24in32Bit(float_buffer, stream_buffer, plug_stream_size);
						}
						else if(_sampleValidBits == 24) {
							Quantize24LE(float_buffer, stream_buffer, plug_stream_size);
						}
						else if (_sampleValidBits == 16) {
							if(settings.dither()) Quantize16WithDither(float_buffer, stream_buffer, plug_stream_size);
							else Quantize16(float_buffer, stream_buffer, plug_stream_size);
						}
						wadriver.worked=true;
					}
					else
					{
						themod->outMod->CanWrite();
						if (!themod->outMod->IsPlaying())
						{
							PostMessage(themod->hMainWindow,WM_WA_MPEG_EOF,0,0);
							return 0;
						}
						Sleep(10);
					}
				}
				else if (themod->outMod->CanWrite() >= (plug_stream_size*sampleSize << (themod->dsp_isactive () ? 1 : 0)))
				{
					int t;
					if (themod->dsp_isactive()) t=themod->dsp_dosamples((short*)stream_buffer,plug_stream_size,
						settings.validBitDepth(),settings.numChannels(),samprate)
						*sampleSize;
					else t=plug_stream_size*sampleSize;

					int s=themod->outMod->GetWrittenTime();
					themod->SAAddPCMData((char*)stream_buffer,settings.numChannels(),settings.validBitDepth(),s);
					themod->VSAAddPCMData((char*)stream_buffer,settings.numChannels(),settings.validBitDepth(),s);

					themod->outMod->Write((char*)stream_buffer,t);

					wadriver.worked=false;
				}
				else {
					themod->SetInfo(pPlayer->bpm,-1,-1,1);
					Sleep(20);
				}

			}
			return 0;
		}
	}
}
