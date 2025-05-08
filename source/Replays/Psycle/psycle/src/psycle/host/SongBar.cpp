/** @file 
 *  @brief SongBar dialog
 *  $Date: 2010-08-15 18:18:35 +0200 (dg., 15 ag 2010) $
 *  $Revision: 9831 $
 */
#include <psycle/host/detail/project.private.hpp>
#include "SongBar.hpp"
#include "PsycleConfig.hpp"
#include "MainFrm.hpp"
#include "ChildView.hpp"
#include "Song.hpp"
#include "Machine.hpp"
#include "Player.hpp"
#include "InputHandler.hpp"

namespace psycle{ namespace host{

	extern CPsycleApp theApp;
IMPLEMENT_DYNAMIC(SongBar, CDialogBar)

	SongBar::SongBar()
	{
		vuprevR = 0;
		vuprevL = 0;
	}

	SongBar::~SongBar()
	{
	}
	void SongBar::InitializeValues(CMainFrame* frame, CChildView* view, Song& song)
	{
		m_pParentMain = frame;
		m_pWndView = view;
		m_pSong = &song;
	}

	void SongBar::DoDataExchange(CDataExchange* pDX)
	{
		CDialogBar::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_TRACKCOMBO, m_trackcombo);
		DDX_Control(pDX, IDC_COMBOOCTAVE, m_octavecombo);
		DDX_Control(pDX, IDC_MASTERSLIDER, m_masterslider);
		DDX_Control(pDX, IDC_BPMLABEL, m_bpmlabel);
		DDX_Control(pDX, IDC_TPBLABEL, m_lpblabel);
	}

	//Message Maps are defined in SongBar, since this isn't a window, but a DialogBar.
/*
	BEGIN_MESSAGE_MAP(SongBar, CDialogBar)
		ON_MESSAGE(WM_INITDIALOG, OnInitDialog )
		ON_WM_HSCROLL()
	END_MESSAGE_MAP()
*/

	// SongBar message handlers
	LRESULT SongBar::OnInitDialog ( WPARAM wParam, LPARAM lParam)
	{
		BOOL bRet = HandleInitDialog(wParam, lParam);

		if (!UpdateData(FALSE))
		{
		   TRACE0("Warning: UpdateData failed during dialog init.\n");
		}

		((CButton*)GetDlgItem(IDC_BPM_DECONE))->SetIcon(PsycleGlobal::conf().iconless);
		((CButton*)GetDlgItem(IDC_BPM_ADDONE))->SetIcon(PsycleGlobal::conf().iconmore);
		((CButton*)GetDlgItem(IDC_DEC_TPB))->SetIcon(PsycleGlobal::conf().iconless);
		((CButton*)GetDlgItem(IDC_INC_TPB))->SetIcon(PsycleGlobal::conf().iconmore);

		for(int i=4;i<=MAX_TRACKS;i++)
		{
			char s[4];
			_snprintf(s,4,"%i",i);
			m_trackcombo.AddString(s);
		}
		m_trackcombo.SetCurSel(m_pSong->SONGTRACKS-4);

		m_masterslider.SetRange(0,1024);
		m_masterslider.SetTipSide(TBTS_TOP);
		float val = 1.0f;
		if ( m_pSong->_pMachine[MASTER_INDEX] != NULL) {
			 val =value_mapper::map_256_1(((Master*)m_pSong->_pMachine[MASTER_INDEX])->_outDry);
		}
		int nPos = helpers::dsp::AmountToSliderHoriz(val);
		m_masterslider.SetPos(nPos);
		m_masterslider.SetTicFreq(64);
		m_masterslider.SetPageSize(64);

		return bRet;
	}


	void SongBar::OnSelchangeTrackcombo() 
	{
		m_pSong->SONGTRACKS=m_trackcombo.GetCurSel()+4;
		if (m_pWndView->editcur.track >= m_pSong->SONGTRACKS )
			m_pWndView->editcur.track= m_pSong->SONGTRACKS-1;

		m_pWndView->RecalculateColourGrid();
		m_pWndView->Repaint();
		m_pWndView->SetFocus();
		ANOTIFY(Actions::tracknumchanged);
	}

	void SongBar::OnCloseupTrackcombo() 
	{
		m_pWndView->SetFocus();
	}

	void SongBar::OnBpmAddOne()
	{
		if ( GetKeyState(VK_CONTROL)<0) SetAppSongBpm(10);
		else SetAppSongBpm(1);
		((CButton*)GetDlgItem(IDC_BPM_ADDONE))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();	
	}

	void SongBar::OnBpmDecOne() 
	{
		if ( GetKeyState(VK_CONTROL)<0) SetAppSongBpm(-10);
		else SetAppSongBpm(-1);
		((CButton*)GetDlgItem(IDC_BPM_DECONE))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();	
	}

	void SongBar::OnDecTPB()
	{
		SetAppSongTpb(-1);
		SetAppSongBpm(0);
		((CButton*)GetDlgItem(IDC_DEC_TPB))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
		m_pWndView->Repaint();
	}

	void SongBar::OnIncTPB()
	{
		SetAppSongTpb(+1);
		SetAppSongBpm(0);
		((CButton*)GetDlgItem(IDC_INC_TPB))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
		m_pWndView->Repaint();
	}

	void SongBar::SetAppSongBpm(int x) 
	{
		char buffer[16];
		if ( x != 0 )
		{
			if (Global::player()._playing ) {
				Global::song().BeatsPerMin(Global::player().bpm+x);
			} else Global::song().BeatsPerMin(Global::song().BeatsPerMin()+x);

			Global::player().SetBPM(Global::song().BeatsPerMin());
			m_pParentMain->UpdatePlayOrder(false);
		}
		if (Global::player()._playing ) {
			sprintf(buffer,"%d (%.02f)",Global::player().bpm,Global::player().EffectiveBPM());
		}
		else {
			sprintf(buffer,"%d (%.02f)",Global::song().BeatsPerMin(),Global::song().EffectiveBPM(Global::player().SampleRate()));
		}
		m_bpmlabel.SetWindowText(buffer);
	}

	void SongBar::SetAppSongTpb(int x) 
	{
		char buffer[16];
		if ( x != 0)
		{
			if (Global::player()._playing ) {
				Global::song().LinesPerBeat(Global::player().lpb+x);
			} else Global::song().LinesPerBeat(Global::song().LinesPerBeat()+x);

			Global::player().SetBPM(-1, Global::song().LinesPerBeat(), Global::song().ExtraTicksPerLine());
			m_pParentMain->UpdatePlayOrder(false);
		}
		if (Global::player()._playing )  {
			if (Global::player().ExtraTicks() != 0) {
				sprintf(buffer,"%d (+%d)",Global::player().lpb,Global::player().ExtraTicks());
			}
			else {
				sprintf(buffer,"%d",Global::player().lpb);
			}
		}
		else {
			if (Global::song().ExtraTicksPerLine() != 0) {
				sprintf(buffer,"%d (+%d)",Global::song().LinesPerBeat(),Global::song().ExtraTicksPerLine());
			}
			else {
				sprintf(buffer,"%d",Global::song().LinesPerBeat());
			}
		}
		
		m_lpblabel.SetWindowText(buffer);
	}

	void SongBar::OnCloseupCombooctave() 
	{
		m_pWndView->SetFocus();
	}

	void SongBar::OnSelchangeCombooctave() 
	{
		m_pSong->currentOctave=m_octavecombo.GetCurSel();
		
		m_pWndView->Repaint();
		m_pWndView->SetFocus();
	}

	//////////////////////////////////////////////////////////////////////
	// Function that shift the current editing octave

	void SongBar::ShiftOctave(int x)
	{
		m_pSong->currentOctave += x;
		if ( m_pSong->currentOctave < 0 )	 { m_pSong->currentOctave = 0; }
		else if ( m_pSong->currentOctave > 8 ){ m_pSong->currentOctave = 8; }

		m_octavecombo.SetCurSel(m_pSong->currentOctave);
	}
	void SongBar::UpdateMasterValue(int newvalue)
	{
		int value = helpers::dsp::AmountToSliderHoriz(value_mapper::map_256_1(newvalue));
		if ( m_pSong->_pMachine[MASTER_INDEX] != NULL)
		{
			if (m_masterslider.GetPos() != value) {
				m_masterslider.SetPos(value);
			}
		}
	}
	void SongBar::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
		CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);

		switch(nSBCode){
		case TB_BOTTOM: //fallthrough
		case TB_LINEDOWN: //fallthrough
		case TB_PAGEDOWN: //fallthrough
		case TB_TOP: //fallthrough
		case TB_LINEUP: //fallthrough
		case TB_PAGEUP: //fallthrough
			if (  the_slider == &m_masterslider) {
				((Master*)m_pSong->_pMachine[MASTER_INDEX])->_outDry = 256 * helpers::dsp::SliderToAmountHoriz(m_masterslider.GetPos());
			}
			break;
		case TB_THUMBPOSITION: //fallthrough
		case TB_THUMBTRACK:
			if ( m_pSong->_pMachine[MASTER_INDEX] != NULL && the_slider == &m_masterslider) {
				((Master*)m_pSong->_pMachine[MASTER_INDEX])->_outDry = value_mapper::map_1_256<int>(helpers::dsp::SliderToAmountHoriz(nPos));
			}
			break;
		case TB_ENDTRACK:
			m_pWndView->SetFocus();
			break;
		}
		CDialogBar::OnHScroll(nSBCode, nPos, pScrollBar);
	}

	void SongBar::OnClipbut() 
	{
		static_cast<Master*>(Global::song()._pMachine[MASTER_INDEX])->_clip = false;
		reinterpret_cast<CButton*>(GetDlgItem(IDC_CLIPBUT))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
	}

	//l and r are the left and right vu meter values
	void SongBar::UpdateVumeters()
	{
		PsycleConfig::MachineView& macView = PsycleGlobal::conf().macView();
		Master* master = static_cast<Master*>(m_pSong->_pMachine[MASTER_INDEX]);
		if (macView.draw_vus)
		{

			CStatic *stclip=(CStatic *)GetDlgItem(IDC_FRAMECLIP);
			CClientDC clcanvas(stclip);
			CRect rect;
			stclip->GetClientRect(&rect);
			
			if (master->_clip) clcanvas.FillSolidRect(rect,macView.vu3);
			else  clcanvas.FillSolidRect(rect,macView.vu2);
			
			CStatic *lv=(CStatic *)GetDlgItem(IDC_LVUM);
			CClientDC canvasl(lv);
			lv->GetClientRect(rect);
			int half = rect.Height()/2;
			int log_l=master->volumeDisplayLeft*rect.Width()/100;
			int log_r=master->volumeDisplayRight*rect.Width()/100;
			//Transpose scale from -40dbs...0dbs to 0 to rect.width()
			vuprevL = (40.0f+helpers::dsp::dB((1.0f+master->_lMax)*(1.f/master->GetAudioRange())))*rect.Width()/40.f;
			vuprevR = (40.0f+helpers::dsp::dB((1.0f+master->_rMax)*(1.f/master->GetAudioRange())))*rect.Width()/40.f;
			if(vuprevL > rect.Width()) vuprevL = rect.Width();
			if(vuprevR > rect.Width()) vuprevR = rect.Width();

			if (log_l || vuprevL)
			{
				canvasl.FillSolidRect(0,0,log_l,half,macView.vu1);
				if (vuprevL > log_l )
				{
					canvasl.FillSolidRect(log_l,0,vuprevL-log_l,half,macView.vu3);
					canvasl.FillSolidRect(vuprevL,0,rect.Width()-vuprevL,half,macView.vu2);
				}
				else 
				{
					canvasl.FillSolidRect(log_l,0,rect.Width()-log_l,half,macView.vu2);
				}
			}
			else
				canvasl.FillSolidRect(0,0,rect.Width(),half,macView.vu2);

			if (log_r || vuprevR)
			{
				canvasl.FillSolidRect(0,half+1,log_r,half,macView.vu1);
				
				if (vuprevR > log_r )
				{
					canvasl.FillSolidRect(log_r,half+1,vuprevR-log_r,half,macView.vu3);
					canvasl.FillSolidRect(vuprevR,half+1,rect.Width()-vuprevR,half,macView.vu2);
				}
				else 
				{
					canvasl.FillSolidRect(log_r,half+1,rect.Width()-log_r,half,macView.vu2);
				}
			}
			else
				canvasl.FillSolidRect(0,half+1,rect.Width(),half,macView.vu2);
		}
		master->vuupdated = true;
	}


BOOL SongBar::OnToolTipNotify( UINT unId, NMHDR *pstNMHDR, LRESULT *pstResult )
{
/*
	TOOLTIPTEXT* pstTTT = (TOOLTIPTEXT * )pstNMHDR;
	//UINT nID = pstNMHDR->idFrom;
	if ((pstTTT->uFlags & TTF_IDISHWND))
	{
		// idFrom is actually the HWND of the tool
		//nID = ::GetDlgCtrlID((HWND)nID);
		if(m_masterslider.GetPos() == 0) {
			sprintf(pstTTT->szText, "-inf");
		}
		else {
			sprintf(pstTTT->szText, "%.02f dB", helpers::dsp::dB(helpers::dsp::SliderToAmountHoriz(m_masterslider.GetPos())));
		}
		pstTTT->hinst = AfxGetResourceHandle();
		return(TRUE);
	}
*/
	return (FALSE);
} 

}}