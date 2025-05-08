#include <psycle/host/detail/project.private.hpp>
#include "InstrIndividualMap.hpp"
#include "PsycleConfig.hpp"
#include <psycle/host/XMInstrument.hpp>
#include <psycle/host/Song.hpp>

namespace psycle { namespace host {

CInstrIndividualMap::CInstrIndividualMap()
: CDialog(CInstrIndividualMap::IDD)
, editPos(0)
, xins(NULL)
{
}

CInstrIndividualMap::~CInstrIndividualMap()
{
}

void CInstrIndividualMap::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

/*
BEGIN_MESSAGE_MAP(CInstrIndividualMap, CDialog)
	ON_BN_CLICKED(IDOK, OnOk)
END_MESSAGE_MAP()
*/


BOOL CInstrIndividualMap::OnInitDialog() 
{
	CDialog::OnInitDialog();
	CComboBox* notes =((CComboBox*)GetDlgItem(IDC_INDIV_NOTE));
	CComboBox* samples = ((CComboBox*)GetDlgItem(IDC_INDIV_SAMPLE));
	PsycleConfig::PatternView & patView = PsycleGlobal::conf().patView();
	char ** notechars = (patView.showA440) ? patView.notes_tab_a440 : patView.notes_tab_a220;
	SampleList& list = Global::song().samples;

	SetWindowText(std::string("Editing Note map: ").append(notechars[editPos]).c_str());

	for (int i = 0; i<120; i++ ) {
		notes->AddString(notechars[i]);
	}

	samples->AddString("Not set");
	for (int i = 0; i<list.lastused()+1; i++ ) {
		CString strFmt;
		strFmt.Format("%02X%s", i, list.IsEnabled(i) ? std::string("*:").append(list[i].WaveName()).c_str() : " :");
		samples->AddString(strFmt);
	}

	const XMInstrument::NotePair & pair = xins->NoteToSample(editPos);
	notes->SetCurSel(pair.first);
	if (pair.second == 255 ) { samples->SetCurSel(0); }
	else { samples->SetCurSel(pair.second+1); }

	return TRUE;
	// return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CInstrIndividualMap::OnOk()
{
	CComboBox* notes =((CComboBox*)GetDlgItem(IDC_INDIV_NOTE));
	CComboBox* samples = ((CComboBox*)GetDlgItem(IDC_INDIV_SAMPLE));
	XMInstrument::NotePair pair;
	pair.first = notes->GetCurSel();
	if (samples->GetCurSel() == 0) { pair.second = 255; }
	else { pair.second = samples->GetCurSel()-1; }
	xins->NoteToSample(editPos,pair);
	CDialog::OnOK();
}

}}