///\file
///\interface psycle::host::Configuration.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include <universalis/os/fs.hpp>
#include <string>

namespace psycle
{
	namespace host
	{
		class AudioDriver;
		class ConfigStorage;

		/// configuration.
		class Configuration
		{
		public:
			Configuration();
			virtual ~Configuration();
			
			//Actions
			virtual void SetDefaultSettings(bool include_others=true);
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
			virtual void RefreshSettings();
			std::string AsAbsolute(std::string dir) const;

			//Members

			std::string const & appPath              () const { return program_executable_dir_; }
			boost::filesystem::path const & cacheDir () const { return cache_dir_; }

			std::string         GetAbsolutePluginDir () const;
			std::string const & GetPluginDir         () const { return plugin_dir_; }
			               void SetPluginDir         (std::string const &d) { plugin_dir_ = d; }
			std::string         GetAbsoluteLuaDir    () const;
			std::string const & GetLuaDir            () const { return lua_dir_; }
			               void SetLuaDir            (std::string const &d) { lua_dir_ = d;}
			std::string         GetAbsoluteLadspaDir () const;
			std::string const & GetLadspaDir         () const { return ladspa_dir_; }
			               void SetLadspaDir         (std::string const &d) { ladspa_dir_ = d;}
			std::string         GetAbsoluteVst32Dir  () const;
			std::string const & GetVst32Dir          () const { return vst32_dir_; }
			               void SetVst32Dir          (std::string const &d) { vst32_dir_ = d;}
			std::string         GetAbsoluteVst64Dir  () const;
			std::string const & GetVst64Dir          () const { return vst64_dir_; }
			               void SetVst64Dir          (std::string const &d) { vst64_dir_ = d;}
						   bool SupportsJBridge      () const;
			               void UseJBridge           (bool use);
			               bool UsesJBridge          () const;
			               void UsePsycleVstBridge   (bool use);
			               bool UsesPsycleVstBridge  () const;
			      AudioDriver & audioDriver          () const { return *_pOutputDriver; }
			               void SetDefaultPatLines   (int);
			               int  GetDefaultPatLines   () const;
			               void UseAutoStopMachines  (bool use);
			               bool UsesAutoStopMachines () const;
			               void LoadNewBlitz         (bool use);
			               bool LoadsNewBlitz        () const;
			               void SetNumThreads         (int numThreads);
			               int  GetNumThreads        () const;

		public:
			AudioDriver* _pOutputDriver;
		protected:
			void SetCacheDir          (boost::filesystem::path const &cachedir) { cache_dir_ = cachedir; }
		private:
			bool prefferNewBlitz;
			int numThreads_;
			std::string program_executable_dir_;
			std::string plugin_dir_;
			std::string plugin_dir_other;
			boost::filesystem::path cache_dir_;
			std::string lua_dir_;
			std::string ladspa_dir_;
			std::string vst32_dir_;
			std::string vst64_dir_;
		};
	}
}
