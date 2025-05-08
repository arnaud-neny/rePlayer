// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "instpreview.h"
#include "instrument.h"
#include <universalis/stdlib/cstdint.hpp>

namespace psycle { namespace core {

void InstPreview::Work(float * pInSamplesL, float * pInSamplesR, int numSamples) {
	if(!m_pInstrument) return;

	float * pSamplesL = pInSamplesL;
	float * pSamplesR = pInSamplesR;
	--pSamplesL;
	--pSamplesR;
	
	int16_t * wl = m_pInstrument->waveDataL;
	int16_t * wr = m_pInstrument->waveDataR;
	float ld = 0;
	float rd = 0;
	
	do {
		ld = (*(wl + m_pos)) * m_vol;
		if(m_pInstrument->waveStereo) rd = (*(wr + m_pos)) * m_vol;
		else rd = ld;
			
		*(++pSamplesL) += ld;
		*(++pSamplesR) += rd;
			
		if(++m_pos>=m_pInstrument->waveLength) {
			Stop();
			return;
		}
		if(m_bLoop && m_pInstrument->waveLoopEnd == m_pos) {
			m_pos=m_pInstrument->waveLoopStart;
		}
	} while(--numSamples);
}

void InstPreview::Play(unsigned long startPos/* = 0 */) {
	if(!m_pInstrument) return;

	m_bLoop = m_pInstrument->waveLoopType;
	if(startPos < m_pInstrument->waveLength) {
		m_bPlaying = true;
		m_pos = startPos;
	}
}

void InstPreview::Stop() {
	m_bPlaying = false;
}

void InstPreview::Release() {
	m_bLoop = false;
}

}}
