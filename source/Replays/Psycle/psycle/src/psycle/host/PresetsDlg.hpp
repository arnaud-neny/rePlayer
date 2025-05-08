///\file
///\brief interface file for psycle::host::CPresetsDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PresetIO.hpp"
#include <cstring>

namespace psycle {
namespace host {

		class Machine;

		/// machine parameter preset window.
		class CPresetsDlg : public CDialog
		{
		public:
			CPresetsDlg(CWnd* pParent = 0);
			enum { IDD = IDD_PRESETS };
			CButton	m_exportButton;
			CButton	m_preview;
			CComboBox	m_preslist;
			Machine* _pMachine;
		protected:
			void SavePresets();
			void ReadPresets();
			void UpdateList();

			CPreset iniPreset;
			std::list<CPreset> presets;
			int numParameters;
			long int dataSizeStruct;
			CString presetFile;
			bool presetChanged;

		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual void OnOK();
			virtual void OnCancel();
			virtual BOOL OnInitDialog();

			DECLARE_MESSAGE_MAP()
			afx_msg void OnSave();
			afx_msg void OnDelete();
			afx_msg void OnImport();
			afx_msg void OnSelchangePresetslist();
			afx_msg void OnDblclkPresetslist();
			afx_msg void OnExport();
		};

	}   // namespace host
}   // namespace psycle
