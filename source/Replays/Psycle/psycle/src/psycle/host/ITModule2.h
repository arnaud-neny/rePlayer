#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "FileIO.hpp"
#include "XMInstrument.hpp"
#include <psycle/host/LoaderHelper.hpp>

namespace psycle { namespace host {
		class Song;
		class XMSampler;
		class CProgressDialog;

		class ITModule2 : public OldPsyFile
		{
		private:
			Song* _pSong;

		public:
			ITModule2();
			virtual ~ITModule2() throw();

			// Extra class for Reading of IT compressed samples.
			class BitsBlock
			{
			public:
				BitsBlock() : pdata(0), rpos(0), rend(0), rembits(0) {}
				~BitsBlock() throw() { delete[] pdata; }
				bool ReadBlock(OldPsyFile* pFile);
				uint32_t ReadBits(unsigned char bitwidth);
			private:
				/// pointer to data
				unsigned char * pdata;
				/// read position
				unsigned char * rpos;
				/// read end
				unsigned char * rend;
				/// remaining bits in current pos
				unsigned char rembits;
			};

			//////////////////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////////////////
			// IT Fileformat

		public:
			struct CMD
			{
				enum
				{
					SET_SPEED            =  1,
					JUMP_TO_ORDER        =  2,
					BREAK_TO_ROW         =  3,
					VOLUME_SLIDE         =  4,
					PORTAMENTO_DOWN      =  5,
					PORTAMENTO_UP        =  6,
					TONE_PORTAMENTO      =  7,
					VIBRATO              =  8,
					TREMOR               =  9,
					ARPEGGIO             = 10,
					VOLSLIDE_VIBRATO     = 11, ///< Dual command: H00 and Dxy 
					VOLSLIDE_TONEPORTA   = 12, ///< Dual command: G00 and Dxy
					SET_CHANNEL_VOLUME   = 13,
					CHANNEL_VOLUME_SLIDE = 14,
					SET_SAMPLE_OFFSET    = 15,
					PANNING_SLIDE        = 16,
					RETRIGGER_NOTE       = 17,
					TREMOLO              = 18,
					S                    = 19,
					SET_SONG_TEMPO       = 20, ///< T0x Slide tempo down . T1x slide tempo up
					FINE_VIBRATO         = 21,
					SET_GLOBAL_VOLUME    = 22,
					GLOBAL_VOLUME_SLIDE  = 23,
					SET_PANNING          = 24,
					PANBRELLO            = 25,
					MIDI_MACRO           = 26  ///< see MIDI.TXT ([bohan] uhu? what file?)
				};
			};

			struct CMD_S
			{
				enum
				{
					S_SET_FILTER             = 0x00, ///< Greyed out in IT...
					S_SET_GLISSANDO_CONTROL  = 0x10, ///< Greyed out in IT...
					S_FINETUNE               = 0x20, ///< Greyed out in IT...
					S_SET_VIBRATO_WAVEFORM   = 0x30, ///< Check XMInstrument::WaveData::WaveForms! IT is sine, square, sawdown and random
					S_SET_TREMOLO_WAVEFORM   = 0x40, ///< Check XMInstrument::WaveData::WaveForms! IT is sine, square, sawdown and random
					S_SET_PANBRELLO_WAVEFORM = 0x50, ///< Check XMInstrument::WaveData::WaveForms! IT is sine, square, sawdown and random
					S_FINE_PATTERN_DELAY     = 0x60, ///< causes a "pause" of x ticks ( i.e. the current row becomes x ticks longer)
					S7                       = 0x70,
					S_SET_PAN                = 0x80,
					S9                       = 0x90,
					S_SET_HIGH_OFFSET        = 0xA0,
					S_PATTERN_LOOP           = 0xB0,
					S_DELAYED_NOTE_CUT       = 0xC0,
					S_NOTE_DELAY             = 0xD0,
					S_PATTERN_DELAY          = 0xE0, ///< causes a "pause" of x rows ( i.e. the current row becomes x rows longer)
					S_SET_MIDI_MACRO         = 0xF0
				};
			};

			struct CMD_S7
			{
				enum
				{
					S7_BACKGROUNDNOTECUT  =  0,
					S7_BACKGROUNDNOTEOFF  =  1,
					S7_BACKGROUNDNOTEFADE =  2,
					S7_SETNOTECUT         =  3,
					S7_SETNOTECONTINUE    =  4,
					S7_SETNOTEOFF         =  5,
					S7_SETNOTEFADE        =  6,
					S7_VOLENVOFF          =  7,
					S7_VOLENVON           =  8,
					S7_PANENVOFF          =  9,
					S7_PANENVON           = 10,
					S7_PITCHENVON         = 11,
					S7_PITCHENVOFF        = 12
				};
			};

			struct CMD_S9
			{
				enum
				{
					S9_SURROUND_OFF	=		0,
					S9_SURROUND_ON	=		1,
					S9_REVERB_OFF	=		8,
					S9_REVERB_FORCE	=		9,
					S9_STANDARD_SURROUND = 10,
					S9_QUAD_SURROUND     = 11, ///< Select quad surround mode: this allows you to pan in the rear channels, especially useful for 4-speakers playback. Note that S9A and S9B do not activate the surround for the current channel, it is a global setting that will affect the behavior of the surround for all channels. You can enable or disable the surround for individual channels by using the S90 and S91 effects. In quad surround mode, the channel surround will stay active until explicitely disabled by a S90 effect
					S9_GLOBAL_FILTER     = 12, ///< Select global filter mode (IT compatibility). This is the default, when resonant filters are enabled with a Zxx effect, they will stay active until explicitely disabled by setting the cutoff frequency to the maximum (Z7F), and the resonance to the minimum (Z80).
					S9_LOCAL_FILTER      = 13, ///< Select local filter mode (MPT beta compatibility): when this mode is selected, the resonant filter will only affect the current note. It will be deactivated when a new note is being played.
					S9_PLAY_FORWARD      = 14, ///< Play forward. You may use this to temporarily force the direction of a bidirectional loop to go forward.
					S9_PLAY_BACKWARD     = 15  ///< Play backward. The current instrument will be played backwards, or it will temporarily set the direction of a loop to go backward. 
				};
			};

			struct ITVOLCMD
			{
				enum
				{
					VOL_VOL_POSITION		=   0x0,
					VOL_FINE_VOLSLIDE_UP	=	0x41,
					VOL_FINE_VOL_SLIDE_DOWN	=	0x4B,
					VOL_VOL_SLIDE_UP		=	0x55,
					VOL_VOL_SLIDE_DOWN		=	0x5F,
					VOL_PITCH_SLIDE_DOWN	=	0x69,
					VOL_PITCH_SLIDE_UP		=	0x73,
					VOL_PAN_POSITION		=	0x80,
					VOL_PORTANOTE			=	0xC1,
					VOL_VIBRATO				=	0xCB 
				};
			};

			/*
			The following effects 'memorise' their previous values:
			(D/K/L), (E/F/G), (HU), I, J, N, O, S, T, W

			Commands H and U are linked even more closely.
			If you use H00 or U00, then the previous vibrato, no matter
			whether it was set with Hxx or Uxx will be used.


			Volume column effects are selected by pressing A-H in the first
			column of the effects.

			Ax = fine volume slide up by x
			Bx = fine volume slide down by x
			Cx = volume slide up by x
			Dx = volume slide down by x
			Ex = pitch slide down by x
			Fx = pitch slide up by x
			Gx = portamento to note with speed x
			Hx = vibrato with depth x

			// Volume ranges from 0->64
			// Panning ranges from 0->64, mapped onto 128->192
			// Prepare for the following also:
			//  65->74 = Fine volume up
			//  75->84 = Fine volume down
			//  85->94 = Volume slide up
			//  95->104 = Volume slide down
			//  105->114 = Pitch Slide down
			//  115->124 = Pitch Slide up
			//  193->202 = Portamento to
			//  203->212 = Vibrato
			In all cases, if x is 0, then the effect memory is used (as
			explained in (4)

			The memory for Ax/Bx/Cx/Dx are shared, as is the memory for
			Ex/Fx.
			*/


			/*
			//Some effects needed for XM compatibility
			int const static IT_XM_PORTAMENTO_DOWN    = 27;
			int const static IT_XM_PORTAMENTO_UP      = 28;
			int const static IT_XM_FINE_VOLSLIDE_DOWN = 29;
			int const static IT_XM_FINE_VOLSLIDE_UP   = 30;
			int const static IT_XM_RETRIGGER_NOTE     = 31;
			*/

			// little-endian values.
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			static const uint32_t IMPM_ID=0x4D504D49;
			static const uint32_t IMPI_ID=0x49504D49;
			static const uint32_t IMPS_ID=0x53504D49;
#else
			static const uint32_t IMPM_ID=0x494D504D;
			static const uint32_t IMPI_ID=0x494D5049;
			static const uint32_t IMPS_ID=0x494D5053;
#endif

#pragma pack(push, 1)

			struct itHeader
			{
				uint32_t tag;
				char songName[26];
				uint16_t pHiligt,ordNum,insNum,sampNum,patNum,trackerV,ffv,flags,special;
				uint8_t gVol,mVol,iSpeed,iTempo,panSep,PWD;
				uint16_t msgLen;
				uint32_t msgOffset,reserved;
				uint8_t chanPan[64],chanVol[64];
			};
			struct Flags
			{
				enum
				{
					STEREO       = 0x1,
					VOLOPT       = 0x2,
					USEINSTR     = 0x4,
					LINEARSLIDES = 0x8,
					OLDEFFECTS   = 0x10,
					LINKGXXMEM   = 0x20,
					USEMIDIPITCH = 0x40,
					REQUESTMIDI  = 0x80
				};
			};
			struct SpecialFlags
			{
				enum
				{
					HASMESSAGE   = 0x01,
					MIDIEMBEDDED = 0x08
					// Other values are unused.
				};
			};
			struct ChanFlags
			{
				enum
				{
					IS_SURROUND = 0x64,
					IS_DISABLED = 0x80
				};
			};

			struct EmbeddedMIDIData
			{
				char Start[32];
				char Stop[32];
				char Tick[32];
				char NoteOn[32];
				char NoteOff[32];
				char Volume[32];
				char Pan[32];
				char BankChange[32];
				char ProgramChange[32];
				char SFx[16][32];
				char Zxx[128][32];
			};

			struct ITNotePair { uint8_t first; uint8_t second; };
			struct ITNodePair1x { uint8_t first; uint8_t second; };
			struct ITNodePair { int8_t first; uint8_t secondlo; uint8_t secondhi; };

			struct itInsHeader1x
			{
				uint32_t tag;
				char fileName[13];
				uint8_t flg,loopS,loopE,sustainS,sustainE;
				uint8_t unused1[2];
				uint16_t fadeout;
				uint8_t NNA,DNC;
				uint16_t trackerV;
				uint8_t noS;
				uint8_t unused2;
				char sName[26];
				uint8_t unused[6];
				ITNotePair notes[120];
				/// value of the volume for each tick. (Seems it was probably an internal IT thing to represent the envelope)
				uint8_t volEnv[200];
				/// Each of the defined nodes (tick index, value). value ranges from 0 to 64 (decimal). 0xFF is end of envelope.
				ITNodePair1x nodepair[25];
			};

			struct ITEnvStruct
			{
				uint8_t flg,numP,loopS,loopE,sustainS,sustainE;
				ITNodePair nodes[25];
				uint8_t unused;
			};

			struct itInsHeader2x
			{
				uint32_t tag;
				char fileName[13];
				uint8_t NNA,DCT,DCA;
				uint16_t fadeout;
				uint8_t pPanSep,pPanCenter,gVol,defPan,randVol,randPan;
				uint16_t trackerV;
				uint8_t noS,unused;
				char sName[26];
				uint8_t inFC,inFR,mChn,mPrg;
				uint16_t mBnk;
				ITNotePair notes[120];
				ITEnvStruct volEnv;
				ITEnvStruct panEnv;
				ITEnvStruct pitchEnv;
			};

			struct EnvFlags // Same flags used for itInsHeader1x
			{
				enum
				{	
					USE_ENVELOPE = 0x01,
					USE_LOOP     = 0x02,
					USE_SUSTAIN  = 0x04,
					ENABLE_CARRY = 0x08,
					ISFILTER     = 0x80, // Only meaningful in a pitch envelope
				};
			};
			struct DCAction {
				enum Type {
				STOP=0x0,
				NOTEOFF,
				FADEOUT
				};
			};

			struct itSampleHeader
			{
				uint32_t tag;
				char fileName[13];
				uint8_t gVol,flg,vol;
				char sName[26];
				uint8_t cvt,dfp;
				uint32_t length,loopB,loopE,c5Speed,sustainB,sustainE,smpData;
				uint8_t vibS,vibD,vibR,vibT;
			};
#pragma pack(pop)
			struct SampleFlags
			{
				enum
				{
					HAS_SAMPLE       = 0x01,
					IS16BIT          = 0x02,
					ISSTEREO         = 0x04,
					ISCOMPRESSED     = 0x08,
					USELOOP          = 0x10,
					USESUSTAIN       = 0x20,
					ISLOOPPINPONG    = 0x40,
					ISSUSTAINPINPONG = 0x80
				};
			};

			struct SampleConvert
			{
				enum
				{
					IS_SIGNED    = 0x01, ///< Only this one is used by Impulse Tracker. The others were for its sample importer.
					IS_MOTOROLA = 0x02,
					IS_DELTA	= 0x04,
					IS_BYTEDELTA = 0x08,
					IS_12bit	= 0x10,
					IS_PROMPT    = 0x20
				};
			};
		public:
			virtual bool Load(Song &song,CProgressDialog& progress, bool fullopen=true);
			bool LoadITModule(Song& song);
			bool LoadOldITInst(const itInsHeader1x& curH,XMInstrument &xins);
			bool LoadITInst(const itInsHeader2x& curH,XMInstrument &xins);
			int LoadInstrumentFromFile(LoaderHelper& loadhelp);
			bool LoadITSample(XMInstrument::WaveData<>& _wave);
			bool LoadITSampleData(XMInstrument::WaveData<>& _wave,uint32_t iLen,bool bstereo,bool b16Bit, unsigned char convert);
			bool LoadITCompressedData(int16_t* pWavedata,uint32_t iLen,bool b16Bit,unsigned char convert);
			bool LoadITPattern(int patIdx,int &numchans, std::map<int,int>& ittovirtual);
			void ParseEffect(PatternEntry&pent, int patIdx, int row, int command,int param,int channel);
		private:
			uint8_t highOffset[64];
			EmbeddedMIDIData* embeddedData;
			itHeader itFileH;
			short m_extracolumn;
			short m_maxextracolumn;


			//////////////////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////////////////
			// S3M Fileformat 


		public:
			// little-endian
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			static const uint32_t SCRM_ID = 0x4D524353;
			static const uint32_t SCRS_ID = 0x53524353;
			static const uint32_t SCRI_ID = 0x49524353;
#else
			static const uint32_t SCRM_ID = 0x5343524D;
			static const uint32_t SCRS_ID = 0x53435253;
			static const uint32_t SCRI_ID = 0x53435249;
#endif

#pragma pack(push, 1)
			struct s3mHeader
			{
				char songName[28];
				uint8_t end,type;
				uint8_t unused1[2];
				uint16_t ordNum,insNum,patNum,flags,trackerV,trackerInf;
				uint32_t tag; // SCRM
				uint8_t gVol,iSpeed,iTempo,mVol,uClick,defPan;
				uint8_t unused2[8];
				uint16_t pSpecial;
				uint8_t chanSet[32];
			};

			struct S3MFlags
			{
				enum
				{
					ST2VIBRATO  = 0x01, ///< Unused in ST3
					ST2TEMPO    = 0x02, ///< Unused in ST3
					AMIGASLIDES = 0x04, ///< Unused in ST3
					VOLOPT      = 0x08,
					AMIGALIMITS = 0x10,
					ENABLE_S0   = 0x20, ///< Unused in ST3
					SLIDEALWAYS = 0x40,
					CUSTOMDATA  = 0x80  ///< Unused in ST3
				};
			};

			struct S3MChanType
			{
				enum
				{
					ISRIGHTCHAN  = 0x08, ///< Else, is Left Chan.
					ISADLIBCHAN  = 0x10,
					//HASCUSTOMPOS is applied over channelpansettings, not over channel settings.
					HASCUSTOMPOS = 0x20,
					ISDISABLED   = 0x80,
					ISUNUSED     = 0xFF
				};
			};
			/*Generic header. casted to one of the other two depending on "type" (or "tag") value*/
			struct s3mInstHeader
			{
				uint8_t type;
				char fileName[12];
				uint8_t data[35];
				char sName[28];
				uint32_t tag;
			};

			struct s3mSampleHeader
			{
				uint8_t type;
				char filename[12];
				uint8_t hiMemSeg;
				uint16_t lomemSeg;
				uint32_t length,loopb,loope;
				uint8_t vol;
				uint8_t unused;
				uint8_t packed,flags; //[P]ack   0=unpacked, 1=DP30ADPCM packing (not used by ST3.01)
				uint32_t c2speed;
				uint32_t unused2,internal1,internal2;
				char sName[28];
				/// SCRS
				uint32_t tag;
			};

			struct S3MSampleFlags
			{
				enum
				{
					LOOP    = 0x01,
					STEREO  = 0x02,
					IS16BIT = 0x04
				};
			};

			struct s3madlibheader
			{
				uint8_t type;
				char filename[12];
				uint8_t unused[3];
				uint8_t D00,D01,D02,D03,D04,D05,D06,D07,D08,D09,D0A,D0B,vol;
				uint8_t unused2[3];
				uint32_t c2speed;
				uint8_t unused3[12];
				char sName[28];
				/// SCRI
				uint32_t tag;
			};
/*
OFFSET              Count TYPE   Description
0000h                   1 byte   Instrument type
									2 - melodic instrument
									3 - bass drum
									4 - snare drum
									5 - tom tom
									6 - cymbal
									7 - hihat
0001h                  12 char   DOS file name
000Dh                   3 byte   reserved
0010h                   1 byte   Modulator description (bitmapped)
									0-3 - frequency multiplier
									4 - scale envelope
									5 - sustain
									6 - pitch vibrato
									7 - volume vibrato
0011h                   1 byte   Carrier description (same as modulator)
0012h                   1 byte   Modulator miscellaneous (bitmapped)
									0-5 - 63-volume
									6 - MSB of levelscale
									7 - LSB of levelscale
0013h                   1 byte   Carrier description (same as modulator)
0014h                   1 byte   Modulator attack / decay byte (bitmapped)
									0-3 - Decay
									4-7 - Attack
0015h                   1 byte   Carrier description (same as modulator)
0016h                   1 byte   Modulator sustain / release byte (bitmapped)
									0-3 - Release count
									4-7 - 15-Sustain
0017h                   1 byte   Carrier description (same as modulator)
0018h                   1 byte   Modulator wave select
0019h                   1 byte   Carrier wave select
001Ah                   1 byte   Modulator feedback byte (bitmapped)
									0 - additive synthesis on/off
									1-7 - modulation feedback
 */
#pragma pack(pop)

		public:
			bool LoadS3MModuleX(Song& song);
			//This is used to load s3i (st3 standalone samples/instruments).
			int LoadS3MFileS3I(LoaderHelper& loadhelp);
			bool LoadS3MInstX(Song& song, uint16_t iSampleIdx);
			bool LoadS3MSampleX(XMInstrument::WaveData<>& _wave,s3mSampleHeader *currHeader);
			bool LoadS3MSampleDataX(XMInstrument::WaveData<>& _wave,uint32_t iLen,bool bstereo,bool b16Bit,bool packed);
			bool LoadS3MPatternX(uint16_t patIdx, std::map<int,int>& s3mtovirtual);
		private:
			s3mHeader  s3mFileH;
		};
}}
