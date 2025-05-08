// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

///\interface psycle::core::RiffFile

#ifndef PSYCLE__CORE__FILE_IO__INCLUDED
#define PSYCLE__CORE__FILE_IO__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

#include <limits>
#include <cstdio>
#include <cstddef>
#include <fstream>
#include <vector>

namespace psycle { namespace core {

class RiffChunkHeader {
	public:
		/// chunk type identifier.
		/// 4-character code.
		char id[4];

		/// size of the chunk in bytes.
		/// little endian for RIFF files ; big endian for RIFX files.
		uint32_t size;
};

/// riff file format.
/// RIFF has a counterpart, RIFX, that is used to define RIFF file formats
/// that use the Motorola integer byte-ordering format rather than the Intel format.
/// A RIFX file is the same as a RIFF file, except that the first four bytes are "RIFX" instead of "RIFF",
/// and integer byte ordering is represented in Motorola format.
class PSYCLE__CORE__DECL RiffFile {
	public:
		std::string const inline & file_name() const throw() { return file_name_; }
	private:
		std::string file_name_;

	public:
		virtual ~RiffFile() {};

		///\todo shouldn't be public
		RiffChunkHeader _header;

		bool Open  (std::string const &);
		bool Create(std::string const &, bool overwrite);
		void Close();
		bool Error();
		bool Eof();
		virtual std::size_t FileSize();
		virtual std::size_t GetPos();
		virtual std::size_t Seek(std::ptrdiff_t bytes);
		virtual std::size_t Skip(std::ptrdiff_t bytes);

	protected:
		virtual bool WriteChunk(void const *, std::size_t);
		virtual bool ReadChunk (void       *, std::size_t);
		virtual bool Expect    (void       *, std::size_t);

	public:
		///\name 1 bit
		///\{
			bool Read(bool & x) {
				uint8_t c;
				if (!Read(c)) return false;
				x = c;
				return true;
			}

			bool Write(bool x) { uint8_t c = x; return Write(c); }
		///\}

		///\name 8 bits
		///\{
			bool Read(uint8_t & x) { return ReadChunk(&x,1); }
			bool Read(int8_t & x) { return ReadChunk(&x,1); }
			bool Read(char & x) { return ReadChunk(&x,1); } // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type

			bool Write(uint8_t x) { return WriteChunk(&x,1); }
			bool Write(int8_t x) { return WriteChunk(&x,1);  }
			bool Write(char x) { return WriteChunk(&x,1);  } // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type
		///\}

		///\name 16 bits
		///\{
			bool Read(uint16_t & x) {
				uint8_t data[2];
				if(!ReadChunk(data,2)) return false;
				x = (data[1])<<8 | data[0];
				return true;
			}

			bool Read(int16_t & x) { return Read(reinterpret_cast<uint16_t&>(x)); }

			bool ReadBE(uint16_t & x) {
				uint8_t data[2];
				if(!ReadChunk(data,2)) return false;
				x = (data[0])<<8 | data[1];
				return true;
			}

			bool ReadBE(int16_t & x) { return ReadBE(reinterpret_cast<uint16_t&>(x)); }

			bool Write(uint16_t x) {
				uint8_t data[2] = {
					static_cast<uint8_t>(x & 0xff),
					static_cast<uint8_t>((x >> 8) & 0xff)
				};
				return WriteChunk(data, 2);
			}
			bool Write(int16_t x) { return Write(reinterpret_cast<uint16_t&>(x)); }
		///\}

		///\name 32 bits
		///\{
			bool Read(uint32_t & x) {
				uint8_t data[4];
				if(!ReadChunk(data,4)) return false;
				x = (data[3]<<24) | (data[2]<<16) | (data[1]<<8) | data[0];
				return true;
			}

			bool Read(int32_t & x) { return Read(reinterpret_cast<uint32_t&>(x)); }

			bool ReadBE(uint32_t & x) {
				uint8_t data[4];
				if(!ReadChunk(data,4)) return false;
				x = (data[0]<<24) | (data[1]<<16) | (data[2]<<8) | data[3];
				return true;
			}

			bool ReadBE(int32_t & x) { return ReadBE(reinterpret_cast<uint32_t&>(x)); }

			bool Write(uint32_t x) {
				uint8_t data[4] = {
					static_cast<uint8_t>(x & 0xff),
					static_cast<uint8_t>((x >> 8) & 0xff),
					static_cast<uint8_t>((x >> 16) & 0xff),
					static_cast<uint8_t>((x >> 24) & 0xff)
				};
				return WriteChunk(data, 4);
			}

			bool Write(int32_t x) { return Write(reinterpret_cast<uint32_t&>(x)); }
		///\}

		///\name 64 bits
		///\{
			bool Read(uint64_t & x) {
				uint8_t data[8];
				if(!ReadChunk(data,8)) return false;
				x = (uint64_t(data[7])<<56) | (uint64_t(data[6])<<48) | (uint64_t(data[5])<<40) | (uint64_t(data[4])<<32) | (uint64_t(data[3])<<24) | (data[2]<<16) | (data[1]<<8) | data[0];
				return true;
			}

			bool Read(int64_t & x) { return Read(reinterpret_cast<uint64_t&>(x)); }

			bool ReadBE(uint64_t & x) {
				uint8_t data[8];
				if(!ReadChunk(data,8)) return false;
				x = (uint64_t(data[0])<<56) | (uint64_t(data[1])<<48) | (uint64_t(data[2])<<40) | (uint64_t(data[3])<<32) | (uint64_t(data[4])<<24) | (data[5]<<16) | (data[6]<<8) | data[7];
				return true;
			}

			bool ReadBE(int64_t & x) { return ReadBE(reinterpret_cast<uint64_t&>(x)); }

			bool Write(uint64_t x) {
				uint8_t data[8] = {
					static_cast<uint8_t>(x & 0xff),
					static_cast<uint8_t>((x >> 8) & 0xff),
					static_cast<uint8_t>((x >> 16) & 0xff),
					static_cast<uint8_t>((x >> 24) & 0xff),
					static_cast<uint8_t>((x >> 32) & 0xff),
					static_cast<uint8_t>((x >> 40) & 0xff),
					static_cast<uint8_t>((x >> 48) & 0xff),
					static_cast<uint8_t>((x >> 56) & 0xff)
				};
				return WriteChunk(data, 8);
			}

			bool Write(int64_t x) { return Write(reinterpret_cast<uint64_t&>(x)); }

		///\name resolving special case for 'long int'
		///\{
			#if defined DIVERSALIS__OS__MICROSOFT
				// ban 'long int' which is too ambiguous

				UNIVERSALIS__COMPILER__DEPRECATED("ambiguous")
				bool Read(long int & x) { int32_t i; if(!Read(i)) return false; x = i; return true; }

				UNIVERSALIS__COMPILER__DEPRECATED("ambiguous")
				bool Read(unsigned long int & x) { uint32_t i; if(!Read(i)) return false; x = i; return true; }

				UNIVERSALIS__COMPILER__DEPRECATED("ambiguous")
				bool ReadBE(long int & x) { int32_t i; if(!ReadBE(i)) return false; x = i; return true; }

				UNIVERSALIS__COMPILER__DEPRECATED("ambiguous")
				bool ReadBE(unsigned long int & x) { uint32_t i; if(!ReadBE(i)) return false; x = i; return true; }

				UNIVERSALIS__COMPILER__DEPRECATED("ambiguous")
				bool Write(long int x) { int32_t i = x; return Write(i); }

				UNIVERSALIS__COMPILER__DEPRECATED("ambiguous")
				bool Write(unsigned long int x) { uint32_t i = x; return Write(i); }

			#elif 0
				// If int32_t is 'int' and int64_t is 'long long int',
				// this leaves a hole for the 'long int' type.
				// Similarly, if int32_t is 'long int' and int64_t is 'long long int',
				// this leaves a hole for the 'int' type.
				// The following templates solve this issue.
				// If there is an overloaded specialized normal function for a given type,
				// the compiler will prefer it over these non-specialized templates.
				// So, for example, reading/writing a float will not go through these templates.

				template<typename X>
				bool Read(X & x) {
					BOOST_STATIC_ASSERT((std::numeric_limits<X>::is_integer));
					BOOST_STATIC_ASSERT((sizeof x == 4 || sizeof x == 8));
					return sizeof x == 8 ?
						Read(reinterpret_cast<uint64_t&>(x)) :
						Read(reinterpret_cast<uint32_t&>(x));
				}

				template<typename X>
				bool ReadBE(X & x) {
					BOOST_STATIC_ASSERT((std::numeric_limits<X>::is_integer));
					BOOST_STATIC_ASSERT((sizeof x == 4 || sizeof x == 8));
					return sizeof x == 8 ?
						ReadBE(reinterpret_cast<uint64_t&>(x)) :
						ReadBE(reinterpret_cast<uint32_t&>(x));
				}

				template<typename X>
				bool Write(X x) {
					BOOST_STATIC_ASSERT((std::numeric_limits<X>::is_integer));
					BOOST_STATIC_ASSERT((sizeof x == 4 || sizeof x == 8));
					return sizeof x == 8 ?
						Write(reinterpret_cast<uint64_t&>(x)) :
						Write(reinterpret_cast<uint32_t&>(x));
				}
			#endif
		///\}

		///\name 32-bit floating point
		///\{
			bool Read(float & x) {
				union {
					float f;
					uint8_t data[4];
				};
				f = 1.0f;
				if(data[0] == 63 && data[1] == 128) {
					if(!ReadChunk(data, 4)) return false;
					std::swap(data[0], data[3]);
					std::swap(data[1], data[2]);
					x = f;
					return true;
				} else if(data[3] == 63 && data[2] == 128) {
					if(!ReadChunk(data, 4)) return false;
					x = f;
					return true;
				} else {
					assert(!"Error: Couldn't determine floating point endianness");
					return false;
				}
			}
		
			bool Write(float x) {
				union {
					float f;
					uint8_t data[4];
				};
				f = 1.0f;
				if(data[0] == 63 && data[1] == 128) {
					f = x;
					std::swap(data[0], data[3]);
					std::swap(data[1], data[2]);
					return WriteChunk(data, 4);
				} else if(data[3] == 63 && data[2] == 128) {
					f = x;
					return WriteChunk(data, 4);
				} else {
					assert(!"Error: Couldn't determine floating point endianness");
					return false;
				}
			}
		///\}

		///\name 64-bit floating point
		///\{
			bool Read(double & x) {
				union {
					float f;
					double d;
					uint8_t data[8];
				};
				f = 1.0f;
				if(data[0] == 63 && data[1] == 128) {
					if(!ReadChunk(data, 8)) return false;
					std::swap(data[0], data[7]);
					std::swap(data[1], data[6]);
					std::swap(data[2], data[5]);
					std::swap(data[3], data[4]);
					x = d;
					return true;
				} else if(data[3] == 63 && data[2] == 128) {
					if(!ReadChunk(data, 8)) return false;
					x = d;
					return true;
				} else {
					assert(!"Error: Couldn't determine floating point endianness");
					return false;
				}
			}

			bool Write(double x) {
				union {
					float f;
					double d;
					uint8_t data[8];
				};
				f = 1.0f;
				if(data[0] == 63 && data[1] == 128) {
					d = x;
					std::swap(data[0], data[7]);
					std::swap(data[1], data[6]);
					std::swap(data[2], data[5]);
					std::swap(data[3], data[4]);
					return WriteChunk(data, 8);
				} else if(data[3] == 63 && data[2] == 128) {
					d = x;
					return WriteChunk(data, 8);
				} else {
					assert(!"Error: Couldn't determine floating point endianness");
					return false;
				}
			}
		///\}

		///\name arrays
		///\{
			template<typename T>
			bool ReadArray(T* array, int n) {
				for(int i=0;i<n;i++) if (!Read(array[i])) return false;
				return true;
			}

			template<typename T>
			bool WriteArray(T const* array, int n) {
				for(int i=0;i<n;i++) if (!Write(array[i])) return false;
				return true;
			}
		///\}

		///\name strings
		///\{
			bool ReadString(std::string &);
			bool ReadString(char *, std::size_t max_length);
			bool WriteString(std::string const &);
			///\todo : Implement a ReadSizedString() which would do the same as ReadString
			//         which won't stop on the null, but rather on the size of the array(or else indicated by the second parameter). Finally,
			//         setting the last char to null.
			//bool ReadSizedString(std::string &, std::size_t numchars);
			//bool WriteSizedString(std::string const &, std::size_t numchars);
		///\}

		///\name riff four cc
		///\{
			static bool matchFourCC(char const a[4], char const b[4]) {
				return *(uint32_t const*)a == *(uint32_t const*)b;
			}
		///\}

		///\name riff chunk headers
		///\{
			bool Read(RiffChunkHeader &h) {
				return ReadChunk(h.id, 4) && Read(h.size);
			}

			bool Write(RiffChunkHeader &h) {
				return WriteChunk(h.id, 4) && Write(h.size);
			}
		///\}

	private:
		bool is_write_mode_;
		std::fstream stream_;
};

#if 0 // unfinished
class PSYCLE__CORE__DECL MemoryFile : public RiffFile {
	public:
		MemoryFile();
		virtual ~MemoryFile();

		bool OpenMem(std::ptrdiff_t blocksize);
		bool CloseMem();
		virtual std::size_t FileSize();
		virtual std::size_t GetPos();
		virtual int Seek(std::ptrdiff_t bytes);
		virtual int Skip(std::ptrdiff_t bytes);
		virtual bool ReadString(char *, std::size_t max_length);

	protected:
		virtual bool WriteChunk(void const *, std::size_t);
		virtual bool ReadChunk (void       *, std::size_t);
		virtual bool Expect    (void       *, std::size_t);

		std::vector<void*> memoryblocks_;
		int blocksize_;
};
#endif

}}
#endif
