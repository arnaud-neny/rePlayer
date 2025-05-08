///\file
///\interface psycle::host::Configuration.
#pragma once
#include <psycle/host/Global.hpp>
#include <psycle/host/Configuration.hpp>
#include "FoobarDriver.hpp"

namespace psycle
{
	namespace host
	{
		/// configuration.
		class FoobarConfig : public Configuration
		{
		public:
			FoobarConfig();
			virtual ~FoobarConfig() throw();
			//Actions
			virtual void SetDefaultSettings(bool include_others);
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
			virtual void RefreshSettings();
			virtual void RefreshAudio();

			bool LoadFoobarSettings();
			bool SaveFoobarSettings();
			bool DeleteFoobarSettings();

			void OutputChanged();
			void SetSamplesPerSec(int samples);
		private:
			FoobarSettings settings;
		};
	}
}
