///\file
///\brief implementation file for psycle::host::FoobarDriver.

#include <psycle/host/detail/project.private.hpp>
#include <foobar2000/SDK/foobar2000.h>
#include "FoobarDriver.hpp"
#include <psycle/host/ConfigStorage.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/helpers/math.hpp>


namespace psycle
{
	namespace host
	{
		AudioDriverInfo FoobarSettings::info_ = { "Foobar output plugin" };

		void FoobarDriver::Error(const TCHAR msg[])
		{
			MessageBox(0, msg, _T("Foobar Psycle Output driver"), MB_OK | MB_ICONERROR);
		}

		FoobarSettings::FoobarSettings()
		{
			SetDefaultSettings();
		}
		FoobarSettings::FoobarSettings(const FoobarSettings& othersettings)
			: AudioDriverSettings(othersettings)
		{
			SetDefaultSettings();
		}
		FoobarSettings& FoobarSettings::operator=(const FoobarSettings& othersettings)
		{
			AudioDriverSettings::operator=(othersettings);
			return *this;
		}
		bool FoobarSettings::operator!=(FoobarSettings const &othersettings)
		{
			return
				AudioDriverSettings::operator!=(othersettings);
		}

		AudioDriver* FoobarSettings::NewDriver()
		{
			return new FoobarDriver(this);
		}

		void FoobarSettings::SetDefaultSettings()
		{
			AudioDriverSettings::SetDefaultSettings();
			//TODO
		}
		void FoobarSettings::Load(ConfigStorage &store)
		{
			store.OpenGroup("devices\\winamp");
			unsigned int tmp = samplesPerSec();
			store.Read("SamplesPerSec", tmp);
			setSamplesPerSec(tmp);
			store.CloseGroup();
		}
		void FoobarSettings::Save(ConfigStorage &store)
		{
			store.CreateGroup("devices\\winamp");
			store.Write("SamplesPerSec", samplesPerSec());
			store.CloseGroup();
		}



		FoobarDriver::FoobarDriver(FoobarSettings* settings)
		{
		}
		FoobarDriver::~FoobarDriver()
		{
			Enable(false);
		}
		
		bool FoobarDriver::Enable(bool e)
		{
			return  e;
		}

		void FoobarDriver::SetSamplesPerSec(int samples) 
		{
			bool isenabled = Enabled();
			if(isenabled) Enable(false);
			settings_->setSamplesPerSec(samples);
			if(isenabled) Enable(true);
		}
		
		bool FoobarDriver::decode_run(audio_chunk & p_chunk,Player* pPlayer)
		{
			float *float_buffer;
			int samprate = settings_->samplesPerSec();
			int nchan = settings_->numChannels();
			int plug_stream_size = 1024;

			if (pPlayer->_playing)
			{
				float_buffer = pPlayer->Work(&pPlayer, plug_stream_size);
				p_chunk.set_data_32(float_buffer,plug_stream_size,nchan,samprate);
				return true;
			}
			else 
			{
				return false;
			}
		}
	}
}
