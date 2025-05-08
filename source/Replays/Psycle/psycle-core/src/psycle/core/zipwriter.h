/*
* zipwriter
* a library for writing zipfiles
*
* copyright (c) 2007 Mrs. Brisby <mrs.brisby@nimh.org>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__ZIP_WRITER__INCLUDED
#define PSYCLE__CORE__ZIP_WRITER__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

#include <sys/types.h>
#include <ctime>

namespace psycle { namespace core {

/**
* this is a compile-time tunable. mrsbrisby wouldn't recommend making it smaller
* than this as some compression methods (deflate) use the size of this in
* electing a strategy. the only reason you should want to make this smaller
* is to compile zipwriter on small-memory implementations. This must not be
* smaller than 16K (16384) however, without reducing MAX_WBITS in zlib
*/
unsigned int const static ZW_BUFSIZE = 65536;

struct zipwriter;
struct zipwriter_method;
struct zipwriter_file;

struct zipwriter {
	int fd;
	int err;
	zipwriter_file *top, *cur;

	unsigned char *tail;
	void *comment;
	size_t comment_length;

	unsigned char buffer[ZW_BUFSIZE];

	/* may grow in the future... */
	void *compressor_state[5];
};

struct zipwriter_method {
	unsigned int num;
	void (*start)(zipwriter_file *f);
	void (*chunk)(zipwriter_file *f, const void *buf, unsigned int len);
	void (*finish)(zipwriter_file *f);
};

struct zipwriter_file {
	time_t mtime;
	unsigned char *head;
	unsigned char *head2;
	unsigned int head2_size;

	off_t head_off;
	char *filename;
	unsigned int crc32;
	unsigned int len, wlen;
	unsigned int flags;

	zipwriter_file *next, *prev;
	zipwriter_method *proc;
	zipwriter *top;
};

/**
* outfd must be opened for reading+writing, be seekable, and must be empty.
* this means that you probably should have opened it with:
*    open(..., O_CREAT|O_EXCL|O_RDWR)
* it should also probably be a temporary file so that you can rename() it
* over the target filename thus making your file-save operations crashproof.
*
* outfd must not be used (directly) again.
*/
zipwriter *zipwriter_start(int outfd);

/**
* set the global zipfile comment.
*
* this MAY be disturbed by various zipfile reading/writing tools, so you
* should not put anything in here that you need to get back.
*
* if it fails to put the comment in, zipwriter_finish() will return an
* error.
*/
void zipwriter_comment(zipwriter *z, const void *buf, size_t length);

/**
*  starts adding a file. the directory entry isn't actually written until the
* end of the curent file.
*
* zipwriter_file->mtime will be initialized to "now" you can change this.
*
* the name must be the path relative to the root of the zipfile directory,
* separated by forward-slashes -- EVEN on platforms that use a backslash.
*
*/
zipwriter_file *zipwriter_addfile(zipwriter *d, const char *name, unsigned int flags);
unsigned int const static ZIPWRITER_STORE    = 0;
unsigned int const static ZIPWRITER_DEFLATE1 = 1;
unsigned int const static ZIPWRITER_DEFLATE2 = 2;
unsigned int const static ZIPWRITER_DEFLATE3 = 3;
unsigned int const static ZIPWRITER_DEFLATE4 = 4;
unsigned int const static ZIPWRITER_DEFLATE5 = 5;
unsigned int const static ZIPWRITER_DEFLATE6 = 6;
unsigned int const static ZIPWRITER_DEFLATE7 = 7;
unsigned int const static ZIPWRITER_DEFLATE8 = 8;
unsigned int const static ZIPWRITER_DEFLATE9 = 9;
unsigned int const static ZIPWRITER_DEFLATE  = 8;
unsigned int const static ZIPWRITER_COMPRESS_MASK = 0xffff;

/**
*  this is optional; it doesn't affect the behavior of zipwriter
* when writing files to the zipfile, so unless you're actually making
* your "text" files have MS-DOS style line-endings (\x0d\x0a), you
* should probably use ZIPWRITER_FILE_BINARY i.e. leave this alone.
*
* on the other hand, if you _really_ want to make text files, the
* functionality is exposed here to at least set the bit in the zipfile
* so that Info-ZIP on UNIX, MacOS, Amiga, and etc, will translate the
* appropriate line-endings...
*/
unsigned int const static ZIPWRITER_FILE_BINARY = 0x00000;
unsigned int const static ZIPWRITER_FILE_TEXT   = 0x10000;

/**
*  writes a chunk of data to the current file. returns zero if f isn't
* opened, if it isn't the current file, or if write() fails. when
* possible, f->top->err will be set, and it can be detected at the
* end of the zipwriter process
*/
int zipwriter_write(zipwriter_file *f, const void *buf, size_t len);

/**
*  finishes the current zipfile, adding a comment if requested, and
* writing the zip index. this closes the file descriptor, and fsync()s
* it, so when this function returns nonzero, the file is on the disk.
* if this function returns zero, either it failed to do this, or
* a zipwriter_write() failed earlier.
*/
int zipwriter_finish(zipwriter *d);

void zipwriter_copy(int in, zipwriter_file *out);

}}
#endif
