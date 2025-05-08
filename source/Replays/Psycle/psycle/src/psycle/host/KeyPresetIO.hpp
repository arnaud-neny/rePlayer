///\file
///\brief interface file for psycle::host::KeyPresetIO.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PsycleConfig.hpp"

namespace psycle { namespace host {

		/// key preset file IO
		class KeyPresetIO
		{
		protected:
			KeyPresetIO();
			virtual ~KeyPresetIO();
		public:
			static void LoadPreset(const char* szFile, PsycleConfig::InputHandler& settings);
			static void SavePreset(const char* szFile, PsycleConfig::InputHandler& settings);
			static void LoadOldPrivateProfile(PsycleConfig::InputHandler& settings);

		};
	}   // namespace
}   // namespace
