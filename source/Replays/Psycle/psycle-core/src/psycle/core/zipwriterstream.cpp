// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "zipwriterstream.h"
#include "zipwriter.h"

namespace psycle { namespace core {
// the streambuffer descent

zipfilestreambuf::zipfilestreambuf( )
{
	setp(buffer, buffer + (bufferSize - 1));
	setg(
		buffer + 4, // beginning of putback area
		buffer + 4, // read position
		buffer + 4  // end position
	);
	// ASSERT: both input & output capabilities will not be used together
	open_ = 0;
}

zipfilestreambuf::~ zipfilestreambuf( )
{
}

zipfilestreambuf * zipfilestreambuf::open( zipwriter * writer, const char * name , int compression)
{
	if (!open_) {
		z = writer;
		f = zipwriter_addfile(z, name, compression );
		if (f) {
			open_ = 1;
		}
	}
	return this;
}

void zipfilestreambuf::close( )
{
	if (open_) {
		sync();
		open_ = 0;
	}
}

int zipfilestreambuf::overflow( int c ) // used for output buffer only
{

	if ( !f )
				return EOF;
		if (c != EOF) {
				*pptr() = c;
				pbump(1);
		}

		if ( flush_buffer() == EOF)
				return EOF;

		return c;
}

int zipfilestreambuf::underflow( ) // used for input buffer only
{
	return 0;
}

int zipfilestreambuf::sync( )
{
		if ( pptr() && pptr() > pbase()) {
				if ( flush_buffer() == EOF)
						return -1;
		}
		return 0;
}

int zipfilestreambuf::flush_buffer( )
{
	// Separate the writing of the buffer from overflow() and
	// sync() operation.
	int w = pptr() - pbase();
	
	if (w==0) return EOF;
	
	if (zipwriter_write(f, pbase(), w) == 0) return EOF;
	
	pbump( -w);
	return w;
}

void zipfilestreambase::close( )
{
	buf.close();
}


zipfilestreambase::zipfilestreambase( )
{
		init( &buf);
}

zipfilestreambase::zipfilestreambase(  zipwriter *z, const char * name , int compression)
{
	init( &buf);
	if ( ! buf.open( z, name, compression))
				clear( rdstate() | std::ios::badbit);
}

zipfilestreambase::~ zipfilestreambase( )
{
	buf.close();
}

zipwriterfilestream::~ zipwriterfilestream( )
{
}

zipwriterfilestream::zipwriterfilestream( zipwriter * z, const char * name , int compression)
	: zipfilestreambase( z, name, compression), std::ostream( &buf)
{}

}}
