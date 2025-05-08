
/**********************************************************************************************
	Copyright 2007-2008 members of the psycle project http://psycle.sourceforge.net

	This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**********************************************************************************************/

#include "riff.hpp"
namespace psycle { namespace helpers {

uint32_t FourCC(const char * ChunkName) {
	int32_t retbuf = 0x20202020;   // four spaces (padding)
	char *p = ((char *)&retbuf);
	// Remember, this is Intel format!
	// The first character goes in the LSB
	for(int i(0) ; i < 4 && ChunkName[i]; ++i) *p++ = ChunkName[i];
	return retbuf;
}

ExtRiffFile::ExtRiffFile() {
	file = 0;
	fmode = RFM_UNKNOWN;
	ExtRiff_header.ckID = FourCC("RIFF");
	ExtRiff_header.ckSize = 0;
}

ExtRiffFile::~ExtRiffFile() {
	if(fmode != RFM_UNKNOWN) Close();
}

DDCRET ExtRiffFile::Open(const char * Filename, ExtRiffFileMode NewMode) {
	DDCRET retcode = DDC_SUCCESS;
	if(fmode != RFM_UNKNOWN) retcode = Close();
	if(retcode == DDC_SUCCESS) {
		switch(NewMode) {
			case RFM_WRITE:
				file = std::fopen(Filename, "wb");
				if(file) {
					// Write the ExtRiff header...
					// We will have to come back later and patch it!
					ExtRiff_header.ckID = FourCC("RIFF");
					ExtRiff_header.ckSize = 0;
					if(std::fwrite( &ExtRiff_header, sizeof(ExtRiff_header), 1, file) != 1) {
						std::fclose(file);
						remove(Filename);
						fmode = RFM_UNKNOWN;
						file = 0;
						retcode = DDC_FILE_ERROR;
					} else fmode = RFM_WRITE;
				} else {
					fmode = RFM_UNKNOWN;
					retcode = DDC_FILE_ERROR;
				}
				break;
			case RFM_READ:
				file = std::fopen(Filename, "rb");
				if(file) {
					// Try to read the ExtRiff header...
					if(std::fread(&ExtRiff_header, sizeof(ExtRiff_header), 1, file) != 1) {
						std::fclose(file);
						fmode = RFM_UNKNOWN;
						file = 0;
						retcode = DDC_FILE_ERROR;
					}
					else fmode = RFM_READ;
				} else {
					fmode = RFM_UNKNOWN;
					retcode = DDC_FILE_ERROR;
				}
				break;
			default:
				retcode = DDC_INVALID_CALL;
		}
	}
	return retcode;
}

DDCRET ExtRiffFile::Write(const void * Data, uint32_t NumBytes) {
	if(fmode != RFM_WRITE) return DDC_INVALID_CALL;
	if(std::fwrite(Data, NumBytes, 1, file) != 1) return DDC_FILE_ERROR;
	ExtRiff_header.ckSize += NumBytes;
	return DDC_SUCCESS;
}

DDCRET ExtRiffFile::Close() {
	DDCRET retcode = DDC_SUCCESS;
	switch(fmode) {
		case RFM_WRITE:
			if(
				std::fflush(file) ||
				std::fseek(file, 0, SEEK_SET) ||
				std::fwrite(&ExtRiff_header, sizeof(ExtRiff_header), 1, file) != 1 ||
				std::fclose(file)
			) retcode = DDC_FILE_ERROR;
			break;
		case RFM_READ:
			std::fclose(file);
			break;
		default:
			throw std::runtime_error("bad mode");
	}
	file = 0;
	fmode = RFM_UNKNOWN;
	return retcode;
}

int32_t ExtRiffFile::CurrentFilePosition() const {
	return std::ftell(file);
}

DDCRET ExtRiffFile::Seek(int32_t offset) {
	std::fflush(file);
	DDCRET rc;
	if(std::fseek(file, offset, SEEK_SET)) rc = DDC_FILE_ERROR;
	else rc = DDC_SUCCESS;
	return rc;
}


DDCRET ExtRiffFile::Backpatch(int32_t FileOffset, const void * Data, uint32_t NumBytes) {
	if(!file || fmode != RFM_WRITE) return DDC_INVALID_CALL;
	if(
		std::fflush(file) ||
		std::fseek(file, FileOffset, SEEK_SET) ||
		std::fwrite(Data, NumBytes, 1, file) != 1
	) return DDC_FILE_ERROR;
	return DDC_SUCCESS;
}

DDCRET ExtRiffFile::Expect(const void * Data, uint32_t NumBytes) {
	char *p = (char*) Data;
	while(NumBytes--) if(std::fgetc(file) != *p++) return DDC_FILE_ERROR;
	return DDC_SUCCESS;
}

DDCRET ExtRiffFile::Skip(int32_t NumBytes) {
	//std::fflush(file);
	DDCRET rc;
	if(std::fseek(file, NumBytes, SEEK_CUR)) rc = DDC_FILE_ERROR;
	else rc = DDC_SUCCESS;
	return rc;
}

DDCRET ExtRiffFile::Read(void * Data, uint32_t NumBytes) {
	DDCRET retcode = DDC_SUCCESS;
	if(std::fread(Data, NumBytes, 1, file) != 1) retcode = DDC_FILE_ERROR;
	return retcode;
}

/*****************************************************************************************/
// WaveFile

WaveFile::WaveFile() {
	pcm_data.ckID = FourCC("data");
	pcm_data.ckSize = 0;
	num_samples = 0;
}

// Modified by [JAZ]
DDCRET WaveFile::OpenForRead(const char * Filename) {
	// Verify filename parameter as best we can...
	if(!Filename) return DDC_INVALID_CALL;
	DDCRET retcode = Open(Filename, RFM_READ);
	if(retcode == DDC_SUCCESS) {
		retcode = Expect("WAVE", 4);
		if(retcode == DDC_SUCCESS) {
			retcode = Read(&wave_format.header, sizeof wave_format.header);
			while(wave_format.header.ckID != FourCC("fmt ")) { 
				//Skip (wave_format.header.ckSize);// read each block until we find the correct one
				// we didn't find our header, so move back and try again
				Skip(1 - static_cast<uint32_t>(sizeof wave_format.header));
				retcode = Read(&wave_format.header, sizeof wave_format.header);
				if(retcode != DDC_SUCCESS) return retcode;
			}
			retcode = Read(&wave_format.data, sizeof wave_format.data);
			if(!wave_format.VerifyValidity()) {
				// This isn't standard PCM, so we don't know what it is!
				retcode = DDC_FILE_ERROR;
			}
			if(retcode == DDC_SUCCESS) {
				// Figure out number of samples from
				// file size, current file position, and
				// WAVE header.
				pcm_data_offset = CurrentFilePosition();
				retcode = Read(&pcm_data, sizeof pcm_data);
				while(pcm_data.ckID != FourCC("data") || pcm_data.ckSize == 0) {
					// Skip (pcm_data.ckSize);// read each block until we find the correct one
					// this is not our block, so move back and search for our header
					Skip(1 - static_cast<uint32_t>(sizeof pcm_data));
					pcm_data_offset = CurrentFilePosition();
					retcode = Read(&pcm_data, sizeof pcm_data);
					if(retcode != DDC_SUCCESS) return retcode;
				}
				num_samples = pcm_data.ckSize;
				//num_samples = filelength(fileno(file)) - CurrentFilePosition();
				num_samples /= NumChannels();
				num_samples /= BitsPerSample() / 8;
			}
		}
	}
	return retcode;
}

DDCRET WaveFile::OpenForWrite(const char * Filename, uint32_t SamplingRate, uint16_t BitsPerSample, uint16_t NumChannels, bool isFloat) {
	// Verify parameters...
	if(
		!Filename ||
		(BitsPerSample != 8 && BitsPerSample != 16 && BitsPerSample != 24 && BitsPerSample != 32) ||
		NumChannels < 1 || NumChannels > 2
	) return DDC_INVALID_CALL;
	wave_format.data.Config(SamplingRate, BitsPerSample, NumChannels, isFloat);
	DDCRET retcode = Open(Filename, RFM_WRITE);
	if(retcode == DDC_SUCCESS) {
		retcode = Write("WAVE", 4);
		if(retcode == DDC_SUCCESS) {
			retcode = Write(&wave_format, sizeof wave_format);
			if(retcode == DDC_SUCCESS) {
				pcm_data.ckSize = 0;
				pcm_data_offset = CurrentFilePosition();
				retcode = Write(&pcm_data, sizeof pcm_data);
			}
		}
	}
	return retcode;
}

DDCRET WaveFile::Close() {
	DDCRET rc = DDC_SUCCESS;
	if(fmode == RFM_WRITE) rc = Backpatch(pcm_data_offset, &pcm_data, sizeof pcm_data);
	if(rc == DDC_SUCCESS && fmode != RFM_UNKNOWN) rc = ExtRiffFile::Close();
	return rc;
}

DDCRET WaveFile::WriteSample(const int16_t Sample[MAX_WAVE_CHANNELS]) {
	DDCRET retcode = DDC_SUCCESS;
	switch(wave_format.data.nChannels) {
		case 1:
			switch( wave_format.data.nBitsPerSample) {
				case 8:
					pcm_data.ckSize += 1;
					retcode = Write(&Sample[0], 1);
					break;

				case 16:
					pcm_data.ckSize += 2;
					retcode = Write(&Sample[0], 2);
					break;

				default: retcode = DDC_INVALID_CALL;
			}
			break;
		case 2:
			switch( wave_format.data.nBitsPerSample) {
				case 8:
					retcode = Write(&Sample[0], 1);
					if(retcode == DDC_SUCCESS) {
						retcode = Write(&Sample[1], 1);
						if(retcode == DDC_SUCCESS) pcm_data.ckSize += 2;
					}
					break;
				case 16:
					retcode = Write (&Sample[0], 2);
					if(retcode == DDC_SUCCESS) {
						retcode = Write(&Sample[1], 2);
						if(retcode == DDC_SUCCESS) pcm_data.ckSize += 4;
					}
					break;
				default: retcode = DDC_INVALID_CALL;
			}
			break;
		default: retcode = DDC_INVALID_CALL;
	}
	return retcode;
}

DDCRET WaveFile::WriteMonoSample(float SampleData) {
	int32_t d;

	switch( wave_format.data.wFormatTag )
	{
	case 1: // Integer PCM
		if(SampleData > 32767.0f) SampleData = 32767.0f;
		else if(SampleData < -32768.0f) SampleData = -32768.0f;
		switch( wave_format.data.nBitsPerSample) {
		case 8:
			pcm_data.ckSize += 1;
			d = int32_t(SampleData / 256.0f);
			d += 128;
			return Write(&d, 1);
		case 16:
			pcm_data.ckSize += 2;
			d = int32_t(SampleData);
			return Write(&d, 2);
		case 24:
			pcm_data.ckSize += 3;
			d = int32_t(SampleData * 256.0f);
			return Write(&d, 3 );
		case 32:
			pcm_data.ckSize += 4;
			d = int32_t(SampleData * 65536.0f);
			return Write(&SampleData, 4);
		default:
			break;
		}
		break;
	case 3: // IEEE float PCM
		if( wave_format.data.nBitsPerSample == 32)
		{
			pcm_data.ckSize += 4;
			const float f = SampleData * 0.000030517578125f;
			return Write ( &f, 4 );
		}
	default:
		break;
	}
	return DDC_INVALID_CALL;
}

DDCRET WaveFile::WriteStereoSample(float LeftSample, float RightSample) {
	DDCRET retcode = DDC_SUCCESS;
	int32_t l, r;
	float f;
	switch( wave_format.data.wFormatTag )
	{
	case 1: // Integer PCM
		if(LeftSample > 32767.0f) LeftSample = 32767.0f;
		else if(LeftSample < -32768.0f) LeftSample = -32768.0f;
		if(RightSample > 32767.0f) RightSample = 32767.0f;
		else if(RightSample < -32768.0f) RightSample = -32768.0f;
		switch(wave_format.data.nBitsPerSample) {
		case 8:
			l = int32_t(LeftSample / 256.0f);
			r = int32_t(RightSample / 256.0f);
			l += 128;
			r += 128;
			retcode = Write(&l, 1);
			if(retcode == DDC_SUCCESS) {
				retcode = Write(&r, 1);
				if(retcode == DDC_SUCCESS) pcm_data.ckSize += 2;
			}
			break;
		case 16:
			l = int32_t(LeftSample);
			r = int32_t(RightSample);
			retcode = Write(&l, 2);
			if(retcode == DDC_SUCCESS) {
				retcode = Write(&r, 2);
				if(retcode == DDC_SUCCESS) pcm_data.ckSize += 4;
			}
			break;
		case 24:
			l = int32_t(LeftSample * 256.0f);
			r = int32_t(RightSample * 256.0f);
			retcode = Write(&l, 3);
			if(retcode == DDC_SUCCESS) {
				retcode = Write(&r, 3);
				if(retcode == DDC_SUCCESS) pcm_data.ckSize += 6;
			}
			break;
		case 32:
			l = int32_t(LeftSample * 65536.0f);
			r = int32_t(RightSample * 65536.0f);
			retcode = Write(&l, 4);
			if(retcode == DDC_SUCCESS) {
				retcode = Write(&r, 4);
				if(retcode == DDC_SUCCESS) pcm_data.ckSize += 8;
			}
			break;
		default:
			retcode = DDC_INVALID_CALL;
		}
		break;
	case 3: // IEEE float PCM
		if( wave_format.data.nBitsPerSample == 32)
		{
			f = LeftSample * 0.000030517578125f;
			retcode = Write ( &f, 4 );
			if(retcode == DDC_SUCCESS)
			{
				f = RightSample * 0.000030517578125f;
				retcode = Write ( &f, 4 );
				if( retcode == DDC_SUCCESS) pcm_data.ckSize += 8;
			}
		}
	default:
		break;
	}
	return retcode;
}

DDCRET WaveFile::ReadSample(int16_t Sample[MAX_WAVE_CHANNELS]) {
	DDCRET retcode = DDC_SUCCESS;
	switch(wave_format.data.nChannels) {
		case 1:
			switch(wave_format.data.nBitsPerSample) {
				case 8:
					uint8_t x;
					retcode = Read(&x, 1);
					Sample[0] = int16_t(x);
					break;
				case 16:
					retcode = Read(&Sample[0], 2);
					break;
				default: retcode = DDC_INVALID_CALL;
			}
			break;
		case 2:
			switch(wave_format.data.nBitsPerSample) {
				case 8:
					uint8_t x[2];
					retcode = Read(x, 2);
					Sample[0] = int16_t(x[0]);
					Sample[1] = int16_t(x[1]);
					break;
				case 16:
					retcode = Read(Sample, 4);
					break;
				default: retcode = DDC_INVALID_CALL;
			}
			break;
		default: retcode = DDC_INVALID_CALL;
	}
	return retcode;
}


DDCRET WaveFile::ReadSamples(uint32_t num, WaveFileSample sarray[]) {
	DDCRET retcode = DDC_SUCCESS;
	switch(wave_format.data.nChannels) {
		case 1:
			switch(wave_format.data.nBitsPerSample) {
				case 8:
					for(uint32_t i(0) ; i < num && retcode == DDC_SUCCESS; ++i) {
						uint8_t x;
						retcode = Read(&x, 1);
						sarray[i].chan[0] = int16_t(x);
					}
					break;
				case 16:
					for(uint32_t i(0); i < num && retcode == DDC_SUCCESS; ++i) {
						retcode = Read(&sarray[i].chan[0], 2);
					}
					break;

				default: retcode = DDC_INVALID_CALL;
			}
			break;
		case 2:
			switch(wave_format.data.nBitsPerSample) {
				case 8:
					for(uint32_t i(0); i < num && retcode == DDC_SUCCESS; ++i) {
						uint8_t x[2];
						retcode = Read(x, 2);
						sarray[i].chan[0] = int16_t(x[0]);
						sarray[i].chan[1] = int16_t(x[1]);
					}
					break;
				case 16:
					retcode = Read(sarray, 4 * num);
					break;
				default: retcode = DDC_INVALID_CALL;
			}
			break;
		default: retcode = DDC_INVALID_CALL;
	}
	return retcode;
}

DDCRET WaveFile::ReadMonoSample(int16_t * Sample) {
	DDCRET retcode = DDC_SUCCESS;
	switch(wave_format.data.nBitsPerSample) {
		case 8:
			uint8_t x;
			retcode = Read(&x, 1);
			*Sample = int16_t(x);
			break;
		case 16:
			retcode = Read(Sample, 2);
			break;
		default: retcode = DDC_INVALID_CALL;
	}
	return retcode;
}

DDCRET WaveFile::ReadStereoSample(int16_t * L, int16_t * R) {
	DDCRET retcode = DDC_SUCCESS;
	int8_t x[2];
	int16_t y[2];
	switch(wave_format.data.nBitsPerSample) {
		case 8:
			retcode = Read(x, 2);
			*L = int16_t(x[0]);
			*R = int16_t(x[1]);
			break;
		case 16:
			retcode = Read(y, 4);
			*L = int16_t(y[0]);
			*R = int16_t(y[1]);
			break;
		default: retcode = DDC_INVALID_CALL;
	}
	return retcode;
}

DDCRET WaveFile::SeekToSample(uint32_t SampleIndex) {
	if( SampleIndex >= NumSamples()) return DDC_INVALID_CALL;
	uint32_t SampleSize = (BitsPerSample() + 7) / 8;
	DDCRET rc = Seek(pcm_data_offset + sizeof(pcm_data) + SampleSize * NumChannels() * SampleIndex);
	return rc;
}

uint32_t WaveFile::SamplingRate() const {
	return wave_format.data.nSamplesPerSec;
}

uint16_t WaveFile::BitsPerSample() const {
	return wave_format.data.nBitsPerSample;
}

uint16_t WaveFile::NumChannels() const {
	return wave_format.data.nChannels;
}

uint32_t WaveFile::NumSamples() const {
	return num_samples;
}

DDCRET WaveFile::WriteData(const int16_t * data, uint32_t numData) {
	uint32_t extraBytes = numData * sizeof *data;
	pcm_data.ckSize += extraBytes;
	return ExtRiffFile::Write(data, extraBytes);
}

DDCRET WaveFile::WriteData(const uint8_t * data, uint32_t numData) {
	pcm_data.ckSize += numData;
	return ExtRiffFile::Write(data, numData);
}

DDCRET WaveFile::ReadData(int16_t * data, uint32_t numData) {
	return ExtRiffFile::Read(data, numData * sizeof *data);
}

DDCRET WaveFile::ReadData(uint8_t * data, uint32_t numData) {
	return ExtRiffFile::Read(data, numData);
}

}}
