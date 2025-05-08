///\file
///\brief interface file for psycle::host::AudioDriver.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "AudioDriver.hpp"
#include <audioclient.h>


#define MAX_STR_LEN 512

interface IMMDevice;
interface IMMDeviceEnumerator;
interface IMMDeviceCollection;

namespace psycle
{
	namespace host
	{
		class WasapiSettings : public AudioDriverSettings
		{
		public:
			WasapiSettings();
			WasapiSettings(const WasapiSettings& othersettings);
			WasapiSettings& operator=(const WasapiSettings& othersettings);
			virtual bool operator!=(WasapiSettings const &);
			virtual bool operator==(WasapiSettings const & other) { return !((*this) != other); }
			virtual AudioDriver* NewDriver();
			virtual const AudioDriverInfo& GetInfo() const { return info_; }

			virtual void SetDefaultSettings();
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
			bool shared;
			WCHAR szDeviceID[MAX_STR_LEN];
		private:
			static AudioDriverInfo info_;
		};

		/// output device interface.
		class WasapiDriver : public AudioDriver
		{
			class PortEnum
			{
			public:
				PortEnum() {};
				bool IsFormatSupported(WAVEFORMATEXTENSIBLE& pwfx, AUDCLNT_SHAREMODE sharemode) const;
				// from GetId
				WCHAR szDeviceID[MAX_STR_LEN];
				// from PropVariant
				char portName[MAX_STR_LEN];

				REFERENCE_TIME DefaultDevicePeriod;
				REFERENCE_TIME MinimumDevicePeriod;

				WAVEFORMATEXTENSIBLE MixFormat;
				bool isDefault;
			};
			// ------------------------------------------------------------------------------------------
			typedef struct PaWasapiSubStream
			{
				// Device
				WCHAR szDeviceID[MAX_STR_LEN];
				IMMDevice			*device;
				IAudioClient        *client;
				WAVEFORMATEXTENSIBLE wavex;
				UINT32               bufferFrameCount;
				REFERENCE_TIME       device_latency;
				REFERENCE_TIME       period;
				AUDCLNT_SHAREMODE    shareMode;
				UINT32               streamFlags; // AUDCLNT_STREAMFLAGS_EVENTCALLBACK, ...
				UINT32               flags;
				float *pleft;
				float *pright;
				int _machinepos;
			}
			PaWasapiSubStream;
		public:
			WasapiDriver(WasapiSettings* settings);
			virtual ~WasapiDriver();
			virtual AudioDriverSettings& settings() const { return *settings_;}

			virtual void Initialize(AUDIODRIVERWORKFN pCallback, void* context);
			virtual bool Enable(bool e) { return e ? Start() : Stop(); }
			virtual void Reset(void);
			virtual void Configure(void);
			virtual bool Initialized(void) const { return _initialized; }
			virtual bool Enabled() const { return running; }
			virtual void GetPlaybackPorts(std::vector<std::string> &ports) const;
			virtual void GetCapturePorts(std::vector<std::string> &ports) const;
			virtual void GetReadBuffers(int idx, float **pleft, float **pright,int numsamples);
			virtual bool AddCapturePort(int idx);
			virtual bool RemoveCapturePort(int idx);
			virtual	void RefreshAvailablePorts();
			virtual uint32_t GetWritePosInSamples() const;
			virtual uint32_t GetPlayPosInSamples();
			virtual uint32_t GetInputLatencyMs() const;
			virtual uint32_t GetOutputLatencyMs() const;
			virtual uint32_t GetInputLatencySamples() const;
			virtual uint32_t GetOutputLatencySamples() const;
		private:
			static void Error(const char msg[]);
			static const char* GetError(HRESULT hr);
			void RefreshPorts(IMMDeviceEnumerator *pEnumerator);
			void FillPortList(std::vector<PortEnum>& portList, IMMDeviceCollection *pCollection, LPWSTR defaultID);
			uint32_t GetIdxFromDevice(WCHAR* szDeviceID) const;
			bool Start();
			bool Stop();
			static DWORD WINAPI EventAudioThread(void* pWasapi);
			HRESULT CreateCapturePort(IMMDeviceEnumerator* pEnumerator, PaWasapiSubStream &port);
			HRESULT GetStreamFormat(PaWasapiSubStream& stream, WAVEFORMATEXTENSIBLE* pwfx) const;
			HRESULT DoBlock(IAudioRenderClient *pRenderClient, int numFramesAvailable);
			HRESULT DoBlockRecording(PaWasapiSubStream& port, IAudioCaptureClient *pCaptureClient, int numFramesAvailable);

			WasapiSettings* settings_;
			static AudioDriverEvent _event;
			void* _callbackContext;
			AUDIODRIVERWORKFN _pCallback;
			
			bool _initialized;
			bool _configured;
		public:
			std::vector<PortEnum> _playEnums;
			std::vector<PortEnum> _capEnums;
		private:
			std::vector<PaWasapiSubStream> _capPorts;
			std::vector<int> _portMapping;
			// output
			PaWasapiSubStream out;
			IAudioClock* pAudioClock;
			UINT64 audioClockFreq;
			uint32_t writeMark;

			// must be volatile to avoid race condition on user query while
			// thread is being started
			volatile bool running;

			DWORD dwThreadId;
		};
	}
}


namespace portaudio{
	// ------------------------------------------------------------------------------------------
	// Aligns v backwards
	static inline UINT32 ALIGN_BWD(UINT32 v, UINT32 align)
	{
		return ((v - (align ? v % align : 0)));
	}
	// ------------------------------------------------------------------------------------------
	static inline double nano100ToMillis(const REFERENCE_TIME &ref)
	{
		// 1 nano = 0.000000001 seconds
		//100 nano = 0.0000001 seconds
		//100 nano = 0.0001 milliseconds
		return ((double)ref)*0.0001;
	}
	// ------------------------------------------------------------------------------------------
	static inline REFERENCE_TIME MillisTonano100(const double &ref)
	{
		// 1 nano = 0.000000001 seconds
		//100 nano = 0.0000001 seconds
		//100 nano = 0.0001 milliseconds
		return (REFERENCE_TIME)(ref/0.0001);
	}
	// ------------------------------------------------------------------------------------------
	// Makes Hns period from frames and sample rate
	static inline REFERENCE_TIME MakeHnsPeriod(UINT32 nFrames, DWORD nSamplesPerSec)
	{
		return (REFERENCE_TIME)((10000.0 * 1000 / nSamplesPerSec * nFrames) + 0.5);
	}
	// ------------------------------------------------------------------------------------------
	// Aligns WASAPI buffer to 128 byte packet boundary. HD Audio will fail to play if buffer
	// is misaligned. This problem was solved in Windows 7 were AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED
	// is thrown although we must align for Vista anyway.
	static inline UINT32 AlignFramesPerBuffer(UINT32 nFrames, UINT32 nSamplesPerSec, UINT32 nBlockAlign)
	{
#define HDA_PACKET_SIZE 128

		long packets_total = 10000 * (nSamplesPerSec * nBlockAlign / HDA_PACKET_SIZE);
		long frame_bytes = nFrames * nBlockAlign;

		if(frame_bytes % HDA_PACKET_SIZE != 0) {
			//Ensure we round up, not down
			frame_bytes = ALIGN_BWD(frame_bytes+HDA_PACKET_SIZE, HDA_PACKET_SIZE);
		}

		// align to packet size
		nFrames = frame_bytes / nBlockAlign;
		long packets = frame_bytes / HDA_PACKET_SIZE;

		// align to packets count
		while (packets && ((packets_total % packets) != 0))
		{
			//round up
			++packets;
		}

		frame_bytes = packets * HDA_PACKET_SIZE;
		nFrames = frame_bytes / nBlockAlign;

		return nFrames;
	}
	// ------------------------------------------------------------------------------------------
	// Converts Hns period into number of frames
	static inline UINT32 MakeFramesFromHns(REFERENCE_TIME hnsPeriod, UINT32 nSamplesPerSec)
	{
		UINT32 nFrames = (UINT32)( // frames =
			1.0 * hnsPeriod * // hns *
			nSamplesPerSec / // (frames / s) /
			1000 / // (ms / s) /
			10000 // (hns / s) /
			+ 0.5 // rounding
			);
		return nFrames;
	}
}