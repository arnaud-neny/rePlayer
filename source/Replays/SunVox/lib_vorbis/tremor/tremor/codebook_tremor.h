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

 function: basic shared codebook operations

 ********************************************************************/

#pragma once

#include "ogg.h"

typedef struct codebook{
  long  dim;             /* codebook dimensions (elements per vector) */
  long  entries;         /* codebook entries */
  long  used_entries;    /* populated codebook entries */

  int   dec_maxlength;
  void *dec_table;
  int   dec_nodeb;      
  int   dec_leafw;      
  int   dec_type;        /* 0 = entry number
			    1 = packed vector of values
			    2 = packed vector of column offsets, maptype 1 
			    3 = scalar offset into value array,  maptype 2  */

  ogg_int32_t q_min;  
  int         q_minp;  
  ogg_int32_t q_del;
  int         q_delp;
  int         q_seq;
  int         q_bits;
  int         q_pack;
  void       *q_val;   

} codebook;

extern void tremor_vorbis_book_clear(codebook *b);
extern int  tremor_vorbis_book_unpack(tremor_oggpack_buffer *b,codebook *c);

extern long tremor_vorbis_book_decode(codebook *book, tremor_oggpack_buffer *b);
extern long tremor_vorbis_book_decodevs_add(codebook *book, ogg_int32_t *a, 
				     tremor_oggpack_buffer *b,int n,int point);
extern long tremor_vorbis_book_decodev_set(codebook *book, ogg_int32_t *a, 
				    tremor_oggpack_buffer *b,int n,int point);
extern long tremor_vorbis_book_decodev_add(codebook *book, ogg_int32_t *a, 
				    tremor_oggpack_buffer *b,int n,int point);
extern long tremor_vorbis_book_decodevv_add(codebook *book, ogg_int32_t **a,
				     long off,int ch, 
				    tremor_oggpack_buffer *b,int n,int point);

extern int _ilog(unsigned int v);
