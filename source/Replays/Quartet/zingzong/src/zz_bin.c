/**
 * @file   zz_bin.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Binary containers.
 */

#define ZZ_DBG_PREFIX "(bin) "
#include "zz_private.h"

void
bin_free(bin_t ** pbin)
{
  if (pbin && *pbin)
    dmsg("free <%p>:%lu:%lu\n",
         (*pbin), LU((*pbin)->len), LU((*pbin)->max));
  zz_free(pbin);
}

zz_err_t
bin_alloc(bin_t ** pbin, u32_t len, u32_t xlen)
{
  zz_err_t ecode;
  const u32_t size   = len + xlen;
  const u32_t nalloc = size + (intptr_t)(((bin_t*)0)->_buf);
  bin_t * bin = 0;
  zz_assert(pbin);
  zz_assert(len);

  do {
    ecode = E_ARG;
    if (!pbin)
      break;
    ecode = zz_memnew(pbin, nalloc, 0);
    if (ecode)
      break;
    bin = *pbin;
    zz_assert(bin);
    bin->ptr = bin->_buf;
    bin->max = size;
    bin->len = len;
  } while (0);

  dmsg("<%p..%p> %lu/%lu/%lu\n",
       bin, bin->_buf+bin->max,
       LU(len), LU(xlen), LU(nalloc));

  return ecode;
}

zz_err_t
bin_read(bin_t * bin, vfs_t vfs, u32_t off, u32_t len)
{
  return vfs_read_exact(vfs, bin->ptr+off, len)
    ? E_SYS
    : E_OK
    ;
}

zz_err_t
bin_load(bin_t ** pbin, vfs_t vfs, u32_t len, u32_t xlen, u32_t max)
{
  int ecode;

  ecode = E_ARG;
  if (!vfs || ! pbin)
    goto error;

  if (*pbin) {
    wmsg("deleting a previous voice set");
    bin_free(pbin);
  }
  zz_assert( !*pbin );

  if (!len) {
    int pos;
    if (-1 == (pos = vfs_tell(vfs)) ||
        -1 == (len = vfs_size(vfs))) {
      ecode = E_SYS;
      goto error;
    }
    len -= pos;
  }
  if (max && len > max) {
    dmsg("too large (load > %lu) -- %s\n", LU(max), vfs_uri(vfs));
    ecode = E_ERR;
    goto error;
  }

  ecode = bin_alloc(pbin, len, xlen);
  if (ecode)
    goto error;
  ecode = bin_read(*pbin, vfs, 0, len);

error:
  if (ecode)
    bin_free(pbin);
  return ecode;
}
