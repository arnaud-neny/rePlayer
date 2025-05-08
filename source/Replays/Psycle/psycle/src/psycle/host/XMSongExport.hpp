#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "XMFile.hpp"
#include "FileIO.hpp"
#include <map>

namespace psycle { namespace host {
	class Song;

	class XMSongExport : public OldPsyFile
	{
	public:
		XMSongExport();
		virtual ~XMSongExport();

		virtual void exportsong(const Song& song);
	private:
		void writeSongHeader(const Song &song);
		void SavePatterns(const Song & song);
		void SaveSinglePattern(const Song & song, const int patIdx);
		void GetCommand(const Song& song, int i, const PatternEntry *entry, unsigned char &vol, unsigned char &type, unsigned char &param);
		
		void SaveInstruments(const Song & song);
		void SaveEmptyInstrument(const std::string& name);
		void SaveSamplerInstrument(const Song& song, int instIdx);
		void SaveSampulseInstrument(const Song& song, int instIdx);
		void SaveSampleHeader(const Song & song, const int instrIdx);
		void SaveSampleData(const Song & song, const int instrIdx);
		void SetSampulseEnvelopes(const Song & song, int instIdx, XMSAMPLEHEADER & sampleHeader);
		void SetSamplerEnvelopes(const Song & song, int instIdx, XMSAMPLEHEADER & sampleHeader);
		
		XMFILEHEADER m_Header;
		int macInstruments;
		int xmInstruments;
		int correctionIndex;
		bool isSampler[MAX_BUSES];
		bool isSampulse[MAX_BUSES];
		bool isBlitzorVst[256];
		int lastInstr[32];
		const PatternEntry* extraEntry[32];
		int addTicks;
	};
}}
