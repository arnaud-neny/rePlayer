#include "appleaiff.hpp"
#include "sampleconverter.hpp"

namespace psycle
{
	namespace helpers
	{

		const IffChunkId AppleAIFF::AiffID= {'A','I','F','F'};   /* form name */
		const IffChunkId AppleAIFF::CommonID = {'C','O','M','M'};   /* chunkID for Common Chunk */
		const IffChunkId AppleAIFF::SoundDataID = {'S','S','N','D'};  /* chunk ID for Sound Data Chunk */
		const IffChunkId AppleAIFF::MarkerID = {'M','A','R','K'};  /* chunkID for Marker Chunk */
		const IffChunkId AppleAIFF::InstrumentID = {'I','N','S','T'};  /*chunkID for Instruments Chunk */

		AppleAIFF::AppleAIFF() {
		}
        AppleAIFF::~AppleAIFF() {
		}

		void AppleAIFF::Open(const std::string& fname)
		{ 
			EaIff::Open(fname);
			if (isValid) {
				IffChunkHeader type;
				Read(type.id);
				if (type.matches(AiffID)) {
					ReadFormat();
				}
				else {isValid=false; }
			}
		}
		void AppleAIFF::ReadFormat()
		{
			findChunk(CommonID);
			ReadRaw(&commchunk,sizeof(commchunk));
			skipThisChunk();
			try {
				findChunk(MarkerID, true);
				Read(markers.numMarkers);
				for (uint16_t i=0;i<markers.numMarkers.unsignedValue();i++) {
					Marker mark;
					Read(mark.id);
					Read(mark.position);
					ReadPString(mark.markerName);
					markers.Markers[mark.id.signedValue()]=mark;
				}
			}
			catch(const std::runtime_error & e){
				markers.numMarkers.changeValue(static_cast<uint16_t>(0));
			}
			try {
				findChunk(InstrumentID, true);
				ReadRaw(&instchunk,sizeof(InstrumentChunk));
			}
			catch(const std::runtime_error & e){
				instchunk.baseNote=0;
				instchunk.detune=0;
				instchunk.gain=0;
				instchunk.highNote=127;
				instchunk.highvelocity=127;
				instchunk.lowNote=0;
				instchunk.lowvelocity=0;
				instchunk.releaseLoop.playMode.changeValue(static_cast<uint16_t>(0));
				instchunk.sustainLoop.playMode.changeValue(static_cast<uint16_t>(0));
			}
		}

		//Psycle only uses 16bit samples, so this implementation is fully functional for Psycle's intentions.
		//It would fail to load multichannel (i.e. more than two) samples.
		void AppleAIFF::readInterleavedSamples(void* pSamps, uint32_t maxSamples, CommonChunk* convertTo)
		{
			findChunk(SoundDataID, true);
			uint32_t max = (maxSamples>0) ? maxSamples : commchunk.numSampleFrames.signedValue();
			if (convertTo == NULL || (convertTo->numChannels.signedValue() == commchunk.numChannels.signedValue()
				&& convertTo->sampleSize.signedValue() == commchunk.sampleSize.signedValue()))
			{
				readMonoSamples(pSamps, max*commchunk.numChannels.signedValue());
			}
			else if (convertTo->numChannels.signedValue() == commchunk.numChannels.signedValue()) {
				//Finish for other destination bitdepths
				switch(convertTo->sampleSize.signedValue()) 
				{
				case 8:readMonoConvertToInteger<uint8_t,assignconverter<uint8_t,uint8_t>,int16touint8,int24touint8,int32touint8>
						(reinterpret_cast<uint8_t*>(pSamps),max*commchunk.numChannels.signedValue(),127.0);break;
				case 16:readMonoConvertToInteger<int16_t,uint8toint16,assignconverter<int16_t,int16_t>,int24toint16,int32toint16>
						(reinterpret_cast<int16_t*>(pSamps),max*commchunk.numChannels.signedValue(),32767.0);break;
				case 24:break;
				case 32:break;
				default:break;
				}
			}
			else if (convertTo->numChannels.signedValue() == 1) {
				//Finish for multichannel (more than two)
			}
			else {
				//Finish for multichannel (more than two)
			}
			skipThisChunk();
		}
		//Psycle only uses 16bit samples, so this implementation is fully functional for Psycle's intentions.
		//It would fail to load multichannel (i.e. more than two) samples.
		void AppleAIFF::readDeInterleavedSamples(void** pSamps, uint32_t maxSamples, CommonChunk* convertTo)
		{
			findChunk(SoundDataID, true);
			uint32_t max = (maxSamples>0) ? maxSamples : commchunk.numSampleFrames.signedValue();
			if (convertTo == NULL || (convertTo->numChannels.signedValue() == commchunk.numChannels.signedValue() 
				&& convertTo->sampleSize.signedValue() == commchunk.sampleSize.signedValue()))
			{
				if (commchunk.numChannels.signedValue()==1) {
					readMonoSamples(pSamps[0],max);
				}
				else {
					readDeintMultichanSamples(pSamps, max);
				}
			}
			else if (convertTo->numChannels.signedValue() == commchunk.numChannels.signedValue()) {
				//Finish for other destination bitdepths
				switch(convertTo->sampleSize.signedValue()) 
				{
				case 8: {
					if (commchunk.numChannels.signedValue() ==1) {
						readMonoConvertToInteger<uint8_t,assignconverter<uint8_t,uint8_t>,int16touint8,int24touint8,int32touint8>
						(reinterpret_cast<uint8_t*>(pSamps[0]),max,127.0);
					}
					else {
						readDeintMultichanConvertToInteger<uint8_t,assignconverter<uint8_t,uint8_t>,int16touint8,int24touint8,int32touint8>
						(reinterpret_cast<uint8_t**>(pSamps),max,127.0);
					}
					break;
				}
				case 16: {
					if (commchunk.numChannels.signedValue() ==1) {
						readMonoConvertToInteger<int16_t,uint8toint16,assignconverter<int16_t,int16_t>,int24toint16,int32toint16>
						(reinterpret_cast<int16_t*>(pSamps[0]),max,32767.0);
					}
					else {
						readDeintMultichanConvertToInteger<int16_t,uint8toint16,assignconverter<int16_t,int16_t>,int24toint16,int32toint16>
						(reinterpret_cast<int16_t**>(pSamps),max,32767.0);
					}
					break;
				}
				case 24:break;
				case 32:break;
				default:break;
				}
			}
			else if (convertTo->numChannels.signedValue() == 1) {
				//Finish for multichannel (more than two)
			}
			else {
				//Finish for multichannel (more than two)
			}
			skipThisChunk();
		}


	void AppleAIFF::readMonoSamples(void* pSamp, uint32_t samples)
	{
		switch(commchunk.sampleSize.signedValue())
		{
			case 8: {
				uint8_t* smp8 = static_cast<uint8_t*>(pSamp);
				ReadArray(smp8, samples);
				break;
			}
			case 16: {
				int16_t* smp16 = static_cast<int16_t*>(pSamp);
				ReadArray(smp16, samples);
				break;
			}
			case 24: {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					Long24LE* samps = static_cast<Long24LE*>(pSamp);
					ReadWithintegerconverter<Long24BE,Long24LE,endianessconverter<Long24BE,Long24LE> >(samps, samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					Long24BE* samps = static_cast<Long24BE*>(pSamp);
					ReadArray(samps,samples);
#endif
				break;
			}
			case 32: {
					int32_t* smp32 = static_cast<int32_t*>(pSamp);
					ReadArray(smp32, samples);
				break;
			}
			default:
				break;
		}
	}

	void AppleAIFF::readDeintMultichanSamples(void** pSamps, uint32_t samples)
	{
		switch (commchunk.sampleSize.signedValue()) {
			case  8: readDeinterleaveSamples<uint8_t>(pSamps,commchunk.numChannels.signedValue(), samples);break;
			case 16: readDeinterleaveSamples<int16_t>(pSamps,commchunk.numChannels.signedValue(), samples);break;
			case 24: {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
					readDeinterleaveSamplesendian<Long24BE,Long24LE>(pSamps,commchunk.numChannels.signedValue(), samples);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
					readDeinterleaveSamples<Long24BE>(pSamps,commchunk.numChannels.signedValue(), samples);
#endif
				break;
			}
			case 32: {
					readDeinterleaveSamples<int32_t>(pSamps,commchunk.numChannels.signedValue(), samples);
				break;
			}
			default: break; ///< \todo should throw an exception
		}
	}
	}
}

