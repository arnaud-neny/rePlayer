/**
 * @file   zz_init.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet song and voice-set parser.
 */

#define ZZ_DBG_PREFIX "(ini) "
#include "zz_private.h"
#include <ctype.h>

/* An extension of qts file + my own extension. */
static int is_valid_tck(const uint16_t tck) {
  return !tck || (tck >= RATE_MIN && tck <= RATE_MAX);
}

/* {4..20} as defined as by the original replay routine. */
static int is_valid_khz(const uint16_t khz) {
  return khz >= 4 && khz <= 20;
}

/* Encountered values so far [8,12,16,24]. */
static int is_valid_bar(const uint16_t bar) {
  return bar >= 4 && bar <= 48 && !(bar & 3);
}

/* Encountered values so far {04..36}. */
static int is_valid_spd(const uint16_t tempo) {
  return tempo >= 1 && tempo <= 64;
}

/* Encountered values so far [2:4,3:4,4:4]. */
static int is_valid_sig(const uint8_t sigm, const uint8_t sigd) {
  return sigm >= 1 && sigd <= 4 && sigm <= sigd;
}

/* Instruments {1..20} */
static int is_valid_ins(const uint8_t ins) {
  return ins >= 1 && ins <= 20;
}

/* ----------------------------------------------------------------------
 * quartet song
 * ----------------------------------------------------------------------
 */

zz_err_t
song_init_header(song_t * song, const void *_hd)
{
  const uint8_t * hd = _hd;
  const uint16_t khz = U16(hd+0), bar = U16(hd+2), spd = U16(hd+4);
  uint16_t tck = U16(hd+12) == 0x5754 ? U16(hd+14) : 0;
  zz_err_t ecode;

  /* Parse song header */
  song->rate  = tck;
  song->khz   = khz;
  song->barm  = bar;
  song->tempo = spd;
  song->sigm  = hd[6];
  song->sigd  = hd[7];

  dmsg("song: rate:%huHz spr:%hukHz, bar:%hu, tempo:%hu, signature:%hu/%hu\n",
       HU(song->rate), HU(song->khz),
       HU(song->barm), HU(song->tempo), HU(song->sigm), HU(song->sigd));

  ecode = E_SNG & -!( is_valid_tck(song->rate)  &&
                      is_valid_khz(song->khz)   &&
                      is_valid_bar(song->barm)  &&
                      is_valid_spd(song->tempo) &&
                      is_valid_sig(song->sigm,song->sigd) );
  if (ecode != E_OK)
    emsg("invalid song header\n");
   return ecode;
}

static inline
u16_t seq_idx(const sequ_t * const org, const sequ_t * seq)
{
  return divu( (intptr_t)seq-(intptr_t)org, sizeof(sequ_t) );
}

struct loop_stack_s {
  uint32_t len:31, has:1;
};
typedef struct loop_stack_s loop_stack_t[MAX_LOOP+1];

/* collapse loop stack */
static u8_t collapse_one(loop_stack_t loops, u8_t ssp, u16_t times)
{
  u32_t const len = loops[ssp].len;
  u32_t const amt = len + (times ? mulu32(len,times) : 0);

  if (!ssp)
    loops[0].len = amt;
  else {
    --ssp;
    loops[ssp].has |= loops[ssp+1].has;
    loops[ssp].len += amt;
  }
  dmsg("%s +%hux%lu (%lu) => %lu\n",
       ".........."+10-(ssp+1),HU(times),LU(len),LU(amt), LU(loops[ssp].len));

  return ssp;
}

static void collapse_all(loop_stack_t loops, u8_t ssp)
{
  while (ssp)
    ssp = collapse_one(loops, ssp, 0);
}

zz_err_t
song_init(song_t * song)
{
  zz_err_t ecode=E_SNG;
  u16_t off, size;
  u8_t k, cur=0, ssp=0;

  /* sequ_t * out=0; */
  loop_stack_t loops;

  /* sequence to use in case of empty sequence to avoid endless loop
   * condition and allow to properly measure music length.
   */
  static uint8_t nullseq[2][12] = {
    { 0,'R', 0, 1 },                    /* "R"est 1 tick */
    { 0,'F' }                           /* "F"inish      */
  };

  /* Clean-up */
  song->seq[0] = song->seq[1] = song->seq[2] = song->seq[3] = 0;
  song->stepmin = song->stepmax = 0;
  song->iuse = song->iref = 0;
  song->ticks = 0;

  /* Basic parsing of the sequences to find the end. It replaces
   * empty sequences by something that won't loop endlessly and won't
   * disrupt the end of music detection.
   */
  for (k=0, off=11, size=song->bin->len;
       k<4 && off<size;
       off += 12)
  {
    sequ_t * const seq = (sequ_t *)(song->bin->ptr+off-11);
    u16_t    const cmd = U16(seq->cmd);
    u16_t    const len = U16(seq->len);
    u32_t    const stp = U32(seq->stp);
    u32_t    const par = U32(seq->par);
    u8_t           s12;

    if (!song->seq[k]) {
      /* new sequence starts */
      song->seq[k] = seq;           /* new channel sequence       */
      cur = 0;                      /* current instrument (0:def) */
      ssp = 0;                      /* loop stack pointer         */
      loops[0].len = 0;             /*  */
      loops[0].has = 0;             /* #0:note #1:wait            */
      /*out = seq;*/                    /* on the fly patch */
    }

    switch (cmd) {

    case 'F':                           /* Finish */
      if (ssp) {
        dmsg("(song) %c[%hu] loop not closed -- %hu\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), HU(ssp));
        collapse_all(loops,ssp);
      }

      if (!loops[0].has) {
        song->seq[k] = (sequ_t *) nullseq;
        loops[0].len = 1;
      }
      dmsg("%c duration: %lu ticks\n",'A'+k, LU(loops[0].len));
      if ( loops[0].len > song->ticks)
        song->ticks = loops[0].len;
      ++k;
      break;

    case 'P':                           /* Play-Note */
      loops[ssp].has = 1;
      song->iuse |= 1l << cur;

    case 'S':                           /* Slide-To-Note */
      if (stp < SEQ_STP_MIN || stp > SEQ_STP_MAX) {
        emsg("song: %c[%hu] step out of range -- %08lx\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), LU(stp));
        goto error;
      }
      if (!song->stepmax)
        song->stepmax = song->stepmin = stp;
      else if (stp > song->stepmax)
        song->stepmax = stp;
      else if (stp < song->stepmin)
        song->stepmin = stp;

      s12 = (stp+4095) >> 12;
      if (s12 > song->istep[cur])
        song->istep[cur] = s12;

    case 'R':                           /* Rest */
      if (!len) {
        emsg("song: %c[%hu] length out of range -- %08lx\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), LU(len));
        goto error;
      }
      loops[ssp].len += len;
      break;

    case 'l':                           /* Set-Loop-Point */
      if (ssp == MAX_LOOP) {
        emsg("song: %c[%hu] loop stack overflow\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)));
        goto error;
      }
      ++ssp;
      loops[ssp].len = 0;
      loops[ssp].has = 0;
      break;

    case 'L':                           /* Loop-To-Point */
      dmsg("%c[%05hu][%hu] ",'A'+k, HU(seq_idx(song->seq[k],seq)), HU(ssp));
      ssp = collapse_one(loops, ssp, par>>16);
      break;

    case 'V':                           /* Voice-Set */
      cur = par >> 2;
      if ( cur >= 20 || (par & 3) ) {
        emsg("song: %c[%hu] instrument out of range -- %08lx\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), LU(par));
        goto error;
      }
      song->iref |= 1l << cur;
      break;

    default:
      /* dmsg("%c[%hu] invalid sequence -- %04hx-%04hx-%08lx-%08lx\n", */
      /*      'A'+k, HU(seq_idx(song->seq[k],seq)), */
      /*      HU(cmd), HU(len), LU(stp), LU(par)); */
      /* seq = 0;                          /\* mark invalid *\/ */
      /* break; */

#ifndef NDEBUG
      collapse_all(loops, ssp);
      dmsg("%c (truncated) duration: %lu ticks\n",'A'+k, LU(loops[0].len));
#endif
      emsg("song: %c[%hu] invalid sequence -- %04hx-%04hx-%08lx-%08lx\n",
           'A'+k, HU(seq_idx(song->seq[k],seq)),
           HU(cmd), HU(len), LU(stp), LU(par));
      goto error;
    }

    /* if (seq == out) */
    /*   ++out; */
    /* else if (seq) { */
    /*   int i; */
    /*   for (i=0; i<12; ++i) { */
    /*     ((uint8_t*)out)[i] = ((uint8_t*)seq)[i]; */
    /*   } */
    /*   ++out; */
    /* } */
  }

  if ( (off -= 11) != song->bin->len) {
    dmsg("garbage data after voice sequences -- %lu bytes\n",
         LU(song->bin->len) - off);
  }

#ifdef ZZ_MINIMAL
  if (k != 4) {
    emsg("song: channel %c is truncated\n",'A'+k);
    goto error;
  }
#else
  for ( ;k<4; ++k) {
    ssp = 0;
    if (loops[0].has) {
      /* Add 'F'inish mark */
      sequ_t * const seq = (sequ_t *) (song->bin->ptr+off);
      seq->cmd[0] = 0; seq->cmd[1] = 'F'; off += 12;

      collapse_all(loops, ssp);
      loops[0].has = 0;

      dmsg("%c duration: %lu ticks\n",'A'+k, LU(loops[0].len));
      if ( loops[0].len > song->ticks)
        song->ticks = loops[0].len;
      wmsg("channel %c is truncated\n", 'A'+k);
    } else {
      wmsg("channel %c is MIA\n", 'A'+k);
      song->seq[k] = (sequ_t *) nullseq;
    }
  }
#endif
  dmsg("song steps: %08lx .. %08lx\n", LU(song->stepmin), LU(song->stepmax));
  dmsg("song instruments: used: %05lx referenced:%05lx difference:%05lx\n",
       LU(song->iuse), LU(song->iref), LU(song->iref^song->iuse));
  ecode = E_OK;

error:
  if (ecode)
    bin_free(&song->bin);

  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet voiceset
 * ----------------------------------------------------------------------
 */

zz_err_t
vset_init_header(vset_t *vset, const void * _hd)
{
  const uint8_t *hd = _hd;
  int ecode = E_SET;

  /* header */
  vset->khz = *hd++;
  vset->nbi = *hd++ - 1;
  vset->nul = 128;
  vset->one = 255;
  vset->unroll = 0;
  vset->iref = 0;
  dmsg("vset: spr:%hukHz, ins:%hu/0x%05hx\n",
       HU(vset->khz), HU(vset->nbi), HU(vset->iref));

  if (is_valid_khz(vset->khz) && is_valid_ins(vset->nbi)) {
    i8_t i;

    hd += 7*20;                         /* skip instrument names */
    /* copy instrument offset and setup */
    for (i=0; i<20; ++i, hd+=4) {
      inst_t * const inst = vset->inst+i;
      inst->pcm = (uint8_t *) (intptr_t) U32(hd);
    }
    ecode = E_OK;
  }
  return ecode;
}

zz_err_t
vset_init(vset_t * const vset)
{
  bin_t * const bin = vset->bin;
  u32_t imsk;
  u8_t i;

  zz_assert( vset->nbi > 0 && vset->nbi <= 20 );

  /* imsk is best set before when possible so that unused (yet valid)
   * instruments can be ignored. This gives a little more space to
   * unroll loops and save a bit of processing during init. However it
   * might destroy instrument used by other songs using the same
   * voiceset.
   *
   * GB: Currently using the referenced instrument mask instead of
   * used instrument mask. That's because in some cases where an
   * instrument is selected at the end of a loop it can be used
   * without being properly detected (due to init code not following
   * loops). It's possible (yet unlikely) for an instrument to be
   * referenced but not being used. That's the lesser of two evils.
   *
   */
  imsk = vset->iref ? vset->iref : (1l<<vset->nbi)-1;

  for (i=0, vset->iref=0; i<20; ++i) {
    inst_t * const inst = vset->inst+i;
    const i32_t off = (intptr_t)inst->pcm - 222 + 8;
    u32_t len = 0, lpl = 0;
    uint8_t * pcm = 0;
#ifdef DEBUG_LOG
    const char * tainted = 0;
#   define TAINTED(COND) if ( COND ) { tainted = #COND; break; } else
#else
#   define TAINTED(COND) if ( COND ) { break; } else
#endif

    do {
      /* Sanity tests */
      TAINTED (!(1&(imsk>>i)));         /* instrument in used ? */
      TAINTED (off < 8);                /* offset in range ? */
      TAINTED (off >= bin->len);        /* offset in range ? */

      /* Get sample info */
      pcm = bin->ptr + off;
      len = U32(pcm-4);
      lpl = U32(pcm-8);
      if (lpl == 0xFFFFFFFF) lpl = 0;

      /* Some of these tests might be a tad conservative but as the
       * format is not very robust It might be a nice safety net.
       * Currently I haven't found a music file that is valid and does
       * not pass them.
       */
      TAINTED (len & 0xFFFF);           /* clean length ? */
      TAINTED (lpl & 0xFFFF);           /* clean loop ? */
      TAINTED ((len >>= 16) == 0);      /* have data ? */
      TAINTED ((lpl >>= 16) > len);     /* loop inside sample ? */
      TAINTED (off+len > bin->len);     /* sample in range ? */

      vset->iref |= 1l << i;
      dmsg("I#%02hu [$%05lX:$%05lX:$%05lX] [$%05lX:$%05lX:$%05lX]\n",
           HU(i+1),
           LU(0), LU(len-lpl), LU(len),
           LU(off), LU(off+len-lpl), LU(off+len));
    } while (0);

    if ( !(1 & (vset->iref>>i)) ) {
      dmsg("I#%02hu was tainted by (%s)\n",HU(i+1),tainted);
      pcm = 0;
      len = lpl = 0;                   /* If not used mark as dirty */
    }

    zz_assert( (!!len) == (1 & (vset->iref>>i)) );
    zz_assert( len < 0x10000 );
    zz_assert( lpl <= len );

    inst->end = len;
    inst->len = len;
    inst->lpl = lpl;
    inst->pcm = pcm;
  }

  return E_OK;
}

static u8_t
sort_inst(const inst_t inst[], uint8_t idx[], u32_t iuse)
{
  uint8_t nbi;

  /* while instruments remaining */
  for ( nbi=0; iuse; ) {
    u8_t i, j; u32_t irem;

    /* find first */
    for (i=0; ! ( iuse & (1l<<i) )  ; ++i)
      ;
    irem = iuse & ~(1l<<i);
    zz_assert ( inst[i].pcm );
    zz_assert ( inst[i].len );

    /* find best */
    for (j=i+1; irem; ++j) {
      if (irem & (1l<<j)) {
        irem &= ~(1l<<j);
        zz_assert ( inst[j].pcm );
        zz_assert ( inst[j].len );
        if ( inst[j].pcm < inst[i].pcm )
          i = j;
      }
    }
    idx[nbi++] = i;
    iuse &= ~(1l<<i);
  }

  return nbi;
}

#define pcmcpy(D,S,L,T) zz_memxla((D),(S),(T),(L))

static void
unroll_loop(uint8_t * dst, uint8_t * const end, i32_t lpl, const uint8_t nul)
{
  zz_assert( dst < end );
  if (lpl)
#if defined __m68k__
    asm (
      "    subq.l  #1,%[cnt]            \n"
      "    swap    %[cnt]               \n"
      "2:  swap    %[cnt]               \n"
      "1:  move.b  (%[src])+,(%[dst])+  \n"
      "    dbf     %[cnt],1b            \n"
      "    swap    %[cnt]               \n"
      "    dbf     %[cnt],2b            \n\t"
      :: [dst] "a" (dst), [src] "a" (dst-lpl), [cnt] "d" (end-dst));
#else
    do {
      *dst = dst[-lpl];
    } while (++dst < end);
#endif
  else
    zz_memset(dst,nul,end-dst);
}

zz_err_t
vset_unroll(vset_t * const vset, const uint8_t *tohw)
{
  bin_t   * const bin = vset->bin;
  uint8_t * const beg = bin->ptr;
  uint8_t * const end = beg + bin->max;
  const uint8_t nul = tohw ? tohw[128] : 128;

  u8_t nbi, i;
  uint8_t idx[20];

  uint8_t * e;
  u32_t tot, unroll;

  if (vset->unroll) {
    dmsg("voice-set already prepared 0:%02hX 1:%02hX unroll:%lu\n",
         HU(vset->nul), HU(vset->one), LU(vset->unroll));
    return E_OK;
  }

  nbi = sort_inst(vset->inst, idx, vset->iref);
  if (!nbi)
    return E_SET;

  /* 3 pass loop unroll is the only way to unsure not to trash some
   * sample during the process. */

  /* Pass#1: Stack all samples at the bottom of the buffer; changing
   *         sign in the process and computing the total space needed
   *         to store them all.
   */
  for (i=1, e=end, tot=0; i <= nbi; ++i) {
    inst_t * const inst = &vset->inst[idx[nbi-i]];
    pcmcpy(e -= inst->len, inst->pcm, inst->len, tohw);
    inst->pcm = e;
    tot += inst->len;
  }

  if (tohw) {
    vset->nul = tohw[vset->nul];
    vset->one = tohw[vset->one];
  }

  unroll = divu32(bin->max-tot, nbi);
  dmsg("total space is: %lu/%lu +%lu/spl\n",
       LU(tot), LU(bin->max),LU(unroll));

  /* We really need at least an amount of 2 unrolled pcm for the
   * quadratic interpolation to work properly. As the sample have an 8
   * bytes header it should always be alright.
   */
  zz_assert(unroll >= 2);

  /* Pass#2: Move the sample to their finale location. */
  for (i=0, e=beg; i<nbi; ++i) {
    const intptr_t align = 2-1; // $$$
    inst_t * const inst = &vset->inst[idx[i]];
    uint8_t * const a = (uint8_t *)((intptr_t) (e + align) & ~align);

    zz_assert( inst->pcm );
    zz_assert( inst->len );
    zz_memcpy(a, inst->pcm, inst->len); /* zz_memcpy() is memmove() */
    inst->pcm = a;
    e += inst->len+unroll;
  }

  /* Pass#3 finally unroll the loop */
  for (i=0; i<nbi; ) {
    inst_t * const inst = &vset->inst[idx[i]];
    uint8_t * pcm = inst->pcm;
    uint8_t * mcp = ++i < nbi ? vset->inst[idx[i]].pcm : end;

    inst->end = mcp-pcm;
    pcm += inst->len;
    zz_assert(pcm+2 < mcp);
    unroll_loop(pcm, mcp, inst->lpl, nul);
  }

  vset->unroll = unroll;

  return ZZ_OK;
}
