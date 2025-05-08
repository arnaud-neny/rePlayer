///\file
///\brief interface file for psycle::host::FoobarDriver.
#pragma once
#include <psycle/host/detail/project.hpp>
#include <psycle/host/Global.hpp>
#include <psycle/host/AudioDriver.hpp>

class audio_chunk;

namespace psycle
{
	namespace host
	{
		class FoobarSettings : public AudioDriverSettings
		{
		public:
			FoobarSettings();
			FoobarSettings(const FoobarSettings& othersettings);
			FoobarSettings& operator=(const FoobarSettings& othersettings);
			virtual bool operator!=(FoobarSettings const &);
			virtual bool operator==(FoobarSettings const & other) { return !((*this) != other); }
			virtual AudioDriver* NewDriver();
			virtual AudioDriverInfo& GetInfo() const { return info_; }

			virtual void SetDefaultSettings();
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
		private:
			static AudioDriverInfo info_;
		};

		/// output device interface implemented by Foobar
		class FoobarDriver : public AudioDriver
		{
		public:
			FoobarDriver(FoobarSettings* settings);
			virtual ~FoobarDriver() throw();
			inline virtual AudioDriverSettings& settings() const { return *settings_; };

			virtual void Initialize(AUDIODRIVERWORKFN pCallback, void * context) {};
			virtual bool Enable(bool e);
			virtual void Reset() { Enable(false); }
			virtual bool Initialized() const { return true; }
			virtual bool Enabled() const { return true; }
			virtual std::uint32_t GetInputLatencySamples() const { return 0; };
			virtual std::uint32_t GetOutputLatencySamples() const { return 0; };

			void SetSamplesPerSec(int samples);
			bool decode_run(audio_chunk & p_chunk,Player* pPlayer);

		protected:
			void Error(const TCHAR msg[]);

		private:
			FoobarSettings* settings_;
		};
	}
}