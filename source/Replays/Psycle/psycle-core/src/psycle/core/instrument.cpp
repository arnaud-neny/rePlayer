// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

///\implementation psycle::core::Instrument.

#include <psycle/core/detail/project.private.hpp>
#include "instrument.h"

#include <psycle/helpers/datacompression.hpp>
#include <psycle/helpers/filter.hpp>
#include "constants.h"
#include "fileio.h"

#include <iostream> // only for debug output
#include <sstream>
#include <iomanip>
#include <cstring>

namespace psycle { namespace core {

using namespace helpers;

///\todo doc!
///\todo move to psycle-helpers?
template<class T> inline std::string toHex(T value , int nums = 8) {

		std::ostringstream buffer;
		buffer.setf(std::ios::uppercase);

		buffer.str("");
		buffer << std::setfill('0') << std::hex << std::setw( nums );
		buffer << (int) value;

		return buffer.str();
}

///\todo doc!
///\todo move to psycle-helpers?
template<class T> inline T str_hex(const std::string &  value, int pos) {
		T result;

		pos = pos*8;

		std::stringstream str;
		str << value.substr(pos,8);
		str >> std::hex >> result;

		return result;
}

Instrument::Instrument()
:
	waveLength(0),
	waveDataL(0),
	waveDataR(0)
{
	// clear everythingout
	Reset();
}

Instrument::~Instrument()
{
	delete[] waveDataL; 
	delete[] waveDataR;
}

void Instrument::Reset()
{
	_locked_machine_index = -1;
	_locked_to_machine = false;
	// Reset envelope
	ENV_AT = 1; // 16
	ENV_DT = 1; // 16386
	ENV_SL = 100; // 64
	ENV_RT = 16; // OVERLAPTIME
	
	ENV_F_AT = 16;
	ENV_F_DT = 16384;
	ENV_F_SL = 64;
	ENV_F_RT = 16384;
	
	ENV_F_CO = 64;
	ENV_F_RQ = 64;
	ENV_F_EA = 128;
	ENV_F_TP = dsp::F_NONE;
	
	_loop = false;
	_lines = 16;
	
	_NNA = 0; // NNA set to Note Cut [Fast Release]
	
	_pan = 128;
	_RPAN = false;
	_RCUT = false;
	_RRES = false;
	
	DeleteLayer();
	
	std::sprintf(_sName, "empty");
}

void Instrument::DeleteLayer(void)
{
	std::sprintf(waveName, "empty");
	
	delete[] waveDataL; waveDataL = 0;
	delete[] waveDataR; waveDataR = 0;
	waveLength = 0;
	waveStereo=false;
	waveLoopStart=0;
	waveLoopEnd=0;
	waveLoopType=0;
	waveVolume=100;
	waveFinetune=0;
	waveTune=0;
}

bool Instrument::Empty()
{
	return !waveLength;
}

void Instrument::LoadFileChunk(RiffFile* pFile,int /*version*/,bool fullopen)
{
	Reset();
	// assume version 0 for now
	pFile->Read(_loop);
	pFile->Read(_lines);
	pFile->Read(_NNA);

	pFile->Read(ENV_AT);
	pFile->Read(ENV_DT);
	pFile->Read(ENV_SL);
	pFile->Read(ENV_RT);
	
	pFile->Read(ENV_F_AT);
	pFile->Read(ENV_F_DT);
	pFile->Read(ENV_F_SL);
	pFile->Read(ENV_F_RT);

	pFile->Read(ENV_F_CO);
	pFile->Read(ENV_F_RQ);
	pFile->Read(ENV_F_EA);
    { int val = 0; pFile->Read(val); ENV_F_TP = static_cast<helpers::dsp::FilterType>(val); }

	pFile->Read(_pan);
	pFile->Read(_RPAN);
	pFile->Read(_RCUT);
	pFile->Read(_RRES);

	pFile->ReadString(_sName, sizeof _sName);

	// now we have to read waves

    int32_t numwaves = 0;
	pFile->Read(numwaves);
	for (int i = 0; i < numwaves; i++)
	{
		char Header[5];

		pFile->ReadArray(Header,4);
		Header[4] = 0;
		uint32_t version = 0;
		uint32_t size = 0;

		if (strcmp(Header,"WAVE")==0)
		{

			pFile->Read(version);
			pFile->Read(size);
			//fileformat supports several waves, but sampler only supports one.
			if (version > CURRENT_FILE_VERSION_WAVE || i > 0)
			{
				// there is an error, this file is newer than this build of psycle
				//MessageBox(NULL,"Wave Segment of File is from a newer version of psycle!",NULL,NULL);
				pFile->Skip(size);
			}
			else
			{
				uint32_t index = 0;
				pFile->Read(index);

				pFile->Read(waveLength);
				pFile->Read(waveVolume);
				pFile->Read(waveLoopStart);
				pFile->Read(waveLoopEnd);
				
				pFile->Read(waveTune);
				pFile->Read(waveFinetune);
				pFile->Read(waveLoopType);
				pFile->Read(waveStereo);
				
				pFile->ReadString(waveName, sizeof waveName);
				
				pFile->Read(size);
				uint8_t* pData;
				
				if ( fullopen )
				{
					pData = new uint8_t[size+4];// +4 to avoid any attempt at buffer overflow by the code <-- ?
					pFile->ReadArray(pData,size);
					///\todo SoundDesquash should be object-oriented and provide access to this via its interface
					uint32_t pDataLength = (pData[4]<<24) | (pData[3]<<16) | (pData[2]<<8) | pData[1];
					if(waveLength != pDataLength)
					{
						std::ostringstream s;
						s << "instrument: " << index << ", name: " << waveName << std::endl;
						s << "sample data: unpacked length mismatch: " << waveLength << " versus " << pDataLength << std::endl;
						s << "You should reload this wave sample and all the samples after this one!";
						//loggers::warning(s.str());
						std::cout << "Warning: " << s.str() << std::endl;
						//MessageBox(0, s.str().c_str(), "Loading wave sample data", MB_ICONWARNING | MB_OK);
					}
					DataCompression::SoundDesquash(pData,&waveDataL);
					delete[] pData;
				}
				else
				{
					pFile->Skip(size);
					waveDataL=new int16_t[2];
				}

				if (waveStereo)
				{
					pFile->Read(size);
					if ( fullopen )
					{
						pData = new uint8_t[size+4]; // +4 to avoid any attempt at buffer overflow by the code <-- ?
						pFile->ReadArray(pData,size);
						uint32_t pDataLength = (pData[4]<<24) | (pData[3]<<16) | (pData[2]<<8) | pData[1];
						///\todo SoundDesquash should be object-oriented and provide access to this via its interface
						if(waveLength != pDataLength)
						{
							std::ostringstream s;
							s << "instrument: " << index << ", name: " << waveName << std::endl;
							s << "stereo wave sample data: unpacked length mismatch: " << waveLength << " versus " << pDataLength << std::endl;
							s << "You should reload this wave sample and all the samples after this one!";
							//loggers::warning(s.str());
							std::cout << "Warning: " << s.str() << std::endl;
							//MessageBox(0, s.str().c_str(), "Loading stereo wave sample data", MB_ICONWARNING | MB_OK);
						}
						DataCompression::SoundDesquash(pData,&waveDataR);
						delete[] pData;
					}
					else
					{
						pFile->Skip(size);
						waveDataR = new int16_t[2];
					}
				}
			}
		}
		else
		{
			pFile->Read(version);
			pFile->Read(size);
			// there is an error, this file is newer than this build of psycle
			//MessageBox(NULL,"Wave Segment of File is from a newer version of psycle!",NULL,NULL);
			pFile->Skip(size);
		}
		if ((version & 0xFF) >= 1) 
		{ //revision 1 or greater
			pFile->Read(_locked_machine_index);
			pFile->Read(_locked_to_machine);
		}

	}
}

void Instrument::SaveFileChunk(RiffFile* pFile)
{
	pFile->Write(_loop);
	pFile->Write(_lines);
	pFile->Write(_NNA);

	pFile->Write(ENV_AT);
	pFile->Write(ENV_DT);
	pFile->Write(ENV_SL);
	pFile->Write(ENV_RT);
	
	pFile->Write(ENV_F_AT);
	pFile->Write(ENV_F_DT);
	pFile->Write(ENV_F_SL);
	pFile->Write(ENV_F_RT);

	pFile->Write(ENV_F_CO);
	pFile->Write(ENV_F_RQ);
	pFile->Write(ENV_F_EA);
	pFile->Write(ENV_F_TP);
	{ int val; val = ENV_F_TP; pFile->Write(val); }

	pFile->Write(_pan);
	pFile->Write(_RPAN);
	pFile->Write(_RCUT);
	pFile->Write(_RRES);

	pFile->WriteArray(_sName, std::strlen(_sName) + 1);

	// now we have to write out the waves, but only if valid

	int numwaves = (waveLength > 0) ? 1 : 0; // The sampler has never supported more than one sample per instrument, even when the GUI did.

	pFile->Write(numwaves);
	if (waveLength > 0)
	{
		uint8_t * pData1(0);
		uint8_t * pData2(0);
		uint32_t size1=0,size2=0;
		size1 = DataCompression::SoundSquash(waveDataL,&pData1,waveLength);
		if (waveStereo)
		{
			size2 = DataCompression::SoundSquash(waveDataR,&pData2,waveLength);
		}

		uint32_t index = 0;
		pFile->WriteArray("WAVE",4);
		uint32_t version = CURRENT_FILE_VERSION_WAVE;
		uint32_t size =
			sizeof index +
			sizeof waveLength +
			sizeof waveVolume +
			sizeof waveLoopStart +
			sizeof waveLoopEnd +
			sizeof waveTune +
			sizeof waveFinetune +
			sizeof waveStereo +
			std::strlen(waveName) + 1 +
			size1 +
			size2;

		pFile->Write(version);
		pFile->Write(size);
		pFile->Write(index);

		pFile->Write(waveLength);
		pFile->Write(waveVolume);
		pFile->Write(waveLoopStart);
		pFile->Write(waveLoopEnd);

		pFile->Write(waveTune);
		pFile->Write(waveFinetune);
		pFile->Write(waveLoopType);
		pFile->Write(waveStereo);

		pFile->WriteArray(waveName, std::strlen(waveName) + 1);

		pFile->Write(size1);
		pFile->WriteArray(pData1,size1);
		delete[] pData1;
		if (waveStereo)
		{
			pFile->Write(size2);
			pFile->WriteArray(pData2,size2);
		}
		delete[] pData2;
	}
	pFile->Write(_locked_machine_index);
	pFile->Write(_locked_to_machine);

}

std::string Instrument::toXml( ) const
{
std::cout << "loopStart" << waveLoopStart << std::endl;

	std::ostringstream xml;
	xml << "<instrument name='" << std::string(_sName) << "'>" << std::endl;

	xml << "<header bin='";
	xml << toHex(_loop);
	xml << toHex(_lines);      
	xml << toHex(_NNA);

	xml << toHex(ENV_AT);
	xml << toHex(ENV_DT);
	xml << toHex(ENV_SL);
	xml << toHex(ENV_RT);
	
	xml << toHex(ENV_F_AT);
	xml << toHex(ENV_F_DT);
	xml << toHex(ENV_F_SL);
	xml << toHex(ENV_F_RT);

	xml << toHex(ENV_F_CO);
	xml << toHex(ENV_F_RQ);
	xml << toHex(ENV_F_EA);
	xml << toHex(ENV_F_TP);

	xml << toHex(_pan);
	xml << toHex(_RPAN);
	xml << toHex(_RCUT);
	xml << toHex(_RRES);

	// now we have to write out the waves, but only if valid

	int numwaves = (waveLength > 0) ? 1 : 0; // The sampler has never supported more than one sample per instrument, even when the GUI did.

	xml << toHex(numwaves);
	
	xml << "'>" << std::endl;

	if (waveLength > 0)
	{
		uint8_t * pData1(0);
		uint8_t * pData2(0);
		uint32_t size1=0,size2=0;

		size1 = DataCompression::SoundSquash(waveDataL,&pData1,waveLength);
		if (waveStereo)
		{
			size2 = DataCompression::SoundSquash(waveDataR,&pData2,waveLength);
		}
		
		xml << "<wave name='" << waveName << "' bin='";
		
		xml << toHex(waveLength);
		xml << toHex(waveVolume);
		xml << toHex(waveLoopStart);
		xml << toHex(waveLoopEnd);
		
		xml << toHex(waveTune);
		xml << toHex(waveFinetune);
		xml << toHex(waveLoopType);
		xml << toHex(waveStereo);

		xml << "'>" << std::endl;

		xml << "<waveleft size='";
		xml << toHex(size1) <<"'>";
		
		xml << "<hex v='";
		for (uint32_t k = 0; k < size1; k++) {
			xml << toHex(pData1[k],2);
		}
		xml << "'/>" << std::endl;
		xml << "</waveleft>" << std::endl;
		delete[] pData1;
		if (waveStereo)
		{
			xml << "<waveright size='";
			xml << toHex(size2) <<"'>";
		
			xml << "<hex v='";
			for (uint32_t k = 0; k < size2; k++) {
				xml << toHex(pData2[k], 2);
			}
			xml << "'/>" << std::endl;
			xml << std::endl;
			xml << "</waveright>" << std::endl;
		}
		delete[] pData2;
	}
	xml << "</wave>" << std::endl;
	xml << "</header>";
	xml << "</instrument>";

	return xml.str();
}

void Instrument::setName( const std::string & name )
{
	int size_count = 0;
	for (std::string::const_iterator it = name.begin(); size_count < 31 && it != name.end(); it++, size_count++) {
		_sName[size_count] = *it;
	}
	_sName[size_count++] = '\0';
}

void Instrument::createWavHeader( const std::string & name, const std::string & header )
{
	int size_count = 0;
	for (std::string::const_iterator it = name.begin(); size_count < 31 && it != name.end(); it++, size_count++) {
		waveName[size_count] = *it;
	}
	waveName[size_count++] = '\0';

	int pos = 0;

	waveLength = str_hex<unsigned int> (header,pos++);
	std::cout << waveLength << std::endl;
	waveVolume = str_hex<int> (header,pos++);
	waveLoopStart = str_hex<unsigned int> (header,pos++);
	waveLoopEnd = str_hex<unsigned int> (header,pos++);
		
	waveTune = str_hex<int> (header,pos++);
	waveFinetune = str_hex<int> (header,pos++);
	waveLoopType = str_hex<int> (header,pos++);
	waveStereo = str_hex<int> (header,pos++);
}

void Instrument::setCompressedData( unsigned char * left, unsigned char * right )
{
	DataCompression::SoundDesquash( left , &waveDataL );
	if (waveStereo) {
		DataCompression::SoundDesquash( right, &waveDataR );
	}
}

void Instrument::getData( unsigned char * data, const std::string & dataStr )
{
	std::string::const_iterator it = dataStr.begin();
	int byte_pos = 0;
	while ( it != dataStr.end() ) {
		char hex[3];
		hex[0] = *it; ++it;
		hex[1] = *it; ++it;
		hex[2] = 0;
		data[byte_pos++] = str_hex<int> ( std::string(hex), 0);
	}
}

}}
