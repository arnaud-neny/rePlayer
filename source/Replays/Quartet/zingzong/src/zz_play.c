/**
 * @file   zz_play.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Quartet player.
 */

#define ZZ_DBG_PREFIX "(pla) "
#include "zz_private.h"


/* ---------------------------------------------------------------------- */

zz_err_t zz_tick(play_t * restrict P)
{
  zz_err_t ecode;

  P->ms_pos  = P->ms_end;
  P->ms_end += P->ms_per_tick;
  P->ms_err += P->ms_err_tick;
  if (P->ms_err >= P->rate) {
    P->ms_err -= P->rate;
    ++ P->ms_end;
  }

  ecode = zz_core_tick(&P->core);

  if ( ZZ_OK == ecode ) {
    if ( P->ms_max == ZZ_EOF )
      P->done |= ((15 & P->core.loop) == 15) | ((P->ms_pos > P->ms_len) << 1);
    else if ( P->ms_max != 0 )
      P->done |= (P->ms_pos > P->ms_max) << 2;

    /* PCM this frame/tick */
    P->pcm_cnt  = P->pcm_per_tick;
    P->pcm_err += P->pcm_err_tick;
    if (P->pcm_err >= P->rate) {
      P->pcm_err -= P->rate;
      ++P->pcm_cnt;
    }
  }

  return ecode;
}

/* ---------------------------------------------------------------------- */

i16_t
zz_play(play_t * restrict P, void * restrict pcm, const i16_t n)
{
  i16_t ret = 0;

  zz_assert( P );
  zz_assert( P->core.mixer || !pcm );

  /* Already done ? */
  if (P->done)
    return -P->core.code;

  do {
    i16_t cnt;

    if (!P->pcm_cnt) {
      P->core.code = zz_tick(P);
      if (P->core.code != E_OK) {
        ret = -P->core.code;
        break;
      }
      if (P->done)
        break;
    }

    if (n == 0) {
      /* n == 0 :: request remaining pcm to close the current tick */
      zz_assert( P->pcm_cnt > 0 );
      zz_assert( ! ret );
      ret = P->pcm_cnt;
      break;
    }

    if ( n < 0 ) {
      /* n < 0 :: finish the tick but no more than -n */
      zz_assert( !ret );
      cnt = -n;
    } else {
      /* remain to mix */
      cnt = n-ret;
    }
    if (cnt > P->pcm_cnt)
      cnt = P->pcm_cnt;
    P->pcm_cnt -= cnt;

    if (pcm) {
      i16_t written = P->core.mixer->push(&P->core, pcm, cnt);
      if (written < 0) {
        ret = -(P->core.code = E_MIX);
        break;
      } else if (written == 0) {
        /* GB: Reversed for future use. Currently should not happen.
         *     A mixer only returning 0 causes an infinite loop.
         */
        zz_assert( ! "mixer should not return 0" );
      }

      /* GB: currently mixers should have mix it all. */
      zz_assert( cnt == written );
      cnt = written;

      /* $$$ GB: currently assuming all mixers returns 2x16bit pcm */
      pcm = (int32_t *) pcm + cnt;
    }
    ret += cnt;

    /* Reset triggers for all channels. */
    P->core.chan[0].trig = P->core.chan[1].trig =
      P->core.chan[2].trig = P->core.chan[3].trig = TRIG_NOP;

    zz_assert( ret <= (n<0?-n:n) );
  } while ( ret < n );

  return ret;
}

/* ---------------------------------------------------------------------- */

zz_err_t
zz_init(play_t * P, u16_t rate, u32_t ms)
{
  if (!P)
    return E_ARG;

  P->ms_max = ms;
  if (!rate)
    rate = P->core.song.rate ? P->core.song.rate : RATE_DEF;
  if (rate < RATE_MIN) rate = RATE_MIN;
  if (rate > RATE_MAX) rate = RATE_MAX;
  P->rate = rate;

  P->ms_len = divu32( mulu32(P->core.song.ticks,1000), P->rate );
  dmsg("ms-len: %lu (%lu@%huhz)\n",
       LU(P->ms_len),LU(P->core.song.ticks),HU(P->rate));
  xdivu(1000u, P->rate, &P->ms_per_tick, &P->ms_err_tick);
  dmsg("rate=%huhz ms:%hu(+%hu)\n",
       HU(P->rate), HU(P->ms_per_tick), HU(P->ms_err_tick));
  P->pcm_per_tick = 1;
  P->pcm_err_tick = 0;

  return P->core.code = ZZ_OK;
}

zz_err_t
zz_setup(play_t * P, u8_t mid, u32_t spr)
{
  zz_err_t ecode;

  if (!P)
    return E_ARG;
  if (P->core.code)
    return P->core.code;

  ecode = E_PLA;
  if (!P->rate)
    goto error;

  ecode = E_SET;
  if (!P->core.vset.iref)
    goto error;

  ecode = zz_core_init(&P->core, zz_mixer_get(&mid), spr);
  if (ecode)
    goto error;
  zz_assert( P->core.spr );

  xdivu(P->core.spr, P->rate, &P->pcm_per_tick, &P->pcm_err_tick);
  dmsg("spr=%luhz pcm:%hu(+%hu)\n",
       LU(P->core.spr), HU(P->pcm_per_tick), HU(P->pcm_err_tick));

error:
  P->done = -!!ecode;
  return P->core.code = ecode;
}

/* ---------------------------------------------------------------------- */

zz_u32_t zz_position(const zz_play_t P)
{
  return !P ? ZZ_EOF : P->ms_pos;
}

static void memb_free(struct memb_s * memb)
{
  if (memb && memb->bin)
    bin_free(&memb->bin);
}

static void memb_wipe(struct memb_s * memb, int size)
{
  if (memb) {
    memb_free(memb);
    zz_memclr(memb,size);
  }
}

static void song_wipe(song_t * song)
{
  memb_wipe((struct memb_s *)song, sizeof(*song));
}

static void vset_wipe(vset_t * vset)
{
  memb_wipe((struct memb_s *)vset, sizeof(*vset));
}

static void info_wipe(info_t * info)
{
  memb_wipe((struct memb_s *)info, sizeof(*info));
}

void zz_wipe(zz_play_t P)
{
  song_wipe(&P->core.song);
  vset_wipe(&P->core.vset);
  info_wipe(&P->info);
}

zz_err_t zz_close(zz_play_t P)
{
  zz_err_t ecode = E_ARG;

  if (P) {
    zz_core_kill(&P->core);

    zz_wipe(P);
    zz_strdel(&P->songuri);
    zz_strdel(&P->vseturi);
    zz_strdel(&P->infouri);
    P->st_idx = 0;
    P->pcm_per_tick = 0;
    P->format = ZZ_FORMAT_UNKNOWN;
    P->rate = 0;

    ecode = E_OK;
  }
  return ecode;
}

void zz_del(zz_play_t * pP)
{
  zz_assert( pP );
  if (pP && *pP) {
    zz_close(*pP);
    zz_free(pP);
  }
}

zz_err_t zz_new(zz_play_t * pP)
{
  zz_assert( pP );
  return zz_calloc(pP,sizeof(**pP));
}

static char empty_str[] = "";
#define NEVER_NIL(S) if ( (S) ) {} else (S) = empty_str

const char * zz_formatstr(zz_u8_t fmt)
{
  switch ( fmt )
  {
  case ZZ_FORMAT_UNKNOWN: return "unknown";
  case ZZ_FORMAT_4V:      return "4v";
  case ZZ_FORMAT_4Q:      return "4q";
  case ZZ_FORMAT_QUAR:    return "quar";
  }
  zz_assert( ! "unexpected format" );
  return "?";
}

extern zz_u8_t  zz_chan_map;
extern zz_u16_t zz_chan_lr8;

zz_err_t zz_info(zz_play_t P, zz_info_t * pinfo)
{
  zz_assert(pinfo);
  if (!pinfo)
    return E_ARG;

  if (!P || P->format == ZZ_FORMAT_UNKNOWN) {
    zz_memclr(pinfo,sizeof(*pinfo));
    pinfo->mix.map  = zz_chan_map;
    pinfo->mix.lr8  = zz_chan_lr8;
    pinfo->mix.spr  = SPR_DEF;
    pinfo->len.rate = RATE_DEF;
    pinfo->fmt.str  = zz_formatstr(pinfo->fmt.num);
  } else {
    dmsg("set info from <%s> \"%s\"\n",
         zz_formatstr(P->format),
         ZZSTR_NOTNIL(P->infouri));
    /* format */
    pinfo->fmt.num = P->format;
    pinfo->fmt.str = zz_formatstr(pinfo->fmt.num);

    /* rates */
    pinfo->len.rate = P->rate =
      P->rate ? P->rate : P->core.song.rate;
    pinfo->mix.spr  = P->core.spr;
    pinfo->len.ms   = P->ms_len;

    /* Blending */
    pinfo->mix.map  = P->core.cmap;
    pinfo->mix.lr8  = P->core.cmap;

    /* mixer */
    pinfo->mix.num = P->mixer_id;
    if (P->core.mixer) {
      pinfo->mix.name = P->core.mixer->name;
      pinfo->mix.desc = P->core.mixer->desc;
    } else {
      zz_mixer_info(pinfo->mix.num, &pinfo->mix.name, &pinfo->mix.desc);
    }

    pinfo->sng.uri = ZZSTR_SAFE(P->songuri);
    pinfo->set.uri = ZZSTR_SAFE(P->vseturi);
    pinfo->sng.khz = P->core.song.khz;
    pinfo->set.khz = P->core.vset.khz;

    /* meta-tags */
    pinfo->tag.album  = P->info.album;
    pinfo->tag.title  = P->info.title;
    pinfo->tag.artist = P->info.artist;
    pinfo->tag.ripper = P->info.ripper;
  }

  /* Ensure no strings are nil */
  NEVER_NIL(pinfo->fmt.str);
  NEVER_NIL(pinfo->mix.name);
  NEVER_NIL(pinfo->mix.desc);
  NEVER_NIL(pinfo->set.uri);
  NEVER_NIL(pinfo->sng.uri);
  NEVER_NIL(pinfo->tag.album);
  NEVER_NIL(pinfo->tag.title);
  NEVER_NIL(pinfo->tag.artist);
  NEVER_NIL(pinfo->tag.ripper);

  return E_OK;
}

zz_err_t zz_vfs_add(zz_vfs_dri_t dri)
{
  return vfs_register(dri);
}

zz_err_t zz_vfs_del(zz_vfs_dri_t dri)
{
  return vfs_unregister(dri);
}
