///\file
///\brief implementation file for psycle::host::CSkinDlg.

#include <psycle/host/detail/project.private.hpp>
#include "SkinDlg.hpp"
#include "SkinIO.hpp"
#include <psycle/helpers/pathname_validate.hpp>

namespace psycle { namespace host {

		int const ID_TIMER_SKINGDLG = 1;

		IMPLEMENT_DYNCREATE(CSkinDlg, CPropertyPage)

		CSkinDlg::CSkinDlg() : CPropertyPage(CSkinDlg::IDD)
			, patConfig(PsycleGlobal::conf().patView())
			, macConfig(PsycleGlobal::conf().macView())
			, paramConfig(PsycleGlobal::conf().macParam())
		{
		}

		CSkinDlg::~CSkinDlg()
		{
		}

		void CSkinDlg::DoDataExchange(CDataExchange* pDX)
		{
			CPropertyPage::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_DRAW_EMPTY_DATA, m_pattern_draw_empty_data);
			DDX_Control(pDX, IDC_PATTERN_FONTFACE, m_pattern_fontface);
			DDX_Control(pDX, IDC_PATTERN_FONT_POINT, m_pattern_font_point);
			DDX_Control(pDX, IDC_PATTERN_FONT_X, m_pattern_font_x);
			DDX_Control(pDX, IDC_PATTERN_FONT_Y, m_pattern_font_y);
			DDX_Control(pDX, IDC_LINE_NUMBERS, m_linenumbers);
			DDX_Control(pDX, IDC_LINE_NUMBERS_HEX, m_linenumbersHex);
			DDX_Control(pDX, IDC_LINE_NUMBERS_CURSOR, m_linenumbersCursor);
			DDX_Control(pDX, IDC_CENTERCURSOR, m_centercursor);
			DDX_Control(pDX, IDC_COMBO1, m_timesig);
			DDX_Control(pDX, IDC_PATTERN_HEADER_SKIN, m_pattern_header_skin);
			DDX_Control(pDX, IDC_SHOWA440, m_showA440);

			DDX_Control(pDX, IDC_MACHINE_FONTFACE, m_generator_fontface);
			DDX_Control(pDX, IDC_MACHINE_FONT_POINT, m_generator_font_point);
			DDX_Control(pDX, IDC_MACHINE_FONTFACE2, m_effect_fontface);
			DDX_Control(pDX, IDC_MACHINE_FONT_POINT2, m_effect_font_point);
			DDX_Control(pDX, IDC_DRAW_MAC_INDEX, m_draw_mac_index);
			DDX_Control(pDX, IDC_WIRE_WIDTH, m_wirewidth);
			DDX_Control(pDX, IDC_WIREAA, m_wireaa);
			DDX_Control(pDX, IDC_TRIANGLESIZE, m_triangle_size);
			DDX_Control(pDX, IDC_MACHINE_SKIN, m_machine_skin);
			DDX_Control(pDX, IDC_MACHINE_BITMAP, m_machine_background_bitmap);
			DDX_Control(pDX, IDC_MACHINEGUI_BITMAP, m_machine_GUI_bitmap);
			DDX_Control(pDX, IDC_CHECK_VUS, m_draw_vus);
		}

/*
		BEGIN_MESSAGE_MAP(CSkinDlg, CPropertyPage)
			ON_WM_CLOSE()
			ON_WM_TIMER()
			ON_BN_CLICKED(IDC_BG_COLOUR, OnColourMachine)
			ON_BN_CLICKED(IDC_WIRE_COLOUR, OnColourWire)
			ON_BN_CLICKED(IDC_POLY_COLOUR, OnColourPoly)
			ON_BN_CLICKED(IDC_VUBAR_COLOUR, OnVuBarColor)
			ON_BN_CLICKED(IDC_VUBACK_COLOUR, OnVuBackColor)
			ON_BN_CLICKED(IDC_VUCLIP_COLOUR, OnVuClipBar)
			ON_BN_CLICKED(IDC_PATTERNBACKC, OnButtonPattern)
			ON_BN_CLICKED(IDC_PATTERNBACKC2, OnButtonPattern2)
			ON_BN_CLICKED(IDC_PATSEPARATORC, OnButtonPatternSeparator)
			ON_BN_CLICKED(IDC_PATSEPARATORC2, OnButtonPatternSeparator2)
			ON_BN_CLICKED(IDC_ROWC, OnRowc)
			ON_BN_CLICKED(IDC_ROWC2, OnRowc2)
			ON_BN_CLICKED(IDC_FONTC, OnFontc)
			ON_BN_CLICKED(IDC_FONTC2, OnFontc2)
			ON_BN_CLICKED(IDC_FONTSELC, OnFontSelc)
			ON_BN_CLICKED(IDC_FONTSELC2, OnFontSelc2)
			ON_BN_CLICKED(IDC_FONTPLAYC, OnFontPlayc)
			ON_BN_CLICKED(IDC_FONTPLAYC2, OnFontPlayc2)
			ON_BN_CLICKED(IDC_FONTCURSORC, OnFontCursorc)
			ON_BN_CLICKED(IDC_FONTCURSORC2, OnFontCursorc2)
			ON_BN_CLICKED(IDC_BEATC, OnBeatc)
			ON_BN_CLICKED(IDC_BEATC2, OnBeatc2)
			ON_BN_CLICKED(IDC_4BEAT, On4beat)
			ON_BN_CLICKED(IDC_4BEAT2, On4beat2)
			ON_BN_CLICKED(IDC_PLAYBARC, OnPlaybar)
			ON_BN_CLICKED(IDC_PLAYBARC2, OnPlaybar2)
			ON_BN_CLICKED(IDC_SELECTIONC, OnSelection)
			ON_BN_CLICKED(IDC_SELECTIONC2, OnSelection2)
			ON_BN_CLICKED(IDC_CURSORC, OnCursor)
			ON_BN_CLICKED(IDC_CURSORC2, OnCursor2)
			ON_BN_CLICKED(IDC_SHOWA440, OnShowA440)
			ON_BN_CLICKED(IDC_CHECK_VUS, OnCheckVus)
			ON_BN_CLICKED(IDC_LINE_NUMBERS, OnLineNumbers)
			ON_BN_CLICKED(IDC_LINE_NUMBERS_HEX, OnLineNumbersHex)
			ON_BN_CLICKED(IDC_LINE_NUMBERS_CURSOR, OnLineNumbersCursor)
			ON_BN_CLICKED(IDC_DEFAULTSKIN, OnDefaults)
			ON_BN_CLICKED(IDC_IMPORTREG, OnImportReg)
			ON_BN_CLICKED(IDC_EXPORTREG, OnExportReg)
			ON_CBN_SELCHANGE(IDC_PATTERN_FONT_POINT, OnSelchangePatternFontPoint)
			ON_CBN_SELCHANGE(IDC_PATTERN_FONT_X, OnSelchangePatternFontX)
			ON_CBN_SELCHANGE(IDC_PATTERN_FONT_Y, OnSelchangePatternFontY)
			ON_BN_CLICKED(IDC_PATTERN_FONTFACE, OnPatternFontFace)
			ON_CBN_SELCHANGE(IDC_PATTERN_HEADER_SKIN, OnSelchangePatternHeaderSkin)
			ON_CBN_SELCHANGE(IDC_WIRE_WIDTH, OnSelchangeWireWidth)
			ON_CBN_SELCHANGE(IDC_MACHINE_SKIN, OnSelchangeMachineSkin)
			ON_CBN_SELCHANGE(IDC_WIREAA, OnSelchangeWireAA)
			ON_CBN_SELCHANGE(IDC_MACHINE_FONT_POINT, OnSelchangeGeneratorFontPoint)
			ON_BN_CLICKED(IDC_MACHINE_FONTFACE, OnGeneratorFontFace)
			ON_BN_CLICKED(IDC_MV_FONT_COLOUR, OnMVGeneratorFontColour)
			ON_CBN_SELCHANGE(IDC_MACHINE_FONT_POINT2, OnSelchangeEffectFontPoint)
			ON_BN_CLICKED(IDC_MACHINE_FONTFACE2, OnEffectFontFace)
			ON_BN_CLICKED(IDC_MV_FONT_COLOUR2, OnMVEffectFontColour)
			ON_BN_CLICKED(IDC_DRAW_EMPTY_DATA, OnDrawEmptyData)
			ON_BN_CLICKED(IDC_DRAW_MAC_INDEX, OnDrawMacIndex)
			ON_BN_CLICKED(IDC_MACHINE_BITMAP, OnMachineBitmap)
			ON_CBN_SELCHANGE(IDC_TRIANGLESIZE, OnSelchangeTrianglesize)
			ON_BN_CLICKED(IDC_MACHINEGUI_TOPFONTC, OnBnClickedMachineguiFontc)
			ON_BN_CLICKED(IDC_MACHINEGUI_TOPC, OnBnClickedMachineguiTopc)
			ON_BN_CLICKED(IDC_MACHINEGUI_BOTTOMC, OnBnClickedMachineguiBottomc)
			ON_BN_CLICKED(IDC_MACHINEGUI_BOTTOMFONTC, OnBnClickedMachineguiBottomfontc)
			ON_BN_CLICKED(IDC_MACHINEGUI_TITLEC, OnBnClickedMachineguiTitlec)
			ON_BN_CLICKED(IDC_MACHINEGUI_TITLEFONTC, OnBnClickedMachineguiTitlefontc2)
			ON_BN_CLICKED(IDC_MACHINEGUI_BITMAP, OnBnClickedMachineguiBitmap)
			ON_BN_CLICKED(IDC_MACHINEGUI_TOPC2, OnBnClickedMachineguiTopc2)
			ON_BN_CLICKED(IDC_MACHINEGUI_TOPFONTC2, OnBnClickedMachineguiTopfontc2)
			ON_BN_CLICKED(IDC_MACHINEGUI_BOTTOMC2, OnBnClickedMachineguiBottomc2)
			ON_BN_CLICKED(IDC_MACHINEGUI_BOTTOMFONTC2, OnBnClickedMachineguiBottomfontc2)
		END_MESSAGE_MAP()
*/

		BOOL CSkinDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			for(int i=4;i<=64;i++)
			{
				char s[4];
				sprintf(s,"%2i",i);
				m_pattern_font_x.AddString(s);
				m_pattern_font_y.AddString(s);
			}
			for (int i = 50; i <= 320; i+=5)
			{
				char s[8];
				if (i < 100)
				{
					sprintf(s," %.1f",float(i)/10.0f);
				}
				else
				{
					sprintf(s,"%.1f",float(i)/10.0f);
				}
				m_pattern_font_point.AddString(s);
				m_generator_font_point.AddString(s);
				m_effect_font_point.AddString(s);
			}

			char s[4];
			m_wireaa.AddString("off");
			for (int i = 1; i <= 16; i++)
			{
				sprintf(s,"%2i",i);
				m_wirewidth.AddString(s);
				m_wireaa.AddString(s);
			}
			for (int i = 8; i <= 64; i++)
			{
				sprintf(s,"%2i",i);
				m_triangle_size.AddString(s);
			}
			// ok now browse our folder for skins
			SkinIO::LocateSkins(PsycleGlobal::conf().GetAbsoluteSkinDir().c_str(), pattern_skins, machine_skins);

			m_pattern_header_skin.AddString(PSYCLE__PATH__DEFAULT_PATTERN_HEADER_SKIN);
			m_machine_skin.AddString(PSYCLE__PATH__DEFAULT_MACHINE_SKIN);
			std::list<std::string>::const_iterator it;
			for(it = pattern_skins.begin(); it != pattern_skins.end(); ++it) {
				std::string tmp = it->substr(it->rfind('\\')+1);
				m_pattern_header_skin.AddString(tmp.c_str());
			}
			for(it = machine_skins.begin(); it != machine_skins.end(); ++it) {
				std::string tmp = it->substr(it->rfind('\\')+1);
				m_machine_skin.AddString(tmp.c_str());
			}
			RefreshAllValues();

			SetTimer(ID_TIMER_SKINGDLG,100,0);
			return TRUE;
			// return TRUE unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return FALSE
		}

		void CSkinDlg::OnOK()
		{
			///\todo: Set all settings.
			patConfig._centerCursor = m_centercursor.GetCheck()?true:false;
			patConfig.timesig = m_timesig.GetCurSel()+1;
			KillTimer(ID_TIMER_SKINGDLG);
			CDialog::OnOK();
		}

		void CSkinDlg::OnTimer(UINT_PTR nIDEvent) 
		{
			if ( nIDEvent == ID_TIMER_SKINGDLG )
			{
				RepaintAllCanvas();
			}
			
			CPropertyPage::OnTimer(nIDEvent);
		}

		void CSkinDlg::OnCancel() 
		{
			KillTimer(ID_TIMER_SKINGDLG);
			CDialog::OnCancel();
		}

		void CSkinDlg::OnButtonPattern() {
			ChangeColour(IDC_PBG_CAN,patConfig.background);
		}
		void CSkinDlg::OnButtonPattern2() {
			ChangeColour(IDC_PBG_CAN2,patConfig.background2);
		}
		void CSkinDlg::OnButtonPatternSeparator() {
			ChangeColour(IDC_PBG_SEPARATOR,patConfig.separator);
		}
		void CSkinDlg::OnButtonPatternSeparator2() {
			ChangeColour(IDC_PBG_SEPARATOR2,patConfig.separator2);
		}
		void CSkinDlg::OnRowc() {
			ChangeColour(IDC_ROW_CAN,patConfig.row);
		}
		void CSkinDlg::OnRowc2() {
			ChangeColour(IDC_ROW_CAN2,patConfig.row2);
		}
		void CSkinDlg::OnFontc() {
			ChangeColour(IDC_FONT_CAN,patConfig.font);
		}
		void CSkinDlg::OnFontc2() {
			ChangeColour(IDC_FONT_CAN2,patConfig.font2);
		}
		void CSkinDlg::OnFontPlayc() {
			ChangeColour(IDC_FONTPLAY_CAN,patConfig.fontPlay);
		}
		void CSkinDlg::OnFontPlayc2() {
			ChangeColour(IDC_FONTPLAY_CAN2,patConfig.fontPlay2);
		}
		void CSkinDlg::OnFontSelc() {
			ChangeColour(IDC_FONTSEL_CAN,patConfig.fontSel);
		}
		void CSkinDlg::OnFontSelc2() {
			ChangeColour(IDC_FONTSEL_CAN2,patConfig.fontSel2);
		}
		void CSkinDlg::OnFontCursorc() {
			ChangeColour(IDC_FONTCURSOR_CAN,patConfig.fontCur);
		}
		void CSkinDlg::OnFontCursorc2() {
			ChangeColour(IDC_FONTCURSOR_CAN2,patConfig.fontCur2);
		}
		void CSkinDlg::OnBeatc() {
			ChangeColour(IDC_BEAT_CAN,patConfig.rowbeat);
		}
		void CSkinDlg::OnBeatc2() {
			ChangeColour(IDC_BEAT_CAN2,patConfig.rowbeat2);
		}
		void CSkinDlg::On4beat() {
			ChangeColour(IDC_4BEAT_CAN,patConfig.row4beat);
		}
		void CSkinDlg::On4beat2() {
			ChangeColour(IDC_4BEAT_CAN2,patConfig.row4beat2);
		}
		void CSkinDlg::OnSelection() {
			ChangeColour(IDC_SELECTION_CAN,patConfig.selection);
		}
		void CSkinDlg::OnSelection2() {
			ChangeColour(IDC_SELECTION_CAN2,patConfig.selection2);
		}
		void CSkinDlg::OnCursor() {
			ChangeColour(IDC_CURSOR_CAN,patConfig.cursor);
		}
		void CSkinDlg::OnCursor2() {
			ChangeColour(IDC_CURSOR_CAN2,patConfig.cursor2);
		}
		void CSkinDlg::OnPlaybar() {
			ChangeColour(IDC_PLAYBAR_CAN,patConfig.playbar);
		}
		void CSkinDlg::OnPlaybar2() {
			ChangeColour(IDC_PLAYBAR_CAN2,patConfig.playbar2);
		}

		void CSkinDlg::OnMVGeneratorFontColour() {
			ChangeColour(IDC_MBG_MV_FONT,macConfig.generator_fontcolour);
		}
		void CSkinDlg::OnMVEffectFontColour() {
			ChangeColour(IDC_MBG_MV_FONT2,macConfig.effect_fontcolour);
		}
		void CSkinDlg::OnColourMachine() {
			ChangeColour(IDC_MBG_CAN,macConfig.colour);
		}
		void CSkinDlg::OnColourWire() {
			ChangeColour(IDC_MWIRE_COL,macConfig.wirecolour);
		}
		void CSkinDlg::OnColourPoly() {
			ChangeColour(IDC_MPOLY_COL,macConfig.polycolour);
		}
		void CSkinDlg::OnVuBarColor() {
			ChangeColour(IDC_VU1_CAN,macConfig.vu1);
		}
		void CSkinDlg::OnVuBackColor() {
			ChangeColour(IDC_VU2_CAN,macConfig.vu2);
		}
		void CSkinDlg::OnVuClipBar() {
			ChangeColour(IDC_VU3_CAN,macConfig.vu3);
		}

		void CSkinDlg::OnBnClickedMachineguiFontc() {
			ChangeColour(IDC_MACHINEFONTTOP_CAN,paramConfig.fontTopColor);
		}
		void CSkinDlg::OnBnClickedMachineguiBottomfontc() {
			ChangeColour(IDC_MACHINEFONTBOTTOM_CAN,paramConfig.fontBottomColor);
		}
		void CSkinDlg::OnBnClickedMachineguiTopc() {
			ChangeColour(IDC_MACHINETOP_CAN,paramConfig.topColor);
		}
		void CSkinDlg::OnBnClickedMachineguiBottomc() {
			ChangeColour(IDC_MACHINEBOTTOM_CAN,paramConfig.bottomColor);
		}
		void CSkinDlg::OnBnClickedMachineguiTitlec() {
			ChangeColour(IDC_MACHINETITLE_CAN,paramConfig.titleColor);
		}
		void CSkinDlg::OnBnClickedMachineguiTitlefontc2() {
			ChangeColour(IDC_MACHINEFONTTITLE_CAN,paramConfig.fonttitleColor);
		}
		void CSkinDlg::OnBnClickedMachineguiTopc2() {
			ChangeColour(IDC_MACHINETOP_CAN2,paramConfig.hTopColor);
		}
		void CSkinDlg::OnBnClickedMachineguiTopfontc2() {
			ChangeColour(IDC_MACHINEFONTTOP_CAN2,paramConfig.fonthTopColor);
		}
		void CSkinDlg::OnBnClickedMachineguiBottomc2() {
			ChangeColour(IDC_MACHINEBOTTOM_CAN2,paramConfig.hBottomColor);
		}
		void CSkinDlg::OnBnClickedMachineguiBottomfontc2() {
			ChangeColour(IDC_MACHINEFONTBOTTOM_CAN2,paramConfig.fonthBottomColor);
		}

		void CSkinDlg::OnDrawEmptyData()
		{
			patConfig.draw_empty_data = m_pattern_draw_empty_data.GetCheck() >0?true:false;
		}
		void CSkinDlg::OnLineNumbers() 
		{
			patConfig._linenumbers = m_linenumbers.GetCheck() >0?true:false;
		}
		void CSkinDlg::OnLineNumbersCursor() 
		{
			patConfig._linenumbersCursor = m_linenumbersCursor.GetCheck() >0?true:false;
		}
		void CSkinDlg::OnLineNumbersHex() 
		{
			patConfig._linenumbersHex = m_linenumbersHex.GetCheck() >0?true:false;
		}
		void CSkinDlg::OnShowA440() 
		{
			patConfig.showA440 = m_showA440.GetCheck() >0?true:false;
		}

		void CSkinDlg::OnDrawMacIndex()
		{
			macConfig.draw_mac_index = m_draw_mac_index.GetCheck() >0?true:false;
		}
		void CSkinDlg::OnCheckVus() 
		{
			macConfig.draw_vus = m_draw_vus.GetCheck() >0?true:false;
		}


		void CSkinDlg::OnPatternFontFace()
		{
			ChangeFont(patConfig.font_name,
				patConfig.font_point, patConfig.font_flags);
		}

		void CSkinDlg::OnSelchangePatternFontPoint() 
		{
			patConfig.font_point=(m_pattern_font_point.GetCurSel()*5)+50;
		}
		void CSkinDlg::OnSelchangePatternFontX() 
		{
			patConfig.font_x=m_pattern_font_x.GetCurSel()+4;
		}
		void CSkinDlg::OnSelchangePatternFontY() 
		{
			patConfig.font_y=m_pattern_font_y.GetCurSel()+4;
		}
		void CSkinDlg::OnSelchangePatternHeaderSkin()
		{
			int pos = m_pattern_header_skin.GetCurSel();
			if(pos == 0) {
				patConfig.SetDefaultSkin();
			}
			else {
				std::list<std::string>::const_iterator it;
				int i;
				for(i=1,it=pattern_skins.begin() ; it != pattern_skins.end() ; ++i,++it)
				{
					if(i == pos)
					{
						SkinIO::LoadPatternSkin((*it + ".psh").c_str(),patConfig.PatHeaderCoords);
						patConfig.header_skin=*it;
						break;
					}
				}
			}
		}

		void CSkinDlg::OnSelchangeMachineSkin()
		{
			int pos = m_machine_skin.GetCurSel();
			if(pos == 0) {
				macConfig.SetDefaultSkin();
			}
			else {
				std::list<std::string>::const_iterator it;
				int i;
				for(i=1,it=machine_skins.begin() ; it != machine_skins.end() ; ++i,++it)
				{
					if(i == pos)
					{
						SkinIO::LoadMachineSkin((*it + ".psm").c_str(),macConfig.MachineCoords);
						macConfig.machine_skin=*it;
						break;
					}
				}
			}
		}
		void CSkinDlg::OnSelchangeWireWidth()
		{
			macConfig.wirewidth = m_wirewidth.GetCurSel()+1;
		}
		void CSkinDlg::OnSelchangeWireAA()
		{
			macConfig.wireaa = m_wireaa.GetCurSel();
		}
		void CSkinDlg::OnSelchangeTrianglesize() 
		{
			macConfig.triangle_size=m_triangle_size.GetCurSel()+8;
		}


		void CSkinDlg::OnGeneratorFontFace()
		{
			ChangeFont(macConfig.generator_fontface,
				macConfig.generator_font_point, macConfig.generator_font_flags);
		}
		void CSkinDlg::OnEffectFontFace()
		{
			ChangeFont(macConfig.effect_fontface, 
				macConfig.effect_font_point, macConfig.effect_font_flags);
		}

		void CSkinDlg::OnSelchangeGeneratorFontPoint() 
		{
			macConfig.generator_font_point=(m_generator_font_point.GetCurSel()*5)+50;
		}
		void CSkinDlg::OnSelchangeEffectFontPoint() 
		{
			macConfig.effect_font_point=(m_effect_font_point.GetCurSel()*5)+50;
		}

		void CSkinDlg::OnBnClickedMachineguiBitmap()
		{
			ChangeBitmap(paramConfig.szBmpControlsFilename, true);
			if (paramConfig.szBmpControlsFilename.empty())
			{
				m_machine_GUI_bitmap.SetWindowText("Default Dial Bitmap");
			}
			else
			{
				std::string str1 = paramConfig.szBmpControlsFilename;
				if (str1.find(".psc") != std::string::npos) {
					SkinIO::LoadControlsSkin(str1.c_str(), paramConfig.coords);
				}
				m_machine_GUI_bitmap.SetWindowText(str1.substr(str1.rfind('\\')+1).c_str());
			}
		}

		void CSkinDlg::OnMachineBitmap() 
		{
			ChangeBitmap(macConfig.szBmpBkgFilename);
			if (macConfig.szBmpBkgFilename.empty()) {
				m_machine_background_bitmap.SetWindowText("No Background Bitmap");
			}
			else
			{
				std::string str1 = macConfig.szBmpBkgFilename;
				m_machine_background_bitmap.SetWindowText(str1.substr(str1.rfind('\\')+1).c_str());
			}
		}

		void CSkinDlg::OnDefaults()
		{
			patConfig.SetDefaultSettings();
			macConfig.SetDefaultSettings();
			paramConfig.SetDefaultSettings();
			patConfig.RefreshSettings();
			macConfig.RefreshSettings();
			paramConfig.RefreshSettings();
			RefreshAllValues();
		}
		void CSkinDlg::OnImportReg() 
		{
			OPENFILENAME ofn = OPENFILENAME(); // common dialog box structure
			char szFile[MAX_PATH]; // buffer for file name
			szFile[0]='\0';
			std::string dir = PsycleGlobal::conf().GetAbsoluteSkinDir();
			// Initialize OPENFILENAME
			ofn.lStructSize = sizeof ofn;
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof szFile;
			ofn.lpstrFilter = "Psycle Display Presets\0*.psv\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = 0;
			ofn.nMaxFileTitle = 0;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			ofn.lpstrInitialDir = dir.c_str();
			// Display the Open dialog box. 
			if(::GetOpenFileName(&ofn))
			{
				SkinIO::LoadTheme(szFile, macConfig, paramConfig, patConfig);
				RefreshAllValues();
			}
		}

		void CSkinDlg::OnExportReg() 
		{
			OPENFILENAME ofn = OPENFILENAME(); // common dialog box structure
			char szFile[MAX_PATH]; // buffer for file name
			szFile[0]='\0';
			std::string dir = PsycleGlobal::conf().GetAbsoluteSkinDir();
			// Initialize OPENFILENAME
			ofn.lStructSize = sizeof ofn;
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof szFile;
			ofn.lpstrFilter = "Psycle Display Presets\0*.psv\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = 0;
			ofn.nMaxFileTitle = 0;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;	
			ofn.lpstrInitialDir = dir.c_str();
			if(::GetSaveFileName(&ofn))
			{
				SkinIO::SaveTheme(szFile, macConfig, paramConfig, patConfig);
			}
		}

		void CSkinDlg::RefreshAllValues()
		{
			m_pattern_draw_empty_data.SetCheck(patConfig.draw_empty_data);
			m_linenumbers.SetCheck(patConfig._linenumbers);
			m_linenumbersCursor.SetCheck(patConfig._linenumbersCursor);
			m_linenumbersHex.SetCheck(patConfig._linenumbersHex);
			m_centercursor.SetCheck(patConfig._centerCursor?1:0);
			m_timesig.SetCurSel(patConfig.timesig-1);

			m_showA440.SetCheck(patConfig.showA440);
			
			m_draw_mac_index.SetCheck(macConfig.draw_mac_index);
			m_draw_vus.SetCheck(macConfig.draw_vus);

			m_pattern_font_x.SetCurSel(patConfig.font_x-4);
			m_pattern_font_y.SetCurSel(patConfig.font_y-4);

			SetFontNames();

			m_wirewidth.SetCurSel(macConfig.wirewidth-1);
			m_wireaa.SetCurSel(macConfig.wireaa);

			m_triangle_size.SetCurSel(macConfig.triangle_size-8);
			
			bool found = false;
			std::list<std::string>::const_iterator it;
			int i;
			for(i=1, it = pattern_skins.begin(); it != pattern_skins.end(); ++i, ++it) {
				if(*it == patConfig.header_skin) {
					found = true;
					break;
				}
			}
			if(found) { m_pattern_header_skin.SetCurSel(i); }
			else { m_pattern_header_skin.SetCurSel(0); }

			found = false;
			for(i=1, it = machine_skins.begin(); it != machine_skins.end(); ++i, ++it) {
				if(*it == macConfig.machine_skin) {
					found = true;
					break;
				}
			}
			if(found) { m_machine_skin.SetCurSel(i); }
			else { m_machine_skin.SetCurSel(0); }

			if (macConfig.szBmpBkgFilename.empty())
			{
				m_machine_background_bitmap.SetWindowText("No Background Bitmap");
			}
			else
			{
				std::string str1 = macConfig.szBmpBkgFilename;
				m_machine_background_bitmap.SetWindowText(str1.substr(str1.rfind('\\')+1).c_str());
			}

			if (paramConfig.szBmpControlsFilename.empty())
			{
				m_machine_GUI_bitmap.SetWindowText("Default Dial Bitmap");
			}
			else
			{
				std::string str1 = paramConfig.szBmpControlsFilename;
				m_machine_GUI_bitmap.SetWindowText(str1.substr(str1.rfind('\\')+1).c_str());
			}
		}

		void CSkinDlg::ChangeColour(UINT id, COLORREF& colour)
		{
			CColorDialog dlg(colour);
			if(dlg.DoModal() == IDOK)
			{
				colour = dlg.GetColor();
				UpdateCanvasColour(id, colour);
			}
		}
		void CSkinDlg::RepaintAllCanvas()
		{
			UpdateCanvasColour(IDC_MBG_CAN,macConfig.colour);
			UpdateCanvasColour(IDC_MWIRE_COL,macConfig.wirecolour);
			UpdateCanvasColour(IDC_MPOLY_COL,macConfig.polycolour);
			UpdateCanvasColour(IDC_MBG_MV_FONT,macConfig.generator_fontcolour);
			UpdateCanvasColour(IDC_MBG_MV_FONT2,macConfig.effect_fontcolour);
			UpdateCanvasColour(IDC_VU1_CAN,macConfig.vu1);
			UpdateCanvasColour(IDC_VU2_CAN,macConfig.vu2);
			UpdateCanvasColour(IDC_VU3_CAN,macConfig.vu3);
			UpdateCanvasColour(IDC_PBG_CAN,patConfig.background);
			UpdateCanvasColour(IDC_PBG_CAN2,patConfig.background2);
			UpdateCanvasColour(IDC_PBG_SEPARATOR,patConfig.separator);
			UpdateCanvasColour(IDC_PBG_SEPARATOR2,patConfig.separator2);
			UpdateCanvasColour(IDC_4BEAT_CAN,patConfig.row4beat);
			UpdateCanvasColour(IDC_4BEAT_CAN2,patConfig.row4beat2);
			UpdateCanvasColour(IDC_BEAT_CAN,patConfig.rowbeat);
			UpdateCanvasColour(IDC_BEAT_CAN2,patConfig.rowbeat2);
			UpdateCanvasColour(IDC_ROW_CAN,patConfig.row);
			UpdateCanvasColour(IDC_ROW_CAN2,patConfig.row2);
			UpdateCanvasColour(IDC_FONT_CAN,patConfig.font);
			UpdateCanvasColour(IDC_FONT_CAN2,patConfig.font2);
			UpdateCanvasColour(IDC_FONTPLAY_CAN,patConfig.fontPlay);
			UpdateCanvasColour(IDC_FONTPLAY_CAN2,patConfig.fontPlay2);
			UpdateCanvasColour(IDC_FONTCURSOR_CAN,patConfig.fontCur);
			UpdateCanvasColour(IDC_FONTCURSOR_CAN2,patConfig.fontCur2);
			UpdateCanvasColour(IDC_FONTSEL_CAN,patConfig.fontSel);
			UpdateCanvasColour(IDC_FONTSEL_CAN2,patConfig.fontSel2);
			UpdateCanvasColour(IDC_CURSOR_CAN,patConfig.cursor);
			UpdateCanvasColour(IDC_CURSOR_CAN2,patConfig.cursor2);
			UpdateCanvasColour(IDC_PLAYBAR_CAN,patConfig.playbar);
			UpdateCanvasColour(IDC_PLAYBAR_CAN2,patConfig.playbar2);
			UpdateCanvasColour(IDC_SELECTION_CAN,patConfig.selection);
			UpdateCanvasColour(IDC_SELECTION_CAN2,patConfig.selection2);
			UpdateCanvasColour(IDC_MACHINETOP_CAN,paramConfig.topColor);
			UpdateCanvasColour(IDC_MACHINEFONTTOP_CAN,paramConfig.fontTopColor);
			UpdateCanvasColour(IDC_MACHINEBOTTOM_CAN,paramConfig.bottomColor);
			UpdateCanvasColour(IDC_MACHINEFONTBOTTOM_CAN,paramConfig.fontBottomColor);
			UpdateCanvasColour(IDC_MACHINETOP_CAN2,paramConfig.hTopColor);
			UpdateCanvasColour(IDC_MACHINEFONTTOP_CAN2,paramConfig.fonthTopColor);
			UpdateCanvasColour(IDC_MACHINEBOTTOM_CAN2,paramConfig.hBottomColor);
			UpdateCanvasColour(IDC_MACHINEFONTBOTTOM_CAN2,paramConfig.fonthBottomColor);
			UpdateCanvasColour(IDC_MACHINETITLE_CAN,paramConfig.titleColor);
			UpdateCanvasColour(IDC_MACHINEFONTTITLE_CAN,paramConfig.fonttitleColor);		
		}

		void CSkinDlg::UpdateCanvasColour(int id,COLORREF col)
		{
			CStatic *obj=(CStatic *)GetDlgItem(id);
			CClientDC can(obj);
			CRect rect;
			obj->GetClientRect(rect);
			can.FillSolidRect(rect,col);
		}

		void CSkinDlg::ChangeFont(std::string& face, int& point, UINT& flags)
		{
			LOGFONT lf;
			ZeroMemory(&lf, sizeof(LOGFONT));
			CClientDC dc(this);
			lf.lfHeight = -MulDiv(point/10, dc.GetDeviceCaps(LOGPIXELSY), 72);
			_tcscpy_s(lf.lfFaceName, LF_FACESIZE, _T(face.c_str()));
			if (flags&1)
			{
				lf.lfWeight = FW_BOLD;
			}
			lf.lfItalic = (flags&2)?true:false;
			lf.lfQuality = DEFAULT_QUALITY;
			
			CFontDialog dlg(&lf,CF_SCREENFONTS);
			if (dlg.DoModal() == IDOK)
			{
				face = dlg.GetFaceName();
				flags = 0;
				if (dlg.IsBold())
				{
					flags |= 1;
				}
				if (dlg.IsItalic())
				{
					flags |= 2;
				}
				point = dlg.GetSize();
				if (point > 320)
				{
					point = 320;
				}
				// get size, colour too
				SetFontNames();
			}
		}

		void CSkinDlg::SetFontNames()
		{
			m_pattern_fontface.SetWindowText(
				describeFont(patConfig.font_name, patConfig.font_flags).c_str());
			m_generator_fontface.SetWindowText(
				describeFont(macConfig.generator_fontface, macConfig.generator_font_flags).c_str());
			m_effect_fontface.SetWindowText(
				describeFont(macConfig.effect_fontface, macConfig.effect_font_flags).c_str());

			m_pattern_font_point.SetCurSel((patConfig.font_point-50)/5);
			m_generator_font_point.SetCurSel((macConfig.generator_font_point-50)/5);
			m_effect_font_point.SetCurSel((macConfig.effect_font_point-50)/5);
		}
		
		std::string CSkinDlg::describeFont(std::string name, UINT flags)
		{
			std::string fontDesc = name;
			if (flags & 1)
				fontDesc += " Bold";
			if (flags & 2)
				fontDesc += " Italic";
			return fontDesc;
		}

		void CSkinDlg::ChangeBitmap(std::string& filename, bool isDial/*=false*/)
		{
			OPENFILENAME ofn; // common dialog box structure
			char szFile[_MAX_PATH]; // buffer for file name
			char szPath[_MAX_PATH]; // buffer for file name
			szFile[0]='\0';
			szPath[0]='\0';
			if(filename.empty()) {
				std::strcpy(szPath,PsycleGlobal::conf().GetAbsoluteSkinDir().c_str());
			}
			else{
				std::string nametemp = filename;
				pathname_validate::validate(nametemp);

				CString str1(nametemp.c_str());
				int i = str1.ReverseFind('\\')+1;
				CString str2 = str1.Mid(i);
				std::strcpy(szFile,str2);
				std::strcpy(szPath,str1);
				szPath[i]=0;
			}
			// Initialize OPENFILENAME
			::ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			if (isDial) {
					ofn.lpstrFilter = "Control Skins (*.psc)\0*.psc\0Bitmaps (*.bmp)\0*.bmp\0";
			}
			else {
				ofn.lpstrFilter = "Bitmaps (*.bmp)\0*.bmp\0";
			}
			ofn.nFilterIndex = 0;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = szPath;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			// Display the Open dialog box. 
			if(GetOpenFileName(&ofn)==TRUE)
			{
				filename = szFile;
			}
			else
			{
				filename = "";
			}
		}
	}   // namespace
}   // namespace
