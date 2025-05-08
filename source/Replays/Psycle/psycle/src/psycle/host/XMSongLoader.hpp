#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "XMFile.hpp"
#include "FileIO.hpp"
#include "XMInstrument.hpp"
#include <psycle/host/LoaderHelper.hpp>

namespace psycle { namespace host {
	class Song;
	class XMSampler;
	class CProgressDialog;

	class XMSongLoader : public OldPsyFile
	{
	public:
		XMSongLoader(void);
		virtual ~XMSongLoader(void);
		/// RIFF 
		virtual bool Load(Song &song,CProgressDialog& progress, bool fullopen=true);
		int LoadInstrumentFromFile(LoaderHelper& loadhelp);
	private:
		bool IsValid();

		size_t LoadPatterns(Song& song, std::map<int,int>& xmtovirtual);
		size_t LoadSinglePattern(Song& song, size_t start, int patIdx, int iTracks, std::map<int,int>& xmtovirtual);	
		bool LoadInstruments(Song& song, size_t iInstrStart, std::map<int,int>& xmtovirtual);
		size_t LoadInstrument(Song& song, XMInstrument& instr, size_t iStart, int idx,int& curSample);
		size_t LoadSampleHeader(XMInstrument::WaveData<>& sample, size_t iStart, int InstrIdx, int SampleIdx);
		size_t LoadSampleData(XMInstrument::WaveData<>& sample, size_t iStart, int InstrIdx, int SampleIdx);
		BOOL WritePatternEntry(Song& song,int patIdx,int row, int col, PatternEntry & e);
		void SetEnvelopes(XMInstrument& inst,const XMSAMPLEHEADER & sampleHeader);		
		char * AllocReadStr(int size, signed int start=-1);

		// inlines
		inline char ReadInt1()
		{	
			char i;
			return Read(&i,1)?i:0;
		}

		inline short ReadInt2()
		{
			short i;
			return Read(&i,2)?i:0;
		}

		inline int ReadInt4()
		{
			int i;
			return Read(&i,4)?i:0;
		}
		inline char ReadInt1(const signed int start)
		{	
			char i;
			if(start>=0) Seek(start);
			return Read(&i,1)?i:0;
		}

		inline short ReadInt2(const signed int start)
		{
			short i;
			if(start>=0) Seek(start);
			return Read(&i,2)?i:0;
		}

		inline int ReadInt4(const signed int start)
		{
			int i;
			if(start>=0) Seek(start);
			return Read(&i,4)?i:0;
		}
		int m_iInstrCnt;
		int smpLen[256];
		char smpFlags[256];
		unsigned char highOffset[32];
		unsigned char memPortaUp[32];
		unsigned char memPortaDown[32];
		unsigned char memPortaNote[32];
		unsigned char memPortaPos[32];

		short m_iTempoTicks;
		short m_iTempoBPM;
		short m_extracolumn;
		short m_maxextracolumn;
		XMFILEHEADER m_Header;
		XMSampler* m_pSampler;
	};



	struct MODHEADER
	{
		unsigned char songlength;
		unsigned char unused;
		unsigned char order[128];
		unsigned char pID[4];
	};
	struct MODSAMPLEHEADER
	{
		char sampleName[22];
		unsigned short sampleLength;
		unsigned char finetune;
		unsigned char volume;
		unsigned short loopStart;
		unsigned short loopLength;
	};

	class MODSongLoader : public OldPsyFile
	{
	public:
		MODSongLoader();
		virtual ~MODSongLoader();
		/// RIFF 
		virtual bool Load(Song &song,CProgressDialog& progress, bool fullopen=true);
		bool IsValid();

	private:
		void LoadPatterns(Song & song, std::map<int,int>& modtovirtual);
		void LoadSinglePattern(Song & song, int patIdx, int iTracks, std::map<int,int>& modtovirtual);	
		unsigned char ConvertPeriodtoNote(unsigned short period);
		void LoadInstrument(Song & song, int idx);
		void LoadSampleHeader(XMInstrument::WaveData<>& _wave, int InstrIdx);
		void LoadSampleData(XMInstrument::WaveData<>& _wave, int InstrIdx);
		BOOL WritePatternEntry(Song& song,int patIdx,int row, int col, PatternEntry & e);
		char * AllocReadStr(int32_t size, signed int start=-1);

		// inlines
		inline unsigned char ReadUInt1()
		{	
			uint8_t i(0);
			return Read(&i,1)?i:0;
		}

		inline unsigned short ReadUInt2()
		{
			uint16_t i(0);
			return Read(&i,2)?i:0;
		}

		inline unsigned int ReadUInt4()
		{
			uint32_t i(0);
			return Read(&i,4)?i:0;
		}
		inline unsigned char ReadUInt1(const int32_t start)
		{	
			uint8_t i(0);
			if(start>=0) Seek(start);
			return Read(&i,1)?i:0;
		}

		inline unsigned short ReadUInt2(const int32_t start)
		{
			uint16_t i(0);
			if(start>=0) Seek(start);
			return Read(&i,2)?i:0;
		}

		inline unsigned int ReadUInt4(const int32_t start)
		{
			uint32_t i(0);
			if(start>=0) Seek(start);
			return Read(&i,4)?i:0;
		}
		static const short BIGMODPERIODTABLE[37*8];
		unsigned short smpLen[32];
		bool speedpatch;
		MODHEADER m_Header;
		MODSAMPLEHEADER m_Samples[32];
		XMSampler* m_pSampler;
	};
}}
