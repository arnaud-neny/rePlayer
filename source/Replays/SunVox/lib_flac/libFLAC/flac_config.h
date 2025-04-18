//Manual config based on config.cmake.h.in for the SunDog Engine

#define SUNDOG_NO_DEFAULT_INCLUDES
#include "sundog.h"

#define FLAC__NO_DLL 1
#ifdef OS_WINCE
    //HACK
    //(we don't need it in SunDog-based apps)
    #define __stat64 stat
    #define ftello64 ftell
    #define fseeko64 fseek
    #define utime_utf8 utime
#endif

#ifdef ARCH_ARM64
    #define FLAC__CPU_ARM64
#endif

#if defined(ARCH_X86) || defined(ARCH_ARM) || defined(ARCH_MIPS)
    #define FLAC__BYTES_PER_WORD 4
    #define ENABLE_64_BIT_WORDS 0
#else
    #define FLAC__BYTES_PER_WORD 8
    #define ENABLE_64_BIT_WORDS 1
#endif

/* define to align allocated memory on 32-byte boundaries */
#if defined(ARCH_X86) || defined(ARCH_X86_64)
    #define FLAC__ALIGN_MALLOC_DATA 1
#endif

/* Set to 1 if <x86intrin.h> is available. */
#if !defined(NOSIMD) && defined(ARCH_X86) || defined(ARCH_X86_64)
    #define FLAC__HAS_X86INTRIN 1
#endif

/* Set to 1 if <arm_neon.h> is available. */
#if !defined(NOSIMD) && defined(ARCH_ARM64)
    #define FLAC__HAS_NEONINTRIN 1
    #define FLAC__HAS_A64NEONINTRIN 1
#endif

/* define if building for Darwin / MacOS X */
#ifdef OS_APPLE
    #define FLAC__SYS_DARWIN
#endif

/* define if building for Linux */
#if defined(OS_LINUX) || defined(OS_ANDROID)
    #define FLAC__SYS_LINUX
#endif

#define OGG_FOUND 0
#define FLAC__HAS_OGG OGG_FOUND

/* Compiler has the __builtin_bswap16 intrinsic */
//#define HAVE_BSWAP16

/* Compiler has the __builtin_bswap32 intrinsic */
//#define HAVE_BSWAP32

/* Define to 1 if you have the <byteswap.h> header file. */
//#define HAVE_BYTESWAP_H 1

/* define if you have clock_gettime */
#define HAVE_CLOCK_GETTIME

/* Define to 1 if you have the <cpuid.h> header file. */
//#define HAVE_CPUID_H 1

/* Define to 1 if fseeko (and presumably ftello) exists and is declared. */
#ifndef OS_WINCE
    #define HAVE_FSEEKO 1
#endif

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET

/* lround support */
#define HAVE_LROUND 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#undef HAVE_SYS_PARAM_H // rePlayer

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if typeof works with your compiler. */
#define HAVE_TYPEOF 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <x86intrin.h> header file. */
#define HAVE_X86INTRIN_H FLAC__HAS_X86INTRIN

/* Define as const if the declaration of iconv() needs const. */
//#define ICONV_CONST

/* Define if debugging is disabled */
#ifndef NDEBUG
    #define NDEBUG
#endif

/* Name of package */
#define PACKAGE ""

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "@PROJECT_VERSION@"

/* The size of `off_t', as computed by sizeof. */
//#define SIZEOF_OFF_T sizeof( off_t )

/* The size of `void*', as computed by sizeof. */
//#define SIZEOF_VOIDP sizeof( void* )
