#include <psycle/host/detail/project.private.hpp>
#include "InstrNoteMap.hpp"
#include <psycle/host/Song.hpp>
#include "PsycleConfig.hpp"

namespace psycle { namespace host {

CInstrNoteMap::CInstrNoteMap()
: CDialog(CInstrNoteMap::IDD)
, m_instIdx(0)
{
}

CInstrNoteMap::~CInstrNoteMap()
{
}

void CInstrNoteMap::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_SAMPLESLIST, m_SampleList);
	DDX_Control(pDX, IDC_NOTELIST, m_NoteList);
	DDX_Control(pDX, IDC_NOTEMAP_LIST, m_NoteMapList);
	DDX_Control(pDX, IDC_RADIO_EDIT, m_radio_edit);
	DDX_Control(pDX, IDC_INS_SHIFTMOVE, m_ShiftMove);

}

/*
BEGIN_MESSAGE_MAP(CInstrNoteMap, CDialog)
	ON_BN_CLICKED(IDOK, OnOk)
	ON_LBN_SELCHANGE(IDC_SAMPLESLIST, OnSelchangeSampleList)
	ON_LBN_SELCHANGE(IDC_NOTELIST, OnSelchangeNoteList)
	ON_LBN_SELCHANGE(IDC_NOTEMAP_LIST, OnSelchangeNoteMapList)
	ON_BN_CLICKED(IDC_RADIO_EDIT, OnRadioEdit)
	ON_BN_CLICKED(IDC_RADIO_SHOW, OnRadioShow)

	ON_BN_CLICKED(IDC_SETDEFAULT,OnBtnSetDefaults)
	ON_BN_CLICKED(IDC_INCREASEOCT,OnBtnIncreaseOct)
	ON_BN_CLICKED(IDC_DECREASEOCT,OnBtnDecreaseOct)
	ON_BN_CLICKED(IDC_INCREASENOTE,OnBtnIncreaseNote)
	ON_BN_CLICKED(IDC_DECREASENOTE,OnBtnDecreaseNote)
	ON_BN_CLICKED(IDC_MAP_SELECT_ALL,OnBtnSelectAll)
	ON_BN_CLICKED(IDC_MAP_SELECT_NONE,OnBtnNone)
	ON_BN_CLICKED(IDC_MAP_SELECT_OCTAVE,OnBtnOctave)


END_MESSAGE_MAP()
*/

BOOL CInstrNoteMap::PreTranslateMessage(MSG* pMsg)
{
/*	BOOL res = parent->PreTranslateChildMessage(pMsg, GetFocus()->GetSafeHwnd());
	if (res == FALSE )*/ return CDialog::PreTranslateMessage(pMsg);
//	return res;
}

BOOL CInstrNoteMap::OnInitDialog() 
{
	CDialog::OnInitDialog();
	copyInstr = *m_instr;

	CFont* m_font = &PsycleGlobal::conf().fixedFont;
	m_NoteList.SetColumnWidth(35);
	m_NoteList.SetFont(m_font);
	m_NoteMapList.SetFont(m_font);
	m_SampleList.SetColumnWidth(150);

	m_ShiftMove.AddString(_T("Change tune"));
	m_ShiftMove.AddString(_T("Move all"));
	m_ShiftMove.AddString(_T("Move notes"));
	m_ShiftMove.AddString(_T("Move samples"));
	m_ShiftMove.SetCurSel(0);

	((CButton*)GetDlgItem(IDC_DECREASENOTE))->SetIcon(PsycleGlobal::conf().iconless);
	((CButton*)GetDlgItem(IDC_INCREASENOTE))->SetIcon(PsycleGlobal::conf().iconmore);
	((CButton*)GetDlgItem(IDC_DECREASEOCT))->SetIcon(PsycleGlobal::conf().iconlessless);
	((CButton*)GetDlgItem(IDC_INCREASEOCT))->SetIcon(PsycleGlobal::conf().iconmoremore);

	RefreshSampleList(-1);
	RefreshNoteList();
	RefreshNoteMapList();

	m_radio_edit.SetCheck(TRUE);

	return TRUE;
	// return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CInstrNoteMap::OnOk()
{
	for (int i = 0; i<XMInstrument::NOTE_MAP_SIZE; i++ ) {
		m_instr->NoteToSample(i,copyInstr.NoteToSample(i));
	}
	OnOK();		
}


void CInstrNoteMap::RefreshSampleList(int sample){

	char line[48];
	SampleList& list = Global::song().samples;
	if (sample == -1) {
		int i = m_SampleList.GetCurSel();
		m_SampleList.ResetContent();
		for (int i=0;i<=list.lastused();i++)
		{
			if (list.Exists(i)) {
				const XMInstrument::WaveData<>& wave = list[i];
				sprintf(line,"%02X%s: ",i,wave.WaveLength()>0?"*":" ");
				strcat(line,wave.WaveName().c_str());
			}
			else {
				sprintf(line,"%02X : ",i);
			}
			m_SampleList.AddString(line);
		}
		m_SampleList.AddString(" -- : No sample");
		if (i !=  LB_ERR) {
			m_SampleList.SetCurSel(i);
		}
	}
}

void CInstrNoteMap::RefreshNoteList(){
	PsycleConfig::PatternView & patView = PsycleGlobal::conf().patView();
	char ** notechars = (patView.showA440) ? patView.notes_tab_a440 : patView.notes_tab_a220;

	m_NoteList.ResetContent();
	for (int i = 0; i<120; i++ ) {
		m_NoteList.AddString(notechars[i]);
	}
}

void CInstrNoteMap::RefreshNoteMapList(bool reset/*=true*/){
	PsycleConfig::PatternView & patView = PsycleGlobal::conf().patView();
	char ** notechars = (patView.showA440) ? patView.notes_tab_a440 : patView.notes_tab_a220;
	char line[48];
	if (reset) {
		m_NoteMapList.ResetContent();
		
		for (int i = 0; i<XMInstrument::NOTE_MAP_SIZE; i++ ) {
			const XMInstrument::NotePair& pair = copyInstr.NoteToSample(i);
			if(pair.second==255) { sprintf(line,"%s: %s --",notechars[i],notechars[pair.first]);}
			else { sprintf(line,"%s: %s %02X",notechars[i],notechars[pair.first],pair.second);}
			m_NoteMapList.AddString(line);
		}
	}
	else { 
		for (int i = 0; i<XMInstrument::NOTE_MAP_SIZE; i++ ) {
			bool isSel = (m_NoteMapList.GetSel(i) > 0);
			m_NoteMapList.DeleteString(i);

			const XMInstrument::NotePair& pair = copyInstr.NoteToSample(i);
			if(pair.second==255) { sprintf(line,"%s: %s --",notechars[i],notechars[pair.first]);}
			else { sprintf(line,"%s: %s %02X",notechars[i],notechars[pair.first],pair.second);}

			m_NoteMapList.InsertString(i,line);
			m_NoteMapList.SetSel(i,isSel);
		}
	}
}

void CInstrNoteMap::OnSelchangeSampleList()
{
	int maxitems=m_NoteMapList.GetCount();
	int nosampleidx = Global::song().samples.lastused()+1;
	if(m_radio_edit.GetCheck()> 0) {
		if (m_SampleList.GetSelCount()>1) {
			if (m_NoteMapList.GetSelCount() != m_SampleList.GetSelCount()) {
				MessageBox("It is necessary that the selection of the mapping and the samples contain the same number of elements");
				m_SampleList.SelItemRange(false,0,m_SampleList.GetCount()-1);
			}
			else {
				int smpidx=0;
				for (int inp=0;inp<maxitems;inp++) {
					if ( m_NoteMapList.GetSel(inp) != 0) {
						while( m_SampleList.GetSel(smpidx)==0) smpidx++;
						XMInstrument::NotePair pair = copyInstr.NoteToSample(inp);
						if (smpidx == nosampleidx) {
							pair.second = 255;
						}
						else {
							pair.second = smpidx;
						}
						copyInstr.NoteToSample(inp,pair);
						smpidx++;
					}
				}
				RefreshNoteMapList(false);
			}
		}
		else {
			int smpidx = m_SampleList.GetCurSel();
			for	(int inp=0;inp<maxitems;inp++ ) {
				if ( m_NoteMapList.GetSel(inp) != 0) {
					XMInstrument::NotePair pair = copyInstr.NoteToSample(inp);
					if (smpidx == nosampleidx) {
						pair.second = 255;
					}
					else {
						pair.second = smpidx;
					}
					copyInstr.NoteToSample(inp, pair);
				}
			}
			RefreshNoteMapList(false);
		}
	}
	else {
		m_NoteList.SelItemRange(false,0,m_NoteList.GetCount()-1);
		m_NoteMapList.SelItemRange(false,0,m_NoteMapList.GetCount()-1);

		for (int smpidx=0; smpidx< m_SampleList.GetCount(); smpidx++) {
			if (m_SampleList.GetSel(smpidx) != 0 ) {
				for (int inp=0;inp<maxitems;inp++ ) {
					int smpidxbis = (smpidx==nosampleidx)?255:smpidx;
					const XMInstrument::NotePair & pair = copyInstr.NoteToSample(inp);
					if (pair.second == smpidxbis ) {
						m_NoteMapList.SetSel(inp,true);
						m_NoteList.SetSel(pair.first,true);
					}
				}
			}
		}
	}
}
void CInstrNoteMap::OnSelchangeNoteList()
{
	int maxitems=m_NoteMapList.GetCount();
	if(m_radio_edit.GetCheck()> 0) {
		if (m_NoteList.GetSelCount()>1) {
			if (m_NoteMapList.GetSelCount() != m_NoteList.GetSelCount()) {
				MessageBox("It is necessary that the selection of the mapping and the samples contain the same number of elements");
				m_NoteList.SelItemRange(false,0,m_NoteList.GetCount()-1);
			}
			else {
				int noteidx=0;
				for (int inp=0;inp<maxitems;inp++) {
					if ( m_NoteMapList.GetSel(inp) != 0) {
						while( m_NoteList.GetSel(noteidx)==0) noteidx++;
						XMInstrument::NotePair pair = copyInstr.NoteToSample(inp);
						pair.first = noteidx;
						copyInstr.NoteToSample(inp, pair);
						noteidx++;
					}
				}
				RefreshNoteMapList(false);
			}
		}
		else {
			int noteidx = m_NoteList.GetCurSel();
			for (int inp=0;inp<maxitems;inp++ ) {
				if ( m_NoteMapList.GetSel(inp) != 0) {
					XMInstrument::NotePair pair = copyInstr.NoteToSample(inp);
					pair.first = noteidx;
					copyInstr.NoteToSample(inp, pair);
				}
			}
			RefreshNoteMapList(false);
		}
	}
	else {
		m_SampleList.SelItemRange(false,0,m_SampleList.GetCount()-1);
		m_NoteMapList.SelItemRange(false,0,m_NoteMapList.GetCount()-1);


		for (int noteidx=0; noteidx< m_NoteList.GetCount(); noteidx++) {
			if (m_NoteList.GetSel(noteidx) != 0 ) {
				for (int inp=0;inp<maxitems;inp++ ) {
					const XMInstrument::NotePair & pair = copyInstr.NoteToSample(inp);
					if (pair.first == noteidx ) {
						m_NoteMapList.SetSel(inp,true);
						if (pair.second == 255) {
							m_SampleList.SetSel(Global::song().samples.lastused()+1,true);
						}
						else {
							m_SampleList.SetSel(pair.second,true);
						}
					}
				}
			}
		}
	}
}
void CInstrNoteMap::OnSelchangeNoteMapList()
{
	m_SampleList.SelItemRange(false,0,m_SampleList.GetCount()-1);
	m_NoteList.SelItemRange(false,0,m_NoteList.GetCount()-1);
	
	int maxitems=m_NoteMapList.GetCount();
	for (int inp=0;inp<maxitems;inp++ ) {
		if ( m_NoteMapList.GetSel(inp) != 0) {
			const XMInstrument::NotePair & pair = copyInstr.NoteToSample(inp);
			m_NoteList.SetSel(pair.first,true);
			if (pair.second == 255) {
				m_SampleList.SetSel(Global::song().samples.lastused()+1,true);
			}
			else {
				m_SampleList.SetSel(pair.second,true);
			}
		}
	}
}
void CInstrNoteMap::OnRadioEdit()
{
	OnSelchangeNoteMapList();
}
void CInstrNoteMap::OnRadioShow()
{
	OnSelchangeNoteMapList();
}
void CInstrNoteMap::OnBtnSelectAll()
{
	m_NoteMapList.SelItemRange(true,0,m_NoteMapList.GetCount()-1);
	OnSelchangeNoteMapList();
}
void CInstrNoteMap::OnBtnNone()
{
	m_NoteMapList.SelItemRange(false,0,m_NoteMapList.GetCount()-1);
	OnSelchangeNoteMapList();
}
void CInstrNoteMap::OnBtnOctave()
{
	if (m_NoteMapList.GetSelCount()!= 0) {
		int maxitems=m_NoteMapList.GetCount();
		int element=0;
		for (int inp=0;inp<maxitems;inp++ ) {
			if ( m_NoteMapList.GetSel(inp) != 0) {
				element=inp;
				break;
			}
		}
		m_NoteMapList.SelItemRange(false,0,m_NoteMapList.GetCount()-1);
		int octave=element/12;
		for (int inp=octave*12; inp < (octave+1)*12; inp++) {
			m_NoteMapList.SetSel(inp,true);
		}
	}
	else {
		m_NoteMapList.SelItemRange(false,0,m_NoteMapList.GetCount()-1);
		for (int inp=0;inp<12;inp++ ) {
			m_NoteMapList.SetSel(inp,true);
		}
	}
}


void CInstrNoteMap::OnBtnSetDefaults()
{
	if ( m_NoteMapList.GetSelCount() == 0 ) {
		copyInstr.SetDefaultNoteMap(m_instIdx);
	}
	else {
		XMInstrument::NotePair npair;
		npair.second=m_instIdx;
		int maxitems=m_NoteMapList.GetCount();
		for (int inp=0;inp<maxitems;inp++ ) {
			if ( m_NoteMapList.GetSel(inp) != 0) {
				npair.first=inp;
				copyInstr.NoteToSample(inp,npair);
			}
		}
	}
	RefreshNoteMapList(false);
	OnSelchangeNoteMapList();
}

void CInstrNoteMap::OnBtnIncreaseOct()
{
	if (m_ShiftMove.GetCurSel()==0) { copyInstr.TuneNotes(12); }
	else if (m_ShiftMove.GetCurSel()==1) { copyInstr.MoveMapping(12); }
	else if (m_ShiftMove.GetCurSel()==2) { copyInstr.MoveOnlyNotes(12); }
	else { copyInstr.MoveOnlySamples(12); }
	RefreshNoteMapList(false);
	OnSelchangeNoteMapList();
}
void CInstrNoteMap::OnBtnIncreaseNote()
{
	if (m_ShiftMove.GetCurSel()==0) { copyInstr.TuneNotes(1); }
	else if (m_ShiftMove.GetCurSel()==1) { copyInstr.MoveMapping(1); }
	else if (m_ShiftMove.GetCurSel()==2) { copyInstr.MoveOnlyNotes(1); }
	else { copyInstr.MoveOnlySamples(1); }
	RefreshNoteMapList(false);
	OnSelchangeNoteMapList();
}
void CInstrNoteMap::OnBtnDecreaseNote()
{
	if (m_ShiftMove.GetCurSel()==0) { copyInstr.TuneNotes(-1); }
	else if (m_ShiftMove.GetCurSel()==1) { copyInstr.MoveMapping(-1); }
	else if (m_ShiftMove.GetCurSel()==2) { copyInstr.MoveOnlyNotes(-1); }
	else { copyInstr.MoveOnlySamples(-1); }
	RefreshNoteMapList(false);
	OnSelchangeNoteMapList();
}
void CInstrNoteMap::OnBtnDecreaseOct()
{
	if (m_ShiftMove.GetCurSel()==0) { copyInstr.TuneNotes(-12); }
	else if (m_ShiftMove.GetCurSel()==1) { copyInstr.MoveMapping(-12); }
	else if (m_ShiftMove.GetCurSel()==2) { copyInstr.MoveOnlyNotes(-12); }
	else { copyInstr.MoveOnlySamples(-12); }
	RefreshNoteMapList(false);
	OnSelchangeNoteMapList();
}




}}