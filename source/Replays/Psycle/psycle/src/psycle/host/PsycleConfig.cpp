///\file
///\implementation psycle::host::Configuration.

#include <psycle/host/detail/project.private.hpp>
#include "PsycleConfig.hpp"

#include "Registry.hpp"
#include "WinIniFile.hpp"
#include "SkinIO.hpp"
#include "KeyPresetIO.hpp"

//#include "WaveOut.hpp"
//#include "DirectSound.hpp"
//#include "ASIOInterface.hpp"
//#include "WasapiDriver.hpp"

#include "MidiInput.hpp"

#include "NewMachine.hpp"
#include "Player.hpp"
#include "Song.hpp"
#include "NativeGraphics.hpp"
#include "NativeView.hpp"

#include <universalis/os/fs.hpp>
#include <wingdi.h>
#include <string>
#include <sstream>

#include <boost/filesystem/operations.hpp>

namespace psycle { namespace host {

	namespace {
		static const char registry_root_path[] = "software\\" PSYCLE__NAME "\\" PSYCLE__BRANCH;
		static const char registry_config_subkey[] = "configuration";
	}
		char* PsycleConfig::PatternView::notes_tab_a440[256] = {
			"C-m","C#m","D-m","D#m","E-m","F-m","F#m","G-m","G#m","A-m","A#m","B-m", //0
			"C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0", //1
			"C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1", //2
			"C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2", //3
			"C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3", //4
			"C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4", //5
			"C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5", //6
			"C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6", //7
			"C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7", //8
			"C-8","C#8","D-8","D#8","E-8","F-8","F#8","G-8","G#8","A-8","A#8","B-8", //9
			"off","twk","twf","mcm","tws","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
		};
		char* PsycleConfig::PatternView::notes_tab_a220[256] = {
			"C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0", //0
			"C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1", //1
			"C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2", //2
			"C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3", //3
			"C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4", //4
			"C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5", //5
			"C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6", //6
			"C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7", //7
			"C-8","C#8","D-8","D#8","E-8","F-8","F#8","G-8","G#8","A-8","A#8","B-8", //8
			"C-9","C#9","D-9","D#9","E-9","F-9","F#9","G-9","G#9","A-9","A#9","B-9", //9
			"off","twk","twf","mcm","tws","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
			"   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ","   ",
		};

		void SPatternHeaderCoords::SwitchToClassic()
		{
			sBackground = sBackgroundClassic;
			sNumber0 = sNumber0Classic;
			sRecordOn = sRecordOnClassic;
			sMuteOn = sMuteOnClassic;
			sSoloOn = sSoloOnClassic;
			sPlayOn = sPlayOnClassic;
			dDigitX0 = dDigitX0Classic;
			dDigit0X = dDigit0XClassic;
			dRecordOn = dRecordOnClassic;
			dMuteOn = dMuteOnClassic;
			dSoloOn = dSoloOnClassic;
			dPlayOn = dPlayOnClassic;
			if (bHasTextSkin) {
				sMuteOnTracking = sMuteOnTrackingClassic;
				sSoloOnTracking = sSoloOnTrackingClassic;
				sRecordOnTracking = sRecordOnTrackingClassic;
				dMuteOnTrack = dMuteOnTrackClassic;
				dSoloOnTrack = dSoloOnTrackClassic;
				dRecordOnTrack = dRecordOnTrackClassic;
			}
			else {
				sMuteOnTracking = sMuteOnClassic;
				sSoloOnTracking = sSoloOnClassic;
				sRecordOnTracking = sRecordOnClassic;
				dMuteOnTrack = dMuteOnClassic;
				dSoloOnTrack = dSoloOnClassic;
				dRecordOnTrack = dRecordOnClassic;
			}
		}
		void SPatternHeaderCoords::SwitchToText()
		{
			sBackground = sBackgroundText;
			sNumber0 = sNumber0Text;
			sRecordOn = sRecordOnText;
			sMuteOn = sMuteOnText;
			sSoloOn = sSoloOnText;
			sPlayOn = sPlayOnText;
			dDigitX0 = dDigitX0Text;
			dDigit0X = dDigit0XText;
			dRecordOn = dRecordOnText;
			dMuteOn = dMuteOnText;
			dSoloOn = dSoloOnText;
			dPlayOn = dPlayOnText;
			if (bHasTextSkin) {
				sMuteOnTracking = sMuteOnTrackingText;
				sSoloOnTracking = sSoloOnTrackingText;
				sRecordOnTracking = sRecordOnTrackingText;
				dMuteOnTrack = dMuteOnTrackText;
				dSoloOnTrack = dSoloOnTrackText;
				dRecordOnTrack = dRecordOnTrackText;
			}
			else {
				//setting also background to maintain the header sizes
				sBackground = sBackgroundClassic;
				sMuteOnTracking = sMuteOnClassic;
				sSoloOnTracking = sSoloOnClassic;
				sRecordOnTracking = sRecordOnClassic;
				dMuteOnTrack = dMuteOnClassic;
				dSoloOnTrack = dSoloOnClassic;
				dRecordOnTrack = dRecordOnClassic;
				sTextCrop.x = 0;
				sTextCrop.y = 1;
				sTextCrop.width = sBackgroundClassic.width-1;
				sTextCrop.height = sBackgroundClassic.height;
			}
		}


		PsycleConfig::MachineParam::MachineParam()
		{
			CNativeView::uiSetting = this;
			Knob::uiSetting = this;
			InfoLabel::uiSetting = this;
			GraphSlider::uiSetting = this;
			SwitchButton::uiSetting = this;
			CheckedButton::uiSetting = this;
			VuMeter::uiSetting = this;
			MasterUI::uiSetting = this;
			masterRefresh = false;
			hbmMachineDial = NULL;
			hbmMixerSkin = NULL;
			hbmMasterSkin = NULL;
			SetDefaultSettings();
		}
		PsycleConfig::MachineParam::~MachineParam()
		{
			font.DeleteObject();
			font_bold.DeleteObject();
			masterNamesFont.DeleteObject();
			dial.DeleteObject();
			mixerSkin.DeleteObject();
			masterSkin.DeleteObject();
			if (hbmMachineDial) { DeleteObject(hbmMachineDial); hbmMachineDial=NULL; }
			if (hbmMixerSkin) { DeleteObject(hbmMixerSkin); hbmMixerSkin=NULL; }
			if (hbmMasterSkin) { DeleteObject(hbmMasterSkin); hbmMasterSkin=NULL; }
			CNativeView::uiSetting = NULL;
			Knob::uiSetting = NULL;
			InfoLabel::uiSetting = NULL;
			GraphSlider::uiSetting = NULL;
			SwitchButton::uiSetting = NULL;
			CheckedButton::uiSetting = NULL;
			VuMeter::uiSetting = NULL;
		}
		void PsycleConfig::MachineParam::SetDefaultSettings(bool include_others/*=true*/)
		{
			toolbarOnMachineParams = true;

			if(include_others) {
				SetDefaultColours();
				SetDefaultSkin();
			}
		}
		void PsycleConfig::MachineParam::SetDefaultColours()
		{
			topColor = 0x00555555;		
			fontTopColor = 0x00CDCDCD; 
			bottomColor = 0x00444444;
			fontBottomColor = 0x00E7BD18; 
			
			//highlighted param colours
			hTopColor = 0x00555555;
			fonthTopColor = 0x00CDCDCD; 
			hBottomColor = 0x00292929;
			fonthBottomColor = 0x00E7BD18; 

			titleColor = 0x00292929;
			fonttitleColor = 0x00B4B4B4;
		}
		void PsycleConfig::MachineParam::SetDefaultSkin()
		{
			szBmpControlsFilename = "";

			dialwidth = 28;
			dialheight = 28;
			dialframes = 64;

			coords.sMixerSlider.x = 0;
			coords.sMixerSlider.y = 0;
			coords.sMixerSlider.width = 30;
			coords.sMixerSlider.height = 182;
			coords.sMixerKnob.x = 0;
			coords.sMixerKnob.y = 182;
			coords.sMixerKnob.width = 22;
			coords.sMixerKnob.height = 10;

			coords.sMixerVuOff.x = 30;
			coords.sMixerVuOff.y = 0;
			coords.sMixerVuOff.width = 16;
			coords.sMixerVuOff.height = 90; //97;
			coords.sMixerVuOn.x = 46;
			coords.sMixerVuOn.y = 0;

			coords.sMixerSwitchOff.x = 30;
			coords.sMixerSwitchOff.y = 90;
			coords.sMixerSwitchOff.width = 28;
			coords.sMixerSwitchOff.height = 28;
			coords.sMixerSwitchOn.x = 30;
			coords.sMixerSwitchOn.y = 118;

			coords.sMixerCheckOff.x = 30;
			coords.sMixerCheckOff.y = 146;
			coords.sMixerCheckOff.width = 13; //16;
			coords.sMixerCheckOff.height = 13; //14;
			coords.sMixerCheckOn.x = 30;
			coords.sMixerCheckOn.y = 159;

			coords.szParamsFont ="Tahoma";
			coords.paramsFontPoint = 80;
			coords.paramsFontFlags = 0;
			coords.szParamsBoldFont ="Tahoma";
			coords.paramsBoldFontPoint = 80;
			coords.paramsBoldFontFlags = 1;
			coords.szMasterFont ="Tahoma";
			coords.masterFontPoint = 70;
			coords.masterFontFlags = 0;
			coords.masterFontBackColour = 0x00292929;
			coords.masterFontForeColour = 0x00E7BD18;
			coords.dMasterNames.x = 427;
			coords.dMasterNames.y = 33;
			coords.dMasterNames.width = 75;
			coords.dMasterNames.height = 12;
			coords.dMasterMasterNumbers.x = 22;
			coords.dMasterMasterNumbers.y = 187;
			coords.dMasterChannelNumbers.x = 118;
			coords.dMasterChannelNumbers.y = 187;

			coords.sMasterVuLeftOff.x = 516;
			coords.sMasterVuLeftOff.y = 0;
			coords.sMasterVuLeftOff.width = 18;
			coords.sMasterVuLeftOff.height = 159;
			coords.sMasterVuLeftOn.x = 534;
			coords.sMasterVuLeftOn.y = 0;
			coords.sMasterVuRightOff.x = 552;
			coords.sMasterVuRightOff.y = 0;
			coords.sMasterVuRightOn.x = 570;
			coords.sMasterVuRightOn.y = 0;
			coords.sMasterKnob.x = 516;
			coords.sMasterKnob.y = 159;
			coords.sMasterKnob.width = 22;
			coords.sMasterKnob.height = 10;
		}

		void PsycleConfig::MachineParam::Load(ConfigStorage & store,std::string mainSkinDir, std::string machine_skin)
		{
			// Do not open group if loading version 1.8.6
			if(!store.GetVersion().empty()) {
				store.OpenGroup("MacParamVisual");
			}
			store.Read("toolbarOnVsts", toolbarOnMachineParams);
			store.Read("szBmpDialFilename", szBmpControlsFilename);
			store.Read("machineGUIFontTopColor", fontTopColor);
			store.Read("machineGUIFontBottomColor", fontBottomColor);
			store.Read("machineGUIHFontTopColor", fonthTopColor);
			store.Read("machineGUIHFontBottomColor", fonthBottomColor);
			store.Read("machineGUITitleFontColor", fonttitleColor);
			store.Read("machineGUITopColor", topColor);
			store.Read("machineGUIBottomColor", bottomColor);
			store.Read("machineGUIHTopColor", hTopColor);
			store.Read("machineGUIHBottomColor", hBottomColor);
			store.Read("machineGUITitleColor", titleColor);
			// Close group if loading version 1.8.8 and onwards
			if(!store.GetVersion().empty()) {
				store.CloseGroup();
			}
			if(szBmpControlsFilename.empty() || szBmpControlsFilename == PSYCLE__PATH__DEFAULT_DIAL_SKIN) {
				SetDefaultSkin();
			}
			else {
				// Setting the default skin parameters also when loading so that if something is missing, the defaults remain.
				std::string filename = szBmpControlsFilename;
				SetDefaultSkin();
				szBmpControlsFilename = filename;
				if (szBmpControlsFilename.find(".psc") != std::string::npos) {
					std::string str1 = szBmpControlsFilename.substr(szBmpControlsFilename.rfind('\\')+1);
					std::string current_skin_dir = mainSkinDir;
					SkinIO::LocateSkinDir(current_skin_dir.c_str(), str1.substr(0,str1.find(".psc")).c_str(), ".psc", current_skin_dir);
					SkinIO::LoadControlsSkin((current_skin_dir + "\\" + str1).c_str(), coords);
					szBmpControlsFilename = current_skin_dir + "\\" +  str1;
				}
				else if(szBmpControlsFilename.rfind('\\') == -1) {
					if(machine_skin.empty()) {
						szBmpControlsFilename = mainSkinDir + "\\" + szBmpControlsFilename;
					}
					else {
						std::string skin_dir = machine_skin.substr(0,machine_skin.rfind('\\'));
						szBmpControlsFilename = skin_dir + "\\" + szBmpControlsFilename;
					}
				}
			}
		}
		void PsycleConfig::MachineParam::Save(ConfigStorage & store)
		{
			store.CreateGroup("MacParamVisual");
			store.Write("toolbarOnVsts", toolbarOnMachineParams);
			store.Write("szBmpDialFilename", szBmpControlsFilename);
			store.Write("machineGUIFontTopColor", fontTopColor);
			store.Write("machineGUIFontBottomColor", fontBottomColor);
			store.Write("machineGUIHFontTopColor", fonthTopColor);
			store.Write("machineGUIHFontBottomColor", fonthBottomColor);
			store.Write("machineGUITitleFontColor", fonttitleColor);
			store.Write("machineGUITopColor", topColor);
			store.Write("machineGUIBottomColor", bottomColor);
			store.Write("machineGUIHTopColor", hTopColor);
			store.Write("machineGUIHBottomColor", hBottomColor);
			store.Write("machineGUITitleColor", titleColor);
			store.CloseGroup();
		}

		void PsycleConfig::MachineParam::RefreshSettings()
		{
			HWND hwndDesk = ::GetDesktopWindow();
			::GetWindowRect(hwndDesk, &deskrect); 

			bool bold = coords.paramsFontFlags &1;
			bool italics = coords.paramsFontFlags &2;
			if(!PsycleConfig::CreatePsyFont(font,coords.szParamsFont,coords.paramsFontPoint,bold,italics))
			{
				Error("Could not find this font! " + coords.szParamsFont);
				if(!PsycleConfig::CreatePsyFont(font,"Tahoma",coords.paramsFontPoint,bold,false))
					PsycleConfig::CreatePsyFont(font,"Arial",80,false,false);
			}
			bold = coords.paramsBoldFontFlags &1;
			italics = coords.paramsBoldFontFlags &2;
			if(!PsycleConfig::CreatePsyFont(font_bold,coords.szParamsBoldFont,coords.paramsBoldFontPoint,bold,italics))
			{
				Error("Could not find this font! " + coords.szParamsBoldFont);
				if(!PsycleConfig::CreatePsyFont(font_bold,"Tahoma",coords.paramsBoldFontPoint,bold,false))
					PsycleConfig::CreatePsyFont(font_bold,"Arial",80,false,false);
			}
			bold = coords.masterFontFlags &1;
			italics = coords.masterFontFlags &2;
			if(!PsycleConfig::CreatePsyFont(masterNamesFont,coords.szMasterFont,coords.masterFontPoint,bold,italics))
			{
				Error("Could not find this font! " + coords.szMasterFont);
				if(!PsycleConfig::CreatePsyFont(masterNamesFont,"Tahoma",coords.masterFontPoint,bold,false))
					PsycleConfig::CreatePsyFont(masterNamesFont,"Arial",80,false,false);
			}

			RefreshSkin();
		}
		void PsycleConfig::MachineParam::RefreshSkin()
		{
			dial.DeleteObject();
			mixerSkin.DeleteObject();
			masterSkin.DeleteObject();
			if (hbmMachineDial) { DeleteObject(hbmMachineDial); hbmMachineDial=NULL; }
			if (hbmMixerSkin) { DeleteObject(hbmMixerSkin); hbmMixerSkin=NULL; }
			if (hbmMasterSkin) { DeleteObject(hbmMasterSkin); hbmMasterSkin=NULL; }

			if (szBmpControlsFilename.empty())
			{
				dial.LoadBitmap(IDB_KNOB);
				mixerSkin.LoadBitmap(IDB_MIXER_SKIN);
				masterSkin.LoadBitmap(IDB_MASTER_SKIN);
			}
			else if(!RefreshBitmaps())
			{
				PsycleConfig::Error("The controls skin specified cannot be loaded. Wrong name?");
				SetDefaultSkin();
				dial.LoadBitmap(IDB_KNOB);
				mixerSkin.LoadBitmap(IDB_MIXER_SKIN);
				masterSkin.LoadBitmap(IDB_MASTER_SKIN);
			}
			masterRefresh = true;

			GraphSlider::xoffset = ((coords.sMixerSlider.width-coords.sMixerKnob.width)/2);
		}
		bool PsycleConfig::MachineParam::RefreshBitmaps()
		{
			bool bDialSkinned=false;
			bool bMixerSkinned=false;
			bool bMasterSkinned=false;
			std::string dialFile;
			std::string dir = szBmpControlsFilename.substr(0,szBmpControlsFilename.rfind('\\')+1);
			if (szBmpControlsFilename.find(".psc") != std::string::npos) 
			{
				dialFile = (dir + coords.dialBmp);
				hbmMixerSkin = (HBITMAP)LoadImage(NULL, (dir + coords.sendMixerBmp).c_str(), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
				if (hbmMixerSkin) {	bMixerSkinned = mixerSkin.Attach(hbmMixerSkin);}

				hbmMasterSkin = (HBITMAP)LoadImage(NULL, (dir + coords.masterBmp).c_str(), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
				if (hbmMasterSkin) { bMasterSkinned = masterSkin.Attach(hbmMasterSkin); }
				if (!bMixerSkinned || !bMasterSkinned) {
					mixerSkin.DeleteObject();
					masterSkin.DeleteObject();
					if (hbmMixerSkin) { DeleteObject(hbmMixerSkin); hbmMixerSkin=NULL; }
					if (hbmMasterSkin) { DeleteObject(hbmMasterSkin); hbmMasterSkin=NULL; }
					return false;
				}
			}
			else {
				dialFile = szBmpControlsFilename;
				SetDefaultSkin();
				szBmpControlsFilename = dialFile;
				mixerSkin.LoadBitmap(IDB_MIXER_SKIN);
				masterSkin.LoadBitmap(IDB_MASTER_SKIN);
			}

			hbmMachineDial = (HBITMAP)LoadImage(NULL, dialFile.c_str(), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
			if (hbmMachineDial && dial.Attach(hbmMachineDial)) {
				BITMAP bm;
				GetObject(hbmMachineDial,sizeof(BITMAP),&bm);
				bDialSkinned = (bm.bmWidth == 1792 && bm.bmHeight == 28);
			}
			if (bDialSkinned) {
				return true;
			}
			else {
				dial.DeleteObject();
				mixerSkin.DeleteObject();
				masterSkin.DeleteObject();
				if (hbmMachineDial) { DeleteObject(hbmMachineDial); hbmMachineDial=NULL; }
				if (hbmMixerSkin) { DeleteObject(hbmMixerSkin); hbmMixerSkin=NULL; }
				if (hbmMasterSkin) { DeleteObject(hbmMasterSkin); hbmMasterSkin=NULL; }
				return false;
			}
		}
		////////////////////////////////////
		PsycleConfig::MachineView::MachineView()
		{
			hbmMachineSkin = NULL;
			hbmMachineBkg = NULL;
			SetDefaultSettings();
		}
		PsycleConfig::MachineView::~MachineView()
		{
			generatorFont.DeleteObject();
			effectFont.DeleteObject();
			machineskin.DeleteObject();
			DeleteObject(hbmMachineSkin);
			hbmMachineSkin=NULL;
			machineskinmask.DeleteObject();
			machinebkg.DeleteObject();
			DeleteObject(hbmMachineBkg);
			hbmMachineBkg=NULL;
		}
		void PsycleConfig::MachineView::SetDefaultSettings(bool include_others)
		{
			draw_mac_index = true;
			draw_vus = true;

			if(include_others) {
				SetDefaultColours();
				SetDefaultSkin();
				SetDefaultBackground();
			}
		}
		void PsycleConfig::MachineView::SetDefaultColours()
		{
			colour =	0x00232323;
			polycolour =	0x005F5F5F;
			wirecolour =	0x005F5F5F;
			triangle_size = 10;
			wirewidth = 1;
			wireaa = 0;

			vu1 = 0x004DFFA6;
			vu2 = 0x00282828;
			vu3 = 0x004D4DFF;

			generator_fontcolour = 0x00B1C8B0;
			generator_fontface = "Arial";
			generator_font_point = 80;
			generator_font_flags = 0;
			effect_fontcolour = 0x00D1C5B6;
			effect_fontface = "Arial";
			effect_font_point = 80;
			effect_font_flags = 0;
		}
		void PsycleConfig::MachineView::SetDefaultSkin()
		{
			machine_skin = "";

			MachineCoords.sMaster.x = 0;
			MachineCoords.sMaster.y = 52;
			MachineCoords.sMaster.width = 138;
			MachineCoords.sMaster.height = 35;

			MachineCoords.sGenerator.x = 0;
			MachineCoords.sGenerator.y = 87;
			MachineCoords.sGenerator.width = 138;
			MachineCoords.sGenerator.height = 52;
			MachineCoords.sGeneratorVu0.x = 0;
			MachineCoords.sGeneratorVu0.y = 156;
			MachineCoords.sGeneratorVu0.width = 0;
			MachineCoords.sGeneratorVu0.height = 7;
			MachineCoords.sGeneratorVuPeak.x = 108;
			MachineCoords.sGeneratorVuPeak.y = 156;
			MachineCoords.sGeneratorVuPeak.width = 1;
			MachineCoords.sGeneratorVuPeak.height = 7;
			MachineCoords.sGeneratorPan.x = 0;
			MachineCoords.sGeneratorPan.y = 139;
			MachineCoords.sGeneratorPan.width = 6;
			MachineCoords.sGeneratorPan.height = 13;
			MachineCoords.sGeneratorMute.x = 23;
			MachineCoords.sGeneratorMute.y = 139;
			MachineCoords.sGeneratorMute.width = 17;
			MachineCoords.sGeneratorMute.height = 17;
			MachineCoords.sGeneratorSolo.x = 6;
			MachineCoords.sGeneratorSolo.y = 139;
			MachineCoords.sGeneratorSolo.width = 17;
			MachineCoords.sGeneratorSolo.height = 17;

			MachineCoords.sEffect.x = 0;
			MachineCoords.sEffect.y = 0;
			MachineCoords.sEffect.width = 138;
			MachineCoords.sEffect.height = 52;
			MachineCoords.sEffectVu0.x = 0;
			MachineCoords.sEffectVu0.y = 163;
			MachineCoords.sEffectVu0.width = 0;
			MachineCoords.sEffectVu0.height = 7;
			MachineCoords.sEffectVuPeak.x = 108;
			MachineCoords.sEffectVuPeak.y = 156;
			MachineCoords.sEffectVuPeak.width = 1;
			MachineCoords.sEffectVuPeak.height = 7;
			MachineCoords.sEffectPan.x = 57;
			MachineCoords.sEffectPan.y = 139;
			MachineCoords.sEffectPan.width = 6;
			MachineCoords.sEffectPan.height = 13;
			MachineCoords.sEffectMute.x = 23;
			MachineCoords.sEffectMute.y = 139;
			MachineCoords.sEffectMute.width = 17;
			MachineCoords.sEffectMute.height = 17;
			MachineCoords.sEffectBypass.x = 40;
			MachineCoords.sEffectBypass.y = 139;
			MachineCoords.sEffectBypass.width = 17;
			MachineCoords.sEffectBypass.height = 17;

			MachineCoords.dGeneratorVu.x = 4;
			MachineCoords.dGeneratorVu.y = 20;
			MachineCoords.dGeneratorVu.width = 129;
			MachineCoords.dGeneratorVu.height = 0;
			MachineCoords.dGeneratorPan.x = 6;
			MachineCoords.dGeneratorPan.y = 33;
			MachineCoords.dGeneratorPan.width = 82;
			MachineCoords.dGeneratorPan.height = 0;
			MachineCoords.dGeneratorMute.x = 117;
			MachineCoords.dGeneratorMute.y = 31;
			MachineCoords.dGeneratorSolo.x = 98;
			MachineCoords.dGeneratorSolo.y = 31;
			MachineCoords.dGeneratorName.x = 20;
			MachineCoords.dGeneratorName.y = 3;
			MachineCoords.dGeneratorNameClip.x = 117;
			MachineCoords.dGeneratorNameClip.y = 15;

			MachineCoords.dEffectVu.x = 4;
			MachineCoords.dEffectVu.y = 20;
			MachineCoords.dEffectVu.width = 129;
			MachineCoords.dEffectVu.height = 0;
			MachineCoords.dEffectPan.x = 6;
			MachineCoords.dEffectPan.y = 33;
			MachineCoords.dEffectPan.width = 82;
			MachineCoords.dEffectPan.height = 0;
			MachineCoords.dEffectMute.x = 117;
			MachineCoords.dEffectMute.y = 31;
			MachineCoords.dEffectBypass.x = 98;
			MachineCoords.dEffectBypass.y = 31;
			MachineCoords.dEffectName.x = 20;
			MachineCoords.dEffectName.y = 3;
			MachineCoords.dEffectNameClip.x = 117;
			MachineCoords.dEffectNameClip.y = 15;

			MachineCoords.bHasTransparency = false;
			MachineCoords.cTransparency = 0x0000FF00;
		}
		void PsycleConfig::MachineView::SetDefaultBackground()
		{
			bBmpBkg = false;
			szBmpBkgFilename = "";
			bkgx = 0;
			bkgy = 0;
		}
		void PsycleConfig::MachineView::Load(ConfigStorage &store, std::string mainSkinDir)
		{
			// Do not open group if loading version 1.8.6
			if(!store.GetVersion().empty()) {
				store.OpenGroup("MachineVisual");
			}
			store.Read("mv_colour", colour);
			store.Read("mv_polycolour", polycolour);
			store.Read("mv_triangle_size", triangle_size);
			store.Read("mv_wirecolour", wirecolour);
			store.Read("mv_wirewidth", wirewidth);
			store.Read("mv_wireaa", wireaa);

			store.Read("vu1", vu1);
			store.Read("vu2", vu2);
			store.Read("vu3", vu3);
			store.Read("draw_vus", draw_vus);

			store.Read("szBmpBkgFilename", szBmpBkgFilename);
			store.Read("machine_skin", machine_skin);
			store.Read("draw_mac_index", draw_mac_index);

			store.Read("mv_generator_fontcolour", generator_fontcolour);
			store.Read("generator_fontface", generator_fontface);
			store.Read("generator_font_point", generator_font_point);
			store.Read("generator_font_flags", generator_font_flags);
			store.Read("mv_effect_fontcolour", effect_fontcolour);
			store.Read("effect_fontface", effect_fontface);
			store.Read("effect_font_point", effect_font_point);
			store.Read("effect_font_flags", effect_font_flags);
			// Close group if loading version 1.8.8 and onwards
			if(!store.GetVersion().empty()) {
				store.CloseGroup();
			}

			std::string skin_dir = mainSkinDir;
			if(machine_skin.empty() || machine_skin == PSYCLE__PATH__DEFAULT_MACHINE_SKIN)
			{
				SetDefaultSkin();
			}
			else {
				if(machine_skin.rfind('\\') == -1) {
					SkinIO::LocateSkinDir(mainSkinDir.c_str(), machine_skin.c_str(), ".psm", skin_dir);
					machine_skin = skin_dir + "\\" + machine_skin;
				}
				else {
					skin_dir = machine_skin.substr(0,machine_skin.rfind('\\'));
				}
				SkinIO::LoadMachineSkin((machine_skin + ".psm").c_str() ,MachineCoords);
			}
			if(szBmpBkgFilename.empty() || szBmpBkgFilename == PSYCLE__PATH__DEFAULT_BACKGROUND_SKIN)
			{
				SetDefaultBackground();
			}
			else if(szBmpBkgFilename.rfind('\\') == -1) {
				szBmpBkgFilename = skin_dir + "\\" + szBmpBkgFilename;
			}
		}
		void PsycleConfig::MachineView::Save(ConfigStorage &store)
		{
			store.CreateGroup("MachineVisual");
			store.Write("mv_colour", colour);
			store.Write("mv_polycolour", polycolour);
			store.Write("mv_triangle_size", triangle_size);
			store.Write("mv_wirecolour", wirecolour);
			store.Write("mv_wirewidth", wirewidth);
			store.Write("mv_wireaa", wireaa);

			store.Write("vu1", vu1);
			store.Write("vu2", vu2);
			store.Write("vu3", vu3);
			store.Write("draw_vus", draw_vus);

			store.Write("szBmpBkgFilename", szBmpBkgFilename);
			store.Write("machine_skin", machine_skin);
			store.Write("draw_mac_index", draw_mac_index);

			store.Write("mv_generator_fontcolour", generator_fontcolour);
			store.Write("generator_fontface", generator_fontface);
			store.Write("generator_font_point", generator_font_point);
			store.Write("generator_font_flags", generator_font_flags);
			store.Write("mv_effect_fontcolour", effect_fontcolour);
			store.Write("effect_fontface", effect_fontface);
			store.Write("effect_font_point", effect_font_point);
			store.Write("effect_font_flags", effect_font_flags);
			store.CloseGroup();
		}
		void PsycleConfig::MachineView::RefreshSettings()
		{
			wireaacolour =
				((((wirecolour&0x00ff0000) + ((colour&0x00ff0000)*4))/5)&0x00ff0000) +
				((((wirecolour&0x00ff00) + ((colour&0x00ff00)*4))/5)&0x00ff00) +
				((((wirecolour&0x00ff) + ((colour&0x00ff)*4))/5)&0x00ff);

			wireaacolour2 =
				(((((wirecolour&0x00ff0000)) + ((colour&0x00ff0000)))/2)&0x00ff0000) +
				(((((wirecolour&0x00ff00)) + ((colour&0x00ff00)))/2)&0x00ff00) +
				(((((wirecolour&0x00ff)) + ((colour&0x00ff)))/2)&0x00ff);

			bool bBold = generator_font_flags & 1;
			bool bItalic = generator_font_flags & 2;
			if(!PsycleConfig::CreatePsyFont(generatorFont,generator_fontface,generator_font_point,bBold,bItalic))
			{
				Error("Could not find this font! " + generator_fontface);
				if(!PsycleConfig::CreatePsyFont(generatorFont,"Tahoma",generator_font_point,bBold,bItalic))
					PsycleConfig::CreatePsyFont(generatorFont,"Arial",10,false,false);
			}
			bBold = effect_font_flags & 1;
			bItalic = effect_font_flags & 2;
			if(!PsycleConfig::CreatePsyFont(effectFont,effect_fontface,effect_font_point,bBold,bItalic))
			{
				Error("Could not find this font! " + effect_fontface);
				if(!PsycleConfig::CreatePsyFont(effectFont,"Tahoma",effect_font_point,bBold,bItalic))
					PsycleConfig::CreatePsyFont(effectFont,"Arial",10,false,false);
			}
			RefreshSkin();
		}
		void PsycleConfig::MachineView::RefreshSkin()
		{
			machineskin.DeleteObject();
			DeleteObject(hbmMachineSkin);
			hbmMachineSkin=NULL;
			machineskinmask.DeleteObject();
			if (machine_skin.empty())
			{
				machineskin.LoadBitmap(IDB_MACHINE_SKIN);
			}
			else if(!RefreshBitmaps())
			{
				PsycleConfig::Error("The machine skin specified cannot be loaded. Wrong name?");
				SetDefaultSkin();
				machineskin.LoadBitmap(IDB_MACHINE_SKIN);
			}
			if (MachineCoords.bHasTransparency)
			{
				PrepareMask(&machineskin,&machineskinmask,MachineCoords.cTransparency);
			}
			RefreshBackground();
		}
		bool PsycleConfig::MachineView::RefreshBitmaps()
		{
			hbmMachineSkin = (HBITMAP)LoadImage(NULL, (machine_skin+ ".bmp").c_str(), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
			if (hbmMachineSkin)
			{	
				 if (machineskin.Attach(hbmMachineSkin)) return true;
				 else DeleteObject(hbmMachineSkin);
			}
			return false;
		}
		void PsycleConfig::MachineView::RefreshBackground()
		{
			machinebkg.DeleteObject();
			if ( hbmMachineBkg) { DeleteObject(hbmMachineBkg); hbmMachineBkg = NULL; }
			hbmMachineBkg=NULL;
			bBmpBkg=false;
			if (!szBmpBkgFilename.empty())
			{
				hbmMachineBkg = (HBITMAP)LoadImage(NULL, szBmpBkgFilename.c_str(), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
				if (hbmMachineBkg && machinebkg.Attach(hbmMachineBkg))
				{	
					BITMAP bm;
					GetObject(hbmMachineBkg,sizeof(BITMAP),&bm);

					bkgx=bm.bmWidth;
					bkgy=bm.bmHeight;

					if ((bkgx > 0) && (bkgy > 0))
					{
						bBmpBkg=true;
					}
					else {
						machinebkg.DeleteObject();
					}
				}
			}
			if(!bBmpBkg) {
				szBmpBkgFilename="";
				if ( hbmMachineBkg) { DeleteObject(hbmMachineBkg); hbmMachineBkg = NULL; }
			}
		}

		///////////////////////////////////////////////
		PsycleConfig::PatternView::PatternView()
		{
			hbmPatHeader = NULL;
			SetDefaultSettings();
		}
		PsycleConfig::PatternView::~PatternView()
		{
			pattern_font.DeleteObject();
			if (PatHeaderCoords.bHasTextSkin) {
				pattern_tracknames_font.DeleteObject();
			}
			patternheader.DeleteObject();
			DeleteObject(hbmPatHeader);
			hbmPatHeader=NULL;
			patternheadermask.DeleteObject();
		}
		void PsycleConfig::PatternView::SetDefaultSettings(bool include_others)
		{
			_centerCursor = false;
			draw_empty_data = false;
			timesig = 4;
			showTrackNames_ = false;
			showA440 = true;

			_linenumbers = true;
			_linenumbersHex = false;
			_linenumbersCursor = true;
			if(include_others) {
				SetDefaultColours();
				SetDefaultSkin();
			}
		}
		void PsycleConfig::PatternView::SetDefaultColours()
		{
			separator  = 0x00292929;
			separator2  = 0x00292929;
			background  = 0x00292929;
			background2  = 0x00292929;
			row4beat  = 0x00595959;
			row4beat2 = 0x00595959;
			rowbeat  = 0x00363636;
			rowbeat2 = 0x00363636;
			row  = 0x003E3E3E;
			row2 = 0x003E3E3E;
			font  = 0x00CACACA;
			font2  = 0x00CACACA;
			fontPlay  = 0x00FFFFFF;
			fontPlay2  = 0x00FFFFFF;
			fontCur  = 0x00FFFFFF;
			fontCur2  = 0x00FFFFFF;
			fontSel  = 0x00FFFFFF;
			fontSel2  = 0x00FFFFFF;
			selection  = 0x009B7800;
			selection2 = 0x009B7800;
			playbar  = 0x009F7B00;
			playbar2 = 0x009F7B00;
			cursor  = 0x009F7B00;
			cursor2 = 0x009F7B00;

			font_name = "Verdana";
			font_flags = 1;
			font_point = 70;
			font_x = 9;
			font_y = 12;
		}
		void PsycleConfig::PatternView::SetDefaultSkin()
		{
			header_skin = "";

			PatHeaderCoords.sBackgroundClassic.x=0;
			PatHeaderCoords.sBackgroundClassic.y=0;
			PatHeaderCoords.sBackgroundClassic.width=107;
			PatHeaderCoords.sBackgroundClassic.height=23;
			PatHeaderCoords.sNumber0Classic.x = 0;
			PatHeaderCoords.sNumber0Classic.y = 23;
			PatHeaderCoords.sNumber0Classic.width = 9;
			PatHeaderCoords.sNumber0Classic.height = 17;
			PatHeaderCoords.sRecordOnClassic.x = 42;
			PatHeaderCoords.sRecordOnClassic.y = 40;
			PatHeaderCoords.sRecordOnClassic.width = 17;
			PatHeaderCoords.sRecordOnClassic.height = 17;
			PatHeaderCoords.sMuteOnClassic.x = 25;
			PatHeaderCoords.sMuteOnClassic.y = 40;
			PatHeaderCoords.sMuteOnClassic.width = 17;
			PatHeaderCoords.sMuteOnClassic.height = 17;
			PatHeaderCoords.sSoloOnClassic.x = 8;
			PatHeaderCoords.sSoloOnClassic.y = 40;
			PatHeaderCoords.sSoloOnClassic.width = 17;
			PatHeaderCoords.sSoloOnClassic.height = 17;
			PatHeaderCoords.sPlayOnClassic.x = 0;
			PatHeaderCoords.sPlayOnClassic.y = 40;
			PatHeaderCoords.sPlayOnClassic.width = 8;
			PatHeaderCoords.sPlayOnClassic.height = 17;
			PatHeaderCoords.dDigitX0Classic.x = 15;
			PatHeaderCoords.dDigitX0Classic.y = 3;
			PatHeaderCoords.dDigit0XClassic.x = 22;
			PatHeaderCoords.dDigit0XClassic.y = 3;
			PatHeaderCoords.dRecordOnClassic.x = 85;
			PatHeaderCoords.dRecordOnClassic.y = 3;
			PatHeaderCoords.dMuteOnClassic.x = 66;
			PatHeaderCoords.dMuteOnClassic.y = 3;
			PatHeaderCoords.dSoloOnClassic.x = 47;
			PatHeaderCoords.dSoloOnClassic.y = 3;
			PatHeaderCoords.dPlayOnClassic.x = 5;
			PatHeaderCoords.dPlayOnClassic.y = 3;
			PatHeaderCoords.sMuteOnTrackingClassic.x = 79;
			PatHeaderCoords.sMuteOnTrackingClassic.y = 40;
			PatHeaderCoords.sMuteOnTrackingClassic.width = 17;
			PatHeaderCoords.sMuteOnTrackingClassic.height = 17;
			PatHeaderCoords.sSoloOnTrackingClassic.x = 62;
			PatHeaderCoords.sSoloOnTrackingClassic.y = 40;
			PatHeaderCoords.sSoloOnTrackingClassic.width = 17;
			PatHeaderCoords.sSoloOnTrackingClassic.height = 17;
			PatHeaderCoords.sRecordOnTrackingClassic.x = 96;
			PatHeaderCoords.sRecordOnTrackingClassic.y = 40;
			PatHeaderCoords.sRecordOnTrackingClassic.width = 17;
			PatHeaderCoords.sRecordOnTrackingClassic.height = 17;
			PatHeaderCoords.dRecordOnTrackClassic.x = 85;
			PatHeaderCoords.dRecordOnTrackClassic.y = 3;
			PatHeaderCoords.dMuteOnTrackClassic.x = 66;
			PatHeaderCoords.dMuteOnTrackClassic.y = 3;
			PatHeaderCoords.dSoloOnTrackClassic.x = 47;
			PatHeaderCoords.dSoloOnTrackClassic.y = 3;

			PatHeaderCoords.sBackgroundText.x=0;
			PatHeaderCoords.sBackgroundText.y=57;
			PatHeaderCoords.sBackgroundText.width=107;
			PatHeaderCoords.sBackgroundText.height=23;
			PatHeaderCoords.sNumber0Text.x = 0;
			PatHeaderCoords.sNumber0Text.y = 80;
			PatHeaderCoords.sNumber0Text.width = 6;
			PatHeaderCoords.sNumber0Text.height = 12;
			PatHeaderCoords.sRecordOnText.x = 46;
			PatHeaderCoords.sRecordOnText.y = 92;
			PatHeaderCoords.sRecordOnText.width = 17;
			PatHeaderCoords.sRecordOnText.height = 4;
			PatHeaderCoords.sMuteOnText.x = 29;
			PatHeaderCoords.sMuteOnText.y = 92;
			PatHeaderCoords.sMuteOnText.width = 17;
			PatHeaderCoords.sMuteOnText.height = 4;
			PatHeaderCoords.sSoloOnText.x = 12;
			PatHeaderCoords.sSoloOnText.y = 92;
			PatHeaderCoords.sSoloOnText.width = 17;
			PatHeaderCoords.sSoloOnText.height = 4;
			PatHeaderCoords.sPlayOnText.x = 0;
			PatHeaderCoords.sPlayOnText.y = 92;
			PatHeaderCoords.sPlayOnText.width = 12;
			PatHeaderCoords.sPlayOnText.height = 4;
			PatHeaderCoords.dDigitX0Text.x = 5;
			PatHeaderCoords.dDigitX0Text.y = 8;
			PatHeaderCoords.dDigit0XText.x = 11;
			PatHeaderCoords.dDigit0XText.y = 8;
			PatHeaderCoords.dRecordOnText.x = 85;
			PatHeaderCoords.dRecordOnText.y = 3;
			PatHeaderCoords.dMuteOnText.x = 66;
			PatHeaderCoords.dMuteOnText.y = 3;
			PatHeaderCoords.dSoloOnText.x = 47;
			PatHeaderCoords.dSoloOnText.y = 3;
			PatHeaderCoords.dPlayOnText.x = 5;
			PatHeaderCoords.dPlayOnText.y = 3;
			PatHeaderCoords.sMuteOnTrackingText.x = 79;
			PatHeaderCoords.sMuteOnTrackingText.y = 40;
			PatHeaderCoords.sMuteOnTrackingText.width = 17;
			PatHeaderCoords.sMuteOnTrackingText.height = 17;
			PatHeaderCoords.sSoloOnTrackingText.x = 62;
			PatHeaderCoords.sSoloOnTrackingText.y = 40;
			PatHeaderCoords.sSoloOnTrackingText.width = 17;
			PatHeaderCoords.sSoloOnTrackingText.height = 17;
			PatHeaderCoords.sRecordOnTrackingText.x = 96;
			PatHeaderCoords.sRecordOnTrackingText.y = 40;
			PatHeaderCoords.sRecordOnTrackingText.width = 17;
			PatHeaderCoords.sRecordOnTrackingText.height = 17;
			PatHeaderCoords.dRecordOnTrackText.x = 85;
			PatHeaderCoords.dRecordOnTrackText.y = 3;
			PatHeaderCoords.dMuteOnTrackText.x = 66;
			PatHeaderCoords.dMuteOnTrackText.y = 3;
			PatHeaderCoords.dSoloOnTrackText.x = 47;
			PatHeaderCoords.dSoloOnTrackText.y = 3;
			PatHeaderCoords.sTextCrop.x = 18;
			PatHeaderCoords.sTextCrop.y = 8;
			PatHeaderCoords.sTextCrop.width = 84;
			PatHeaderCoords.sTextCrop.height = 13;

			PatHeaderCoords.bHasTransparency = true;
			PatHeaderCoords.bHasPlaying = true;
			PatHeaderCoords.bHasTextSkin = true;
			PatHeaderCoords.cTransparency = 0x00FFFFFF;	
			PatHeaderCoords.cTextFontColour = 0x00F0F0F0;
			PatHeaderCoords.szTextFont ="Arial";
			PatHeaderCoords.textFontPoint = 80;
			PatHeaderCoords.textFontFlags = 0;
		}
		void PsycleConfig::PatternView::Load(ConfigStorage &store, std::string mainSkinDir)
		{
			// Do not open group if loading version 1.8.6
			if(!store.GetVersion().empty()) {
				store.OpenGroup("PatternVisual");
			}
			store.Read("pvc_separator", separator);
			store.Read("pvc_separator2", separator2);
			store.Read("pvc_background", background);
			store.Read("pvc_background2", background2);
			store.Read("pvc_row4beat", row4beat);
			store.Read("pvc_row4beat2", row4beat2);
			store.Read("pvc_rowbeat", rowbeat);
			store.Read("pvc_rowbeat2", rowbeat2);
			store.Read("pvc_row", row);
			store.Read("pvc_row2", row2);
			store.Read("pvc_font", font);
			store.Read("pvc_font2", font2);
			store.Read("pvc_fontPlay", fontPlay);
			store.Read("pvc_fontPlay2", fontPlay2);
			store.Read("pvc_fontCur", fontCur);
			store.Read("pvc_fontCur2", fontCur2);
			store.Read("pvc_fontSel", fontSel);
			store.Read("pvc_fontSel2", fontSel2);
			store.Read("pvc_selection", selection);
			store.Read("pvc_selection2", selection2);
			store.Read("pvc_playbar", playbar);
			store.Read("pvc_playbar2", playbar2);
			store.Read("pvc_cursor", cursor);
			store.Read("pvc_cursor2", cursor2);

			store.Read("pattern_header_skin", header_skin);

			store.Read("pattern_fontface", font_name);
			store.Read("pattern_font_flags", font_flags);
			store.Read("pattern_font_point", font_point);
			store.Read("pattern_font_x", font_x);
			store.Read("pattern_font_y", font_y);

			store.Read("CenterCursor", _centerCursor);
			store.Read("pattern_draw_empty_data", draw_empty_data);
			store.Read("pv_timesig", timesig);
			store.Read("showTrackNames", showTrackNames_);
			bool tmp;
			if(store.Read("showA440", tmp)) {
				showA440 = tmp;
			}
			store.Read("DisplayLineNumbers", _linenumbers);
			store.Read("DisplayLineNumbersHex", _linenumbersHex);
			store.Read("DisplayLineNumbersCursor", _linenumbersCursor);

			// Close group if loading version 1.8.8 and onwards
			if(!store.GetVersion().empty()) {
				store.CloseGroup();
			}

			if(header_skin.empty() || header_skin == PSYCLE__PATH__DEFAULT_PATTERN_HEADER_SKIN)
			{
				SetDefaultSkin();
			}
			else {
				if(header_skin.rfind('\\') == -1 && header_skin.length() > 0) {
					std::string skin_dir = mainSkinDir;
					SkinIO::LocateSkinDir(mainSkinDir.c_str(), header_skin.c_str(), ".psh", skin_dir);
					header_skin = skin_dir + "\\" +  header_skin;
				}
				SkinIO::LoadPatternSkin((header_skin + ".psh").c_str() ,PatHeaderCoords);
			}
		}
		void PsycleConfig::PatternView::Save(ConfigStorage &store)
		{
			store.CreateGroup("PatternVisual");
			store.Write("pvc_separator", separator);
			store.Write("pvc_separator2", separator2);
			store.Write("pvc_background", background);
			store.Write("pvc_background2", background2);
			store.Write("pvc_row4beat", row4beat);
			store.Write("pvc_row4beat2", row4beat2);
			store.Write("pvc_rowbeat", rowbeat);
			store.Write("pvc_rowbeat2", rowbeat2);
			store.Write("pvc_row", row);
			store.Write("pvc_row2", row2);
			store.Write("pvc_font", font);
			store.Write("pvc_font2", font2);
			store.Write("pvc_fontPlay", fontPlay);
			store.Write("pvc_fontPlay2", fontPlay2);
			store.Write("pvc_fontCur", fontCur);
			store.Write("pvc_fontCur2", fontCur2);
			store.Write("pvc_fontSel", fontSel);
			store.Write("pvc_fontSel2", fontSel2);
			store.Write("pvc_selection", selection);
			store.Write("pvc_selection2", selection2);
			store.Write("pvc_playbar", playbar);
			store.Write("pvc_playbar2", playbar2);
			store.Write("pvc_cursor", cursor);
			store.Write("pvc_cursor2", cursor2);

			store.Write("pattern_header_skin", header_skin);

			store.Write("pattern_fontface", font_name);
			store.Write("pattern_font_flags", font_flags);
			store.Write("pattern_font_point", font_point);
			store.Write("pattern_font_x", font_x);
			store.Write("pattern_font_y", font_y);

			store.Write("CenterCursor", _centerCursor);
			store.Write("pattern_draw_empty_data", draw_empty_data);
			store.Write("pv_timesig", timesig);
			store.Write("showTrackNames", showTrackNames_);
			store.Write("showA440", showA440);
			store.Write("DisplayLineNumbers", _linenumbers);
			store.Write("DisplayLineNumbersHex", _linenumbersHex);
			store.Write("DisplayLineNumbersCursor", _linenumbersCursor);

			store.CloseGroup();
		}
		void PsycleConfig::PatternView::RefreshSettings()
		{
			bool bBold = font_flags & 1;
			bool bItalic = font_flags & 2;
			if(!CreatePsyFont(pattern_font,font_name,font_point,bBold,bItalic))
			{
				Error("Could not find this font! " + font_name);
				if(!CreatePsyFont(pattern_font,"Tahoma",font_point,bBold,bItalic))
					CreatePsyFont(pattern_font,"Arial",10,false,false);
			}
			RefreshSkin();
		}
		void PsycleConfig::PatternView::RefreshSkin()
		{
			patternheader.DeleteObject();
			DeleteObject(hbmPatHeader);
			hbmPatHeader=NULL;
			patternheadermask.DeleteObject();

			if (header_skin.empty())
			{
				patternheader.LoadBitmap(IDB_PATTERN_HEADER_SKIN);
			}
			else if(!RefreshBitmaps())
			{
				PsycleConfig::Error("The pattern header skin specified cannot be loaded. Wrong name?");
				SetDefaultSkin();
				patternheader.LoadBitmap(IDB_PATTERN_HEADER_SKIN);
			}
			if (PatHeaderCoords.bHasTransparency)
			{
				PrepareMask(&patternheader,&patternheadermask,PatHeaderCoords.cTransparency);
			}
			if (PatHeaderCoords.bHasTextSkin) {
				bool bBold = PatHeaderCoords.textFontFlags & 1;
				bool bItalic = PatHeaderCoords.textFontFlags & 2;
				if(!CreatePsyFont(pattern_tracknames_font,PatHeaderCoords.szTextFont,PatHeaderCoords.textFontPoint,bBold,bItalic))
				{
					if(!CreatePsyFont(pattern_font,"Tahoma",font_point,bBold,bItalic))
						CreatePsyFont(pattern_font,"Arial",10,false,false);
				}
			}
			if(showTrackNames_) { PatHeaderCoords.SwitchToText(); }
			else { PatHeaderCoords.SwitchToClassic(); }
		}
		bool PsycleConfig::PatternView::RefreshBitmaps()
		{
			hbmPatHeader = (HBITMAP)LoadImage(NULL, (header_skin+ ".bmp").c_str(), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
			if (hbmPatHeader)
			{	
				 if (patternheader.Attach(hbmPatHeader)) return true;
				 else DeleteObject(hbmPatHeader);
			}
			return false;
		}
		///////////////////////////////////////////////
		PsycleConfig::InputHandler::InputHandler()
		{
			SetDefaultSettings();
		}
		PsycleConfig::InputHandler::~InputHandler()
		{
		}
		void PsycleConfig::InputHandler::SetDefaultSettings(bool include_others)
		{
			bCtrlPlay = true;
			bFT2HomeBehaviour = true;
			bFT2DelBehaviour = true;
			_windowsBlocks = true;
			bShiftArrowsDoSelect = false;
			_wrapAround = true;
			_cursorAlwaysDown = false;
			_RecordMouseTweaksSmooth = false;
			_RecordUnarmed = true;
			_NavigationIgnoresStep = false;
			_pageUpSteps = 1;

			_RecordNoteoff = false;
			_RecordTweaks = false;
			bMultiKey = true;
			_notesToEffects = false;
			bMoveCursorPaste = true;

			if(include_others)
			{
				SetDefaultKeys();
			}
		}
		void PsycleConfig::InputHandler::SetDefaultKeys()
		{
			keyMap.clear();
			setMap.clear();
			// note keys

			// octave 0
			SetCmd(cdefKeyC_0,0,'Z',false);
			SetCmd(cdefKeyCS0,0,'S',false);
			SetCmd(cdefKeyD_0,0,'X',false);
			SetCmd(cdefKeyDS0,0,'D',false);
			SetCmd(cdefKeyE_0,0,'C',false);
			SetCmd(cdefKeyF_0,0,'V',false);
			SetCmd(cdefKeyFS0,0,'G',false);
			SetCmd(cdefKeyG_0,0,'B',false);
			SetCmd(cdefKeyGS0,0,'H',false);
			SetCmd(cdefKeyA_0,0,'N',false);
			SetCmd(cdefKeyAS0,0,'J',false);
			SetCmd(cdefKeyB_0,0,'M',false);

			// octave 1
			SetCmd(cdefKeyC_1,0,'Q',false);
			SetCmd(cdefKeyCS1,0,'2',false);
			SetCmd(cdefKeyD_1,0,'W',false);
			SetCmd(cdefKeyDS1,0,'3',false);
			SetCmd(cdefKeyE_1,0,'E',false);
			SetCmd(cdefKeyF_1,0,'R',false);
			SetCmd(cdefKeyFS1,0,'5',false);
			SetCmd(cdefKeyG_1,0,'T',false);
			SetCmd(cdefKeyGS1,0,'6',false);
			SetCmd(cdefKeyA_1,0,'Y',false);
			SetCmd(cdefKeyAS1,0,'7',false);
			SetCmd(cdefKeyB_1,0,'U',false);

			// octave 2
			SetCmd(cdefKeyC_2,0,'I',false);
			SetCmd(cdefKeyCS2,0,'9',false);
			SetCmd(cdefKeyD_2,0,'O',false);
			SetCmd(cdefKeyDS2,0,'0',false);
			SetCmd(cdefKeyE_2,0,'P',false);
			
			SetCmd(cdefKeyF_2,0,0,false);
			SetCmd(cdefKeyFS2,0,0,false);
			SetCmd(cdefKeyG_2,0,0,false);
			SetCmd(cdefKeyGS2,0,0,false);
			SetCmd(cdefKeyA_2,0,0,false);
			SetCmd(cdefKeyAS2,0,0,false);
			SetCmd(cdefKeyB_2,0,0,false);
			SetCmd(cdefKeyC_3,0,0,false);
			SetCmd(cdefKeyCS3,0,0,false);
			SetCmd(cdefKeyD_3,0,0,false);
			SetCmd(cdefKeyDS3,0,0,false);
			SetCmd(cdefKeyE_3,0,0,false);
			SetCmd(cdefKeyF_3,0,0,false);
			SetCmd(cdefKeyFS3,0,0,false);
			SetCmd(cdefKeyG_3,0,0,false);
			SetCmd(cdefKeyGS3,0,0,false);
			SetCmd(cdefKeyA_3,0,0,false);
			SetCmd(cdefKeyAS3,0,0,false);
			SetCmd(cdefKeyB_3,0,0,false);
			
			// special
			SetCmd(cdefKeyStop,0,'1',false);
			SetCmd(cdefKeyStopAny,MOD_C,'1',false);
			SetCmd(cdefTweakM,0,192,false);        // tweak machine (`)
			SetCmd(cdefMIDICC,MOD_S,192,false);    // Previously Tweak Effect. Now Mcm Command (~)
			SetCmd(cdefTweakS,MOD_C,192,false);        // tweak machine smooth (`)

			// immediate commands
			SetCmd(cdefEditToggle,0,' ',false);

			SetCmd(cdefOctaveUp,0,VK_MULTIPLY,false);
			SetCmd(cdefOctaveDn,MOD_E,VK_DIVIDE,false);

			SetCmd(cdefMachineDec,MOD_C|MOD_E,VK_LEFT,false);
			SetCmd(cdefMachineInc,MOD_C|MOD_E,VK_RIGHT,false);

			SetCmd(cdefInstrDec,MOD_C|MOD_E,VK_DOWN,false);
			SetCmd(cdefInstrInc,MOD_C|MOD_E,VK_UP,false);

			SetCmd(cdefPlayRowTrack,0,'4',false);
			SetCmd(cdefPlayRowPattern,0,'8',false);
			SetCmd(cdefPlayStart,MOD_S,VK_F5,false);
			SetCmd(cdefPlaySong,0,VK_F5,false);
			SetCmd(cdefPlayBlock,0,VK_F6,false);
			SetCmd(cdefPlayFromPos,0,VK_F7,false);
			SetCmd(cdefPlayStop,0,VK_F8,false);


			SetCmd(cdefInfoPattern,MOD_C,VK_RETURN,false);
			SetCmd(cdefInfoMachine,MOD_S,VK_RETURN,false);
			SetCmd(cdefEditMachine,0,VK_F2,false);
			SetCmd(cdefEditPattern,0,VK_F3,false);
			SetCmd(cdefEditInstr,0,VK_F10,false);
			SetCmd(cdefAddMachine,0,VK_F9,false);

			SetCmd(cdefPatternInc,MOD_S|MOD_E,VK_UP,false);
			SetCmd(cdefPatternDec,MOD_S|MOD_E,VK_DOWN,false);
			SetCmd(cdefSongPosInc,MOD_S|MOD_E,VK_RIGHT,false);
			SetCmd(cdefSongPosDec,MOD_S|MOD_E,VK_LEFT,false);

			SetCmd(cdefColumnNext,0,VK_TAB,false);
			SetCmd(cdefColumnPrev,MOD_S,VK_TAB,false);


			SetCmd(cdefNavUp,MOD_E,VK_UP,false);
			SetCmd(cdefNavDn,MOD_E,VK_DOWN,false);
			SetCmd(cdefNavLeft,MOD_E,VK_LEFT,false);
			SetCmd(cdefNavRight,MOD_E,VK_RIGHT,false);

			SetCmd(cdefNavPageUp,MOD_E,VK_PRIOR,false);
			SetCmd(cdefNavPageDn,MOD_E,VK_NEXT,false);
			SetCmd(cdefNavTop,MOD_E,VK_HOME,false);
			SetCmd(cdefNavBottom,MOD_E,VK_END,false);

			SetCmd(cdefSelectMachine,0,VK_RETURN,false);		
			SetCmd(cdefUndo,MOD_C,'Z',false);
			SetCmd(cdefRedo,MOD_C|MOD_S,'Z',false);
			SetCmd(cdefFollowSong,MOD_C,'F',false);
			SetCmd(cdefMaxPattern,MOD_C,VK_TAB,false);

			// editor commands

			SetCmd(cdefTransposeChannelInc,MOD_C,VK_F2,false);	
			SetCmd(cdefTransposeChannelDec,MOD_C,VK_F1,false);	
			SetCmd(cdefTransposeChannelInc12,MOD_C|MOD_S,VK_F2,false);
			SetCmd(cdefTransposeChannelDec12,MOD_C|MOD_S,VK_F1,false);
			SetCmd(cdefTransposeBlockInc,MOD_C,VK_F12,false);
			SetCmd(cdefTransposeBlockDec,MOD_C,VK_F11,false);
			SetCmd(cdefTransposeBlockInc12,MOD_C|MOD_S,VK_F12,false);	
			SetCmd(cdefTransposeBlockDec12,MOD_C|MOD_S,VK_F11,false);

			SetCmd(cdefPatternCut,MOD_C,VK_F3,false);
			SetCmd(cdefPatternCopy,MOD_C,VK_F4,false);
			SetCmd(cdefPatternPaste,MOD_C,VK_F5,false);
			SetCmd(cdefRowInsert,MOD_E,VK_INSERT,false);
			SetCmd(cdefRowDelete,0,VK_BACK,false);
			SetCmd(cdefRowClear,MOD_E,VK_DELETE,false);

			SetCmd(cdefBlockStart,MOD_C,'B',false);
			SetCmd(cdefBlockEnd,MOD_C,'E',false);
			SetCmd(cdefBlockUnMark,MOD_C,'U',false);
			SetCmd(cdefBlockDouble,MOD_C,'D',false);
			SetCmd(cdefBlockHalve,MOD_C,'H',false);
			SetCmd(cdefBlockCut,MOD_C,'X',false);
			SetCmd(cdefBlockCopy,MOD_C,'C',false);
			SetCmd(cdefBlockPaste,MOD_C,'V',false);
			SetCmd(cdefBlockMix,MOD_C,'M',false);
			SetCmd(cdefBlockInterpolate,MOD_C,'I',false);
			SetCmd(cdefBlockSetMachine,MOD_C,'G',false);
			SetCmd(cdefBlockSetInstr,MOD_C,'T',false);

			SetCmd(cdefSelectAll,MOD_C,'A',false);
			SetCmd(cdefSelectCol,MOD_C,'R',false);

			SetCmd(cdefEditQuantizeDec,0,219,false);    // lineskip - 1
			SetCmd(cdefEditQuantizeInc,0,221,false);    // lineskip + 1

			SetCmd(cdefPatternMixPaste,MOD_C|MOD_S,VK_F5,false);
			SetCmd(cdefPatternTrackMute,MOD_C,VK_F9,false);
			SetCmd(cdefPatternTrackSolo,MOD_C,VK_F8,false);
			SetCmd(cdefPatternTrackRecord,MOD_C,VK_F7,false);

			SetCmd(cdefPatternDelete,MOD_C|MOD_S,VK_F3,false);
			SetCmd(cdefBlockDelete,MOD_C|MOD_S,'X',false);

			SetCmd(cdefSelectBar,MOD_C,'K',false);
		}

		void PsycleConfig::InputHandler::Load(ConfigStorage &store)
		{
			// Load from private profile if loading version 1.8.6
			if(store.GetVersion().empty())
			{
				KeyPresetIO::LoadOldPrivateProfile(*this);
			}
			else {
				store.OpenGroup("InputHandling");
				store.Read("bCtrlPlay", bCtrlPlay);
				store.Read("bFT2HomeBehaviour", bFT2HomeBehaviour);
				store.Read("bFT2DelBehaviour", bFT2DelBehaviour);
				store.Read("bShiftArrowsDoSelect", bShiftArrowsDoSelect);
				store.Read("MoveCursorPaste", bMoveCursorPaste);
			}

			store.Read("windowsBlocks", _windowsBlocks);
			store.Read("WrapAround", _wrapAround);
			store.Read("CursorAlwaysDown", _cursorAlwaysDown);
			store.Read("RecordMouseTweaksSmooth", _RecordMouseTweaksSmooth);
			store.Read("RecordUnarmed", _RecordUnarmed);
			store.Read("NavigationIgnoresStep", _NavigationIgnoresStep);
			store.Read("pageupStepSize", _pageUpSteps);
			store.Read("RecordNoteoff", _RecordNoteoff);
			store.Read("RecordTweaks", _RecordTweaks);
			store.Read("bMultiKey", bMultiKey);
			store.Read("notesToEffects", _notesToEffects);

			// Finish loading if version 1.8.8 and onwards
			if(!store.GetVersion().empty()) {
				char buffer[64];
				std::map<CmdSet,std::pair<int,int>>::const_iterator it;
				for(it = setMap.begin(); it != setMap.end(); ++it)
				{
					sprintf(buffer, "CmdSet:%04d", it->first);

					int value = 0;
					if (store.Read(buffer, value)) {
						SetCmd(it->first,value>>8,value&0xFF);
					}
				}
				store.CloseGroup();
			}
		}

		void PsycleConfig::InputHandler::Save(ConfigStorage &store)
		{
			store.CreateGroup("InputHandling");
			store.Write("bCtrlPlay", bCtrlPlay);
			store.Write("bFT2HomeBehaviour", bFT2HomeBehaviour);
			store.Write("bFT2DelBehaviour", bFT2DelBehaviour);
			store.Write("bShiftArrowsDoSelect", bShiftArrowsDoSelect);
			store.Write("MoveCursorPaste", bMoveCursorPaste);
			store.Write("windowsBlocks", _windowsBlocks);
			store.Write("wrapAround", _wrapAround);
			store.Write("cursorAlwaysDown", _cursorAlwaysDown);
			store.Write("RecordMouseTweaksSmooth", _RecordMouseTweaksSmooth);
			store.Write("RecordUnarmed", _RecordUnarmed);
			store.Write("NavigationIgnoresStep", _NavigationIgnoresStep);
			store.Write("pageupStepSize", _pageUpSteps);
			store.Write("RecordNoteoff", _RecordNoteoff);
			store.Write("RecordTweaks", _RecordTweaks);
			store.Write("bMultiKey", bMultiKey);
			store.Write("notesToEffects", _notesToEffects);

			char buffer[64];
			std::map<CmdSet,std::pair<int,int>>::const_iterator it;
			for(it = setMap.begin(); it != setMap.end(); ++it)
			{
				sprintf(buffer, "CmdSet:%04d", it->first);

				int value = (it->second.first<<8) | (it->second.second&0xFF);
				store.Write(buffer, value);
			}
			store.CloseGroup();
		}
		void PsycleConfig::InputHandler::RefreshSettings()
		{
			//Nothing to refresh for InputHandler.
		}

		// SetCmd
		// in: command def, key, modifiers
		// out: true if we had to remove another definition
		///\todo more warnings if we are changing existing defs
		bool PsycleConfig::InputHandler::SetCmd(CmdDef const &cmd, UINT modifiers, UINT key, bool checkforduplicates)
		{	
			std::map<CmdSet,std::pair<int,int>>::iterator itSet;
			std::map<std::pair<int,int>,CmdDef>::iterator itKey;
			std::pair<int,int> theKey(modifiers,key);
			bool modified = false;

			if(!cmd.IsValid())
			{
				return false;
			}

			if(!checkforduplicates)
			{
				//Do not map a 0,0 key. Just add it to the set.
				if(key != 0 || modifiers != 0) {
					keyMap[theKey]=cmd;
				}
				setMap[cmd.GetID()]=theKey;
				return false;
			}
			// Update the keyMap entry if this key was already being used.
			// Also, change the setMap entry if the keymap used a different CmdDef.
			itKey = keyMap.find(theKey);
			if(itKey != keyMap.end() && itKey->second.GetID() != cdefNull)
			{
				if(itKey->second.GetID() != cmd.GetID())
				{
					modified = true;
					std::map<CmdSet,std::pair<int,int>>::iterator itPrevSet;
					itPrevSet = setMap.find(itKey->second.GetID());
					itPrevSet->second.first = 0;
					itPrevSet->second.second = 0;
				}
				itKey->second=cmd;
			}
			//Do not map a 0,0 key. Just add it to the set.
			else if(key != 0 || modifiers != 0)
			{
				keyMap[theKey]=cmd;
			}

			// Update the setMap entry.
			// Also change the keyMap if this entry was already mapped
			itSet = setMap.find(cmd.GetID());
			assert(itSet != setMap.end());
			if(itSet->second.first != 0 || itSet->second.second != 0) {
				std::map<std::pair<int,int>,CmdDef>::iterator itPrevKey;
				itPrevKey = keyMap.find(itSet->second);
				if(itPrevKey != keyMap.end())
				{
					if(itPrevKey->first.first != theKey.first ||
						itPrevKey->first.second != theKey.second) {
						itPrevKey->second=cdefNull;
					}
				} else {
					//Command mapped, but the mapped key not found? Well, whatever, let's fix it.
					keyMap[theKey]=cmd;
				}
			}
			itSet->second = theKey;

			return modified;
		}
		///////////////////////////////////////////////
		PsycleConfig::Midi::Midi() : groups_(16), velocity_(0x0c), pitch_(1), raw_()
		{
			SetDefaultSettings();
		}
		PsycleConfig::Midi::~Midi()
		{
		}
		void PsycleConfig::Midi::SetDefaultSettings()
		{
			for(std::size_t i(0) ; i < groups().size() ; ++i)
			{
				group(i).message() = group(i).command() = static_cast<int>(i + 1);
				group(i).record()  = false;
				group(i).type()    = 0;
				group(i).from()    = 0;
				group(i).to()      = 0xff;
			}
			// enable velocity, raw and the default gen and inst selection
			pitch_.record()     = false;
			pitch_.type()       = 0; // 0 is cmd
			pitch_.command()    = 0x1;
			pitch_.from()       = 0;
			pitch_.to()         = 0xff;
			velocity_.record()  = true;
			velocity_.type()    = 0; // 0 is cmd
			velocity_.command() = 0xC;
			velocity_.from()    = 0;
			velocity_.to()      = 0xff;
			raw_                = true;
			gen_select_with_     = Midi::MS_USE_SELECTED;
			inst_select_with_    = Midi::MS_USE_SELECTED;
			_midiHeadroom = 100;
			_midiMachineViewSeqMode = false;
		}
		void PsycleConfig::Midi::Load(ConfigStorage &store)
		{
			// Do not open group if loading version 1.8.6
			if(!store.GetVersion().empty()) {
				store.OpenGroup("devices\\midi");
			}

			store.Read("MidiMachineViewSeqMode", _midiMachineViewSeqMode);
			store.Read("MidiInputHeadroom", _midiHeadroom);

			// velocity
			{
				store.Read("MidiRecordVel" , velocity_.record() );
				store.Read("MidiTypeVel"   , velocity_.type()   );
				store.Read("MidiCommandVel", velocity_.command());
				store.Read("MidiFromVel"   , velocity_.from()   );
				store.Read("MidiToVel"     , velocity_.to()     );
			}
			// pitch
			{
				store.Read("MidiRecordPit" , pitch_   .record() );
				store.Read("MidiTypePit"   , pitch_   .type()   );
				store.Read("MidiCommandPit", pitch_   .command());
				store.Read("MidiFromPit"   , pitch_   .from()   );
				store.Read("MidiToPit"     , pitch_   .to()     );
			}
			for(std::size_t i(0) ; i < groups().size() ; ++i)
			{
				std::ostringstream oss;
				oss << i;
				std::string s(oss.str());
				store.Read("MidiMessage" + s, group(i).message());
				store.Read("MidiRecord"  + s, group(i).record() );
				store.Read("MidiType"    + s, group(i).type()   );
				store.Read("MidiCommand" + s, group(i).command());
				store.Read("MidiFrom"    + s, group(i).from()   );
				store.Read("MidiTo"      + s, group(i).to()     );
			}
			store.Read("MidiRawMcm", raw());

			int with = static_cast<int>(gen_select_with_);
			store.Read("MidiGenSelectorType", with);
			gen_select_with_ = static_cast<Midi::selector_t>(with);

			with = static_cast<int>(inst_select_with_);
			store.Read("MidiInstSelectorType", with);
			inst_select_with_ = static_cast<Midi::selector_t>(with);

			// Close group if loading version 1.8.8 and onwards
			if(!store.GetVersion().empty()) {
				store.CloseGroup();
			}
		}
		void PsycleConfig::Midi::Save(ConfigStorage &store)
		{
			store.CreateGroup("devices\\midi");
			store.Write("MidiMachineViewSeqMode", _midiMachineViewSeqMode);
			store.Write("MidiInputHeadroom", _midiHeadroom);
			store.Write("MidiRecordVel" , velocity_.record() );
			store.Write("MidiTypeVel"   , velocity_.type()   );
			store.Write("MidiCommandVel", velocity_.command());
			store.Write("MidiFromVel"   , velocity_.from()   );
			store.Write("MidiToVel"     , velocity_.to()     );
			store.Write("MidiRecordPit" , pitch_   .record() );
			store.Write("MidiTypePit"   , pitch_   .type()   );
			store.Write("MidiCommandPit", pitch_   .command());
			store.Write("MidiFromPit"   , pitch_   .from()   );
			store.Write("MidiToPit"     , pitch_   .to()     );
			for(std::size_t i(0) ; i < groups().size() ; ++i)
			{
				std::ostringstream oss;
				oss << i;
				std::string s(oss.str());
				store.Write("MidiMessage" + s, group(i).message());
				store.Write("MidiRecord"  + s, group(i).record() );
				store.Write("MidiType"    + s, group(i).type()   );
				store.Write("MidiCommand" + s, group(i).command());
				store.Write("MidiFrom"    + s, group(i).from()   );
				store.Write("MidiTo"      + s, group(i).to()     );
			}
			store.Write("MidiRawMcm", raw());
			store.Write("MidiGenSelectorType", static_cast<int>(gen_select_with_));
			store.Write("MidiInstSelectorType", static_cast<int>(inst_select_with_));

			store.CloseGroup();
		}
		void PsycleConfig::Midi::RefreshSettings()
		{
			PsycleGlobal::midi().GetConfigPtr()->midiHeadroom = _midiHeadroom;
		}


		/////////////////////////////////////////////
		PsycleConfig::PsycleConfig() : Configuration()
			, audioSettings(5), store_place_(STORE_USER_REGEDIT)
		{
			if(!PsycleConfig::CreatePsyFont(fixedFont,"Consolas",80,false,false)) {
				PsycleConfig::CreatePsyFont(fixedFont,"Lucida Console",80,false,false);
			}
			HINSTANCE hinst = GetModuleHandle(NULL);
			iconless = (HICON)::LoadImage(hinst, MAKEINTRESOURCE(IDI_LESS),IMAGE_ICON,16,16,0);
			iconlessless = (HICON)::LoadImage(hinst, MAKEINTRESOURCE(IDI_LESSLESS),IMAGE_ICON,16,16,0);
			iconmore = (HICON)::LoadImage(hinst, MAKEINTRESOURCE(IDI_MORE),IMAGE_ICON,16,16,0);
			iconmoremore = (HICON)::LoadImage(hinst, MAKEINTRESOURCE(IDI_MOREMORE),IMAGE_ICON,16,16,0);
			iconplus = (HICON)::LoadImage(hinst, MAKEINTRESOURCE(IDI_PLUS),IMAGE_ICON,16,16,0);
			iconminus = (HICON)::LoadImage(hinst, MAKEINTRESOURCE(IDI_MINUS),IMAGE_ICON,16,16,0);

			// soundcard output device
			{
// 				bool vista = Is_Vista_or_Later();
// 				bool asio = ASIOInterface::SupportsAsio();

				int idx = 0;
				audioSettings[idx++] = new SilentSettings();
// 				audioSettings[idx++] = new WaveOutSettings();
// 				audioSettings[idx++] = new DirectSoundSettings();
// 				if(asio) {
// 					audioSettings[idx++] = new ASIODriverSettings();
// 				}
// 				if (vista) {
// 					audioSettings[idx++] = new WasapiSettings();
// 				}
				_numOutputDrivers=idx;
			}
			//All the other constructors already do SetDefaultSettings.
			SetDefaultSettings(false);
		}

		PsycleConfig::~PsycleConfig() throw()
		{
			fixedFont.DeleteObject();
			::DestroyIcon(iconless);
			::DestroyIcon(iconlessless);
			::DestroyIcon(iconmore);
			::DestroyIcon(iconmoremore);
			::DestroyIcon(iconplus);
			::DestroyIcon(iconminus);


			if(_numOutputDrivers)
			{
				for (int i(0);i<_numOutputDrivers;++i)
				{
					delete audioSettings[i];
				}
			}
			delete _pOutputDriver;
		}

		void PsycleConfig::SetDefaultSettings(bool include_others)
		{
			if(include_others)
			{
				Configuration::SetDefaultSettings();
				patView_.SetDefaultSettings();
				macView_.SetDefaultSettings();
				macParam_.SetDefaultSettings();
				input_.SetDefaultSettings();
				midi_.SetDefaultSettings();
				for(int i=0; i < _numOutputDrivers; i++)
				{
					audioSettings[i]->SetDefaultSettings();
				}
			}
			// soundcard output device
			{
				_outputDriverIndex = 2;
			}

			_midiDriverIndex = 0;
			_syncDriverIndex = 0;

			if (store_place_ < 0 || store_place_ >= STORE_TYPES) {
				store_place_ = STORE_USER_REGEDIT;
			}
			_allowMultipleInstances = false;
			_showAboutAtStart = true;
			bShowSongInfoOnLoad = true;
			bFileSaveReminders = true;
			autosaveSong = true;
			autosaveSongTime = 10;

			_bShowPatternNames = false;
			_followSong = false;


			// paths
			{
// 				SetSongDir((universalis::os::fs::home() / "Songs").string());
// 				SetWaveRecDir((universalis::os::fs::home() / "Songs").string());
// 				SetInstrumentDir((universalis::os::fs::home() / "Instruments").string());
// 				SetSkinDir(appPath()+"Skins");
// 				SetPresetsDir((universalis::os::fs::home() / "Presets").string());
			}
// 			recent_files_.clear();
// 			recent_files_.push_back("");
// 			recent_files_.push_back("");
// 			recent_files_.push_back("");
// 			recent_files_.push_back("");
		}

    ConfigStorage* PsycleConfig::CreateStore() {      
      //There is no default setting. If the storage exists, it is used.
			ConfigStorage* store = NULL;
			boost::filesystem::path cachedir;
			bool opened = false;
			for(int i=0;i < STORE_TYPES; i++) {
				switch(i) 
				{
					case STORE_EXE_DIR:
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->OpenLocation((boost::filesystem::path(appPath()) / PSYCLE__NAME ".ini").string());
						cachedir = boost::filesystem::path(appPath());
						break;
					case STORE_USER_DATA:
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->OpenLocation((universalis::os::fs::home_app_local(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
						cachedir = universalis::os::fs::home_app_local(PSYCLE__NAME);
						break;
					case STORE_USER_REGEDIT:
						store = new Registry(Registry::HKCU, "");
						opened = store->OpenLocation(registry_root_path);
						cachedir = universalis::os::fs::home_app_local(PSYCLE__NAME);
						break;
					case STORE_ALL_DATA:
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->OpenLocation((universalis::os::fs::all_users_app_settings(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
						cachedir = universalis::os::fs::all_users_app_settings(PSYCLE__NAME);
						break;
					case STORE_ALL_REGEDIT:
						store = new Registry(Registry::HKLM, "");
						opened = store->OpenLocation(registry_root_path);
						cachedir = universalis::os::fs::all_users_app_settings(PSYCLE__NAME);
						break;
					default:
						break;
				}
				if(!opened) {
					delete store;
          store = 0;
					continue;
				}
				// So here we go,
				// First, try current path since version 1.8.8
				if(!store->OpenGroup(registry_config_subkey)) {
					// else, resort to the old path used from versions 1.8.0 to 1.8.6 included
					if(!store->OpenGroup("configuration--1.8")) {
						// If neither, return.
						store->CloseLocation();
						delete store;
						return 0;
					}
				}	
        break;
			}
			return store;
    }

		bool PsycleConfig::LoadPsycleSettings()
		{
			//There is no default setting. If the storage exists, it is used.
			ConfigStorage* store = NULL;
			boost::filesystem::path cachedir;
			bool opened = false;
			for(int i=0;i < STORE_TYPES; i++) {
				switch(i) 
				{
					case STORE_EXE_DIR:
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->OpenLocation((boost::filesystem::path(appPath()) / PSYCLE__NAME ".ini").string());
						cachedir = boost::filesystem::path(appPath());
						break;
					case STORE_USER_DATA:
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->OpenLocation((universalis::os::fs::home_app_local(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
						cachedir = universalis::os::fs::home_app_local(PSYCLE__NAME);
						break;
					case STORE_USER_REGEDIT:
						store = new Registry(Registry::HKCU, "");
						opened = store->OpenLocation(registry_root_path);
						cachedir = universalis::os::fs::home_app_local(PSYCLE__NAME);
						break;
					case STORE_ALL_DATA:
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->OpenLocation((universalis::os::fs::all_users_app_settings(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
						cachedir = universalis::os::fs::all_users_app_settings(PSYCLE__NAME);
						break;
					case STORE_ALL_REGEDIT:
						store = new Registry(Registry::HKLM, "");
						opened = store->OpenLocation(registry_root_path);
						cachedir = universalis::os::fs::all_users_app_settings(PSYCLE__NAME);
						break;
					default:
						break;
				}
				if(!opened) {
					delete store;
					continue;
				}
				// So here we go,
				// First, try current path since version 1.8.8
				if(!store->OpenGroup(registry_config_subkey)) {
					// else, resort to the old path used from versions 1.8.0 to 1.8.6 included
					if(!store->OpenGroup("configuration--1.8")) {
						// If neither, return.
						store->CloseLocation();
						delete store;
						return false;
					}
				}
				SetCacheDir(cachedir);
				store_place_ = static_cast<store_t>(i);
				Load(*store);
				store->CloseLocation();
				delete store;
				break;
			}
			return opened;

			//////////////////////
			//
			// History of the registry config path:
			//
			//////////////////////
			//
			// In early psycle versions, registry config used to be stored in:
			// \HKCU\Software\AAS\Psycle\1.0\
			//
			// "AAS" stood for Arguru Audio Software, Arguru being Juan Antonio Argüelles Rius's handle, late original author of psycle.
			//
			//////////////////////
			//
			// Later, it was stored in:
			// \HKCU\Software\AAS\Psycle\CurrentVersion\
			//
			// The "CurrentVersion" was hardcoded in the source code as a string instead of specifying an actual version,
			// which made it, hmm, interesting. It was probably edited by hand just before releasing a version, but we don't know for sure.
			//
			//////////////////////
			//
			// In version 1.7.56, the path changed to:
			// \HKCU\Software\psycle\Psycledelics--1.7\configuration\ (which was expanded from macros \HKCU\Software\<PSYCLE__NAME>\<PSYCLE__BRANCH>--<PSYCLE__VERSION__MAJOR>.<PSYCLE__VERSION__MINOR>\configuration\)
			//
			// Around that time, there had been two known forks of psycle circulating, one from Krissce, and one from Satoshi Fujiwara.
			// Putting "Psycledelics" in the path prevented any clash in config settings between the branches/forks.
			// Also probably unconsciously influenced by this open source nature of psycle,
			// the path became a bit unsusal since it's in the form <App>\<Branch> and not <Company>\<App>.
			//
			// Audio driver settings were stored in the sub path:
			// \HKCU\Software\psycle\Psycledelics--1.7\configuration\devices\
			//
			// The recent file list was stored outside of the configuration path, in:
			// \HKCU\Software\psycle\Psycledelics-1.7\recent-files\
			//
			//////////////////////
			//
			// In version 1.8.0, the path changed to:
			// \HKCU\Software\psycle\Psycledelics\Configuration--1.8\ (which was expanded from macros \HKCU\Software\<PSYCLE__NAME>\<PSYCLE__BRANCH>\Configuration--<PSYCLE__VERSION__MAJOR>.<PSYCLE__VERSION__MINOR>\)
			//
			// Audio driver settings were stored in the sub path:
			// \HKCU\Software\psycle\Psycledelics\Configuration--1.8\devices\
			//
			// The recent file list was stored outside of the configuration path, in:
			// \HKCU\Software\psycle\Psycledelics\recent-files\
			//
			//////////////////////
			//
			// Since 1.8.8, registry config is stored in:
			// \HKCU\Software\psycle\Psycledelics\ (which is expanded from macros \HKCU\Software\<PSYCLE__NAME>\<PSYCLE__BRANCH>\)
			//
			// General settings are stored in the sub path:
			// \HKCU\Software\psycle\Psycledelics\Configuration\
			//
			// Audio driver settings are stored in the sub path:
			// \HKCU\Software\psycle\Psycledelics\devices\
			//
			// The recent file list is stored in the sub path:
			// \HKCU\Software\psycle\Psycledelics\recent-files\
			//
			// The keyboard config file %SystemRoot%\PsycleKeys.ini is gone, it's now all stored in the sub path:
			// \HKCU\Software\psycle\Psycledelics\InputHandling\
			//
			// Some other settings are also now stored by categories in sub paths:
			// \HKCU\Software\psycle\Psycledelics\MachineVisual\
			// \HKCU\Software\psycle\Psycledelics\MacParamVisual\
			// \HKCU\Software\psycle\Psycledelics\PatternVisual\
			//
			// The toolbar settings, however are stored in:
			// \HKCU\Software\psycle\Psycle\ (because of MFC limitations?)
			//
			//////////////////////
			//
			// Thankfully, each psycle version migrated the config flawlessly by importing the config from older paths.
			//
			//////////////////////
		}

		bool PsycleConfig::SavePsycleSettings() {
			ConfigStorage* store = 0;
			bool opened = false;
			boost::filesystem::path cachedir;
			boost::filesystem::path tempfile;
			std::string temp;
			try {
				switch(store_place_) {
					case STORE_EXE_DIR: 
						store = new WinIniFile(PSYCLE__VERSION);
						// Writing the .ini file in a temporary dir and then to the destination dir because the current implementation
						// of WinIniFile uses Microsoft WritePrivateProfile and that method (re)writes the file in each call.
						// Doing so in a temp file can increase the speed when writing into an USB pen drive.
						temp = std::getenv("TEMP");
						if (temp.empty()) temp = std::getenv("TMP");
						if (temp.empty()) temp = universalis::os::fs::home_app_local(PSYCLE__NAME).string();
						tempfile = boost::filesystem::path(temp) / PSYCLE__NAME ".ini";
						opened = store->CreateLocation(tempfile.string());
						cachedir = boost::filesystem::path(appPath());
						break;
					case STORE_USER_DATA: 
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->CreateLocation((universalis::os::fs::home_app_local(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
						cachedir = universalis::os::fs::home_app_local(PSYCLE__NAME);
						break;
					case STORE_USER_REGEDIT: 
						store = new Registry(Registry::HKCU, PSYCLE__VERSION);
						opened = store->CreateLocation(registry_root_path);
						cachedir = universalis::os::fs::home_app_local(PSYCLE__NAME);
						break;
					case STORE_ALL_DATA: 
						store = new WinIniFile(PSYCLE__VERSION);
						opened = store->CreateLocation((universalis::os::fs::all_users_app_settings(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
						cachedir = universalis::os::fs::all_users_app_settings(PSYCLE__NAME);
						break;
					case STORE_ALL_REGEDIT: 
						store = new Registry(Registry::HKLM, PSYCLE__VERSION);
						opened = store->CreateLocation(registry_root_path);
						cachedir = universalis::os::fs::all_users_app_settings(PSYCLE__NAME);
						break;
					default:
						break;
				}
				SetCacheDir(cachedir);
				if (opened) {
					if(store->CreateGroup(registry_config_subkey)) {
						Save(*store);
					}
					else {
						opened = false;
					}
					store->CloseLocation();
					if (store_place_ == STORE_EXE_DIR) {
						// Writing the .ini file in a temporary dir and then to the destination dir because the current implementation
						// of WinIniFile uses Microsoft WritePrivateProfile and that method (re)writes the file in each call.
						// Doing so in a temp file can increase the speed if writing into an USB pen drive.
						boost::filesystem::copy_file(tempfile, boost::filesystem::path(appPath()) / PSYCLE__NAME ".ini", 
							boost::filesystem::copy_option::overwrite_if_exists);
						boost::filesystem::remove(tempfile);
					}
				}
				delete store;
				return opened;
			} catch(std::runtime_error e) {
				delete store;
				MessageBox(NULL,e.what(),"Error saving settings",MB_OK);
				return false;
			} catch(...) {
				delete store;
				MessageBox(NULL,"Error saving settings","Settings",MB_OK);
				return false;
			}
		}
		void PsycleConfig::DeleteStorage(store_t storetodelete)
		{
			switch(storetodelete) {
				case STORE_EXE_DIR:
					{
					WinIniFile store(WinIniFile(PSYCLE__VERSION));
					store.DeleteLocation((boost::filesystem::path(appPath()) / PSYCLE__NAME ".ini").string());
					break;
					}
				case STORE_USER_DATA: 
					{
					WinIniFile store(WinIniFile(PSYCLE__VERSION));
					store.DeleteLocation((universalis::os::fs::home_app_local(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
					break;
					}
				case STORE_USER_REGEDIT: 
					{
					Registry store(Registry(Registry::HKCU, PSYCLE__VERSION));
					store.DeleteLocation(registry_root_path);
					break;
					}
				case STORE_ALL_DATA: 
					{
					WinIniFile store(WinIniFile(PSYCLE__VERSION));
					store.DeleteLocation((universalis::os::fs::all_users_app_settings(PSYCLE__NAME) / PSYCLE__NAME ".ini").string());
					break;
					}
				case STORE_ALL_REGEDIT: 
					{
					Registry store(Registry(Registry::HKLM, PSYCLE__VERSION));
					store.DeleteLocation(registry_root_path);
					break;
					}
				default:
					break;
			}
		}
		void PsycleConfig::Load(ConfigStorage & store)
		{
			Configuration::Load(store);

			store.Read("OutputDriver", _outputDriverIndex);

			store.Read("MidiInputDriver", _midiDriverIndex);
			store.Read("MidiSyncDriver", _syncDriverIndex);

			store.Read("SongDir", song_dir_);
			store.Read("WaveRecDir", wave_rec_dir_);
			store.Read("WaveRecInPSYDir", rec_in_psy_dir_);
			store.Read("InstrumentDir", instrument_dir_);
			store.Read("SkinDir", skin_dir_);
			store.Read("PresetsDir", presets_dir_);

			store.Read("AllowMultipleInstances", _allowMultipleInstances);
			store.Read("showAboutAtStart", _showAboutAtStart);
			store.Read("bShowSongInfoOnLoad", bShowSongInfoOnLoad);
			store.Read("bFileSaveReminders", bFileSaveReminders);
			store.Read("autosaveSong", autosaveSong);
			store.Read("autosaveSongTime", autosaveSongTime);
			//Restoring the place were the setting was saved is counterproductive.
			//Concretely, it can cause the settings to be loaded from a store, and saved 
			//to a different one.
			/*int storepl = 0;
			store.Read("storePlace",storepl);
			if (storepl < 0 || storepl >= STORE_TYPES) {
				store_place_ = STORE_REGEDIT;
			}
			else {
				store_place_ = static_cast<store_t>(storepl);
			}
			*/

			store.Read("ShowPatternNames", _bShowPatternNames);
			store.Read("FollowSong", _followSong);

			///\todo: Check what to do with these three.
			bool cpu_measurements;
			store.Read("UseCPUMeasurements", cpu_measurements);
			Global::player().measure_cpu_usage_ = cpu_measurements;

			store.Read("NewMacDlgpluginOrder", CNewMachine::pluginOrder);
			store.Read("NewMacDlgpluginName", CNewMachine::pluginName);
///
// The other Load() calls can change the group, so do not add settings of PsycleConfig below here.
///
			patView_.Load(store, skin_dir_);
			macView_.Load(store, skin_dir_);
			macParam_.Load(store,skin_dir_, macView_.machine_skin);
			input_.Load(store);
			midi_.Load(store);
			for(int i=0; i < _numOutputDrivers; i++)
			{
				audioSettings[i]->Load(store);
			}

			recent_files_.clear();
			store.OpenGroup("recent-files");
			std::string read;
			if(store.Read("0",read)) {
				recent_files_.push_back(read);
			}
			read="";
			if(store.Read("1",read)) {
				recent_files_.push_back(read);
			}
			read="";
			if(store.Read("2",read)) {
				recent_files_.push_back(read);
			}
			read="";
			if(store.Read("3",read)) {
				recent_files_.push_back(read);
			}
			store.CloseGroup();
///
//Do not add settings here, see the comment above about groups
///
		}

		void PsycleConfig::Save(ConfigStorage & store)
		{
			Configuration::Save(store);

			store.Write("OutputDriver", _outputDriverIndex);

			store.Write("MidiInputDriver", _midiDriverIndex);
			store.Write("MidiSyncDriver", _syncDriverIndex);

			store.Write("SongDir", GetSongDir());
			store.Write("WaveRecDir", GetWaveRecDir());
			store.Write("WaveRecInPSYDir", rec_in_psy_dir_);
			store.Write("InstrumentDir", GetInstrumentDir());
			store.Write("SkinDir", GetSkinDir());
			store.Write("PresetsDir", GetPresetsDir());

			store.Write("AllowMultipleInstances", _allowMultipleInstances);
			store.Write("showAboutAtStart", _showAboutAtStart);
			store.Write("bShowSongInfoOnLoad", bShowSongInfoOnLoad);
			store.Write("bFileSaveReminders", bFileSaveReminders);
			store.Write("autosaveSong", autosaveSong);
			store.Write("autosaveSongTime", autosaveSongTime);
			//store.Write("storePlace",static_cast<int>(store_place_));
			store.Write("FollowSong", _followSong);
			store.Write("ShowPatternNames", _bShowPatternNames);

			bool cpu_measurements = Global::player().measure_cpu_usage_;
			store.Write("UseCPUMeasurements", cpu_measurements);

			store.Write("NewMacDlgpluginOrder", CNewMachine::pluginOrder);
			store.Write("NewMacDlgpluginName", CNewMachine::pluginName);

			patView_.Save(store);
			macView_.Save(store);
			macParam_.Save(store);
			input_.Save(store);
			midi_.Save(store);
			for(int i=0; i < _numOutputDrivers; i++)
			{
				audioSettings[i]->Save(store);
			}

			store.CreateGroup("recent-files");
			for(int i=0; i < recent_files_.size(); i++) {
				std::ostringstream str;
				str << i;
				store.Write(str.str().c_str(),recent_files_[i]);
			}
			store.CloseGroup();
		}


		void PsycleConfig::RefreshSettings()
		{
			if(_pOutputDriver) _pOutputDriver->Enable(false);
			Configuration::RefreshSettings();
			SetCurrentSongDir(GetAbsoluteSongDir());
			if(IsRecInPSYDir()) {
				SetCurrentWaveRecDir(GetAbsoluteSongDir());
			}
			else {
				SetCurrentWaveRecDir(GetAbsoluteWaveRecDir());
			}
			
			SetCurrentInstrumentDir(GetAbsoluteInstrumentDir());
			
			patView_.RefreshSettings();
			macView_.RefreshSettings();
			macParam_.RefreshSettings();
			input_.RefreshSettings();
			midi_.RefreshSettings();

			RefreshAudio();
		}
		
		void PsycleConfig::RefreshAudio()
		{
			bool refreshAudio = (!_pOutputDriver || !_pOutputDriver->Enabled());
			bool refreshMidi = refreshAudio;
			if(0 > _outputDriverIndex || _outputDriverIndex >= _numOutputDrivers) {
				_outputDriverIndex = 0;
				refreshAudio=true;
				refreshMidi=true;
			}
			int _numMidiDrivers = CMidiInput::GetNumDevices();
			if (0 > _midiDriverIndex || _midiDriverIndex > _numMidiDrivers) {
				_midiDriverIndex = 0;
				refreshMidi=true;
			}
			if (0 > _syncDriverIndex || _syncDriverIndex > _numMidiDrivers) {
				_syncDriverIndex = 0;
				refreshMidi=true;
			}

			if(refreshAudio) {
				OutputChanged(_outputDriverIndex);
			}
			if(refreshMidi) {
				PsycleGlobal::midi().Close();
				PsycleGlobal::midi().SetDeviceId(DRIVER_MIDI, _midiDriverIndex - 1);
				PsycleGlobal::midi().SetDeviceId(DRIVER_SYNC, _syncDriverIndex - 1);
				PsycleGlobal::midi().Open();
			}
		}
		void PsycleConfig::OutputChanged(int newidx)
		{
			if(0 > newidx || newidx >= _numOutputDrivers) {
				_outputDriverIndex = 0;
			}
			else { _outputDriverIndex = newidx; }
			delete _pOutputDriver;
			_pOutputDriver = audioSettings[_outputDriverIndex]->NewDriver();
			_pOutputDriver->Initialize(Global::player().Work, &Global::player());
			Global::player().SetSampleRate(audioSettings[_outputDriverIndex]->samplesPerSec());
			_pOutputDriver->Enable(true);
		}
		void PsycleConfig::MidiChanged(int newidx)
		{
			if (0 > newidx || newidx > CMidiInput::GetNumDevices()) {
				_midiDriverIndex = 0;
			}
			else { _midiDriverIndex = newidx; }
			PsycleGlobal::midi().Close();
			PsycleGlobal::midi().SetDeviceId(DRIVER_MIDI, _midiDriverIndex - 1);
			PsycleGlobal::midi().Open();
		}
		void PsycleConfig::SyncChanged(int newidx)
		{
			if (0 > newidx || newidx > CMidiInput::GetNumDevices()) {
				_syncDriverIndex = 0;
			}
			else { _syncDriverIndex = newidx; }
			PsycleGlobal::midi().Close();
			PsycleGlobal::midi().SetDeviceId(DRIVER_SYNC, _syncDriverIndex - 1);
			PsycleGlobal::midi().Open();
		}

        void PsycleConfig::AddRecentFile(std::string const &f)
		{
			bool found = false;
			signed int initial = static_cast<signed int>(recent_files_.size())-1;
			for(std::size_t i = 0; i < recent_files_.size(); ++i) 
			{
				if( f == recent_files_[i]) {
					found = true;
					initial = i;
					break;
				}
			}
			if(recent_files_.size() < 4 && !found) {
				recent_files_.push_back("");
				initial++;
			}
			for(int i = initial; i > 0; --i) 
			{
				recent_files_[i]=recent_files_[i-1];
			}
			recent_files_[0]=f;
		}

		void PsycleConfig::Error(std::string const & what)
		{
			AfxMessageBox(what.c_str(), MB_ICONERROR | MB_OK);
		}


		bool PsycleConfig::CreatePsyFont(CFont & f, std::string const & sFontFace, int const HeightPx, bool const bBold, bool const bItalic)
		{
			f.DeleteObject();
			CString sFace(sFontFace.c_str());
			LOGFONT lf = LOGFONT();
			if(bBold) lf.lfWeight = FW_BOLD;
			if(bItalic) lf.lfItalic = true;
			lf.lfHeight = HeightPx;
			lf.lfQuality = DEFAULT_QUALITY;
			std::strncpy(lf.lfFaceName,(LPCTSTR)sFace,32);
			if(!f.CreatePointFontIndirect(&lf))
			{			
				CString sFaceLowerCase = sFace;
				sFaceLowerCase.MakeLower();
				strncpy(lf.lfFaceName,(LPCTSTR)sFaceLowerCase,32);
				if(!f.CreatePointFontIndirect(&lf)) return false;
			}
			return true;
		}

		void PsycleConfig::PrepareMask(CBitmap* pBmpSource, CBitmap* pBmpMask, COLORREF clrTrans)
		{
			BITMAP bm;
			// Get the dimensions of the source bitmap
			pBmpSource->GetObject(sizeof(BITMAP), &bm);
			// Create the mask bitmap
			pBmpMask->DeleteObject();
			pBmpMask->CreateBitmap( bm.bmWidth, bm.bmHeight, 1, 1, NULL);
			// We will need two DCs to work with. One to hold the Image
			// (the source), and one to hold the mask (destination).
			// When blitting onto a monochrome bitmap from a color, pixels
			// in the source color bitmap that are equal to the background
			// color are blitted as white. All the remaining pixels are
			// blitted as black.
			CDC hdcSrc, hdcDst;
			hdcSrc.CreateCompatibleDC(NULL);
			hdcDst.CreateCompatibleDC(NULL);
			// Load the bitmaps into memory DC
			CBitmap* hbmSrcT = hdcSrc.SelectObject(pBmpSource);
			CBitmap* hbmDstT = hdcDst.SelectObject(pBmpMask);
			// Change the background to trans color
			hdcSrc.SetBkColor(clrTrans);
			// This call sets up the mask bitmap.
			hdcDst.BitBlt(0,0,bm.bmWidth, bm.bmHeight, &hdcSrc,0,0,SRCCOPY);
			// Now, we need to paint onto the original image, making
			// sure that the "transparent" area is set to black. What
			// we do is AND the monochrome image onto the color Image
			// first. When blitting from mono to color, the monochrome
			// pixel is first transformed as follows:
			// if  1 (black) it is mapped to the color set by SetTextColor().
			// if  0 (white) is is mapped to the color set by SetBkColor().
			// Only then is the raster operation performed.
			hdcSrc.SetTextColor(RGB(255,255,255));
			hdcSrc.SetBkColor(RGB(0,0,0));
			hdcSrc.BitBlt(0,0,bm.bmWidth, bm.bmHeight, &hdcDst,0,0,SRCAND);
			// Clean up by deselecting any objects, and delete the
			// DC's.
			hdcSrc.SelectObject(hbmSrcT);
			hdcDst.SelectObject(hbmDstT);
			hdcSrc.DeleteDC();
			hdcDst.DeleteDC();
		}
	}
}
