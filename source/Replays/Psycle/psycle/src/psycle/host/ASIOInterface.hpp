///\file
///\brief interface file for psycle::host::ASIOInterface.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "AudioDriver.hpp"
#include <asiodrivers.h>
#include <asio.h>
#include <map>
//#pragma comment(lib, "asio")
namespace psycle
{
	namespace host
	{
		#define MAX_ASIO_DRIVERS 32
		#define MAX_ASIO_OUTPUTS 128

		class ASIODriverSettings : public AudioDriverSettings
		{
		public:
			ASIODriverSettings();
			ASIODriverSettings(const ASIODriverSettings& othersettings);
			ASIODriverSettings& operator=(const ASIODriverSettings& othersettings);
			bool operator!=(ASIODriverSettings const &);
			bool operator==(ASIODriverSettings const & other) { return !((*this) != other); }
			virtual AudioDriver* NewDriver();
			virtual const AudioDriverInfo& GetInfo() const { return info_; };

			virtual void SetDefaultSettings();
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);

			unsigned int driverID;
		private:
			static AudioDriverInfo info_;
		};

		/// output device interface implemented by asio.
		class ASIOInterface : public AudioDriver
		{
		public:
			class DriverEnum;
			class PortEnum
			{
			public:
				PortEnum():_idx(0)
				{
					_info.channel=0;			_info.isInput=0;
					_info.isActive=0;			_info.channelGroup=0;
					_info.type=0;				memset(_info.name,0,sizeof(_info.name));
				}
				PortEnum(int idx,ASIOChannelInfo info):_idx(idx)
				{
					_info.channel=info.channel;		_info.isInput=info.isInput;
					_info.isActive=info.isActive;	_info.channelGroup=info.channelGroup;
					_info.type=info.type;			strcpy(_info.name,info.name);
				}
				std::string GetName() const;
				bool IsFormatSupported(DriverEnum* driver, int samplerate) const;
			public:
				int _idx;
				ASIOChannelInfo _info;
			};
			class DriverEnum
			{
			public:
				DriverEnum():minSamples(2048),maxSamples(2048),prefSamples(2048),granularity(0){};
				DriverEnum(std::string name):_name(name){};
				~DriverEnum(){};
				void AddInPort(PortEnum &port) { _portin.push_back(port); }
				void AddOutPort(PortEnum &port) { _portout.push_back(port); }
			public:
				std::string _name;
				std::vector<PortEnum> _portout;
				std::vector<PortEnum> _portin;
				long minSamples;
				long maxSamples;
				long prefSamples;
				long granularity;
			};
			class PortOut
			{
			public:
				PortOut():driver(0),port(0){};
				DriverEnum* driver;
				PortEnum* port;
			};
			class PortCapt : public PortOut
			{
			public:
				PortCapt():PortOut(),pleft(0),pright(0),machinepos(0){};
				float *pleft;
				float *pright;
				int machinepos;
			};
			class AsioStereoBuffer
			{
			public:
				AsioStereoBuffer()
				{	pleft[0]=0;	pleft[1]=0;	pright[0]=0;	pright[1]=0;_sampletype=0;	}
				AsioStereoBuffer(void** left,void**right,ASIOSampleType stype)
				{
					pleft[0]=left[0];pleft[1]=left[1];pright[0]=right[0];pright[1]=right[1];
					_sampletype=stype;
				}
				ASIOSampleType _sampletype;
				AsioStereoBuffer operator=(AsioStereoBuffer& buf)
				{
					_sampletype=buf._sampletype;
					pleft[0]=buf.pleft[0];
					pleft[1]=buf.pleft[1];
					pright[0]=buf.pright[0];
					pright[1]=buf.pright[1];
					return *this;
				}
				void *pleft[2];
				void *pright[2];
			};

		public:
			ASIOInterface(ASIODriverSettings* settings);
			virtual ~ASIOInterface() throw();
			inline virtual AudioDriverSettings& settings() const { return *settings_; };

			virtual void Initialize(AUDIODRIVERWORKFN pCallback, void* context);
			virtual bool Enable(bool e);
			virtual void Reset();
			inline virtual bool Initialized() const { return _initialized; };
			virtual bool Enabled() const { return _running; };
			virtual void Configure();
			virtual void RefreshAvailablePorts();
			virtual void GetPlaybackPorts(std::vector<std::string> &ports) const;
			virtual void GetCapturePorts(std::vector<std::string> &ports) const;
			virtual bool AddCapturePort(int idx);
			virtual bool RemoveCapturePort(int idx);
			virtual void GetReadBuffers(int idx, float **pleft, float **pright,int numsamples);
			virtual uint32_t GetWritePosInSamples() const;
			virtual uint32_t GetPlayPosInSamples();
			inline virtual uint32_t GetInputLatencySamples() const { return _inlatency; }
			inline virtual uint32_t GetOutputLatencySamples() const { return _outlatency; }

			DriverEnum GetDriverFromidx(int driverID) const;
			PortOut GetOutPortFromidx(int driverID);
			int GetidxFromOutPort(PortOut&port) const;
			void ControlPanel(int driverID);

			static bool SupportsAsio();
			static void bufferSwitch(long index, ASIOBool processNow);
			static ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow);
			static void sampleRateChanged(ASIOSampleRate sRate);
			static long asioMessages(long selector, long value, void* message, double* opt);

		protected:
			void Error(const char msg[]);
			bool Start();
			bool Stop();

			static AUDIODRIVERWORKFN _pCallback;
			static void* _pCallbackContext;

			ASIOCallbacks asioCallbacks;

		private:
			bool _initialized;
			bool _running;
			long _inlatency;
			long _outlatency;
			std::vector<DriverEnum> drivEnum_;

			static ASIODriverSettings* settings_;
			static AsioDrivers asioDrivers;

			static ::CCriticalSection _lock;
			static PortOut _selectedout;
			static std::vector<PortCapt> _selectedins;
			std::vector<int> _portMapping;
			static AsioStereoBuffer *ASIObuffers;
			static bool _firstrun;
			static bool _supportsOutputReady;
			static uint32_t writePos;
			static uint32_t m_wrapControl;
		};
	}
}
#else
namespace psycle
{
	namespace host
	{
		class ASIOInterface
		{
			static bool SupportsAsio() { return false; }
		};
	}
}
