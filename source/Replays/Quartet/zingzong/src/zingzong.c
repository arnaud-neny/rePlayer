/**
 * @file   zingzong.c
 * @date   2017-06-06
 * @author Benjamin Gerard
 * @brief  a simple Microdeal quartet music player.
 *
 * ----------------------------------------------------------------------
 *
 * MIT License
 *
 * Copyright (c) 2017-2018 Benjamin Gerard AKA Ben/OVR.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

static const char copyright[] = \
  "Copyright (c) 2017-2018 Benjamin Gerard AKA Ben/OVR";
static const char license[] = \
  "Licensed under MIT license";
static const char bugreport[] = \
  "Report bugs to <https://github.com/benjihan/zingzong/issues>";

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# define _DEFAULT_SOURCE
# define _GNU_SOURCE			/* for GNU basename() */
#endif

#define ZZ_DBG_PREFIX "(cli) "
#include "zingzong.h"
#include "zz_def.h"

/* ----------------------------------------------------------------------
 * Includes
 * ---------------------------------------------------------------------- */

/* stdc */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>


#ifdef WIN32
#ifdef __MINGW32__
# include <libgen.h>	 /* no GNU version of basename() with mingw */
#endif
#include <io.h>
#include <fcntl.h>
#endif

typedef uint_fast32_t uint_t;

/* ----------------------------------------------------------------------
 * Globals
 * ---------------------------------------------------------------------- */

ZZ_EXTERN_C
zz_vfs_dri_t zz_file_vfs(void);		/* vfs_file.c */

#ifndef NO_ICE
ZZ_EXTERN_C
zz_vfs_dri_t zz_ice_vfs(void);	       /* vfs_ice.c */
#endif

static char me[] = "zingzong";

enum {
  /* First (0) is default */
#ifndef NO_AO
  OUT_IS_LIVE, OUT_IS_WAVE,
#endif
  OUT_IS_STDOUT, OUT_IS_NULL,
};

/* Options */

static int opt_splrate = SPR_DEF, opt_tickrate, opt_blend = BLEND_DEF;
static int opt_mixerid = ZZ_MIXER_DEF;
static int8_t opt_ignore, opt_mute, opt_help, opt_outtype, opt_cmap;
static char * opt_length, * opt_output;

/* ----------------------------------------------------------------------
 * Message and logging
 * ----------------------------------------------------------------------
 */

static int8_t newline = 1;		/* newline tracker */
ZZ_EXTERN_C int8_t can_use_std;		   /* out_raw.c */

/* Very lazy and very wrong way to handle that. It won't work as soon
 * as stdout ans stderr are not the same file (or tty).
 */
static void set_newline(const char * fmt)
{
  if (*fmt)
    newline = fmt[strlen(fmt)-1] == '\n';
}

/* Determine log FILE from log level. */
static FILE * log_file(int log)
{
  int8_t fd = 1 + ( log <= ZZ_LOG_WRN ); /* preferred channel */
  if ( ! (fd & can_use_std) ) fd ^= 3;	 /* alternate channel */
  return  (fd & can_use_std)
    ? fd == 1 ? stdout : stderr
    : 0					/* should not happen anyway */
    ;
}

static void log_newline(int log)
{
  if (!newline) {
    FILE * out = log_file(log);
    if (out) putc('\n',out);
    newline = 1;
  }
}

static int errcnt;

static void mylog(zz_u8_t log, void * user, const char * fmt, va_list list)
{
  FILE * out = log_file(log);

  errcnt += log == ZZ_LOG_ERR;
  if (out) {
    if (log <= ZZ_LOG_WRN) {
      fprintf(out, "\n%s: "+newline, me);
      newline = 0;
    }
    vfprintf(out, fmt, list);
    set_newline(fmt);
    fflush(out);
  }
}

/* ----------------------------------------------------------------------
 * Usage and version
 * ----------------------------------------------------------------------
 */

/**
 * Print usage message.
 */
static void print_usage(int level)
{
  int i;
  const char * name="?", * desc;
  zz_mixer_info(ZZ_MIXER_DEF,&name,&desc);

  printf (
    "Usage: zingzong [OPTIONS] <song.4v> [<inst.set>]" "\n"
    "       zingzong [OPTIONS] <music.4q>"  "\n"
    "\n"
    "  A Microdeal quartet music file player\n"
    "\n"
#ifndef NDEBUG
    "  -------> /!\\ DEBUG BUILD /!\\ <-------\n"
    "\n"
#endif

    "OPTIONS:\n"
    " -h --help --usage  Print this message and exit.\n"
    " -V --version       Print version and copyright and exit.\n"
    " -t --tick=HZ       Set player tick rate (default is 200hz).\n"
    " -r --rate=[R,]HZ   Set re-sampling method and rate (%s,%uK).\n",
    name, SPR_DEF/1000u);

  if (!level)
    puts("                    Try `-hh' to print the list of [R]esampler.");
  else
    for (i=0; i == zz_mixer_info(i,&name,&desc); ++i) {
      printf("%6s `%s' %s %s.\n",
	     i?"":" R :=", name,"............."+strlen(name),desc);
    }

  puts(
    " -l --length=TIME   Set play time.\n"
    " -b --blend=[X,]Y   Set channel mapping and blending (see below).\n"
    " -m --mute=CHANS    Mute selected channels (bit-field or string).\n"
    " -i --ignore=CHANS  Ignore selected channels (bit-field or string).\n"
    " -o --output=URI    Set output file name (-w or -c).\n"
    " -c --stdout        Output raw PCM to stdout or file (native 16-bit).\n"
    " -n --null          Output to the void.\n"
#ifndef NO_AO
    " -w --wav           Generated a .wav file.\n"
#endif
    );

  puts(
    !level ?
    "Try `-hh' for more details on OUTPUT/TIME/BLEND/CHANS.\n" :

    "OUTPUT:\n"
    " Options `-n/--null',`-c/--stdout' and `-w/--wav' are used to set the\n"
    " output type. The last one is used. Without it the default output type\n"
    " is used which should be playing sound via the default or configured\n"
    " libao driver.\n"
    "\n"
    " The `-o/--output' option specify the output depending on the output\n"
    " type.\n"
    "\n"
    " `-n/--null'    output is ignored\n"
    " `-c/--stdout'  output to the specified file instead of `stdout'.\n"
    " `-w/--wav'     unless set output is a file based on song filename.\n"

#ifdef NO_AO
    "\n"
    "IMPORTANT:\n"
    " This version of zingzong has been built without libao support.\n"
    " Therefore it can not produce audio output nor RIFF .wav file.\n"
#endif

    "\n"
    "TIME:\n"
    " . 0 or `inf' represents an infinite duration\n"
    " . comma `,' dot `.' or double-quote `\"' separates milliseconds\n"
    " . `m' or quote to suffix minutes\n"
    " . `h' to suffix hour\n"
    "\n"
    " If time is not set the player tries to auto-detect the music duration.\n"
    " However a number of musics are going into unnecessary loops which make\n"
    " it harder to properly detect.\n"
    "\n"
    "BLENDING:\n"
    " Blending defines how the 4 channels are mapped to stereo output.\n"
    " The 4 channels are mapped as left pair (lP) and right pair (rP).\n"
    "   X := B  lP=A+B  rP=C+D (default)\n"
    "        C  lP=A+C  rP=B+D\n"
    "        D  lP=A+D  rP=B+C\n"
    " Both channels pairs are blended together according to Y.\n"
    "   Y := 0    L=100%lP       R=100%rP\n"
    "        64   L=75%lP+25%rP  R=25%lP+75%rP\n"
    "        128  L=50%lP+50%rP  R=50%lP+50%rP\n"
    "        192  L=25%lP+75%rP  R=75%lP+25%rP\n"
    "        256  L=100%rP       R=100%lP\n"
    " L := ((256-Y).lP+Y.rP)/256   R := (Y.lP+(256-Y).rP)/256\n"
    " Y defaults to " CPPSTR(BLEND_DEF) ".\n"
    "\n"
    "CHANS:\n"
    " Select channels to be either muted or ignored. It can be either:\n"
    " . an integer representing a mask of selected channels (C-style prefix)\n"
    " . a string containing the letters A to D in any order\n"
    );
  puts(copyright);
  puts(license);
  puts(bugreport);
}

/**
 * Print version and copyright message.
 */
static void print_version(void)
{
#ifndef NDEBUG
  printf("%s (DEBUG BUILD)\n",zz_core_version());
#else
  printf("%s\n",zz_core_version());
#endif
  puts(copyright);
  puts(license);
}

/* ----------------------------------------------------------------------
 * Argument parsing functions
 * ----------------------------------------------------------------------
 */

#ifndef NO_LOG
static const char * timestr(zz_u32_t ms)
{
  static char s[80];
  const int max = sizeof(s)-1;
  int i=0, l=1;

  switch (ms) {
  case 0: return "infinity";
  case ZZ_EOF: return "error";
  }
  if (ms >= 3600000u) {
    i += snprintf(s+i,max-i,"%huh", HU(ms/3600000u));
    ms %= 3600000u;
    l = 2;
  }
  if (i > 0 || ms >= 60000) {
    i += snprintf(s+i,max-i,"%0*hu'",l,HU(ms/60000u));
    ms %= 60000u;
    l = 2;
  }
  if (!i || ms) {
    uint_t sec = ms / 1000u;
    ms %= 1000u;
    if (ms)
      while (ms < 100) ms *= 10u;
    i += snprintf(s+i,max-i,"%0*hu,%03hu\"", l, HU(sec), HU(ms));
  }
  s[i] = 0;
  return s;
}

#endif

#ifndef NO_AO

/* GB: could probably use some portability work. */
static char * baseext(char * s)
{
  int i, l = strlen(s);
  for ( i=l-2; i>=1; --i )
    if (s[i] == '.') { if (l-i<5) l = i; break; }
  dmsg("extension of \"%s\" is \"%s\"\n", s, s+l);
  return s+l;
}

/**
 * Create .wav filename from another filename
 */
static zz_err_t
wav_filename(char ** pwavname, char * sngname)
{
  zz_err_t ecode;
  char *leaf = basename(sngname), *ext = baseext(leaf);
  const int l = ext - leaf;

  ecode = zz_malloc(pwavname, l+8);
  if (ecode == ZZ_OK) {
    char *str  = *pwavname;
    memcpy(str,leaf,l);
    memcpy(str+l,".wav",5);
    dmsg("wav path: \"%s\"\n",str);
  } else {
    emsg("(%d) %s -- %s\n", errno, strerror(errno),sngname);
  }
  return ecode;
}

static zz_err_t
wav_dupname(char ** pwavname, char * outname)
{
  if ( ! ( *pwavname = strdup(outname) ) ) {
    emsg("(%d) %s -- %s\n",errno, strerror(errno),outname);
    return ZZ_ESYS;
  }
  return ZZ_OK;
}

#endif

static uint_t mystrtoul(char **s, const int base)
{
  uint_t v; char * errp;

  errno = 0;
  if (!isdigit((int)**s))
    return -1;
  v = strtoul(*s,&errp,base);
  if (errno)
    return -1;
  *s = errp;
  return v;
}

/**
 * Parse time argument (a bit permissive)
 */
static int time_parse(uint_t * pms, char * time)
{
  int i;
  uint_t ms = 0, w = 1000;
  char *s = time;

  if (!*s) {
    s = "?";				/* trigger an error */
    goto done;
  }

  if (!strcasecmp(time,"inf")) {
    s += 3; goto done;
  }

  for (i=0; *s && i<3; ++i) {
    uint_t v = mystrtoul(&s,10);
    if (v == (uint_t)-1)
      s = "?";
    switch (tolower(*s)) {
    case 0:
      ms += v * w;
      goto done;

    case ',': case '.': case 's': case '"':
      ++s;
      ms += v * 1000u;
      v = mystrtoul(&s,10);
      if (v == (uint_t)-1)
	s = "?";
      else if (v) {
	while (v > 1000) v /= 10u;
	while (v <  100) v *= 10u;
	zz_assert(v >= 0 && v < 1000u);
	ms += v;
      }
      goto done;

    case 'h':
      if (i>0) goto done;
      ms = v * 3600000u;
      w = 60000u;
      ++s;
      break;

    case 'm': case '\'':
      if (i>1) goto done;
      ms += v * 60u * 1000u;
      w = 1000u;
      ++s;
      break;

    default:
      goto done;
    }
  }

done:
  if (*s) {
    emsg("invalid argument -- length=%s\n", time);
    return ZZ_EARG;
  }
  dmsg("parse_time: \"%s\" => %lu-ms\n",
       time, LU(ms));
  *pms	  = ms;
  return ZZ_OK;
}

/**
 * Parse integer argument with (optional) range check.
 * @retcal -1 on error
 */
static int uint_arg(char * arg, const char * name,
		    uint_t min, uint_t max, int base)
{
  uint_t v;
  char * s = arg;

  v = mystrtoul(&s,base);
  if (v == (uint_t)-1) {
    emsg("invalid number -- %s=%s\n", name, arg);
  } else {
    if (*s == 'k') {
      v *= 1000u;
      ++s;
    }
    if (*s) {
      emsg("invalid number -- %s=%s\n", name, arg);
      v = (uint_t) -1;
    } else if  (v < min || (max && v > max)) {
      emsg("out of range {%lu..%lu} -- %s=%s\n",LU(min),LU(max),name,arg);
      v = (uint_t) -1;
    }
  }
  return v;
}

static char * xtrbrk(char * s, const char * tok)
{
  for ( ;*s && !strchr(tok,*s); ++s)
    ;
  return s;
}

/**
 * @retval 0 no match
 * @retval 1 perfect match
 * @retval 2 partial match
 */
static int modecmp(const char * mix, char * arg,  char ** pend)
{
  const char *eng, *qua;
  char *brk, *end;
  int len, elen, qlen, res = 0;

  if (!pend)
    pend = &end;

  /* Split mix into "engine:quality" */
  qua = strchr(eng = mix,':');
  if (qua)
    elen = qua++ - eng;
  else {
    elen = 0;
    qua = mix;
  }
  qlen = strlen(qua);

  brk = xtrbrk(arg,":,");
  len = brk-arg;
  if (*brk == ':') {
    /* [arg:len] = engine */
    if (len > elen || strncasecmp(eng,arg,len))
      return 0;
    res = 2 - (len == elen);   /* perfect if both have same len */
    brk = xtrbrk(arg=brk+1,",");
    len = brk-arg;
  }
  /* [arg:len] = quality */
  if (len > qlen || strncasecmp(qua,arg,len))
    return 0;
  zz_assert(*brk == 0 || *brk == ',');
  *pend = brk + (*brk == ',');
  res |= 2 - (len == qlen);
  return res;
}


/**
 * @return mixer id
 * @retval 0   not found
 * @retval >0  mixer_id+1
 * @retval <0  -(mixer_id+1)
 */
static int find_mixer(char * arg, char ** pend)
{
  int i,f;

  /* Get re-sampling mode */
  for (i=0, f=-1; ; ++i) {
    const char * name, * desc;
    int res, id = zz_mixer_info(i,&name,&desc);

    if (id == ZZ_MIXER_DEF) {
      i = ZZ_MIXER_DEF;
      break;
    }

    res = modecmp(name, arg, pend);
    dmsg("testing mixer#%i(%i) => (%i) \"%s\" == \"%s\"\n",
	 i, id, res, name, arg);

    if (res == 1) {
      /* prefect match */
      f = i; i = ZZ_MIXER_DEF;
      dmsg("perfect match: %i \"%s\" == \"%s\"\n",f,name,arg);
      break;
    } else if (res) {
      /* partial match */
      dmsg("partial match: %i \"%s\" == \"%s\"\n",f,name,arg);
      if (f != -1)
	break;
      f = i;
    }
  }

  if (f < 0)
    f = 0;				/* not found */
  else if (i != ZZ_MIXER_DEF)
    f = -(f+1);				/* ambiguous */
  else
    f = f+1;				/* found */

  return f;
}


/**
 * Parse -r,--rate=[M:Q,]Hz.
 */
static int uint_spr(char * const arg, const char * name,
		    int * prate, int * pmixer)
{
  int rate = SPR_DEF;
  char * end = arg;

  if (isalpha(arg[0])) {
    int f = f = find_mixer(arg, &end);
    dmsg("find mixer in \"%s\" -> %i \"%s\"\n", arg, f, end);
    if (f <= 0) {
      emsg("%s sampling method -- %s=%s\n",
	   !f?"invalid":"ambiguous", name, arg);
    } else {
      dmsg("set mixer id --  %i\n", f-1);
      *pmixer = f-1;
    }
  }

  if (end && *end) {
    rate = uint_arg(end, name, SPR_MIN, SPR_MAX, 10);
    dmsg("set sampling rate -- %i\n", rate);
  }
  if (rate != -1)
    *prate = rate;

  zz_assert( *pmixer >= 0 );
  zz_assert( *prate  >= SPR_MIN );
  zz_assert( *prate  <= SPR_MAX );

  return rate;
}

/**
 * Parse -m/--mute -i/--ignore option argument. Either a string :=
 * [A-D]\+ or an integer {0..15}
 */
static int uint_mute(char * const arg, char * name)
{
  int c = tolower(*arg), mute;
  if (c >= 'a' && c <= 'd') {
    char *s = arg;
    mute = 0;
    do {
      mute |= 1 << (c-'a');
    } while (c = tolower(*++s), (c >= 'a' && c <= 'd') );
    if (c) {
      emsg("invalid channels -- %s=%s\n",name,arg);
      mute = -1;
    }
  } else {
    mute = uint_arg(arg,name,0,15,0);
  }
  return mute;
}

/**
 * Parse -b,--blend=[X,]Y.
 */
static int uint_blend(char * const arg, const char * name, int8_t * map)
{
  char * s = arg;
  int blend = opt_blend, c = tolower(*s);

  if (c >= 'b' && c <= 'd') {
    *map = c - 'b';
    c = *++s;
    if (c == ',')
      ++s;
    else if (c == 0)
      s = 0;
  }
  if (s)
    blend = uint_arg(s,name,0,256,0);

  return blend;
}

static zz_err_t too_few_arguments(void)
{
  emsg("too few arguments. Try --help.\n");
  return ZZ_EARG;
}

static zz_err_t too_many_arguments(void)
{
  emsg("too many arguments. Try --help.\n");
  return ZZ_EARG;
}

/* ----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------
 */

#define RETURN(V) do { ecode = V; goto error_exit; } while(0)

#ifndef NO_AO
# define WAVOPT "w"  /* libao adds support for wav file generation */
#else
# define WAVOPT
#endif

int main(int argc, char *argv[])
{
  static char sopts[] = "hV" WAVOPT "cno:" "r:t:l:m:i:b:";
  static struct option lopts[] = {
    { "help",	 0, 0, 'h' },
    { "usage",	 0, 0, 'h' },
    { "version", 0, 0, 'V' },
    /**/
#ifndef NO_AO
    { "wav",	 0, 0, 'w' },
#endif
    { "output",	 1, 0, 'o' },
    { "stdout",	 0, 0, 'c' },
    { "null",	 0, 0, 'n' },
    { "tick=",	 1, 0, 't' },
    { "rate=",	 1, 0, 'r' },
    { "length=", 1, 0, 'l' },
    { "mute=",	 1, 0, 'm' },
    { "ignore=", 1, 0, 'i' },
    { "blend=",	 1, 0, 'b' },
    { 0 }
  };
  int c, ecode=ZZ_ERR, ecode2;
  char * wavuri = 0;
  char * songuri = 0, * vseturi = 0;
  zz_play_t P = 0;
  zz_u32_t max_ms;
  zz_u8_t format;
  zz_out_t * out = 0;

  argv[0] = me;

  /* Install logger */
  zz_log_fun(mylog,0);
  zz_log_bit(0, (1<<ZZ_LOG_DBG)-1);

  opterr = 1;
  while ((c = getopt_long (argc, argv, sopts, lopts, 0)) != -1) {
    switch (c) {
    case 'h': opt_help++; break;
    case 'V': print_version(); return ZZ_OK;
#ifndef NO_AO
    case 'w': opt_outtype = OUT_IS_WAVE; break;
#endif
    case 'o': opt_output = optarg; break;
    case 'n': opt_outtype = OUT_IS_NULL; break;
    case 'c': opt_outtype = OUT_IS_STDOUT; break;
    case 'l': opt_length = optarg; break;
    case 'r':
      if (-1 == uint_spr(optarg, "rate", &opt_splrate, &opt_mixerid))
	RETURN (ZZ_EARG);
      break;
    case 't':
      if (-1 == (opt_tickrate = uint_arg(optarg,"tick",RATE_MIN,RATE_MAX,0)))
	RETURN (ZZ_EARG);
      break;
    case 'm':
      if (-1 == (opt_mute = uint_mute(optarg,"mute")))
	RETURN (ZZ_EARG);
      break;
    case 'i':
      if (-1 == (opt_ignore = uint_mute(optarg,"ignore")))
	RETURN (ZZ_EARG);
      break;
    case 'b':
      if (-1 == (opt_blend = uint_blend(optarg,"blend", &opt_cmap)))
	RETURN (ZZ_EARG);
      break;
    case 0: break;
    case '?':
      if (!opterr) {
	if (optopt) {
	  if (isgraph(optopt))
	    emsg("unknown option -- `%c'\n",optopt);
	  else
	    emsg("unknown option -- `\\x%02X'\n",optopt);
	} else if (optind-1 > 0 && optind-1 < argc) {
	  emsg("unknown option -- `%s'\n", argv[optind-1]+2);
	}
      }
      RETURN (ZZ_EARG);
    default:
      emsg("unexpected option -- `%c' (%d)\n",
	   isgraph(c)?c:'.', c);
      zz_assert(!"should not happen");
      RETURN (ZZ_666);
    }
  }

  if (opt_help) {
    print_usage(opt_help > 1);
    return ZZ_OK;
  }

  if (opt_length) {
    ecode = time_parse(&max_ms, opt_length);
    if (ecode)
      goto error_exit;
  } else {
    max_ms = ZZ_EOF;
  }

  if (optind >= argc)
    RETURN (too_few_arguments());

  songuri = argv[optind++];
  if (optind < argc)
    vseturi = argv[optind++];

  if (1) {
    const char * name = "?", * desc;
    opt_mixerid = zz_mixer_info(opt_mixerid,&name,&desc);
    dmsg("requested mixer -- %d:%s (%s)\n", opt_mixerid, name, desc);
    zz_assert( opt_mixerid >= 0 );
  }

  ecode = zz_vfs_add(zz_file_vfs());
  if (ecode)
    goto error_exit;

#ifndef NO_ICE
  ecode = zz_vfs_add(zz_ice_vfs());
  if (ecode)
    goto error_exit;
#endif

  ecode = zz_new(&P);
  if (ecode)
    goto error_exit;
  zz_assert( P );

  ecode = zz_load(P, songuri, vseturi, &format);
  if (ecode)
    goto error_exit;
  optind -= vseturi && format >= ZZ_FORMAT_BUNDLE;
  if (optind < argc)
    RETURN(too_many_arguments());	/* or we could just warn */

  /* ----------------------------------------
   *  Output
   * ---------------------------------------- */

  switch (opt_outtype) {

#ifndef NO_AO
  case OUT_IS_WAVE:
    ecode = !opt_output
      ? wav_filename(&wavuri, songuri)
      : wav_dupname(&wavuri,opt_output)
      ;
    if (ecode)
      goto error_exit;
  case OUT_IS_LIVE:
    out = out_ao_open(opt_splrate, wavuri);
    break;
#endif

  case OUT_IS_STDOUT:
    out = out_raw_open(opt_splrate, opt_output ? opt_output : "stdout:");
    break;

  default:
    zz_assert(!"unexpected output type");

  case OUT_IS_NULL:
    out = out_raw_open(opt_splrate,"null:");
    break;
  }

  if (!out)
    RETURN (ZZ_EOUT);

  zz_core_blend(0, opt_cmap, opt_blend);

  ecode = zz_init(P, opt_tickrate, max_ms);
  if (ecode)
    goto error_exit;
  ecode = zz_setup(P, opt_mixerid, out->hz);
  if (ecode)
    goto error_exit;
  zz_core_mute((void*)P, 0xFF, (opt_mute<<4)|opt_ignore);

#ifndef NO_AO
  if (wavuri)
    imsg("wave: \"%s\"\n", wavuri);
#endif

  if (!ecode) {
    zz_info_t info;
    uint_t sec = (uint_t) -1;
    ecode = zz_info(P, &info);
    if (ecode)
      goto error_exit;

    dmsg("info: rate:%hu spr:%lu ms:%lu\n",
	 HU(info.len.rate), LU(info.mix.spr), LU(info.len.ms));

    dmsg("Output via %s to \"%s\"\n", out->name, out->uri);
    imsg("Zing that zong\n"
	 " with the \"%s\" mixer at %luhz\n"
	 " for %s @%huhz\n"
	 " via \"%s\" at %luhz\n"
	 " blending L to %i%% of channels A+%c\n"
	 "vset: \"%s\" (%hukHz)\n"
	 "song: \"%s\" (%hukHz)\n\n"
	 ,
	 info.mix.name, LU(info.mix.spr),
	 max_ms == 0
	 ? "infinity"
	 : timestr( max_ms == ZZ_EOF ? info.len.ms: max_ms),
	 HU(info.len.rate),
	 out->uri, LU(out->hz),
	 ((256-opt_blend)*100) >> 8, 'B'+opt_cmap,
	 basename((char*)info.set.uri), HU(info.set.khz),
	 basename((char*)info.sng.uri), HU(info.sng.khz )
      );

    if (*info.tag.artist)
      imsg("Artist  : %s\n", info.tag.artist);
    if (*info.tag.title)
      imsg("Title   : %s\n", info.tag.title);
    if (*info.tag.album)
      imsg("Album   : %s\n", info.tag.album);
    if (*info.tag.ripper)
      imsg("Ripper  : %s\n", info.tag.ripper);

    do {
      static int32_t pcm[256];
      zz_i16_t n = sizeof(pcm) >> 2;

      n = zz_play(P,pcm,n);
      if (n < 0) {
	ecode = -n;
	break;
      }
      if (!n)
	break;

      n <<= 2;
      if (n != out->write(out,pcm,n))
	ecode = ZZ_EOUT;
      else {
	zz_u32_t pos = zz_position(P) / 1000u;
	if (pos != sec) {
	  sec = pos;
	  imsg("\n |> %02u:%02u\r"+newline,
	       sec / 60u, sec % 60u );
	  newline = 1;
	}
      }
    } while (!ecode);

    if (ecode) {
      emsg("(%hu) prematured end (ms:%lu) -- %s\n",
	   HU(ecode), LU(zz_position(P)), songuri);
    }
  }

error_exit:

  /* clean exit */
  zz_free(&wavuri);
  zz_assert(!wavuri);

  if (out && out->close(out) && !ecode)
    ecode = ZZ_EOUT;

  if (ecode2 = zz_close(P), (ecode2 && !ecode))
    ecode = ecode2;

  zz_del(&P);

  if (ecode && !errcnt) {
    const char *e;
    switch ( ecode ) {
    case ZZ_OK: e = "no"; break;
    case ZZ_ERR: e = "unspecified"; break;
    case ZZ_EARG: e = "argument"; break;
    case ZZ_ESYS: e = "system"; break;
    case ZZ_EINP: e = "input"; break;
    case ZZ_EOUT: e = "output"; break;
    case ZZ_ESNG: e = "song"; break;
    case ZZ_ESET: e = "voiceset"; break;
    case ZZ_EPLA: e = "player"; break;
    case ZZ_EMIX: e = "mixer"; break;
    case ZZ_666: e = "internal"; break;
    default: e = "undef";
    }
    emsg("%s error (%hu)\n", e, HU(ecode));
  }
  log_newline(ZZ_LOG_INF);

#ifndef NDEBUG
  dmsg("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  dmsg("!!! Checking memory allocation on exit !!!\n");
  dmsg("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  if (ZZ_OK == zz_memchk_calls())
    dmsg("-->  Everything looks fine on my side  <--\n");
  else if (ecode == ZZ_OK)
    ecode = ZZ_666;
  dmsg("exit with -- *%d*\n", ecode);
#endif

  return ecode;
}
