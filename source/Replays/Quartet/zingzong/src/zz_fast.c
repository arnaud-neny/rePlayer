/**
 * @file   zz_fast.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-05
 * @brief  Faster (and less solid) version of the Quartet player.
 */

#define ZZ_DBG_PREFIX "(fpl) "
#include "zz_private.h"

typedef struct zz_fast_s zz_fast_t;

/* GB: /!\ this structure can not be more than 12 bytes long /!\ */
struct zz_fast_s {
  uint8_t cmd;
  union {
    struct { uint16_t cnt; } loop;
    struct { uint16_t len; } rest;
    struct { inst_t * ins; } voice;
    struct { uint16_t len; uint32_t note; } play;
    struct { uint16_t len; uint32_t goal; int32_t step; } slide;
  };
};

enum {
  ZZ_FAST_END,
  ZZ_FAST_PLAY,
  ZZ_FAST_REST,
  ZZ_FAST_SLIDE,
  ZZ_FAST_LABEL,
  ZZ_FAST_LOOP,
  ZZ_FAST_VOICE,
};

int zz_fast_init(play_t * const P, chan_t * const C)
{
  sequ_t * seq;

  zz_assert( sizeof(zz_fast_t) <= 12 );

  for (seq = C->seq; ; ++seq) {
    u16_t const cmd = U16(seq->cmd);
    u16_t const len = U16(seq->len);
    u32_t const stp = U32(seq->stp);
    u32_t const par = U32(seq->par);
    zz_fast_t * fast = (zz_fast_t *) seq;

    if (cmd == 'E') {
      fast->cmd = ZZ_FAST_END;
      break;
    }

    switch (cmd) {
    case 'P':
      fast->cmd = ZZ_FAST_PLAY;
      fast->play.len  = len;
      fast->play.note = stp;
      break;
    case 'R':
      fast->cmd = ZZ_FAST_REST;
      fast->rest.len = len;
      break;
    case 'S':
      fast->cmd = ZZ_FAST_SLIDE;
      fast->slide.len  = len;
      fast->slide.goal = stp;
      fast->slide.step = (int32_t) par;
      break;
    case 'V':
      fast->cmd = ZZ_FAST_VOICE;
      fast->voice.ins = &P->core.vset.inst[par>>2];
      break;
    case 'L':
      fast->cmd = ZZ_FAST_LOOP;
      fast->loop.cnt = (par >> 16) + 1;
      break;
    case 'l':
      fast->cmd = ZZ_FAST_LABEL;
      break;
    default:
      zz_assert( "invalid command" );
      return E_SNG;
    }
  }

  return ZZ_OK;
}

static inline
u16_t seq2off(const chan_t * const C, const sequ_t * const seq)
{
  return (intptr_t) seq - (intptr_t) C->seq;
}

static inline
sequ_t * off2seq(const chan_t * const C, const u16_t off)
{
  return (sequ_t *) ( (intptr_t)C->seq + off );
}

int zz_fast_chan(play_t * const P, chan_t * const C)
{
  sequ_t * seq = C->cur;

  if ( 0x0F & P->core.mute & C->msk )
    return E_OK;

  C->trig = TRIG_NOP;

  /* Portamento */
  if (C->note.stp) {
    i32_t difa, difb;

    if (!C->note.cur)
      C->note.cur = C->note.aim; /* safety net */
    C->trig = TRIG_SLIDE;

    difa = C->note.cur - C->note.aim;
    difb = (C->note.cur += C->note.stp) - C->note.aim;

    if ( (difa ^ difb) < 0 ) {
      C->note.cur = C->note.aim;
      C->note.stp = 0;
    }
  }

  if (C->wait) --C->wait;
  while (!C->wait) {
    struct loop_s * l;
    const zz_fast_t * const fast = (const zz_fast_t *) seq++;

    switch (fast->cmd) {

    case ZZ_FAST_END:                   /* End-Voice */
      seq = C->seq;
      P->core.loop |= C->msk;
      C->loop_sp = C->loops;
      break;

    case ZZ_FAST_VOICE:
      C->ins = fast->voice.ins;
      break;

    case ZZ_FAST_PLAY:                  /* Play-Note */
      C->trig     = TRIG_NOTE;
      C->note.ins = C->ins;
      C->note.cur = C->note.aim = fast->play.note;
      C->note.stp = 0;
      C->wait     = fast->play.len;
      break;

    case ZZ_FAST_SLIDE:                 /* Slide-to-note */
      if (!C->note.cur) {
        C->trig     = TRIG_NOTE;
        C->note.ins = C->ins;
        C->note.cur = fast->slide.goal;
      }
      C->wait     = fast->slide.len;
      C->note.aim = fast->slide.goal;
      C->note.stp = fast->slide.step;
      break;

    case ZZ_FAST_REST:                  /* Rest */
      C->trig     = TRIG_STOP;
      C->wait     = fast->rest.len;
      C->note.stp = 0;
      C->note.cur = 0;
      break;

    case ZZ_FAST_LABEL:                 /* Set-Loop-Point */
      l = C->loop_sp++;
      l->off = seq2off(C,seq);
      l->cnt = 0;
      break;

    case ZZ_FAST_LOOP:                  /* Loop-To-Point */
      l = C->loop_sp-1;
      if (l < C->loops) {
        C->loop_sp = (l = C->loops) + 1;
        l->cnt = 0;
        l->off = 0;
      }

      if ( ( l->cnt = l->cnt ? l->cnt-1 : fast->loop.cnt ) )
        seq = off2seq(C,l->off);
      else {
        --C->loop_sp;
        zz_assert(C->loop_sp >= C->loops);
      }
      break;

    default:
      zz_assert( ! "invalid command" );
      return E_PLA;
    } /* switch */
  } /* while !wait */
  C->cur = seq;

  if ( 0x0F0 & P->core.mute & C->msk )
    C->trig = TRIG_STOP;

  return E_OK;
}
