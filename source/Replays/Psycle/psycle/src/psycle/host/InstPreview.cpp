//\file
//\brief implementation file for psycle::host::InstPreview.

#include <psycle/host/detail/project.private.hpp>
#include "InstPreview.hpp"
#include "Instrument.hpp"
#include "Player.hpp"
namespace psycle {
	namespace host {

		void InstPreview::Work(float *pInSamplesL, float *pInSamplesR, int numSamples)
		{
			if(m_pwave == NULL || m_pwave->WaveLength() == 0) return;

			float *pSamplesL = pInSamplesL;
			float *pSamplesR = pInSamplesR;
			--pSamplesL;
			--pSamplesR;
			
			float left_output = 0.0f;
			float right_output = 0.0f;
			//TODO: A way for the user to use the different methods
			//Assuming Linear Panning.
			float lvol=0.5f;
			float rvol=0.5f;
			if (m_pwave->PanEnabled()) {
				if(m_pwave->IsSurround()){
					rvol *= -1.0f;
				}
				else {
					rvol = m_pwave->PanFactor();
					lvol = (1.0f - rvol);
				}
			}
			lvol *= m_vol;
			rvol *= m_vol;
			
			while(numSamples) {
				XMSampler::WaveDataController<>::WorkFunction pWork;
				int nextsamples = std::min(controller.PreWork(numSamples, &pWork, false), numSamples);
				numSamples-=nextsamples;
	#ifndef NDEBUG
				if (numSamples > 256 || numSamples < 0) {
					TRACE("27: numSamples invalid bug triggered!\n");
				}
	#endif
				while (nextsamples)
				{
					pWork(controller,&left_output, &right_output);

					*(++pSamplesL)+=left_output*lvol;
					if(m_pwave->IsWaveStereo()) {
						*(++pSamplesR)+=right_output*rvol;
					}
					else {
						*(++pSamplesR)+=left_output*rvol;
					}
					nextsamples--;
				}
				controller.PostWork();
				if (!controller.Playing()) {
					return;
				}
			}
		}

		void InstPreview::Play(int note/*=notecommands::middleC*/, unsigned long startPos/*=0*/)
		{
			if(m_pwave == NULL || m_pwave->WaveLength() == 0) return;

			m_bLoop = m_pwave->WaveSusLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT;
			if(startPos < m_pwave->WaveLength())
			{
				controller.Init(m_pwave,0,resampler);
				double speed=NoteToSpeed(note);
				controller.Speed(resampler,speed);
				controller.Position(startPos);
				controller.Playing(true);
			}
		}

		void InstPreview::Stop()
		{
			controller.Playing(false);
			if (&m_prevwave == m_pwave) {
				m_prevwave.DeleteWaveData();
			}
		}
		
		void InstPreview::Release()
		{
			m_bLoop = false;
			controller.NoteOff();
		}
		double InstPreview::NoteToSpeed(int note)
		{
			// Linear Frequency (See XMSampler::Voice)
			int period = 9216 - ((double)(note + m_pwave->WaveTune()) * 64.0)
					- ((double)(m_pwave->WaveFineTune()) * 0.64);
			return pow(2.0,
						((5376 - period) /768.0))
					* m_pwave->WaveSampleRate() / (double)Global::player().SampleRate();
		}

	}   // namespace
}   // namespace

