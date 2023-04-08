#define _CRT_SECURE_NO_WARNINGS

#ifndef inline
# define inline __inline
#endif
//#define snprintf _snprintf
#define strcasecmp stricmp

#undef HAVE_DECLSPEC

#if defined (_DEBUG) && !defined(DEBUG)
# define DEBUG _DEBUG
#endif
#if defined (_NDEBUG) && !defined(NDEBUG)
# define NDEBUG _NDEBUG
#endif

/* headers */
#define HAVE_WINDOWS_H 1
#define HAVE_WINREG_H 1
#define HAVE_ASSERT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDIO_H  1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STDINT_H 1
#undef  HAVE_LIBGEN_H
#undef  HAVE_UNISTD_H

/* functions */
#define HAVE_GETENV 1
#define HAVE_FREE 1
#define HAVE_MALLOC 1
#undef HAVE_BASENAME

/* Other stuff */
//#define _POSIX_ 0

//#define memcmp(a,b,c) _memcmp(a,b,c)
//#define read(a,b,c) _read(a,b,c)
//#define lseek(a,b,c) _lseek(a,b,c)
//#define _CRT_NONSTDC_DEPRECATE

#define CPP_SUPPORTS_VA_MACROS 1
#pragma warning(disable: 4996)
