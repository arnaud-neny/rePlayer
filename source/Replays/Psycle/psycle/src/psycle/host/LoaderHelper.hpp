///\file
///\brief interface file for psycle::host::CBaseParamView.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include <psycle/host/XMInstrument.hpp>

namespace psycle { namespace host {

class Song;

class LoaderHelper {
	public:
		LoaderHelper(Song* pSong, bool _ispreview, int _instIdx, int _sampIdx);
		XMInstrument& GetNextInstrument(int &outidx);
		XMInstrument::WaveData<>& GetNextSample(int &outidx);
		XMInstrument::WaveData<>& GetPreviousSample(int idx);
		bool IsPreview() { return ispreview; }
	protected:
		XMInstrument prewInst;
		Song* song;
		bool ispreview;
		int instIdx;
		int sampIdx;
};

}}
