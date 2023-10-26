/**
 * @file   zz_core.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2018-03-06
 * @brief  Quartet player core.
 */

#define ZZ_DBG_PREFIX "(cor) "
#include "zz_private.h"

#if defined __m68k__ && defined SC68
#define SETCOLOR(X) /* *(volatile uint16_t *)0xFFFF8240 = (X) */
#else
#define SETCOLOR(X)
#endif

/* ---------------------------------------------------------------------- */

#ifndef PACKAGE_STRING
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION
#endif

/* reverse mapping */
static uint8_t chan_maps[3][4] = {
  /* ZZ_MAP_ABCD */ { 0,1,2,3 }, /* A B C D */
  /* ZZ_MAP_ACBD */ { 0,2,1,3 }, /* A C B D */
  /* ZZ_MAP_ADBC */ { 0,2,3,1 }  /* A C D B */
};

zz_u8_t  zz_chan_map = ZZ_MAP_ABCD;
zz_u16_t zz_chan_lr8 = BLEND_DEF;

zz_u32_t
zz_core_blend(core_t * K, zz_u8_t map, zz_u16_t lr8)
{
  zz_u32_t old
    = !K
    ? ( (zz_u32_t) zz_chan_lr8 << 16 ) | zz_chan_map
    : ( (zz_u32_t) K->lr8 << 16 ) | K->cmap
    ;

  if (map <= 2u) {
    if (!K)
      zz_chan_map = map;
    else {
      u8_t k;
      K->cmap = map;
      for (k=0; k<4; ++k)
        K->chan[k].pam = chan_maps[map][k];
    }
  }

  if (lr8 <= 256u) {
    if (!K)
      zz_chan_lr8 = lr8;
    else
      K->lr8 = lr8;
  }

#ifndef NDEBUG
  if (K) {
    i8_t k; char s[5];
    for (k=0; k<4;++k)
      s[K->chan[k].pam] = 'A'+k;
    s[k] = 0;
    dmsg("blending: %3hu (%hu) %s\n",
         HU(K->lr8), HU(K->cmap), s);
  }
#endif

  return old;
}

const char *
zz_core_version(void)
{
  return "Zingzong";
}

/* ---------------------------------------------------------------------- */

uint8_t
zz_core_mute(core_t * K, uint8_t clr, uint8_t set)
{
  const uint8_t old = K->mute;
  K->mute = (K->mute & ~clr) | set;
  return old;
}

/* ---------------------------------------------------------------------- */

/* Offset between 2 sequences (should be multiple of 12). */
static inline
u16_t ptr_off(const sequ_t * const a, const sequ_t * const b)
{
  const u32_t off = (const int8_t *) b - (const int8_t *) a;
  zz_assert( a <= b );
  zz_assert( off < 0x10000 );
  return off;
}

/* Index (position) of seq in this channel */
static inline
u16_t seq_idx(const chan_t * const C, const sequ_t * const seq)
{
  zz_assert (sizeof(sequ_t) == 12u);
  return divu(ptr_off(C->seq,seq),sizeof(sequ_t));
}

/* Offset of seq in this channel */
static inline
u16_t seq_off(const chan_t * const C, const sequ_t * const seq)
{
  return ptr_off(C->seq, seq);
}

static inline
sequ_t * loop_seq(const chan_t * const C, const uint16_t off)
{
  return (sequ_t *)&off[(int8_t *)C->seq];
}

#ifndef NDEBUG
static int is_valid_note(const i32_t stp)
{
  return stp >= SEQ_STP_MIN && stp <= SEQ_STP_MAX;
}
#endif

static void
zz_core_chan(core_t * const K, chan_t * const C)
{
  sequ_t * seq;

  zz_assert( C->trig == TRIG_NOP );
  C->trig = TRIG_NOP;

  /* Ignored voices ? */
  if ( 0x0F & K->mute & C->msk )
    return;

  /* Portamento */
  if (C->note.stp) {

    /* dmsg("%c[%hu] slide from:%08lx to:%08lx by:%08lx\n", */
    /*      'A'+C->num, HU(seq_idx(C,C->cur)), */
    /*      LU(C->note.cur), LU(C->note.aim), LU((uint32_t)C->note.stp)); */

    zz_assert(is_valid_note(C->note.cur));
    zz_assert(is_valid_note(C->note.aim));
    if (!C->note.cur) {
      dmsg("%c[%hu]: slide from nothing (to:%08lx by:%08lx)\n",
           'A'+C->num, HU(seq_idx(C,C->cur)),
           LU(C->note.aim), LU((uint32_t)C->note.stp));
      C->note.cur = C->note.aim; /* safety net */
    }
    C->trig = TRIG_SLIDE;
    C->note.cur += C->note.stp;
    if (C->note.stp > 0) {
      if (C->note.cur >= C->note.aim) {
        C->note.cur = C->note.aim;
        C->note.stp = 0;
      }
    } else if (C->note.cur <= C->note.aim) {
      C->note.cur = C->note.aim;
      C->note.stp = 0;
    }
  }

  seq = C->cur;
  if (C->wait) --C->wait;
  while (!C->wait) {
    /* This could be an endless loop on empty track but it should
     * have been checked earlier ! */
    u16_t const cmd = U16(seq->cmd);
    u16_t const len = U16(seq->len);
    u32_t const stp = U32(seq->stp);
    u32_t const par = U32(seq->par);
    ++seq;

    /* dmsg("%c[%hu]: cmd:%c len:%04hX stp:%08lX par:%08lX\n", */
    /*      'A'+C->num, HU(seq_idx(C,seq-1)), */
    /*      (int) cmd, HU(len), LU(stp), LU(par)); */

    switch (cmd) {

    case 'F':                           /* Finish */
      if ( ! ( K->loop & C->msk ) )
        dmsg("%c[%hu] loops @%lu\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(K->tick));

      seq = C->seq;
      K->loop |= C->msk;            /* Set has_loop flags */
      C->loop_sp = C->loops;            /* Safety net */
      break;

    case 'V':                           /* Voice-Set */
      C->curi = par >> 2;
      break;

    case 'P':                           /* Play-Note */
      if (C->curi < 0 || C->curi >= /*K->vset.nbi*/sizeof(K->vset.inst)/sizeof(K->vset.inst[0])) { /* rePlayer */
        dmsg("%c[%hu]@%lu: using invalid instrument number -- %hu/%hu\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(K->tick),
             HU(C->curi+1),HU(K->vset.nbi));
        K->code = E_SNG;
        return;
      }
      if (!K->vset.inst[C->curi].len) {
        dmsg("%c[%hu]@%lu: using tainted instrument -- I#%02hu\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(K->tick),
             HU(C->curi+1));
//         K->code = E_SNG; /* rePlayer */
//         return; /* rePlayer */
      }

      C->trig     = TRIG_NOTE;
      C->note.ins = K->vset.inst + C->curi;
      C->note.cur = C->note.aim = stp;
      C->note.stp = 0;
      C->wait     = len;
      break;

    case 'S':                       /* Slide-to-note */

      /* GB: This actually happened (e.g. Wrath of the demon - tune 1)
       *     I'm not sure what the original quartet player does.
       *     Here I'm just starting the goal note.
       *
       * zz_assert(C->note.cur);
       */
      if (!C->note.cur) {
        C->trig     = TRIG_NOTE;
        C->note.ins = K->vset.inst + C->curi;
        C->note.cur = stp;
      }
      C->wait     = len;
      C->note.aim = stp;
      C->note.stp = (int32_t)par; /* sign extend, slides are signed */
      break;

    case 'R':                       /* Rest */
      C->trig     = TRIG_STOP;
      C->wait     = len;
      C->note.stp = 0;
      C->note.cur = 0;
      /* GB: The original singsong.prg actually set the current note
       *     to 0.
       */
      break;

    case 'l':                       /* Set-Loop-Point */
      if (C->loop_sp < C->loops+MAX_LOOP) {
        struct loop_s * const l = C->loop_sp++;
        l->off = seq_off(C,seq);
        l->cnt = 0;
      } else {
        dmsg("%c[%hu]@%lu -- loop stack overflow\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(K->tick));
        K->code = E_PLA;
        return;
      }
      break;

    case 'L':                       /* Loop-To-Point */
    {
      struct loop_s * l = C->loop_sp-1;

      if (l < C->loops) {
        C->loop_sp = (l = C->loops) + 1;
        l->cnt = 0;
        l->off = 0;
      }

      if ( ( l->cnt = l->cnt ? l->cnt-1 : (par >> 16) ) )
        seq = loop_seq(C,l->off);
      else {
        --C->loop_sp;
        zz_assert(C->loop_sp >= C->loops);
      }

    } break;

    default:
      dmsg("%c[%hu]@%lu command <%c> #%04hX\n",
           'A'+C->num, HU(seq_idx(C,seq-1)), LU(K->tick),
           isalpha(cmd) ? (char)cmd : '^', HU(cmd));
      K->code = E_PLA;
      return;
    } /* switch */
  } /* while !wait */
  C->cur = seq;

  /* Muted voices ? */
  if (0xF0 & K->mute & C->msk)
    C->trig = TRIG_STOP;
}

/* ---------------------------------------------------------------------- */

zz_err_t
zz_core_tick(core_t * const K)
{
  chan_t * C;
  ++ K->tick;
  K->loop &= 0x0F;                     /* clear non-persistent part */
  for ( C=K->chan; C<K->chan+4; ++C )
    zz_core_chan(K,C);
  return K->code;
}

/* ---------------------------------------------------------------------- */

i16_t
zz_core_play(core_t * restrict K, void * restrict pcm, i16_t n)
{
  i16_t ret;

  zz_assert( K );
  zz_assert( K->mixer );

  SETCOLOR(0x755);
  if ( ! (ret = -K->code) ) {
    ret = -zz_core_tick(K);
    SETCOLOR(0x575);
    if (!ret) {
      ret = K->mixer->push(K, pcm, n);
      if (ret != n)
        ret = - (K->code = E_MIX);
    }
  }

  /* Reset triggers for all channels. */
  K->chan[0].trig = K->chan[1].trig =
    K->chan[2].trig = K->chan[3].trig = TRIG_NOP;

  SETCOLOR(0x777);

  return ret;
}

/* ---------------------------------------------------------------------- */

void
zz_core_kill(core_t * K)
{
  if (K) {
    if (K->mixer) {
      K->mixer->free(K);
      K->mixer = 0;
      K->data  = 0;
    }
    K->code = 0;
    K->spr  = 0;
  }
}

/* ---------------------------------------------------------------------- */

static void rt_check(void)
{
#ifndef NDEBUG
  static uint8_t bytes[4] = { 1, 2, 3, 4 };

  /* constant check */
  zz_assert( ZZ_FORMAT_UNKNOWN == 0 );
  zz_assert( ZZ_OK == 0 );
  zz_assert( ZZ_MIXER_DEF > 0 );

  /* built-in type check */
  zz_assert( sizeof(uint8_t)  == 1 );
  zz_assert( sizeof(uint16_t) == 2 );
  zz_assert( sizeof(uint32_t) == 4 );

  zz_assert( sizeof(zz_u8_t)  >= 1 );
  zz_assert( sizeof(zz_u16_t) >= 2 );
  zz_assert( sizeof(zz_u32_t) >= 4 );

  /* compound type check */
  zz_assert( sizeof(struct sequ_s) == 12 );

  /* byte order stuff */
  zz_assert( U32(bytes)   == 0x01020304 );
  zz_assert( U16(bytes)   == 0x00000102 );
  zz_assert( U16(bytes+2) == 0x00000304 );

  /* GB: When using m68k with -mnoshort the default int size is
   *     16bit. It is a problem for some of our bit-masks like
   *     instrument used for instance. We have to cast integer to a
   *     32bit type such as long integer with the a 'l' suffix. This
   *     test that.
   */
  zz_assert( (1l<<20) == 0x100000 );
#endif
  /* Be sure core is the first member of play_t */
  zz_assert( (void*)0 == &((play_t*)0)->core );
}

/* ---------------------------------------------------------------------- */

zz_err_t
zz_core_init(core_t * K, mixer_t * M, u32_t spr)
{
  zz_err_t ecode;
  u8_t k;

  rt_check();

  if (!K)
    return E_ARG;
  if (K->code)
    return K->code;
  if (K->mixer)
    return K->code = E_MIX;

  zz_memclr(K->chan,sizeof(K->chan));
  for (k=0; k<4; ++k) {
    chan_t * const C = K->chan+k;
    C->msk = 0x11 << k;
    C->num = k;
    C->cur = C->seq = K->song.seq[k];
    C->loop_sp = C->loops;
  }
  zz_core_blend(K, zz_chan_map, zz_chan_lr8);
  K->loop = 0x0F & K->mute; /* set ignored voices */
  K->tick = 0;

  if (!K->vset.iref)
    ecode = E_SET;
  else if (!M)
    ecode = E_MIX;
  else {
    K->spr = 0;
    K->mixer = M;
    ecode = M->init(K, spr);
  }

  return K->code = ecode;
}
