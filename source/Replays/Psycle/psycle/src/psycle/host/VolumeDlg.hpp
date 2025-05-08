///\file
///\brief interface file for psycle::host::CVolumeDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
		/// volume window.
		class CVolumeDlg : public CDialog
		{
		public:
			CVolumeDlg(CWnd* pParent = 0);
			float volume;
			int edit_type;
			enum { IDD = IDD_NEW_VOLUME };
			CEdit		m_db;
			CEdit		m_per;
		protected:
			void DrawDb();
			void DrawPer();
			bool go;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnChangeEditDb();
			afx_msg void OnChangeEditPer();
		};

	}   // namespace
}   // namespace
