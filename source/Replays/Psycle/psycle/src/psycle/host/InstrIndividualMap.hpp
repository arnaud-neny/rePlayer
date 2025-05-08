#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

class XMInstrument;
class CInstrIndividualMap : public CDialog
{
public:
	CInstrIndividualMap();
	virtual ~CInstrIndividualMap();

	/// Dialog ID
	enum { IDD = IDD_INST_NOTEMAP_INDIV };

public:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	XMInstrument* xins;
	int editPos;
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnOk();

private:
	CComboBox m_Notes;
	CComboBox m_Samples;

};

}}