/**
 * @file   mix_none.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Low quality no interpolation mixer.
 */

#define NAME "int"
#define METH "none"
#define SYMB mixer_zz_none
#define DESC "no interpolation (lightning fast/LQ)"

#define ZZ_DBG_PREFIX "(mix-" METH  ") "
#include "zz_private.h"

#define OPEPCM(OP) do {                         \
    zz_assert( &pcm[idx>>FP] < K->end );        \
    *b++ OP ( ( pcm[idx>>FP]-128 ) << 8 );      \
    idx += stp;                                 \
  } while (0)

static zz_err_t init_meth(core_t *P)
{
  return ZZ_OK;
}

#include "mix_common.c"
