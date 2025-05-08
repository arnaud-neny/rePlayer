#pragma once
#include "eaiff.hpp"

/**
* @file
* 8svx audio decoder
* @author Jaikrishnan Menon
*
* supports: fibonacci delta encoding
* : exponential encoding
*
* For more information about the 8SVX format:
* http://netghost.narod.ru/gff/vendspec/iff/iff.txt
* http://sox.sourceforge.net/AudioFormats-11.html
* http://aminet.net/package/mus/misc/wavepak
* http://web.archive.org/web/20080828053800/amigan.1emu.net/reg/8SVX.txt  (same as wiki below)
* http://wiki.amigaos.net/wiki/8SVX_IFF_8-Bit_Sampled_Voice
*
* Samples can be found here:
* http://aminet.net/mods/smpl/
*/

#include <list>

namespace psycle
{
	namespace helpers
	{
		namespace SVX8Compression
		{
			typedef enum {
				COMP_NONE,
				COMP_FIB,
				COMP_EXP,
				COMP_MAX_MODES
			} comp_t;
		}
#pragma pack(push, 1)
		typedef struct
		{
			LongBE oneShotHiSamples;  //(loopstart)
			LongBE repeatHiSamples;   //(looplength). loopstart+looplength=loopend which is also equal to length
			LongBE samplesPerHiCycle; //How many samples form a cycle (Hertz).
			ShortBE samplesPerSec;   
			unsigned char numOctaves;  //number of octaves present.  Each following octave waveform is twice as long as the previous one. 
			unsigned char compression; //data compression (0=none, 1=Fibonacci-delta encoding).
			FixedPointBE volume;       // 1.0 means full volume
		} VHeader;

		typedef struct {
			ShortBE duration; /* segment duration in milliseconds, > 0 */
			FixedPointBE dest;     /* destination volume factor  1.0 means full volume. */
		} EGPoint;
#pragma pack(pop)
		/*
		** IFF Riff Header
		** ----------------
		Format is Big endian.

		char Id[4]			// "FORM"
		ULONGBE hlength	// of the data contained in the file (except Id and length)
		char type[4]		// "16SV" == 16bit . 8SVX == 8bit

		char name_Id[4]		// "NAME" (may or may not be present, and might be before or after VHDR)
		ULONGBE hlength	// of the data contained in the header "NAME". (22)
		char name[22]		// name of the sample.

		char vhdr_Id[4]		// "VHDR"
		ULONGBE hlength	// of the data contained in the header "VHDR". (20)
		VHDR+04..07 (ULONG) = samples in the high octave 1-shot part. (in bytes)
		VHDR+08..0B (ULONG) = samples in the high octave repeat part. (in bytes)
		VHDR+0C..0F (ULONG) = samples per cycle in high octave (if repeating), else 0. (in bytes)
		VHDR+10..11 (UWORD) = samples per second.  (Unsigned 16-bit quantity.)
		VHDR+12     (UBYTE) = number of octaves of waveforms in sample.
		VHDR+13     (UBYTE) = data compression (0=none, 1=Fibonacci-delta encoding).
		VHDR+14..17 (FIXED) = volume.  (The number 65536 means 1.0 or full volume.)

		The field samplesPerHiCycle tells the number of samples/cycle in the highest frequency octave of data, or else 0 for “unknown”.
		Each successive (lower frequency) octave contains twice as many samples/cycle.
		The samplesPerHiCycle value is needed to compute the data rate for a desired playback pitch.

		Actually, samplesPerHiCycle is an average number of samples/cycle.
		If the one-shot part contains pitch bends, store the samples/cycle of the repeat part in samplesPerHiCycle.
		The division repeatHiSamples/samplesPerHiCycle should yield an integer number of cycles. (When the repeat waveform is repeated,
		a partial cycle would come out as a higher-frequency cycle with a “click”.) 

		The field samplesPerSec gives the sound sampling rate.
		A program may adjust this to achieve frequency shifts or vary it dynamically to achieve pitch bends and vibrato.
		A program that plays a FORM 8SVX as a musical instrument would ignore samplesPerSec and select a playback rate for each musical pitch. 

		//A Voice holds waveform data for one or more octaves. 
		//The one-shot part is played once and the repeat part is looped.
		//The sum of oneShotHiSamples and repeatHiSamples is the full length
		// of the highest octave waveform. Each following octave waveform is twice
		// as long as the previous one. 


		char body_Id[4]		// "BODY"
		ULONGBE hlength	   // of the data contained in the BODY. May contain more than one octave.
		char *data			// the sample.

		*/

		/*********  IFF file reader comforming to Amiga audio format specifications ****/
		class AmigaSvx : public EaIff {
		public:
			AmigaSvx();
			virtual ~AmigaSvx();

			virtual void Open(const std::string& fname);
			virtual void Close() {EaIff::Close(); }
			virtual bool Eof() const {return EaIff::Eof();}
			
			const VHeader & getformat() { return format; }
            bool stereo() const { return isStereo; }
			std::string GetName();
			uint32_t getLength(int octave=1);
			uint32_t getLoopStart(int octave=1);
			uint32_t getLoopEnd(int octave=1);
			uint32_t getNoteOffsetFor(int octave=1);
			std::list<EGPoint> getVolumeEnvelope();
			float getPan() { return pan; }

			void readMonoConvertTo16(int16_t* pSamp, uint32_t samples, int octave=0);
			void readStereoConvertTo16(int16_t* pSampL,int16_t* pSampR, uint32_t samples, int octave=0);

			static const IffChunkId T8SVX;
			static const IffChunkId T16SV;
			static const IffChunkId VHDR;
			static const IffChunkId NAME;
			static const IffChunkId CHAN;
			static const IffChunkId PAN;
			static const IffChunkId ATAK;
			static const IffChunkId RLSE;
			static const IffChunkId BODY;

		protected:
			void ReadFormat();
			bool is16bits;
			bool useLE16bit;
			bool isStereo;
			float pan;
			VHeader format;
		};
	}
}

