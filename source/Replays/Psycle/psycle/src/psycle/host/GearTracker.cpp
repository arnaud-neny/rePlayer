///\file
///\brief implementation file for psycle::host::CGearTracker.
#include <psycle/host/detail/project.private.hpp>
#include "GearTracker.hpp"
#include "Sampler.hpp"
#include "PsycleConfig.hpp"
#include "Song.hpp"

namespace psycle { namespace host {

		CGearTracker::CGearTracker(CGearTracker** windowVar, Sampler& machineref)
			: CDialog(CGearTracker::IDD, AfxGetMainWnd())
			, machine(machineref)
			, windowVar_(windowVar)
		{
			CDialog::Create(IDD, AfxGetMainWnd());
		}

		void CGearTracker::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_COMBO1, m_interpol);
			DDX_Control(pDX, IDC_TRACKSLIDER2, m_polyslider);
			DDX_Control(pDX, IDC_TRACKLABEL2, m_polylabel);
			DDX_Control(pDX, IDC_DEFAULTC4, m_defaultC4);
			DDX_Control(pDX, IDC_LINEARSLIDE, m_linearSlide);
		}

/*
		BEGIN_MESSAGE_MAP(CGearTracker, CDialog)
			ON_WM_CLOSE()
			ON_WM_HSCROLL()
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_TRACKSLIDER2, OnCustomdrawNumVoices)
			ON_CBN_SELCHANGE(IDC_COMBO1, OnSelchangeCombo1)
			ON_BN_CLICKED(IDC_DEFAULTC4, OnDefaultC4)
			ON_BN_CLICKED(IDC_LINEARSLIDE, OnLinearSlide)
		END_MESSAGE_MAP()
*/

		BOOL CGearTracker::OnInitDialog() 
		{
			CDialog::OnInitDialog();

			m_interpol.AddString(_T("Hold/Chip Interp. [Lowest quality]"));
			m_interpol.AddString(_T("Linear Interpolation [Low quality]"));
			m_interpol.AddString(_T("Spline Interpolation [Medium quality]"));
			m_interpol.AddString(_T("32Tap Sinc Interp. [High quality]"));
			m_interpol.SetCurSel(machine._resampler.quality());
			m_defaultC4.SetCheck(machine.isDefaultC4());
			m_linearSlide.SetCheck(machine.isLinearSlide());
			if(PsycleGlobal::conf().patView().showA440) {
				m_defaultC4.SetWindowText(_T("C4 plays the default speed (Otherwise, C3 does it)"));
			}
			else {
				m_defaultC4.SetWindowText(_T("C5 plays the default speed (Otherwise, C4 does it)"));
			}
			CString bla(machine._editName);
			SetWindowText(bla);

			m_polyslider.SetRange(2, SAMPLER_MAX_POLYPHONY, true);
			m_polyslider.SetPos(machine._numVoices);

			return TRUE;
		}

		void CGearTracker::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
		{
			int voices = machine._numVoices;
			switch(nSBCode)
			{
				case SB_TOP:
					voices=0;
					break;
				case SB_BOTTOM:
					voices=SAMPLER_MAX_POLYPHONY;
					break;
				case SB_LINERIGHT:
				case SB_PAGERIGHT:
					if ( voices < SAMPLER_MAX_POLYPHONY) { voices++; }
					break;
				case SB_LINELEFT:
				case SB_PAGELEFT:
					if ( voices>0 ) { voices--; }
					break;
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					voices=(int)std::max(0,std::min((int)nPos,SAMPLER_MAX_POLYPHONY));
					break;
				default: 
					break;
			}
			if (voices != machine._numVoices) {
				machine._numVoices = m_polyslider.GetPos();
				for(int c=machine._numVoices; c<SAMPLER_MAX_POLYPHONY; c++)
				{
					machine._voices[c].NoteOffFast();
				}
			}
			CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
		}
		void CGearTracker::OnCustomdrawNumVoices(NMHDR* pNMHDR, LRESULT* pResult) 
		{
/*
			NMCUSTOMDRAW nmcd = *reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
			if (nmcd.dwDrawStage == CDDS_POSTPAINT)
			{
				CString buf;
				buf.Format(_T("%d"), machine._numVoices);
				m_polylabel.SetWindowText(buf);
				*pResult = CDRF_DODEFAULT;
			}
			else if (nmcd.dwDrawStage == CDDS_PREPAINT ){
				*pResult = CDRF_NOTIFYITEMDRAW|CDRF_NOTIFYPOSTPAINT;
			}
			else {
				*pResult = CDRF_DODEFAULT;
			}
*/
		}

		void CGearTracker::OnSelchangeCombo1() 
		{
			CExclusiveLock lock(&Global::song().semaphore, 2, true);
			machine.ChangeResamplerQuality((helpers::dsp::resampler::quality::type)m_interpol.GetCurSel());
		}
		void CGearTracker::OnDefaultC4() 
		{
			machine.DefaultC4(m_defaultC4.GetCheck());
		}
		void CGearTracker::OnLinearSlide() 
		{
			machine.LinearSlide(m_linearSlide.GetCheck());
		}
		void CGearTracker::OnCancel() {
			DestroyWindow();
		}
		void CGearTracker::OnClose()
		{
			CDialog::OnClose();
			DestroyWindow();
		}
		void CGearTracker::PostNcDestroy()
		{
			CDialog::PostNcDestroy();
			if(windowVar_ !=NULL) *windowVar_ = NULL;
			delete this;
		}
}}
