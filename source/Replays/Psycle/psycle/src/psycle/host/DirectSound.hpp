///\file
///\interface psycle::host::DirectSound.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "AudioDriver.hpp"

#define DIRECTSOUND_VERSION 0x8000
#include <dsound.h>
#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
	#pragma comment(lib, "dsound")
	#pragma comment(lib, "dxguid")
#endif

#include <map>
namespace psycle
{
	namespace host
	{

		class DirectSoundSettings : public AudioDriverSettings
		{
		public:
			DirectSoundSettings();
			DirectSoundSettings(const DirectSoundSettings& othersettings);
			DirectSoundSettings& operator=(const DirectSoundSettings& othersettings);
			virtual bool operator!=(DirectSoundSettings const &);
			virtual bool operator==(DirectSoundSettings const & other) { return !((*this) != other); }
			virtual AudioDriver* NewDriver();
			virtual const AudioDriverInfo& GetInfo() const { return info_; }

			virtual void SetDefaultSettings();
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
			GUID device_guid_;
		private:
			static AudioDriverInfo info_;
		};

		/// output device interface implemented by direct sound.
		class DirectSound : public AudioDriver
		{
			class PortEnums
			{
			public:
				PortEnums():guid(0) {};
				PortEnums(LPGUID _guid,std::string _pname):guid(_guid),portname(_pname){}
				bool IsFormatSupported(WAVEFORMATEXTENSIBLE& pwfx, bool isInput) const;
				std::string portname;
				LPGUID guid;
			};
			class PortCapt
			{
			public:
				PortCapt():pleft(0),pright(0),_pGuid(0),_pDs(0),_pBuffer(0),_lowMark(0),_machinepos(0) {};

				LPGUID _pGuid;
				LPDIRECTSOUNDCAPTURE8 _pDs;
				LPDIRECTSOUNDCAPTUREBUFFER8  _pBuffer;
				int _lowMark;
				float *pleft;
				float *pright;
				int _machinepos;
			};
		public:
			DirectSound(DirectSoundSettings* settings);
			virtual ~DirectSound() throw();
			inline virtual AudioDriverSettings& settings() const { return *settings_; };

			virtual void Initialize(AUDIODRIVERWORKFN pCallback, void * context);
			virtual bool Enable(bool e);
			virtual void Reset();
			virtual void Configure();
			virtual bool Initialized() const { return _initialized; }
			virtual bool Enabled() const { return _running; }
			virtual void RefreshAvailablePorts();
			virtual void GetPlaybackPorts(std::vector<std::string> &ports) const;
			virtual void GetCapturePorts(std::vector<std::string> &ports) const;
			virtual bool AddCapturePort(int idx);
			virtual bool RemoveCapturePort(int idx);
			virtual void GetReadBuffers(int idx, float **pleft, float **pright,int numsamples);
			virtual uint32_t GetInputLatencySamples() const { return _dsBufferSize/GetSampleSizeBytes(); }
			virtual uint32_t GetOutputLatencySamples() const { return _dsBufferSize/GetSampleSizeBytes(); }
			virtual uint32_t GetWritePosInSamples() const;
			virtual uint32_t GetPlayPosInSamples();

			bool CreateCapturePort(PortCapt &port);
			static BOOL CALLBACK DSEnumCallback(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext);
			static BOOL CALLBACK DSCaptureEnumCallback(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext);
		protected:
			static void Error(const TCHAR msg[]);
			bool Start();
			bool Stop();

			uint32_t DirectSound::GetIdxFromDevice(GUID* device) const;
			static DWORD WINAPI NotifyThread(void* pDirectSound);
			static DWORD WINAPI PollerThread(void* pDirectSound);
			void DoBlocks();
			bool WantsMoreBlocks();
			void DoBlocksRecording(PortCapt& port);

			//Reposition the write block before the play cursor.
			void RepositionMark(int &low, int pos);

		private:
			bool _initialized;

			//controls if the driver is supposed to be running or not
			bool _running;
			//informs the real state of the DSound buffer (see the control of buffer play in DoBlocks())
			bool _playing;
			//Controls if we want the thread to be running or not
			bool _threadRun;
			DirectSoundSettings* settings_;
			static AudioDriverEvent _event;
			CCriticalSection _lock;

			uint32_t _dsBufferSize;
			uint32_t _lowMark;
			uint32_t _highMark;
			/// number of "wraparounds" to compensate the GetCurrentPosition() call.
			int m_readPosWraps;

			std::vector<PortEnums> _playEnums;
			std::vector<PortEnums> _capEnums;
			std::vector<PortCapt> _capPorts;
			std::vector<int> _portMapping;

			LPDIRECTSOUND8 _pDs;
			LPDIRECTSOUNDBUFFER8 _pBuffer;
			void* _callbackContext;
			AUDIODRIVERWORKFN _pCallback;
		};
	}
}
