/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2003    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: stdio-based convenience library for opening/seeking/decoding

 ********************************************************************/

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <stdio.h>
#include "ivorbiscodec.h"

/* The function prototypes for the callbacks are basically the same as for
 * the stdio functions fread, fseek, fclose, ftell. 
 * The one difference is that the FILE * arguments have been replaced with
 * a void * - this is to be used as a pointer to whatever internal data these
 * functions might need. In the stdio case, it's just a FILE * cast to a void *
 * 
 * If you use other functions, check the docs for these functions and return
 * the right values. For seek_func(), you *MUST* return -1 if the stream is
 * unseekable
 */
typedef struct {
  size_t (*read_func)  (void *ptr, size_t size, size_t nmemb, void *datasource);
  int    (*seek_func)  (void *datasource, ogg_int64_t offset, int whence);
  int    (*close_func) (void *datasource);
  long   (*tell_func)  (void *datasource);
} ov_callbacks;

typedef struct OggVorbis_File {
  void            *datasource; /* Pointer to a FILE *, etc. */
  int              seekable;
  ogg_int64_t      offset;
  ogg_int64_t      end;
  tremor_ogg_sync_state   *oy; 

  /* If the FILE handle isn't seekable (eg, a pipe), only the current
     stream appears */
  int              links;
  ogg_int64_t     *offsets;
  ogg_int64_t     *dataoffsets;
  ogg_uint32_t    *serialnos;
  ogg_int64_t     *pcmlengths;
  tremor_vorbis_info     vi;
  tremor_vorbis_comment  vc;

  /* Decoding working state local storage */
  ogg_int64_t      pcm_offset;
  int              ready_state;
  ogg_uint32_t     current_serialno;
  int              current_link;

  ogg_int64_t      bittrack;
  ogg_int64_t      samptrack;

  tremor_ogg_stream_state *os; /* take physical pages, weld into a logical
                          stream of packets */
  tremor_vorbis_dsp_state *vd; /* central working state for the packet->PCM decoder */

  ov_callbacks callbacks;

} OggVorbis_File;

extern int tremor_ov_clear(OggVorbis_File *vf);
extern int tremor_ov_open(FILE *f,OggVorbis_File *vf,char *initial,long ibytes);
extern int tremor_ov_open_callbacks(void *datasource, OggVorbis_File *vf,
		char *initial, long ibytes, ov_callbacks callbacks);

extern int tremor_ov_test(FILE *f,OggVorbis_File *vf,char *initial,long ibytes);
extern int tremor_ov_test_callbacks(void *datasource, OggVorbis_File *vf,
		char *initial, long ibytes, ov_callbacks callbacks);
extern int tremor_ov_test_open(OggVorbis_File *vf);

extern long tremor_ov_bitrate(OggVorbis_File *vf,int i);
extern long tremor_ov_bitrate_instant(OggVorbis_File *vf);
extern long tremor_ov_streams(OggVorbis_File *vf);
extern long tremor_ov_seekable(OggVorbis_File *vf);
extern long tremor_ov_serialnumber(OggVorbis_File *vf,int i);

extern ogg_int64_t tremor_ov_raw_total(OggVorbis_File *vf,int i);
extern ogg_int64_t tremor_ov_pcm_total(OggVorbis_File *vf,int i);
extern ogg_int64_t tremor_ov_time_total(OggVorbis_File *vf,int i);

extern int tremor_ov_raw_seek(OggVorbis_File *vf,ogg_int64_t pos);
extern int tremor_ov_pcm_seek(OggVorbis_File *vf,ogg_int64_t pos);
extern int tremor_ov_pcm_seek_page(OggVorbis_File *vf,ogg_int64_t pos);
extern int tremor_ov_time_seek(OggVorbis_File *vf,ogg_int64_t pos);
extern int tremor_ov_time_seek_page(OggVorbis_File *vf,ogg_int64_t pos);

extern ogg_int64_t tremor_ov_raw_tell(OggVorbis_File *vf);
extern ogg_int64_t tremor_ov_pcm_tell(OggVorbis_File *vf);
extern ogg_int64_t tremor_ov_time_tell(OggVorbis_File *vf);

extern tremor_vorbis_info *tremor_ov_info(OggVorbis_File *vf,int link);
extern tremor_vorbis_comment *tremor_ov_comment(OggVorbis_File *vf,int link);

extern long tremor_ov_read(OggVorbis_File *vf,void *buffer,int length,
		    int *bitstream);

#ifdef __cplusplus
}
#endif /* __cplusplus */
