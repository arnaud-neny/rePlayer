#pragma once
#include "msriff.hpp"
namespace psycle { namespace helpers {
/* Some info from http://www.sonicspot.com/guide/wavefiles.html#smpl
Other from Microsoft RIFFNEW.pdf 
*/
	class WaveFormat_Data;

	/// the riff WAVE/fmt chunk.
	class RiffWaveFmtChunk
	{
	public:
		RiffWaveFmtChunk();
		RiffWaveFmtChunk(const WaveFormat_Data& config);
		virtual ~RiffWaveFmtChunk() {}

		static const IffChunkId fmt;
        	static const uint16_t FORMAT_PCM;
        	static const uint16_t FORMAT_FLOAT;
        	static const uint16_t FORMAT_EXTENSIBLE;

		static const std::size_t SIZEOF;

		uint16_t wFormatTag;
		/// Number of channels (mono=1, stereo=2)
		uint16_t wChannels;
		/// Sampling rate [Hz]
		uint32_t dwSamplesPerSec;
		/// Indication of the amount of bytes required for one second of audio
		uint32_t dwAvgBytesPerSec;
		/// block align (i.e. bytes of one frame).
		uint16_t wBlockAlign;
		/// bits per sample (i.e. bits of one frame for a single channel)
		uint16_t wBitsPerSample;
	};

	//WAVEFORMAT_EXTENSIBLE
	struct SubFormatTag_t {
		uint32_t data1;
		uint16_t data2;
		uint16_t data3;
		char data4[8];
	};

	class RiffWaveFmtChunkExtensible : public RiffWaveFmtChunk
	{
	public:
		RiffWaveFmtChunkExtensible();
		RiffWaveFmtChunkExtensible(const RiffWaveFmtChunk& copy);
		RiffWaveFmtChunkExtensible(const WaveFormat_Data& config);
		virtual ~RiffWaveFmtChunkExtensible() {}

	public:
		static const std::size_t SIZEOF;

		uint16_t extensionSize;
		uint16_t numberOfValidBits;
		uint32_t speakerPositionMask;
		SubFormatTag_t subFormatTag;
	};


	//RiffWaveSmplChunk.numLoop amount of RiffWaveSmplLoopChunk are attached at the end of RiffWaveSmplChunk.
	class RiffWaveSmplLoopChunk
	{
	public:
		uint32_t cueId; // 0 - 0xFFFFFFFF
		uint32_t type;	// 0 - 0xFFFFFFFF
		uint32_t start;	// 0 - 0xFFFFFFFF
		uint32_t end;	// 0 - 0xFFFFFFFF
		uint32_t fraction;// 0 - 0xFFFFFFFF
		uint32_t playCount;// 0 - 0xFFFFFFFF
	};
/*
Type
The type field defines how the waveform samples will be looped.

Value 	Loop Type
0 	Loop forward (normal)
1 	Alternating loop (forward/backward, also known as Ping Pong)
2 	Loop backward (reverse)
3 - 31 	Reserved for future standard types
32 - 0xFFFFFFFF 	Sampler specific types (defined by manufacturer)	
	

From the official RIFF specifications by Microsoft (RIFFNEW.pdf):
dwStart: Specifies the startpoint of the loop in samples.
dwEnd: Specifies the endpoint of the loop in samples (this sample will also be
played).
*/
	class RiffWaveSmplChunk
	{
	public:
		static const std::size_t SIZEOF;

		uint32_t manufacturer;  // 0 - 0xFFFFFFFF
		uint32_t product;		// 0 - 0xFFFFFFFF
		uint32_t samplePeriod;	// 0 - 0xFFFFFFFF
		uint32_t midiNote;		// 0 - 127
		uint32_t midiPitchFr;	// 0 - 0xFFFFFFFF
		uint32_t smpteFormat;	// 0, 24, 25, 29, 30
		uint32_t smpteOffset;	// 0 - 0xFFFFFFFF
		uint32_t numLoops;		// 0 - 0xFFFFFFFF
		uint32_t extraDataSize;	// 0 - 0xFFFFFFFF
		RiffWaveSmplLoopChunk* loops;
		
		RiffWaveSmplChunk():loops(NULL){};
		virtual ~RiffWaveSmplChunk() { if (loops) delete[] loops; }
	};
/*
Sample Period
The sample period specifies the duration of time that passes during the playback of one sample in nanoseconds (normally equal to 1 / Samples Per Second, where Samples Per Second is the value found in the format chunk).

MIDI Unity Note
Specifies the MIDI note which will replay the sample at original pitch. This
value ranges from 0 to 127 (a value of 60 represents Middle C as defined
by the MMA).
The MIDI unity note value has the same meaning as the instrument chunk's MIDI Unshifted Note field which specifies the musical note at which the sample will be played at it's original sample rate (the sample rate specified in the format chunk).

MIDI Pitch Fraction
The MIDI pitch fraction specifies the fraction of a semitone up from the specified MIDI unity note field. A value of 0x80000000 means 1/2 semitone (50 cents) and a value of 0x00000000 means no fine tuning between semitones. 

Sample Loops
The sample loops field specifies the number Sample Loop definitions in the following list. This value may be set to 0 meaning that no sample loops follow.

Sampler Data
The sampler data value specifies the number of bytes that will follow this chunk (including the entire sample loop list). This value is greater than 0 when an application needs to save additional information. This value is reflected in this chunks data size value.

*/

	class RiffWaveInstChunk
	{
	public:
		static const std::size_t SIZEOF;
		uint8_t midiNote;	// 1 - 127
		int8_t midiCents;	// -50 - +50
		int8_t gaindB;		// -64 - +64
		uint8_t lowNote;	// 1 - 127
		uint8_t highNote;	// 1 - 127
		uint8_t lowVelocity; // 1 - 127
		uint8_t highVelocity; // 1 - 127
	};
/*
Unshifted Note
The unshifted note field has the same meaning as the sampler chunk's MIDI Unity Note which specifies the musical note at which the sample will be played at it's original sample rate (the sample rate specified in the format chunk).

Fine Tune
The fine tune value specifies how much the sample's pitch should be altered when the sound is played back in cents (1/100 of a semitone). A negative value means that the pitch should be played lower and a positive value means that it should be played at a higher pitch.

Gain
The gain value specifies the number of decibels to adjust the output when it is played. A value of 0dB means no change, 6dB means double the amplitude of each sample and -6dB means to halve the amplitude of each sample. Every additional +/-6dB will double or halve the amplitude again.

Low Note and High Note
The note fields specify the MIDI note range for which the waveform should be played when receiving MIDI note events (from software or triggered by a MIDI controller). This range does not need to include the Unshifted Note value.

Low Velocity and High Velocity
The velocity fields specify the range of MIDI velocities that should cause the waveform to be played. 1 being the lightest amount and 127 being the hardest. 
*/

	class WaveFormat_Data {
	public:
		bool isfloat;
		uint16_t nChannels;
		uint32_t nSamplesPerSec;
		uint16_t nBitsPerSample;
		uint16_t nUsableBitsPerSample;
		//TODO: WAVEFORMAT_EXTENSIBLE
	public:
		WaveFormat_Data() {
			Config();
		}
		WaveFormat_Data(uint32_t NewSamplingRate, uint16_t NewBitsPerSample, uint16_t NewNumChannels, bool isFloat) {
			Config(NewSamplingRate, NewBitsPerSample, NewNumChannels, isFloat);
		}

		inline bool operator==(const WaveFormat_Data& other) const;

		void Config(uint32_t NewSamplingRate = 44100, uint16_t NewBitsPerSample = 16, uint16_t NewNumChannels = 2, bool useFloat = false);
		void Config(const RiffWaveFmtChunk& chunk);
		void Config(const RiffWaveFmtChunkExtensible& chunk);

	};
	bool WaveFormat_Data::operator==(const WaveFormat_Data& other) const {
		return isfloat == other.isfloat && nChannels == other.nChannels
			&& nSamplesPerSec == other.nSamplesPerSec
			&& nBitsPerSample == other.nBitsPerSample
			&& nUsableBitsPerSample == other.nUsableBitsPerSample;
	}



/*********  IFF file reader comforming to IBM/Microsoft WaveRIFF specifications ****/
/* This one only supports up to 32bits (4GB) samples, plus support for reading 64bit RF64 that have less than 2^32 sampleframes*/
class RiffWave : public MsRiff {
public:
	RiffWave();
	virtual ~RiffWave();

	virtual void Open(const std::string& fname);
	virtual void Create(const std::string& fname, bool overwrite, 
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool littleEndian=true
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool littleEndian=false
#endif
		);
	virtual void Close();
	virtual const BaseChunkHeader& findChunk(const IffChunkId& id, bool allowWrap=false);

	const WaveFormat_Data& format() const {return formatdata; }
	uint32_t numSamples() const { return numsamples; }

	void SeekToSample(uint32_t sampleIndex);
	void addFormat(uint32_t SamplingRate, uint16_t BitsPerSample, uint16_t NumChannels, bool isFloat);
	void addSmplChunk(const RiffWaveSmplChunk& smpl);
	void addInstChunk(const RiffWaveInstChunk& inst);
	
	void readInterleavedSamples(void* pSamps, uint32_t maxSamples, WaveFormat_Data* convertTo=NULL);
	void readDeInterleavedSamples(void** pSamps, uint32_t maxSamples, WaveFormat_Data* convertTo=NULL);

	void writeFromInterleavedSamples(void* pSamps, uint32_t samples, WaveFormat_Data* convertFrom=NULL);
	void writeFromDeInterleavedSamples(void** pSamps, uint32_t samples, WaveFormat_Data* convertFrom=NULL);
	
	bool GetSmplChunk(RiffWaveSmplChunk& smpl);
	bool GetInstChunk(RiffWaveInstChunk& inst);

	static const IffChunkId RF64;
	static const IffChunkId WAVE;
	static const IffChunkId ds64;
	static const IffChunkId data;
	static const IffChunkId fact;
	static const IffChunkId smpl;
	static const IffChunkId inst;
protected:
	uint32_t calcSamplesFromBytes(uint32_t length);

	void ReadFormat();
	void readMonoSamples(void* pSamp, uint32_t samples);
	void readDeintMultichanSamples(void** pSamps, uint32_t samples);

	template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
		, out_type (*int32conv)(int32_t), out_type (*floatconv)(float, double), out_type (*doubleconv)(double, double)>
	void readMonoConvertToInteger(out_type* pSamp, uint32_t samples, double multi);

	template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
		, out_type (*int32conv)(int32_t), out_type (*floatconv)(float, double), out_type (*doubleconv)(double, double)>
	void readDeintMultichanConvertToInteger(out_type** pSamps, uint32_t samples, double multi);

	void writeMonoSamples(void* pSamp, uint32_t samples);
	void writeDeintMultichanSamples(void** pSamps, uint32_t samples);




	WaveFormat_Data formatdata;

	std::streampos ds64_pos;
	std::streampos pcmdata_pos;
	uint32_t numsamples;

};

	//Templates to use with RiffWave class.
	///////////////////////////////////////
	template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
		, out_type (*int32conv)(int32_t), out_type (*floatconv)(float, double), out_type (*doubleconv)(double, double)>
	void RiffWave::readMonoConvertToInteger(out_type* pSamp, uint32_t samples, double multi)
	{
		switch(formatdata.nBitsPerSample)
		{
			case 8: ReadWithintegerconverter<uint8_t,out_type,uint8conv>(pSamp, samples);break;
			case 16: ReadWithintegerconverter<int16_t,out_type,int16conv>(pSamp, samples);break;
			case 24: {
				if (isLittleEndian) {
					ReadWithinteger24converter<Long24LE, out_type, int24conv>(pSamp,samples);
				}
				else {
					ReadWithinteger24converter<Long24BE, out_type, int24conv>(pSamp,samples);
				}
				break;
				}
			case 32: {
				if (formatdata.isfloat) {
					ReadWithfloatconverter<float,out_type,floatconv>(pSamp, samples,multi);
				}
				else {
					//TODO: Verify that this works for 24-in-32bits (WAVERFORMAT_EXTENSIBLE)
					ReadWithintegerconverter<int32_t,out_type,int32conv>(pSamp, samples);
				}
				break;
			}
			case 64: ReadWithfloatconverter<double,out_type,doubleconv>(pSamp, samples,multi); break;
			default: break; ///< \todo should throw an exception
		}
	}

	template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
		, out_type (*int32conv)(int32_t), out_type (*floatconv)(float, double), out_type (*doubleconv)(double, double)>
	void RiffWave::readDeintMultichanConvertToInteger(out_type** pSamps, uint32_t samples, double multi)
	{
		switch(formatdata.nBitsPerSample)
		{
			case 8:
				ReadWithmultichanintegerconverter<uint8_t,out_type,uint8conv>(pSamps, formatdata.nChannels, samples);
				break;
			case 16:
				ReadWithmultichanintegerconverter<int16_t,out_type,int16conv>(pSamps, formatdata.nChannels, samples);
				break;
			case 24:
				if (isLittleEndian) {
					ReadWithmultichaninteger24converter<Long24LE, out_type, int24conv>(pSamps,formatdata.nChannels,samples);
				}
				else {
					ReadWithmultichaninteger24converter<Long24BE, out_type, int24conv>(pSamps,formatdata.nChannels,samples);
				}
				break;
			case 32: {
				if (formatdata.isfloat) {
					ReadWithmultichanfloatconverter<float,out_type,floatconv>(pSamps, formatdata.nChannels, samples,multi);
				}
				else {
					//TODO: Verify that this works for 24-in-32bits (WAVERFORMAT_EXTENSIBLE)
					ReadWithmultichanintegerconverter<int32_t,out_type,int32conv>(pSamps, formatdata.nChannels, samples);
				}
				break;
			}
			case 64: ReadWithmultichanfloatconverter<double,out_type,doubleconv>(pSamps, formatdata.nChannels, samples,multi); break;
			default: break; ///< \todo should throw an exception
		}
	}


}}
