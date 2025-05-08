///\file
///\brief implementation file for psycle::host::CPatDlg.
#include <psycle/host/detail/project.private.hpp>
#include "PatDlg.hpp"
#include "PsycleConfig.hpp"
#include "Song.hpp"
#include <sstream>

namespace psycle { namespace host {

		CPatDlg::CPatDlg(Song& song, CWnd* pParent) : CDialog(CPatDlg::IDD, pParent)
			, m_song(song)
		{
			m_adaptsize = FALSE;
			bInit = FALSE;
		}

		void CPatDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_CHECK1, m_adaptsizeCheck);
			DDX_Control(pDX, IDC_PAT_NUMLINES, m_numlines);
			DDX_Control(pDX, IDC_EDIT1, m_patname);
			DDX_Control(pDX, IDC_SPIN1, m_spinlines);
			DDX_Control(pDX, IDC_TEXT, m_text);
			DDX_Check(pDX, IDC_CHECK1, m_adaptsize);
			DDX_Radio(pDX, IDC_HEADER_HEADER, m_shownames);
			DDX_Radio(pDX, IDC_NAMES_SHARE, m_independentnames);
			DDX_Control(pDX, IDC_TRACKLIST, m_tracklist);
			DDX_Control(pDX, IDC_TRACKEDIT, m_trackedit);
			DDX_Control(pDX, IDC_COPY_BUTTON, m_copybutton);			
			DDX_Control(pDX, IDC_PATNAMES_COMBO, m_patternlist);			
		}

/*
		BEGIN_MESSAGE_MAP(CPatDlg, CDialog)
			ON_BN_CLICKED(IDC_CHECK1, OnAdaptContent)
			ON_BN_CLICKED(IDC_HEADER_HEADER, OnHeaderHeader)
			ON_BN_CLICKED(IDC_HEADER_NAMES, OnHeaderNames)
			ON_BN_CLICKED(IDC_NAMES_INDIVIDUAL, OnNotShareNames)
			ON_BN_CLICKED(IDC_NAMES_SHARE, OnShareNames)
			ON_BN_CLICKED(IDC_COPY_BUTTON, OnCopyNames)
			ON_EN_CHANGE(IDC_PAT_NUMLINES, OnChangeNumLines)
			ON_EN_CHANGE(IDC_TRACKEDIT, OnChangeTrackEdit)
			ON_LBN_SELCHANGE(IDC_TRACKLIST, OnSelchangeTracklist)
		END_MESSAGE_MAP()
*/

		BOOL CPatDlg::OnInitDialog() 
		{
			char buffer[16];
			CDialog::OnInitDialog();
			m_patname.SetLimitText(30);
			m_spinlines.SetRange(1,MAX_LINES);
			UDACCEL acc[2];
			acc[0].nSec = 0;
			acc[0].nInc = 4;
			acc[1].nSec = 1;
			acc[1].nInc = 16;
			m_spinlines.SetAccel(2, acc);

			m_patname.SetWindowText(patName);
			std::ostringstream os;
			os << patLines;
			m_numlines.SetWindowText(os.str().c_str());
			sprintf(buffer,"HEX: %x",patLines);
			m_text.SetWindowText(buffer);

			FillTrackList();
			FillPatternCombo();
			prevsel = m_tracklist.GetCurSel();
			m_trackedit.SetWindowText(tracknames[prevsel].c_str());

			if(m_independentnames) {
				OnNotShareNames();
			}
			// Pass the focus to the texbox
			m_patname.SetFocus();
			m_patname.SetSel(0,-1);
			bInit = TRUE;

			return FALSE;
		}

		void CPatDlg::OnOK() 
		{
			char buffer[32];
			m_numlines.GetWindowText(buffer,16);
			
			int nlines = atoi(buffer);
			if (nlines < 1)
				{ nlines = 1; }
			else if (nlines > MAX_LINES)
				{ nlines = MAX_LINES; }

			patLines=nlines;

			m_patname.GetWindowText(buffer,31);
			buffer[31]='\0';
			strcpy(patName,buffer);

			CString text;
			m_trackedit.GetWindowText(text);
			std::stringstream ss;
			ss << text;
			tracknames[prevsel] = ss.str();

			CDialog::OnOK();
		}

		void CPatDlg::OnAdaptContent() 
		{
			m_adaptsize = m_adaptsizeCheck.GetCheck();
		}

		void CPatDlg::OnChangeNumLines() 
		{
			char buffer[256];
			if (bInit)
			{
				m_numlines.GetWindowText(buffer,16);
				int val=atoi(buffer);

				if (val < 0)
				{
					val = 0;
				}
				else if(val > MAX_LINES)
				{
					val = MAX_LINES-1;
				}
				sprintf(buffer,"HEX: %x",val);
				m_text.SetWindowText(buffer);
			}
		}
		void CPatDlg::OnHeaderHeader()
		{
			m_shownames = 0;
		}
		void CPatDlg::OnHeaderNames()
		{
			m_shownames = 1;
		}
		void CPatDlg::OnShareNames()
		{
			m_independentnames = 0;
			m_copybutton.EnableWindow(FALSE);
			m_patternlist.EnableWindow(FALSE);
		}
		void CPatDlg::OnNotShareNames()
		{
			m_independentnames = 1;
			m_copybutton.EnableWindow(TRUE);
			m_patternlist.EnableWindow(TRUE);
		}
		void CPatDlg::OnCopyNames()
		{
			int sel = m_patternlist.GetCurSel();
			if (sel == -1) {
				MessageBox("No pattern selected!\nNames can only be copied from patterns with data","Copy pattern names", MB_ICONERROR|MB_OK);
				return;
			}
			m_song.CopyNamesFrom(sel,patIdx);
			FillTrackList();
			bInit = false;
			m_trackedit.SetWindowText(tracknames[prevsel].c_str());
			bInit = true;
		}
		void CPatDlg::OnChangeTrackEdit()
		{
			CString text;
			char buffer[256];
			if (bInit)
			{
				m_trackedit.GetWindowText(text);

				m_tracklist.DeleteString(prevsel);
				sprintf(buffer,"%.2d: %s",prevsel,text);
				m_tracklist.InsertString(prevsel,buffer);
				m_tracklist.SetCurSel(prevsel);
			}
		}
		void CPatDlg::OnSelchangeTracklist()
		{
			CString text;
			m_trackedit.GetWindowText(text);
			std::stringstream ss;
			ss << text;
			tracknames[prevsel] = ss.str();

			prevsel = m_tracklist.GetCurSel();
			bInit = false;
			m_trackedit.SetWindowText(tracknames[prevsel].c_str());
			bInit = true;
		}
		void CPatDlg::FillTrackList() 
		{
			char buffer[256];
			for(int i(0); i<m_song.SONGTRACKS; i++) 
			{
				tracknames[i] = m_song._trackNames[patIdx][i];
			}
			m_tracklist.ResetContent();
			for(int i(0); i<m_song.SONGTRACKS; i++) 
			{
				sprintf(buffer,"%.2d: %s",i,tracknames[i].c_str());
				m_tracklist.AddString(buffer);
			}
			m_tracklist.SetCurSel(0);
		}
		void CPatDlg::FillPatternCombo() 
		{
			char buffer[256];
			m_patternlist.ResetContent();
			int lastPatternUsed = m_song.GetHighestPatternIndexInSequence();
			for(int i(0); i<=lastPatternUsed; i++) 
			{
				if(!m_song.IsPatternEmpty(i)) {
					sprintf(buffer,"%.2d: %s",i,m_song.patternName[i]);
					m_patternlist.AddString(buffer);
				}
			}
			m_patternlist.SetCurSel(0);
		}
	}   // namespace
}   // namespace
