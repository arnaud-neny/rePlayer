///\file
///\implementation psycle::host::Configuration.

#include <psycle/host/detail/project.private.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include "Configuration.hpp"
#include "ConfigStorage.hpp"
#include "Song.hpp"
#include "Player.hpp"
#include "VstHost24.hpp"
#include "LuaPlugin.hpp" // Refresh settings

namespace psycle { namespace host {

		/////////////////////////////////////////////
		Configuration::Configuration() : _pOutputDriver(0)
		{
			char c[MAX_PATH];
			c[0]='\0';
			::GetModuleFileName(0, c, sizeof c);
			program_executable_dir_ = c;
			program_executable_dir_ = program_executable_dir_.substr(0, program_executable_dir_.rfind('\\')) + '\\';

			SetDefaultSettings();
		}

		Configuration::~Configuration() throw()
		{
		}
		//////////////////////////////////
		void Configuration::SetDefaultSettings(bool include_others)
		{
			SetPluginDir(appPath()+"PsyclePlugins");
			#if PSYCLE__CONFIGURATION__USE_BUILT_PLUGINS
				if(std::getenv("PSYCLE__CONFIGURATION__USE_BUILT_PLUGINS"))
					SetPluginDir(appPath() + "psycle-plugins");
			#endif
			SetCacheDir(universalis::os::fs::home_app_local(PSYCLE__NAME));
			SetLuaDir(appPath()+"LuaScripts");
			SetLadspaDir(appPath()+"LadspaPlugins");
			SetVst32Dir(appPath()+"VstPlugins");
			SetVst64Dir(appPath()+"VstPlugins64");
			UseJBridge(false);
			UsePsycleVstBridge(false);
			SetDefaultPatLines(64);
			UseAutoStopMachines(false);
			LoadNewBlitz(false);
			SetNumThreads(0);
		}
		void Configuration::Load(ConfigStorage & store)
		{
			if(
				#if PSYCLE__CONFIGURATION__USE_BUILT_PLUGINS
					!std::getenv("PSYCLE__CONFIGURATION__USE_BUILT_PLUGINS")
				#else
					true
				#endif
			) {
				#if defined _WIN64
					store.Read("PluginDir64",plugin_dir_);
					store.Read("PluginDir",plugin_dir_other);
				#elif defined _WIN32
					store.Read("PluginDir",plugin_dir_);
					store.Read("PluginDir64",plugin_dir_other);
				#endif
			}
			store.Read("LuaScripts", lua_dir_);
			store.Read("LadspaDir", ladspa_dir_);
			store.Read("VstDir",vst32_dir_);
			store.Read("VstDir64",vst64_dir_);
			bool use=false;
			if(store.Read("jBridge", use)) {
				UseJBridge(use);
			}
			use=false;
			if(store.Read("psycleBridge", use)) {
				UsePsycleVstBridge(use);
			}
			int defaultPatLines=0;
			if(store.Read("defaultPatLines", defaultPatLines))
			{
				SetDefaultPatLines(defaultPatLines);
			}
			use=false;
			if(store.Read("autoStopMachines", use))
			{
				UseAutoStopMachines(use);
			}
			use=false;
			if(store.Read("prefferNewBlitz", use))
			{
				LoadNewBlitz(use);
			}
			int numThreads=0;
			if(store.Read("numThreads", numThreads))
			{
				SetNumThreads(numThreads);
			}
		}
		void Configuration::Save(ConfigStorage & store)
		{
			if(
				#if PSYCLE__CONFIGURATION__USE_BUILT_PLUGINS
					!std::getenv("PSYCLE__CONFIGURATION__USE_BUILT_PLUGINS")
				#else
					true
				#endif
			) {
					#if defined _WIN64
						store.Write("PluginDir64",plugin_dir_);
						store.Write("PluginDir",plugin_dir_other);
					#elif defined _WIN32
						store.Write("PluginDir",plugin_dir_);
						store.Write("PluginDir64",plugin_dir_other);
					#endif
			}
			store.Write("LuaScripts", lua_dir_);
			store.Write("LadspaDir", ladspa_dir_);
			store.Write("VstDir",vst32_dir_);
			store.Write("VstDir64",vst64_dir_);

			bool use=UsesJBridge();
			store.Write("jBridge", use);

			use=UsesPsycleVstBridge();
			store.Write("psycleBridge", use);
				
			int defaultPatLines=GetDefaultPatLines();
			store.Write("defaultPatLines", defaultPatLines);

			use=UsesAutoStopMachines();
			store.Write("autoStopMachines", use);

			use=LoadsNewBlitz();
			store.Write("prefferNewBlitz", use);

			int numThreads=GetNumThreads();
			store.Write("numThreads", numThreads);
		}
		void Configuration::RefreshSettings() 
		{
			Global::player().stop_threads();
			unsigned int thread_count = 0;
			if (numThreads_ > 0)
			{
				thread_count = numThreads_;
			}
			else
			{	 // thread count env var
				char const * const env(std::getenv("PSYCLE_THREADS"));
				if(env) {
					std::stringstream s;
					s << env;
					s >> thread_count;
				}
			}
			HostExtensions::Instance().UpdateStyles();
			Global::player().start_threads(thread_count);
		}

		std::string Configuration::AsAbsolute(std::string dir) const
		{
			boost::filesystem::path mypath(dir);
			if (!mypath.has_root_directory()) {
				boost::filesystem::path root_begin = appPath();
				mypath = root_begin / mypath;
				mypath.normalize();
				std::string res = mypath.string();
#if defined DIVERSALIS__OS__MICROSOFT
//Workaround. normalize() converts C:\bla\bla2 into C:/bla\bla2
				if (res.at(2) == '/') {
					res[2] = '\\';
				}
#endif
				return res;
			}
			else {
				return dir;
			}
		}
		std::string Configuration::GetAbsolutePluginDir() const
		{
			return AsAbsolute(plugin_dir_);
		}
		std::string Configuration::GetAbsoluteLuaDir() const
		{
			return AsAbsolute(lua_dir_);
		}
		std::string Configuration::GetAbsoluteLadspaDir() const
		{
			return AsAbsolute(ladspa_dir_);
		}
		std::string Configuration::GetAbsoluteVst32Dir() const
		{
			return AsAbsolute(vst32_dir_);
		}
		std::string Configuration::GetAbsoluteVst64Dir() const
		{
			return AsAbsolute(vst64_dir_);
		}

		bool Configuration::SupportsJBridge() const
		{
			char testjBridge[MAX_PATH];
			JBridge::getJBridgeLibrary(testjBridge, MAX_PATH);
			return testjBridge[0]!='\0';
		}
		void Configuration::UseJBridge(bool use) 
		{
			vst::Host::UseJBridge(use);
		}
		bool Configuration::UsesJBridge() const 
		{
			return vst::Host::UseJBridge();
		}
		void Configuration::UsePsycleVstBridge(bool use)
		{
			vst::Host::UsePsycleVstBridge(use);
		}
		bool Configuration::UsesPsycleVstBridge() const
		{
			return vst::Host::UsePsycleVstBridge();
		}

		void Configuration::SetDefaultPatLines(int defLines)
		{
			Song::SetDefaultPatLines(defLines);
		}
        int  Configuration::GetDefaultPatLines() const
		{
			return Song::GetDefaultPatLines();
		}
       void Configuration::UseAutoStopMachines(bool use)
	   {
		   Machine::autoStopMachine = use;
	   }
       bool Configuration::UsesAutoStopMachines() const
	   {
		   return Machine::autoStopMachine;
	   }
       void Configuration::LoadNewBlitz(bool use)
	   {
		   prefferNewBlitz = use;
	   }
       bool Configuration::LoadsNewBlitz() const
	   {
		   return prefferNewBlitz;
	   }
       void Configuration::SetNumThreads(int numThreads)
	   {
		   numThreads_ = numThreads;
	   }
       int  Configuration::GetNumThreads() const
	   {
		   return numThreads_;
	   }
	}
}
