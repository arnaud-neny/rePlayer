///\file
///\brief implementation file for psycle::host::CDirectoryDlg.
#include <psycle/host/detail/project.private.hpp>
#include "XMSamplerUIGeneral.hpp"
#include "XMInstrument.hpp"
#include "XMSampler.hpp"
#include "Song.hpp"
#include "XMSamplerUI.hpp"

namespace psycle { namespace host {

	IMPLEMENT_DYNCREATE(XMSamplerUIGeneral, CPropertyPage)

	XMSamplerUIGeneral::XMSamplerUIGeneral() : CPropertyPage(XMSamplerUIGeneral::IDD)
	{
		m_bInitialize = false;
	}

	void XMSamplerUIGeneral::DoDataExchange(CDataExchange* pDX)
	{
		CPropertyPage::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_XMINTERPOL, m_interpol);
		DDX_Control(pDX, IDC_XMPOLY, m_polyslider);
		DDX_Control(pDX, IDC_XMPOLYLABEL, m_polylabel);
		DDX_Control(pDX, IDC_COMMANDINFO, m_ECommandInfo);
		DDX_Control(pDX, IDC_CHECK1, m_bAmigaSlides);
		DDX_Control(pDX, IDC_FILTERED, m_ckFilter);
		DDX_Control(pDX, IDC_XMPANNINGMODE, m_cbPanningMode);
	}
/*
	BEGIN_MESSAGE_MAP(XMSamplerUIGeneral, CPropertyPage)
		ON_WM_HSCROLL()
		ON_CBN_SELENDOK(IDC_XMINTERPOL, OnCbnSelendokXminterpol)
		ON_BN_CLICKED(IDC_CHECK1, OnBnClickedAmigaSlide)
		ON_BN_CLICKED(IDC_FILTERED, OnBnClickedFilter)
		ON_CBN_SELENDOK(IDC_XMPANNINGMODE, OnCbnSelendokXmpanningmode)
	END_MESSAGE_MAP()
*/

	XMSamplerUIGeneral::~XMSamplerUIGeneral()
	{
	}

	BOOL XMSamplerUIGeneral::PreTranslateMessage(MSG* pMsg) 
	{
		XMSamplerUI* parent = dynamic_cast<XMSamplerUI*>(GetParent());
		BOOL res = parent->PreTranslateChildMessage(pMsg, GetFocus()->GetSafeHwnd());
		if (res == FALSE ) return CPropertyPage::PreTranslateMessage(pMsg);
		return res;
	}
	/////////////////////////////////////////////////////////////////////////////
	// XMSamplerUIGeneral message handlers

	BOOL XMSamplerUIGeneral::OnInitDialog() 
	{
		CPropertyPage::OnInitDialog();
		m_bInitialize=false;
		m_interpol.AddString("Hold/Chip Interp. [Lowest quality]");
		m_interpol.AddString("Linear Interpolation [Low quality]");
		m_interpol.AddString("Spline Interpolation [Medium quality]");
		m_interpol.AddString("32Tap Sinc Interp. [High quality]");
		m_interpol.SetCurSel(_pMachine->ResamplerQuality());


		m_polyslider.SetRange(2, XMSampler::MAX_POLYPHONY);
		m_polyslider.SetPos(_pMachine->NumVoices());
		// Label on dialog display
		std::stringstream ss;
		ss << _pMachine->NumVoices();
		m_polylabel.SetWindowText(ss.str().c_str());

		m_ECommandInfo.SetWindowText("Track Commands:\r\n\t\
01xx: Portamento Up ( Fx: fine, Ex: Extra fine)\r\n\t\
02xx: Portamento Down (Fx: fine, Ex: Extra fine)\r\n\t\
03xx: Tone Portamento\r\n\t\
04xy: Vibrato with speed y and depth x\r\n\t\
05xx: Continue Portamento and Volume Slide with speed xx\r\n\t\
06xx: Continue Vibrato and Volume Slide with speed xx\r\n\t\
07xx: Tremolo\r\n\t\
08xx: Pan. 0800 Left 08FF right\r\n\t\
09xx: Panning slide x0 Left, 0x Right\r\n\t\
0Axx: Channel Volume, 00 = Min, 40 = Max\r\n\t\
0Bxx: Channel VolSlide x0 Up (xF fine), 0x Down (Fx Fine)\r\n\t\
0Cxx: Volume (0C80 : 100%)\r\n\t\
0Dxx: Volume Slide x0 Up (xF fine), 0x Down (Fx Fine)\r\n\t\
0Exy: Extended (see below).\r\n\t\
0Fxx: Filter.\r\n\t\
10xy: Arpeggio with note, note+x and note+y\r\n\t\
11xy: Retrig note after y ticks\r\n\t\
14xx: Fine Vibrato with speed y and depth x\r\n\t\
17xy: Tremor Effect ( ontime x, offtime y )\r\n\t\
18xx: Panbrello\r\n\t\
19xx: Set Envelope position (in ticks)\r\n\t\
1Cxx: Global Volume, 00 = Min, 80 = Max\r\n\t\
1Dxx: Global Volume Slide x0 Up (xF fine), 0x Down (Fx Fine)\r\n\t\
1Exx: Send xx to volume colum (see below)\r\n\t\
9xxx: Sample Offset x*256\r\n\r\n\
Extended Commands:\r\n\t\
30/1: Glissando mode Off/on\r\n\t\
4x: Vibrato Wave\r\n\t\
5x: Panbrello Wave\r\n\t\
7x: Tremolo Wave\r\n\t\
Waves: 0:Sinus, 1:Square\r\n\t\
2:Ramp Up, 3:Ramp Down, 4: Random\r\n\t\
8x: Panning\r\n\t\
90: Surround Off\r\n\t\
91: Surround On\r\n\t\
9E: Play Forward\r\n\t\
9F: Play Backward\r\n\t\
Cx: Delay NoteCut by x ticks\r\n\t\
Dx: Delay New Note by x ticks\r\n\t\
E0: Notecut background notes\r\n\t\
E1: Noteoff background notes\r\n\t\
E2: NoteFade background notes\r\n\t\
E3: Set NNA NoteCut for this voice\r\n\t\
E4: Set NNA NoteContinue for this voice\r\n\t\
E5: Set NNA Noteoff for this voice\r\n\t\
E6: Set NNA NoteFade for this channel\r\n\t\
E7/8: Disable/Enable Volume Envelope\r\n\t\
E9/A: Disable/Enable Pan Envelope\r\n\t\
EB/C: Disable/Enable Pitch/Filter Envelope\r\n\t\
Fx : Set Filter Mode.\r\n\r\n\
Volume Column:\r\n\t\
00..3F: Set volume to x*2\r\n\t\
4x: Volume slide up\r\n\t\
5x: Volume slide down\r\n\t\
6x: Fine Volslide up\r\n\t\
7x: Fine Volslide down\r\n\t\
8x: Panning (0:Left, F:Right)\r\n\t\
9x: PanSlide Left\r\n\t\
Ax: PanSlide Right\r\n\t\
Bx: Vibrato\r\n\t\
Cx: TonePorta\r\n\t\
Dx: Pitch slide up\r\n\t\
Ex: Pitch slide down");

	m_bInitialize=true;

	m_bAmigaSlides.SetCheck(_pMachine->IsAmigaSlides()?1:0);
	m_ckFilter.SetCheck(_pMachine->UseFilters()?1:0);

	m_cbPanningMode.AddString(_T("Linear (Cross)"));
	m_cbPanningMode.AddString(_T("Two Sliders (FT2)"));
	m_cbPanningMode.AddString(_T("Equal Power"));

	m_cbPanningMode.SetCurSel(_pMachine->PanningMode());

	return true;  // return true unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return false
	}

	void XMSamplerUIGeneral::OnCbnSelendokXminterpol()
	{
		CExclusiveLock lock(&Global::song().semaphore, 2, true);
		_pMachine->ResamplerQuality((helpers::dsp::resampler::quality::type)m_interpol.GetCurSel());
	}

	void XMSamplerUIGeneral::SliderPolyphony(CSliderCtrl* slid)
	{
		for(int c = _pMachine->NumVoices(); c < XMSampler::MAX_POLYPHONY; c++)
		{
			_pMachine->rVoice(c).NoteOffFast();
		}

		_pMachine->NumVoices(slid->GetPos());
		// Label on dialog display
		std::stringstream ss;
		ss << _pMachine->NumVoices();
		m_polylabel.SetWindowText(ss.str().c_str());
	}

	void XMSamplerUIGeneral::OnBnClickedAmigaSlide()
	{
		_pMachine->IsAmigaSlides(( m_bAmigaSlides.GetCheck() == 1 )?true:false);
	}

	void XMSamplerUIGeneral::OnBnClickedFilter()
	{
		_pMachine->UseFilters(m_ckFilter.GetCheck()?true:false);
	}

	void XMSamplerUIGeneral::OnCbnSelendokXmpanningmode()
	{
		_pMachine->PanningMode(m_cbPanningMode.GetCurSel());
	}

	void XMSamplerUIGeneral::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
		CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);

		switch(nSBCode){
		case TB_BOTTOM: //fallthrough
		case TB_LINEDOWN: //fallthrough
		case TB_PAGEDOWN: //fallthrough
		case TB_TOP: //fallthrough
		case TB_LINEUP: //fallthrough
		case TB_PAGEUP: //fallthrough
		case TB_THUMBPOSITION: //fallthrough
		case TB_THUMBTRACK:
			if ( the_slider->GetDlgCtrlID() == m_polyslider.GetDlgCtrlID() ) {
				SliderPolyphony(the_slider);
			}
			break;
		case TB_ENDTRACK:
			break;
		}
		CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
	}
}}
