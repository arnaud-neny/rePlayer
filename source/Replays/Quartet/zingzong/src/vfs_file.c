/**
 * @file   vfs_file.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  standard libc FILE VFS.
 */

#define ZZ_DBG_PREFIX "(vfs-file) "
#include "zz_private.h"

#if defined NO_LIBC || defined NO_VFS
# error vfs_file.c should not be compiled with NO_LIBC or NO_VFS defined
#endif

/* ---------------------------------------------------------------------- */

static zz_err_t x_reg(zz_vfs_dri_t);
static zz_err_t x_unreg(zz_vfs_dri_t);
static zz_u16_t x_ismine(const char *);
static zz_vfs_t x_new(const char *, va_list);
static void x_del(vfs_t);
static const char *x_uri(vfs_t);
static zz_err_t x_open(vfs_t);
static zz_err_t x_close(vfs_t);
static zz_u32_t x_read(vfs_t, void *, zz_u32_t);
static zz_u32_t x_tell(vfs_t);
static zz_u32_t x_size(vfs_t);
static zz_err_t x_seek(vfs_t,zz_u32_t,zz_u8_t);

/* ---------------------------------------------------------------------- */

static struct zz_vfs_dri_s file_dri = {
  "file",
  x_reg, x_unreg,
  x_ismine,
  x_new, x_del, x_uri,
  x_open, x_close, x_read,
  x_tell, x_size, x_seek
};

zz_vfs_dri_t zz_file_vfs(void) { return &file_dri; }

/* ---------------------------------------------------------------------- */

typedef struct vfs_file_s * vfs_file_t;

struct vfs_file_s {
  struct vfs_s X;                       /**< Common to all VFS.   */
  FILE      * fp;                       /**< FILE pointer         */
  char        uri[1];                   /**< Path /!\ LAST /!\    */
};

/* ---------------------------------------------------------------------- */

static zz_err_t x_reg(zz_vfs_dri_t dri)   { return E_OK; }
static zz_err_t x_unreg(zz_vfs_dri_t dri) { return E_OK; }

static zz_u16_t
x_ismine(const char * uri)
{
  zz_assert(uri);
  return (!!*uri) << 10;
}

static const char *
x_uri(vfs_t _vfs)
{
  return ((vfs_file_t)_vfs)->uri;
}

static zz_vfs_t
x_new(const char * uri, va_list list)
{
  zz_err_t ecode;
  int len = strlen(uri);
  vfs_file_t fs = 0;
  ecode = zz_memnew(&fs, sizeof(*fs)+len,0);
  if (likely(E_OK == ecode)) {
    zz_assert(fs);
    fs->X.dri = &file_dri;
    fs->fp = 0;
    zz_memcpy(fs->uri, uri, len+1);
  }
  return (zz_vfs_t) fs;
}

static void
x_del(vfs_t vfs)
{
  zz_assert( vfs );
  zz_memdel(&vfs);
  zz_assert( !vfs );
}

static zz_err_t
x_close(vfs_t const _vfs)
{
  int ret;
  vfs_file_t const fs = (vfs_file_t) _vfs;
  if (!fs->fp)
    ret = 0;
  else if (ret = fclose(fs->fp), !ret)
    fs->fp = 0;
  else
    fs->X.err = errno;
  return E_SYS & -!!ret;
}

static zz_err_t
x_open(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  if (fs->fp) {
    dmsg("file pointer is not nil. Already opened ?\n");
    fs->X.err = EINVAL;
    return E_SYS;
  }
  fs->fp = fopen(fs->uri,"rb");
  fs->X.err = errno;
  return E_SYS & -!fs->fp;
}

static zz_u32_t
x_read(vfs_t const _vfs, void * ptr, zz_u32_t n)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  size_t ret = fread(ptr,1,n,fs->fp);
  fs->X.err = errno;
  return ret;
}

static zz_u32_t
x_tell(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  size_t ret = ftell(fs->fp);
  fs->X.err = errno;
  return ret;
}

static zz_u32_t
x_size(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  size_t tell, size;
  size_t ret =
    (-1 == (tell = ftell(fs->fp))    || /* save position    */
     -1 == fseek(fs->fp,0,SEEK_END)  || /* go to end        */
     -1 == (size = ftell(fs->fp))    || /* size = position  */
     -1 == fseek(fs->fp,tell,SEEK_SET)) /* restore position */
    ? ZZ_EOF : size;
  fs->X.err = errno;
  return ret;
}

static zz_err_t
x_seek(vfs_t const _vfs, zz_u32_t offset, zz_u8_t whence)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  int ret = fseek(fs->fp,offset,whence);
  fs->X.err = errno;
  zz_assert ( ret == 0 || ret == -1 );
  return E_SYS & ret;
}
