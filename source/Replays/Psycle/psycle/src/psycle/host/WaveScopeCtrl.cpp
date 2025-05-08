#include <psycle/host/detail/project.private.hpp>
#include "WaveScopeCtrl.hpp"
namespace psycle { namespace host {

// CWaveScopeCtrl
CWaveScopeCtrl::CWaveScopeCtrl()
: m_pWave(NULL)
{
	cpen_lo.CreatePen(PS_SOLID,0,0xFF0000);
	cpen_med.CreatePen(PS_SOLID,0,0xCCCCCC);
	cpen_hi.CreatePen(PS_SOLID,0,0x00FF00);
	cpen_sus.CreatePen(PS_DOT,0,0xFF0000);
	resampler.quality(helpers::dsp::resampler::quality::spline);
}
CWaveScopeCtrl::~CWaveScopeCtrl(){
	cpen_lo.DeleteObject();
	cpen_med.DeleteObject();
	cpen_hi.DeleteObject();
	cpen_sus.DeleteObject();
}

void CWaveScopeCtrl::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	if (m_pWave && lpDrawItemStruct->itemAction == ODA_DRAWENTIRE)
	{
		CDC dc;
		CRect rect;
		GetClientRect(&rect);
		dc.Attach(lpDrawItemStruct->hDC);
		dc.FillSolidRect(&rect,RGB(255,255,255));
		dc.SetBkMode(TRANSPARENT);

		if(rWave().WaveLength())
		{
			int const nWidth=rect.Width();
			int const nHeight=rect.Height();
			int const my=nHeight/2;
			int const wrHeight = (rWave().IsWaveStereo()) ? my/2 : my;
			int const wrHeight_R = my + wrHeight;
			float OffsetStep = (float) rWave().WaveLength() / nWidth;

			// Draw preliminary stuff
			CPen *oldpen= dc.SelectObject(&cpen_med);

			// Left channel 0 amplitude line
			dc.MoveTo(0,wrHeight);
			dc.LineTo(nWidth,wrHeight);

			if(rWave().IsWaveStereo())
			{
				// Right channel 0 amplitude line
				dc.MoveTo(0,wrHeight_R);
				dc.LineTo(nWidth,wrHeight_R);

				// Stereo channels separator line
				dc.SelectObject(&cpen_lo);
				dc.MoveTo(0,my);
				dc.LineTo(nWidth,my);
			}

			dc.SelectObject(&cpen_hi);

			if ( OffsetStep >1.f)
			{
				const int16_t * const pData = rWave().pWaveDataL();
				// Increasing by one can be slow on big ( 100.000+ ) samples
				int offsetInc = ( OffsetStep > 4.f) ? (OffsetStep/4) : 1;
				long offsetEnd=0;
				for(int c(0); c < nWidth; c++)
				{
					int yLow=0, yHi=0;
					long d = offsetEnd;
					offsetEnd = (long)floorf((c+1) * OffsetStep);
					for (; d < offsetEnd; d+=offsetInc)
					{
						if (yLow > pData[d]) yLow = pData[d];
						if (yHi < pData[d]) yHi = pData[d];
					}
					int const ryLow = (wrHeight * yLow) >> 15;
					int const ryHi = (wrHeight * yHi) >> 15;
					dc.MoveTo(c,(wrHeight) - ryLow);
					dc.LineTo(c,(wrHeight) - ryHi);
				}
			}
			else
			{
				ULARGE_INTEGER offsetStepRes; offsetStepRes.QuadPart = (double)OffsetStep* 4294967296.0;
				ULARGE_INTEGER posin; posin.QuadPart = 0;
				for(int c = 0; c < nWidth; c++)
				{
					int yLow=resampler.work(rWave().pWaveDataL()+posin.HighPart,posin.HighPart,posin.LowPart,rWave().WaveLength(), NULL);
					posin.QuadPart += offsetStepRes.QuadPart;

					int const ryLow = (wrHeight * yLow) >>15;
					dc.MoveTo(c,wrHeight - ryLow);
					dc.LineTo(c,wrHeight);
				}
			}

			if(rWave().IsWaveStereo())
			{
				if ( OffsetStep >1.f)
				{
					const int16_t * const pData = rWave().pWaveDataR();
					// Increasing by one can be slow on big ( 100.000+ ) samples
					int offsetInc = ( OffsetStep > 4.f) ? (OffsetStep/4) : 1;
					long offsetEnd=0;
					for(int c(0); c < nWidth; c++)
					{
						int yLow=0, yHi=0;
						long d = offsetEnd;
						offsetEnd = (long)floorf((c+1) * OffsetStep);
						for (; d < offsetEnd; d+=offsetInc)
						{
							if (yLow > pData[d]) yLow = pData[d];
							if (yHi < pData[d]) yHi = pData[d];
						}
						int const ryLow = (wrHeight * yLow) >> 15;
						int const ryHi = (wrHeight * yHi) >> 15;
						dc.MoveTo(c,wrHeight_R - ryLow);
						dc.LineTo(c,wrHeight_R - ryHi);
					}
				}
				else
				{
					ULARGE_INTEGER offsetStepRes; offsetStepRes.QuadPart = (double)OffsetStep* 4294967296.0;
					ULARGE_INTEGER posin; posin.QuadPart = 0;
					for(int c = 0; c < nWidth; c++)
					{
						int yLow=resampler.work(rWave().pWaveDataR()+posin.HighPart,posin.HighPart,posin.LowPart,rWave().WaveLength(), NULL);
						posin.QuadPart += offsetStepRes.QuadPart;

						int const ryLow = (wrHeight * yLow) >> 15;
						dc.MoveTo(c,wrHeight_R - ryLow);
						dc.LineTo(c,wrHeight_R);
					}
				}
			}
			if ( rWave().WaveLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT )
			{
				dc.SelectObject(&cpen_lo);
				int ls = (rWave().WaveLoopStart()* nWidth) /rWave().WaveLength();
				dc.MoveTo(ls,0);
				dc.LineTo(ls,nHeight);
				dc.TextOut(ls,12,"Start");
				int le = (rWave().WaveLoopEnd()* nWidth)/rWave().WaveLength();
				dc.MoveTo(le,0);
				dc.LineTo(le,nHeight);
				dc.TextOut(le-18,nHeight-24,"End");

			}
			if ( rWave().WaveSusLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT )
			{
				dc.SelectObject(&cpen_sus);
				int ls = (rWave().WaveSusLoopStart()* nWidth)/rWave().WaveLength();
				dc.MoveTo(ls,0);
				dc.LineTo(ls,nHeight);
				dc.TextOut(ls,0,"Start");
				int le = (rWave().WaveSusLoopEnd()* nWidth)/rWave().WaveLength();
				dc.MoveTo(le,0);
				dc.LineTo(le,nHeight);
				dc.TextOut(le-18,nHeight-12,"End");
			}
			dc.SelectObject(oldpen);
		}
		else
		{
			dc.TextOut(4,4,"No Wave Data");
		}
		dc.Detach();
	}
}
}}