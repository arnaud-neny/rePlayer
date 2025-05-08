
/**********************************************************************************************
	Copyright 2007-2008 members of the psycle project http://psycle.sourceforge.net

	This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**********************************************************************************************/

#pragma once
#include <universalis.hpp>
#include <cstdio>
namespace psycle { namespace helpers {

using namespace universalis::stdlib;

extern uint32_t FourCC(const char * ChunkName);

enum DDCRET {
	DDC_SUCCESS,           ///< operation succeded
	DDC_FAILURE,           ///< operation failed for unspecified reasons
	DDC_OUT_OF_MEMORY,     ///< operation failed due to running out of memory
	DDC_FILE_ERROR,        ///< operation encountered file I/O error
	DDC_INVALID_CALL,      ///< operation was called with invalid parameters
	DDC_USER_ABORT,        ///< operation was aborted by the user
	DDC_INVALID_FILE       ///< file format does not match
};

uint32_t FourCC(const char * ChunkName);

enum ExtRiffFileMode {
	RFM_UNKNOWN, ///< undefined type (can use to mean "N/A" or "not open")
	RFM_WRITE, ///< open for write
	RFM_READ ///< open for read
};

class ExtRiffChunkHeader {
	public:
		/// Four-character chunk ID
		uint32_t ckID;       
		/// Length of data in chunk
		uint32_t ckSize;     
};

/// riff file format.
//UNIVERSALIS__COMPILER__DEPRECATED("c++ iostream for the better")
class ExtRiffFile {
	private:
		/// header for whole file
		ExtRiffChunkHeader   ExtRiff_header;      
	protected:
		/// current file I/O mode
		ExtRiffFileMode      fmode;
		/// I/O stream to use
		FILE             *file;
		DDCRET Seek(int32_t offset);
	public:
		ExtRiffFile();
		virtual ~ExtRiffFile();
		ExtRiffFileMode CurrentFileMode() const { return fmode; }
		DDCRET Open(const char * Filename, ExtRiffFileMode NewMode);
		DDCRET Close();

		int32_t CurrentFilePosition() const;

		template<typename X>
		DDCRET inline Read(X & x) { return Read(&x, sizeof x); }
		template<typename X>
		DDCRET inline Write(X const & x) { return Write(&x, sizeof x); }

		DDCRET Read(void *, uint32_t bytes); // Remember to fix endian if needed when you call this
		DDCRET Write(void const *, uint32_t bytes); // Remember to fix endian if needed when you call this
		DDCRET Expect(void const *, uint32_t bytes); // Remember to fix endian if needed when you call this

		/// Added by [JAZ]
		DDCRET Skip(int32_t NumBytes);

		DDCRET Backpatch(int32_t FileOffset, const void * Data, uint32_t NumBytes); // Remember to fix endian if needed when you call this
};

class WaveFormat_ChunkData {
	public:
		/// Format category (PCM=1)
		uint16_t wFormatTag;       
		/// Number of channels (mono=1, stereo=2)
		uint16_t nChannels;        
		/// Sampling rate [Hz]
		uint32_t nSamplesPerSec;   
		uint32_t nAvgBytesPerSec;
		uint16_t nBlockAlign;
		uint16_t nBitsPerSample;
		void Config(uint32_t NewSamplingRate = 44100, uint16_t NewBitsPerSample = 16, uint16_t NewNumChannels = 2, bool isFloat = false) {
			if (isFloat) {
				wFormatTag = 3; // IEEE float
			}
			else {
				wFormatTag = 1; // PCM
			}
			nSamplesPerSec = NewSamplingRate;
			nChannels = NewNumChannels;
			nBitsPerSample = NewBitsPerSample;
			nAvgBytesPerSec = nChannels * nSamplesPerSec * nBitsPerSample / 8;
			nBlockAlign = nChannels * nBitsPerSample / 8;
		}
		WaveFormat_ChunkData() {
			Config();
		}
};


class WaveFormat_Chunk {
	public:
		ExtRiffChunkHeader header;
		WaveFormat_ChunkData data;
		WaveFormat_Chunk() {
			header.ckID = FourCC("fmt");
			header.ckSize = sizeof(WaveFormat_ChunkData);
		}
		bool VerifyValidity() {
			return
				header.ckID == FourCC("fmt") &&
				(data.nChannels == 1 || data.nChannels == 2) &&
				data.nAvgBytesPerSec == data.nChannels * data.nSamplesPerSec * data.nBitsPerSample / 8 &&
				data.nBlockAlign == data.nChannels * data.nBitsPerSample / 8;
		}
};

uint16_t const MAX_WAVE_CHANNELS = 2;

class WaveFileSample {
	public:
		int16_t chan[MAX_WAVE_CHANNELS];
};


/// riff wave file format.
/// MODIFIED BY [JAZ]. It was "private ExtRiffFile".
//UNIVERSALIS__COMPILER__DEPRECATED("c++ iostream for the better")
class WaveFile: public ExtRiffFile {
	WaveFormat_Chunk wave_format;
	ExtRiffChunkHeader pcm_data;
	/// offset of 'pcm_data' in output file
	uint32_t pcm_data_offset;
	uint32_t num_samples;

	public:
	WaveFile();

	DDCRET OpenForWrite(const char * Filename,
		uint32_t SamplingRate = 44100,
		uint16_t BitsPerSample = 16,
		uint16_t NumChannels = 2,
		bool isFloat = false);

	DDCRET OpenForRead(const char * Filename);

	DDCRET ReadSample(int16_t Sample[MAX_WAVE_CHANNELS]);
	DDCRET WriteSample(const int16_t Sample[MAX_WAVE_CHANNELS]);
	DDCRET SeekToSample(uint32_t SampleIndex);

	/// work only with 16-bit audio
	DDCRET WriteData(const int16_t * data, uint32_t numData);
	/// work only with 16-bit audio
	DDCRET ReadData(int16_t * data, uint32_t numData);

	/// work only with unsigned 8-bit audio
	DDCRET WriteData(const uint8_t * data, uint32_t numData);
	/// work only with unsigned 8-bit audio
	DDCRET ReadData(uint8_t * data, uint32_t numData);

	DDCRET ReadSamples(uint32_t num, WaveFileSample[]);

	DDCRET WriteMonoSample(float ChannelData);
	DDCRET WriteStereoSample(float LeftChannelData, float RightChannelData);

	DDCRET ReadMonoSample(int16_t * ChannelData);
	DDCRET ReadStereoSample(int16_t * LeftSampleData, int16_t * RightSampleData);

	DDCRET Close();

	/// [Hz]
	uint32_t SamplingRate() const;
	uint16_t BitsPerSample() const;
	uint16_t NumChannels() const;
	uint32_t NumSamples() const;

	/// Open for write using another wave file's parameters...
	DDCRET OpenForWrite(const char * Filename, WaveFile & OtherWave) {
		return OpenForWrite(Filename, OtherWave.SamplingRate(), OtherWave.BitsPerSample(), OtherWave.NumChannels());
	}

	int32_t CurrentFilePosition() const {
		return ExtRiffFile::CurrentFilePosition();
	}
};

}}
