/**
 * @file   mix_common.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  common parts for none, lerp and qerp mixer.
 */

#ifndef NAME
#error NAME should be defined
#endif

#ifndef DESC
#error DESC should be defined
#endif

#if !defined (FP) || (FP > 16) || (FP < 7)
# error undefined of invalid FP
#endif

#define xtr(X) str(X)
#define str(X) #X

#include <string.h>

typedef struct mix_fp_s mix_fp_t;
typedef struct mix_chan_s mix_chan_t;

#define BLKMAX 64

struct mix_chan_s {
  uint8_t *pcm, *end;
  u32_t idx, lpl, len, xtp;
  i16_t buf[BLKMAX];
};

struct mix_fp_s {
  mix_chan_t chan[4];
};

#define SETPCM() OPEPCM(=);
#define ADDPCM() OPEPCM(+=);

static inline void
mix_blk(mix_chan_t * const restrict K, int n)
{
  const uint8_t * const pcm = (const uint8_t *)K->pcm;
  const u32_t stp = K->xtp;
  u32_t idx = K->idx;
  i16_t * b;

  zz_assert( n >= 0 );
  if (n <= 0)
    return;

  b = K->buf;

  if ( K->pcm ) {
    zz_assert( n <= BLKMAX );
    zz_assert( K->pcm );
    zz_assert( K->end > K->pcm );
    zz_assert( K->xtp > 0 );
    zz_assert( K->idx >= 0 && K->idx < K->len );

    if (!K->lpl) {
      /* Instrument does not loop */
      do {
        SETPCM();
        /* Have reach end ? */
        if ( unlikely (idx >= K->len) ) {
          K->pcm = 0;                     /* This only is mandatory */
          --n;
          break;
        }
      } while (--n);
    } else {
      const u32_t off = K->len - K->lpl;  /* loop start index */
      /* Instrument does loop */
      do {
        SETPCM();
        /* Have reach end ? */
        if (idx >= K->len) {
          u32_t ovf = idx - K->len;
          if (ovf >= K->lpl) ovf %= K->lpl;
          idx = off+ovf;
          zz_assert( idx >= off && idx < K->len );
        }
      } while (--n);
    }
  }
  K->idx = idx;
  if (n > 0)
    do {
      *b++ = 0;
    } while (--n);
}

static u32_t xstep(u32_t stp, u32_t ikhz, u32_t ohz)
{
  /* stp is fixed-point 16
   * res is fixed-point FP
   * 1000 = 2^3 * 5^3
   */
  u64_t const tmp = (FP > 12)
    ? (u64_t) stp * ikhz * (1000u >> (16-FP)) / ohz
    : (u64_t) stp * ikhz * (1000u >> 3) / (ohz << (16-FP-3))
    ;
  u32_t const res = tmp;
  zz_assert( (u64_t)res == tmp );       /* check overflow */
  zz_assert( res );
  return res;
}

static i16_t
push_cb(core_t * const P, void * restrict pcm, i16_t N)
{
  mix_fp_t * const M = (mix_fp_t *)P->data;
  int k;
  i16_t rem = N;

  zz_assert( P );
  zz_assert( M );
  zz_assert( pcm );
  zz_assert( N != 0 );
  zz_assert( N > 0 );

  /* Setup channels */
  for (k=0; k<4; ++k) {
    chan_t     * const C = P->chan+k;
    mix_chan_t * const K = M->chan + C->pam;
    const u8_t trig = C->trig;

    C->trig = TRIG_NOP;
    switch (trig) {
    case TRIG_NOTE:
      zz_assert( C->note.ins == P->vset.inst+C->curi );
      K->idx = 0;
      K->pcm = C->note.ins->pcm;
      K->len = C->note.ins->len << FP;
      K->lpl = C->note.ins->lpl << FP;
      K->end = C->note.ins->end + K->pcm;

    case TRIG_SLIDE:
      K->xtp = xstep(C->note.cur, P->song.khz, P->spr);
      break;

    case TRIG_STOP: K->pcm = 0;
    case TRIG_NOP:
      break;
    default:
      zz_assert( !"wtf" );
      return -1;
    }
  }

  zz_assert( P->lr8 <= 256 );
  while (rem > 0) {
    const int n = rem < BLKMAX ? rem : BLKMAX;
    rem -= n;

    /* src voices */
    for (k=0; k<4; ++k)
      mix_blk(M->chan+k, n);

    map_i16_to_i16(pcm,
                   M->chan[0].buf, M->chan[1].buf,
                   M->chan[2].buf, M->chan[3].buf,
                   256-P->lr8, P->lr8, n);
    pcm = ((int32_t*)pcm) + n;
  }

  return N;
}

static void * local_calloc(u32_t size, zz_err_t * err)
{
  void * ptr = 0;
  *err =  zz_memnew(&ptr,size,1);
  return ptr;
}

static zz_err_t init_cb(core_t * const P, u32_t spr)
{
  zz_err_t ecode = E_OK;
  mix_fp_t * M = local_calloc(sizeof(mix_fp_t), &ecode);

  if (likely(M)) {
    zz_assert( !P->data );
    P->data = M;
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

    ecode = init_meth(P);
  }

  return ecode;
}

static void free_cb(core_t * const P)
{
  zz_memdel(&P->data);
}

mixer_t SYMB =
{
  NAME ":" METH, DESC, init_cb, free_cb, push_cb
};
