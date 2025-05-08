#pragma once
#include <fstream>
#include <universalis.hpp>

namespace psycle
{
	namespace helpers
	{
		using namespace universalis::stdlib;
		class FormatDetector {
		public:
			//IDS returned by Autodetect
			static const uint32_t PSYI_ID;
			static const uint32_t XI_ID;
			static const uint32_t IMPI_ID;
			static const uint32_t IMPS_ID;
			static const uint32_t SCRS_ID;
			static const uint32_t WAVE_ID;
			static const uint32_t AIFF_ID;
			static const uint32_t SVX8_ID; //Returned for both, 8 and 16bit files
			static const uint32_t INVALID_ID;
			
			FormatDetector(){}
			virtual ~FormatDetector();
			uint32_t AutoDetect(const std::string& filename);

		protected:
			//Internal use
			static const char XI_STRING_ID[];
			static const uint32_t FORM_ID;
			static const uint32_t RIFF_ID;
			static const uint32_t SV16_ID;

			bool DetectPSYINS();
			bool DetectXI();
			bool DetectITI();
			bool DetectITS();
			bool DetectS3I();
			bool DetectWAVE();
			bool DetectAIFF();
			bool DetectSVX();

			void ReadRaw (void * data, std::size_t const bytes);

			std::fstream _stream;
		};
	}
}

