// TransformPatternDlg.cpp : implementation file
#include <psycle/host/detail/project.private.hpp>
#include "TransformPatternDlg.hpp"
#include "Song.hpp"
#include "ChildView.hpp"
#include "MainFrm.hpp"
namespace psycle { namespace host {

		static const char notes[12][3]={"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
		static const char *empty ="Empty";
		static const char *nonempty="Nonempty";
		static const char *all="All";
		static const char *same="Same";
		static const char *off="off";
		static const char *twk="twk";
		static const char *tws="tws";
		static const char *mcm="mcm";

		// CTransformPatternDlg dialog
		IMPLEMENT_DYNAMIC(CTransformPatternDlg, CDialog)

		CTransformPatternDlg::CTransformPatternDlg(Song& _pSong, CChildView& _cview, CWnd* pParent /*=NULL*/)
			: CDialog(CTransformPatternDlg::IDD, pParent)
			, song(_pSong), cview(_cview), m_applyto(0)
		{
		}

		CTransformPatternDlg::~CTransformPatternDlg()
		{
		}

		void CTransformPatternDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_SEARCHNOTECOMB, m_searchnote);
			DDX_Control(pDX, IDC_SEARCHINSTCOMB, m_searchinst);
			DDX_Control(pDX, IDC_SEARCHMACHCOMB, m_searchmach);
			DDX_Control(pDX, IDC_REPLNOTECOMB, m_replacenote);
			DDX_Control(pDX, IDC_REPLINSTCOMB, m_replaceinst);
			DDX_Control(pDX, IDC_REPLMACHCOMB, m_replacemach);
			DDX_Control(pDX, IDC_REPLTWEAKCHECK, m_replacetweak);
			DDX_Radio(pDX,IDC_APPLYTOSONG, m_applyto);
			DDX_Control(pDX, IDC_CH_INCLUDEPAT, m_includePatNotInSeq);			
		}


/*
		BEGIN_MESSAGE_MAP(CTransformPatternDlg, CDialog)
			ON_BN_CLICKED(IDD_SEARCH, &CTransformPatternDlg::OnBnClickedSearch)
			ON_BN_CLICKED(IDD_REPLACE, &CTransformPatternDlg::OnBnClickedReplace)
		END_MESSAGE_MAP()
*/

		BOOL CTransformPatternDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();

			//Note (search and replace)
			m_searchnote.AddString(all);			m_searchnote.SetItemData(0,1003);
			m_searchnote.AddString(empty);			m_searchnote.SetItemData(1,1001);
			m_searchnote.AddString(nonempty);		m_searchnote.SetItemData(2,1002);
			m_replacenote.AddString(same);			m_replacenote.SetItemData(0,1002);
			m_replacenote.AddString(empty);			m_replacenote.SetItemData(1,1001);
			bool is440 = PsycleGlobal::conf().patView().showA440;
			for (int i=notecommands::c0; i <= notecommands::b9;i++) {
				std::ostringstream os;
				os << notes[i%12];
				if (is440) os << (i/12)-1;
				else os << (i/12);
				m_searchnote.AddString(os.str().c_str());		m_searchnote.SetItemData(3+i,i);
				m_replacenote.AddString(os.str().c_str());		m_replacenote.SetItemData(2+i,i);
			}
			m_searchnote.AddString(off);			m_searchnote.SetItemData(123,notecommands::release);
			m_searchnote.AddString(twk);			m_searchnote.SetItemData(124,notecommands::tweak);
			m_searchnote.AddString(tws);			m_searchnote.SetItemData(125,notecommands::tweakslide);
			m_searchnote.AddString(mcm);			m_searchnote.SetItemData(126,notecommands::midicc);
			m_replacenote.AddString(off);			m_replacenote.SetItemData(122,notecommands::release);
			m_replacenote.AddString(twk);			m_replacenote.SetItemData(123,notecommands::tweak);
			m_replacenote.AddString(tws);			m_replacenote.SetItemData(124,notecommands::tweakslide);
			m_replacenote.AddString(mcm);			m_replacenote.SetItemData(125,notecommands::midicc);

			m_searchnote.SetCurSel(0);
			m_replacenote.SetCurSel(0);

			//Inst (search and replace)
			m_searchinst.AddString(all);			m_searchinst.SetItemData(0,1003);
			m_searchinst.AddString(empty);			m_searchinst.SetItemData(1,1001);
			m_searchinst.AddString(nonempty);		m_searchinst.SetItemData(2,1002);
			m_replaceinst.AddString(same);			m_replaceinst.SetItemData(0,1002);
			m_replaceinst.AddString(empty);			m_replaceinst.SetItemData(1,1001);
			for (int i=0; i < 0xFF; i++) {
				std::ostringstream os;
				if (i < 16) os << "0";
				os << std::uppercase << std::hex << i;
				m_searchinst.AddString(os.str().c_str());		m_searchinst.SetItemData(3+i,i);
				m_replaceinst.AddString(os.str().c_str());		m_replaceinst.SetItemData(2+i,i);
			}
			m_searchinst.SetCurSel(0);
			m_replaceinst.SetCurSel(0);

			//Mach (search and replace)
			m_searchmach.AddString(all);			m_searchmach.SetItemData(0,1003);
			m_searchmach.AddString(empty);			m_searchmach.SetItemData(1,1001);
			m_searchmach.AddString(nonempty);		m_searchmach.SetItemData(2,1002);
			m_replacemach.AddString(same);			m_replacemach.SetItemData(0,1002);
			m_replacemach.AddString(empty);			m_replacemach.SetItemData(1,1001);
			for (int i=0; i < 0xFF; i++) {
				std::ostringstream os;
				if (i < 16) os << "0";
				os << std::uppercase << std::hex << i;
				m_searchmach.AddString(os.str().c_str());		m_searchmach.SetItemData(3+i,i);
				m_replacemach.AddString(os.str().c_str());		m_replacemach.SetItemData(2+i,i);
			}
			m_searchmach.SetCurSel(0);
			m_replacemach.SetCurSel(0);

			if (cview.blockSelected) m_applyto = 2;
			UpdateData(FALSE);

			return true;  // return true unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return false
		}

		// CTransformPatternDlg message handlers

		void CTransformPatternDlg::OnBnClickedSearch()
		{
			CSearchReplaceMode mode = cview.SetupSearchReplaceMode(
				m_searchnote.GetItemData(m_searchnote.GetCurSel()),
				m_searchinst.GetItemData(m_searchinst.GetCurSel()),
				m_searchmach.GetItemData(m_searchmach.GetCurSel()));
			CCursor cursor;
			cursor.line = -1;


			int pattern = -1;
			UpdateData (TRUE);
			
			if (m_applyto == 0) {
				bool includeOther = m_includePatNotInSeq.GetCheck() > 0;
				int lastPatternUsed = (includeOther )? song.GetHighestPatternIndexInSequence() : MAX_PATTERNS;
				for (int currentPattern = 0; currentPattern <= lastPatternUsed; currentPattern++)
				{
					if (song.IsPatternUsed(currentPattern, !includeOther))
					{
						CSelection sel;
						sel.start.line = 0; sel.start.track = 0;
						sel.end.line = song.patternLines[currentPattern];
						sel.end.track = MAX_TRACKS;
						cursor = cview.SearchInPattern(currentPattern, sel , mode);
						if (cursor.line != -1) {
							pattern=currentPattern;
							break;
						}
					}
				}
			}
			else if (m_applyto == 1) {
				CSelection sel;
				sel.start.line = 0; sel.start.track = 0;
				sel.end.line = song.patternLines[cview._ps()];
				sel.end.track = MAX_TRACKS;
				cursor = cview.SearchInPattern(cview._ps(), sel , mode);
				pattern = cview._ps();
			}
			else if (m_applyto == 2 && cview.blockSelected) {
				cursor = cview.SearchInPattern(cview._ps(), cview.blockSel , mode);
				pattern = cview._ps();
			}
			else {
				MessageBox("No block selected for action","Search and replace",MB_ICONWARNING);
				return;
			}
			if (cursor.line == -1) {
				MessageBox("Nothing found that matches the selected options","Search and replace",MB_ICONINFORMATION);
			}
			else {
				cview.editcur = cursor;
				if (cview._ps() != pattern) {
					int pos = -1;
					for (int i=0; i < MAX_SONG_POSITIONS; i++) {
						if (song.playOrder[i] == pattern) {
							pos = i;
							break;
						}
					}
					if (pos == -1){
						pos = song.playLength;
						++song.playLength;
						song.playOrder[pos]=pattern;
						((CMainFrame*)cview.pParentFrame)->UpdateSequencer();
					}
					cview.editPosition = pos;
					memset(song.playOrderSel,0,MAX_SONG_POSITIONS*sizeof(bool));
					song.playOrderSel[cview.editPosition]=true;
					((CMainFrame*)cview.pParentFrame)->UpdatePlayOrder(true);
					cview.Repaint(draw_modes::pattern);
				}
				else {
					cview.Repaint(draw_modes::cursor);
				}
			}
		}

		void CTransformPatternDlg::OnBnClickedReplace()
		{
			CSearchReplaceMode mode = cview.SetupSearchReplaceMode(
				m_searchnote.GetItemData(m_searchnote.GetCurSel()),
				m_searchinst.GetItemData(m_searchinst.GetCurSel()),
				m_searchmach.GetItemData(m_searchmach.GetCurSel()),
				m_replacenote.GetItemData(m_replacenote.GetCurSel()),
				m_replaceinst.GetItemData(m_replaceinst.GetCurSel()), 
				m_replacemach.GetItemData(m_replacemach.GetCurSel()),
				m_replacetweak.GetCheck());

			bool replaced=false;
			UpdateData (TRUE);

			if (m_applyto == 0) {
				bool includeOther = m_includePatNotInSeq.GetCheck() > 0;
				int lastPatternUsed = (includeOther )? song.GetHighestPatternIndexInSequence() : MAX_PATTERNS;
				for (int currentPattern = 0; currentPattern <= lastPatternUsed; currentPattern++)
				{
					if (song.IsPatternUsed(currentPattern, !includeOther))
					{				
						CSelection sel;
						sel.start.line = 0; sel.start.track = 0;
						sel.end.line = song.patternLines[currentPattern];
						sel.end.track = MAX_TRACKS;
						replaced=cview.SearchReplace(currentPattern, sel , mode);
					}
				}
			}
			else if (m_applyto == 1) {
				CSelection sel;
				sel.start.line = 0; sel.start.track = 0;
				sel.end.line = song.patternLines[cview._ps()];
				sel.end.track = MAX_TRACKS;
				replaced=cview.SearchReplace(cview._ps(), sel, mode);
			}
			else if (m_applyto == 2 && cview.blockSelected) {
				replaced=cview.SearchReplace(cview._ps(), cview.blockSel , mode);
			}
			else {
				MessageBox("No block selected for action","Search and replace",MB_ICONWARNING);
				return;
			}
			if (replaced) {
				cview.Repaint(draw_modes::pattern);
			}
			else {
				MessageBox("Nothing found that matches the selected options","Search and replace",MB_ICONINFORMATION);
			}
		}
	}   // namespace
}   // namespace
