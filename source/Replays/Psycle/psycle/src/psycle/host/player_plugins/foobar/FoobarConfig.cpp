///\file
///\implementation psycle::host::Configuration.

#include <psycle/host/detail/project.private.hpp>
#include "FoobarConfig.hpp"
#include "FoobarDriver.hpp"

#include <psycle/host/Player.hpp>
#include <psycle/host/ConfigStorage.hpp>

#include <universalis/os/fs.hpp>
#include <string>
#include <sstream>

namespace psycle { namespace host {

// These GUIDs identify the variables within our component's configuration file.
static const GUID guid_cfg_bogoSetting1 = { 0xbd5c777, 0x735c, 0x440d, { 0x8c, 0x71, 0x49, 0xb6, 0xac, 0xff, 0xce, 0xb8 } };
static const GUID guid_cfg_bogoSetting2 = { 0x752f1186, 0x9f61, 0x4f91, { 0xb3, 0xee, 0x2f, 0x25, 0xb1, 0x24, 0x83, 0x5d } };

static cfg_uint cfg_bogoSetting1(guid_cfg_bogoSetting1, default_cfg_bogoSetting1), cfg_bogoSetting2(guid_cfg_bogoSetting2, default_cfg_bogoSetting2);

		/////////////////////////////////////////////
		FoobarConfig::FoobarConfig() : Configuration()
		{
			SetDefaultSettings(false);
		}

		FoobarConfig::~FoobarConfig() throw()
		{
			if (_pOutputDriver) {
				delete _pOutputDriver;
			}
		}

		void FoobarConfig::SetDefaultSettings(bool include_others)
		{
			if(include_others)
			{
				Configuration::SetDefaultSettings();
				_pOutputDriver->settings().SetDefaultSettings();
			}
		}

		bool FoobarConfig::LoadFoobarSettings()
		{
			_pOutputDriver = new FoobarDriver(&settings);

			//Foobar stores automatically the settings.
			return false;
		}

		bool FoobarConfig::SaveFoobarSettings() {
			//Foobar stores automatically the settings.
			return false;
		}
		bool FoobarConfig::DeleteFoobarSettings()
		{
			return false;
		}

		void FoobarConfig::Load(ConfigStorage & store)
		{
			Configuration::Load(store);
			_pOutputDriver->settings().Load(store);
			store.CloseGroup();
		}

		void FoobarConfig::Save(ConfigStorage & store)
		{
			Configuration::Save(store);
			_pOutputDriver->settings().Save(store);
			store.CloseGroup();
		}


		void FoobarConfig::RefreshSettings()
		{
			Configuration::RefreshSettings();
			RefreshAudio();
		}
		
		void FoobarConfig::RefreshAudio()
		{
			if (!_pOutputDriver || !_pOutputDriver->Enabled()) {
				OutputChanged();
			}
		}
		void FoobarConfig::SetSamplesPerSec(int samples) {
			((FoobarDriver*)_pOutputDriver)->SetSamplesPerSec(samples);
		}
		void FoobarConfig::OutputChanged()
		{
			_pOutputDriver->Initialize(Global::player().Work, &Global::player());
			Global::player().SetSampleRate(settings.samplesPerSec());
		}
	}
}
