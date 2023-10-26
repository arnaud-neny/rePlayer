/**
 * @file   zz_log.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Messages logging functions.
 */

#include "zz_private.h"

#ifdef NO_LOG

zz_u8_t zz_log_bit(const zz_u8_t clr, const zz_u8_t set) { return 0; }
void zz_log_fun(zz_log_t func, void * user) {}
/* # error zz_log.c should not be compiled with NO_LOG defined */

#else

#ifndef ZZ_LOG_CHANNELS
# ifdef NDEBUG
#  define ZZ_LOG_CHANNELS ((1<<ZZ_LOG_DBG)-1)
# else
#  define ZZ_LOG_CHANNELS ~0
# endif
#endif

static zz_u8_t  log_mask = (zz_u8_t) ZZ_LOG_CHANNELS;
static zz_log_t log_func;
static void    *log_user;

static inline zz_u8_t can_log(const zz_u8_t channel)
{
  return log_func && ( log_mask & ( 1 << channel ) );
}

zz_u8_t zz_log_bit(const zz_u8_t clr, const zz_u8_t set)
{
  uint8_t old_mask = log_mask;          /* uint8_t is truly 8-bit */
  log_mask = ( log_mask & ~clr ) | set;
  return old_mask;
}

void zz_log_fun(zz_log_t func, void * user)
{
  log_func = func;
  log_user = user;
}

void zz_log_err(const char * fmt,...)
{
  va_list list;
  if (can_log(ZZ_LOG_ERR)) {
    va_start(list,fmt);
    log_func(ZZ_LOG_ERR, log_user, fmt, list);
    va_end(list);
  }
}

void zz_log_wrn(const char * fmt,...)
{
  va_list list;
  if (can_log(ZZ_LOG_WRN)) {
    va_start(list,fmt);
    log_func(ZZ_LOG_WRN, log_user, fmt, list);
    va_end(list);
  }
}

void zz_log_inf(const char * fmt,...)
{
  va_list list;
  if (can_log(ZZ_LOG_INF)) {
    va_start(list,fmt);
    log_func(ZZ_LOG_INF, log_user, fmt, list);
    va_end(list);
  }
}

void zz_log_dbg(const char * fmt,...)
{
  va_list list;
  if (can_log(ZZ_LOG_DBG)) {
    va_start(list,fmt);
    log_func(ZZ_LOG_DBG, log_user, fmt, list);
    va_end(list);
  }
}

#endif /* NO_LOG */
