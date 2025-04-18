/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: libvorbis codec headers

 ********************************************************************/

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include "ogg.h"

struct tremor_vorbis_dsp_state;
typedef struct tremor_vorbis_dsp_state tremor_vorbis_dsp_state;

typedef struct tremor_vorbis_info{
  int version;
  int channels;
  long rate;

  /* The below bitrate declarations are *hints*.
     Combinations of the three values carry the following implications:
     
     all three set to the same value: 
       implies a fixed rate bitstream
     only nominal set: 
       implies a VBR stream that averages the nominal bitrate.  No hard 
       upper/lower limit
     upper and or lower set: 
       implies a VBR bitstream that obeys the bitrate limits. nominal 
       may also be set to give a nominal rate.
     none set:
       the coder does not care to speculate.
  */

  long bitrate_upper;
  long bitrate_nominal;
  long bitrate_lower;
  long bitrate_window;

  void *codec_setup;
} tremor_vorbis_info;

typedef struct tremor_vorbis_comment{
  char **user_comments;
  int   *comment_lengths;
  int    comments;
  char  *vendor;

} tremor_vorbis_comment;


/* Vorbis PRIMITIVES: general ***************************************/

extern void     tremor_vorbis_info_init(tremor_vorbis_info *vi);
extern void     tremor_vorbis_info_clear(tremor_vorbis_info *vi);
extern int      tremor_vorbis_info_blocksize(tremor_vorbis_info *vi,int zo);
extern void     tremor_vorbis_comment_init(tremor_vorbis_comment *vc);
extern void     tremor_vorbis_comment_add(tremor_vorbis_comment *vc, char *comment); 
extern void     tremor_vorbis_comment_add_tag(tremor_vorbis_comment *vc, 
				       char *tag, char *contents);
extern char    *tremor_vorbis_comment_query(tremor_vorbis_comment *vc, char *tag, int count);
extern int      tremor_vorbis_comment_query_count(tremor_vorbis_comment *vc, char *tag);
extern void     tremor_vorbis_comment_clear(tremor_vorbis_comment *vc);

/* Vorbis ERRORS and return codes ***********************************/

#define OV_FALSE      -1  
#define OV_EOF        -2
#define OV_HOLE       -3

#define OV_EREAD      -128
#define OV_EFAULT     -129
#define OV_EIMPL      -130
#define OV_EINVAL     -131
#define OV_ENOTVORBIS -132
#define OV_EBADHEADER -133
#define OV_EVERSION   -134
#define OV_ENOTAUDIO  -135
#define OV_EBADPACKET -136
#define OV_EBADLINK   -137
#define OV_ENOSEEK    -138

#ifdef __cplusplus
}
#endif /* __cplusplus */
