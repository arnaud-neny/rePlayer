/*
* zipreader
* a library for reading zipfiles
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
#include "zipreader.h"

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
#include <cstdio>
#include <cstdlib>
#include <cstring>

// case-insensitive string comparison function
// TODO BAD: strcasecmp is not part of the iso std lib (hence the __STRICT_ANSI__ check below)
#if defined DIVERSALIS__COMPILER__MICROSOFT
	#define strcasecmp stricmp 
#elif defined DIVERSALIS__COMPILER__GNU
	#if defined DIVERSALIS__OS__CYGWIN__ && defined __STRICT_ANSI__
		// copied from cygwin's <string.h> header
		_BEGIN_STD_C
		int _EXFUN(strcasecmp,(const char *, const char *));
		_END_STD_C
	#endif
#endif

namespace psycle { namespace core {

static int _load(int fd, char *buf, size_t bufsize)
{
	int r;

	if (fd == -1) return 0;
	while (bufsize > 0) {
		r = read(fd,buf,bufsize);
		if (r == -1 && errno == EINTR) continue;
		if (r < 1) return 0;
		buf += r;
		bufsize -= r;
	}
	return 1;
}
static void _zr_fix_name(char **fn)
{
	for (;;) {
		if (strncmp(*fn, "./", 2) == 0) (*fn) = (*fn) + 2;
		else if (**fn == '/') (*fn) = (*fn) + 1;
		else break;
	}
}
static zipreader *_zr_step2(int fd, unsigned int /*a*/, long int b, unsigned int y)
{
	unsigned int i;
	unsigned char head1[30];
	unsigned char head2[46];
	off_t *ldset;
	unsigned int n, m, lh;
	unsigned int names;
	unsigned int tail;
	union u {
		char * raw;
		zipreader * z;
	} zu;
	zu.z = 0;

	ldset = (off_t*)malloc(y * sizeof(off_t));
	if (!ldset) return 0;

	/* step 2, we seek to aaaa and we'll find exactly xx
		* of the central directory entry file headers:
		* - aaaa is eodl[12] le32
		* - yy is eodl[8] le16
		*
		* PK\2\1...
		*
		* that fills out the offset of all the local headers...
		*/

	names = 0;
	for (i = 0; i < y; i++) { /* should be number of entries */
		if (lseek(fd, b, SEEK_SET) != b) goto FAIL;
		if (!_load(fd, (char*)head2, 46)) goto FAIL;

		/* check central directory header */
		if (memcmp(head2, "PK\1\2", 4) != 0) goto FAIL;
		if (memcmp(head2+34, "\0\0",2) != 0) goto FAIL;

		n = (head2[28] | (((unsigned int)head2[29]) << 8));/* file name length */
		m = (head2[30] | (((unsigned int)head2[31]) << 8)) /* extra field length */
			+ (head2[31] | (((unsigned int)head2[32]) << 8)); /* file comment length */

		/* local header offset */
		lh = head2[42]
			| (((unsigned int)head2[43]) << 8)
			| (((unsigned int)head2[44]) << 16)
			| (((unsigned int)head2[45]) << 24);
		ldset[i] = lh;

		/* next central directory header */
		b += 46 + n + m;
		names += n + 1;
	}
	zu.raw = (char*)malloc(sizeof(zipreader) + (sizeof(zipreader_file) * y) + names);
	if (!zu.raw) goto FAIL;
	zu.z->fd = fd;
	zu.z->headers = ldset;
	zu.z->_fnp = zu.z->files = y;
	tail = names;
	names = sizeof(zipreader) + (sizeof(zipreader_file) * y);
	tail += names;
	for (i = 0; i < y; i++) {
		if (lseek(fd, ldset[i], SEEK_SET) != ldset[i]) goto FAIL;
		if (!_load(fd, (char*)head1, 30)) goto FAIL;
		if (memcmp(head1, "PK\3\4", 4) != 0) goto FAIL;
		n = (head1[26] | (((unsigned int)head1[27]) << 8));/* file name length */
		m = (head1[28] | (((unsigned int)head1[29]) << 8));/* extra field length */
		zu.z->file[i].gpbits = head1[6] | (((unsigned int)head1[7]) << 8);
		zu.z->file[i].method = head1[8] | (((unsigned int)head1[9]) << 8);
		zu.z->file[i].crc32 = head1[14] | (((unsigned int)head1[15]) << 8)
			| (((unsigned int)head1[16]) << 16)
			| (((unsigned int)head1[17]) << 24);
		zu.z->file[i].csize = head1[18] | (((unsigned int)head1[19]) << 8)
			| (((unsigned int)head1[20]) << 16)
			| (((unsigned int)head1[21]) << 24);
		zu.z->file[i].esize = head1[22] | (((unsigned int)head1[23]) << 8)
			| (((unsigned int)head1[24]) << 16)
			| (((unsigned int)head1[25]) << 24);
		zu.z->file[i].filename_ptr = zu.raw + names;

		_zr_fix_name(&zu.z->file[i].filename_ptr);

		names += n + 1;
		if (names > tail) goto FAIL;
		if (!_load(fd, zu.z->file[i].filename_ptr, n)) goto FAIL;
		zu.z->file[i].filename_ptr[n] = 0; /* null terminate */
		zu.z->file[i].pos = (ldset[i] + n + m) + 30;
		zu.z->file[i].top = zu.z;
	}

	/* are we still here?!? i think we're done... */
	return zu.z;

FAIL:
	free(zu.raw);
	free(ldset);
	return 0;
}
static zipreader *_zr_step1(int fd, char *buf, size_t len, off_t left)
{
	unsigned int x;
	unsigned int a;
	unsigned int b;
	unsigned int y;
	unsigned int k;

	if (len < 22) return 0;

	/* step 1 is locating the central directory record;
		* the signature we're looking for (looking backwards)
		*
		* PK\5\6\0\0\0\0xxyyaaaabbbbkk...
		*
		* where xx==yy, and kk is the number of
		* bytes found in "..." until the end of the file.
		*
		* since aaaa is the size of the central directory,
		* and bbbb is the offset of the central directory,
		* bbbb+aaaa will be LESS (or equal) than left+x
		*
		* the window size is 22 bytes for this stage
		*/
	for (x = len-22;; x--) {
		if (memcmp(buf+x, "PK\5\6\0\0\0\0", 8) == 0 /* sig */
		&& memcmp(buf+x+8, buf+x+10, 2) == 0) { /* xx=yy */
			a = (((unsigned char *)buf)[x+12])
			| (((unsigned int )(((unsigned char *)buf)[x+13]))<<8)
			| (((unsigned int )(((unsigned char *)buf)[x+14]))<<16)
			| (((unsigned int )(((unsigned char *)buf)[x+15]))<<24);

			b = (((unsigned char *)buf)[x+16])
			| (((unsigned int )(((unsigned char *)buf)[x+17]))<<8)
			| (((unsigned int )(((unsigned char *)buf)[x+18]))<<16)
			| (((unsigned int )(((unsigned char *)buf)[x+19]))<<24);

			k = (((unsigned char *)buf)[x+20])
			| (((unsigned int )(((unsigned char *)buf)[x+21]))<<8);
			if (k == len-(x+22) && ((a+b) <= left+x)) {
				/* okay, step1 signature matches;
					* this could very well be the end signature
					*
					* we'll start trying to load the signature
					*/
				y = (((unsigned char *)buf)[x+8])
				| (((unsigned int )(((unsigned char *)buf)[x+9]))<<8);
				if (y) return _zr_step2(fd, a, b, y);
				
			}
		}
		if (x == 0) break;
	}
	return 0;
}

zipreader *zipreader_open(int fd)
{
	char buf[65558]; /* 64K+22 */
	zipreader *z;
	off_t pos;

	pos = lseek(fd, 0, SEEK_END);
	/* search backwards */
	//FIXME: Is the loop really necessary? The table is always at the end! (AFAIK)
    while ((uint) pos >= sizeof(buf)) {
		pos -= sizeof(buf);
		if (lseek(fd, pos, SEEK_SET) != pos) return 0;
		if (!_load(fd, buf, sizeof(buf))) return 0;
		if ((z = _zr_step1(fd,buf,sizeof(buf),pos))) return z; /* found it */
		/* window size */
		pos += 22;
	}
	if (pos > 0) {
		/* err, it's _got_ to be in this chunk */
		if (lseek(fd, 0, SEEK_SET) != 0) return 0;
		if (!_load(fd,buf,pos)) return 0; /* no overflow */
		return _zr_step1(fd,buf,pos,0);
	}
	/* nope */
	return 0;
}

static zipreader_file *_zw_finish_seek(zipreader_file *f)
{
	if (lseek(f->top->fd, f->pos, SEEK_SET) != f->pos) return 0;
	return f;
}

zipreader_file *zipreader_seek(zipreader *z, const char *filename)
{
	if (!z) return 0;
	while (z->_fnp < z->files) {
		if ( strcasecmp((char*)z->file[z->_fnp].filename_ptr, filename) == 0) {
			return _zw_finish_seek(&z->file[z->_fnp]);
		}
		z->_fnp++;
	}
	z->_fnp = 0;
	while (z->_fnp < z->files) {
		if (strcasecmp((char*)z->file[z->_fnp].filename_ptr, filename) == 0) {
			return _zw_finish_seek(&z->file[z->_fnp]);
		}
		z->_fnp++;
	}
	return 0;
}

void zipreader_close(zipreader *z)
{
	unsigned int i;
	if (!z) return;
	for (i = 0; i < z->files; i++) {
		z->file[i].filename_ptr = 0;
		z->file[i].top = 0;
	}
	z->files=0;
	free(z->headers);z->headers=0;
	free(z);
}

static int _zm_write(zipreader_file */*f*/, unsigned int *xcrc32, int outfd, void *buf, size_t len, void */*extra*/)
{
	int r;
	*xcrc32 = crc32(*xcrc32, (Bytef*)buf, len);
	while (len > 0) {
		do {
			r = write(outfd,buf,len);
		} while (r == -1 && errno == EINTR);
		if (r < 1) return 0;
		buf = (((char*)buf)+r);
		len -= r;
	}
	return 1;
}
static int _zm_readin(zipreader_file *f, int outfd,
	int (*chunk)(zipreader_file *f, unsigned int *xcrc32, int outfd, void *buf, size_t len, void *extra),
	int (*finish)(zipreader_file *f, unsigned int *xcrc32, int outfd, void *extra),
	void *extra)
{
	unsigned char sbuf[65536];
	unsigned int todo, getme;
	unsigned int xcrc32;
	int r;

	xcrc32 = crc32(0, Z_NULL, 0);
	todo = f->csize;
	if (lseek(f->top->fd, f->pos, SEEK_SET) != f->pos) return 0;
	getme = sizeof(sbuf);
	while (todo > 0) {
		if (todo < getme) getme = todo;
		do {
			r = read(f->top->fd, sbuf, getme);
		} while (r == -1 && errno == EINTR);
		if (r < 1) return 0;
		if (chunk && !chunk(f,&xcrc32,outfd,sbuf,r,extra)) return 0;
		todo -= r;
	}
	if (finish && !finish(f,&xcrc32,outfd,extra)) return 0;
	if (xcrc32 != f->crc32) return 0;
	return 1;
}

struct deflate_memory {
	z_stream str;
	unsigned char buf[65548];
};

static int _zm_inflate_fin(zipreader_file *f, unsigned int *xcrc32, int outfd, void *extra)
{
	struct deflate_memory *dm;
	int r;

	dm = (struct deflate_memory *)extra;

	dm->str.next_in = (Bytef*)0;
	dm->str.avail_in = 0;
	do {
		dm->str.next_out = (Bytef*)dm->buf;
		dm->str.avail_out = sizeof(dm->buf);

		r = inflate(&dm->str, Z_FINISH);
		if (r != Z_OK && r != Z_STREAM_END) return 0;
		if (sizeof(dm->buf) != dm->str.avail_out) {
			_zm_write(f,xcrc32, outfd, dm->buf, sizeof(dm->buf)-dm->str.avail_out, 0);
		}

	} while (r == Z_OK);
	return 1;
}
static int _zm_inflate(zipreader_file *f, unsigned int *xcrc32, int outfd, void *buf, size_t len, void *extra)
{
	struct deflate_memory *dm;
	int r;

	dm = (struct deflate_memory *)extra;
	dm->str.next_in = (Bytef*)buf;
	dm->str.avail_in = len;

	while (dm->str.avail_in > 0) {
		dm->str.next_out = (Bytef*)dm->buf;
		dm->str.avail_out = sizeof(dm->buf);
		r = inflate(&dm->str, Z_SYNC_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END) return 0;
		if (sizeof(dm->buf) != dm->str.avail_out) {
			_zm_write(f,xcrc32,outfd, dm->buf, sizeof(dm->buf)-dm->str.avail_out, 0);
		}
		if (r == Z_STREAM_END) return 1;
	}
	return 1;
}

int zipreader_extract(zipreader_file *f, int outfd)
{
	int r;
	if (!f || !f->top) return 0;
	switch (f->method) {
	case 0: /* STORE */
		return _zm_readin(f, outfd, _zm_write, 0, 0);
	case 8: /* DEFLATE */
		{
			struct deflate_memory dm;
			memset(&dm, 0, sizeof(dm));
			if (inflateInit2(&dm.str, -MAX_WBITS) != Z_OK) return 0;
			r = _zm_readin(f, outfd, _zm_inflate, _zm_inflate_fin, &dm);
			inflateEnd(&dm.str);
			return r;
		};
	};
	return 0;
}

}}
