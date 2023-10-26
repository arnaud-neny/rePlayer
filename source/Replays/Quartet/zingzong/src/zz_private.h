/**
 * @file   zz_private.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  zingzong private definitions.
 */

#ifdef ZZ_PRIVATE_H
# error Should be included only once
#endif
#define ZZ_PRIVATE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# define _DEFAULT_SOURCE
# define _GNU_SOURCE			/* for GNU basename() */
# define _BSD_SOURCE
#endif

#ifdef ZZ_MINIMAL
# define NO_FLOAT
# define NO_LIBC
# define NO_VFS
# ifdef SC68
#  define NO_LOG
# endif
#endif

#define ZZ_VFS_DRI
#include "zingzong.h"

/* Old error code names have been changed to avoid name conflict
 * likeliness in the public space. They are still in use in zingzong
 * internal code. */
#define E_OK  ZZ_OK
#define E_ERR ZZ_ERR
#define E_666 ZZ_666
#define E_ARG ZZ_EARG
#define E_SYS ZZ_ESYS
#define E_MEM ZZ_ESYS
#define E_INP ZZ_EINP
#define E_OUT ZZ_EOUT
#define E_SET ZZ_ESET
#define E_SNG ZZ_ESNG
#define E_PLA ZZ_EPLA
#define E_MIX ZZ_EMIX

#if !defined DEBUG && !defined NDEBUG
# define NDEBUG 1
#endif

#ifndef NO_LIBC

# include <stdio.h>
# include <ctype.h>
# include <string.h>			 /* memset, basename */

#ifdef __MINGW32__
# include <libgen.h>	 /* no GNU version of basename() with mingw */
#endif

# include <errno.h>
# define ZZ_EIO	   EIO
# define ZZ_EINVAL EINVAL
# define ZZ_ENODEV ENODEV

#else

# define ZZ_EIO	   5
# define ZZ_EINVAL 22
# define ZZ_ENODEV 19

#endif

/* because this is not really part of the API but we need similar
 * functions in the program that use the public API and I am too lazy
 * to duplicate the code. */

#ifndef likely
# if defined(__GNUC__) || defined(__clang__)
#  define likely(x)   __builtin_expect(!!(x),1)
#  define unlikely(x) __builtin_expect((x),0)
# else
#  define likely(x)   (x)
#  define unlikely(x) (x)
# endif
#endif

#ifndef unreachable
# if defined(__GNUC__) || defined(__clang__)
#  define unreachable() __builtin_unreachable()
# else
#  define unreachable() zz_void
# endif
#endif

#ifndef always_inline
# if defined __GNUC__ || defined __clang__
#  define always_inline __attribute__((always_inline))
# elif defined _MSC_VER
#  define always_inline __forceinline
# else
#  define always_inline inline
# endif
#endif

#ifndef never_inline
# if defined(__GNUC__) || defined(__clang__)
#  define never_inline	__attribute__((noinline))
# elif defined _MSC_VER
#  define never_inline __declspec(noinline)
# else
#  define never_inline
# endif
#endif

#ifndef FP
# define FP 15				/* mixer step precision */
#endif

#define VSET_EXTRA    4096	   /* extra space for unrolling */
#define VSET_MAX_SIZE (1<<19)	   /* arbitrary .set max size */
#define SONG_MAX_SIZE 0xFFF0	   /* not so arbitrary .4v max size */
#define INFO_MAX_SIZE 2048	   /* arbitrary .4q info max size */

/* The size of the loop stack in the singsong.prg program is *67*.
 * The maximum depth encountered so far in a Quartet module is *6*.
 */
#ifndef MAX_LOOP
#define MAX_LOOP 15
#endif

/* Encountered lowest and highest notes are respectively:
 *
 * 0x04C1B (~0.3) = -21 semitones
 * 0x50A28 (~5.0) = +30 semitones
 *
 */
#define SEQ_STP_MIN 0x04C1B  /**< Lowest note without re-sampling.  */
#define SEQ_STP_MAX 0x50A28  /**< Highest note without re-sampling. */

enum {
  TRIG_NOP, TRIG_NOTE, TRIG_SLIDE, TRIG_STOP
};

/* ----------------------------------------------------------------------
 *  Types definitions
 * ----------------------------------------------------------------------
 */

/* Don't need for the exact size but at least this size. */
typedef uint_fast64_t  u64_t;
typedef int_fast64_t   i64_t;
typedef zz_u32_t       u32_t;
typedef zz_i32_t       i32_t;
typedef zz_u16_t       u16_t;
typedef zz_i16_t       i16_t;
typedef zz_u8_t	       u8_t;
typedef zz_i8_t	       i8_t;
typedef unsigned int   uint_t;

typedef struct bin_s   bin_t;	  /**< binary data container.     */
typedef struct q4_s    q4_t;	  /**< 4q header.                 */
typedef struct info_s  info_t;	  /**< song info.                 */
typedef struct vset_s  vset_t;	  /**< voice set (.set file).     */
typedef struct inst_s  inst_t;	  /**< instrument.                */
typedef struct song_s  song_t;	  /**< song (.4v file).           */
typedef struct sequ_s  sequ_t;	  /**< sequence definition.       */
typedef struct core_s  core_t;	  /**< core player.               */
typedef struct play_s  play_t;	  /**< high level player.         */
typedef struct chan_s  chan_t;	  /**< one channel.               */
typedef struct note_s  note_t;	  /**< channel step (pitch) info. */
typedef struct mixer_s mixer_t;	  /**< channel mixer.             */
typedef struct songhd songhd_t;	  /**< .4v file header.           */

typedef struct vfs_s * vfs_t;
typedef struct zz_vfs_dri_s vfs_dri_t;

struct songhd {
  uint8_t rate[2], measure[2], tempo[2], timesig[2], reserved[8];
};

struct sequ_s {
  uint8_t cmd[2],len[2],stp[4],par[4];
};

struct str_s {
  /**
   * Pointer to the actual string.
   * @notice  ALWAYS FIRST
   */
  char * ptr;		     /**< buf: dynamic else: static.        */
  u16_t	 ref;		     /**< number of reference.              */
  u16_t	 max;		     /**< 0: const static else buffer size. */
  u16_t	 len;		     /**< 0: ndef else len+1.               */
  /**
   * buffer when dynamic.
   * @notice  ALWAYS LAST
   */
  char	 buf[4];
};

struct bin_s {
  uint8_t *ptr;			     /**< pointer to data (_buf).   */
  u32_t	   max;			     /**< maximum allocated string. */
  u32_t	   len;			     /**< length including.         */
  uint8_t _buf[1];		     /**< buffer (always last).     */
};

/** Prepared instrument (sample). */
struct inst_s {
  u16_t	   len;		     /**< size in bytes.                    */
  u16_t	   lpl;		     /**< loop length in bytes.             */
  u32_t	   end;		     /**< unrolled end.                     */
  uint8_t *pcm;		     /**< sample address.                   */
};

struct memb_s {
  bin_t	 *bin;		     /**< data container.                   */
};

/** Prepared instrument set. */
struct vset_s {
  bin_t *bin;		     /**< voiceset data container.          */
  /* */
  uint8_t khz;		     /**< sampling rate from .set.          */
  uint8_t nbi;		     /**< number of instrument [1..20].     */
  uint8_t nul;		     /**< value of middle point PCM.        */
  uint8_t one;		     /**< value of positive max PCM.        */
  u32_t	 unroll;	     /**< unrolled amount.                  */
  u32_t	 iref;		     /**< mask of instrument referenced.    */
  inst_t inst[20];	     /**< instrument definitions.           */
};

/** Prepared song. */
struct song_s {
  bin_t	 *bin;		     /**< song data container.              */
  /* */
  uint8_t rate;		     /**< tick rate (0: unspecified).       */
  uint8_t khz;		     /**< header sampling rate (kHz).       */
  uint8_t barm;		     /**< header bar measure.               */
  uint8_t tempo;	     /**< header tempo.                     */
  uint8_t sigm;		     /**< header signature numerator.       */
  uint8_t sigd;		     /**< header signature denominator.     */
  u32_t	  iuse;		     /**< mask of instrument really used.   */
  u32_t	  iref;		     /**< mask of instrument referened.     */
  u32_t	  stepmin;	     /**< estimated minimal note been used. */
  u32_t	  stepmax;	     /**< estimated maximal note been used. */
  u32_t	  ticks;	     /**< estimated song length in ticks.   */
  sequ_t *seq[4];	     /**< pointers to channel sequences.    */
  uint8_t istep[20];	     /**< max step per instrument.          */
};

/** Song meta info. */
struct info_s {
  bin_t *bin;		     /**< info data container.              */
  /* */
  char	*album;		     /**< decoded album.                    */
  char	*title;		     /**< decoded title.                    */
  char	*artist;	     /**< decoded artist.                   */
  char	*ripper;	     /**< decoded ripper.                   */
};

/** Played note. */
struct note_s {
  i32_t	  cur;		     /**< current note.                     */
  i32_t	  aim;		     /**< current note slide goal.          */
  i32_t	  stp;		     /**< note slide speed (step).          */
  inst_t *ins;		     /**< Current instrument.               */
};


/** .4q file. */
struct q4_s {
  song_t * song;
  vset_t * vset;
  info_t * info;

  u32_t songsz;
  u32_t vsetsz;
  u32_t infosz;
};

#include "zz_def.h"

/**
 * Played channel.
 */
struct chan_s {
  sequ_t  *seq;			      /**< sequence address.        */
  sequ_t  *cur;			      /**< next sequence.           */
  inst_t  *ins;			      /**< instrument (zz_fast)     */

  uint8_t num;			    /**< channel number [0..3].     */
  uint8_t pam;			    /**< map to [0..3],             */
  uint8_t msk;			    /**< {0x11,0x22,0x44,0x88}.     */
  uint8_t trig;			    /**< see TRIG_* enum.           */
  uint8_t curi;			    /**< current instrument number. */

  u16_t wait;			  /**< number of tick left to wait. */
  note_t note;			  /**< note (and slide) info.       */
  struct loop_s {
    u16_t cnt;				/**< loop count. */
    u16_t off;				/**< loop point. */
  }
  *loop_sp,				/**< loop stack pointer.    */
  loops[MAX_LOOP];			/**< loop stack.   */
};

/**
 *  Player core information.
 */
struct core_s {
  song_t   song;		/**< Music song (.4v). */
  vset_t   vset;		/**< Music instrument (.set). */
  void	  *user;		/**< User data. */
  mixer_t *mixer;		/**< Mixer to use. */
  void	  *data;		/**< Mixer private data. */
  u32_t	   tick;		/**< current tick (0:init 1:first). */
  u32_t	   spr;			/**< Sampling rate (hz). */
  u16_t	   lr8;			/**< L/R channels blending. */
  uint8_t  mute;		/**< #0-3: ignored #4-7: muted. */
  uint8_t  loop;		/**< #0-3:loop #4-7:tick loop. */
  uint8_t  code;		/**< Error code. */
  uint8_t  cmap;		/**< channel mapping (ZZ_MAP_*). */

  chan_t   chan[4];		/**< 4 channels info. */
};

struct play_s {
  /* /!\  must be first /!\ */
  core_t core;
  /* /!\  must be first /!\ */

  volatile u8_t st_idx;
  struct str_s st_strings[4];

  info_t   info;			/**< Music info. */
  str_t songuri;
  str_t vseturi;
  str_t infouri;

  u32_t ms_pos;		 /**< current frame start position (in ms). */
  u32_t ms_end;		 /**< current frame end position (in ms).   */
  u32_t ms_max;		 /**< maximum ms to play.                   */
  u32_t ms_len;		 /**< measured ms length.                   */

  u16_t	   rate;		/**< tick rate (in hz). */
  u16_t pcm_cnt;		 /**< pcm remaining on this tick.   */
  u16_t pcm_err;		 /**< pcm error accumulator.        */
  u16_t pcm_per_tick;		 /**< pcm per tick (integer).       */
  u16_t pcm_err_tick;		 /**< pcm per tick (correction).    */

  u16_t ms_err;			     /**< ms error accumulator.     */
  u16_t ms_per_tick;		     /**< ms per tick (integer).    */
  u16_t ms_err_tick;		     /**< ms per tick (correction). */

  uint8_t done;		   /**< non zero when done.  */
  uint8_t format;	   /**< see ZZ_FORMAT_ enum. */
  uint8_t mixer_id;	   /**< mixer identifier.    */
};

/* ---------------------------------------------------------------------- */

/**
 * Machine specific (byte order and fast mul/div.
 * @{
 */

#ifndef __ORDER_BIG_ENDIAN__
# define __ORDER_BIG_ENDIAN__ 4321
#endif

#ifdef __m68k__

# include "m68k_muldiv.h"
# ifndef __BYTE_ORDER__
#  define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
# endif

ZZ_EXTERN_C
int m68k_memcmp(const void *, const void *, uint32_t);
#define zz_memcmp(A,B,C) m68k_memcmp((A),(B),(C))

ZZ_EXTERN_C
void * m68k_memset(void * restrict, uint32_t, uint32_t);
#define zz_memset(A,B,C) m68k_memset((A),(B),(C))

ZZ_EXTERN_C
void * m68k_memcpy(void * restrict, const void *, uint32_t);
#define zz_memcpy(A,B,C) m68k_memcpy((A),(B),(C))

ZZ_EXTERN_C
void * m68k_memxla(void * restrict, const void *,
		   const uint8_t *, uint32_t);
#define zz_memxla(A,B,C,D) m68k_memxla((A),(B),(C),(D))

#else /* __m68k__ */

static inline u32_t always_inline c_mulu(u16_t a, u16_t b)
{ return a * b; }
static inline uint32_t always_inline c_mulu32(uint32_t a, uint16_t b)
{ return a * b; }
static inline u32_t always_inline c_divu(u32_t n, u16_t d)
{ return n / d; }
static inline u32_t always_inline c_modu(u32_t n, u16_t d)
{ return n % d; }
static inline void always_inline c_xdivu(u32_t n, u16_t d, u16_t *q, u16_t *r)
{ *q = n / d; *r = n % d; }
static inline u32_t always_inline c_divu32(u32_t n, u16_t d)
{ return n / d; }
# define mulu(a,b)	c_mulu((a),(b))
# define divu(a,b)	c_divu((a),(b))
# define modu(a,b)	c_modu((a),(b))
# define xdivu(a,b,q,r) c_xdivu((a),(b),(q),(r))
# define mulu32(a,b)	c_mulu32((a),(b))
# define divu32(a,b)	c_divu32((a),(b))

#endif /* __m68k__ */

/* Peek/Poke Motorola bigendian word and longword. */

#ifndef U16
static inline u16_t always_inline host_u16(const uint8_t * const v)
{ return *(const uint16_t *)v; }
static inline u16_t always_inline byte_u16(const uint8_t * const v)
{ return ((u16_t)v[0]<<8) | v[1]; }
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define U16(v) host_u16((v))
# else
#  define U16(v) byte_u16((v))
# endif
#endif

#ifndef U32
static inline u32_t always_inline host_u32(const uint8_t * const v)
{ return *(const uint32_t *)v; }
static inline u32_t always_inline byte_u32(const uint8_t * const v)
{ return ((u32_t)U16(v)<<16) | U16(v+2); }
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define U32(v) host_u32((v))
# else
#  define U32(v) byte_u32((v))
# endif
#endif

#ifndef SET_U16
static inline void always_inline poke_u16(uint8_t * const a, u16_t v)
{ *(uint16_t *)a = v; }
static inline void always_inline poke_b16(uint8_t * const a, u16_t v)
{ a[0] = v>>8; a[1] = v; }
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define SET_U16(a,v) poke_u16((a),(v))
# else
#  define SET_U16(a,v) poke_b16((a),(v))
# endif
#endif

#ifndef SET_U32
static inline void always_inline poke_u32(uint8_t * const a, u32_t v)
{ *(uint32_t *)a = v; }
static inline void always_inline poke_b32(uint8_t * const a, u32_t v)
{ a[0] = v>>24; a[1] = v>>16; a[2] = v>>8; a[3] = v; }
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define SET_U32(a,v) poke_u32((a),(v))
# else
#  define SET_U32(a,v) poke_b32((a),(v))
# endif
#endif

/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Mixer helper.
 * @{
 */
ZZ_EXTERN_C
void map_i16_to_i16(int16_t * d,
		    const i16_t * va, const i16_t * vb,
		    const i16_t * vc, const i16_t * vd,
		    const i16_t sc1, const i16_t sc2, int n);

#ifndef NO_FLOAT

ZZ_EXTERN_C
void map_flt_to_i16(int16_t * d,
		    const float * va, const float * vb,
		    const float * vc, const float * vd,
		    const float sc1, const float sc2, const int n);

ZZ_EXTERN_C
void i8tofl(float * const d, const uint8_t * const s, const int n);

ZZ_EXTERN_C
void fltoi16(int16_t * const d, const float * const s, const int n);

#endif /* NO_FLOAT */
/**
 * @}
 */



/* ---------------------------------------------------------------------- */

/**
 * Memory, string and binary container functions.
 * @{
 */
#ifndef NO_LIBC

# ifndef zz_memcmp
#  define zz_memcmp(D,S,N) memcmp((D),(S),(N))
# endif

# ifndef zz_memcpy
#  define zz_memcpy(D,S,N) memmove((D),(S),(N)) /* memmove() required */
# endif

# ifndef zz_memclr
#  define zz_memclr(D,N) memset((D),0,(N))
# endif

# ifndef zz_memset
#  define zz_memset(D,V,N) memset((D),(V),(N))
# endif

#endif /* NO_LIBC */
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Binary container.
 * @{
 */
ZZ_EXTERN_C
void bin_free(bin_t ** pbin);
ZZ_EXTERN_C
zz_err_t bin_alloc(bin_t ** pbin, u32_t len, u32_t xlen);
ZZ_EXTERN_C
zz_err_t bin_read(bin_t * bin, vfs_t vfs, u32_t off, u32_t len);
ZZ_EXTERN_C
zz_err_t bin_load(bin_t ** pbin, vfs_t vfs, u32_t len, u32_t xlen, u32_t max);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * VFS functions.
 * @{
 */
ZZ_EXTERN_C
zz_err_t vfs_register(const vfs_dri_t * dri);
ZZ_EXTERN_C
zz_err_t vfs_unregister(const vfs_dri_t * dri);
ZZ_EXTERN_C
const char * vfs_uri(vfs_t vfs);
ZZ_EXTERN_C
void vfs_del(vfs_t * pvfs);
ZZ_EXTERN_C
vfs_t vfs_new(const char * uri, ...);
ZZ_EXTERN_C
zz_err_t vfs_open_uri(vfs_t * pvfs, const char * uri);
ZZ_EXTERN_C
zz_err_t vfs_open(vfs_t vfs);
ZZ_EXTERN_C
zz_u32_t vfs_read(vfs_t vfs, void *b, zz_u32_t n);
ZZ_EXTERN_C
zz_err_t vfs_read_exact(vfs_t vfs, void *b, zz_u32_t n);
ZZ_EXTERN_C
zz_u32_t vfs_tell(vfs_t vfs);
ZZ_EXTERN_C
zz_u32_t vfs_size(vfs_t vfs);
ZZ_EXTERN_C
zz_err_t vfs_seek(vfs_t vfs, zz_u32_t pos, zz_u8_t set);
ZZ_EXTERN_C
zz_err_t vfs_push(vfs_t vfs, const void * b, zz_u8_t n);

/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * voiceset and song file loader.
 * @{
 */
ZZ_EXTERN_C
zz_err_t song_parse(song_t *song, vfs_t vfs, uint8_t *hd, u32_t size);
ZZ_EXTERN_C
zz_err_t song_load(song_t *song, const char *uri);
ZZ_EXTERN_C
zz_err_t vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, u32_t size);
ZZ_EXTERN_C
zz_err_t vset_load(vset_t *vset, const char *uri);
ZZ_EXTERN_C
zz_err_t q4_load(vfs_t vfs, q4_t *q4);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * voiceset and song setup.
 * @{
 */
ZZ_EXTERN_C
zz_err_t song_init_header(zz_song_t const song, const void * hd);
ZZ_EXTERN_C
zz_err_t song_init(zz_song_t const song);
ZZ_EXTERN_C
zz_err_t vset_init_header(zz_vset_t const vset, const void * hd);
ZZ_EXTERN_C
zz_err_t vset_init(zz_vset_t const vset);
ZZ_EXTERN_C
zz_err_t vset_unroll(zz_vset_t const vset, const uint8_t * tohw);
/**
 * @}
 */
