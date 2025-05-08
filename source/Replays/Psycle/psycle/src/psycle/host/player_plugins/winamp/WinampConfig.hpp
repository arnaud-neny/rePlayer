///\file
///\interface psycle::host::Configuration.
#pragma once
#include <psycle/host/detail/project.hpp>
#include <psycle/host/Global.hpp>
#include <psycle/host/Configuration.hpp>
#include "WinampDriver.hpp"

namespace psycle
{
	namespace host
	{
		/// configuration.
		class WinampConfig : public Configuration
		{
		public:
			WinampConfig();
			virtual ~WinampConfig() throw();
			//Actions
			virtual void SetDefaultSettings(bool include_others);
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
			virtual void RefreshSettings();
			virtual void RefreshAudio();

			bool LoadWinampSettings(In_Module& inMod);
			bool SaveWinampSettings(In_Module& inMod);
			bool DeleteWinampSettings(In_Module& inMod);

			void OutputChanged();
			void SetSamplesPerSec(int samples);
		private:
			WinampSettings settings;
		};
	}
}
