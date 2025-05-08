#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

#include <psycle/host/XMInstrument.hpp>
#include <psycle/helpers/resampler.hpp>

namespace psycle { namespace host {

class CWaveScopeCtrl : public CStatic
{
public:
	CWaveScopeCtrl();
	virtual ~CWaveScopeCtrl();

	void SetWave(XMInstrument::WaveData<> *pWave) { m_pWave = pWave; }
	XMInstrument::WaveData<>& rWave() { return *m_pWave; }

	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

protected:
	XMInstrument::WaveData<>* m_pWave;
	helpers::dsp::cubic_resampler resampler;
	CPen cpen_lo;
	CPen cpen_med;
	CPen cpen_hi;
	CPen cpen_sus;

};
}}