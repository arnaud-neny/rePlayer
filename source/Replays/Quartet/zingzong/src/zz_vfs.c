/**
 * @file   zz_vfs.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High level VFS functions.
 */

#define ZZ_DBG_PREFIX "(vfs) "
#include "zz_private.h"

#ifdef NO_VFS
# error	 zz_vfs.c should not be compiled with NO_VFS defined
#endif


static inline int valid_vfs(const vfs_t vfs) { return vfs != 0; }

#define VFS_OR(E,X) if (!valid_vfs((E))) { return (X); } else (E)->err=0
#define VFS_OR_EOF(E) VFS_OR( (E) , ZZ_EOF    )
#define VFS_OR_ARG(E) VFS_OR( (E) , E_ARG     )
#define VFS_OR_NIL(E) VFS_OR( (E) , (void *)0 )

#ifndef DRIVER_MAX
# define DRIVER_MAX 8
#endif

static zz_vfs_dri_t drivers[DRIVER_MAX];

#ifdef NO_LOG
# define vfs_emsg(DRI,ERR,FCT,ALT,OBJ) zz_nop
# else
static void
vfs_emsg(const char * dri, int err, const char * fct,
	 const char * alt, const char * obj)
{
  const char* msg = alt ? alt : "failed";

#ifndef NO_LIBC
  if (err)
    msg = strerror(err);
#endif
  if (!obj)
    emsg("VFS(%s): %s: (%d) %s\n",
	 dri, fct, err, msg);
  else
    emsg("VFS(%s): %s: (%d) %s -- %s\n",
	 dri, fct, err, msg, obj);
}
#endif

static int
vfs_find(zz_vfs_dri_t dri)
{
  const int max = sizeof(drivers)/sizeof(*drivers);
  int i;
  for (i=0; i<max && drivers[i] != dri; ++i)
    ;
  return i<max ? i : -1;
}

zz_err_t
vfs_register(zz_vfs_dri_t dri)
{
  int i = E_ERR;

  if (dri) {
    i = vfs_find(dri);		    /* looking for this driver slot */
    if (i < 0)			    /* not found ? */
      i = vfs_find(0);		    /* looking for a free slot */
    if (i >= 0) {
      if (dri->reg(dri) >= 0) {
	drivers[i] = dri;
	i = E_OK;
      }
      else
	i = E_ERR;
    }
    if (i != E_OK )
      (void) vfs_emsg(dri->name,0,"register",0,0);
  }
  return E_ERR & i;
}

zz_err_t
vfs_unregister(zz_vfs_dri_t dri)
{
  int i = E_ERR;

  if (dri) {
    i = vfs_find(dri);		    /* looking for this driver slot */
    if (i >= 0) {		    /* found ? */
      int res = drivers[i]->unreg(dri);
      if (!res) {
	drivers[i] = 0;
	i = E_OK;
      }
      else if (res < 0) {
	(void) vfs_emsg(dri->name,0,"unregister",0,0);
	i = E_ERR;
      }
    }
  }

  return i;
}

vfs_t
vfs_new(const char * uri, ...)
{
  const int max = sizeof(drivers)/sizeof(*drivers);
  int i, best, k = -1;
  vfs_t vfs = 0;

  zz_assert(uri);
  for (i=0, best=0, k=-1; i<max; ++i) {
    int score;
    if (!drivers[i]) continue;
    score = drivers[i]->ismine(uri);
    if (score > best) {
      best = score;
      k = i;
    }
  }

  if (k < 0)
    emsg("VFS: not available -- %s\n",uri);
  else {
    va_list list;
    va_start(list,uri);
    vfs = drivers[k]->create(uri, list);
    va_end(list);
    if (vfs)
      vfs->err = 0;
    else
      (void)vfs_emsg(drivers[k]->name,0,"new",0,uri);
  }
  return vfs;
}

zz_err_t
vfs_open(vfs_t vfs)
{
  zz_err_t ecode;
  VFS_OR_ARG(vfs);
  ecode = vfs->dri->open(vfs);
  if (ecode != E_OK)
    (void) vfs_emsg(vfs->dri->name,vfs->err,"open",0,vfs_uri(vfs));
  vfs->pb_pos = vfs->pb_len = 0;
  return ecode;
}

void
vfs_del(vfs_t * pvfs)
{
  vfs_t const vfs = pvfs ? *pvfs : 0;
  if (vfs) {
    vfs->dri->close(vfs);
    vfs->dri->destroy(vfs);
    *pvfs = 0;
  }
}

zz_err_t
vfs_close(vfs_t vfs)
{
  zz_err_t ecode;
  VFS_OR_ARG(vfs);
  ecode = vfs->dri->close(vfs);
  if (E_OK != ecode)
    vfs_emsg(vfs->dri->name,vfs->err,"close", 0, vfs_uri(vfs));
  vfs->pb_pos = vfs->pb_len = 0;
  return ecode;
}

zz_err_t
vfs_push(vfs_t vfs, const void * ptr, zz_u8_t size)
{
  zz_err_t ecode = E_INP;
  VFS_OR_ARG(vfs);
  if (vfs->pb_len != vfs->pb_pos) {
    vfs_emsg(vfs->dri->name,vfs->err,"push", "not empty", vfs_uri(vfs));
  }
  else if (size > sizeof(vfs->pb_buf)) {
    vfs_emsg(vfs->dri->name,vfs->err,"push", "overflow", vfs_uri(vfs));
  }
  else {
    zz_memcpy(vfs->pb_buf, ptr, size);
    vfs->pb_pos = 0;
    vfs->pb_len = size;
    ecode = E_OK;
    dmsg("pushed back %u bytes\n", size);
  }
  return ecode;
}

static zz_u32_t pb_read(zz_vfs_t vfs, void * ptr, zz_u32_t size)
{
  uint8_t * const dst = ptr;
  zz_u32_t cnt, n;

  cnt = 0;
  while ( vfs->pb_pos < vfs->pb_len ) {
    dst[cnt++] = vfs->pb_buf[vfs->pb_pos++];
    if ( cnt == size )
      return size;
  }

  n = vfs->dri->read(vfs, dst+cnt, size-cnt);
  if (n != ZZ_EOF)
    n += cnt;
  return n;
}

zz_u32_t
vfs_read(zz_vfs_t vfs, void * ptr, zz_u32_t size)
{
  if (!size || size == ZZ_EOF)
    return size;
  if (!ptr) vfs = 0;
  VFS_OR_EOF(vfs);
  return pb_read(vfs, ptr, size);
}

zz_err_t
vfs_read_exact(vfs_t vfs, void * ptr, zz_u32_t size)
{
  zz_err_t ecode;
  u32_t n;

  if (!size)
    return E_OK;

  if (size == ZZ_EOF || !ptr)
    vfs = 0;			  /* to trigger an error just below */
  VFS_OR_ARG(vfs);

  n = pb_read(vfs,ptr,size);
  if (n == ZZ_EOF) {
    vfs_emsg(vfs->dri->name, vfs->err,"read_exact", 0, vfs_uri(vfs));
    ecode = E_INP;
  }
  else if (n != size) {
    emsg("%s: read too short by %lu\n",
	 vfs->dri->uri(vfs), LU(size-n));
    ecode = E_INP;
  }
  else ecode = E_OK;

  return ecode;
}

zz_u32_t
vfs_tell(vfs_t vfs)
{
  VFS_OR_EOF(vfs);
  return vfs->dri->tell(vfs);
}

static zz_err_t vfs_seek_simul(vfs_t vfs, zz_u32_t pos, zz_u8_t set);

zz_err_t
vfs_seek(vfs_t vfs, zz_u32_t pos, zz_u8_t set)
{
  zz_err_t ecode = E_INP;
  VFS_OR_ARG(vfs);

  if (vfs->pb_len != vfs->pb_pos) {
    vfs->err = E_ERR;
    vfs_emsg(vfs->dri->name, vfs->err,"seek",
	     "push not empty", vfs_uri(vfs));
    return E_INP;
  }

  ecode = vfs->dri->seek
    ? vfs->dri->seek(vfs,pos,set)
    : vfs_seek_simul(vfs,pos,set)
    ;
  if (ecode)
    vfs_emsg(vfs->dri->name,vfs->err,"seek","seek error", vfs_uri(vfs));

  return ecode;
}

/* GB: I wrote this function that simulates a seek forward by reading
   and discarding when possible. It's completely *UNTESTED* and
   could be improved (for example seek_cur does not need to tell()
   nor size().

   GB: Backward seeks could be implemented to an extend using a
   push-back buffer.
*/
static zz_err_t
vfs_seek_simul(vfs_t vfs, zz_u32_t pos, zz_u8_t set)
{
  zz_u32_t size, tell;

  dmsg("seek-sim[%s]: pos=%lu set=%hu\n", vfs->dri->name, LU(pos), HU(set));
  if ( ZZ_EOF == (size = vfs->dri->size(vfs)) ||
       ZZ_EOF == (tell = vfs->dri->tell(vfs)) )
    return E_INP;

  dmsg("seek-sim[%s]: size=%lu\n", vfs->dri->name, LU(size));
  dmsg("seek-sum[%s]: tell=%lu\n", vfs->dri->name, LU(tell));

  switch (set) {
  case ZZ_SEEK_SET: break;
  case ZZ_SEEK_END: pos = size + pos; break;
  case ZZ_SEEK_CUR: pos = tell + pos; break;
  default:
    vfs_emsg(vfs->dri->name, vfs->err = ZZ_EINVAL,
	     "seek", "invalid whence", vfs_uri(vfs));
    return E_666;
  }
  dmsg("seek-sim[%s]: goto=%lu offset=%+li\n",
       vfs->dri->name, LU(pos), (long int) (pos-tell));

  if (pos < tell || pos > size) {
    dmsg("seek-sim[%s] simulation impossible (%lu/%lu/%lu)\n",
	 vfs->dri->name, LU(pos), LU(tell), LU(size));
    if (!vfs->err) vfs->err = ZZ_EIO;
    return E_INP;
  }

  while (tell < pos) {
    uint8_t tmp[64];
    zz_u32_t r, n = pos-tell;
    if (n > sizeof(tmp)) n = sizeof(tmp);
    r = vfs->dri->read(vfs,tmp,n);
    if (r != n) {
      if (!vfs->err) vfs->err = ZZ_EIO;
      return E_INP;
    }
    tell += n;
  }

  dmsg("seek[%s]: tell=%lu\n",
       vfs->dri->name, LU(vfs->dri->tell(vfs)));

  zz_assert( tell == pos );
  zz_assert( pos == vfs->dri->tell(vfs) );

  return E_OK;
}

zz_u32_t
vfs_size(vfs_t vfs)
{
  VFS_OR_EOF(vfs);
  return vfs->dri->size(vfs);
}

zz_err_t
vfs_open_uri(vfs_t * pvfs, const char * uri)
{
  zz_err_t ecode = E_ARG;
  vfs_t vfs = 0, ice = 0;
  if (likely(pvfs && uri)) {
    ecode = E_MEM;
    vfs = vfs_new(uri,0);
    if (likely(vfs)) {
      ecode = vfs_open(vfs);
      if (E_OK != ecode) {
	vfs_del(&vfs);
	zz_assert(vfs == 0);
      }
      else if (ice = vfs_new("ice://", vfs, 0), !ice) {
	/* Something got wrong while depacking ... */
	ecode = E_SYS;
	vfs_del(&vfs);
      }
      else if (ice != vfs) {
	/* ICE! depacked, we can now destroy vfs */
	vfs_del(&vfs);		/* close and destroy vfs */
	vfs = ice;		/* continue w/ ICE! vfs */
	ecode = vfs_open(vfs);	/* never fails */
	zz_assert(ecode == ZZ_OK);
      }
    }
    *pvfs = vfs;
  }
  return ecode;
}

const char *
vfs_uri(vfs_t vfs)
{
  const char * uri = vfs ? vfs->dri->uri(vfs) : 0;
  if (!uri) uri = "";
  return uri;
}
