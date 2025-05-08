///\file
///\brief interface file for psycle::host::CNewMachine.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

#include "plugincatcher.hpp"
#include <universalis/stdlib/cstdint.hpp>

#include <iostream>
#include <typeinfo>
#include <map>

namespace psycle { namespace host {


		const int MAX_BROWSER_NODES = 64;

		enum selectionclasses
		{
			internal=0,
			native=1,
			vstmac=2,
			luascript=2,
			ladspa=4
		};
		enum selectionmodes
		{
			modegen=0,
			modefx
		};
		/// new machine dialog window.
		class CNewMachine : public CDialog
		{
		public:
			CNewMachine(CWnd* pParent = 0);
			virtual ~CNewMachine();
			enum { IDD = IDD_NEWMACHINE };

		public:
			int Outputmachine;
			std::string psOutputDll;
			int32_t shellIdx;
			static int pluginOrder;
			static bool pluginName;
			static int selectedClass;
			static int selectedMode;

		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnSelchangedBrowser(NMHDR* pNMHDR, LRESULT* pResult);
			afx_msg void OnRefresh();
			afx_msg void OnByclass();
			afx_msg void OnBytype();
			afx_msg void OnDblclkBrowser(NMHDR* pNMHDR, LRESULT* pResult);
			afx_msg void OnDestroy();
			afx_msg void OnShowdllname();
			afx_msg void OnShoweffname();
			afx_msg void OnLoadNewBlitz();
			afx_msg void OnCheckAllow();
			afx_msg void OnEnChangeEdit1();
			afx_msg void OnStnClickedNamelabel();
			afx_msg void OnEnChangeRichedit21();
			afx_msg void OnScanNew();
		private:
			void UpdateList(bool bInit = false);

			CButton	m_Allow;
			CButton	m_LoadNewBlitz;
			CStatic	m_nameLabel;
			CTreeCtrl	m_browser;
			CStatic	m_versionLabel;
			CStatic	m_descLabel;
			int		m_orderby;
			CStatic	m_dllnameLabel;
			int		m_showdllName;
			CStatic m_APIversionLabel;

			CImageList imgList;
			HTREEITEM tHand;

			bool bAllowChanged;
			HTREEITEM hNodes[MAX_BROWSER_NODES];
			HTREEITEM hInt[7];
			HTREEITEM hPlug[MAX_BROWSER_PLUGINS];

		};

	}   // namespace
}   // namespace
