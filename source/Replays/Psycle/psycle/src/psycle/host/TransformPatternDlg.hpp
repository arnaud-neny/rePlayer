#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

#include "Song.hpp"

// CTransformPatternDlg dialog
namespace psycle { namespace host {
		class Song;
		class CChildView;

		class CTransformPatternDlg : public CDialog
		{
			DECLARE_DYNAMIC(CTransformPatternDlg)

		private:
			Song& song;
			CChildView& cview;
		public:
			CTransformPatternDlg(Song& _pSong, CChildView& _cview, CWnd* pParent = NULL);   // standard constructor
			virtual ~CTransformPatternDlg();

			enum { IDD = IDD_TRANSFORMPATTERN };			
			CComboBox m_searchnote;
			CComboBox m_searchinst;
			CComboBox m_searchmach;
			CComboBox m_replacenote;
			CComboBox m_replaceinst;
			CComboBox m_replacemach;
			CButton m_replacetweak;
			int m_applyto;
			CButton m_includePatNotInSeq;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
		protected:
		public:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnBnClickedSearch();
			afx_msg void OnBnClickedReplace();

		};

}   // namespace
}   // namespace
