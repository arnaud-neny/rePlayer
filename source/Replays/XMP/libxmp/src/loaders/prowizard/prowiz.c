/* ProWizard
 * Copyright (C) 1997-1999 Sylvain "Asle" Chipaux
 * Copyright (C) 2006-2007 Claudio Matsuoka
 * Copyright (C) 2021-2024 Alice Rowan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Pro-Wizard_1.c
 */

#include "xmp.h"
#include "prowiz.h"

const struct pw_format *const pw_formats[NUM_PW_FORMATS + 1] = {
	/* With signature */
	&pw_ac1d,
	&pw_fchs,
	&pw_fcm,
	&pw_fuzz,
	&pw_hrt,
	/* &pw_kris, */
	&pw_ksm,
	&pw_mp_id,
	&pw_ntp,
	&pw_p18a,
	&pw_p10c,
	&pw_pru1,
	&pw_pru2,
	&pw_pha,
	&pw_wn,
	&pw_unic_id,
	&pw_tp3,
	&pw_tp2,
	&pw_tp1,
	&pw_skyt,

	/* No signature */
	&pw_xann,
	&pw_di,
	&pw_eu,
	&pw_p4x,
	&pw_pp21,
	&pw_pp30,
	&pw_pp10,
	&pw_p50a,
	&pw_p60a,
	&pw_p61a,
	&pw_mp_noid,	/* Must check before Heatseeker, after ProPacker 1.0 */
	&pw_nru,
	&pw_np2,
	&pw_np1,
	&pw_np3,
	&pw_zen,
	&pw_unic_emptyid,
	&pw_unic_noid,
	&pw_unic2,
	&pw_crb,
	&pw_tdd,
	&pw_starpack,
	&pw_gmc,
	/* &pw_pm01, */
	&pw_titanics,
	NULL
};

int pw_move_data(mem_out *out, HIO_HANDLE *in, int len) // rePlayer
{
	uint8 buf[1024];
	int l;

	do {
		l = hio_read(buf, 1, len > 1024 ? 1024 : len, in);
		bwrite(buf, 1, l, out); // rePlayer
		len -= l;
	} while (l > 0 && len > 0);

	return 0;
}

int pw_write_zero(mem_out *out, int len) // rePlayer
{
	uint8 buf[1024];
	int l;

	do {
		l = len > 1024 ? 1024 : len;
		memset(buf, 0, l);
		bwrite(buf, 1, l, out);	// rePlayer
		len -= l;
	} while (l > 0 && len > 0);

	return 0;
}

int pw_wizardry(HIO_HANDLE *file_in, mem_out *file_out, const char **name) // rePlayer
{
	const struct pw_format *format;

	/**********   SEARCH   **********/
	format = pw_check(file_in, NULL);
	if (format == NULL) {
		return -1;
	}

	hio_seek(file_in, 0, SEEK_SET);
	if (format->depack(file_in, file_out) < 0) {
		return -1;
	}

	if (hio_error(file_in)) {
		return -1;
	}

	// fflush(file_out); // rePlayer

	if (name != NULL) {
		*name = format->name;
	}

	return 0;
}

#define BUF_SIZE 0x10000

const struct pw_format *pw_check(HIO_HANDLE *f, struct xmp_test_info *info)
{
	int i, res;
	char title[21];
	unsigned char *b;
	const unsigned char *internal;
	const unsigned char *src;
	int s = BUF_SIZE;

	if ((internal = hio_get_underlying_memory(f)) != NULL) {
		/* File is in memory, so reading chunks isn't necessary. */
		src = internal;
		s = hio_size(f);
		b = NULL;

	} else {
	b = (unsigned char *) calloc(1, BUF_SIZE);
	if (b == NULL)
		return NULL;

	s = hio_read(b, 1, s, f);
		src = b;
	}

	for (i = 0; pw_formats[i] != NULL; i++) {
		D_("checking format [%d]: %s", s, pw_formats[i]->name);
		res = pw_formats[i]->test(src, title, s);
		if (res > 0 && !internal) {
			/* Extra data was requested. */
			/* Round requests up to 4k to reduce slow checks. */
			int fetch = (res + 0xfff) & ~0xfff;
			unsigned char *buf = (unsigned char *) realloc(b, s + fetch);
			if (buf == NULL) {
				free(b);
				return NULL;
			}
			b = buf;
			src = b;

			/* If the requested data can't be read, try the next format. */
			fetch = hio_read(b + s, 1, fetch, f);
			if (fetch < res) {
				continue;
			}

			/* Try this format again... */
			s += fetch;
			i--;
		} else if (res == 0) {
			D_("format ok: %s\n", pw_formats[i]->name);
			if (info != NULL) {
				memcpy(info->name, title, 21);
				strncpy(info->type, pw_formats[i]->name,
							XMP_NAME_SIZE - 1);
			}
			free(b);
			return pw_formats[i];
		}
	}
	free(b);
	return NULL;
}

void pw_read_title(const unsigned char *b, char *t, int s)
{
	if (t == NULL) {
		return;
	}

	if (b == NULL) {
		*t = 0;
		return;
	}

	if (s > 20) {
		s = 20;
	}

	memcpy(t, b, s);
	t[s] = 0;
}
