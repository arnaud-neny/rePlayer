/* config.h.in.  Generated from configure.ac by autoheader.  */

/* A form that will not confuse apibuild.py */
#undef ATTRIBUTE_DESTRUCTOR

/* Define to 1 if you have the <arpa/inet.h> header file. */
#undef HAVE_ARPA_INET_H

/* Define if __attribute__((destructor)) is accepted */
#undef HAVE_ATTRIBUTE_DESTRUCTOR

/* Define to 1 if you have the <dlfcn.h> header file. */
#undef HAVE_DLFCN_H

/* Have dlopen based dso */
#undef HAVE_DLOPEN

/* Define to 1 if you have the <dl.h> header file. */
#undef HAVE_DL_H

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `ftime' function. */
#undef HAVE_FTIME

/* Define to 1 if you have the `getentropy' function. */
#undef HAVE_GETENTROPY

/* Define to 1 if you have the `gettimeofday' function. */
#undef HAVE_GETTIMEOFDAY

/* Define to 1 if you have the <glob.h> header file. */
#undef HAVE_GLOB_H

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define if history library is there (-lhistory) */
#undef HAVE_LIBHISTORY

/* Define if readline library is there (-lreadline) */
#undef HAVE_LIBREADLINE

/* Define to 1 if you have the <lzma.h> header file. */
#undef HAVE_LZMA_H

/* Define to 1 if you have the `mmap' function. */
#undef HAVE_MMAP

/* Define to 1 if you have the `munmap' function. */
#undef HAVE_MUNMAP

/* mmap() is no good without munmap() */
#if defined(HAVE_MMAP) && !defined(HAVE_MUNMAP)
#  undef /**/ HAVE_MMAP
#endif

/* Define to 1 if you have the <netdb.h> header file. */
#undef HAVE_NETDB_H

/* Define to 1 if you have the <netinet/in.h> header file. */
#undef HAVE_NETINET_IN_H

/* Define to 1 if you have the <poll.h> header file. */
#undef HAVE_POLL_H

/* Define to 1 if you have the <pthread.h> header file. */
#undef HAVE_PTHREAD_H

/* Have shl_load based dso */
#undef HAVE_SHLLOAD

/* Define to 1 if you have the `stat' function. */
#undef HAVE_STAT

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#undef HAVE_SYS_MMAN_H

/* Define to 1 if you have the <sys/random.h> header file. */
#undef HAVE_SYS_RANDOM_H

/* Define to 1 if you have the <sys/select.h> header file. */
#undef HAVE_SYS_SELECT_H

/* Define to 1 if you have the <sys/socket.h> header file. */
#undef HAVE_SYS_SOCKET_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/timeb.h> header file. */
#undef HAVE_SYS_TIMEB_H

/* Define to 1 if you have the <sys/time.h> header file. */
#undef HAVE_SYS_TIME_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to 1 if you have the <zlib.h> header file. */
#undef HAVE_ZLIB_H

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#undef LT_OBJDIR

/* Name of package */
#define PACKAGE "libxml2"

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#define PACKAGE_NAME "libxml2"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libxml2 2.13.5"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libxml2"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://gitlab.gnome.org/GNOME/libxml2"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.13.5"

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#undef STDC_HEADERS

/* Support for IPv6 */
#undef SUPPORT_IP6

/* Version number of package */
#undef VERSION

/* Determine what socket length (socklen_t) data type is */
#undef XML_SOCKLEN_T

/* TLS specifier */
#undef XML_THREAD_LOCAL
