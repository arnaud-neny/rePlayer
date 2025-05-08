///\file
///\brief interface file for psycle::host::CConfigDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "DirectoryDlg.hpp"
#include "SkinDlg.hpp"
#include "OutputDlg.hpp"
#include "MidiInputDlg.hpp"
#include "KeyConfigDlg.hpp"

namespace psycle { namespace host {

		class PsycleConfig;

		/// config window.
		class CConfigDlg : public CPropertySheet
		{
			DECLARE_DYNAMIC(CConfigDlg)
		public:
			CConfigDlg(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
			CConfigDlg(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
			virtual ~CConfigDlg();
		// Operations
		public:
			void AddControlPages();
		// Overrides
		public:
			virtual INT_PTR DoModal();
		// Implementation
		protected:
			CDirectoryDlg _dirDlg;
			CSkinDlg _skinDlg;
			COutputDlg _outputDlg;
			CMidiInputDlg _midiDlg;
			CKeyConfigDlg _keyDlg;

			PsycleConfig* _pConfig;
		protected:
			DECLARE_MESSAGE_MAP()
		};
	}   // namespace
}   // namespace
