/**
 * @file   zz_mem.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-01
 * @brief  memory functions.
 *
 * API functions:
 *
 * - void zz_mem(new,del)
 *
 * Exported functions:
 *
 * - zz_memnew(size,clear)
 * - zz_memdel(ptr)
 *
 * These functions are exported. However if NO_LIBC is defined they
 * will issue a warning on call.
 *
 * - zz_memcpy(d,s,n)
 * - zz_memset(d,v,n)
 * - zz_memclr(d,n)
 * - zz_memcmp(a,b,n)
 *
 */

#define ZZ_DBG_PREFIX "(mem) "
#include "zz_private.h"

#ifdef NO_LIBC

/* **********************************************************************
   No libc : memory handler.
*/

static zz_new_t newf = 0;
static zz_del_t delf = 0;

# ifndef NDEBUG
zz_err_t zz_memchk_calls(void)          { return E_OK; }
zz_err_t zz_memchk_block(const void *m) { return E_OK; }
# endif

#else /* NO_LIBC */

# include <string.h>
# include <stdlib.h>

# ifdef NDEBUG

/* **********************************************************************
   Release build : no memory checker
*/

static void* zz_libc_new(zz_u32_t n) { return malloc(n); }
static void  zz_libc_del(void * p)   { free(p); }
static zz_new_t newf = zz_libc_new;
static zz_del_t delf = zz_libc_del;
zz_err_t zz_memchk_calls(void)          { return E_OK; }
zz_err_t zz_memchk_block(const void *m) { return E_OK; }

# else /* NDEBUG */

/* **********************************************************************
   Anything else : used memory checker
*/

static u32_t mem_calls, mem_bytes;
static const char mem_fcc[4] = { 'Z', 'M', '3', 'm' };

typedef struct {
  void   *me;
  u32_t   len;
  char    fcc[4];
  uint8_t buf[1];
} memchk_t ;


#define XTRA ((intptr_t)((memchk_t *)0)->buf)

zz_err_t zz_memchk_calls(void)
{
  if (mem_calls || mem_bytes) {
    wmsg("!!! (mem) check failed -- calls:%lu bytes:%lu\n",
         LU(mem_calls), LU(mem_bytes));
    return E_ERR;
  }
  return E_OK;
}


static inline memchk_t * memchk_of(const void * const ptr)
{
  return  (memchk_t *) ( (intptr_t) ptr - XTRA );
}


zz_err_t zz_memchk_block(const void * p)
{
  const memchk_t * const memchk = memchk_of(p);
  const uint8_t * end;

  if (memchk->me != &memchk->me) {
    wmsg("free: not me ? <%p> != <%p>\n", memchk->me, &memchk->me);
    zz_assert( ! "not me ?" );
    return E_666;
  }

  end = (const uint8_t *)memchk->fcc;
  if (!FCC_EQ(end,mem_fcc)) {
    wmsg("free: invalid header 4cc ? %02hX-%02hX-%02hX-%02hX\n",
         HU(end[0]), HU(end[1]), HU(end[2]), HU(end[3]));
    zz_assert( ! "header 4cc ?" );
    return E_666;
  }

  end = memchk->buf + memchk->len;
  if (!FCC_EQ(end,mem_fcc)) {
    wmsg("free: invalid footer 4cc ? %02hX-%02hX-%02hX-%02hX\n",
         HU(end[0]), HU(end[1]), HU(end[2]), HU(end[3]));
    zz_assert( ! "footer 4cc ?" );
    return E_666;
  }

  return E_OK;
}


static void * zz_libc_new(u32_t n)
{
  memchk_t * memchk;

  zz_assert(n);
  memchk = malloc(XTRA + n + 4);
  if ( unlikely(!memchk) ) {
    emsg("(%i) %s -- %s\n", errno, strerror(errno), "malloc");
    return 0;
  }

  mem_calls++;                          /* GB: $$$ should be atomic */
  mem_bytes += n;                       /* GB: $$$ should be atomic */

  memchk->me = &memchk->me;
  zz_memcpy(memchk->fcc,   mem_fcc, 4);
  zz_memcpy(memchk->buf+n, mem_fcc, 4);
  memchk->len = n;
  zz_memset(memchk->buf, 0x55, memchk->len);

  dmsg("new:%p +%lu (%lu/%lu)\n",
       memchk->buf, LU(n), LU(mem_calls),LU(mem_bytes));
  return memchk->buf;
}


static void zz_libc_del(void * p)
{
  zz_assert (p);

  if ( likely ( E_OK == zz_memchk_block(p)) ) {
    memchk_t * const memchk = memchk_of(p);
    u32_t const n = memchk->len;
    mem_calls--;                          /* GB: $$$ should be atomic */
    zz_assert( mem_calls >= 0 );
    mem_bytes -= memchk->len;             /* GB: $$$ should be atomic */
    zz_assert( mem_bytes < (u32_t)(mem_bytes + memchk->len) );
    zz_assert( (!mem_calls && !mem_bytes) || (mem_calls>0 && mem_bytes>0) );
    zz_memset(memchk, 0x5A, XTRA + memchk->len + 4);
    free(memchk);
    dmsg("del:%p -%lu (%lu/%lu)\n",
         p, LU(n), LU(mem_calls), LU(mem_bytes));
  }
}

static zz_new_t newf = zz_libc_new;
static zz_del_t delf = zz_libc_del;

#endif /* NDEBUG */
#endif /* NO_LIBC */


/* **********************************************************************
   Generic mem allocation functions
*/


static void * mem_alloc(u32_t size)
{
  void * mem;
  zz_assert(size);

  if (unlikely(!newf)) {
    wmsg("(alloc) no memory handler -- %lu", LU(size));
    mem = 0;
  } else
    mem = newf(size);
  return mem;
}


static void mem_free(void * mem)
{
  zz_assert(mem);
  if (unlikely(!delf))
    wmsg("(free) no memory handler");
  else
    delf(mem);
}

/* **********************************************************************
*/

zz_err_t zz_memnew(void * restrict pmem, zz_u32_t size, zz_u8_t clear)
{
  zz_err_t ecode = E_MEM;

  zz_assert( pmem );
  zz_assert( size );
  zz_assert( (clear&1) == clear );

  if (likely(pmem)) {
    void * mem = unlikely(!size)
      ? 0
      : mem_alloc(size)
      ;
    zz_assert( ! *(void**)pmem );       /*  GB: a tad conservative */
    zz_assert( mem );
    *(void**)pmem = mem;
     if (likely(mem)) {
       ecode = E_OK;
       if (clear)
         zz_memclr(mem, size);
     }
  }
  return ecode;
}

void zz_memdel(void * restrict pmem)
{
  zz_assert(pmem);
  if (likely(pmem)) {
    void * const mem = *(void **)pmem;
    if (likely(mem)) {
      *(void**)pmem = 0;
      mem_free(mem);
    }
  }
}


/* Install memory handlers. */
void zz_mem(zz_new_t user_newf, zz_del_t user_delf)
{
  zz_assert(user_newf);
  newf = user_newf;
  zz_assert(user_delf);
  delf = user_delf;
}


/* GB: /!\ ORDER IS IMPORTANT HERE /!\
 *
 *     We don't want to un-define a function before when we are going
 *     to use it.
 */

#undef zz_memxla
void * zz_memxla(void * restrict _d, const void * _s,
                 const uint8_t *xla, zz_u32_t n)
{
#ifdef __m68k__
  asm volatile ("illegal \n\t"); /* should be calling m68k_memxla() */
  return _d;
#elif defined NO_LIBC
  if (n) {
    if (!xla)
      return zz_memcpy(_d,_s,n);
    else {
      uint8_t * d = _d; const uint8_t * s = _s;
      if (d <= s)
        do { *d++ = xla[ *s++ ]; } while (--n);
      else {
        d += n, s += n;
        do { *--d = xla[ *--s ]; } while (--n);
      }
    }
  }
  return _d;
#else
  wmsg("calling %s() instead of %s()\n", __func__, __func__+3);
  return memmove(_d,_s,n);
#endif
}

#undef zz_memcpy
void * zz_memcpy(void * restrict _d, const void * _s, zz_u32_t n)
{
#ifdef __m68k__
  asm volatile ("illegal \n\t"); /* should be calling m68k_memcpy() */
  return _d;
#elif defined NO_LIBC
  if (n) {
    uint8_t * d = _d; const uint8_t * s = _s;
    if (d < s)
      do { *d++ = *s++; } while (--n);
    else if (d > s) {
      d += n, s += n;
      do { *--d = *--s; } while (--n);
    }
  }
  return _d;
#else
  wmsg("calling %s() instead of %s()\n", __func__, __func__+3);
  return memmove(_d,_s,n);
#endif
}


#undef zz_memclr
void * zz_memclr(void * restrict _d, zz_u32_t n)
{
#ifdef NO_LIBC
  return zz_memset(_d,0,n);
#else
  wmsg("calling %s() instead of %s()\n", __func__, __func__+3);
  return memset(_d,0,n);
#endif
}

#undef zz_memset
void * zz_memset(void * restrict _d, int v, zz_u32_t n)
{
#ifdef __m68k__
  asm volatile ("illegal \n\t"); /* should be calling m68k_memset() */
  return _d;
#elif defined NO_LIBC
  uint8_t * restrict d = _d;
  while (n--) *d++ = v;
  return _d;
#else
  wmsg("calling %s() instead of %s()\n", __func__, __func__+3);
  return memset(_d,v,n);
#endif
}


#undef zz_memcmp
int zz_memcmp(const void *_a, const void *_b, zz_u32_t n)
{
#ifdef __m68k__
  asm volatile ("illegal \n\t"); /* should be calling m68k_memcmp() */
  return 0;
#elif defined NO_LIBC
  int8_t c = 0;
  const uint8_t *a = _a, *b = _b;
  if (n) do {
      c = *a++ - *b++;
    } while (!c && --n);
  return c;
#else
  wmsg("calling %s() instead of %s()\n", __func__, __func__+3);
  return memcmp(_a,_b,n);
#endif
}

