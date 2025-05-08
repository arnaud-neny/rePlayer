///\file
///\brief implementation file for psycle::host::SkinIO.

#include <psycle/host/detail/project.private.hpp>
#include "SkinIO.hpp"
#include <sstream>
#include <boost/algorithm/string.hpp>
namespace psycle { namespace host {

		bool SkinIO::LocateSkinDir(CString findDir, CString findName, const char ext[], std::string& result)
		{
			CFileFind finder;
			bool found = false;
			int loop = finder.FindFile(findDir + "\\*");	// check for subfolders.
			while (loop) 
			{		
				loop = finder.FindNextFile();
				if (finder.IsDirectory() && !finder.IsDots())
				{
					found = LocateSkinDir(finder.GetFilePath(),findName, ext, result);
					if (found) {
						finder.Close();
						return found;
					}
				}
			}
			finder.Close();

			loop = finder.FindFile(findDir + "\\" + findName + ext); // check if the directory is empty
			while (loop)
			{
				loop = finder.FindNextFile();
				if (!finder.IsDirectory())
				{
					char szOpenName[MAX_PATH];
					std::sprintf(szOpenName,"%s\\%s.bmp",findDir,findName);
					FILE* file;
					file = std::fopen(szOpenName, "rb");
					if(file) {
						result = findDir;
						std::fclose(file);
						found = true;
						break;
					}
				}
			}
			finder.Close();
			return found;
		}

		void SkinIO::LocateSkins(CString findDir, std::list<std::string>& pattern_skins, std::list<std::string>& machine_skins)
		{
			CFileFind finder;
			int loop = finder.FindFile(findDir + "\\*");	// check for subfolders.
			while (loop) 
			{		
				loop = finder.FindNextFile();
				if (finder.IsDirectory() && !finder.IsDots())
				{
					LocateSkins(finder.GetFilePath(),pattern_skins, machine_skins);
				}
			}
			finder.Close();

			loop = finder.FindFile(findDir + "\\*.psh"); // check if the directory is empty
			while (loop)
			{
				loop = finder.FindNextFile();
				if (!finder.IsDirectory())
				{
					// ok so we have a .psh, does it have a valid matching .bmp?
					CString sName;
					char szOpenName[MAX_PATH];
					sName = finder.GetFileName();
					///\todo [bohan] const_cast for now, not worth fixing it imo without making something more portable anyway
					char* pExt = const_cast<char*>(strrchr(sName,'.')); // last .
					pExt[0]=0;
					std::sprintf(szOpenName,"%s\\%s.bmp",findDir,sName);
					FILE* file;
					file = std::fopen(szOpenName, "rb");
					if(file) {
						std::sprintf(szOpenName,"%s\\%s",findDir,sName);
						pattern_skins.push_back(szOpenName);
						std::fclose(file);
					}
				}
			}
			finder.Close();
			loop = finder.FindFile(findDir + "\\*.psm"); // check if the directory is empty
			while (loop)
			{
				loop = finder.FindNextFile();
				if (!finder.IsDirectory())
				{
					// ok so we have a .psm, does it have a valid matching .bmp?
					CString sName;
					char szOpenName[MAX_PATH];
					sName = finder.GetFileName();
					///\todo [bohan] const_cast for now, not worth fixing it imo without making something more portable anyway
					char* pExt = const_cast<char*>(strrchr(sName,'.')); // last .
					pExt[0]=0;
					std::sprintf(szOpenName,"%s\\%s.bmp",findDir,sName);
					FILE* file;
					file = std::fopen(szOpenName, "rb");
					if(file) {
						std::sprintf(szOpenName,"%s\\%s",findDir,sName);
						machine_skins.push_back(szOpenName);
						std::fclose(file);
					}
				}
			}
			finder.Close();
			pattern_skins.sort();
			machine_skins.sort();
		}
		void SkinIO::LoadTheme(const char* szFile, PsycleConfig::MachineView& macView,
			PsycleConfig::MachineParam& macParam, PsycleConfig::PatternView& patView)
		{
			std::FILE * hfile;
			if(!(hfile=std::fopen(szFile,"r")))
			{
				::MessageBox(0, "Couldn't open File for Reading. Operation Aborted",
					"File Open Error", MB_ICONERROR | MB_OK);
				return;
			}
			std::map<std::string,std::string> props;
			LoadProperties(hfile, props);

			std::string str1 = szFile;
			std::string skin_dir = str1.substr(0, str1.rfind('\\')).c_str();

			using helpers::hexstring_to_integer;
			hexstring_to_integer(props["pvc_separator"], patView.separator);
			hexstring_to_integer(props["pvc_separator2"], patView.separator2);
			hexstring_to_integer(props["pvc_background"], patView.background);
			hexstring_to_integer(props["pvc_background2"], patView.background2);
			hexstring_to_integer(props["pvc_row4beat"], patView.row4beat);
			hexstring_to_integer(props["pvc_row4beat2"], patView.row4beat2);
			hexstring_to_integer(props["pvc_rowbeat"], patView.rowbeat);
			hexstring_to_integer(props["pvc_rowbeat2"], patView.rowbeat2);
			hexstring_to_integer(props["pvc_row"], patView.row);
			hexstring_to_integer(props["pvc_row2"], patView.row2);
			hexstring_to_integer(props["pvc_font"], patView.font);
			hexstring_to_integer(props["pvc_font2"], patView.font2);
			hexstring_to_integer(props["pvc_fontPlay"], patView.fontPlay);
			hexstring_to_integer(props["pvc_fontPlay2"], patView.fontPlay2);
			hexstring_to_integer(props["pvc_fontCur"], patView.fontCur);
			hexstring_to_integer(props["pvc_fontCur2"], patView.fontCur2);
			hexstring_to_integer(props["pvc_fontSel"], patView.fontSel);
			hexstring_to_integer(props["pvc_fontSel2"], patView.fontSel2);
			hexstring_to_integer(props["pvc_selection"], patView.selection);
			hexstring_to_integer(props["pvc_selection2"], patView.selection2);
			hexstring_to_integer(props["pvc_playbar"], patView.playbar);
			hexstring_to_integer(props["pvc_playbar2"], patView.playbar2);
			hexstring_to_integer(props["pvc_cursor"], patView.cursor);
			hexstring_to_integer(props["pvc_cursor2"], patView.cursor2);

			patView.header_skin = props["pattern_header_skin"];
			if ( patView.header_skin.empty() || patView.header_skin == PSYCLE__PATH__DEFAULT_PATTERN_HEADER_SKIN)
			{
				patView.header_skin = "";
			}
			else if (patView.header_skin.rfind('\\') == -1)
			{
				std::string current_skin_dir = skin_dir;
				SkinIO::LocateSkinDir(current_skin_dir.c_str(), patView.header_skin.c_str(), ".psh", current_skin_dir);
				patView.header_skin = current_skin_dir + '\\' + patView.header_skin;
				LoadPatternSkin((patView.header_skin + ".psh").c_str(),patView.PatHeaderCoords);
			}

			patView.font_name = props["pattern_fontface"];
			hexstring_to_integer(props["pattern_font_point"], patView.font_point);
			hexstring_to_integer(props["pattern_font_flags"], patView.font_flags);
			hexstring_to_integer(props["pattern_font_x"], patView.font_x);
			hexstring_to_integer(props["pattern_font_y"], patView.font_y);

			hexstring_to_integer(props["mv_colour"], macView.colour);
			hexstring_to_integer(props["mv_polycolour"], macView.polycolour);
			hexstring_to_integer(props["mv_triangle_size"], macView.triangle_size);
			hexstring_to_integer(props["mv_wirecolour"], macView.wirecolour);
			hexstring_to_integer(props["mv_wirewidth"], macView.wirewidth);
			hexstring_to_integer(props["mv_wireaa"], macView.wireaa);
			hexstring_to_integer(props["mv_generator_fontcolour"], macView.generator_fontcolour);
			hexstring_to_integer(props["mv_effect_fontcolour"], macView.effect_fontcolour);
			hexstring_to_integer(props["vu1"], macView.vu1);
			hexstring_to_integer(props["vu2"], macView.vu2);
			hexstring_to_integer(props["vu3"], macView.vu3);

			macView.machine_skin = props["machine_skin"];
			if (macView.machine_skin.empty() || macView.machine_skin == PSYCLE__PATH__DEFAULT_MACHINE_SKIN)
			{
				macView.machine_skin = "";
			}
			else if(macView.machine_skin.rfind('\\') == -1)
			{
				std::string current_skin_dir = skin_dir;
				SkinIO::LocateSkinDir(current_skin_dir.c_str(), macView.machine_skin.c_str(), ".psm", current_skin_dir);
				macView.machine_skin = current_skin_dir + "\\" +  macView.machine_skin;
				LoadMachineSkin((macView.machine_skin + ".psm").c_str(),macView.MachineCoords);
			}
			
			std::map<std::string,std::string>::iterator it = props.find("machine_background");
			// This is set to true only when doing RefreshSettings
			macView.bBmpBkg = false;
			if(it != props.end() && !it->second.empty()) {
				macView.szBmpBkgFilename = it->second;
				if(macView.szBmpBkgFilename.substr(1,1) != ":")
				{
					macView.szBmpBkgFilename = skin_dir + '\\' + macView.szBmpBkgFilename;
				}
			}
			else {
				macView.szBmpBkgFilename = "";
			}

			macView.generator_fontface = props["generator_fontface"];
			hexstring_to_integer(props["generator_font_point"], macView.generator_font_point);
			hexstring_to_integer(props["generator_font_flags"], macView.generator_font_flags);
			macView.effect_fontface = props["effect_fontface"];
			hexstring_to_integer(props["effect_font_point"], macView.effect_font_point);
			hexstring_to_integer(props["effect_font_flags"], macView.effect_font_flags);

			hexstring_to_integer(props["machineGUITopColor"], macParam.topColor);
			hexstring_to_integer(props["machineGUIFontTopColor"], macParam.fontTopColor);
			hexstring_to_integer(props["machineGUIBottomColor"], macParam.bottomColor);
			hexstring_to_integer(props["machineGUIFontBottomColor"], macParam.fontBottomColor);
			hexstring_to_integer(props["machineGUIHTopColor"], macParam.hTopColor);
			hexstring_to_integer(props["machineGUIHFontTopColor"], macParam.fonthTopColor);
			hexstring_to_integer(props["machineGUIHBottomColor"], macParam.hBottomColor);
			hexstring_to_integer(props["machineGUIHFontBottomColor"], macParam.fonthBottomColor);
			hexstring_to_integer(props["machineGUITitleColor"], macParam.titleColor);
			hexstring_to_integer(props["machineGUITitleFontColor"], macParam.fonttitleColor);

			//Set default so that any missing parameter will have the default value.
			macParam.SetDefaultSkin();
			it = props.find("machine_GUI_bitmap");
			if(it != props.end() && !it->second.empty()) {
				macParam.szBmpControlsFilename = it->second;
				std::string str1 = macParam.szBmpControlsFilename;
				if (str1.find(".psc") != std::string::npos) {
					str1 = str1.substr(str1.rfind('\\')+1);
					std::string current_skin_dir = skin_dir;
					SkinIO::LocateSkinDir(current_skin_dir.c_str(), str1.substr(0,str1.find(".psc")).c_str(), ".psc", current_skin_dir);
					SkinIO::LoadControlsSkin((current_skin_dir + "\\" + str1).c_str(), macParam.coords);
					macParam.szBmpControlsFilename = current_skin_dir + '\\' + str1;
				}
				else if(macParam.szBmpControlsFilename.substr(1,1) != ":")
				{
					macParam.szBmpControlsFilename = skin_dir + '\\' + macParam.szBmpControlsFilename;
				}
			}

			//
			//
			//
			// legacy...
			//
			//
			//
			it = props.find("machine_fontface");
			if(it != props.end()) {
				macView.generator_fontface = it->second;
				macView.effect_fontface = it->second;
			}
			it = props.find("machine_font_point");
			if(it != props.end()) {
				hexstring_to_integer(it->second, macView.generator_font_point);
				hexstring_to_integer(it->second, macView.effect_font_point);
			}
			it = props.find("mv_fontcolour");
			if(it != props.end()) {
				hexstring_to_integer(it->second, macView.generator_fontcolour);
				hexstring_to_integer(it->second, macView.effect_fontcolour);
			}
			std::fclose(hfile);
		}

		void SkinIO::SaveTheme(const char* szFile, PsycleConfig::MachineView& macView,
			PsycleConfig::MachineParam& macParam, PsycleConfig::PatternView& patView)
		{
			std::FILE * hfile;
			::CString str = szFile;
			::CString str2 = str.Right(4);
			if(str2.CompareNoCase(".psv")) str.Insert(str.GetLength(),".psv");
			if(!(hfile=std::fopen(str,"wb")))
			{
				::MessageBox(0, "Couldn't open File for Writing. Operation Aborted",
					"File Save Error", MB_ICONERROR | MB_OK);
				return;
			}
			std::string str1 = szFile;
			std::string skin_dir = str1.substr(0, str1.rfind('\\')).c_str();

			std::fprintf(hfile,"[Psycle Display Presets v1.0]\n\n");
			std::fprintf(hfile,"\"pattern_fontface\"=\"%s\"\n", patView.font_name.c_str());
			std::fprintf(hfile,"\"pattern_font_point\"=dword:%.8X\n", patView.font_point);
			std::fprintf(hfile,"\"pattern_font_flags\"=dword:%.8X\n", patView.font_flags);
			std::fprintf(hfile,"\"pattern_font_x\"=dword:%.8X\n", patView.font_x);
			std::fprintf(hfile,"\"pattern_font_y\"=dword:%.8X\n", patView.font_y);
			if(patView.header_skin.empty())
			{
				std::fprintf(hfile,"\"pattern_header_skin\"=\"%s\"\n", PSYCLE__PATH__DEFAULT_PATTERN_HEADER_SKIN);
			}
			else
			{
				std::string str1 = patView.header_skin;
				std::string str2 = str1.substr(str1.rfind('\\')+1);
				std::fprintf(hfile,"\"pattern_header_skin\"=\"%s\"\n", str2.c_str());
			}
			std::fprintf(hfile,"\"pvc_separator\"=dword:%.8X\n", patView.separator);
			std::fprintf(hfile,"\"pvc_separator2\"=dword:%.8X\n", patView.separator2);
			std::fprintf(hfile,"\"pvc_background\"=dword:%.8X\n", patView.background);
			std::fprintf(hfile,"\"pvc_background2\"=dword:%.8X\n", patView.background2);
			std::fprintf(hfile,"\"pvc_font\"=dword:%.8X\n", patView.font);
			std::fprintf(hfile,"\"pvc_font2\"=dword:%.8X\n", patView.font2);
			std::fprintf(hfile,"\"pvc_fontCur\"=dword:%.8X\n", patView.fontCur);
			std::fprintf(hfile,"\"pvc_fontCur2\"=dword:%.8X\n", patView.fontCur2);
			std::fprintf(hfile,"\"pvc_fontSel\"=dword:%.8X\n", patView.fontSel);
			std::fprintf(hfile,"\"pvc_fontSel2\"=dword:%.8X\n", patView.fontSel2);
			std::fprintf(hfile,"\"pvc_fontPlay\"=dword:%.8X\n", patView.fontPlay);
			std::fprintf(hfile,"\"pvc_fontPlay2\"=dword:%.8X\n", patView.fontPlay2);
			std::fprintf(hfile,"\"pvc_row\"=dword:%.8X\n", patView.row);
			std::fprintf(hfile,"\"pvc_row2\"=dword:%.8X\n", patView.row2);
			std::fprintf(hfile,"\"pvc_rowbeat\"=dword:%.8X\n", patView.rowbeat);
			std::fprintf(hfile,"\"pvc_rowbeat2\"=dword:%.8X\n", patView.rowbeat2);
			std::fprintf(hfile,"\"pvc_row4beat\"=dword:%.8X\n", patView.row4beat);
			std::fprintf(hfile,"\"pvc_row4beat2\"=dword:%.8X\n", patView.row4beat2);
			std::fprintf(hfile,"\"pvc_selection\"=dword:%.8X\n", patView.selection);
			std::fprintf(hfile,"\"pvc_selection2\"=dword:%.8X\n", patView.selection2);
			std::fprintf(hfile,"\"pvc_playbar\"=dword:%.8X\n", patView.playbar);
			std::fprintf(hfile,"\"pvc_playbar2\"=dword:%.8X\n", patView.playbar2);
			std::fprintf(hfile,"\"pvc_cursor\"=dword:%.8X\n", patView.cursor);
			std::fprintf(hfile,"\"pvc_cursor2\"=dword:%.8X\n", patView.cursor2);
			std::fprintf(hfile,"\"vu1\"=dword:%.8X\n", macView.vu1);
			std::fprintf(hfile,"\"vu2\"=dword:%.8X\n", macView.vu2);
			std::fprintf(hfile,"\"vu3\"=dword:%.8X\n", macView.vu3);
			std::fprintf(hfile,"\"generator_fontface\"=\"%s\"\n", macView.generator_fontface.c_str());
			std::fprintf(hfile,"\"generator_font_point\"=dword:%.8X\n", macView.generator_font_point);
			std::fprintf(hfile,"\"generator_font_flags\"=dword:%.8X\n", macView.generator_font_flags);
			std::fprintf(hfile,"\"effect_fontface\"=\"%s\"\n", macView.effect_fontface.c_str());
			std::fprintf(hfile,"\"effect_font_point\"=dword:%.8X\n", macView.effect_font_point);
			std::fprintf(hfile,"\"effect_font_flags\"=dword:%.8X\n", macView.effect_font_flags);

			if(macView.machine_skin.empty())
			{
				std::fprintf(hfile,"\"machine_skin\"=\"%s\"\n", PSYCLE__PATH__DEFAULT_MACHINE_SKIN);
			}
			else
			{
				std::string str1 = macView.machine_skin;
				std::string str2 = str1.substr(str1.rfind('\\')+1);
				std::fprintf(hfile,"\"machine_skin\"=\"%s\"\n", str2.c_str());
			}

			std::fprintf(hfile,"\"mv_colour\"=dword:%.8X\n", macView.colour);
			std::fprintf(hfile,"\"mv_wirecolour\"=dword:%.8X\n", macView.wirecolour);
			std::fprintf(hfile,"\"mv_polycolour\"=dword:%.8X\n", macView.polycolour);
			std::fprintf(hfile,"\"mv_generator_fontcolour\"=dword:%.8X\n", macView.generator_fontcolour);
			std::fprintf(hfile,"\"mv_effect_fontcolour\"=dword:%.8X\n", macView.effect_fontcolour);
			std::fprintf(hfile,"\"mv_wirewidth\"=dword:%.8X\n", macView.wirewidth);
			std::fprintf(hfile,"\"mv_wireaa\"=hex:%.2X\n", macView.wireaa);
			if(macView.szBmpBkgFilename.empty())
			{
				std::fprintf(hfile,"\"machine_background\"=\"%s\"\n", "");
			}
			else
			{
				std::string str1 = macView.szBmpBkgFilename;
				if (str1.find(skin_dir) != std::string::npos) {
					str1 = str1.substr(skin_dir.length()+1);
				}
				std::fprintf(hfile,"\"machine_background\"=\"%s\"\n", str1.c_str());
			}
			if(macParam.szBmpControlsFilename.empty())
			{
				std::fprintf(hfile,"\"machine_GUI_bitmap\"=\"%s\"\n", "");
			}
			else
			{
				std::string str1 = macParam.szBmpControlsFilename;
				if (str1.find(skin_dir) != std::string::npos) {
					str1 = str1.substr(skin_dir.length()+1);
				}
				std::fprintf(hfile,"\"machine_GUI_bitmap\"=\"%s\"\n", str1.c_str());
			}
			std::fprintf(hfile,"\"mv_triangle_size\"=hex:%.2X\n", macView.triangle_size);
			std::fprintf(hfile,"\"machineGUITopColor\"=dword:%.8X\n", macParam.topColor);
			std::fprintf(hfile,"\"machineGUIFontTopColor\"=dword:%.8X\n", macParam.fontTopColor);
			std::fprintf(hfile,"\"machineGUIBottomColor\"=dword:%.8X\n", macParam.bottomColor);
			std::fprintf(hfile,"\"machineGUIFontBottomColor\"=dword:%.8X\n", macParam.fontBottomColor);
			std::fprintf(hfile,"\"machineGUIHTopColor\"=dword:%.8X\n", macParam.hTopColor);
			std::fprintf(hfile,"\"machineGUIHFontTopColor\"=dword:%.8X\n", macParam.fonthTopColor);
			std::fprintf(hfile,"\"machineGUIHBottomColor\"=dword:%.8X\n", macParam.hBottomColor);
			std::fprintf(hfile,"\"machineGUIHFontBottomColor\"=dword:%.8X\n", macParam.fonthBottomColor);
			std::fprintf(hfile,"\"machineGUITitleColor\"=dword:%.8X\n", macParam.titleColor);
			std::fprintf(hfile,"\"machineGUITitleFontColor\"=dword:%.8X\n", macParam.fonttitleColor);
			std::fclose(hfile);
		}

		void SkinIO::LoadMachineSkin(const char* szFile, SMachineCoords& coords)
		{
			std::FILE * hfile;
			if(!(hfile=std::fopen(szFile,"r")))
			{
				::MessageBox(0, "Couldn't open File for Reading. Operation Aborted",
					"File Open Error", MB_ICONERROR | MB_OK);
				return;
			}
			std::map<std::string,std::string> props;
			LoadProperties(hfile, props);

			memset(&coords,0,sizeof(SMachineCoords));
			SetSkinSource(props["master_source"], coords.sMaster);
			SetSkinSource(props["generator_source"], coords.sGenerator);
			SetSkinSource(props["generator_vu0_source"], coords.sGeneratorVu0);
			SetSkinSource(props["generator_vu_peak_source"], coords.sGeneratorVuPeak);
			SetSkinSource(props["generator_pan_source"], coords.sGeneratorPan);
			SetSkinSource(props["generator_mute_source"], coords.sGeneratorMute);
			SetSkinSource(props["generator_solo_source"], coords.sGeneratorSolo);
			SetSkinSource(props["effect_source"], coords.sEffect);
			SetSkinSource(props["effect_vu0_source"], coords.sEffectVu0);
			SetSkinSource(props["effect_vu_peak_source"], coords.sEffectVuPeak);
			SetSkinSource(props["effect_pan_source"], coords.sEffectPan);
			SetSkinSource(props["effect_mute_source"], coords.sEffectMute);
			SetSkinSource(props["effect_bypass_source"], coords.sEffectBypass);

			SetSkinSource(props["generator_vu_dest"], coords.dGeneratorVu);
			SetSkinSource(props["generator_pan_dest"], coords.dGeneratorPan);
			SetSkinDest(props["generator_mute_dest"], coords.dGeneratorMute);
			SetSkinDest(props["generator_solo_dest"], coords.dGeneratorSolo);
			SetSkinDest(props["generator_name_dest"], coords.dGeneratorName);
			SetSkinSource(props["effect_vu_dest"], coords.dEffectVu);
			SetSkinSource(props["effect_pan_dest"], coords.dEffectPan);
			SetSkinDest(props["effect_mute_dest"], coords.dEffectMute);
			SetSkinDest(props["effect_bypass_dest"], coords.dEffectBypass);
			SetSkinDest(props["effect_name_dest"], coords.dEffectName);
			
			std::map<std::string,std::string>::iterator it = props.find("transparency");
			if (it != props.end()) {
				int val;
				helpers::hexstring_to_integer(it->second, val);
				coords.cTransparency = val;
				coords.bHasTransparency = true;
			}
			else {
				coords.bHasTransparency = false;
			}
			it = props.find("generator_name_clip_coords");
			if (it != props.end()) {
				SetSkinDest(it->second, coords.dGeneratorNameClip);
			}
			else {
				coords.dGeneratorNameClip.x=0;
				coords.dGeneratorNameClip.y=0;
			}
			it = props.find("effect_name_clip_coords");
			if (it != props.end()) {
				SetSkinDest(it->second, coords.dEffectNameClip);
			}
			else {
				coords.dEffectNameClip.x=0;
				coords.dEffectNameClip.y=0;
			}
			std::fclose(hfile);
		}

		void SkinIO::LoadPatternSkin(const char* szFile, SPatternHeaderCoords& coords)
		{
			std::FILE * hfile;
			if(!(hfile=std::fopen(szFile,"r")))
			{
				::MessageBox(0, "Couldn't open File for Reading. Operation Aborted",
					"File Open Error", MB_ICONERROR | MB_OK);
				return;
			}
			std::map<std::string,std::string> props;
			LoadProperties(hfile, props);

			memset(&coords,0,sizeof(SPatternHeaderCoords));
			SetSkinSource(props["background_source"], coords.sBackgroundClassic);
			SetSkinSource(props["number_0_source"], coords.sNumber0Classic);
			SetSkinSource(props["record_on_source"], coords.sRecordOnClassic);
			SetSkinSource(props["mute_on_source"], coords.sMuteOnClassic);
			SetSkinSource(props["solo_on_source"], coords.sSoloOnClassic);

			SetSkinDest(props["digit_x0_dest"], coords.dDigitX0Classic);
			SetSkinDest(props["digit_0x_dest"], coords.dDigit0XClassic);
			SetSkinDest(props["record_on_dest"], coords.dRecordOnClassic);
			SetSkinDest(props["mute_on_dest"], coords.dMuteOnClassic);
			SetSkinDest(props["solo_on_dest"], coords.dSoloOnClassic);

			//Transparency extension
			std::map<std::string,std::string>::iterator it = props.find("transparency");
			if (it != props.end()) {
				int val;
				helpers::hexstring_to_integer(it->second, val);
				coords.cTransparency = val;
				coords.bHasTransparency = true;
			}
			else {
				coords.bHasTransparency = false;
			}
			//Playing indicator extension
			std::map<std::string,std::string>::iterator it2 = props.find("playing_on_source");
			if (it2 != props.end()) {
				SetSkinSource(it2->second, coords.sPlayOnClassic);
				SetSkinDest(props["playing_on_dest"], coords.dPlayOnClassic);
				coords.bHasPlaying = true;
			}
			else {
				coords.bHasPlaying = false;
			}
			//Tracking extension on classic header
			std::map<std::string,std::string>::iterator it3 = props.find("record_tracking_source");
			if (it3 != props.end()) {
				SetSkinSource(it3->second, coords.sRecordOnTrackingClassic);
				SetSkinSource(props["mute_tracking_source"], coords.sMuteOnTrackingClassic);
				SetSkinSource(props["solo_tracking_source"], coords.sSoloOnTrackingClassic);
				SetSkinDest(props["record_tracking_dest"], coords.dRecordOnTrackClassic);
				SetSkinDest(props["mute_tracking_dest"], coords.dMuteOnTrackClassic);
				SetSkinDest(props["solo_tracking_dest"], coords.dSoloOnTrackClassic);
			}
			else {
				coords.sMuteOnTrackingClassic = coords.sMuteOnClassic;
				coords.sSoloOnTrackingClassic = coords.sSoloOnClassic;
				coords.sRecordOnTrackingClassic = coords.sRecordOnClassic;
				coords.dMuteOnTrackClassic = coords.dMuteOnClassic;
				coords.dSoloOnTrackClassic = coords.dSoloOnClassic;
				coords.dRecordOnTrackClassic = coords.dRecordOnClassic;
			}
			//track names background extension + tracking extension
			std::map<std::string,std::string>::iterator it4 = props.find("text_background_source");
			if (it4 != props.end()) {
				coords.bHasTextSkin = true;
				SetSkinSource(it4->second, coords.sBackgroundText);
				SetSkinSource(props["text_number_0_source"], coords.sNumber0Text);
				SetSkinSource(props["text_record_on_source"], coords.sRecordOnText);
				SetSkinSource(props["text_mute_on_source"], coords.sMuteOnText);
				SetSkinSource(props["text_solo_on_source"], coords.sSoloOnText);
				SetSkinSource(props["text_playing_on_source"], coords.sPlayOnText);
				SetSkinSource(props["text_crop_rectangle"], coords.sTextCrop);
				SetSkinDest(props["text_digit_x0_dest"], coords.dDigitX0Text);
				SetSkinDest(props["text_digit_0x_dest"], coords.dDigit0XText);
				SetSkinDest(props["text_record_on_dest"], coords.dRecordOnText);
				SetSkinDest(props["text_mute_on_dest"], coords.dMuteOnText);
				SetSkinDest(props["text_solo_on_dest"], coords.dSoloOnText);
				SetSkinDest(props["text_playing_on_dest"], coords.dPlayOnText);
				
				SetSkinSource(props["text_record_tracking_source"], coords.sRecordOnTrackingText);
				SetSkinSource(props["text_mute_tracking_source"], coords.sMuteOnTrackingText);
				SetSkinSource(props["text_solo_tracking_source"], coords.sSoloOnTrackingText);
				SetSkinDest(props["text_record_tracking_dest"], coords.dRecordOnTrackText);
				SetSkinDest(props["text_mute_tracking_dest"], coords.dMuteOnTrackText);
				SetSkinDest(props["text_solo_tracking_dest"], coords.dSoloOnTrackText);
				int val=0;
				helpers::hexstring_to_integer(props["text_colour"], val);
				coords.cTextFontColour = val;
				coords.szTextFont = props["text_font_name"];
				hexstring_to_integer(props["text_font_point"], coords.textFontPoint );
				hexstring_to_integer(props["text_font_flags"], coords.textFontFlags );
			}
			else {
				SSkinSource emptySource;
				SSkinDest emptyDest;
				emptySource.x=0; emptySource.y=0;emptySource.width=0; emptySource.height=0;
				emptyDest.x=0; emptyDest.y=0;

				coords.sBackgroundText = emptySource;
				coords.sNumber0Text = emptySource;
				coords.sRecordOnText = emptySource;
				coords.sMuteOnText = emptySource;
				coords.sSoloOnText = emptySource;
				coords.sPlayOnText = emptySource;
				coords.dDigitX0Text = emptyDest;
				coords.dDigit0XText = emptyDest;
				coords.dRecordOnText = emptyDest;
				coords.dMuteOnText = emptyDest;
				coords.dSoloOnText = emptyDest;
				coords.dPlayOnText = emptyDest;

				coords.bHasTextSkin = false;
			}
			std::fclose(hfile);
		}

		void SkinIO::LoadControlsSkin(const char* szFile, SParamsCoords& coords)
		{
			std::FILE * hfile;
			if(!(hfile=std::fopen(szFile,"r")))
			{
				::MessageBox(0, "Couldn't open File for Reading. Operation Aborted",
					"File Open Error", MB_ICONERROR | MB_OK);
				return;
			}
			std::map<std::string,std::string> props;
			LoadProperties(hfile, props);
			
			coords.dialBmp = props["machinedial_bmp"];
			coords.sendMixerBmp = props["send_return_bmp"];
			coords.masterBmp = props["master_bmp"];

			coords.szMasterFont = props["params_text_font_name"];
			hexstring_to_integer(props["params_text_font_point"], coords.paramsFontPoint );
			hexstring_to_integer(props["params_text_font_flags"], coords.paramsFontFlags );
			coords.szMasterFont = props["params_text_font_bold_name"];
			hexstring_to_integer(props["params_text_font_bold_point"], coords.paramsBoldFontPoint );
			hexstring_to_integer(props["params_text_font_bold_flags"], coords.paramsBoldFontFlags );

			hexstring_to_integer(props["master_text_backcolour"], coords.masterFontBackColour );
			hexstring_to_integer(props["master_text_forecolour"], coords.masterFontForeColour );
			coords.szMasterFont = props["master_text_font_name"];
			hexstring_to_integer(props["master_text_font_point"], coords.masterFontPoint );
			hexstring_to_integer(props["master_text_font_flags"], coords.masterFontFlags );
			SetSkinSource(props["master_text_names_dest"], coords.dMasterNames );
			SetSkinDest(props["master_text_numbers_master_dest"], coords.dMasterMasterNumbers );
			SetSkinDest(props["master_text_numbers_channels_dest"], coords.dMasterChannelNumbers );

		}
		void SkinIO::LoadProperties(std::FILE* hfile, std::map<std::string,std::string> & props)
		{
			char buf[1 << 10];
			bool loaded = false;
			while(std::fgets(buf, sizeof buf, hfile))
			{
				if(buf[0] == '#' || (buf[0] == '/' && buf[1] == '/') || buf[0] == '\n')
				{ 
					// Skip comments
					continue;
				} 
				char* equal = std::strchr(buf,'=');
				if (equal != NULL)
				{
					equal[0]='\0';
					//Skip the double quotes containing strings
					std::string key;
					if(buf[0] == '"')
					{
						char *tmp = equal-1;
						tmp[0]='\0';
						tmp = buf+1;
						key = tmp;
					} else { key = buf;	}

					char* value = &equal[1];
					int length = std::strlen(value);
					if(value[0] == '"')
					{
						length-=2;
						value++;
						value[length-1]='\0';
					}
					else {
						//skip the "dword:"  and "hex:" keywords
						char* twodots = std::strchr(value,':');
						if(twodots != NULL) {
							value =  &twodots[1];
						}
					}
					std::string strvalue = value;
					props[key] = strvalue;
					loaded = true;
				}
			}
			if(!loaded)
			{
				::MessageBox(0, "No settings found in the specified file",
					"File Load Error", MB_ICONERROR | MB_OK);
				return;
			}
		}

		void SkinIO::SetSkinSource(const std::string& buf, SSkinSource& skin)
		{
			std::vector<std::string> result(4);
			boost::split(result, buf, boost::is_any_of(","));
			std::istringstream istreamx(result[0]);
			istreamx >> skin.x;
			if (result.size() > 1) {
				std::istringstream istreamy(result[1]);
				istreamy >> skin.y;
			}
			if (result.size() > 2) {
				std::istringstream istreamw(result[2]);
				istreamw >> skin.width;
			}
			if (result.size() > 3) {
				std::istringstream istreamh(result[3]);
				istreamh >> skin.height;
			}
		}

		void SkinIO::SetSkinDest(const std::string& buf, SSkinDest& skin)
		{
			std::vector<std::string> result(2);
			boost::split(result, buf, boost::is_any_of(","));
			std::istringstream istreamx(result[0]);
			istreamx >> skin.x;
			if (result.size() > 1) {
				std::istringstream istreamy(result[1]);
				istreamy >> skin.y;
			}
		}


	}   // namespace
}   // namespace
