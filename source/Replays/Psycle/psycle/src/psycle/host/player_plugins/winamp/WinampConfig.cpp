///\file
///\implementation psycle::host::Configuration.

#include <psycle/host/detail/project.private.hpp>
#include "WinampConfig.hpp"
#include "WinampDriver.hpp"

#include <psycle/host/WinIniFile.hpp>
#include <psycle/host/Player.hpp>

#include <Winamp/wa_ipc.h>

#include <universalis/os/fs.hpp>
#include <string>
#include <sstream>

extern In_Module mod;

namespace psycle { namespace host {

		static const char registry_config_subkey[] = "configuration";
		/////////////////////////////////////////////
		WinampConfig::WinampConfig() : Configuration()
		{
			SetDefaultSettings(false);
		}

		WinampConfig::~WinampConfig() throw()
		{
			if (_pOutputDriver) {
				delete _pOutputDriver;
			}
		}

		void WinampConfig::SetDefaultSettings(bool include_others)
		{
			if(include_others)
			{
				Configuration::SetDefaultSettings();
				_pOutputDriver->settings().SetDefaultSettings();
			}
		}

		bool WinampConfig::LoadWinampSettings(In_Module& inMod)
		{
			_pOutputDriver = new WinampDriver(&settings, &inMod);
			ConfigStorage* store;
			store = new WinIniFile(PSYCLE__VERSION);
			std::string config_file;
			if(SendMessage(inMod.hMainWindow,WM_WA_IPC,0,IPC_GETVERSION) >= 0x2900){
				// this gets the string of the full ini file path
				char ini_path[MAX_PATH] = {0};
				lstrcpyn(ini_path,(char*)SendMessage(inMod.hMainWindow,WM_WA_IPC,0,IPC_GETINIDIRECTORY),sizeof(ini_path));
				config_file = ini_path;
				config_file = config_file + "\\Plugins\\in_psycle.ini";
				std::string cachepath = ini_path;
				cachepath = cachepath + "\\Plugins\\";
				SetCacheDir(cachepath);
		    }
		    else{
				config_file = (boost::filesystem::path(appPath()) / "in_psycle.ini").string();
				SetCacheDir(appPath());
			}
			if(!store->OpenLocation(config_file))
			{
				delete store;
				return false;
			}
			if(!store->OpenGroup(registry_config_subkey)) {
				store->CloseLocation();
				delete store;
				return false;
			}
			Load(*store);
			store->CloseLocation();
			delete store;
			return true;
		}

		bool WinampConfig::SaveWinampSettings(In_Module& inMod) {
			ConfigStorage* store = 0;
			try {
				store = new WinIniFile(PSYCLE__VERSION);
				std::string config_file;
				if(SendMessage(inMod.hMainWindow,WM_WA_IPC,0,IPC_GETVERSION) >= 0x2900){
					// this gets the string of the full ini file path
					char ini_path[MAX_PATH] = {0};
					lstrcpyn(ini_path,(char*)SendMessage(inMod.hMainWindow,WM_WA_IPC,0,IPC_GETINIDIRECTORY),sizeof(ini_path));
					config_file = ini_path;
					config_file = config_file + "\\Plugins\\in_psycle.ini";
				}
				else{
					config_file = (boost::filesystem::path(appPath()) / "in_psycle.ini").string();
				}
				if(!store->CreateLocation(config_file)) {
					delete store;
					return false;
				}
				if(!store->CreateGroup(registry_config_subkey)) {
					store->CloseLocation();
					delete store;
					return false;
				}
				Save(*store);
				store->CloseLocation();
				delete store;
				return true;
			} catch(...) {
				delete store;
				throw;
			}
		}
		bool WinampConfig::DeleteWinampSettings(In_Module& inMod)
		{
			ConfigStorage* store;
			store = new WinIniFile(PSYCLE__VERSION);
			std::string config_file;
			if(SendMessage(inMod.hMainWindow,WM_WA_IPC,0,IPC_GETVERSION) >= 0x2900){
				// this gets the string of the full ini file path
				char ini_path[MAX_PATH] = {0};
				lstrcpyn(ini_path,(char*)SendMessage(inMod.hMainWindow,WM_WA_IPC,0,IPC_GETINIDIRECTORY),sizeof(ini_path));
				config_file = ini_path;
				config_file = config_file + "\\Plugins\\in_psycle.ini";
		    }
		    else{
				config_file = (boost::filesystem::path(appPath()) / "in_psycle.ini").string();
			}
			if(!store->OpenLocation(config_file))
			{
				delete store;
				return false;
			}
			store->DeleteGroup(registry_config_subkey);
			store->CloseLocation();
			delete store;
			return true;
		}

		void WinampConfig::Load(ConfigStorage & store)
		{
			Configuration::Load(store);
			_pOutputDriver->settings().Load(store);
			store.CloseGroup();
		}

		void WinampConfig::Save(ConfigStorage & store)
		{
			Configuration::Save(store);
			_pOutputDriver->settings().Save(store);
			store.CloseGroup();
		}


		void WinampConfig::RefreshSettings()
		{
			Configuration::RefreshSettings();
			RefreshAudio();
		}
		
		void WinampConfig::RefreshAudio()
		{
			if (!_pOutputDriver || !_pOutputDriver->Enabled()) {
				OutputChanged();
			}
		}
		void WinampConfig::SetSamplesPerSec(int samples) {
			((WinampDriver*)_pOutputDriver)->SetSamplesPerSec(samples);
		}
		void WinampConfig::OutputChanged()
		{
			_pOutputDriver->Initialize(Global::player().Work, &Global::player());
			Global::player().SetSampleRate(settings.samplesPerSec());
		}
	}
}
