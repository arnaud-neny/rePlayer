/**
 * @file   vfs_ice.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2023-08-29
 * @brief  ICE! transparent depacker VFS.
 */

#define ZZ_DBG_PREFIX "(ice) "
#include "zz_private.h"

#if defined NO_ICE || defined NO_VFS
# error vfs_ice.c should not be compiled with NO_ICE or NO_VFS defined
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

int ice_depacker(void *, const void *);
int ice_depacked_size(const void *, int *_csize);

/* ---------------------------------------------------------------------- */

static struct zz_vfs_dri_s ice_dri = {
  "ice!",
  x_reg, x_unreg,
  x_ismine,
  x_new, x_del, x_uri,
  x_open, x_close, x_read,
  x_tell, x_size, x_seek
};

zz_vfs_dri_t zz_ice_vfs(void) { return &ice_dri; }

/* ---------------------------------------------------------------------- */

typedef struct vfs_ice_s * vfs_ice_t;

struct vfs_ice_s {
  struct vfs_s X;			/**< Common to all VFS.   */
  zz_u32_t len;				/**< Unpacked data length */
  zz_u32_t pos;				/**< Current position */
  uint8_t  data[8];			/**< Unpacked data */
};

/* ---------------------------------------------------------------------- */

static zz_err_t x_reg(zz_vfs_dri_t dri)	  { return E_OK; }
static zz_err_t x_unreg(zz_vfs_dri_t dri) { return E_OK; }

static zz_u16_t
x_ismine(const char * uri)
{
  zz_assert(uri);
  return (!strncmp(uri,"ice://",7)) << 12;
}

static const char *
x_uri(vfs_t _vfs)
{
  /* TODO: do something with slave->uri or ICE! header */
  return "ice://";
}


static zz_vfs_t
x_new(const char * uri, va_list list)
{
  const vfs_t slave = va_arg(list, vfs_t);
  vfs_ice_t fs = 0;
  zz_u8_t hd[12], *c_data = 0;
  int no_slave, f_size, d_size, c_size = 0;

  zz_assert( sizeof(int) >= 4 );

  if (no_slave = !slave, no_slave)
    goto error;

  if (f_size = vfs_size(slave), f_size < 14)
    goto error;

  if (ZZ_OK != vfs_read_exact(slave, hd, 12))
    goto error;

  d_size = ice_depacked_size(hd, &c_size);
  dmsg("file:%i packed:%i depacked:%d\n", f_size, c_size, d_size);
  if (d_size < 0 || c_size > f_size || c_size < 14) {
    vfs_push(slave, hd, 12);
    goto error;
  }

  /* From this point we assume it's an ICE! packed file. Any error is
   * now fatal. */
  no_slave = 1;
  if ( ZZ_OK != zz_memnew(&c_data, c_size ,0) )
    goto error;

  zz_memcpy(c_data, hd, 12);
  if (ZZ_OK != vfs_read_exact(slave, c_data+12, c_size-12))
    goto error;

  if (ZZ_OK != zz_memnew(&fs, sizeof(*fs)+d_size, 0))
    goto error;
  fs->X.dri = &ice_dri;

  if ( ice_depacker(fs->data, c_data) )
    goto error;

  zz_memdel(&c_data);
  fs->pos = 0;
  fs->len = d_size;

  return (zz_vfs_t) fs;

error:
  zz_memdel(&c_data);
  zz_memdel(&fs);
  return no_slave ? 0 : slave;
}

static void
x_del(vfs_t vfs)
{
  zz_memdel(&vfs);
}

static zz_err_t
x_close(vfs_t const _vfs)
{
  vfs_ice_t const fs = (vfs_ice_t) _vfs;
  fs->len = fs->pos = 0;
  return ZZ_OK;
}

static zz_err_t
x_open(vfs_t const _vfs)
{
  vfs_ice_t const fs = (vfs_ice_t) _vfs;
  fs->pos = 0;
  return ZZ_OK;
}

static zz_u32_t
x_read(vfs_t const _vfs, void * ptr, zz_u32_t n)
{
  vfs_ice_t const fs = (vfs_ice_t) _vfs;
  const zz_u32_t max = fs->len - fs->pos;
  if (n > max) n = max;

  zz_memcpy(ptr, fs->data+fs->pos, n);
  fs->pos += n;
  return n;
}

static zz_u32_t
x_tell(vfs_t const _vfs)
{
  vfs_ice_t const fs = (vfs_ice_t) _vfs;
  return fs->pos;
}

static zz_u32_t
x_size(vfs_t const _vfs)
{
  vfs_ice_t const fs = (vfs_ice_t) _vfs;
  return fs->len;
}

static zz_err_t
x_seek(vfs_t const _vfs, zz_u32_t offset, zz_u8_t whence)
{
  vfs_ice_t const fs = (vfs_ice_t) _vfs;
  zz_i32_t base;

  switch (whence) {
  case ZZ_SEEK_CUR: base = fs->pos; break;
  case ZZ_SEEK_SET: base = 0; break;
  case ZZ_SEEK_END: base = fs->len; break;
  default:
    return fs->X.err = E_ARG;
  }

  base += offset;
  if (base < 0 || base > (zz_i32_t)fs->len) {
    return fs->X.err = E_SYS;
  }
  fs->pos = base;
  return fs->X.err = ZZ_OK;
}

typedef zz_u8_t	 u8;
typedef zz_i8_t	 s8;
typedef zz_i16_t s16;

typedef u8 * areg_t;
typedef int dreg_t;

typedef struct {
  areg_t a0,a1,a2,a3,a4,a5,a6,a7;
  dreg_t d0,d1,d2,d3,d4,d5,d6,d7;
  areg_t srcbuf,srcend,dstbuf,dstend;
  int overflow;
} all_regs_t;

#define ICE_MAGIC 0x49434521 /* 'ICE!' */

#define B_CC(CC, LABEL) if (CC) {goto LABEL;} else
#define DB_CC(CC, REG, LABEL) if (!(CC) && --REG >= 0) {goto LABEL;} else
#define DBF(REG,LABEL) DB_CC(0, REG, LABEL)
#define GET_1_BIT_BCC(LABEL) B_CC(!get_1_bit(R), LABEL)
#define DBF_COUNT(REG) ( ( (REG) & 0xFFFF ) + 1 )

static const int direkt_tab[] = {
  0x7fff000e,0x00ff0007,0x00070002,0x00030001,0x00030001,
  270-1,	15-1,	 8-1,	 5-1,	 2-1
};

static const u8 length_tab[] = {
  9,1,0,-1,-1,
  8,4,2,1,0
};

static const s8 more_offset[] = {
  11,	4,   7,	 0,
  0x01,0x1f, -1,-1, 0x00,0x1F
};

static const int more_offset_word[] = {
  0x0b04,0x0700,0x011f,-1,0x001f
};

static void strings(all_regs_t *);
static void normal_bytes(all_regs_t *);
static int get_d0_bits(all_regs_t *, int d0);

static inline int chk_dst_range(all_regs_t *R, const areg_t a, const areg_t b)
{
  R->overflow |= (a <  R->dstbuf) << 0;
  R->overflow |= (b >= R->dstend) << 1;
  return R->overflow;
}

static inline int chk_src_range(all_regs_t *R, const areg_t a, const areg_t b)
{
  R->overflow |= (a <  R->srcbuf) << 2;
  R->overflow |= (b >= R->srcend) << 3;
  return R->overflow;
}

static inline int getinfo(all_regs_t *R)
{
  const areg_t a0 = R->a0;
  const int r0 = (0[a0]<<24) + (1[a0]<<16) + (2[a0]<<8) + 3[a0];
  R->a0 = a0+4;
  return r0;
}

static inline int get_1_bit(all_regs_t *R)
{
  int r;

  r = (R->d7 & 255) << 1;
  B_CC(r & 255, bitfound);

  chk_src_range(R,R->a5-1,R->a5-1);

  r = (r>>8) + (*(--R->a5) << 1);
bitfound:
  R->d7 = (R->d7 & ~0xFF) | (r & 0xFF);
  return r >> 8;
}


/* ice_decrunch:
 *  a0 = Pointer to packed data
 *  a1 = Address to which the data is unpacked
 *
 * During depacking:
 *
 *  a5 : current packed data pointer (read from bottom to top)
 *  a6 : current depacked data pointer (written from bottom to top)
 *  a4 : depack buffer top, used to detect end of depacking.
 */

static int ice_decrunch(all_regs_t *R)
{
  int id;
  int csize;
  int dsize;

  R->srcbuf = R->a0;
  R->dstbuf = R->a1;

  id = getinfo(R);	  /* Works with 'Ice!' too */
  if ( ( id & ~0x202000 ) != ICE_MAGIC) {
    return -1;
  }
  csize = getinfo(R);
  R->srcend = R->a5 = R->a0 - 8 + csize;
  R->d0 = dsize = getinfo(R);
  R->a6 = R->a4 = R->a1;
  R->a6 += R->d0;
  R->dstend = R->a3 = R->a6;

  R->d7 = *(--R->a5);
  normal_bytes(R);

  R->a6 = R->a3;
  GET_1_BIT_BCC(not_packed);
  R->d7 = 0x0f9f;
  GET_1_BIT_BCC(ice_00);
  R->d7 = R->d1 = get_d0_bits(R, 15);

ice_00:
  R->d6 = 3;
ice_01:
  R->a3 -= 2;
  R->d4 = (R->a3[0]<<8) | R->a3[1];
  R->d5 = 3;
ice_02:
  R->d4 += R->d4;
  R->d0 += R->d0 + (R->d4>>16);
  R->d4 &= 0xFFFF;

  R->d4 += R->d4;
  R->d1 += R->d1 + (R->d4>>16);
  R->d4 &= 0xFFFF;

  R->d4 += R->d4;
  R->d2 += R->d2 + (R->d4>>16);
  R->d4 &= 0xFFFF;

  R->d4 += R->d4;
  R->d3 += R->d3 + (R->d4>>16);
  R->d4 &= 0xFFFF;

  DBF(R->d5,ice_02);
  DBF(R->d6,ice_01);

  if (chk_src_range(R, R->a3, R->a3+7)) {
    goto not_packed;
  }

  0[R->a3] = R->d0 >> 8;
  1[R->a3] = R->d0;

  2[R->a3] = R->d1 >> 8;
  3[R->a3] = R->d1;

  4[R->a3] = R->d2 >> 8;
  5[R->a3] = R->d2;

  6[R->a3] = R->d3 >> 8;
  7[R->a3] = R->d3;

  DBF(R->d7,ice_00);

not_packed:
  return -!!R->overflow;
}

static void normal_bytes(all_regs_t *R)
{
  while (1) {
    const int * tab;

    GET_1_BIT_BCC(test_if_end);
    R->d1 = 0;
    GET_1_BIT_BCC(copy_direkt);

    tab = direkt_tab + (20>>2);
    R->d3 = 4;
 nextgb:
    R->d0 = * (--tab);
    R->d1 = get_d0_bits(R, R->d0);
    R->d0 = (R->d0 >> 16) | ~0xFFFF;
    DB_CC((R->d0^R->d1)&0xFFFF, R->d3, nextgb);
    R->d1 += tab[(20>>2)];

 copy_direkt:
    {
      const int cnt = DBF_COUNT(R->d1);
      if (chk_dst_range(R, R->a6-cnt, R->a6-1) |
	  chk_src_range(R, R->a5-cnt, R->a5-1)) {
	break;
      }
    }
 lp_copy:
    *(--R->a6) = *(--R->a5);
    DBF(R->d1,lp_copy);

 test_if_end:
    if (R->a6 <= R->a4) {
      if (R->a6 < R->a4) {
	chk_dst_range(R, R->a6, R->a6);
      }
      break;
    }
    strings(R);
  }
}

static int get_d0_bits(all_regs_t *R, int r0)
{
  int r7 = R->d7;
  int r1 = 0;

  r0 &= 0xFFFF;
  if (r0 > 15) {
    R->overflow |= (1 << 4);
    return 0;
  }

hole_bit_loop:
  r7 = (r7 & 255) << 1;
  B_CC(r7 & 255, on_d0);

  chk_src_range(R,R->a5-1,R->a5-1);

  r7 = (*(--R->a5) << 1) + (r7>>8);
on_d0:
  r1 += r1 + (r7>>8);
  DBF(r0,hole_bit_loop);
  R->d7 = (R->d7 &~0xFF) | (r7 & 0xFF);
  R->d0 |= 0xFFFF;
  return r1;
}

static void strings(all_regs_t *R)
{
  R->a1 = (areg_t)length_tab;
  R->d2 = 3;
get_length_bit:
  DB_CC(get_1_bit(R)==0, R->d2, get_length_bit);
  R->d4 = R->d1 = 0;
  R->d0 = (R->d0 & ~0xFFFF) | (0xFFFF & (s8)R->a1[ 1 + (s16)R->d2 ]);
  B_CC(R->d0&0x8000, no_Ober);
  R->d1 = get_d0_bits(R, R->d0);
  R->d0 |= 0xFFFF;
no_Ober:
  R->d4 = R->a1 [ 6 + (s16)R->d2 ];
  R->d4 += R->d1;
  B_CC(R->d4==0,get_offset_2);
  R->a1 = (areg_t)more_offset;
  R->d2 = 1;
getoffs:
  DB_CC(get_1_bit(R)==0,R->d2,getoffs);

  R->d1 = get_d0_bits(R,(int)(s8)more_offset[1+(s16)R->d2]);
  R->d0 |= 0xFFFF;
  R->d1 += more_offset_word[3+(s16)R->d2];
  if (R->d1 < 0) {
    R->d1 -= R->d4;
  }
  goto depack_bytes;

get_offset_2:
  R->d1 = 0;
  R->d0 = 5;
  R->d2 = -1;
  GET_1_BIT_BCC(less_40);
  R->d0 = 8;
  R->d2 = 0x3f;
less_40:
  R->d1 = get_d0_bits(R, R->d0);
  R->d0 |= 0xFFFF;
  R->d1 += R->d2;

depack_bytes:
  R->a1 = R->a6 + 2 + (s16)R->d4 + (s16)R->d1;
  chk_dst_range(R, R->a6 - DBF_COUNT(R->d4) - 1, R->a6-1);
  if (R->a6>R->a4) *(--R->a6) = *(--R->a1);
dep_b:
  if (R->a6>R->a4) *(--R->a6) = *(--R->a1);
  DBF(R->d4,dep_b);
}

int ice_depacked_size(const void * buffer, int * p_csize)
{
  int id, csize, dsize;
  int csize_verif = p_csize ? *p_csize : 0;
  all_regs_t allregs;

  if (csize_verif && csize_verif<12) {
    return -1;
  }

  allregs.a0 = (areg_t)buffer;
  id = getinfo(&allregs);
  if ( ( id & ~0x202000 ) != ICE_MAGIC) {
    return -1;
  }
  csize = getinfo(&allregs);
  if (csize < 12) {
    return -2;
  }
  /* csize -= 12; */
  dsize = getinfo(&allregs);
  if (p_csize) {
    *p_csize = csize;
  }
  if (csize_verif && (csize != csize_verif)) {
    dsize = ~dsize;
  }
  return dsize;
}

int ice_depacker(void * dest, const void * src)
{
  all_regs_t allregs;

  allregs.d0 =
    allregs.d1 =
    allregs.d2 =
    allregs.d3 =
    allregs.d4 =
    allregs.d5 =
    allregs.d6 =
    allregs.d7 = 0;

  allregs.a2 =
    allregs.a3 =
    allregs.a4 =
    allregs.a5 =
    allregs.a6 =
    allregs.a7 = 0;

  allregs.a0 = (areg_t)src;
  allregs.a1 = dest;
  allregs.overflow = 0;

  return ice_decrunch(&allregs);
}
