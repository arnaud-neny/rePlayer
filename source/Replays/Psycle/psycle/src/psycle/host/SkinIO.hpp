///\file
///\brief interface file for psycle::host::SkinIO.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PsycleConfig.hpp"

namespace psycle { namespace host {

		/// skin file IO
		class SkinIO
		{
		protected:
			SkinIO();
			virtual ~SkinIO();
		public:
			static void LoadTheme(const char* szFile, PsycleConfig::MachineView& macView, PsycleConfig::MachineParam& macParam, PsycleConfig::PatternView& patView);
			static void SaveTheme(const char* szFile, PsycleConfig::MachineView& macView, PsycleConfig::MachineParam& macParam, PsycleConfig::PatternView& patView);
			static void LoadMachineSkin(const char* szFile, SMachineCoords& coords);
			static void LoadPatternSkin(const char* szFile, SPatternHeaderCoords& coords);
			static void LoadControlsSkin(const char* szFile, SParamsCoords& coords);
			static bool LocateSkinDir(CString findDir, CString findName, const char ext[], std::string& result);
			static void LocateSkins(CString findDir, std::list<std::string>& pattern_skins, std::list<std::string>& machine_skins);

		protected:
			static void LoadProperties(std::FILE* hfile, std::map<std::string,std::string> & props);
			static void SetSkinSource(const std::string& buf, SSkinSource& skin);
			static void SetSkinDest(const std::string& buf, SSkinDest& skin);

		};
	}   // namespace
}   // namespace
