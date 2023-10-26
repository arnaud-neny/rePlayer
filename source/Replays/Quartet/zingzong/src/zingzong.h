/**
 * @file   zingzong.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  zingzong public API.
 */

#ifndef ZINGZONG_H
#define ZINGZONG_H

#include <stdarg.h>
#include <stdint.h>

/**
 * Integer types the platform prefers matching our requirements.
 */
typedef	 int_fast8_t  zz_i8_t;
typedef uint_fast8_t  zz_u8_t;
typedef	 int_fast16_t zz_i16_t;
typedef uint_fast16_t zz_u16_t;
typedef	 int_fast32_t zz_i32_t;
typedef uint_fast32_t zz_u32_t;

#ifndef ZINGZONG_API

# ifndef ZZ_EXTERN_C
#  ifdef __cplusplus
#   define ZZ_EXTERN_C extern "C"
#  else
#   define ZZ_EXTERN_C extern
#  endif
# endif

# if !defined ZZ_ATTRIBUT && defined DLL_EXPORT
#  if defined _WIN32 || defined WIN32 || defined __SYMBIAN32__
#   define ZZ_ATTRIBUT __declspec(dllexport)
#  elif __GNUC__ >= 4
#   define ZZ_ATTRIBUT __attribute__ ((visibility ("default"))
#  endif
# endif

# ifndef ZZ_ATTRIBUT
#  define ZZ_ATTRIBUT
# endif

# define ZINGZONG_API ZZ_EXTERN_C ZZ_ATTRIBUT

#endif /* ZINGZONG_API */


/**
 * Zingzong error codes.
 */
enum {
  ZZ_OK,		   /**< (0) No error.                       */
  ZZ_ERR,		   /**< (1) Unspecified error.              */
  ZZ_EARG,		   /**< (2) Argument error.                 */
  ZZ_ESYS,		   /**< (3) System error (I/O, memory ...). */
  ZZ_EINP,		   /**< (4) Problem with input.             */
  ZZ_EOUT,		   /**< (5) Problem with output.            */
  ZZ_ESNG,		   /**< (6) Song error.                     */
  ZZ_ESET,		   /**< (7) Voice set error                 */
  ZZ_EPLA,		   /**< (8) Player error.                   */
  ZZ_EMIX,		   /**< (9) Mixer error.                    */
  ZZ_666 = 66		   /**< Internal error.                     */
};

/**
 * Known (but not always supported) Quartet file format.
 */
enum zz_format_e {
  ZZ_FORMAT_UNKNOWN,	       /**< Not yet determined (must be 0)  */
  ZZ_FORMAT_4V,		       /**< Original Atari ST song.         */
  ZZ_FORMAT_BUNDLE = 64,       /**< Next formats are bundles.       */
  ZZ_FORMAT_4Q,		       /**< Single song bundle (MUG UK ?).  */
  ZZ_FORMAT_QUAR,	       /**< Multi song bundle (SC68).       */
};

/**
 * Mixer identifiers.
 */
enum {
  ZZ_MIXER_XTN = 254,	       /**< External mixer.                 */
  ZZ_MIXER_DEF = 255,	       /**< Default mixer id.               */
  ZZ_MIXER_ERR = ZZ_MIXER_DEF  /**< Error (alias for ZZ_MIXER_DEF). */
};

/**
 * Stereo channel mapping.
 */
enum {
  ZZ_MAP_ABCD,			       /**< (0) Left:A+B Right:C+D. */
  ZZ_MAP_ACBD,			       /**< (1) Left:A+C Right:B+D. */
  ZZ_MAP_ADBC,			       /**< (2) Left:A+D Right:B+C. */
};

/**
 * Sampler quality.
 */
enum zz_quality_e {
  ZZ_FQ = 1,				/**< (1) Fastest quality. */
  ZZ_LQ,				/**< (2) Low quality.     */
  ZZ_MQ,				/**< (3) Medium quality.  */
  ZZ_HQ					/**< (4) High quality.    */
};

typedef zz_i8_t zz_err_t;
typedef struct vfs_s   * zz_vfs_t;
typedef struct vset_s  * zz_vset_t;
typedef struct song_s  * zz_song_t;
typedef struct core_s  * zz_core_t;
typedef struct play_s  * zz_play_t;
typedef struct mixer_s * zz_mixer_t;
typedef const struct zz_vfs_dri_s * zz_vfs_dri_t;
typedef zz_err_t (*zz_guess_t)(zz_play_t const, const char *);
typedef struct zz_info_s zz_info_t;

/**
 * zingzong info.
 */
struct zz_info_s {

  struct {
    zz_u8_t	 num;		    /**< format (@see zz_format_e). */
    const char * str;		    /**< format string.             */
  } fmt;			    /**< format info.               */

  struct {
    zz_u16_t	 rate;		    /**< player tick rate (200hz).  */
    zz_u32_t	 ms;		    /**< song duration in ms.       */
  } len;			    /**< replay info.               */

  /** mixer info. */
  struct {
    zz_u32_t	 spr;	       /**< sampling rate.                  */
    zz_u8_t	 num;	       /**< mixer identifier.               */
    zz_u8_t	 map;	       /**< channel mapping (ZZ_MAP_*).     */
    zz_u16_t	 lr8;	       /**< 0:normal 128:center 256:invert. */

    const char * name;	       /**< mixer name or "".               */
    const char * desc;	       /**< mixer description or "".        */
  } mix;		       /**< mixer related info.             */

  struct {
    const char * uri;		    /**< URI or path.               */
    zz_u32_t	 khz;		    /**< sampling rate reported.    */
  }
  set,				    /**< voice set info.            */
  sng;				    /**< song info.                 */

  struct {
    const char * album;		    /**< album or "".               */
    const char * title;		    /**< title or "".               */
    const char * artist;	    /**< artist or "".              */
    const char * ripper;	    /**< ripper or "".              */
  } tag;			    /**< meta tags.                 */

};


/* **********************************************************************
 *
 * Low level API (core)
 *
 * **********************************************************************/

ZINGZONG_API
/**
 * Get zingzong version string.
 *
 * @retval "zingzong MAJOR.MINOR.PATCH.TWEAK"
 */
const char * zz_core_version(void);

ZINGZONG_API
/**
 * Mute and ignore voices.
 * - LSQ bits (0~3) are ignored channels.
 * - MSQ bits (4~7) are muted channels.
 *
 * @param  play  player instance
 * @param  clr   clear these bits
 * @param  set   set these bits
 * @return old bits
 */
uint8_t zz_core_mute(zz_core_t K, uint8_t clr, uint8_t set);

ZINGZONG_API
/**
 * Init core player.
 */
zz_err_t zz_core_init(zz_core_t core, zz_mixer_t mixer, zz_u32_t spr);

ZINGZONG_API
/**
 * Kill core player.
 */
void zz_core_kill(zz_core_t core);

ZINGZONG_API
/**
 * Play one tick.
 */
zz_err_t zz_core_tick(zz_core_t const core);

ZINGZONG_API
/**
 * Play one tick by calling zz_core_tick() and generate audio.
 */
zz_i16_t zz_core_play(zz_core_t core, void * pcm, zz_i16_t n);

ZINGZONG_API
/**
 * Set channel blending.
 */
zz_u32_t zz_core_blend(zz_core_t core, zz_u8_t map, zz_u16_t lr8);


/* **********************************************************************
 *
 * Logging and memory allocation
 *
 * **********************************************************************/

/**
 * Log level (first parameter of zz_log_t function).
 */
enum zz_log_e {
  ZZ_LOG_ERR,				/**< Log error.   */
  ZZ_LOG_WRN,				/**< Log warning. */
  ZZ_LOG_INF,				/**< Log info.    */
  ZZ_LOG_DBG				/**< Log debug.   */
};

/**
 * Zingzong log function type (printf-like).
 */
typedef void (*zz_log_t)(zz_u8_t,void *,const char *,va_list);

ZINGZONG_API
/**
 * Get/Set zingzong active logging channels.
 *
 * @param  clr  bit mask of channels to disable).
 * @param  set  bit mask of channels to en able).
 * @return previous active logging channel mask.
 */
zz_u8_t zz_log_bit(const zz_u8_t clr, const zz_u8_t set);

ZINGZONG_API
/**
 * Set Zingzong log function.
 *
 * @param func  pointer to the new log function (0: to disable all).
 * @param user  pointer user private data (parameter #2 of func).
 */
void zz_log_fun(zz_log_t func, void * user);

/**
 * Memory allocation function types.
 */
typedef void * (*zz_new_t)(zz_u32_t); /**< New memory function type. */
typedef void   (*zz_del_t)(void *);   /**< Del memory function type. */

ZINGZONG_API
/**
 * Set Zingzong memory management function.
 *
 * @param  newf pointer to the memory allocation function.
 * @param  delf pointer to the memory free function.
 */
void zz_mem(zz_new_t newf, zz_del_t delf);

ZINGZONG_API
/**
 * Create a new player instance.
 *
 * @param pplay pointer to player instance
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_new(zz_play_t * pplay);

ZINGZONG_API
/**
 * Delete player instance.
 * @param pplay pointer to player instance
 */
void zz_del(zz_play_t * pplay);


/* **********************************************************************
 *
 * High level API
 *
 * **********************************************************************/

ZINGZONG_API
/**
 * Load quartet song and voice-set.
 *
 * @param  play  player instance
 * @param  song  song URI or path ("": skip).
 * @param  vset  voice-set URI or path (0:guess "":skip)
 * @param  pfmt  points to a variable to store file format (can be 0).
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_load(zz_play_t const play,
		 const char * song, const char * vset,
		 zz_u8_t * pfmt);

ZINGZONG_API
/**
 * Close player (release allocated resources).
 *
 * @param  play  player instance
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_close(zz_play_t const play);

ZINGZONG_API
/**
 * Get player info.
 *
 * @param  play  player instance
 * @param  info  info filled by zz_info().
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_info(zz_play_t play, zz_info_t * pinfo);

ZINGZONG_API
/**
 * Init player.
 *
 * @param  play   player instance
 * @param  rate   player tick rate (0:default)
 * @param  ms     playback duration (0:infinite, ZZ_EOF:measured)
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_init(zz_play_t play, zz_u16_t rate, zz_u32_t ms);

ZINGZONG_API
/**
 * Setup mixer.
 *
 * @param  play   player instance
 * @param  mixer  mixer-id
 * @param  spr    sampling rate or quality
 * @return error code
 * @retval ZZ_OK(0) on success
 * @notice Call zz_init() before zz_setup().
 */
zz_err_t zz_setup(zz_play_t play, zz_u8_t mixer, zz_u32_t spr);

ZINGZONG_API
/**
 * Play a tick.
 *
 * @param  play   player instance
 * @return error code
 * @retval ZZ_OK(0) on success
 * @notice zz_tick() is called by zz_play().
 */
zz_err_t zz_tick(zz_play_t play);

ZINGZONG_API
/**
 * Play.
 *
 * @param  play  player instance
 * @param  pcm   pcm buffer (format might depend on mixer).
 * @param  n     >0: number of pcm to fill
 *                0: get number of pcm to complete the tick.
 *               <0: complete the tick but not more than -n pcm.
 *
 * @return number of pcm.
 * @retval 0 play is over
 * @retval >0 number of pcm
 * @retval <0 -error code
 */
zz_i16_t zz_play(zz_play_t play, void * pcm, zz_i16_t n);

ZINGZONG_API
/**
 * Get current play position (in ms).
 * @return number of millisecond
 * @retval ZZ_EOF on error
 */
zz_u32_t zz_position(zz_play_t play);

ZINGZONG_API
/**
 * Get info mixer info.
 *
 * @param  id     mixer identifier (first is 0)
 * @param  pname  receive a pointer to the mixer name
 * @param  pdesc  receive a pointer to the mixer description
 * @return mixer-id (usually id unless id is ZZ_MIXER_DEF)
 * @retval ZZ_MIXER_DEF on error
 *
 * @notice The zz_mixer_info() function can be use to enumerate all
 *         available mixers.
 */
zz_u8_t zz_mixer_info(zz_u8_t id, const char **pname, const char **pdesc);

/**
 * Channels re-sampler and mixer interface.
 */
struct mixer_s {
  const char * name;		     /**< friendly name and method. */
  const char * desc;		     /**< mixer brief description.  */

  /** Init mixer function. */
  zz_err_t (*init)(zz_core_t const, zz_u32_t);

  /** Release mixer function. */
  void (*free)(zz_core_t const);

  /** Push PCM function. */
  zz_i16_t (*push)(zz_core_t const, void *, zz_i16_t);
};

/* **********************************************************************
 *
 * VFS
 *
 * **********************************************************************/

enum {
  ZZ_SEEK_SET, ZZ_SEEK_CUR, ZZ_SEEK_END
};

#define ZZ_EOF ((zz_u32_t)-1)

/**
 * Virtual filesystem driver.
 */
struct zz_vfs_dri_s {
  const char * name;			  /**< friendly name.      */
  zz_err_t (*reg)(zz_vfs_dri_t);	  /**< register driver.    */
  zz_err_t (*unreg)(zz_vfs_dri_t);	  /**< un-register driver. */
  zz_u16_t (*ismine)(const char *);	  /**< is mine.            */
  zz_vfs_t (*create)(const char *, va_list); /**< create VFS.         */
  void	   (*destroy)(zz_vfs_t);		  /**< destroy VFS.        */
  const
  char *   (*uri)(zz_vfs_t);			/**< get URI.       */
  zz_err_t (*open)(zz_vfs_t);			/**< open.          */
  zz_err_t (*close)(zz_vfs_t);			/**< close.         */
  zz_u32_t (*read)(zz_vfs_t, void *, zz_u32_t); /**< read.          */
  zz_u32_t (*tell)(zz_vfs_t);			/**< get position.  */
  zz_u32_t (*size)(zz_vfs_t);			/**< get size.      */
  zz_err_t (*seek)(zz_vfs_t,zz_u32_t,zz_u8_t);	/**< offset,whence. */
};

/**
 * Common (inherited) part to all VFS instance.
 */
struct vfs_s {
  zz_vfs_dri_t dri;		    /**< pointer to the VFS driver. */
  int err;			    /**< last error number.         */
  int pb_pos;			    /**< push-back position.        */
  int pb_len;			    /**< push-back length.          */
  uint8_t pb_buf[16];		    /**< push-back buffer.          */
};

ZINGZONG_API
/**
 * Register a VFS driver.
 * @param  dri  VFS driver
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_vfs_add(zz_vfs_dri_t dri);

ZINGZONG_API
/**
 * Unregister a VFS driver.
 * @param  dri  VFS driver
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_vfs_del(zz_vfs_dri_t dri);

#endif /* #ifndef ZINGZONG_H */
