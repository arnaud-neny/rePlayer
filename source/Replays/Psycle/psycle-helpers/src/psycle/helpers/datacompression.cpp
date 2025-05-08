// This program is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// copyright 2003-2009 members of the psycle project http://psycle.sourceforge.net ; jeremy evers

///\brief implementation beerz77-2 algorithm.
///
/// beerz77-2 algorithm by jeremy evers, loosely based on lz77 
/// -2 designates the smaller window, faster compression version
/// designed for decompression on gameboy advance
/// due to it's efficient decompression, it is usefull for many other things... like pattern data.
///
/// SoundSquash and SoundDesquash by jeremy evers
/// designed with speed in mind
/// simple, non adaptave delta predictor, less effective with high frequency content 
/// simple bit encoder
#include "datacompression.hpp"
#include <cstring>

namespace psycle { namespace helpers { namespace DataCompression {

namespace {
	uint8_t const MIN_REDUNDANT_BYTES_2 = 3;
	inline uint32_t ReadLittleEndian32(uint8_t const * ptr) { return (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0]; }
}

std::size_t BEERZ77Comp2(uint8_t const * pSource, uint8_t ** pDestination, std::size_t size) {
	assert(pSource);
	
	uint8_t * pDestPos = *pDestination = new uint8_t[size * 9 / 8 + 5]; // worst case
	std::memset(pDestPos, 0, size * 9 / 8 + 5);
	*pDestPos++ = static_cast<uint8_t>(0x04); // file version
	*pDestPos++ = static_cast<uint8_t>((size      ) & 0xff);
	*pDestPos++ = static_cast<uint8_t>((size >>  8) & 0xff);
	*pDestPos++ = static_cast<uint8_t>((size >> 16) & 0xff);
	*pDestPos++ = static_cast<uint8_t>((size >> 24) & 0xff);

	// we will compress pSource into pDest
	uint8_t const * pSlidingWindow = pSource;
	uint8_t const * pCurrentPos = pSource;
	uint8_t const * pTestPos;
	uint8_t       * pUncompressedCounter(0);

	while(pCurrentPos < pSource + size) {
		// update our sliding window
		pSlidingWindow = pCurrentPos - 0xff - MIN_REDUNDANT_BYTES_2; // maximum search offset
		// check for overflow!
		if(pSlidingWindow < pSource) pSlidingWindow = pSource;

		// check our current position against our sliding window
		uint8_t const * pBestMatch = pCurrentPos;
		int BestMatchLength = 0;

		// now to find the best match in our string
		for(pTestPos = pSlidingWindow; pTestPos < pCurrentPos - MIN_REDUNDANT_BYTES_2; ++pTestPos) {
			// check for a match
			if(*pTestPos == *pCurrentPos) {
				// set our pointers
				uint8_t const * pMatchPosEnd = pCurrentPos;
				uint8_t const * pMatchPosStart;
				for(pMatchPosStart = pTestPos; pMatchPosStart < pTestPos + 255 + MIN_REDUNDANT_BYTES_2; ++pMatchPosStart) {
					// check for pointer overflow
					if(pMatchPosStart >= pCurrentPos) {
						--pMatchPosStart;
						break;
					}
					// check for match
					else if(*pMatchPosStart == *pMatchPosEnd) {
						++pMatchPosEnd;
						if(pMatchPosEnd >= pSource + size) {
							--pMatchPosStart;
							break;
						}
					} else { // check for end of match
						// there is no match
						break;
					}
				}
				// check for best match
				if(pMatchPosStart - pTestPos > BestMatchLength) {
					BestMatchLength = pMatchPosStart - pTestPos;
					pBestMatch = pMatchPosStart;
				}
			}
		}
		
		// now to see what we need to write -
		// either a new byte or an offset/length to a string

		if(BestMatchLength >= MIN_REDUNDANT_BYTES_2) {
			// we write our string offset/length
			// write our flag
			*pDestPos++ = 0;
			// now we write our compression info
			// first, our LENGTH
			*pDestPos++ = BestMatchLength - MIN_REDUNDANT_BYTES_2;
			// second, our OFFSET
			uint16_t Output = pCurrentPos - pBestMatch;
			*pDestPos++ = Output & 0xff;
			// update the pointer
			pCurrentPos += BestMatchLength;
			pUncompressedCounter = 0;
		} else {
			BestMatchLength = 1;
			// we have an uncompressed byte
			// add it to our uncompressed string
			// if we have one
			if(pUncompressedCounter) {
				// it's cool, increment our counter
				*pUncompressedCounter += 1;
				// check for max string length
				if(*pUncompressedCounter == 255) pUncompressedCounter = 0;
			} else {
				// we need to start a new string
				pUncompressedCounter = pDestPos;
				// we write a byte
				// write our flag
				*pDestPos++ = 1;
			}
			// now we write our byte
			// and update the pointer
			*pDestPos++ = *pCurrentPos++;
		}
	}
	return pDestPos - *pDestination;
}

bool BEERZ77Decomp2(uint8_t const * pSourcePos, uint8_t ** pDestination) {
	assert(pSourcePos);
	if(*pSourcePos++ == 0x04) {
		// get file size
		// This is done byte by byte to avoid endianness issues
		int32_t FileSize = static_cast<int32_t>(ReadLittleEndian32(pSourcePos));

		pSourcePos += 4;

		//ok, now we can start decompressing
		uint8_t* pWindowPos;
		uint8_t* pDestPos = *pDestination = new uint8_t[FileSize];

		uint16_t Offset;
		uint16_t Length;

		while(FileSize > 0) {
			// get our flag
			if((Length = *pSourcePos++)) {
				// we have an unique string to copy
				std::memcpy(pDestPos, pSourcePos, Length);

				pSourcePos += Length;
				pDestPos += Length;
				FileSize -= Length;
			} else {
				// we have a redundancy
				// load length and offset
				Length  = (*pSourcePos++) + MIN_REDUNDANT_BYTES_2;
				Offset  = *pSourcePos++;

				pWindowPos = pDestPos - Offset - Length;
				std::memcpy(pDestPos, pWindowPos, Length);

				pDestPos += Length;
				FileSize -= Length;
			}
		}
		return true;
	}
	return false;
}

std::size_t SoundSquash(int16_t const * pSource, uint8_t ** pDestination, std::size_t size) {
	assert(pSource);
	
	uint8_t * pDestPos = *pDestination = new uint8_t[size * 12 / 4 + 5]; // worst case ; remember we use words of 2 bytes

	std::memset(pDestPos, 0, size * 12 / 4 + 5);
	*pDestPos++ = static_cast<uint8_t>(0x01); // file version
	*pDestPos++ = static_cast<uint8_t>((size      ) & 0xff);
	*pDestPos++ = static_cast<uint8_t>((size >>  8) & 0xff);
	*pDestPos++ = static_cast<uint8_t>((size >> 16) & 0xff);
	*pDestPos++ = static_cast<uint8_t>((size >> 24) & 0xff);

	// init predictor
	int16_t prevprev = 0;
	int16_t prev = 0;
	// init bitpos for encoder
	int bitpos = 0;

	while(size) {
		// predict that this sample should be last sample + (last sample - previous last sample)
		// and calculate the deviation from that as our error value to store
		int16_t t = *pSource++;
		int16_t error = (t - (prev + (prev - prevprev))) & 0xffff;
		// shuffle our previous values for next value
		prevprev = prev;
		prev = t;

		// encode our error value
		// using this format, low to high

		// 4 bits: # of bits to describe value (x)
		// 1 bit : sign bit
		// x bits: value

		// since we generally have error values that can be described in 8 bits or less,
		// this generally results in 13 or less bits being used to describe each value 
		// (rather than 16). often our values only require 8 or less bits.  that's how
		// we get lossless wave compression of over 50% in some cases.

		// other methods may be more efficient bitwise, but i doubt you will find much
		// with a better speed:savings ratio for our purposes

		int bits = 0;
		int data;

		// store info
		if(error & 0x8000) {
			// negative number
			if(!(error & 0x4000)) {
				bits = 15 + 5;
				data = 0x0f | 0x10 | ((error & 0x7fff) << 5);
			} else if(!(error & 0x2000)) {
				bits = 14 + 5;
				data = 0x0e | 0x10 | ((error & 0x3fff) << 5);
			} else if(!(error & 0x1000)) {
				bits = 13 + 5;
				data = 0x0d | 0x10 | ((error & 0x1fff) << 5);
			} else if(!(error & 0x0800)) {
				bits = 12 + 5;
				data = 0x0c | 0x10 | ((error & 0x0fff) << 5);
			} else if(!(error & 0x0400)) {
				bits = 11 + 5;
				data = 0x0b | 0x10 | ((error & 0x07ff) << 5);
			} else if(!(error & 0x0200)) {
				bits = 10 + 5;
				data = 0x0a | 0x10 | ((error & 0x03ff) << 5);
			} else if(!(error & 0x0100)) {
				bits = 9 + 5;
				data = 0x09 | 0x10 | ((error & 0x01ff) << 5);
			} else if (!(error & 0x0080)) {
				bits = 8 + 5;
				data = 0x08 | 0x10 | ((error & 0x00ff) << 5);
			} else if(!(error & 0x0040)) {
				bits = 7 + 5;
				data = 0x07 | 0x10 | ((error & 0x007f) << 5);
			} else if(!(error & 0x0020)) {
				bits = 6 + 5;
				data = 0x06 | 0x10 | ((error & 0x003f) << 5);
			} else if(!(error & 0x0010)) {
				bits = 5 + 5;
				data = 0x05 | 0x10 | ((error & 0x001f) << 5);
			} else if(!(error & 0x0008)) {
				bits = 4 + 5;
				data = 0x04 | 0x10 | ((error & 0x000f) << 5);
			} else if(!(error & 0x0004)) {
				bits = 3 + 5;
				data = 0x03 | 0x10 | ((error & 0x0007) << 5);
			} else if(!(error & 0x0002)) {
				bits = 2 + 5;
				data = 0x02 | 0x10 | ((error & 0x0003) << 5);
			} else if(!(error & 0x0001)) {
				bits = 1 + 5;
				data = 0x01 | 0x10 | ((error & 0x0001) << 5);
			} else {
				bits = 0 + 5;
				data = 0x00 | 0x10;
			}
		} else {
			// positive number
			if(error & 0x4000) {
				bits = 15 + 5;
				data = 0x0f | 0x00 | ((error & 0x7fff) << 5);
			} else if(error & 0x2000) {
				bits = 14 + 5;
				data = 0x0e | 0x00 | ((error & 0x3fff) << 5);
			} else if(error & 0x1000) {
				bits = 13 + 5;
				data = 0x0d | 0x00 | ((error & 0x1fff) << 5);
			} else if(error & 0x0800) {
				bits = 12 + 5;
				data = 0x0c | 0x00 | ((error & 0x0fff) << 5);
			} else if(error & 0x0400) {
				bits = 11 + 5;
				data = 0x0b | 0x00 | ((error & 0x07ff) << 5);
			} else if(error & 0x0200) {
				bits = 10 + 5;
				data = 0x0a | 0x00 | ((error & 0x03ff) << 5);
			} else if(error & 0x0100) {
				bits = 9 + 5;
				data = 0x09 | 0x00 | ((error & 0x01ff) << 5);
			} else if(error & 0x0080) {
				bits = 8 + 5;
				data = 0x08 | 0x00 | ((error & 0x00ff) << 5);
			} else if(error & 0x0040) {
				bits = 7 + 5;
				data = 0x07 | 0x00 | ((error & 0x007f) << 5);
			} else if(error & 0x0020) {
				bits = 6 + 5;
				data = 0x06 | 0x00 | ((error & 0x003f) << 5);
			} else if(error & 0x0010) {
				bits = 5 + 5;
				data = 0x05 | 0x00 | ((error & 0x001f) << 5);
			} else if(error & 0x0008) {
				bits = 4 + 5;
				data = 0x04 | 0x00 | ((error & 0x000f) << 5);
			} else if(error & 0x0004) {
				bits = 3 + 5;
				data = 0x03 | 0x00 | ((error & 0x0007) << 5);
			} else if(error & 0x0002) {
				bits = 2 + 5;
				data = 0x02 | 0x00 | ((error & 0x0003) << 5);
			} else if(error & 0x0001) {
				bits = 1 + 5;
				data = 0x01 | 0x00 | ((error & 0x0001) << 5);
			} else {
				bits = 0 + 5;
				data = 0x00 | 0x00;
			}
		}

		// ok so we know how many bits to store, and what those bits are.  so store 'em!

		data <<= bitpos; // shift our bits for storage

		// store our lsb, merging with existing bits

		*pDestPos = *pDestPos | (data & 0xff);
		++pDestPos;

		bits -= 8 - bitpos;

		// loop for all remaining bits
		while(bits > 0) {
			data >>= 8;
			*pDestPos++ = data & 0xff;
			bits -= 8;
		}
		// calculate what bit to merge at next time
		bitpos = (8 + bits) & 0x7;
		// rewind our pointer if we ended mid-byte
		if(bitpos) --pDestPos;
		// let's do it again, it was fun!
		--size;
	}
	// remember to count that last half-written byte
	if(bitpos) ++pDestPos;
	return pDestPos - *pDestination;
}

bool SoundDesquash(uint8_t const * pSourcePos, int16_t ** pDestination) {
	assert(pSourcePos);
	// check validity of data
	if(*pSourcePos++ == 0x01) { // version 1 is pretty simple
		const int16_t mask[16] = {
			0x0000,
			0x0001,
			0x0003,
			0x0007,
			0x000f,
			0x001f,
			0x003f,
			0x007f,
			0x00ff,
			0x01ff,
			0x03ff,
			0x07ff,
			0x0fff,
			0x1fff,
			0x3fff,
			0x7fff
		};

		// get file size
		// this is done byte-by-byte to avoid endianness issues
		uint32_t FileSize = ReadLittleEndian32(pSourcePos);
		pSourcePos += 4;
		
		//ok, now we can start decompressing
		int16_t * pDestPos = *pDestination = new int16_t[FileSize];

		// init our predictor
		int16_t prevprev = 0;
		int16_t prev = 0;
		// bit counter for decoder
		int bitpos = 0;

		while(FileSize) {
			// read a full uint32_t. in our worst case we will need 7 + 5 + 15 bits, 27, which is easily contained in 32 bits.
			uint32_t bits = ReadLittleEndian32(pSourcePos);
			// note, we do not increment pSourcePos.

			// now shift for our bit position to get the next bit we require

			bits >>= bitpos;
			// low 4 bits are number of valid bits count
			int numbits = bits & 0x0f;
			// next bit is the sign flag
			uint32_t sign = bits & 0x010;
			// the remaining bits are our value
			bits >>= 5;

			// mask out only the bits that are relevant
			int16_t error = static_cast<int16_t>(bits & mask[numbits]);

			// check for negative values
			if(sign) {
				// we need to convert to negative
				int16_t error2 = (0xffff << numbits) & 0xffff;
				error |= error2;
			}

			// and then apply our error value to the prediction
			// sample = last + (last - prev last)

			int16_t t = ((prev + (prev - prevprev)) + error) & 0xffff;
			// store our sample
			*pDestPos++ = t;
			// shuffle our previous values for next value
			prevprev = prev;
			prev = t;

			// and shift our read position to the next value in the stream
			bitpos += numbits + 5;
			while(bitpos >= 8) {
				bitpos -= 8;
				++pSourcePos;
			}

			// and let's do it again!
			--FileSize;
		}
		return true;
	}
	return false;
}

}}}
