// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__ZIP_WRITER_STREAM__INCLUDED
#define PSYCLE__CORE__ZIP_WRITER_STREAM__INCLUDED
#pragma once

#include "zipwriter.h"

#include <ios>
#include <ostream>
#include <streambuf>
#include <cstdio>

#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
	#pragma comment(lib, "zlibwapi")
#endif

namespace psycle { namespace core {

class zipfilestreambuf : public std::streambuf {
	public:
			zipfilestreambuf();

			zipfilestreambuf* open(  zipwriter *writer, const char* name, int compression);

			void close();

			~zipfilestreambuf();

			
			virtual int     overflow( int c = EOF);
			virtual int     underflow();
			virtual int     sync();

	private:
			static const int bufferSize = 1024 ;    // size of data buff
			char             buffer[bufferSize]; // data buffer

			zipwriter_file *f;
			zipwriter *z;

			int flush_buffer();

			bool open_;
};

class zipfilestreambase : virtual public std::ios {
	protected:
		zipfilestreambuf buf;

	public:
		zipfilestreambase();
		zipfilestreambase(  zipwriter *z, const char* name, int compression);
		~zipfilestreambase();

		void close();
};

class zipwriterfilestream : public zipfilestreambase, public std::ostream {
	public:
		zipwriterfilestream( zipwriter *z, const char* name, int compression = 9);
		~zipwriterfilestream();
};

}}
#endif
