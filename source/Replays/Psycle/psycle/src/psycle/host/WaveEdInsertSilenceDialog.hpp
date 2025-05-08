///\file
///\brief interface file for psycle::host::CWaveEdInsertSilenceDialog.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		class CWaveEdInsertSilenceDialog : public CDialog
		{
		// Construction
		public:
			CWaveEdInsertSilenceDialog(CWnd* pParent = 0);
			enum { IDD = IDD_WAVED_INSERTSILENCE };
			CEdit	m_time;
			CButton m_atStart;
			CButton m_atEnd;
			CButton m_atCursor;
		public:
			enum insertPosition
			{
				at_start=0,
				at_end,
				at_cursor
			};
			float timeInSecs;
			insertPosition insertPos;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
			virtual void OnCancel();
		protected:
			DECLARE_MESSAGE_MAP()
		};

	}   // namespace
}   // namespace
