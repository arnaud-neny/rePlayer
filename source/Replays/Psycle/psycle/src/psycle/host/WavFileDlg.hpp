///\file
///\brief interface file for psycle::host::CWavFileDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		class Song;

		/// wave file dialog window.
		class CWavFileDlg : public CFileDialog
		{
			DECLARE_DYNAMIC(CWavFileDlg)
		public:
			Song* m_pSong;
			CString _lastFile;
			CWavFileDlg(
				BOOL bOpenFileDialog, // TRUE for FileOpen, FALSE for FileSaveAs
				LPCTSTR lpszDefExt = NULL,
				LPCTSTR lpszFileName = NULL,
				DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
				LPCTSTR lpszFilter = NULL,
				CWnd* pParentWnd = NULL);

			virtual void OnFileNameChange();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
		};
	}   // namespace
}   // namespace
