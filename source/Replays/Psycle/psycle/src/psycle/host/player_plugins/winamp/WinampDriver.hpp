///\file
///\brief interface file for psycle::host::WinampDriver.
#pragma once
#include <psycle/host/detail/project.hpp>
#include <psycle/host/Global.hpp>
#include <psycle/host/AudioDriver.hpp>
#include <Winamp/in2.h>

namespace psycle
{
	namespace host
	{
		class WinampSettings : public AudioDriverSettings
		{
		public:
			WinampSettings();
			WinampSettings(const WinampSettings& othersettings);
			WinampSettings& operator=(const WinampSettings& othersettings);
			virtual bool operator!=(WinampSettings const &);
			virtual bool operator==(WinampSettings const & other) { return !((*this) != other); }
			virtual AudioDriver* NewDriver();
			virtual AudioDriverInfo& GetInfo() const { return info_; }

			virtual void SetDefaultSettings();
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
		private:
			static AudioDriverInfo info_;
		};

		/// output device interface implemented by Winamp
		class WinampDriver : public AudioDriver
		{
		public:
			WinampDriver(WinampSettings* settings);
			WinampDriver(WinampSettings* settings, In_Module* themod);
			virtual ~WinampDriver() throw();
			inline virtual AudioDriverSettings& settings() const { return *settings_; };

			virtual void Initialize(AUDIODRIVERWORKFN pCallback, void * context);
			virtual bool Enable(bool e);
			virtual void Reset() { Enable(false); }
			virtual bool Initialized() const { return _initialized; }
			virtual bool Enabled() const { return thread_handle != INVALID_HANDLE_VALUE; }
			virtual std::uint32_t GetOutputLatencyMs() const { return outputlatency; };
			virtual std::uint32_t GetInputLatencySamples() const { return 0; };
			virtual std::uint32_t GetOutputLatencySamples() const;

			void SetSamplesPerSec(int samples);
			void Pause(bool dopause);
			bool Paused() const { return paused; }
		protected:
			void Error(const TCHAR msg[]);
			bool Start();
			bool Stop();

			static DWORD WINAPI __stdcall PlayThread(void *b);

		private:
			WinampSettings* settings_;
			In_Module* inmod;
			void* context;
			bool _initialized;
			int outputlatency;

			int killDecodeThread;
			HANDLE thread_handle;
			bool paused;
			bool worked;
		};
	}
}