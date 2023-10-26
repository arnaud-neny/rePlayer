/**
 * @file   zz_load.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet files loader.
 */

#define ZZ_DBG_PREFIX "(lod) "
#include "zz_private.h"
#include "zingzong.h"

zz_err_t
song_parse(song_t *song, vfs_t vfs, uint8_t *hd, u32_t size)
{
  zz_err_t ecode;

  zz_assert( sizeof(songhd_t) ==  16 );
  zz_assert( sizeof(sequ_t)   ==  12 );

  if ( 0
       || (ecode = song_init_header(song,hd))
       || (ecode = bin_load(&song->bin, vfs, size, 12, SONG_MAX_SIZE))
       || (ecode = song_init(song)) )
    bin_free(&song->bin);
  return ecode;
}

zz_err_t
song_load(song_t *song, const char *uri)
{
  uint8_t hd[16];
  vfs_t vfs = 0;
  zz_err_t ecode;

  if ( 0
       || (ecode = vfs_open_uri(&vfs,uri))
       || (ecode = vfs_read_exact(vfs,hd,16))
       || (ecode = song_parse(song,vfs,hd,0))
    );
  vfs_del(&vfs);
  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet voiceset
 * ----------------------------------------------------------------------
 */

#ifdef NO_LIBC

# ifndef NO_LIBC_BUT
/* GB: Technically we can compile zz_load.c without a libc except for
 *     the vset_guess() function. But usually we probably don't want
 *     to. Just define NO_LIBC_BUT if you really want to.
 */
#  error compiling zz_load.c without LIBC ?

# else /* NO_LIBC_BUT */

static int
vset_guess(zz_play_t const P, const char * songuri)
{
  return -1;
}

# endif /* NO_LIBC_BUT */

#else /* NO_LIBC */

static zz_err_t
try_vset_load(vset_t * vset, const char * uri)
{
  const zz_u8_t old_log = zz_log_bit(1<<ZZ_LOG_ERR,0);
  zz_err_t ecode;
  dmsg("try set -- \"%s\"\n", uri);
  ecode = vset_load(vset, uri);
  zz_log_bit(0, old_log & (1<<ZZ_LOG_ERR)); /* restore ZZ_LOG_ERROR  */
  if (ecode)
    dmsg("unable to load voice set (%hu) -- \"%s\"\n", HU(ecode), uri);
  return ecode;
}

/* This function is supposed to tell us a given URI might be case
 * sensitive. It is used by the voice guess function to generate
 * either 1 or 2 versions for each checked file extension.
 *
 * Right now it's just a hard-coded value based on host system
 * traditions.
 */
static int
uri_case_sensitive(const char * songuri)
{
#if defined WIN32 || defined _WIN32 || defined __SYMBIAN32__ || defined __MINT__
  return 0;
#else
  return 1;
#endif
}

static const char *
fileext(const char * const s)
{
  int i, l = strlen(s);
  for ( i=l-2; i>=1; --i )
    if (s[i] == '.') { l=i; i=0; }
  dmsg("extension of \"%s\" is \"%s\"\n", s, s+l);
  return s+l;
}

static zz_err_t
vset_guess(zz_play_t P, const char * songuri)
{
  const int
    lc_to_uc = 'a' - 'A',
    songlen = strlen(songuri),
    case_sensitive = uri_case_sensitive(songuri);
  const char *nud, *dot, *suf_ext;
  int inud, idot;
  int i, c, tr;
  int idx, next_idx, method, next_method;

  enum {
    e_type_ok = 1,		      /* have extension  */
    e_type_4v = 2,		      /* .4v */
    e_type_qt = 4,		      /* .qts or .qta */
    e_type_lc = 8,		      /* has lowercase char */
    e_type_uc = 16,		      /* has uppercase char */
  } e_type = 0;

  /* $$$ IMPROVE ME.
   *
   * GB: Tilde (~) is being used by Windows short names (8.3). However
   * most short names probably won't work anyway.
   */
  static const char num_sep[] = " -_~.:,#"; /* prefix song num */
#if 0
  static const char ext_brk[] = ".:/\\"; /* breaks extension lookup */
  static const char pre_sep[] = "/\\.:-_([{";  /* prefix "4v" */
  static const char suf_sep[] = "/\\.:-_)]}";  /* suffix "4v" */
#endif
  if (!P || !songuri || !*songuri)
    return E_ARG;

  dmsg("URI is%s case sensitive on this system\n",
       case_sensitive ? "" : " NOT");

  zz_assert ( ! P->vseturi );
  if ( P->vseturi ) {
    wmsg("delete previous voiceset URI -- \"%s\"\n", P->vseturi->ptr);
    zz_strdel( &P->vseturi );
    zz_assert ( ! P->vseturi );
  }

  nud = basename((char*)songuri);	/* cast for mingw */
  /* GB: we should be going with gnu_basename(). If anyother version
   *     of basename() is used it has to keep the return value inside
   *     songuri[]. for some reason even gnu_basename() sometime
   *     returns a static empty string. While I understand the reason
   *     when the path is nil, any other case it could return the
   *     address of the trailing zero rather than this static
   *     string. Anyway things being what they are we'll do it this
   *     way on our own.
   */
  if (!*nud) nud = songuri+songlen;
  zz_assert( nud >= songuri && nud <= songuri+songlen );
  if ( ! (nud >= songuri && nud <= songuri+songlen ) )
    return E_666;
  inud = nud-songuri;		     /* index of basename in songuri[] */
  dot  = fileext(nud);
  idot = !*dot ? 0 : dot-songuri;	/* index of '.' in songuri[] */

  dmsg("extension len: %d\n",songlen-idot);
  if (songlen-idot > 4)
    idot = 0;				/* ignore long extension */

  if (idot) {
    e_type = e_type_ok;
    for (i=idot; !!(c = songuri[i]); ++i)
      if (islower(c)) e_type |= e_type_lc;
      else if (isupper(c)) e_type |= e_type_uc;
    if (!strcasecmp(dot+1,"4v"))
      e_type |= e_type_4v;
    else if (!strcasecmp(dot+1,"qts") || !strcasecmp(dot+1,"qta") )
      e_type |= e_type_qt;
    dmsg("extension \"%s\" of type $%X\n",dot,e_type);
  } else {
    dmsg("no extension\n");
  }

  P->vseturi = zz_strnew(songlen+32);
  if (unlikely(!P->vseturi))
    return E_MEM;

  for (idx=0, method=1; method ; method=next_method, idx=next_idx) {
    char * const s = P->vseturi->ptr;

    *s = 0;				      /* no candidat */
    tr = (e_type & e_type_uc) ? lc_to_uc : 0; /* translation */

    /* next method (state-init => run-state) */
    next_method = method;
    if (method & 1) {
      ++next_method;
      idx = 0;
    }
    next_idx = idx+1+!case_sensitive;

    /* dmsg("XXX %hu:%hu %hu:%hu\n", */
    /*      HU(method),HU(idx), HU(next_method), HU(next_idx)); */

    /* GB: Every 2nd idx is the same as its predecessor but using
     *     the other case. The predecessor case depends on the case
     *     of the song file extension. It follows this rule --
     *     lowercase unless there is a single uppercase char in the
     *     original file extension.
     */
    tr ^= lc_to_uc & -(idx & 1);
    dmsg("---------\n");
    dmsg("#%hu:%hu %02hX\n",HU(method), HU(idx), HU(tr));

    switch (method) {

      /* 1&2: change original extension starting with the natural
       *      buddy extension (4v->set qt?->smp). Try removing song
       *      number suffix as well.
       */
    case 1:
      /* No extension ? ignore this method */
      if (!idot) {
	dmsg("skipping method #%hu because no extension\n", HU(method));
	++next_method;
	zz_assert(next_method==3);
	break;
      }

    case 2: {
      if (idx >= 8) { ++next_method; zz_assert(next_method==3); break; }

      /* starts with ".set" for ".4v" songs else uses ".smp" first */
      suf_ext = ( !(e_type & e_type_4v) ^ (idx >= 4) ) ? "smp" : "set";

      /* Copy up to the extension. */
      zz_memcpy(s, songuri, idot+1);

      i = idot;
      if ( ( idx & 2 ) ) {
	/* Try to remove the song number suffix. */
	if (i > inud+1 && isdigit(s[i-1])) {
	  --i;
	  i -= (i > inud+1 && isdigit(s[i-1]));
	  i -= (i > inud+1 && strchr(num_sep,s[i-1]));
	} else {
	  /* No song number suffix. Skip regardless of case
           * sensitivity. */
	  next_idx = idx+2;
	  *s = 0;
	  break;
	}
      }

      /* Copy the new extension with case transformation. */
      for (s[i++] = s[idot]; !!(c = *suf_ext++); ++i)
	s[i] = c - (islower(c) ? tr : 0);
      s[i] = 0;
      zz_assert(i < P->vseturi->max);
    } break;

      /* 3&4: Append extension. */
    case 3:
      if (e_type & (e_type_4v|e_type_qt)) {
	dmsg("skipping method #%hu because have known extension\n", HU(method));
	++next_method;
	zz_assert( next_method == 5 );
	break;
      }
    case 4:
      if (idx >= 4) {
	++next_method;
	zz_assert( next_method == 5 );
	break;
      }
      suf_ext = !(e_type & e_type_4v) ^ (idx >= 2) ? "smp" : "set";
      zz_memcpy(s, songuri, songlen);
      s[songlen] = '.';
      for (i=0; (c=suf_ext[i]); ++i)
	s[songlen+i+1] = c - ( islower(c) ? tr : 0 );
      s[songlen+i+1] = 0;
      break;

      /* 5&6: prefixed extension (Amiga like) */
    case 5:
      if (e_type & (e_type_4v|e_type_qt)) {
	dmsg("skipping method #%hu because have known extension\n", HU(method));
	++next_method;
	zz_assert( next_method == 7 );
	break;
      }
    case 6:
      if (idx >= 6) { ++next_method; zz_assert( next_method == 7 ); break; }
      switch (idx >> 1) {
      case 0: c = strncasecmp("4v",  nud, i=2); suf_ext = "set"; break;
      case 1: c = strncasecmp("qts", nud, i=3); suf_ext = "smp"; break;
      case 2: c = strncasecmp("qta", nud, i=3); suf_ext = "smp"; break;
      default: c = 1;
      }
      if (c) break;
      zz_memcpy(s, songuri, inud);
      zz_memcpy(s+inud+3, songuri+inud+i, songlen-inud-i+1);
      for (i=0; (c=suf_ext[i]); ++i)
	s[inud+i] = c - ( islower(c) ? tr : 0 );
      break;

    default:
      zz_assert( idx == 0 );
      next_method = -2;

    case -2: {
      /* Last resort filenames. */
      static const char * vnames[] = { "voice.set", "SMP.set" };
      if (idx >= 4) { next_method=0; break; }
      suf_ext = vnames[(idx>>1)&1];
      zz_memcpy(s,songuri,inud);
      for (i=0; (c = suf_ext[i]); ++i)
	s[inud+i] = c - (islower(c) ? tr : 0);
      s[inud+i] = 0;
    } break;

    }

    if (*s) {
      dmsg("method: #%hi:%hu, next: #%hi:%hu tr:%02hx\n",
	   HI(method), HU(idx), HI(next_method), HU(next_idx), HU(tr));
      if (E_OK == try_vset_load(&P->core.vset, s))
	return E_OK;
    }
  }

  emsg("unable to find a voice set for -- \"%s\"\n", songuri);

  if (P->core.vset.bin)
    bin_free(&P->core.vset.bin);
  zz_assert( ! P->core.vset.bin );

  if (P->vseturi)
    zz_strdel(&P->vseturi);
  zz_assert( ! P->vseturi );

  return E_SET;
}

#endif

zz_err_t (*zz_guess_vset)(zz_play_t const, const char *) = vset_guess;

zz_err_t
vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, u32_t size)
{
  zz_err_t ecode;
  if (0
      /* Parse header and instruments */
      || (ecode = vset_init_header(vset, hd))
      || (ecode = bin_load(&vset->bin, vfs, size, VSET_EXTRA, VSET_MAX_SIZE))
      || (ecode = vset_init(vset)))
    bin_free(&vset->bin);
  return ecode;
}

zz_err_t
vset_load(vset_t *vset, const char *uri)
{
  uint8_t hd[222];
  vfs_t vfs = 0;
  zz_err_t ecode;

  if ( 0
       || (ecode = vfs_open_uri(&vfs,uri))
       || (ecode = vfs_read_exact(vfs,hd,222))
       || (ecode = vset_parse(vset,vfs,hd,0)))
    ;
  vfs_del(&vfs);
  return ecode;
}

static char * trimstr(char * s)
{
  char *beg, *end = 0;
  while (isspace(*s)) ++s;
  for (beg = s; *s; ++s)
    if (!isspace(*s)) end = s;
  if (end) end[1] = 0;
  return beg;
}

static char *
mystrsep(char ** const ps, const char c)
{
  char *beg, *end;
  zz_assert( ps );
  zz_assert( *ps );
  for (end = beg = *ps ; ; ++end) {
    if ( ! *end ) {
      *ps = 0;
      break;
    }
    if ( *end == c ) {
      *end = 0;
      *ps = end+1;
      break;
    }

  }
  return beg;
}

/**
 * Load .4q file (after header).
 */
zz_err_t
q4_load(vfs_t vfs, q4_t *q4)
{
  zz_err_t ecode = E_SNG;
  uint8_t hd[222];

  zz_assert( vfs_tell(vfs) == 20 );

  ecode = E_SNG;
  if (q4->songsz < 16 + 12*4) {
    dmsg("invalid .4q song size (%lu) -- %s", LU(q4->songsz), vfs_uri(vfs));
    goto error;
  }

  ecode = E_SET;
  if (q4->vset && q4->vsetsz < 222) {
    dmsg("invalid .4q set size (%lu) -- %s", LU(q4->vsetsz), vfs_uri(vfs));
    goto error;
  }

  /* Read song */
  if (q4->song) {
    ecode = vfs_read_exact(vfs, hd, 16);
    if (E_OK == ecode)
      ecode = song_parse(q4->song, vfs, hd, q4->songsz-16);
    if (E_OK == ecode && q4->vset)
      q4->vset->iref = q4->song->iref;
  } else
    ecode = vfs_seek(vfs, q4->songsz, ZZ_SEEK_CUR);

  if (E_OK != ecode)
    goto error;

  /* Voice set */
  if (q4->vset) {
    ecode = vfs_read_exact(vfs,hd,222);
    if (E_OK == ecode)
      ecode = vset_parse(q4->vset, vfs, hd, q4->vsetsz-222);
  } else
    ecode = vfs_seek(vfs, q4->vsetsz, ZZ_SEEK_CUR);

  if (E_OK != ecode)
    goto error;

  /* Info (ignoring errors) */
  if (q4->info && q4->infosz > 0) {
    if (E_OK ==
	bin_load(&q4->info->bin,vfs,q4->infosz, q4->infosz+2,INFO_MAX_SIZE)
	&& q4->info->bin->len > 1 && *q4->info->bin->ptr)
    {
      /* Atari to UTF-8 conversion */
      static const uint16_t atari_to_unicode[128] = {
	0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
	0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
	0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
	0x00ff,0x00d6,0x00dc,0x00a2,0x00a3,0x00a5,0x00df,0x0192,
	0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
	0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
	0x00e3,0x00f5,0x00d8,0x00f8,0x0153,0x0152,0x00c0,0x00c3,
	0x00d5,0x00a8,0x00b4,0x2020,0x00b6,0x00a9,0x00ae,0x2122,
	0x0133,0x0132,0x05d0,0x05d1,0x05d2,0x05d3,0x05d4,0x05d5,
	0x05d6,0x05d7,0x05d8,0x05d9,0x05db,0x05dc,0x05de,0x05e0,
	0x05e1,0x05e2,0x05e4,0x05e6,0x05e7,0x05e8,0x05e9,0x05ea,
	0x05df,0x05da,0x05dd,0x05e3,0x05e5,0x00a7,0x2227,0x221e,
	0x03b1,0x03b2,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
	0x03a6,0x0398,0x03a9,0x03b4,0x222e,0x03c6,0x2208,0x2229,
	0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
	0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x00b3,0x00af
      };
      int i,j,len = q4->info->bin->len;
      char * comment, * s = (char*) q4->info->bin->ptr;

      for (i=0; i<len && s[i]; ++i)
	;

      j = q4->info->bin->max;
      for (s[--j] = 0; i>0 && j>=i; ) {
	const uint8_t c = s[--i];
	uint16_t u = c < 128 ? c : atari_to_unicode[c&127];

	if (u < 0x80) {
	  if (u != '\r')
	    s[--j] = u;
	} else if (u < 0x800 && j >= 2 ) {
	  s[--j] = 0x80 | (u & 63);
	  s[--j] = 0xC0 | (u >> 6);
	} else if ( j >= 3) {
	  s[--j] = 0x80 | (u & 63);
	  s[--j] = 0x80 | ((u >> 6) & 63);
	  s[--j] = 0xE0 | (u >> 12);
	}
	zz_assert(j >= i);
      }
      comment = (char *)q4->info->bin->ptr+j;

      /* Parse comment for title, artist and ripper. */

      s = comment;

      dmsg("== COMMENT: ===\n");

      while (s != NULL) {
	char * line = mystrsep(&s, '\n');

	if (!line) break;

	dmsg("[%s]\n",line);

	/* assume 1st line is always the title. */
	if (!q4->info->title) {
	  q4->info->title = trimstr(line);
	  continue;
	}

	/* continuing previous line ? */
	if (q4->info->artist && !*q4->info->artist) {
	  q4->info->artist = trimstr(line);
	  if (!*q4->info->artist) q4->info->artist = 0;
	  continue;
	}
	if (q4->info->album && !*q4->info->album) {
	  q4->info->album = trimstr(line);
	  if (!*q4->info->album) q4->info->album = 0;
	  continue;
	}
	if (q4->info->ripper && !*q4->info->ripper) {
	  q4->info->ripper = trimstr(line);
	  if (!*q4->info->ripper) q4->info->ripper = 0;
	  continue;
	}

	while ( isspace(*line) ) ++line;
	if (!*line) continue;

	if (!q4->info->artist) {
	  if (!strncasecmp(line,"Artist:",7)) {
	    q4->info->artist = trimstr(line + 7);
	    continue;
	  }
	  if (!strncasecmp(line,"Composed by",11) ||
	      !strncasecmp(line,"Arranged by",11) ||
	      !strncasecmp(line,"Written by",10)) {
	    line += 10 + (tolower(line[6]) == 'e');
	    q4->info->artist = trimstr(line + (*line == ':'));
	    continue;
	  }
	}

	if (!q4->info->ripper) {
	  if (!strncasecmp(line,"Ripper:",7)) {
	    q4->info->ripper = trimstr(line + 7);
	    continue;
	  }
	  if (!strncasecmp(line,"Hacked by",9) ||
	      !strncasecmp(line,"Ripped by",9) ||
	      !strncasecmp(line,"Rippd by",8) ) {
	    line += 8 + (tolower(line[4]) == 'e');
	    q4->info->ripper = trimstr(line + (*line == ':'));
	    continue;
	  }
	}

	if (!q4->info->album) {
	  if (!strncasecmp(line,"Album:",7)) {
	    q4->info->album = trimstr(line + 7);
	    continue;
	  }
	  if (!strncasecmp(line,"Coming from",11)) {
	    line += 11;
	    q4->info->album = trimstr(line + (*line == ':'));
	    continue;
	  }
	}

      }

      dmsg(" --------------\n");
      dmsg("-- 4Q  :\n");
      dmsg("album  : <%s>\n", q4->info->album  ? q4->info->album  : "");
      dmsg("title  : <%s>\n", q4->info->title  ? q4->info->title  : "");
      dmsg("artist : <%s>\n", q4->info->artist ? q4->info->artist : "");
      dmsg("ripper : <%s>\n", q4->info->ripper ? q4->info->ripper : "");
      dmsg("--\n");
    }
  }

  ecode = E_OK;
error:
  return ecode;
}

/**
 * @param  vseturi  0:guess "":song only
 */
zz_err_t
zz_load(play_t * P, const char * songuri, const char * vseturi, zz_u8_t * pfmt)
{
  uint8_t hd[20];
  vfs_t inp = 0;
  zz_err_t ecode;
  zz_u8_t format = ZZ_FORMAT_UNKNOWN;
  const u8_t without_vset = vseturi && !*vseturi;

  dmsg("load: song:<%s>\n", songuri?songuri:"(nil)");
  dmsg("%s: vset:<%s>\n", without_vset?"skip":"load", vseturi?vseturi:"(auto)");

  if (!P || !songuri || !*songuri)
    return E_ARG;

  do {

    ecode = vfs_open_uri(&inp, songuri);
    if (ecode == E_OK)
      /* Read song header */
      ecode = vfs_read_exact(inp, hd, 16);
    if (ecode != E_OK)
      break;

    /* Check for .4q "QUARTET" magic id */
    if (!zz_memcmp(hd,"QUARTET",8)) {
      q4_t q4;

      ecode = vfs_read_exact(inp, hd+16, 4);
      if (E_OK != ecode)
	break;

      format = ZZ_FORMAT_4Q;
      ecode = E_SYS;
      zz_assert ( ! P->songuri );
      zz_assert ( ! P->vseturi );
      zz_assert ( ! P->infouri );

      ecode = E_MEM;
      P->songuri = zz_strset(P->songuri, songuri);
      if (!P->songuri)
	break;

      P->vseturi = zz_strdup(P->songuri);
      P->infouri = zz_strdup(P->songuri);

      q4.info = &P->info; q4.infosz = U32(hd+16);
      q4.song = &P->core.song; q4.songsz = U32(hd+8);
      q4.vset = without_vset ? 0 : &P->core.vset;
      q4.vsetsz = U32(hd+12);

      dmsg("QUARTET header [sng:%lu set:%lu inf:%lu]\n",
	   LU(q4.songsz), LU(q4.vsetsz), LU(q4.infosz));
      ecode = q4_load(inp,&q4);
      vfs_del(&inp);
      break;
    }

    /* Load song */
    ecode = song_parse(&P->core.song, inp, hd, 0);
    if (unlikely(ecode))
      break;
    format = ZZ_FORMAT_4V;
    vfs_del(&inp);

    ecode = E_MEM;
    P->songuri = zz_strset(P->songuri, songuri);
    if (unlikely(!P->songuri))
      break;
    ecode = E_OK;

    zz_assert(inp == 0);
    if (!vseturi)
      ecode = vset_guess(P, songuri);
    else {
      if (*vseturi) {
	ecode = vset_load(&P->core.vset,vseturi);
	if (ecode)
	  break;
	ecode = E_MEM;
	P->vseturi = zz_strset(P->vseturi,vseturi);
	if (unlikely(!P->vseturi))
	  break;
	ecode = E_OK;
      } else {
	dmsg("skipped voice set as requested\n");
	zz_assert( ecode == E_OK);
      }
    }

    /* Finally */
  } while (0);

  vfs_del(&inp);
  if (ecode) {
    zz_wipe(P);
    format = ZZ_FORMAT_UNKNOWN;
  } else {
    zz_assert( format != ZZ_FORMAT_UNKNOWN );
  }
  P->format = format;
  if (pfmt)
    *pfmt = format;

  return ecode;
}
