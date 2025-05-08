///\file
///\brief interface file for psycle::host::CSkinDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PsycleConfig.hpp"

namespace psycle { namespace host {

		/// skin config window.
		class CSkinDlg : public CPropertyPage
		{
			DECLARE_DYNCREATE(CSkinDlg)
		public:
			CSkinDlg();
			virtual ~CSkinDlg();
			enum { IDD = IDD_CONFIG_SKIN };

			CButton	m_pattern_draw_empty_data;
			CButton	m_pattern_fontface;
			CComboBox	m_pattern_font_point;
			CComboBox	m_pattern_font_x;
			CComboBox	m_pattern_font_y;
			CButton	m_linenumbers;
			CButton	m_linenumbersHex;
			CButton	m_linenumbersCursor;
			CButton	m_centercursor;
			// number of beats to show a full row in the pattern editor.
			CComboBox m_timesig;
			CComboBox	m_pattern_header_skin;
			CButton	m_showA440;
			CButton	m_draw_mac_index;
			CButton	m_generator_fontface;
			CComboBox	m_generator_font_point;
			CButton	m_effect_fontface;
			CComboBox	m_effect_font_point;
			CComboBox	m_wireaa;
			CComboBox	m_wirewidth;
			CComboBox	m_triangle_size;
			CComboBox	m_machine_skin;
			CButton	m_machine_background_bitmap;
			CButton	m_draw_vus;
			CButton	m_machine_GUI_bitmap;

		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual void OnOK( );
			virtual void OnCancel();
			virtual BOOL OnInitDialog();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnTimer(UINT_PTR nIDEvent);
			afx_msg void OnColourMachine();
			afx_msg void OnColourWire();
			afx_msg void OnColourPoly();
			afx_msg void OnVuBarColor();
			afx_msg void OnVuBackColor();
			afx_msg void OnVuClipBar();
			afx_msg void OnButtonPattern();
			afx_msg void OnButtonPattern2();
			afx_msg void OnButtonPatternSeparator();
			afx_msg void OnButtonPatternSeparator2();
			afx_msg void OnRowc();
			afx_msg void OnRowc2();
			afx_msg void OnFontc();
			afx_msg void OnFontc2();
			afx_msg void OnFontSelc();
			afx_msg void OnFontSelc2();
			afx_msg void OnFontPlayc();
			afx_msg void OnFontPlayc2();
			afx_msg void OnFontCursorc();
			afx_msg void OnFontCursorc2();
			afx_msg void OnBeatc();
			afx_msg void OnBeatc2();
			afx_msg void On4beat();
			afx_msg void On4beat2();
			afx_msg void OnPlaybar();
			afx_msg void OnPlaybar2();
			afx_msg void OnSelection();
			afx_msg void OnSelection2();
			afx_msg void OnCursor();
			afx_msg void OnCursor2();
			afx_msg void OnShowA440();
			afx_msg void OnLineNumbers();
			afx_msg void OnLineNumbersHex();
			afx_msg void OnLineNumbersCursor();
			afx_msg void OnDefaults();
			afx_msg void OnImportReg();
			afx_msg void OnExportReg();
			afx_msg void OnSelchangePatternFontPoint();
			afx_msg void OnSelchangePatternFontX();
			afx_msg void OnSelchangePatternFontY();
			afx_msg void OnPatternFontFace();
			afx_msg void OnSelchangePatternHeaderSkin();
			afx_msg void OnSelchangeWireWidth();
			afx_msg void OnSelchangeMachineSkin();
			afx_msg void OnSelchangeWireAA();
			afx_msg void OnSelchangeGeneratorFontPoint();
			afx_msg void OnGeneratorFontFace();
			afx_msg void OnMVGeneratorFontColour();
			afx_msg void OnSelchangeEffectFontPoint();
			afx_msg void OnEffectFontFace();
			afx_msg void OnMVEffectFontColour();
			afx_msg void OnDrawEmptyData();
			afx_msg void OnDrawMacIndex();
			afx_msg void OnMachineBitmap();
			afx_msg void OnSelchangeTrianglesize();
			afx_msg void OnCheckVus();
			afx_msg void OnBnClickedMachineguiFontc();
			afx_msg void OnBnClickedMachineguiTopc();
			afx_msg void OnBnClickedMachineguiBottomc();
			afx_msg void OnBnClickedMachineguiBottomfontc();
			afx_msg void OnBnClickedMachineguiTitlec();
			afx_msg void OnBnClickedMachineguiTitlefontc2();
			afx_msg void OnBnClickedMachineguiBitmap();
			afx_msg void OnBnClickedMachineguiTopc2();
			afx_msg void OnBnClickedMachineguiTopfontc2();
			afx_msg void OnBnClickedMachineguiBottomc2();
			afx_msg void OnBnClickedMachineguiBottomfontc2();
		private:
			bool BrowseTo(char *rpath);
			static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX *lpelfe,  NEWTEXTMETRICEX *lpntme, DWORD FontType,  LPARAM lParam);
			static int CALLBACK EnumFontFamExProc2(ENUMLOGFONTEX *lpelfe,  NEWTEXTMETRICEX *lpntme, DWORD FontType,  LPARAM lParam);
			static std::string describeFont(std::string name, UINT flags);
			void RefreshAllValues();
			void RepaintAllCanvas();
			void UpdateCanvasColour(int id,COLORREF col);
			void SetFontNames();
			void ChangeFont(std::string& face, int& point, UINT& flags);
			void ChangeColour(UINT id, COLORREF& colour);
			void ChangeBitmap(std::string& filename, bool isDial=false);

			///\todo: Implement a copy of it, instead of a reference.
			PsycleConfig::PatternView& patConfig;
			PsycleConfig::MachineView& macConfig;
			PsycleConfig::MachineParam& paramConfig;
			std::list<std::string> pattern_skins;
			std::list<std::string> machine_skins;
		};

	}   // namespace
}   // namespace
