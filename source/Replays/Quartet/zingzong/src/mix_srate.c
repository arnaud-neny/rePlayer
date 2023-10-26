/**
 * @file   mix_srate.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High quality mixer using samplerate.
 */

#if WITH_SRATE == 1

#define ZZ_DBG_PREFIX "(mix-sinc) "
#define ZZ_ERR_PREFIX "(mix-sinc) "
#include "zz_private.h"
#include <string.h>
#include <samplerate.h>

#define F32MAX 48
#define FLIMAX (F32MAX)
#define FLOMAX (F32MAX)
#define BLKMAX (F32MAX)

typedef struct mix_data_s mix_data_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  int8_t id;                            /* { 'A','B','C','D' } */
  int8_t run;                           /* 0:voice stopped */

  SRC_STATE * st;
  double   rate;

  uint8_t *pta;                         /* base address */
  uint8_t *ptr;                         /* current address */
  uint8_t *ptl;                         /* loop address (0=no loop) */
  uint8_t *pte;                         /* end address */

  float    iflt[FLIMAX];                /* src input  buffer */
  float    oflt[FLOMAX];                /* src output buffer */
  float    buf[BLKMAX];                 /* channel pcm buffer*/
};

struct mix_data_s {
  int        quality;
  double     rate, irate, orate, rate_min, rate_max;
  mix_chan_t chan[4];
};

/* ----------------------------------------------------------------------

   samplerate helpers

   ---------------------------------------------------------------------- */

static inline void
emsg_srate(const mix_chan_t * const K, int err)
{
  emsg("src: %c: %s\n", K->id, src_strerror(err));
}

static inline double
iorate(const u32_t fp16, const double irate, const double orate)
{
  return (double)fp16 * irate / (65536.0*orate);
}

static inline double
rate_of_fp16(const u32_t fp16, const double rate) {
  return (double)fp16 * rate;
}

/* ----------------------------------------------------------------------

   Read input PCM

   ---------------------------------------------------------------------- */

static void
chan_flread(float * restrict d, mix_chan_t * const K, int n)
{
  zz_assert( d );
  zz_assert( K );
  zz_assert( n > 0 );

  while ( K->ptr && n ) {
    uint8_t * const ptr = K->ptr;
    int l = K->pte - K->ptr;

    if (l > n)
      K->ptr += ( l = n );
    else
      K->ptr = K->ptl;

    i8tofl(d, ptr, l);
    d += l;
    n -= l;
  }
  while (n--)
    *d++ = 0;
}

static long
fill_cb(void *_K, float **data)
{
  mix_chan_t * const restrict K = _K;
  int len;

  *data = K->iflt;
  len = !K->ptr ? 0 : FLIMAX;
  if (len > 0)
    chan_flread(K->iflt, K, len);
  return len;
}

static zz_err_t
restart_chan(mix_chan_t * const K)
{
  int err = src_reset(K->st);
  if (err) {
    emsg_srate(K,err);
    K->ptr = 0;
    K->run = 0;
    return E_MIX;
  }
  K->run = !! ( K->ptr = K->pta );
  return E_OK;
}

/* ----------------------------------------------------------------------

   Mixer Push

   ---------------------------------------------------------------------- */

static i16_t
push_cb(core_t * const P, void * pcm, i16_t N)
{
  mix_data_t * const M = (mix_data_t *) P->data;
  int k;
  i16_t rem = N;
  const float fscl = 32000.0 / 2.0 / 256.0;
  const float rscl = (float) P->lr8 * fscl;
  const float lscl = (float) (256-P->lr8) * fscl;

  zz_assert( P );
  zz_assert( M );
  zz_assert( pcm );
  zz_assert( N != 0 );
  zz_assert( N > 0 );

  /* Setup channels */
  for (k=0; k<4; ++k) {
    chan_t     * const C = P->chan+k;
    mix_chan_t * const K = M->chan+C->pam;

    switch (C->trig) {

    case TRIG_NOTE:
      zz_assert( C->note.ins );
      zz_assert( C->note.ins->len > 0 );

      K->pta = C->note.ins->pcm;
      K->pte = K->pta + C->note.ins->len;
      K->ptl = C->note.ins->lpl ? (K->pte - C->note.ins->lpl) : 0;
      C->note.ins = 0;
      if (restart_chan(K))
        return -1;

    case TRIG_SLIDE:
      K->rate = rate_of_fp16(C->note.cur, M->rate);
      break;
    case TRIG_STOP: K->run = 0;
    case TRIG_NOP:  break;
    default:
      emsg("INTERNAL ERROR: %c: invalid trigger -- %d\n", 'A'+k, C->trig);
      zz_assert( !"wtf" );
      return 0;
    }
  }

  /* Mix channels */
  while (rem > 0) {
    const int n = rem < BLKMAX ? rem : BLKMAX;
    rem -= n;

    /* 4 voices */
    for (k=0; k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      float * restrict flt = K->buf;
      int need;

      for ( need = n; need > 0; ) {
        int odone, want;

        if ( ! K->run ) {
          zz_memclr(flt, need * sizeof(*flt));
          break;
        }

        if ( (want = need) > FLOMAX )
          want = FLOMAX;

        odone = src_callback_read(K->st, 1.0/K->rate, want, K->oflt);
        if (odone < 0) {
          emsg_srate(K,src_error(K->st));
          return -1;
        }
        zz_assert ( odone <= want );

        if (odone < want) {
          zz_assert ( !K->ptr );
          if (!K->ptr)
            K->run = 0;
        }
        need -= odone;

        zz_memcpy(flt, K->oflt, sizeof(float)*odone);
        flt += odone;
      }
    }

    map_flt_to_i16(pcm,
                   M->chan[0].buf, M->chan[1].buf,
                   M->chan[2].buf, M->chan[3].buf,
                   lscl, rscl, n);
    pcm = ((int32_t*)pcm) + n;
  }

  return N;
}

/* ---------------------------------------------------------------------- */

static void free_cb(core_t * const P)
{
  mix_data_t * const M = (mix_data_t *) P->data;
  if (M) {
    int k;
    zz_assert( M == P->data );
    for (k=0; k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      if (K->st) {
        src_delete (K->st);
        K->st = 0;
      }
    }
    zz_free(&P->data);
  }
}

/* ---------------------------------------------------------------------- */

static zz_err_t init_srate(core_t * const P, u32_t spr, const int quality)
{
  zz_err_t ecode = E_SYS;
  int k;
  zz_assert( !P->data );
  zz_assert( sizeof(float) == 4 );

  switch (spr) {
  case 0:
  case ZZ_LQ: spr = mulu(P->song.khz,1000u); break;
  case ZZ_MQ: spr = SPR_DEF; break;
  case ZZ_FQ: spr = SPR_MIN; break;
  case ZZ_HQ: spr = SPR_MAX; break;
  }
  if (spr < SPR_MIN) spr = SPR_MIN;
  if (spr > SPR_MAX) spr = SPR_MAX;
  P->spr = spr;

  /* +1 float already allocated in mix_data_t struct */
  ecode = zz_calloc(&P->data, sizeof(mix_data_t));
  if (likely(E_OK == ecode)) {
    mix_data_t * M = P->data;
    zz_assert( M );
    M->quality  = quality;
    M->irate    = (double) P->song.khz * 1000.0;
    M->orate    = (double) P->spr;
    M->rate     = iorate(1, M->irate, M->orate);
    M->rate_min = rate_of_fp16(P->song.stepmin, M->rate);
    M->rate_max = rate_of_fp16(P->song.stepmax, M->rate);

    dmsg("iorates : %.3lf .. %.3lf\n", M->rate_min, M->rate_max);
    dmsg("irate   : %.3lf\n", M->irate);
    dmsg("orate   : %.3lf\n", M->orate);
    dmsg("rate    : %.3lf\n", M->rate);

    for (k=0, ecode=E_OK; ecode == E_OK && k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      int err = 0;
      K->id = 'A'+k;
      K->st = src_callback_new(fill_cb, quality, 1, &err, K);
      if (!K->st) {
        ecode = E_MIX;
        emsg_srate(K,err);
      } else {
        ecode = restart_chan(K);
      }
    }
  }

  if (ecode)
    free_cb(P);
  else
    i8tofl(0,0,0);                      /* trick to pre-init LUT */

  return ecode;
}

/* ---------------------------------------------------------------------- */

#define XONXAT(A,B) A##B
#define CONCAT(A,B) XONXAT(A,B)
#define XTR(A) #A
#define STR(A) XTR(A)

#define DECL_SRATE_MIXER(Q,QQ,D)                        \
  static zz_err_t init_##Q(core_t * const P, u32_t spr) \
  {                                                     \
    return init_srate(P, spr, SRC_##QQ);                \
  }                                                     \
  mixer_t mixer_srate_##Q =                             \
  {                                                     \
    "sinc:" XTR(Q), D, init_##Q, free_cb, push_cb       \
  }

DECL_SRATE_MIXER(best,SINC_BEST_QUALITY,
                 "band limited sinc (best quality)");
DECL_SRATE_MIXER(medium,SINC_MEDIUM_QUALITY,
                 "band limited sinc (medium quality)");
DECL_SRATE_MIXER(fast,SINC_FASTEST,
                 "band limited sinc (fastest)");

#endif /* WITH_SRATE == 1 */
