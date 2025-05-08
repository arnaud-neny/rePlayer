#include "amigasvx.hpp"
#include <psycle/helpers/math/math.hpp>

#include <string.h>

namespace psycle
{
	namespace helpers
	{
		const IffChunkId AmigaSvx::T8SVX = {'8','S','V','X'};
		const IffChunkId AmigaSvx::T16SV = {'1','6','S','V'};
		const IffChunkId AmigaSvx::VHDR = {'V','H','D','R'};
		const IffChunkId AmigaSvx::NAME = {'N','A','M','E'};
		const IffChunkId AmigaSvx::CHAN = {'C','H','A','N'};
		const IffChunkId AmigaSvx::PAN = {'P','A','N',' '};
		const IffChunkId AmigaSvx::ATAK = {'A','T','A','K'};
		const IffChunkId AmigaSvx::RLSE = {'R','L','S','E'};
		const IffChunkId AmigaSvx::BODY = {'B','O','D','Y'};

		const int8_t fibonacci[16] = { -34, -21, -13, -8, -5, -3, -2, -1, 0, 1, 2, 3, 5, 8, 13, 21 };
		const int8_t exponential[16] = { -128, -64, -32, -16, -8, -4, -2, -1, 0, 1, 2, 4, 8, 16, 32, 64 };

		AmigaSvx::AmigaSvx() {
		}
		AmigaSvx::~AmigaSvx() {
		}

		void AmigaSvx::Open(const std::string& fname) { 
			EaIff::Open(fname);
			if (isValid) {
				IffChunkHeader type;
				Read(type.id);
				if (type.matches(T8SVX)) {
					is16bits=false;
					ReadFormat();
				}
				else if (type.matches(T16SV)) {
					is16bits=true;
					std::streampos pos = GetPos();
					Read(type.id);
					//FastTrackerII saves 16bits in little endian.
					//Also, it seems to also write first NAME than VHDR, so i use this as a hint.
					useLE16bit=type.matches(NAME);
					Seek(pos);
					ReadFormat();
				}
				else {isValid=false; }
			}
		}

		void AmigaSvx::ReadFormat()
		{
			findChunk(VHDR);
			ReadRaw(&format,sizeof(VHeader));
			skipThisChunk();
			try {
				findChunk(CHAN, true);
				LongBE chanVal;
				Read(chanVal);
				switch(chanVal.unsignedValue())
				{
					case 2: isStereo = false; pan = 1.0f; break; // 2 = Left channel
					case 4: isStereo = false; pan = 0.0f; break; // 4 = Right channel
					case 6: isStereo = true;  pan = 0.5f; break; // 6 = Stereo
					default:break;
				}
				skipThisChunk();
			}
			catch(const std::runtime_error & /*e*/) {
				isStereo = false;
			}
			try {
				findChunk(PAN, true);
				FixedPointBE thepan; //If sposition = Unity, the sample is panned all the way to the left.
				ReadRaw(&thepan,sizeof(FixedPointBE));       //If sposition = 0, the sample is panned all the way to the right.
				pan = 1.f-thepan.value(); //If sposition = Unity/2, the sample is centered in the stereo field.
				skipThisChunk();
			}
			catch(const std::runtime_error & /*e*/) {
				pan = 0.5f;
			}

			//Bugfixes for several non-conforming applications
			findChunk(BODY, true);
			uint32_t expectedlen =  currentHeader.length();
			if (is16bits) { expectedlen >>=1; }
			if (isStereo) { expectedlen >>=1; }
			expectedlen >>= (format.numOctaves-1);
			//Fix size not informed
			if (format.oneShotHiSamples.unsignedValue()+format.repeatHiSamples.unsignedValue() == 0) {
				format.oneShotHiSamples.changeValue(expectedlen);
			}
			//Fix size in bytes instead of samples
			if (is16bits && format.oneShotHiSamples.unsignedValue()+format.repeatHiSamples.unsignedValue()>expectedlen) {
				format.oneShotHiSamples.changeValue(format.oneShotHiSamples.unsignedValue()>>1);
				format.repeatHiSamples.changeValue(format.repeatHiSamples.unsignedValue()>>1);
				format.samplesPerHiCycle.changeValue(format.samplesPerHiCycle.unsignedValue()>>1);
			}
			if (isStereo && format.oneShotHiSamples.unsignedValue()+format.repeatHiSamples.unsignedValue()>expectedlen) {
				format.oneShotHiSamples.changeValue(format.oneShotHiSamples.unsignedValue()>>1);
				format.repeatHiSamples.changeValue(format.repeatHiSamples.unsignedValue()>>1);
				format.samplesPerHiCycle.changeValue(format.samplesPerHiCycle.unsignedValue()>>1);
			}
			//Fix oneshot used as repeat.
			if (format.oneShotHiSamples.unsignedValue() == format.samplesPerHiCycle.unsignedValue()
					&& format.repeatHiSamples.unsignedValue() == 0) {
				format.oneShotHiSamples.changeValue(0);
				format.repeatHiSamples.changeValue(format.samplesPerHiCycle.unsignedValue());
			}
			//Fix volume not informed
			if (format.volume.integer.value == 0 && format.volume.decimal.value == 0) {
				format.volume.changeValue(1.0f);
			}
			//Just in case, ensure there is samplerate
			if (format.samplesPerSec.unsignedValue() == 0) {
				format.samplesPerSec.changeValue(static_cast<uint16_t>(8363));
			}
			//ensure there is samples/cycle if there are multiple octaves.
			if (format.numOctaves > 1 && format.samplesPerHiCycle.unsignedValue() == 0) {
				uint32_t val = 0x20 * format.samplesPerSec.unsignedValue()/8363;
				format.samplesPerHiCycle.changeValue(val>>(format.numOctaves-1));
			}
			//validate compression
			if (format.compression >= SVX8Compression::COMP_MAX_MODES
				|| (format.compression != SVX8Compression::COMP_NONE && is16bits)) {
				isValid=false;
			}
			skipThisChunk();
		}
		std::string AmigaSvx::GetName() 
		{
			char sampname[64];
			memset(sampname,0,sizeof(sampname));
			try {
				findChunk(NAME, true);
			}
			catch(const std::runtime_error& /*e*/) {
				memset(sampname,0,sizeof(sampname));
				return sampname;
			}
			ReadSizedString(sampname,std::min(static_cast<uint32_t>(sizeof(sampname))-1,currentHeader.length()));
			skipThisChunk();
			return sampname;
		}

		void AmigaSvx::readMonoConvertTo16(int16_t* pSamp, uint32_t samples, int octave/*=0*/)
		{
			const int8_t *table;
			int sampmult=(format.oneShotHiSamples.unsignedValue()+format.repeatHiSamples.unsignedValue());
			if (is16bits) sampmult<<=1;

			if (format.compression == SVX8Compression::COMP_FIB) {
				table = fibonacci;
			}
			else {
				table = exponential;
			}

			findChunk(BODY, true);

			if (octave < 0 ) {
				// negative octave trick used for reading the stereo part. I have no test file that has both, multiple octaves and stereo,
				// but this is in the definition of CHAN:
				// The BODY chunk for stereo pairs contains both left and right information. To adhere to existing
				// conventions, sampling software should write first the LEFT information,
				// followed by the RIGHT. The LEFT and RIGHT information should be equal in length.
				octave*=-1;
				uint32_t skipamount=0;
				for (int octcount=1;octcount<format.numOctaves+octave;octcount++) {
					skipamount += getLength(octcount);
				}
				Skip(skipamount);
			}
			else if (octave > 1) {
				//The sum of oneShotHiSamples and repeatHiSamples is the full length
				// of the highest octave waveform. Each following octave waveform is twice as long as the previous one. 
				uint32_t skipamount=0;
				for (int octcount=1;octcount<octave;octcount++) {
					skipamount += getLength(octcount);
				}
				Skip(skipamount);
			}

			if (is16bits) {
				if (useLE16bit) {
					for (uint32_t i=0; i < samples; i++,pSamp++) {
						ShortLE sample;
						Read(sample);
						*pSamp=sample.signedValue();
					}
				}
				else {
					for (uint32_t i=0; i < samples; i++,pSamp++) {
						ShortBE sample;
						Read(sample);
						*pSamp=sample.signedValue();
					}
				}
			}
			else switch(format.compression) {
				case SVX8Compression::COMP_NONE:
					for (uint32_t i=0; i < samples; i++,pSamp++) {
						int8_t sample;
						Read(sample);
						*pSamp=sample<<8;
					}
					break;
				case SVX8Compression::COMP_FIB: //fallthrough
				case SVX8Compression::COMP_EXP:
					int8_t val;
					Skip(1); //Skip first byte. it is a padding byte.
					Read(val);//Get the initial value.
					for (uint32_t i=0; i < samples; i+=2,pSamp+=2) {
						int8_t sample;
						Read(sample);
						/* Decode a data nibble; high nibble then low nibble. */
						val = helpers::math::clip<8,int8_t>(val + table[sample>>4]);
						pSamp[0] = val;
						val = helpers::math::clip<8,int8_t>(val + table[sample&0xF]);
						pSamp[1] = val;
					}
				default:
					break;
			}
			skipThisChunk();
		}
		void AmigaSvx::readStereoConvertTo16(int16_t* pSampL,int16_t* pSampR, uint32_t samples, int octave/*=0*/)
		{
			readMonoConvertTo16(pSampL,samples,octave);
			readMonoConvertTo16(pSampR,samples,octave*-1);
		}

		uint32_t AmigaSvx::getLength(int octave)
		{
			uint32_t length = format.oneShotHiSamples.unsignedValue() + format.repeatHiSamples.unsignedValue();
			return length << (octave-1);
		}

		uint32_t AmigaSvx::getLoopStart(int octave)
		{
			if (format.repeatHiSamples.unsignedValue() > 0) {
				uint32_t start = format.oneShotHiSamples.unsignedValue();
				return start << (octave-1);
			}
			else return 0;
		}
		uint32_t AmigaSvx::getLoopEnd(int octave)
		{
			if (format.repeatHiSamples.unsignedValue() > 0) {
				uint32_t end = format.oneShotHiSamples.unsignedValue() + format.repeatHiSamples.unsignedValue();
				return end << (octave-1);
			}
			else return 0;
		}
		uint32_t AmigaSvx::getNoteOffsetFor(int octave)
		{
			int noteres=0;
			//Calculate it only on instruments, or if there is a loop, but ignore beat loop types.
			if (format.samplesPerHiCycle.unsignedValue() > 0 && 
				( format.numOctaves > 1 || ( format.repeatHiSamples.unsignedValue() > 0 
					&& format.repeatHiSamples.unsignedValue() < 16*format.samplesPerHiCycle.unsignedValue()))
			){
				float cycles = format.samplesPerHiCycle.unsignedValue() << (octave-1);
				noteres=96-(log10f(format.samplesPerSec.unsignedValue()/cycles)/log10f(2.f)*12);
			}
			return noteres;
		}

		std::list<EGPoint> AmigaSvx::getVolumeEnvelope()
		{
			EGPoint p;
			int numpoints;
			std::list<EGPoint> list;
			try {
				findChunk(ATAK, true);
			}
			catch(const std::runtime_error& /*e*/) {
				return list;
			}
			numpoints = currentHeader.length() / sizeof(EGPoint);
			for (int i=0; i< numpoints;i++) {
				Read(p.duration);
				ReadRaw(&p.dest, sizeof(FixedPointBE));
				list.push_back(p);
			}
			skipThisChunk();

			//Sustain point marker
			p.duration.changeValue(static_cast<int16_t>(-1));
			list.push_back(p);

			try {
				findChunk(RLSE, true);
			}
			catch(const std::runtime_error& /*e*/) {
				return list;
			}
			// The documentation seems to suggest that release points work backward
			// i.e. they say the time until next position, rather than the time to reach the point.
			numpoints = currentHeader.length() / sizeof(EGPoint);
			if (numpoints > 0){
				Read(p.duration);
				//Ignored, assumed equal to sustain point (last attack point)
				ReadRaw(&p.dest, sizeof(FixedPointBE));
				uint16_t prevduration=p.duration.unsignedValue();
				for (int i=0; i< numpoints;i++) {
					p.duration.changeValue(prevduration);
					Read(prevduration);
					ReadRaw(&p.dest, sizeof(FixedPointBE));
					list.push_back(p);
				}
				p.dest.changeValue(0.f);
				list.push_back(p);
			}
			skipThisChunk();
			return list;
		}

	}
}

