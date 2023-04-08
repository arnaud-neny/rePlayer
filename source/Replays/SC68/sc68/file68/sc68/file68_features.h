/* file68_features for sc68-msvc */
#ifndef FILE68_FEATURES_H
#define FILE68_FEATURES_H

/**
 * Defined if file68 supports !ice depacker (via unice68)
 */
#define FILE68_UNICE68 1

/**
 * Defined if file68 supports deflate (via zlib).
 */
#define FILE68_Z 1

/**
 * Defined if file68 supports remote files (via libcurl).
 */
#undef FILE68_CURL

/**
 * Defined if file68 supports audio (via libao).
 */
#undef FILE68_AO

/**
 * Minimal sampling rate.
 */
/* #undef FILE68_SPR_MIN */
#ifndef FILE68_SPR_MIN
enum { FILE68_SPR_MIN = 8000 };
#endif

/**
 * Maximal sampling rate.
 */
/* #undef FILE68_SPR_MAX */
#ifndef FILE68_SPR_MAX
enum { FILE68_SPR_MAX = 96000 };
#endif

/**
 * Default sampling rate.
 */
/* #undef FILE68_SPR_DEF */
#ifndef FILE68_SPR_DEF
enum { FILE68_SPR_DEF = 44100 };
#endif

#endif
