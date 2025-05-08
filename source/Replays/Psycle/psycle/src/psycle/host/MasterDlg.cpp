///\file
///\brief implementation file for psycle::host::CMasterDlg.
#include <psycle/host/detail/project.private.hpp>
#include "MasterDlg.hpp"
#include "InputHandler.hpp"
#include "Machine.hpp"
#include "DPI.hpp"
#include "NativeGraphics.hpp"
#include <psycle/helpers/dsp.hpp>

namespace psycle { namespace host {
	int CMasterDlg::masterWidth = 516;
	int CMasterDlg::masterHeight = 225;
	int CMasterDlg::numbersAddX = 24;
	int CMasterDlg::textYAdd = 15;

/*
  BEGIN_MESSAGE_MAP(CMasterVu, CProgressCtrl)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
  END_MESSAGE_MAP()
*/

	CMasterVu::CMasterVu():CProgressCtrl()
	{
		offsetX=0;
		doStretch = Is_Vista_or_Later();
	}
	CMasterVu::~CMasterVu()
	{
	}
	void CMasterVu::SetOffset(int offset)
	{
		offsetX=offset;
	}
	BOOL CMasterVu::OnEraseBkgnd(CDC* pDC) 
	{
		return TRUE;
	}
	void CMasterVu::OnPaint() 
	{
		const int vuImgW = MasterUI::uiSetting->coords.sMasterVuLeftOff.width;
		const int vuImgH = MasterUI::uiSetting->coords.sMasterVuLeftOff.height;
		CRect crect, wrect;
		GetClientRect(&crect);
		CBitmap* oldbmp;
		int vol = vuImgH - GetPos()*vuImgH/100;
		int volRel = crect.Height() - GetPos()*crect.Height()/100;
		CPaintDC dc(this);
		CDC memDC;
		if (doStretch) {
			dc.SetStretchBltMode(HALFTONE);
			memDC.CreateCompatibleDC(&dc);
			oldbmp=memDC.SelectObject(&MasterUI::uiSetting->masterSkin);
			dc.StretchBlt(0,volRel,crect.Width(),crect.Height(),&memDC,offsetX+vuImgW,vol,vuImgW,vuImgH,SRCCOPY);
			dc.StretchBlt(0,0,crect.Width(),volRel,&memDC,offsetX,0,vuImgW,vol,SRCCOPY);
		}
		else { // In Windows XP, it looks ugly, because the stretching makes the vumeter move its placement by one pixel depending on the position.
			memDC.CreateCompatibleDC(&dc);
			oldbmp=memDC.SelectObject(&MasterUI::uiSetting->masterSkin);
			dc.BitBlt(0, vol, crect.Width(), crect.Height(), &memDC, offsetX+vuImgW, vol, SRCCOPY);
			dc.BitBlt(0, 0, crect.Width(), vol, &memDC, offsetX, 0, SRCCOPY);
		}
		memDC.SelectObject(oldbmp);
	}


		/// master dialog
		CMasterDlg::CMasterDlg(CWnd* wndView, Master& new_master, CMasterDlg** windowVar) 
			: CDialog(CMasterDlg::IDD, AfxGetMainWnd()), windowVar_(windowVar)
			, machine(new_master), m_slidermaster(-1)
			, mainView(wndView)
		{
			doStretch = Is_Vista_or_Later();
			memset(macname,0,sizeof(macname));
			for (int i = 0; i < 12; ++i) {
				CVolumeCtrl* slider = new CVolumeCtrl(i);
				sliders_.push_back(slider);
			}
			CDialog::Create(CMasterDlg::IDD, AfxGetMainWnd());
		}

		CMasterDlg::~CMasterDlg() {
			std::vector<CVolumeCtrl*>::iterator it = sliders_.begin();
			for ( ; it != sliders_.end(); ++it ) {
				delete *it;
			}
		}
		void CMasterDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_MASTERPEAK, m_masterpeak);
			DDX_Control(pDX, IDC_SLIDERMASTER, m_slidermaster);
			DDX_Control(pDX, IDC_SLIDERM9, *sliders_[8]);
			DDX_Control(pDX, IDC_SLIDERM8, *sliders_[7]);
			DDX_Control(pDX, IDC_SLIDERM7, *sliders_[6]);
			DDX_Control(pDX, IDC_SLIDERM6, *sliders_[5]);
			DDX_Control(pDX, IDC_SLIDERM5, *sliders_[4]);
			DDX_Control(pDX, IDC_SLIDERM4, *sliders_[3]);
			DDX_Control(pDX, IDC_SLIDERM3, *sliders_[2]);
			DDX_Control(pDX, IDC_SLIDERM2, *sliders_[1]);
			DDX_Control(pDX, IDC_SLIDERM12, *sliders_[11]);
			DDX_Control(pDX, IDC_SLIDERM11, *sliders_[10]);
			DDX_Control(pDX, IDC_SLIDERM10, *sliders_[9]);
			DDX_Control(pDX, IDC_SLIDERM1, *sliders_[0]);
			DDX_Control(pDX, IDC_AUTODEC, m_autodec);
			DDX_Control(pDX, IDC_MASTERDLG_VULEFT, m_vuLeft);
			DDX_Control(pDX, IDC_MASTERDLG_VURIGHT, m_vuRight);
		}

/*
		BEGIN_MESSAGE_MAP(CMasterDlg, CDialog)
			ON_WM_VSCROLL()
			ON_WM_PAINT()
	        ON_WM_ERASEBKGND()
			ON_WM_CLOSE()
			ON_BN_CLICKED(IDC_AUTODEC, OnAutodec)
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_SLIDERMASTER, OnCustomdrawSlidermaster)
			ON_NOTIFY_RANGE(NM_CUSTOMDRAW, IDC_SLIDERM1, IDC_SLIDERM12, OnCustomdrawSliderm)
		END_MESSAGE_MAP()
*/

		BOOL CMasterDlg::PreTranslateMessage(MSG* pMsg) {
			if ((pMsg->message == WM_KEYDOWN) || (pMsg->message == WM_KEYUP)) {
				CmdDef def = PsycleGlobal::inputHandler().KeyToCmd(pMsg->wParam,0);
				if(def.GetType() == CT_Note) {
					mainView->SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
					return true;
				}
			}
			return CDialog::PreTranslateMessage(pMsg);
		}

		BOOL CMasterDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();

			RefreshSkin();
			
			m_slidermaster.SetRange(0, 832);
			m_slidermaster.SetPageSize(96);
			std::vector<CVolumeCtrl*>::iterator it = sliders_.begin();
			for ( ; it != sliders_.end(); ++it ) {
				CVolumeCtrl* slider = *it;
				slider->SetRange(0, 832);
				slider->SetPageSize(96);
			}
			SetSliderValues();
			m_vuLeft.SetRange(0,100);
			m_vuRight.SetRange(0,100);
			if (machine.decreaseOnClip) m_autodec.SetCheck(1);
			else m_autodec.SetCheck(0);
			return TRUE;
		}
		void CMasterDlg::OnCancel()
		{
			DestroyWindow();
		}
		void CMasterDlg::OnClose()
		{
			CDialog::OnClose();
			DestroyWindow();
		}
		void CMasterDlg::PostNcDestroy()
		{
			CDialog::PostNcDestroy();
			if(windowVar_!=NULL) *windowVar_ = NULL;
			delete this;
		}

		void CMasterDlg::OnAutodec() 
		{
			if (m_autodec.GetCheck())
			{
				machine.decreaseOnClip=true;
			}
			else machine.decreaseOnClip=false;
		}

		void CMasterDlg::RefreshSkin()
		{
			// Using CDPI example class
			CDPI& g_metrics = PsycleGlobal::dpiSetting();
			m_vuLeft.SetOffset(MasterUI::uiSetting->coords.sMasterVuLeftOff.x);
			m_vuRight.SetOffset(MasterUI::uiSetting->coords.sMasterVuRightOff.x);

			scnumbersMasterX = g_metrics.ScaleX(MasterUI::uiSetting->coords.dMasterMasterNumbers.x);
			scnumbersMasterY = g_metrics.ScaleY(MasterUI::uiSetting->coords.dMasterMasterNumbers.y);
			scnumbersX = g_metrics.ScaleX(MasterUI::uiSetting->coords.dMasterChannelNumbers.x);
			scnumbersY = g_metrics.ScaleY(MasterUI::uiSetting->coords.dMasterChannelNumbers.y);
			sctextX = g_metrics.ScaleX(MasterUI::uiSetting->coords.dMasterNames.x);
			sctextY = g_metrics.ScaleY(MasterUI::uiSetting->coords.dMasterNames.y);
			sctextW = g_metrics.ScaleX(MasterUI::uiSetting->coords.dMasterNames.width);
			sctextH = g_metrics.ScaleY(MasterUI::uiSetting->coords.dMasterNames.height);

			scmasterWidth = g_metrics.ScaleX(masterWidth);
			scmasterHeight = g_metrics.ScaleX(masterHeight);
			scnumbersAddX = g_metrics.ScaleX(numbersAddX);
			sctextYAdd = g_metrics.ScaleY(textYAdd);
		}

		void CMasterDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
			CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
			switch(nSBCode){
			case TB_BOTTOM: //fallthrough
			case TB_LINEDOWN: //fallthrough
			case TB_PAGEDOWN: //fallthrough
			case TB_TOP: //fallthrough
			case TB_LINEUP: //fallthrough
			case TB_PAGEUP: //fallthrough
				if(the_slider == &m_slidermaster) {
					OnChangeSliderMaster(m_slidermaster.GetPos());
				}
				else {
					std::vector<CVolumeCtrl*>::iterator it = sliders_.begin();
					for ( ; it != sliders_.end(); ++it ) {
						if( the_slider == *it) {
							OnChangeSliderMacs(*it);
						}
					}
				}
				break;
			case TB_THUMBPOSITION: //fallthrough
			case TB_THUMBTRACK:
				if(the_slider == &m_slidermaster) {
					OnChangeSliderMaster(m_slidermaster.GetPos());
				}
				else {
					std::vector<CVolumeCtrl*>::iterator it = sliders_.begin();
					for ( ; it != sliders_.end(); ++it ) {
						if( the_slider == *it) {
							OnChangeSliderMacs(*it);
						}
					}
				}
				break;
			}
			CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
		}

		void CMasterDlg::SetSliderValues()
		{
			float val;
			float db;
			if(machine._outDry>0) {
				db = helpers::dsp::dB(value_mapper::map_256_1(machine._outDry));
			}
			else {
				db = -99.f;
			}
			m_slidermaster.SetPos(832-(int)((db+40.0f)*16.0f));
			std::vector<CVolumeCtrl*>::iterator it = sliders_.begin();
			for ( int i = 0; it != sliders_.end(); ++it, ++i ) {
				CVolumeCtrl* slider = *it;
				if (machine.inWires[i].Enabled()) {		
					machine.GetWireVolume(i,val);
					slider->SetPos(832-(int)((helpers::dsp::dB(val)+40.0f)*16.0f));
				} else {
					slider->SetPos(832);
				}
			}
		}

		void CMasterDlg::UpdateUI(void)
		{
			if (!--machine.peaktime) 
			{
				std::string peak;
				
				char tmp[10];
				if ( machine.currentpeak > 0.001f) //26bits of precision in the display (note: GetAudioRange is 32768 for master)
				{
					sprintf(tmp,"%.2fdB",helpers::dsp::dB(machine.currentpeak*(1.f/machine.GetAudioRange())));
					peak = tmp;
				}
				else peak = "-inf dB";
				if ( machine.currentrms > 0.001f) //26bits of precision in the display (note: GetAudioRange is 32768 for master)
				{
					sprintf(tmp,"%.2fdB",helpers::dsp::dB(machine.currentrms*(1.f/machine.GetAudioRange())));
					peak = peak + "/" + tmp;
				}
				else peak = peak + "/-inf dB";

				m_masterpeak.SetWindowText(peak.c_str());
				
				SetSliderValues();

				machine.peaktime=25;
				machine.currentpeak=0.0f;
				machine.currentrms=0.0f;
				CRect r;
				r.top=sctextY;
				r.left=sctextX;
				r.right=sctextX+sctextW;
				r.bottom=sctextY+MAX_CONNECTIONS*sctextYAdd;
				InvalidateRect(r,FALSE);
			}
			m_vuLeft.SetPos(machine.volumeDisplayLeft);
			m_vuRight.SetPos(machine.volumeDisplayRight);
			if (MasterUI::uiSetting->masterRefresh) {
				RefreshSkin();
				Invalidate();
				MasterUI::uiSetting->masterRefresh = false;
			}
		}

		void CMasterDlg::OnChangeSliderMaster(int pos)
		{
			float db = ((832-pos)/16.0f)-40.0f;
			machine._outDry = value_mapper::map_1_256<int>(helpers::dsp::dB2Amp(db));
		}

		void CMasterDlg::OnChangeSliderMacs(CVolumeCtrl* slider)
		{
			float db = ((832-slider->GetPos())/16.0f)-40.0f;
			machine.SetWireVolume(slider->index(),helpers::dsp::dB2Amp(db));
		}

		void CMasterDlg::OnPaint() 
		{
			CPaintDC dc(this); // device context for painting
	
			RECT& rect = dc.m_ps.rcPaint;
			if ( rect.bottom >= std::min(scnumbersMasterY,scnumbersY) && rect.top <= std::max(scnumbersMasterY,scnumbersY)+sctextH)
			{
				PaintNumbersDC(&dc,((832-m_slidermaster.GetPos())/16.0f)-40.0f,scnumbersMasterX,scnumbersMasterY);
				std::vector<CVolumeCtrl*>::iterator it = sliders_.begin();
				for ( int i= 0; it != sliders_.end(); ++it, ++i) {
					CVolumeCtrl* slider = *it;
					PaintNumbersDC(&dc,((832-slider->GetPos())/16.0f)-40.0f,scnumbersX +i*scnumbersAddX,scnumbersY);
				}
			}
			if (rect.right >=sctextX)
			{
				CFont* oldfont = dc.SelectObject(&MasterUI::uiSetting->masterNamesFont);
				dc.SetTextColor(MasterUI::uiSetting->coords.masterFontForeColour);
				dc.SetBkColor(MasterUI::uiSetting->coords.masterFontBackColour);

				for(int i=0, y=sctextY ; i < MAX_CONNECTIONS; i++, y += sctextYAdd)
				{
					dc.ExtTextOut(sctextX, y-1, ETO_OPAQUE|ETO_CLIPPED, CRect(sctextX,y,sctextX+sctextW,y+sctextH), CString(macname[i]), 0);
				}
				dc.SelectObject(oldfont);
			}
			// Do not call CDialog::OnPaint() for painting messages
		}
		void CMasterDlg::PaintNumbersDC(CDC *dc, float val, int x, int y)
		{
			char valtxt[6];
			if ( fabs(val) < 10.0f )
			{
				if ( val < 0 ) sprintf(valtxt,"%.01f",val);
				else sprintf(valtxt," %.01f",val);
			} else {
				if ( val < -39.5f) strcpy(valtxt,"-99 ");
				else if ( val < 0) sprintf(valtxt,"%.0f ",val);
				else sprintf(valtxt," %.0f ",val);
			}
			CFont* oldfont = dc->SelectObject(&MasterUI::uiSetting->masterNamesFont);
			dc->SetTextColor(MasterUI::uiSetting->coords.masterFontForeColour);
			dc->SetBkColor(MasterUI::uiSetting->coords.masterFontBackColour);
			dc->ExtTextOut(x, y-2, ETO_CLIPPED, CRect(x,y,x+scnumbersAddX-2,y+sctextH), CString(valtxt), 0);
			dc->SelectObject(oldfont);
		}

		void CMasterDlg::OnCustomdrawSlidermaster(NMHDR* pNMHDR, LRESULT* pResult) 
		{
/*
			NMCUSTOMDRAW nmcd = *reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
			if (nmcd.dwDrawStage == CDDS_POSTPAINT)
			{
				float db = ((832-m_slidermaster.GetPos())/16.0f)-40.0f;
				CClientDC dc(this);
				PaintNumbersDC(&dc,db,scnumbersMasterX,scnumbersMasterY);
				*pResult = CDRF_DODEFAULT;
			}
			else {
				*pResult = DrawSliderGraphics(pNMHDR);
			}
*/
		}

		void CMasterDlg::OnCustomdrawSliderm(UINT idx, NMHDR* pNMHDR, LRESULT* pResult) 
		{
/*
			NMCUSTOMDRAW nmcd = *reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
			if (nmcd.dwDrawStage == CDDS_POSTPAINT)
			{
				CVolumeCtrl* slider =reinterpret_cast<CVolumeCtrl*>(GetDlgItem(pNMHDR->idFrom));
				float db = ((832-slider->GetPos())/16.0f)-40.0f;
				CClientDC dc(this);
				PaintNumbersDC(&dc,db,scnumbersX + slider->index()*scnumbersAddX,scnumbersY);
				*pResult = CDRF_DODEFAULT;
			}
			else {
				*pResult = DrawSliderGraphics(pNMHDR);
			}
*/
		}

		LRESULT CMasterDlg::DrawSliderGraphics(NMHDR* pNMHDR)
		{
/*
			NMCUSTOMDRAW nmcd = *reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
			switch(nmcd.dwDrawStage)
			{
			case CDDS_PREPAINT:
			{
				return CDRF_NOTIFYITEMDRAW|CDRF_NOTIFYPOSTPAINT;
			}
			case CDDS_ITEMPREPAINT:
			{
				if ( nmcd.dwItemSpec == TBCD_THUMB )
				{
					SSkinSource& knob = MasterUI::uiSetting->coords.sMasterKnob;
					CDC* pDC = CDC::FromHandle( nmcd.hdc );
					CDC memDC;
					CBitmap* oldbmp;
					memDC.CreateCompatibleDC(pDC);
					oldbmp=memDC.SelectObject(&MasterUI::uiSetting->masterSkin);
					pDC->StretchBlt(nmcd.rc.left,nmcd.rc.top,nmcd.rc.right-nmcd.rc.left,nmcd.rc.bottom-nmcd.rc.top,&memDC,knob.x,knob.y,knob.width,knob.height,SRCCOPY);
					//pDC->BitBlt(nmcd.rc.left,nmcd.rc.top,nmcd.rc.right-nmcd.rc.left,nmcd.rc.bottom-nmcd.rc.top,&memDC,knob.x,knob.y,SRCCOPY);
					memDC.SelectObject(oldbmp);
				}
				else if(nmcd.dwItemSpec == TBCD_CHANNEL)
				{
					//Drawing the whole background, not only the channel.
					CDC* pDC = CDC::FromHandle( nmcd.hdc );
					CDC memDC;
					CBitmap* oldbmp;
					CDPI& g_metrics = PsycleGlobal::dpiSetting();
					pDC->SetStretchBltMode(HALFTONE);
					memDC.CreateCompatibleDC(pDC);
					oldbmp=memDC.SelectObject(&MasterUI::uiSetting->masterSkin);
					CVolumeCtrl* slider =reinterpret_cast<CVolumeCtrl*>(GetDlgItem(pNMHDR->idFrom));
					CRect crect, wrect;
					slider->GetClientRect(&crect);
					slider->GetWindowRect(&wrect);
					ScreenToClient(wrect);
					pDC->StretchBlt(0, 0, crect.Width(), crect.Height(), &memDC, g_metrics.UnscaleX(wrect.left), g_metrics.UnscaleY(wrect.top), g_metrics.UnscaleX(crect.Width()), g_metrics.UnscaleY(crect.Height()), SRCCOPY);
					//pDC->BitBlt(0, 0, crect.Width(), crect.Height(), &memDC, wrect.left, wrect.top, SRCCOPY);
					memDC.SelectObject(oldbmp);
				}
				else {
					//Do not draw ticks.
				}
				return CDRF_SKIPDEFAULT;
			}
			default:
				return CDRF_DODEFAULT;
			}
*/
			return 0;
		}
		
		BOOL CMasterDlg::OnEraseBkgnd(CDC* pDC) 
		{
			BOOL res = CDialog::OnEraseBkgnd(pDC);
			if (MasterUI::uiSetting->masterSkin.m_hObject != NULL)
			{
				// Using CDPI example class
				pDC->SetStretchBltMode(HALFTONE);
				CDC memDC;
				memDC.CreateCompatibleDC(pDC);
				CBitmap* pOldBitmap = memDC.SelectObject(&MasterUI::uiSetting->masterSkin);
				pDC->StretchBlt(0,0,scmasterWidth,scmasterHeight,&memDC,0,0,masterWidth,masterHeight,SRCCOPY);
				//pDC->BitBlt(0, 0, m_nBmpWidth, m_nBmpHeight, &memDC, 0, 0, SRCCOPY);
				memDC.SelectObject(pOldBitmap);
				res = TRUE;
			}
			return res;
		}
	}   // namespace
}   // namespace
