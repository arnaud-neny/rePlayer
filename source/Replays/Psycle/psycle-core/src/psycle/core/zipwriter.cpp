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
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "zipwriter.h"

#if defined DIVERSALIS__OS__MICROSOFT
	#include <universalis/os/include_windows_without_crap.hpp>
	#include <io.h>
#else
	#include <unistd.h>
	#include <sys/types.h>
#endif

#include <zlib.h> // include after windows header

#include <sys/stat.h>
#include <fcntl.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace psycle { namespace core {

static void _zw_tail(zipwriter *d);
static void _zw_eodr(zipwriter *d, unsigned char *ptr);
static void _zw_write(zipwriter *d, const void *buf, size_t len);

static void _zm_store_chunk(zipwriter_file *f, const void *buf, unsigned int len);
static void _zm_deflate_start(zipwriter_file *f);
static void _zm_deflate_chunk(zipwriter_file *f, const void *buf, unsigned int len);
static void _zm_deflate_finish(zipwriter_file *f);

static zipwriter_method zm_store = {
	ZIPWRITER_STORE,
	0,
	_zm_store_chunk,
	0
};

static zipwriter_method zm_deflate = {
	ZIPWRITER_DEFLATE,
	_zm_deflate_start,
	_zm_deflate_chunk,
	_zm_deflate_finish
};

zipwriter *zipwriter_start(int outfd)
{
	zipwriter *z;

	if (outfd == -1) return 0;
	if (lseek(outfd, 0, SEEK_SET) != 0) return 0;

	z = (zipwriter*)malloc(sizeof(zipwriter));
	if (!z) return 0;

	z->compressor_state[0] = (void*)malloc(sizeof(z_stream));
	if (!z->compressor_state[0]) {
		free(z);
		return 0;
	}
	z->fd = outfd;
	z->err = 0;
	z->top = z->cur = 0;
	z->tail = 0;
	z->comment = 0;
	z->comment_length = 0;
	return z;
}

void zipwriter_comment(zipwriter *z, const void *buf, size_t length)
{
	if (!z || z->err) return;
	free(z->tail);
	z->comment_length = length;
	z->tail = (unsigned char *)malloc(length + 22);
	if (!z->tail) {
		z->err = errno;
		return;
	}
	z->comment = (void *)(z->tail + 22);
	memcpy(z->comment, buf, length);
}

zipwriter_file *zipwriter_addfile(zipwriter *d, const char *name,
		unsigned int flags)
{
	zipwriter_file *f;
	int n;

	if (!d || d->err) return 0;

	_zw_tail(d);

	f = (zipwriter_file*)malloc(sizeof(zipwriter_file));
	if (!f) {
		d->err = errno;
		return 0;
	}
	f->head = (unsigned char *)malloc(strlen(name)+30);
	if (!f->head) {
		d->err = errno;
		free(f);
		return 0;
	}
	f->head2 = (unsigned char *)malloc(strlen(name)+46);
	if (!f->head2) {
		d->err = errno;
		free(f->head);
		free(f);
		return 0;
	}

	memcpy(f->head2, "PK\1\2"
		"\027\00"  /* 4 made by version */
		"\012\00"  /* 6 version needed to extract */
		"\0\0"     /* 8 general purpose bit flag */
		"\1\1"     /* 10 compression method */
		"\1\1"     /* 12 mtime file time */
		"\1\1"     /* 14 mtime file date */
		"\2\2\2\2" /* 16 crc32 */
		"\3\3\3\3" /* 20 compressed size */
		"\3\3\3\3" /* 24 uncompressed size */
		"\4\4"     /* 28 file name length */
		"\0\0"     /* 30 extra field length */
		"\0\0"     /* 32 file comment length */
		"\0\0"     /* 34 disk number */
		"\0\0"     /* 36 internal file attributes */
		"\0\0\0\0" /* 38 external file attributes */
		"\0\0\0\0" /* 42 relative offset of local header */
	, 46);

	memcpy(f->head, "PK\3\4"
		"\012\0"   /* 4 version needed to extract */
		"\0\0"     /* 6 general purpose bit flag */
		"\1\1"     /* 8 compression method */
		"\1\1"     /* 10 mtime file time */
		"\1\1"     /* 12 mtime file date */
		"\2\2\2\2" /* 14 crc32 */
		"\3\3\3\3" /* 18 compressed size */
		"\3\3\3\3" /* 22 uncompressed size */
		"\4\4"     /* 26 file name length */
		"\0\0"     /* 28 extra field length */
	, 30);
	/* compression method */

	if ((flags &  ZIPWRITER_COMPRESS_MASK) == 0) {
		f->head[8] = 0;
		f->head[9] = 0;
		f->proc = &zm_store;
	} else if ((flags &  ZIPWRITER_COMPRESS_MASK) < 10) {
		f->head[8] = 8; /* deflate */
		f->head[9] = 0;
		switch ((flags & ZIPWRITER_COMPRESS_MASK)) {
		case 9:
		case 8:
			f->head[6] |= 2;
			break;
		case 2:
			f->head[6] |= 4;
			break;
		case 1:
			f->head[6] |= 6;
			break;
		};
		f->proc = &zm_deflate;
	}
	n = strlen(name);
	f->head[26] = n & 255; /* LSB */
	f->head[27] = (n >> 8) & 255;
	f->filename = (char*)(f->head+30);
	memcpy(f->head+30, name, n);
	memcpy(f->head2+46, f->head+30, n);
	f->head2_size = 46+n;

	time(&f->mtime);
	f->flags = flags;
	f->crc32 = crc32(0,NULL,0);
	f->next = d->top;
	if (f->next) f->next->prev = f;
	f->prev = 0;
	f->head_off = lseek(d->fd, 0, SEEK_CUR);
	d->cur = d->top = f;

	f->top = d;

	/* write local file header */
	_zw_write(d, (void*)f->head, 30+n);

	/* write len header */
	f->len = f->wlen = 0;

	if (f->proc->start) f->proc->start(f);

	return f;
}
int zipwriter_write(zipwriter_file *f, const void *buf, size_t len)
{
	if (!f || (f->top && f->top->err)) return 0;

	f->crc32 = crc32(f->crc32, (const unsigned char *)buf, len);
	f->len += len;

	if (f->proc->chunk) f->proc->chunk(f, buf, len);
	if (f->top && f->top->err) return 0;
	return 1;
}

int zipwriter_finish(zipwriter *d)
{
	zipwriter_file *f, *nf;
	unsigned char eodr_buf[22];
	unsigned int eodr_length;
	unsigned char *eodr_ptr;
	unsigned int num, size;
	int fd, err;

	if (!d) return 0;

	_zw_tail(d);
	if (d->tail) {
		eodr_ptr = d->tail;
		eodr_length = 22 + d->comment_length;
	} else {
		eodr_ptr = eodr_buf;
		eodr_length = 22;
	}

	_zw_eodr(d, eodr_ptr);
	eodr_ptr[20] = (d->comment_length & 255);
	eodr_ptr[21] = ((d->comment_length >> 8) & 255);

	/* write central directory */
	num = size = 0;
	for (f = d->top; f && f->next; f = f->next);
	while (f) {
		nf = f->prev;
		_zw_write(d, f->head2, f->head2_size);
		size += f->head2_size;
		num++;
		free(f->head2);
		free(f->head);
		free(f);
		f = nf;
	}
	eodr_ptr[8] = eodr_ptr[10] = num & 255;
	eodr_ptr[9] = eodr_ptr[11] = (num >> 8) & 255;

	eodr_ptr[12] = size & 255;
	eodr_ptr[13] = (size >> 8) & 255;
	eodr_ptr[14] = (size >> 16) & 255;
	eodr_ptr[15] = (size >> 24) & 255;
	d->cur = d->top = 0;
	_zw_write(d, eodr_ptr, eodr_length);

	err = d->err;
	fd = d->fd;

	free(d->compressor_state[0]);
	free(d);
	if (err) return 0;
	#if defined __unix__ || defined __APPLE__
	if (fsync(fd) == -1) return 0;
	#else
	if (_commit(fd) == -1) return 0;
	#endif
	if (close(fd) == -1) return 0;
	return 1;
}

/* helpers */
static void _zw_eodr(zipwriter *d, unsigned char *ptr)
{
	off_t o;
	memcpy(ptr,"PK\5\6"
			"\0\0"     /* 4 number of this disk */
			"\0\0"     /* 6 disk with the central directory */
			"\1\1"     /* 8 number of entries in central directory on this disk */
			"\1\1"     /* 10 number of entries in central directory */
			"\2\2\2\2" /* 12 size in bytes of central directory */
			"\3\3\3\3" /* 16 global offset to start of central directory */
			"\0\0"     /* 20 comment length */
		, 22);
	o = lseek(d->fd, 0, SEEK_CUR);
	if (o == -1) d->err = errno;
	ptr[16] = static_cast< unsigned char >(o & 255);
	ptr[17] = static_cast< unsigned char >((o >> 8) & 255);
	ptr[18] = static_cast< unsigned char >((o >> 16) & 255);
	ptr[19] = static_cast< unsigned char >((o >> 24) & 255);
}
static void _zw_tail(zipwriter *d)
{
	zipwriter_file *f;
	struct tm *tm;
	unsigned int dt;
	off_t here;

	if (!d->cur) return;
	f = d->cur;

	if (f->proc && f->proc->finish) f->proc->finish(f);

	tm = localtime(&f->mtime);
	if (tm->tm_year < 80) tm->tm_year = 0;
	else tm->tm_year -= 80; /* epoch in msdos time is 1980 */
	tm->tm_mon++;

	dt = (tm->tm_year << 25)
		| (tm->tm_mon << 21)
		| (tm->tm_mday << 16)
		| (tm->tm_hour << 11)
		| (tm->tm_min << 5)
		| (tm->tm_sec >> 1);
	f->head[10] = dt & 255;
	f->head[11] = (dt >> 8) & 255;
	f->head[12] = (dt >> 16) & 255;
	f->head[13] = (dt >> 24) & 255;

	dt = f->crc32;
	f->head[14] = dt & 255;
	f->head[15] = (dt>>8) & 255;
	f->head[16] = (dt>>16) & 255;
	f->head[17] = (dt>>24) & 255;

	dt = f->wlen; /* compressed size */
	f->head[18] = dt & 255;
	f->head[19] = (dt>>8) & 255;
	f->head[20] = (dt>>16) & 255;
	f->head[21] = (dt>>24) & 255;

	dt = f->len; /* uncompressed size */
	f->head[22] = dt & 255;
	f->head[23] = (dt>>8) & 255;
	f->head[24] = (dt>>16) & 255;
	f->head[25] = (dt>>24) & 255;

	here = lseek(d->fd, 0, SEEK_CUR);
	if (here <= 0 || lseek(d->fd, f->head_off, SEEK_SET) != f->head_off) {
		d->err = errno;
		return;
	}
	_zw_write(d, f->head, 30);
	if (lseek(d->fd, here, SEEK_SET) != here) {
		d->err = errno;
		return;
	}

	/* copy head options */
	memcpy(f->head2+8, f->head+6, 24);

	/* ascii? */
	if (f->flags & ZIPWRITER_FILE_TEXT) f->head2[36] |= 1;

	/* local offset */
	dt = f->head_off;
	f->head2[42] = dt & 255; 
	f->head2[43] = (dt>>8) & 255; 
	f->head2[44] = (dt>>16) & 255; 
	f->head2[45] = (dt>>24) & 255; 

	d->cur = 0;
}
static void _zw_write(zipwriter *d, const void *buf, size_t len)
{
	int r;
	const char *bp;

	if (d->err) return;
	if (d->cur) d->cur->wlen += len;
	bp=(const char*)buf;
	while (len > 0) {
		do {
			r = write(d->fd, bp, len);
		} while (r == -1 && errno == EINTR);
		if (r == -1) {
			d->err = errno;
			break;
		}
		len -= r;
		bp += r;
	}
}


/* compressors */
static void _zm_store_chunk(zipwriter_file *f, const void *buf, unsigned int len)
{
	_zw_write(f->top, buf, len);
}
static void _zm_deflate_start(zipwriter_file *f)
{
	z_stream *s = (z_stream *)f->top->compressor_state[0];

	s->avail_in = 0;
	s->avail_out = ZW_BUFSIZE;
	s->next_out = f->top->buffer;
	s->total_in = 0;
	s->total_out = 0;
	s->zalloc = 0;
	s->zfree = 0;
	s->opaque = 0;
	if (deflateInit2(s, (f->flags & ZIPWRITER_COMPRESS_MASK),
	Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
		f->top->err = EINVAL;
	}
}
static void _zm_deflate_chunk(zipwriter_file *f, const void *buf, unsigned int len)
{
	z_stream *s = (z_stream *)f->top->compressor_state[0];

	s->next_in = (unsigned char *)buf;
	s->avail_in = len;

	CONT:
	if (s->avail_out == 0) {
		_zw_write(f->top, f->top->buffer, ZW_BUFSIZE);
		s->avail_out = ZW_BUFSIZE;
		s->next_out = f->top->buffer;
	}

	if (deflate(s, Z_NO_FLUSH) != Z_OK) {
		f->top->err = EINVAL;
	}
	if (s->avail_out == 0) {
		_zw_write(f->top, f->top->buffer, ZW_BUFSIZE);
		s->avail_out = ZW_BUFSIZE;
		s->next_out = f->top->buffer;
	}
	if (s->avail_in) goto CONT; /* more todo! */

}
static void _zm_deflate_finish(zipwriter_file *f)
{
	z_stream *s = (z_stream *)f->top->compressor_state[0];
	int tmp;

	do {
		if (s->avail_out == 0) {
			_zw_write(f->top, f->top->buffer, ZW_BUFSIZE);
			s->avail_out = ZW_BUFSIZE;
			s->next_out = f->top->buffer;
		}
	} while ((tmp = deflate(s, Z_FINISH)) == Z_OK);
				
	if (tmp != Z_STREAM_END) {
		f->top->err = EINVAL;
		return;
	}
	s->avail_in = 0;
	if (s->avail_out == 0) {
		_zw_write(f->top, f->top->buffer, ZW_BUFSIZE);
		s->avail_out = ZW_BUFSIZE;
		s->next_out = f->top->buffer;
	}
	if (deflateEnd(s) != Z_OK) {
		f->top->err = EINVAL;
		return;
	}

	/* write whatever we can get away with.. */
	if (s->avail_out < ZW_BUFSIZE) {
		_zw_write(f->top, f->top->buffer, ZW_BUFSIZE-s->avail_out);
	}
}

void zipwriter_copy(int in, zipwriter_file *out) 
{
	char buffer[65536];
	int r;

	(void)lseek(in,0,SEEK_SET);
	for (;;) {
		do {
			r = read(in, buffer,sizeof(buffer));
		} while (r == -1 && errno == EINTR);
		if (!r) break;
		zipwriter_write(out, buffer, r);
	}
}

}}
