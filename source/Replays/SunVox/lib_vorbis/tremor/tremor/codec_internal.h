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

 function: libvorbis codec headers

 ********************************************************************/

#pragma once

#define CHUNKSIZE 1024

#include "codebook_tremor.h"
#include "ivorbiscodec.h"

#define VI_TRANSFORMB 1
#define VI_WINDOWB 1
#define VI_TIMEB 1
#define VI_FLOORB 2
#define VI_RESB 3
#define VI_MAPB 1

typedef void tremor_vorbis_info_floor;

/* tremor_vorbis_dsp_state buffers the current vorbis audio
   analysis/synthesis state.  The DSP state belongs to a specific
   logical bitstream ****************************************************/
struct tremor_vorbis_dsp_state{
  tremor_vorbis_info    *vi;
  tremor_oggpack_buffer  opb;

  ogg_int32_t   **work;
  ogg_int32_t   **mdctright;
  int             out_begin;
  int             out_end;

  long lW;
  long W;

  ogg_int64_t granulepos;
  ogg_int64_t sequence;
  ogg_int64_t sample_count;

};


/* Floor backend generic *****************************************/

extern tremor_vorbis_info_floor *floor0_info_unpack(tremor_vorbis_info *,tremor_oggpack_buffer *);
extern void floor0_free_info(tremor_vorbis_info_floor *);
extern int floor0_memosize(tremor_vorbis_info_floor *);
extern ogg_int32_t *floor0_inverse1(struct tremor_vorbis_dsp_state *,
				    tremor_vorbis_info_floor *,ogg_int32_t *);
extern int floor0_inverse2 (struct tremor_vorbis_dsp_state *,tremor_vorbis_info_floor *,
			    ogg_int32_t *buffer,ogg_int32_t *);

extern tremor_vorbis_info_floor *floor1_info_unpack(tremor_vorbis_info *,tremor_oggpack_buffer *);
extern void floor1_free_info(tremor_vorbis_info_floor *);
extern int floor1_memosize(tremor_vorbis_info_floor *);
extern ogg_int32_t *floor1_inverse1(struct tremor_vorbis_dsp_state *,
				    tremor_vorbis_info_floor *,ogg_int32_t *);
extern int floor1_inverse2 (struct tremor_vorbis_dsp_state *,tremor_vorbis_info_floor *,
			    ogg_int32_t *buffer,ogg_int32_t *);

typedef struct{
  int   order;
  long  rate;
  long  barkmap;

  int   ampbits;
  int   ampdB;

  int   numbooks; /* <= 16 */
  char  books[16];

} tremor_vorbis_info_floor0;

typedef struct{
  char  class_dim;        /* 1 to 8 */
  char  class_subs;       /* 0,1,2,3 (bits: 1<<n poss) */
  unsigned char  class_book;       /* subs ^ dim entries */
  unsigned char  class_subbook[8]; /* [VIF_CLASS][subs] */
} floor1class;  

typedef struct{
  floor1class  *class;          /* [VIF_CLASS] */
  char         *partitionclass; /* [VIF_PARTS]; 0 to 15 */
  ogg_uint16_t *postlist;       /* [VIF_POSIT+2]; first two implicit */ 
  char         *forward_index;  /* [VIF_POSIT+2]; */
  char         *hineighbor;     /* [VIF_POSIT]; */
  char         *loneighbor;     /* [VIF_POSIT]; */

  int          partitions;    /* 0 to 31 */
  int          posts;
  int          mult;          /* 1 2 3 or 4 */ 

} tremor_vorbis_info_floor1;

/* Residue backend generic *****************************************/

typedef struct tremor_vorbis_info_residue{
  int type;
  unsigned char *stagemasks;
  unsigned char *stagebooks;

/* block-partitioned VQ coded straight residue */
  long begin;
  long end;

  /* first stage (lossless partitioning) */
  int           grouping;         /* group n vectors per partition */
  char          partitions;       /* possible codebooks for a partition */
  unsigned char groupbook;        /* huffbook for partitioning */
  char          stages;
} tremor_vorbis_info_residue;

extern void res_clear_info(tremor_vorbis_info_residue *info);
extern int res_unpack(tremor_vorbis_info_residue *info,
		      tremor_vorbis_info *vi,tremor_oggpack_buffer *opb);
extern int res_inverse(tremor_vorbis_dsp_state *,tremor_vorbis_info_residue *info,
		       ogg_int32_t **in,int *nonzero,int ch);

/* mode ************************************************************/
typedef struct {
  unsigned char blockflag;
  unsigned char mapping;
} tremor_vorbis_info_mode;

/* Mapping backend generic *****************************************/
typedef struct coupling_step{
  unsigned char mag;
  unsigned char ang;
} coupling_step;

typedef struct submap{
  char floor;
  char residue;
} submap;

typedef struct tremor_vorbis_info_mapping{
  int            submaps; 
  
  unsigned char *chmuxlist;
  submap        *submaplist;

  int            coupling_steps;
  coupling_step *coupling;
} tremor_vorbis_info_mapping;

extern int mapping_info_unpack(tremor_vorbis_info_mapping *,tremor_vorbis_info *,
			       tremor_oggpack_buffer *);
extern void mapping_clear_info(tremor_vorbis_info_mapping *);
extern int mapping_inverse(struct tremor_vorbis_dsp_state *,tremor_vorbis_info_mapping *);

/* codec_setup_info contains all the setup information specific to the
   specific compression/decompression mode in progress (eg,
   psychoacoustic settings, channel setup, options, codebook
   etc).  
*********************************************************************/

typedef struct codec_setup_info {

  /* Vorbis supports only short and long blocks, but allows the
     encoder to choose the sizes */

  long blocksizes[2];

  /* modes are the primary means of supporting on-the-fly different
     blocksizes, different channel mappings (LR or M/A),
     different residue backends, etc.  Each mode consists of a
     blocksize flag and a mapping (along with the mapping setup */

  int        modes;
  int        maps;
  int        floors;
  int        residues;
  int        books;

  tremor_vorbis_info_mode       *mode_param;
  tremor_vorbis_info_mapping    *map_param;
  char                   *floor_type;
  tremor_vorbis_info_floor     **floor_param;
  tremor_vorbis_info_residue    *residue_param;
  codebook               *book_param;

} codec_setup_info;

extern tremor_vorbis_dsp_state *tremor_vorbis_dsp_create(tremor_vorbis_info *vi);
extern void     tremor_vorbis_dsp_destroy(tremor_vorbis_dsp_state *v);
extern int      tremor_vorbis_dsp_headerin(tremor_vorbis_info *vi,tremor_vorbis_comment *vc,
				    tremor_ogg_packet *op);

extern int      tremor_vorbis_dsp_restart(tremor_vorbis_dsp_state *v);
extern int      tremor_vorbis_dsp_synthesis(tremor_vorbis_dsp_state *vd,
				     tremor_ogg_packet *op,int decodep);
extern int      tremor_vorbis_dsp_pcmout(tremor_vorbis_dsp_state *v,
				  ogg_int16_t *pcm,int samples);
extern int      tremor_vorbis_dsp_read(tremor_vorbis_dsp_state *v,int samples);
extern long     tremor_vorbis_packet_blocksize(tremor_vorbis_info *vi,tremor_ogg_packet *op);
