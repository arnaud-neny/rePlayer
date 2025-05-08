///\file
///\brief implementation file for psycle::host::CWireDlg.
#include <psycle/host/detail/project.private.hpp>
#include "WireDlg.hpp"

#include "PsycleConfig.hpp"
#include "InputHandler.hpp"
#include "VolumeDlg.hpp"
#include "ChannelMappingDlg.hpp"

#include "Machine.hpp"
#include "Player.hpp"
#include "Song.hpp"

//Included for AddNewEffectHere
#include "MainFrm.hpp"

#include "Zap.hpp"
#include <psycle/helpers/math/constants.hpp>
#include <universalis/os/aligned_alloc.hpp>

//todo: OUCH!!! This will be difficult to make DPI aware.

namespace psycle { namespace host {
		int const ID_TIMER_WIRE = 2304;

		extern CPsycleApp theApp;

		CWireDlg::CWireDlg(CChildView* mainView_, CWireDlg** windowVar_, int wireDlgIdx_,
			Wire & wireIn)
			: CDialog(CWireDlg::IDD, AfxGetMainWnd())
			, mainView(mainView_)
			, windowVar(windowVar_)
			, wireDlgIdx(wireDlgIdx_)
			, wire(wireIn)
			, srcMachine(wireIn.GetSrcMachine())
			, dstMachine(wireIn.GetDstMachine())
			, FFTMethod(0)
		{
			universalis::os::aligned_memory_alloc(16, pSamplesL, SCOPE_BUF_SIZE);
			universalis::os::aligned_memory_alloc(16, pSamplesR, SCOPE_BUF_SIZE);
			psycle::helpers::dsp::Clear(pSamplesL,SCOPE_BUF_SIZE);
			psycle::helpers::dsp::Clear(pSamplesR,SCOPE_BUF_SIZE);
			resampler.quality(psycle::helpers::dsp::resampler::quality::spline);
			CDialog::Create(IDD, AfxGetMainWnd());
		}

		CWireDlg::~CWireDlg()
		{
			universalis::os::aligned_memory_dealloc(pSamplesL);
			universalis::os::aligned_memory_dealloc(pSamplesR);
		}
		void CWireDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_VOLUME_DB, m_volabel_db);
			DDX_Control(pDX, IDC_VOLUME_PER, m_volabel_per);
			DDX_Control(pDX, IDC_SLIDER1, m_volslider);
			DDX_Control(pDX, IDC_SLIDER, m_sliderMode);
			DDX_Control(pDX, IDC_SLIDER2, m_sliderRate);
			DDX_Control(pDX, IDC_MODE, m_mode);
		}

/*
		BEGIN_MESSAGE_MAP(CWireDlg, CDialog)
			ON_WM_CLOSE()
			ON_WM_TIMER()
			ON_WM_HSCROLL()
			ON_WM_VSCROLL()
			ON_BN_CLICKED(IDC_DELETE, OnDelete)
			ON_BN_CLICKED(IDC_MODE, OnMode)
			ON_BN_CLICKED(IDC_HOLD, OnHold)
			ON_BN_CLICKED(IDC_BUT_CHANNEL, OnChannelMap)
			ON_BN_CLICKED(IDC_VOLUME_DB, OnVolumeDb)
			ON_BN_CLICKED(IDC_VOLUME_PER, OnVolumePer)
			ON_BN_CLICKED(IDC_ADDEFFECTHERE, OnAddEffectHere)
		END_MESSAGE_MAP()
*/

		BOOL CWireDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();

			scope_mode = 0;
			m_sliderMode.ShowWindow(SW_HIDE);
			scope_peak_rate = 20;
			scope_osc_freq = 14;
			scope_osc_rate = 20;
			scope_spec_rate = 20;
			scope_spec_mode = 2;
			scope_spec_samples = 2048;
			scope_phase_rate = 20;
			InitSpectrum();

			m_volslider.SetRange(0,256*4);
			m_volslider.SetTicFreq(16*4);

			float val = wire.GetVolume();
			invol = val;
			UpdateVolPerDb();
			m_volslider.SetPos(helpers::dsp::AmountToSliderVert(val));
			mult = 1.0f / srcMachine.GetAudioRange();

			char buf[128];
			sprintf(buf,"[%d] %s -> %s Connection Volume", wire.GetSrcWireIndex(), srcMachine._editName, dstMachine._editName);
			SetWindowText(buf);

			hold = FALSE;

			CClientDC dc(this);
			rc.top = 2;
			rc.left = 2;
			rc.bottom = 128+rc.top;
			rc.right = 256+rc.left;
			bufBM = new CBitmap;
			bufBM->CreateCompatibleBitmap(&dc,rc.right-rc.left,rc.bottom-rc.top);
			clearBM = new CBitmap;
			clearBM->CreateCompatibleBitmap(&dc,rc.right-rc.left,rc.bottom-rc.top);
///\todo: test
//CLEARTYPE_QUALITY
			font.CreatePointFont(70,"Tahoma");

			SetMode();
			pos = 1;

			return TRUE;
		}
		void CWireDlg::OnCancel()
		{
			OnClose();
		}

		void CWireDlg::OnClose()
		{
			KillTimer(ID_TIMER_WIRE);
			CDialog::OnClose();

			font.DeleteObject();
			bufBM->DeleteObject();
			clearBM->DeleteObject();
			linepenL.DeleteObject();
			linepenR.DeleteObject();
			linepenbL.DeleteObject();
			linepenbR.DeleteObject();
			zapObject(bufBM);
			zapObject(clearBM);
			DestroyWindow();
		}

		void CWireDlg::PostNcDestroy()
		{
			CDialog::PostNcDestroy();
			CExclusiveLock lock(&Global::song().semaphore, 2, true);
			srcMachine._pScopeBufferL = NULL;
			srcMachine._pScopeBufferR = NULL;
			srcMachine._scopeBufferIndex = 0;

			if(windowVar!= NULL) *windowVar = NULL;
			delete this;
		}
		BOOL CWireDlg::PreTranslateMessage(MSG* pMsg) 
		{
			if (pMsg->message == WM_KEYDOWN)
			{
				if (pMsg->wParam == VK_UP)
				{
					int v = m_volslider.GetPos();
					v = std::max(0,v-1);
					m_volslider.SetPos(v);
					return true;
				} else if (pMsg->wParam == VK_DOWN) {
					int v = m_volslider.GetPos();
					v=std::min(v+1,256*4);
					m_volslider.SetPos(v);
					return true;
				} else if (pMsg->wParam == VK_ESCAPE) {
					PostMessage (WM_CLOSE);
					return true;
				} else {
					mainView->SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
					return true;
				}
			}
			else if (pMsg->message == WM_KEYUP)
			{
				if (pMsg->wParam == VK_UP ||pMsg->wParam == VK_DOWN)
				{
					return true;
				} else if (pMsg->wParam == VK_ESCAPE) {
					return true;
				} else {
					mainView->SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
					return true;
				}
			}
			return CDialog::PreTranslateMessage(pMsg);
		}


		void CWireDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
			CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
			switch(nSBCode){
/*			Unnecessary
			case TB_ENDTRACK: //fallthrough
*/
			case TB_BOTTOM: //fallthrough
			case TB_LINEDOWN: //fallthrough
			case TB_PAGEDOWN: //fallthrough
			case TB_TOP: //fallthrough
			case TB_LINEUP: //fallthrough
			case TB_PAGEUP: //fallthrough
				if(the_slider == &m_sliderMode) {
					OnChangeSliderMode(m_sliderMode.GetPos());
				}
				else if(the_slider == &m_sliderRate) {
					OnChangeSliderRate(m_sliderRate.GetPos());
				}
				break;
			case TB_THUMBPOSITION: //fallthrough
			case TB_THUMBTRACK:
				if(the_slider == &m_sliderMode) {
					OnChangeSliderMode(nPos);
				}
				else if(the_slider == &m_sliderRate) {
					OnChangeSliderRate(nPos);
				}
				break;
			}
			CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
		}
		void CWireDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
			CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
			switch(nSBCode){
/*			Unnecessary
			case TB_ENDTRACK: //fallthrough
*/
			case TB_BOTTOM: //fallthrough
			case TB_LINEDOWN: //fallthrough
			case TB_PAGEDOWN: //fallthrough
			case TB_TOP: //fallthrough
			case TB_LINEUP: //fallthrough
			case TB_PAGEUP: //fallthrough
				if(the_slider == &m_volslider) {
					OnChangeSliderVol(m_volslider.GetPos());
				}
				break;
			case TB_THUMBPOSITION: //fallthrough
			case TB_THUMBTRACK:
				if(the_slider == &m_volslider) {
					OnChangeSliderVol(nPos);
				}
				break;
			}
			CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
		}
		void CWireDlg::OnTimer(UINT_PTR nIDEvent) 
		{
			if ( nIDEvent == ID_TIMER_WIRE )
			{
				CClientDC dc(this);

				CDC bufDC;
				bufDC.CreateCompatibleDC(&dc);
				CBitmap* oldbmp;
				oldbmp = bufDC.SelectObject(bufBM);

				CDC clrDC;
				clrDC.CreateCompatibleDC(&dc);
				CBitmap* oldbmp2;
				oldbmp2 = clrDC.SelectObject(clearBM);

				bufDC.BitBlt(0,0,256,128,&clrDC,0,0,SRCCOPY);

				clrDC.SelectObject(oldbmp2);
				clrDC.DeleteDC();
				CFont *oldFont=0;

				switch (scope_mode)
				{
				case 0: // vu-meter
					{
						float maxL, maxR;
						int rmsL,rmsR;
						const float multleft= invol*mult *srcMachine._lVol;
						const float multright= invol*mult *srcMachine._rVol;

						//process the buffer that corresponds to the lapsed time. Also, force 16 bytes boundaries.
						int scopesamples = std::min(SCOPE_BUF_SIZE, static_cast<int>(Global::player().SampleRate()*scope_peak_rate*0.001))&(~0x3);
						//scopeBufferIndex is the position where it will write new data.
						int index  = srcMachine._scopeBufferIndex & (~0x3); // & ~0x3 to ensure aligned to 16 bytes
						if (index > 0 && index < scopesamples) {
							int remaining = scopesamples-index; //Remaining samples at the end of the buffer.
							maxL = psycle::helpers::dsp::GetMaxVol(pSamplesL+SCOPE_BUF_SIZE-remaining,remaining);
							maxL = std::max(maxL,psycle::helpers::dsp::GetMaxVol(pSamplesL,index));
							maxR = psycle::helpers::dsp::GetMaxVol(pSamplesR+SCOPE_BUF_SIZE-remaining,remaining);
							maxR = std::max(maxR,psycle::helpers::dsp::GetMaxVol(pSamplesR,index));
	#if PSYCLE__CONFIGURATION__RMS_VUS
							if (srcMachine.Bypass())
	#endif
							{
								psycle::helpers::dsp::GetRMSVol(srcMachine.rms, pSamplesL+SCOPE_BUF_SIZE-remaining, 
									pSamplesR+SCOPE_BUF_SIZE-remaining, remaining);
								psycle::helpers::dsp::GetRMSVol(srcMachine.rms, pSamplesL, pSamplesR, index);
							}
						}
						else {
							if (index == 0) {index=SCOPE_BUF_SIZE; }
							maxL = psycle::helpers::dsp::GetMaxVol(pSamplesL+index-scopesamples,scopesamples);
							maxR = psycle::helpers::dsp::GetMaxVol(pSamplesR+index-scopesamples,scopesamples);
	#if PSYCLE__CONFIGURATION__RMS_VUS
							if (srcMachine.Bypass())
	#endif
							{
								psycle::helpers::dsp::GetRMSVol(srcMachine.rms,pSamplesL+index-scopesamples, 
									pSamplesR+index-scopesamples, scopesamples);
							}

						}
						maxL= 36 - helpers::dsp::dB(maxL*multleft+0.0000001f) * 3;
						maxR= 36 - helpers::dsp::dB(maxR*multright+0.0000001f) * 3;
						rmsL= 36 - helpers::dsp::dB(srcMachine.rms.previousLeft*multleft+0.0000001f) * 3;
						rmsR= 36 - helpers::dsp::dB(srcMachine.rms.previousRight*multright+0.0000001f) * 3;

						if (maxL<peakL) //  it is a cardinal value, so smaller means higher peak.
						{
							if (maxL<0) maxL=0;
							peakL = maxL;		peakLifeL = 2000/scope_peak_rate; //2 seconds
						}

						if (maxR<peakR)//  it is a cardinal value, so smaller means higher peak.
						{
							if (maxR<0) maxR=0;
							peakR = maxR;		peakLifeR = 2000/scope_peak_rate; //2 seconds
						}

						// now draw our scope
						// LEFT CHANNEL
						RECT rect;
						rect.left = 128-60;
						rect.right = rect.left+24;
						if (peakL<128)
						{
							CPen *oldpen;
							if (peakL<36) {
								oldpen = bufDC.SelectObject(&linepenbL);
							}
							else {
								oldpen = bufDC.SelectObject(&linepenL);
							}
							bufDC.MoveTo(rect.left-1,peakL);
							bufDC.LineTo(rect.right-1,peakL);
							bufDC.SelectObject(oldpen);
						}

						rect.top = maxL;
						rect.bottom = 128;
						bufDC.FillSolidRect(&rect,0xC08040);

						rect.left = 128+6;
						rect.right = rect.left+24;
						rect.top = rmsL;
						rect.bottom = 128;
						bufDC.FillSolidRect(&rect,0xC08040);

						// RIGHT CHANNEL 
						rect.left = 128-30;
						rect.right = rect.left+24;
						if (peakR<128)
						{
							CPen *oldpen;
							if (peakR<36) {
								oldpen = bufDC.SelectObject(&linepenbR);
							}
							else {
								oldpen = bufDC.SelectObject(&linepenR);
							}

							bufDC.MoveTo(rect.left-1,peakR);
							bufDC.LineTo(rect.right-1,peakR);
							bufDC.SelectObject(oldpen);
						}

						rect.top = maxR;
						rect.bottom = 128;
						bufDC.FillSolidRect(&rect,0x90D040);

						rect.left = 128+36;
						rect.right = rect.left+24;
						rect.top = rmsR;
						rect.bottom = 128;
						bufDC.FillSolidRect(&rect,0x90D040);


						// update peak counter.
						if (!hold)
						{
							if  (peakLifeL>0 ||peakLifeR>0)
							{
								peakLifeL--;
								peakLifeR--;
								if (peakLifeL <= 0)	{	peakL = 128;	}
								if (peakLifeR <= 0)	{	peakR = 128;	}
							}
						}
						char buf[64];
						sprintf(buf,"Refresh %.2fhz",1000.0f/scope_peak_rate);
						oldFont= bufDC.SelectObject(&font);
						bufDC.SetBkMode(TRANSPARENT);
						bufDC.SetTextColor(0x505050);
						bufDC.TextOut(4, 128-14, buf);
						bufDC.SelectObject(oldFont);
					}
					break;
				case 1: // oscilloscope
					{
						// ok this is a little tricky - it chases the wrapping buffer, starting at the last sample 
						// buffered and working backwards - it does it this way to minimize chances of drawing 
						// erroneous data across the buffering point
						//scopeBufferIndex is the position where it will write new data. Pos is the buffer position on Hold.
						 //keeping nOrig, since the buffer can change while we draw it.
						int nOrig  = (srcMachine._scopeBufferIndex == 0) ? SCOPE_BUF_SIZE-pos : srcMachine._scopeBufferIndex-pos;
						if (nOrig < 0.f) { nOrig+=SCOPE_BUF_SIZE; }
						// osc_freq range 5..100, freq range 12..5K
						const int freq = (scope_osc_freq*scope_osc_freq)>>1;
						const float add = (float(Global::player().SampleRate())/float(freq))/256.0f;
						const float multleft= invol*mult *srcMachine._lVol;
						const float multright= invol*mult *srcMachine._rVol;
						RECT rect = {0,0,1,0};
						float n=nOrig;
						for (int x = 256; x >= 0; x--, rect.left++, rect.right++) {
							n -= add;
							if (n < 0.f) { n+=SCOPE_BUF_SIZE; }
							
							int aml = GetY(resampler.work_float(pSamplesL,n,SCOPE_BUF_SIZE, NULL,pSamplesL, pSamplesL+SCOPE_BUF_SIZE-1),multleft);
							int amr = GetY(resampler.work_float(pSamplesR,n,SCOPE_BUF_SIZE, NULL,pSamplesR, pSamplesR+SCOPE_BUF_SIZE-1),multright);

							int smaller, bigger, halfbottom, bottompeak;
							COLORREF colourtop, colourbottom;
							if (aml < amr) { smaller=aml; bigger=amr; colourtop=CLLEFT; colourbottom=CLRIGHT; }
							else {			smaller=amr; bigger=aml; colourtop=CLRIGHT; colourbottom=CLLEFT; }
							if (smaller < 64 ){
								rect.top=smaller;
								if (bigger < 64) {
									rect.bottom=bigger;
									halfbottom=bottompeak=64;
								}
								else {
									rect.bottom=halfbottom=64;
									bottompeak=bigger;
								}
							}
							else {
								rect.top=rect.bottom=64;
								halfbottom=smaller;
								bottompeak=bigger;
							}
							bufDC.FillSolidRect(&rect,colourtop);

							rect.top = rect.bottom;
							rect.bottom = halfbottom;
							bufDC.FillSolidRect(&rect,CLBOTH);

							rect.top = rect.bottom;
							rect.bottom = bottompeak;
							bufDC.FillSolidRect(&rect,colourbottom);

						}
						// red line if last frame was clipping
						if (clip)
						{
							RECT rect = {0,32,256,34};
							bufDC.FillSolidRect(&rect,0x00202040);

							rect.top = 95;
							rect.bottom = rect.top+2;
							bufDC.FillSolidRect(&rect,0x00202040);

							rect.top = 64-4;
							rect.bottom = rect.top+8;
							bufDC.FillSolidRect(&rect,0x00202050);

							rect.top = 64-2;
							rect.bottom = rect.top+4;
							bufDC.FillSolidRect(&rect,0x00101080);

							clip = FALSE;
						}

						char buf[64];
						sprintf(buf,"Frequency %dhz Refresh %.2fhz",freq,1000.0f/scope_osc_rate);
						oldFont= bufDC.SelectObject(&font);
						bufDC.SetBkMode(TRANSPARENT);
						bufDC.SetTextColor(0x505050);
						bufDC.TextOut(4, 128-14, buf);
						bufDC.SelectObject(oldFont);
					}
					break;

				case 2: // spectrum analyzer
					{
						float db_left[SCOPE_SPEC_BANDS];
						float db_right[SCOPE_SPEC_BANDS];
						const float multleft= invol*mult *srcMachine._lVol;
						const float multright= invol*mult *srcMachine._rVol;

#if 0
						if (FFTMethod == 0)
#endif
						{
							//schism mod FFT
							float temp[SCOPE_BUF_SIZE];
							float tempout[SCOPE_BUF_SIZE>>1];
							FillLinearFromCircularBuffer(pSamplesL,temp, multleft);
							fftSpec.CalculateSpectrum(temp,tempout);
							fftSpec.FillBandsFromFFT(tempout,db_left);

							FillLinearFromCircularBuffer(pSamplesR,temp, multright);
							fftSpec.CalculateSpectrum(temp,tempout);
							fftSpec.FillBandsFromFFT(tempout,db_right);
						}
#if 0
						else if (FFTMethod == 1)
						{
							//DM FFT
							float temp[SCOPE_BUF_SIZE];
							float tempout[SCOPE_BUF_SIZE];

							FillLinearFromCircularBuffer(pSamplesL,temp, multleft);
							helpers::dsp::dmfft::WindowFunc(3,scope_spec_samples,temp);
							helpers::dsp::dmfft::PowerSpectrum(scope_spec_samples,temp, tempout);
							fftSpec.FillBandsFromFFT(tempout,db_left);

							FillLinearFromCircularBuffer(pSamplesR,temp, multright);
							helpers::dsp::dmfft::WindowFunc(3,scope_spec_samples,temp);
							helpers::dsp::dmfft::PowerSpectrum(scope_spec_samples,temp, tempout);
							fftSpec.FillBandsFromFFT(tempout,db_right);
						}
						else if (FFTMethod == 2)
						{
							// Psycle's pseudo FT
							const float dbinvSamples = psycle::helpers::dsp::dB(1.0f/(scope_spec_samples>>2)); // >>2, because else seems 12dB's off
							float aal[SCOPE_SPEC_BANDS]={0};
							float aar[SCOPE_SPEC_BANDS]={0}; 
							float bbl[SCOPE_SPEC_BANDS]={0}; 
							float bbr[SCOPE_SPEC_BANDS]={0};

							//scopeBufferIndex is the position where it will write new data.
							int index  = (srcMachine._scopeBufferIndex == 0) SCOPE_BUF_SIZE-1 : srcMachine._scopeBufferIndex-1;
							for (int i=0;i<scope_spec_samples;i++, index--, index&=(SCOPE_BUF_SIZE-1)) 
							{ 
								const float wl=pSamplesL[index]*multleft;
								const float wr=pSamplesR[index]*multright;
								for(int h=0;h<SCOPE_SPEC_BANDS;h++) 
								{ 
									aal[h]+=wl*cth[i][h];
									bbl[h]+=wl*sth[i][h];
									aar[h]+=wr*cth[i][h];
									bbr[h]+=wr*sth[i][h];
								} 
							} 

							for (int h=0;h<SCOPE_SPEC_BANDS;h++) 
							{
								float out = aal[h]*aal[h]+bbl[h]*bbl[h];
								db_left[h]= helpers::dsp::powerdB(out+0.0000001f)+dbinvSamples;
								out = aar[h]*aar[h]+bbr[h]*bbr[h];
								db_right[h]= helpers::dsp::powerdB(out+0.0000001f)+dbinvSamples;
							}
						}
#endif
						// draw our bands
						int DCBar = fftSpec.getDCBars();
						RECT rect = {0,0,SCOPE_BARS_WIDTH,0};
						for (int i = 0; i < SCOPE_SPEC_BANDS; 
							i++, rect.left+=SCOPE_BARS_WIDTH, rect.right+=SCOPE_BARS_WIDTH)
						{
							//Remember, 0 -> top of spectrum, 128 bottom of spectrum.
							int curpeak, halfpeak;
							COLORREF colour;
							int aml = - db_left[i] * 2; // Reducing visible range from 128dB to 64dB.
							int amr = - db_right[i] * 2; // Reducing visible range from 128dB to 64dB.
							//int aml = - db_left[i];
							//int amr = - db_right[i];
							aml = (aml < 0) ? 0 : (aml > 128) ? 128 : aml;
							amr = (amr < 0)? 0 : (amr > 128) ? 128 : amr;
							if (i<= DCBar) {
								curpeak=std::min(aml,amr);
								halfpeak=128;
								colour=CLBARDC;
							}
							else if (aml < amr) {
								curpeak=aml;
								halfpeak=amr;
								colour=CLLEFT;
							}
							else {
								curpeak=amr;
								halfpeak=aml;
								colour=CLRIGHT;
							}
							if (curpeak < bar_heights[i]) { bar_heights[i] = curpeak; }
							else if (!hold && bar_heights[i]<128)
							{
								bar_heights[i]+=128/scope_spec_rate*0.5;//two seconds to decay
								if (bar_heights[i] > 128) { bar_heights[i] = 128; }
							}

							rect.top = curpeak;
							rect.bottom = halfpeak;
							bufDC.FillSolidRect(&rect,colour);

							rect.top = rect.bottom;
							rect.bottom = 128;
							bufDC.FillSolidRect(&rect,CLBOTH);

							rect.top = bar_heights[i];
							rect.bottom = bar_heights[i]+1;
							bufDC.FillSolidRect(&rect,CLBARPEAK);
						}
						char buf[64];
						sprintf(buf,"%d Samples Refresh %.2fhz",scope_spec_samples,1000.0f/scope_spec_rate);
						//sprintf(buf,"%d Samples Refresh %.2fhz Window %d",scope_spec_samples,1000.0f/scope_spec_rate, FFTMethod);
						CFont* oldFont= bufDC.SelectObject(&font);
						bufDC.SetBkMode(TRANSPARENT);
						bufDC.SetTextColor(0x505050);
						bufDC.TextOut(4, 128-14, buf);
						bufDC.SelectObject(oldFont);
					}
					break;
				case 3: // phase scope
					{

						// ok we need some points:

						// max vol center
						// max vol phase center
						// max vol left
						// max vol dif left
						// max vol phase left
						// max vol dif phase left
						// max vol right
						// max vol dif right
						// max vol phase right
						// max vol dif phase right

						float mvc, mvpc, mvl, mvdl, mvpl, mvdpl, mvr, mvdr, mvpr, mvdpr;
						mvc = mvpc = mvl = mvdl = mvpl = mvdpl = mvr = mvdr = mvpr = mvdpr = 0.0f;
						const float multleft= invol*mult *srcMachine._lVol;
						const float multright= invol*mult *srcMachine._rVol;


						//process the buffer that corresponds to the lapsed time. Also, force 16 bytes boundaries.
						int scopesamples = std::min(SCOPE_BUF_SIZE, static_cast<int>(Global::player().SampleRate()*scope_peak_rate*0.001))&(~3);
						//scopeBufferIndex is the position where it will write new data.
						int index  = (srcMachine._scopeBufferIndex == 0) ? SCOPE_BUF_SIZE-1 : srcMachine._scopeBufferIndex-1;
						for (int i=0;i<scopesamples;i++, index--, index&=(SCOPE_BUF_SIZE-1)) 
						{ 
							float wl=pSamplesL[index]*multleft;
							float wr=pSamplesR[index]*multright;
							float awl=fabsf(wl);
							float awr=fabsf(wr);
#if 0
							float mid=wl+wr;
							float side=wl-wr;
							float absmid=awl+awr;
							float absside=awl-awr;
							float abssideabs=fabsf(absside);
							float midabs=fabs(mid);
							float sideabs=fabs(side);
#endif

							if ((wl < 0.0f && wr > 0.0f) || (wl > 0.0f && wr < 0.0f)) {
								// phase difference
								if (awl > awr && awl-awr > mvdpl) { mvdpl = awl-awr; }
								else if (awl < awr && awr-awl > mvdpr) { mvdpr = awr-awl; }
								if (awl+awr > mvpl) { mvpl = awl+awr; }
								if (awr+awl > mvpr) { mvpr = awr+awl; }
							}
							else if (awl > awr && awl-awr > mvdl) {
								// left
								mvdl = awl-awr;
							}
							else if (awl < awr && awr-awl > mvdr) {
								// right
								mvdr = awr-awl;
							}
							if (awl > mvl) { mvl = awl; }
							if (awr > mvr) { mvr = awr; }

						} 

					// ok we have some data, lets make some points and draw them
						float maxval = std::max(mvl,mvr);
						//Adapt difference range independently of max amplitude.
						if (maxval > 0.0f) {
							mvdl /= maxval;
							mvdr /= maxval;
							mvdpl /= maxval;
							mvdpr /= maxval;
							mvpl /= 2.f;
							mvpr /= 2.f;
						}


						// maintain peaks
						if (mvpl > o_mvpl)
						{
							o_mvpl = mvpl;
							o_mvdpl = mvdpl;
						}
/*						if (mvpc > o_mvpc)
						{
							o_mvpc = mvpc;
						}
*/
						if (mvpr > o_mvpr)
						{
							o_mvpr = mvpr;
							o_mvdpr = mvdpr;
						}
						if (mvl > o_mvl)
						{
							o_mvl = mvl;
							o_mvdl = mvdl;
						}
/*						if (mvc > o_mvc)
						{
							o_mvc = mvc;
						}
*/
						if (mvr > o_mvr)
						{
							o_mvr = mvr;
							o_mvdr = mvdr;
						}

						int x,y;

						CPen* oldpen = bufDC.SelectObject(&linepenbL);
						float quarterpi = helpers::math::pi_f*0.25f;

						x=helpers::math::round<int,float>(sinf(-quarterpi-(o_mvdpl*quarterpi))*o_mvpl*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf(-quarterpi-(o_mvdpl*quarterpi))*o_mvpl*128.0f) + 128;
						bufDC.MoveTo(x,y);
						bufDC.LineTo(128,128);
//						bufDC.LineTo(128,128-helpers::math::round<int,float>(o_mvpc*128.0f));
//						bufDC.MoveTo(128,128);
						x=helpers::math::round<int,float>(sinf(quarterpi+(o_mvdpr*quarterpi))*o_mvpr*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf(quarterpi+(o_mvdpr*quarterpi))*o_mvpr*128.0f) + 128;
						bufDC.LineTo(x,y);
										
						// panning data
						bufDC.SelectObject(&linepenbR);

						x=helpers::math::round<int,float>(sinf(-(o_mvdl*quarterpi))*o_mvl*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf(-(o_mvdl*quarterpi))*o_mvl*128.0f) + 128;
						bufDC.MoveTo(x,y);
						bufDC.LineTo(128,128);
//						bufDC.LineTo(128,128-helpers::math::round<int,float>(o_mvc*128.0f));
//						bufDC.MoveTo(128,128);
						x=helpers::math::round<int,float>(sinf((o_mvdr*quarterpi))*o_mvr*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf((o_mvdr*quarterpi))*o_mvr*128.0f) + 128;
						bufDC.LineTo(x,y);

						bufDC.SelectObject(&linepenL);

						x=helpers::math::round<int,float>(sinf(-quarterpi-(mvdpl*quarterpi))*mvpl*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf(-quarterpi-(mvdpl*quarterpi))*mvpl*128.0f) + 128;
						bufDC.MoveTo(x,y);
						bufDC.LineTo(128,128);
//						bufDC.LineTo(128,128-helpers::math::round<int,float>(mvpc*128.0f));
//						bufDC.MoveTo(128,128);
						x=helpers::math::round<int,float>(sinf(quarterpi+(mvdpr*quarterpi))*mvpr*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf(quarterpi+(mvdpr*quarterpi))*mvpr*128.0f) + 128;
						bufDC.LineTo(x,y);
										
						// panning data
						bufDC.SelectObject(&linepenR);

						x=helpers::math::round<int,float>(sinf(-(mvdl*quarterpi))*mvl*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf(-(mvdl*quarterpi))*mvl*128.0f) + 128;
						bufDC.MoveTo(x,y);
						bufDC.LineTo(128,128);
//						bufDC.LineTo(128,128-helpers::math::round<int,float>(mvc*128.0f));
//						bufDC.MoveTo(128,128);
						x=helpers::math::round<int,float>(sinf((mvdr*quarterpi))*mvr*128.0f) + 128;
						y=helpers::math::round<int,float>(-cosf((mvdr*quarterpi))*mvr*128.0f) + 128;
						bufDC.LineTo(x,y);

						if (!hold)
						{
							float rate = 2.f/scope_phase_rate;//Decay in half a second.
							o_mvpl -= rate;
							o_mvpc -= rate;
							o_mvpr -= rate;
							o_mvl -= rate;
							o_mvc -= rate;
							o_mvr -= rate;
						}
						bufDC.SelectObject(oldpen);

						char buf[64];
						sprintf(buf,"Refresh %.2fhz",1000.0f/scope_phase_rate);
						oldFont= bufDC.SelectObject(&font);
						bufDC.SetBkMode(TRANSPARENT);
						bufDC.SetTextColor(0x505050);
						bufDC.TextOut(4, 128-14, buf);
						bufDC.SelectObject(oldFont);
					}
					break;
				}
				// and debuffer
				dc.BitBlt(rc.top,rc.left,rc.right-rc.left,rc.bottom-rc.top,&bufDC,0,0,SRCCOPY);

				bufDC.SelectObject(oldbmp);
				bufDC.DeleteDC();
			}
			
			CDialog::OnTimer(nIDEvent);
		}

		void CWireDlg::OnChangeSliderMode(UINT nPos) 
		{
			switch (scope_mode)
			{
			case 1:
				scope_osc_freq = nPos;
				if (hold)
				{
					// osc_freq range 5..100, freq range 12..5K
					const int freq = (scope_osc_freq*scope_osc_freq)>>1;
					int minus = std::min(SCOPE_BUF_SIZE,Global::player().SampleRate()/freq);
					m_sliderRate.SetRange(1,1+SCOPE_BUF_SIZE-minus);
				}
				break;
			case 2:
				scope_spec_mode = nPos;
				scope_spec_samples = (nPos==1)?SCOPE_SPEC_BANDS*2:(nPos==2)?SCOPE_BUF_SIZE/4:SCOPE_BUF_SIZE;
				SetMode();
				InitSpectrum();
				break;
			}
		}

		void CWireDlg::OnChangeSliderRate(UINT nPos) 
		{
			switch (scope_mode)
			{
			case 0:
				if (scope_peak_rate != nPos)
				{
					scope_peak_rate = nPos;
					SetTimer(ID_TIMER_WIRE,scope_peak_rate,0);
				}
				break;
			case 1:
				if (hold)
				{
					pos = nPos&(SCOPE_BUF_SIZE-1);
				}
				else
				{
					pos = 1;
					if (scope_osc_rate != nPos)
					{
						scope_osc_rate = nPos;
						SetTimer(ID_TIMER_WIRE,scope_osc_rate,0);
					}
				}
				break;
			case 2:
				if (scope_spec_rate != nPos)
				{
					scope_spec_rate = nPos;
					SetTimer(ID_TIMER_WIRE,scope_spec_rate,0);
				}
				break;
			case 3:
				if (scope_phase_rate != nPos)
				{
					scope_phase_rate = nPos;
					SetTimer(ID_TIMER_WIRE,scope_phase_rate,0);
				}
				break;
			}
		}

		void CWireDlg::OnChangeSliderVol(UINT nPos) 
		{
			invol = helpers::dsp::SliderToAmountVert(nPos);

			UpdateVolPerDb();
			float f = wire.GetVolume();
			if (f != invol)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();
				wire.SetVolume(invol);
			}
		}

		void CWireDlg::UpdateVolPerDb() {
			char bufper[32];
			char bufdb[32];
			if (invol > 1.0f)
			{	
				sprintf(bufper,"%.2f%%",invol*100); 
				sprintf(bufdb,"+%.1f dB",20.0f * log10(invol)); 
			}
			else if (invol == 1.0f)
			{	
				sprintf(bufper,"100.00%%"); 
				sprintf(bufdb,"0.0 dB"); 
			}
			else if (invol > 0.0f)
			{	
				sprintf(bufper,"%.2f%%",invol*100); 
				sprintf(bufdb,"%.1f dB",20.0f * log10(invol)); 
			}
			else 
			{				
				sprintf(bufper,"0.00%%"); 
				sprintf(bufdb,"-Inf. dB"); 
			}

			m_volabel_per.SetWindowText(bufper);
			m_volabel_db.SetWindowText(bufdb);
		}
		void CWireDlg::OnDelete() 
		{
			PsycleGlobal::inputHandler().AddMacViewUndo();
			CExclusiveLock lock(&Global::song().semaphore, 2, true);
			wire.Disconnect();
			PostMessage (WM_CLOSE);
		}
		void CWireDlg::OnAddEffectHere()
		{
			int newMacidx = Global::song().GetFreeFxBus();
			mainView->NewMachine((srcMachine._x+dstMachine._x)/2,(srcMachine._y+dstMachine._y)/2,newMacidx);

			if(newMacidx != -1 && Global::song()._pMachine[newMacidx]) {
				Machine* newMac = Global::song()._pMachine[newMacidx];
				Global::song().ChangeWireSourceMacBlocking(newMac,&dstMachine,newMac->GetFreeOutputWire(0),wire.GetDstWireIndex());
				{
					CExclusiveLock lock(&Global::song().semaphore, 2, true);
					Global::song().InsertConnectionNonBlocking(&srcMachine, newMac);
				}
				CMainFrame* pParentMain = ((CMainFrame *)theApp.m_pMainWnd);
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				if (mainView->viewMode==view_modes::machine)
				{
					mainView->Repaint();
				}
				PostMessage (WM_CLOSE);
			}
		}
		void CWireDlg::OnMode()
		{
			scope_mode++;
			if (scope_mode > 3)
			{
				scope_mode = 0;
			}
			SetMode();
		}

		void CWireDlg::OnHold()
		{
			CExclusiveLock lock(&Global::song().semaphore, 2, true);

#if 0 // test FFT Methods
			FFTMethod++;
			if (FFTMethod>2) FFTMethod=0;
			return;
#elif 0 //test FFT windows
			FFTMethod++;
			if (FFTMethod>6) FFTMethod=0;
			fftSpec.Setup((helpers::dsp::FftWindowType)FFTMethod, scope_spec_samples, SCOPE_SPEC_BANDS);
			return;
#endif
			hold = !hold;
			pos = 1;
			switch (scope_mode)
			{
			case 1:
				if (hold)
				{
					// osc_freq range 5..100, freq range 12..5K
					const int freq = (scope_osc_freq*scope_osc_freq)>>1;
					int minus = std::min(SCOPE_BUF_SIZE,Global::player().SampleRate()/freq);
					m_sliderRate.SetRange(1,1+SCOPE_BUF_SIZE-minus);
					m_sliderRate.SetPos(100);
					m_sliderRate.SetPos(1);
				}
				else
				{
					pos = 1;
					m_sliderRate.SetRange(10,100);
					m_sliderRate.SetPos(scope_osc_rate);
				}
			}
			if (hold)
			{
				srcMachine._pScopeBufferL = NULL;
				srcMachine._pScopeBufferR = NULL;
			}
			else
			{
				srcMachine._pScopeBufferL = pSamplesL;
				srcMachine._pScopeBufferR = pSamplesR;
			}
		}

		void CWireDlg::OnVolumeDb() 
		{
			CVolumeDlg dlg;
			dlg.volume = invol;
			dlg.edit_type = 0;
			if (dlg.DoModal() == IDOK)
			{
				int volpos = helpers::dsp::AmountToSliderVert(dlg.volume);
				m_volslider.SetPos(volpos);
				OnChangeSliderVol(volpos);
			}
		}
		void CWireDlg::OnChannelMap() 
		{
			CChannelMappingDlg dlg(wire,mainView);
			if(dlg.DoModal() == IDOK) 
			{
			}
		}
		void CWireDlg::OnVolumePer() 
		{
			CVolumeDlg dlg;
			dlg.volume = invol;
			dlg.edit_type = 1;
			if (dlg.DoModal() == IDOK)
			{
				int volpos = helpers::dsp::AmountToSliderVert(dlg.volume);
				m_volslider.SetPos(volpos);
				OnChangeSliderVol(volpos);
			}
		}

		void CWireDlg::InitSpectrum()
		{
			//Setup schismMod FFT
			fftSpec.Setup(helpers::dsp::hann, scope_spec_samples, SCOPE_SPEC_BANDS);

			//Setup DM FFT
			//(Could setup window, but right now it is calculated on work)

			//Setup Psycle's pseudo FT
#if 0
			const float barsize = float(scope_spec_samples>>1)/SCOPE_SPEC_BANDS;
			const float constantexponential = 1.0f/(float)SCOPE_SPEC_BANDS;
			//factor -> set range from 2^0 to 2^8.
			//factor2 -> scale the result to the FFT output size
			const float constantlogfactor = 8.f/(float)SCOPE_SPEC_BANDS;
			const float constantlogfactor2 = float(scope_spec_samples>>1)/256.f;//this 256 is 2^8, not bands.
			for (int i=0;i<scope_spec_samples;i++)
			{ 
				//Linear -pi to pi.
				const float constant = 2.0f * helpers::math::pi_f * (-0.5f + ((float)i/(scope_spec_samples-1)));

				//Hann window 
				const float window = 0.50f - 0.50f * cosf(2.0f * helpers::math::pi * i / float(scope_spec_samples-1));

				float j=barsize;
				cth[i][0] = window;
				sth[i][0] = window;
				for(int h=1;h<SCOPE_SPEC_BANDS;h++)
				{ 
					float th;
					if (scope_spec_mode == 1 ) {
						//this is linear. Equals to (samples/2)*h/bands *constant
						th=j* constant; 
					}
					else if (scope_spec_mode == 2 ) {
						//this makes it somewhat exponential. Equals to (samples/2)*h*h/(bands*bands)  *constant
						th=j* h*constantexponential  *constant; 
					}
					else{
						//This simulates a constant note scale. Equals to (samples/2)*(pow(bands,h/bands)-1)/bands *constant
						//Note: x^(y*z) = (x^z)^y, but I'm unsure if powf(2.0f,x) is optimized compared to other bases.
						th = constantlogfactor2*(powf(2.0f,(float)h*constantlogfactor) -1.f) *constant;
					}
					cth[i][h] = cosf(th) * window;
					sth[i][h] = sinf(th) * window;
					j+=barsize;
				}
				helpers::math::erase_all_nans_infinities_and_denormals(cth[i], SCOPE_SPEC_BANDS);
				helpers::math::erase_all_nans_infinities_and_denormals(sth[i], SCOPE_SPEC_BANDS);
			}
#endif
		}
		void CWireDlg::FillLinearFromCircularBuffer(float inBuffer[], float outBuffer[], float vol)
		{
			//scopeBufferIndex is the position where it will write new data. 
			int index  = srcMachine._scopeBufferIndex & (~0x3); // & ~0x3 to align to 16 bytes.
			if (index > 0 && index < scope_spec_samples) {
				int remaining = scope_spec_samples-index; //Remaining samples at the end of the buffer.
					dsp::MovMul(inBuffer+SCOPE_BUF_SIZE-remaining,outBuffer,remaining,vol);
					dsp::MovMul(inBuffer,outBuffer+remaining,index,vol);
			}
			else {
				if (index == 0) { index=SCOPE_BUF_SIZE; }
				dsp::MovMul(inBuffer+index-scope_spec_samples,outBuffer,scope_spec_samples,vol);
			}
		}

		void CWireDlg::SetMode()
		{
			CExclusiveLock lock(&Global::song().semaphore, 2, true);

			CClientDC dc(this);

			CDC bufDC;
			bufDC.CreateCompatibleDC(&dc);
			CBitmap* oldbmp;
			oldbmp = bufDC.SelectObject(clearBM);

			bufDC.FillSolidRect(0,0,rc.right-rc.left,rc.bottom-rc.top,0);

			linepenL.DeleteObject();
			linepenR.DeleteObject();
			linepenbL.DeleteObject();
			linepenbR.DeleteObject();

			char buf[64];
			switch (scope_mode)
			{
			case 0:
				// vu
				KillTimer(ID_TIMER_WIRE);

				{
					CFont* oldFont= bufDC.SelectObject(&font);
					bufDC.SetBkMode(TRANSPARENT);
					bufDC.SetTextColor(0x606060);

					RECT rect;
					rect.left = 32+24;
					rect.right = 256-32-24;

					rect.top=2;
					rect.bottom=rect.top+1;
					sprintf(buf,"Peak");
					bufDC.TextOut(128-42, rect.top, buf);
					sprintf(buf,"RMS");
					bufDC.TextOut(128+25, rect.top, buf);


					rect.top = 36-18;
					rect.bottom = rect.top+1;
					bufDC.FillSolidRect(&rect,0x00606060);
					sprintf(buf,"+6 db");
					bufDC.TextOut(32-1, rect.top-6, buf);
					bufDC.TextOut(256-32-22, rect.top-6, buf);

					rect.top = 36+18;
					rect.bottom = rect.top+1;
					bufDC.FillSolidRect(&rect,0x00606060);

					sprintf(buf,"-6 db");
					bufDC.TextOut(32-1+4, rect.top-6, buf);
					bufDC.TextOut(256-32-22, rect.top-6, buf);

					rect.top = 36+36;
					rect.bottom = rect.top+1;
					bufDC.FillSolidRect(&rect,0x00606060);
					sprintf(buf,"-12 db");
					bufDC.TextOut(32-1-6+4, rect.top-6, buf);
					bufDC.TextOut(256-32-22, rect.top-6, buf);

					rect.top = 36+36+36;
					rect.bottom = rect.top+1;
					bufDC.FillSolidRect(&rect,0x00606060);
					sprintf(buf,"-24 db");
					bufDC.TextOut(32-1-6+4, rect.top-6, buf);
					bufDC.TextOut(256-32-22, rect.top-6, buf);

					rect.top = 36;
					rect.bottom = rect.top+1;
					bufDC.SetTextColor(0x00707070);
					bufDC.FillSolidRect(&rect,0x00707070);
					sprintf(buf,"0 db");
					bufDC.TextOut(32-1+6, rect.top-6, buf);
					bufDC.TextOut(256-32-22, rect.top-6, buf);

					bufDC.SelectObject(oldFont);
				}
				m_sliderMode.ShowWindow(SW_HIDE);
				m_sliderRate.SetRange(10,100);
				m_sliderRate.SetPos(scope_peak_rate);
				sprintf(buf,"Scope Mode");
				peakL = peakR = 128.0f;
				srcMachine._pScopeBufferL = pSamplesL;
				srcMachine._pScopeBufferR = pSamplesR;
				linepenL.CreatePen(PS_SOLID, 2, 0xc08080);
				linepenR.CreatePen(PS_SOLID, 2, 0x80c080);
				linepenbL.CreatePen(PS_SOLID, 2, 0xc080F0);
				linepenbR.CreatePen(PS_SOLID, 2, 0x80c0F0);
				SetTimer(ID_TIMER_WIRE,scope_peak_rate,0);
				break;
			case 1:
				// oscilloscope
				KillTimer(ID_TIMER_WIRE);
				{
					RECT rect= {0,32,256,34};
					bufDC.FillSolidRect(&rect,0x00202020);

					rect.top = 95;
					rect.bottom = rect.top+2;
					bufDC.FillSolidRect(&rect,0x00202020);

					rect.top = 64-4;
					rect.bottom = rect.top+8;
					bufDC.FillSolidRect(&rect,0x00202020);

					rect.top = 64-2;
					rect.bottom = rect.top+4;
					bufDC.FillSolidRect(&rect,0x00404040);
				}
				linepenL.CreatePen(PS_SOLID, 1, 0xc08080);
				linepenR.CreatePen(PS_SOLID, 1, 0x80c080);

				m_sliderMode.ShowWindow(SW_SHOW);
				m_sliderMode.SetRange(5, 100);
				m_sliderMode.SetPos(scope_osc_freq);
				pos = 1;
				m_sliderRate.SetRange(10,100);
				m_sliderRate.SetPos(scope_osc_rate);
				sprintf(buf,"Oscilloscope");
				srcMachine._pScopeBufferL = pSamplesL;
				srcMachine._pScopeBufferR = pSamplesR;
				SetTimer(ID_TIMER_WIRE,scope_osc_rate,0);
				break;
			case 2:
				// spectrum analyzer
				KillTimer(ID_TIMER_WIRE);
				{
					for (int i = 0; i < SCOPE_SPEC_BANDS; i++)
					{
						bar_heights[i]=256;
					}
					CFont* oldFont= bufDC.SelectObject(&font);
					bufDC.SetBkMode(TRANSPARENT);
					bufDC.SetTextColor(0x505050);

					RECT rect;
					rect.left = 0;
					rect.right = 256;
					sprintf(buf,"db");
					bufDC.TextOut(3, 0, buf);
					bufDC.TextOut(256-13, 0, buf);
					for(int i=1;i<6;i++) {
						rect.top = 20*i;
						rect.bottom = rect.top +1;
						bufDC.FillSolidRect(&rect,0x00505050);

						sprintf(buf,"-%d0",i);
						bufDC.TextOut(0, rect.top-10, buf);
						bufDC.TextOut(256-16, rect.top-10, buf);
					}
					rect.left = 128;
					rect.right = 256;
					rect.top = 120;
					rect.bottom = rect.top +1;
					bufDC.FillSolidRect(&rect,0x00505050);

					sprintf(buf,"-60");
					bufDC.TextOut(256-16, rect.top-10, buf);

					rect.top=0;
					rect.bottom=256;
					float invlog2 = 1.0/log10(2.0);
					float thebar = 440.f*2.f*256.f/Global::player().SampleRate();
					if (scope_spec_mode == 1) rect.left=thebar;
					else if (scope_spec_mode == 2) rect.left=16*sqrt(thebar);
					else if (scope_spec_mode == 3) rect.left=32*log10(1+thebar)*invlog2;
					rect.right=rect.left+1;
					bufDC.FillSolidRect(&rect,0x00606060);
					sprintf(buf,"440");
					bufDC.TextOut(rect.left, 0, buf);
					bufDC.TextOut(rect.left, 128-12, buf);

					thebar = 7000*2.f*256.f/Global::player().SampleRate();
					if (scope_spec_mode == 1) rect.left=thebar;
					else if (scope_spec_mode == 2) rect.left=16*sqrt(thebar);
					else if (scope_spec_mode == 3) rect.left=32*log10(1+thebar)*invlog2;
					rect.right=rect.left+1;
					bufDC.FillSolidRect(&rect,0x00606060);
					sprintf(buf,"7K");
					bufDC.TextOut(rect.left, 0, buf);
					bufDC.TextOut(rect.left, 128-12, buf);

					thebar = 16000*2.f*256.f/Global::player().SampleRate();
					if (scope_spec_mode == 1) rect.left=thebar;
					else if (scope_spec_mode == 2) rect.left=16*sqrt(thebar);
					else if (scope_spec_mode == 3) rect.left=32*log10(1+thebar)*invlog2;
					rect.right=rect.left+1;
					bufDC.FillSolidRect(&rect,0x00606060);
					sprintf(buf,"16K");
					bufDC.TextOut(rect.left, 0, buf);
					bufDC.TextOut(rect.left, 128-12, buf);


					bufDC.SelectObject(oldFont);
				}
				//Hack to fix bar positioning.
				m_sliderMode.SetRangeMin(1);
				m_sliderMode.SetPos(1);
				m_sliderMode.SetRangeMax(3);
				m_sliderMode.SetPos(scope_spec_mode);
				m_sliderRate.SetRange(10,100);
				m_sliderRate.SetPos(scope_spec_rate);
				sprintf(buf,"Spectrum Analyzer");
				srcMachine._pScopeBufferL = pSamplesL;
				srcMachine._pScopeBufferR = pSamplesR;
				SetTimer(ID_TIMER_WIRE,scope_osc_rate,0);
				break;
			case 3:
				// phase
				KillTimer(ID_TIMER_WIRE);
				{
					CPen linepen(PS_SOLID, 8, 0x00303030);

					CPen *oldpen = bufDC.SelectObject(&linepen);

					bufDC.MoveTo(32,32);
					bufDC.LineTo(128,128);
					bufDC.LineTo(128,0);
					bufDC.MoveTo(128,128);
					bufDC.LineTo(256-32,32);
					bufDC.Arc(0,0,256,256,256,128,0,128);
					bufDC.Arc(96,96,256-96,256-96,256-96,128,96,128);

					bufDC.Arc(48,48,256-48,256-48,256-48,128,48,128);

					linepen.DeleteObject();
					linepen.CreatePen(PS_SOLID, 4, 0x00404040);
					bufDC.SelectObject(&linepen);
					bufDC.MoveTo(32,32);
					bufDC.LineTo(128,128);
					bufDC.LineTo(128,0);
					bufDC.MoveTo(128,128);
					bufDC.LineTo(256-32,32);
					linepen.DeleteObject();

					bufDC.SelectObject(oldpen);
				}
				linepenbL.CreatePen(PS_SOLID, 5, 0x705050);
				linepenbR.CreatePen(PS_SOLID, 5, 0x507050);
				linepenL.CreatePen(PS_SOLID, 3, 0xc08080);
				linepenR.CreatePen(PS_SOLID, 3, 0x80c080);

				srcMachine._pScopeBufferL = pSamplesL;
				srcMachine._pScopeBufferR = pSamplesR;
				sprintf(buf,"Stereo Phase");
				o_mvc = o_mvpc = o_mvl = o_mvdl = o_mvpl = o_mvdpl = o_mvr = o_mvdr = o_mvpr = o_mvdpr = 0.0f;
				m_sliderMode.ShowWindow(SW_HIDE);
				m_sliderRate.SetRange(10,100);
				m_sliderRate.SetPos(scope_phase_rate);
				SetTimer(ID_TIMER_WIRE,scope_phase_rate,0);
				break;
			default:
				sprintf(buf,"Scope Mode");
				break;
			}
			m_mode.SetWindowText(buf);
			hold = false;
			pos = 1;

			bufDC.SelectObject(oldbmp);
			bufDC.DeleteDC();

		}

		inline int CWireDlg::GetY(float f, float mult)
		{
			f=64.f-(f*64.f*mult);
			if (f < 1.f) 
			{
				clip = TRUE;
				return 1;
			}
			else if (f > 127.f) 
			{
				clip = TRUE;
				return 127;
			}
			return int(f);
		}
}}
