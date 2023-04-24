#ifndef NORMALIZE_H__
#define NORMALIZE_H__

#include "include/nezplug/nezint.h"

#ifndef Inline
#ifdef __GNUC__
#define Inline        __attribute__((always_inline)) inline
#else
#define Inline
#endif
#endif

#ifndef Unused
#ifdef __GNUC__
#define Unused        __attribute__((unused))
#else
#define Unused
#endif
#endif

#ifndef NEVER_REACH
#ifdef _MSC_VER
#define NEVER_REACH __assume(0);
#else
#define NEVER_REACH
#endif
#endif

#include <stdlib.h>
#include <memory.h>

#define XMALLOC(s)        malloc(s)
#define XREALLOC(p,s)     realloc(p,s)
#define XFREE(p)          free(p)
#define XMEMCPY(d,s,n)    memcpy(d,s,n)
#define XMEMSET(d,c,n)    memset(d,c,n)

#ifndef PROTECTED
#define PROTECTED extern
#endif

#ifndef PROTECTED_VAR
#define PROTECTED_VAR
#endif

#define strncasecmp _strnicmp

#endif /* NORMALIZE_H__ */
