#pragma once
#include "eaiff.hpp"
#include <map>
namespace psycle
{
	namespace helpers
	{
		#define	NoLooping				0
		#define	ForwardLooping			1
		#define	ForwardBackwardLooping	2

#pragma pack(push, 1)

		typedef ShortBE  MarkerId;

		typedef struct {
		  ShortBE       numChannels;
		  LongBE        numSampleFrames;
		  ShortBE       sampleSize;
		  Extended   sampleRate;
		} CommonChunk;

		typedef struct {
		  ShortBE   playMode;
		  MarkerId  beginLoop;
		  MarkerId  endLoop;
		} Loop;

		typedef struct {
		  int8_t baseNote;
		  int8_t detune;
		  int8_t lowNote;
		  int8_t highNote;
		  int8_t lowvelocity;
		  int8_t highvelocity;
		  short  gain;
		  Loop   sustainLoop;
		  Loop   releaseLoop;
		} InstrumentChunk;

#pragma pack(pop)

		typedef struct {
		  LongBE   offset;
		  LongBE   blockSize;
		  uint8_t*  WaveformData;
		}  SoundDataChunk;

		typedef struct {
		  MarkerId     id;
		  LongBE       position;
		  std::string  markerName;
		} Marker;

		typedef  struct {
		  ShortBE  numMarkers;
		  std::map<int16_t,Marker>   Markers;
		} MarkerChunk;


		/*********  IFF file reader comforming to Apple Audio IFF specifications ****/
		class AppleAIFF : public EaIff {
		public:
			AppleAIFF();
			virtual ~AppleAIFF();

			virtual void Open(const std::string& fname);
            virtual void Close() { EaIff::Close(); }
            virtual bool Eof() const { return EaIff::Eof();}

			void readInterleavedSamples(void* pSamps, uint32_t maxSamples, CommonChunk* convertTo=NULL);//Only channels and bits checked for converTo
			void readDeInterleavedSamples(void** pSamps, uint32_t maxSamples, CommonChunk* convertTo=NULL);//Only channels and bits checked for converTo

			static const IffChunkId AiffID;
			static const IffChunkId CommonID;
			static const IffChunkId SoundDataID;
			static const IffChunkId MarkerID;
			static const IffChunkId InstrumentID;

			InstrumentChunk instchunk;
			MarkerChunk markers;
			CommonChunk commchunk;
		protected:
			void ReadFormat();
			void readMonoSamples(void* pSamp, uint32_t samples);
			void readDeintMultichanSamples(void** pSamps, uint32_t samples);

			template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
				, out_type (*int32conv)(int32_t)>
			void readMonoConvertToInteger(out_type* pSamp, uint32_t samples, double multi);

			template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
				, out_type (*int32conv)(int32_t)>
			void readDeintMultichanConvertToInteger(out_type** pSamps, uint32_t samples, double multi);
		};



		template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
			, out_type (*int32conv)(int32_t)>
		void AppleAIFF::readMonoConvertToInteger(out_type* pSamp, uint32_t samples, double multi)
		{
			switch(commchunk.sampleSize.signedValue())
			{
				case 8: ReadWithintegerconverter<uint8_t,out_type,uint8conv>(pSamp, samples);break;
				case 16: ReadWithintegerconverter<int16_t,out_type,int16conv>(pSamp, samples);break;
				case 24: {
						ReadWithinteger24converter<Long24BE, out_type, int24conv>(pSamp,samples);
						break;
					}
				case 32: {
						//TODO: Verify that this works for 24-in-32bits (WAVERFORMAT_EXTENSIBLE)
						ReadWithintegerconverter<int32_t,out_type,int32conv>(pSamp, samples);
					break;
				}
				default: break; ///< \todo should throw an exception
			}
		}

		template<typename out_type, out_type (*uint8conv)(uint8_t), out_type (*int16conv)(int16_t), out_type (*int24conv)(int32_t)
			, out_type (*int32conv)(int32_t)>
		void AppleAIFF::readDeintMultichanConvertToInteger(out_type** pSamps, uint32_t samples, double multi)
		{
			switch(commchunk.sampleSize.signedValue())
			{
				case 8:
					ReadWithmultichanintegerconverter<uint8_t,out_type,uint8conv>(pSamps, commchunk.numChannels.signedValue(), samples);
					break;
				case 16:
					ReadWithmultichanintegerconverter<int16_t,out_type,int16conv>(pSamps, commchunk.numChannels.signedValue(), samples);
					break;
				case 24:
					ReadWithmultichaninteger24converter<Long24BE, out_type, int24conv>(pSamps,commchunk.numChannels.signedValue(),samples);
					break;
				case 32: {
						//TODO: Verify that this works for 24-in-32bits (WAVERFORMAT_EXTENSIBLE)
						ReadWithmultichanintegerconverter<int32_t,out_type,int32conv>(pSamps, commchunk.numChannels.signedValue(), samples);
					break;
				}
				default: break; ///< \todo should throw an exception
			}
		}

	}
}

