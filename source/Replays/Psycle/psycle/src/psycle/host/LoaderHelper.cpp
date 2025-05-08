///\file
///\brief interface file for psycle::host::CBaseParamView.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "Song.hpp"
namespace psycle { namespace host {

LoaderHelper::LoaderHelper(Song* pSong, bool _ispreview, int _instIdx, int _sampIdx)
: song(pSong), ispreview(_ispreview), instIdx(_instIdx), sampIdx(_sampIdx)
{
}

XMInstrument& LoaderHelper::GetNextInstrument(int &outidx)
{
	if (ispreview) {
		outidx = PREV_WAV_INS;
		prewInst.Init();
		return prewInst;
	}

	XMInstrument insTmp;
	if (instIdx != -1 ) {
		song->xminstruments.SetInst(insTmp, instIdx);
		outidx =instIdx;
		instIdx = -1;
	}
	else {
		outidx = song->xminstruments.AddIns(insTmp);
	}
	XMInstrument& inst = song->xminstruments.get(outidx);
	inst.Init();
	song->DeleteVirtualOfInstrument(outidx,true);
	return inst;
}

XMInstrument::WaveData<>& LoaderHelper::GetNextSample(int &outidx)
{
	if (ispreview) {
		XMInstrument::WaveData<>& wave = song->wavprev.UsePreviewWave();
		wave.Init();
		outidx = PREV_WAV_INS;
		return wave;
	}

	XMInstrument::WaveData<> waveTmp;
	if (sampIdx != -1 ) {
		song->samples.SetSample(waveTmp, sampIdx);
		outidx =sampIdx;
		sampIdx = -1;
	}
	else {
		outidx = song->samples.AddSample(waveTmp);
	}
	XMInstrument::WaveData<>& wave = song->samples.get(outidx);
	wave.Init();
	return wave;
}


XMInstrument::WaveData<>& LoaderHelper::GetPreviousSample(int idx)
{
	if (ispreview) {
		return song->wavprev.UsePreviewWave();
	}
	else {
		return song->samples.get(idx);
	}
}

}}
