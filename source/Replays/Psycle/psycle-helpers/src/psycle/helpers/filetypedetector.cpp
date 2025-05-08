#include "filetypedetector.hpp"
namespace psycle
{
	namespace helpers
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		#define MakeID(a,b,c,d) ( (d)<<24 | (c)<<16 | (b)<<8 | (a) )
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		#define MakeID(a,b,c,d) ( (a)<<24 | (b)<<16 | (c)<<8 | (d) )
#endif
		const char FormatDetector::XI_STRING_ID[] = "Extended Instrument: ";

		const uint32_t FormatDetector::FORM_ID = MakeID('F','O','R','M');
		const uint32_t FormatDetector::RIFF_ID = MakeID('R','I','F','F');

		const uint32_t FormatDetector::PSYI_ID = MakeID('P','S','Y','I');
		const uint32_t FormatDetector::XI_ID   = MakeID('X','I',' ',' '); // This is just to use internally in Psycle
		const uint32_t FormatDetector::IMPI_ID = MakeID('I','M','P','I');
		const uint32_t FormatDetector::IMPS_ID = MakeID('I','M','P','S');
		const uint32_t FormatDetector::SCRS_ID = MakeID('S','C','R','S');
		const uint32_t FormatDetector::WAVE_ID = MakeID('W','A','V','E');
		const uint32_t FormatDetector::AIFF_ID = MakeID('A','I','F','F');
		const uint32_t FormatDetector::SVX8_ID = MakeID('8','S','V','X');
		const uint32_t FormatDetector::SV16_ID = MakeID('1','6','S','V');
		const uint32_t FormatDetector::INVALID_ID = 0;

		FormatDetector::~FormatDetector()
		{
			if (_stream.is_open ()) _stream.close();
		}
		uint32_t FormatDetector::AutoDetect(const std::string& filename)
		{
			_stream.open(filename.c_str(), std::ios_base::in | std::ios_base::binary);
			if (!_stream.is_open ()) return INVALID_ID;

			try {
				if (DetectPSYINS()) { _stream.close(); return PSYI_ID; }
				_stream.seekg (0, std::ios::beg);
				if (DetectWAVE()) { _stream.close(); return WAVE_ID; }
				_stream.seekg (0, std::ios::beg);
				if (DetectAIFF()) { _stream.close(); return AIFF_ID; }
				_stream.seekg (0, std::ios::beg);
				if (DetectXI())   { _stream.close(); return XI_ID; }
				_stream.seekg (0, std::ios::beg);
				if (DetectITI())  { _stream.close(); return IMPI_ID; }
				_stream.seekg (0, std::ios::beg);
				if (DetectITS())  { _stream.close(); return IMPS_ID; }
				_stream.seekg (0, std::ios::beg);
				if (DetectS3I())  { _stream.close(); return SCRS_ID; }
				_stream.seekg (0, std::ios::beg);
				if (DetectSVX())  { _stream.close(); return SVX8_ID; }
			}
			catch(const std::runtime_error & /*e*/)
			{
			}
			_stream.close(); 
			return INVALID_ID;
		}

		bool FormatDetector::DetectAIFF()
		{
			uint32_t id;
			ReadRaw(&id,sizeof(uint32_t));
			if (id != FORM_ID) return false;
			_stream.seekg (8, std::ios::beg);
			ReadRaw(&id,sizeof(uint32_t));
			if (id != AIFF_ID) return false;
			return true;
		}
		bool FormatDetector::DetectSVX()
		{
			uint32_t id;
			ReadRaw(&id,sizeof(uint32_t));
			if (id != FORM_ID) return false;
			_stream.seekg (8, std::ios::beg);
			ReadRaw(&id,sizeof(uint32_t));
			if (id != SVX8_ID && id != SV16_ID) return false;
			return true;
		}
		bool FormatDetector::DetectWAVE()
		{
			uint32_t id;
			ReadRaw(&id,sizeof(uint32_t));
			if (id != RIFF_ID) return false;
			_stream.seekg (8, std::ios::beg);
			ReadRaw(&id,sizeof(uint32_t));
			if (id != WAVE_ID) return false;
			return true;
		}
		bool FormatDetector::DetectS3I()
		{
			uint8_t type;
			uint32_t id;
			ReadRaw(&type,sizeof(type));
			if (type != 1) return false;
			_stream.seekg (0x4C, std::ios::beg);
			ReadRaw(&id,sizeof(id));
			if (id != SCRS_ID) return false;
			return true;
		}
		bool FormatDetector::DetectITI()
		{
			uint32_t id;
			ReadRaw(&id,sizeof(uint32_t));
			if (id != IMPI_ID) return false;
			return true;
		}
		bool FormatDetector::DetectITS()
		{
			uint32_t id;
			ReadRaw(&id,sizeof(uint32_t));
			if (id != IMPS_ID) return false;
			return true;
		}
		bool FormatDetector::DetectXI()
		{
			char read[21];
			ReadRaw(read,sizeof(read));
			if ( strncmp(read,XI_STRING_ID,sizeof(read))) return false;
			return true;
		}

		bool FormatDetector::DetectPSYINS() 
		{
			uint32_t id;
			ReadRaw(&id,sizeof(uint32_t));
			if (id != RIFF_ID) return false;
			_stream.seekg (8, std::ios::beg);
			ReadRaw(&id,sizeof(uint32_t));
			if (id != PSYI_ID) return false;
			return true;	
		}

		void FormatDetector::ReadRaw (void * data, std::size_t const bytes)
		{
			if (_stream.eof()) throw std::runtime_error("Read failed");
			_stream.read(reinterpret_cast<char*>(data) ,bytes);
			if (_stream.eof() || _stream.bad()) throw std::runtime_error("Read failed");
		}
	}
}

