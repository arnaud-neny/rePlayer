// Waveform.h
// druttis@darkface.pp.se

#pragma once
#include "../dsp/DspMath.h"
#include "blwtbl.h"

namespace psycle::plugins::druttis {

/// Waveform
class Waveform {
	private:
		WAVEFORM m_wave;
	public:
		//////////////////////////////////////////////////////////////////////
		//				Constructor
		Waveform()
		{
			m_wave.index = WF_INIT_STRUCT;
			Get(WF_INIT_STRUCT);
		}
		//////////////////////////////////////////////////////////////////////
		//				Destructor
		~Waveform()
		{
			Get(WF_INIT_STRUCT);
		}
		//////////////////////////////////////////////////////////////////////
		//				GetWaveform
		bool Get(wf_type index)
		{
			return GetWaveform(index, &m_wave);
		}

		inline WAVEFORM *Get() { return &m_wave; }

		/// Returns a linear interpolated sample by phase (NO BANDLIMIT)
		inline float GetSample(float phase) {
			register int offset = rint<int>(phase);
			const float frac = phase - (float) offset;
			const float out = m_wave.pdata[offset & WAVEMASK];
			return out + (m_wave.pdata[++offset & WAVEMASK] - out) * frac;
		}

		/// Returns a linear interpolated sample by phase and index
		inline float GetSample(float phase, int frequency) {
			const float *pdata = &m_wave.pdata[m_wave.preverse[frequency] << WAVESIBI];
			register int offset = rint<int>(phase);
			const float frac = phase - (float) offset;
			const float out = pdata[offset & WAVEMASK];
			return out + (pdata[++offset & WAVEMASK] - out) * frac;
		}

		/// Returns a linear interpolated sample by phase and incr
		inline float GetSample(float phase, float freq_incr) {
			return GetSample(phase, rint<int>(freq_incr * incr2freq) & 0xffff);
		}
};
}