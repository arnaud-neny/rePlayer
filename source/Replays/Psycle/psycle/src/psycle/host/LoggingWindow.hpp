///\interface psycle::host::CLoggingWindow.
#pragma once
#include "Psycle.hpp"
#include <vector>
#include <string>
NAMESPACE__BEGIN(psycle) NAMESPACE__BEGIN(host)

class CMainFrame;

/// logging window.
class CLoggingWindow : public CDialog
{
	public:
		void AddEntry(const int level, const std::string & string);

	private:
		void ResizeTextBox();
		CHARFORMAT defaultCF;
		CHARRANGE charrange;
		class LogEntry {
			public:
				int level;
				std::string string;
				LogEntry(const int level_, const std::string & string_)
					: level(level_), string(string_) {}
		};
		std::vector<LogEntry> LogVector;

	public:
		CLoggingWindow(CWnd* pParent = 0);
		CMainFrame * pParentMain;
		// Dialog Data
		//{{AFX_DATA(CLoggingWindow)
			enum { IDD = IDD_ERRORLOGGER };
			CRichEditCtrl m_ErrorTxt;
		//}}AFX_DATA

		//{{AFX_VIRTUAL(CLoggingWindow)
			protected:
				virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
		//}}AFX_VIRTUAL

	protected:
		//{{AFX_MSG(CLoggingWindow)
			virtual BOOL OnInitDialog();
			afx_msg void OnClose();
		//}}AFX_MSG
		DECLARE_MESSAGE_MAP()

	public:
		afx_msg void OnSize(UINT nType, int cx, int cy);
};

NAMESPACE__END NAMESPACE__END

