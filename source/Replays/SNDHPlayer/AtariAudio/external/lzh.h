//-----------------------------------------------------------------------------
//	LZH depacking routine
//	Original LZH code by Haruhiko Okumura (1991) and Kerwin F. Medina (1996)
//	I changed to C++ object to remove global vars, so it should be thread safe now.
//-----------------------------------------------------------------------------
#pragma once
#include <stdint.h>

#define BUFSIZE (4*1024)

typedef uint8_t		uchar;
typedef unsigned int   uint;
typedef uint16_t	ushort;

#ifndef CHAR_BIT
#define CHAR_BIT            8
#endif

#ifndef UCHAR_MAX
#define UCHAR_MAX           255
#endif

typedef	uint16_t		BITBUFTYPE;

#define BITBUFSIZ (8*sizeof(BITBUFTYPE))
#define DICBIT    13                              /* 12(-lh4-) or 13(-lh5-) */
#define DICSIZ (1U << DICBIT)
#define MAXMATCH 256                              /* formerly F (not more than UCHAR_MAX + 1) */
#define THRESHOLD  3                              /* choose optimal value */
#define NC (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD) /* alphabet = {0, 1, 2, ..., NC - 1} */
#define CBIT 9                                    /* $\lfloor \log_2 NC \rfloor + 1$ */
#define CODE_BIT  16                              /* codeword length */

#define MAX_HASH_VAL (3 * DICSIZ + (DICSIZ / 512 + 1) * UCHAR_MAX)

#define NP (DICBIT + 1)
#define NT (CODE_BIT + 3)
#define PBIT 4      /* smallest integer such that (1U << PBIT) > NP */
#define TBIT 5      /* smallest integer such that (1U << TBIT) > NT */
#if NT > NP
#define NPT NT
#else
#define NPT NP
#endif

class LzhDepacker
{
public:

	static bool IsLzhPacked(const void* data, uint32_t size);

	void*	Unpack(const void* dataIn, uint32_t inSize, uint32_t& outSize);

private:
	bool	LzUnpack(void *pSrc,int srcSize,void *pDst,int dstSize);
	
	//----------------------------------------------
	// New stuff to handle memory IO
	//----------------------------------------------
	uint8_t*	m_pSrc;
	int	m_srcSize;
	uint8_t*	m_pDst;
	int	m_dstSize;

	int			DataIn(void *pBuffer,int nBytes);
	int			DataOut(void *pOut,int nBytes);


	//----------------------------------------------
	// Original Lzhxlib static func
	//----------------------------------------------
	void		fillbuf (int n);
	ushort		getbits (int n);
	void		init_getbits (void);
	int			make_table (int nchar, uchar *bitlen,int tablebits, ushort *table);
	void		read_pt_len (int nn, int nbit, int i_special);
	void		read_c_len (void);
	ushort		decode_c(void);
	ushort		decode_p(void);
	void		huf_decode_start (void);
	void		decode_start (void);
	void		decode (uint count, uchar buffer[]);


	//----------------------------------------------
	// Original Lzhxlib static vars
	//----------------------------------------------
	int			fillbufsize;
	uchar		buf[BUFSIZE];
	uchar		outbuf[DICSIZ];
	ushort		left [2 * NC - 1];
	ushort		right[2 * NC - 1];
	BITBUFTYPE	bitbuf;
	uint		subbitbuf;
	int			bitcount;
	int			decode_j;    /* remaining bytes to copy */
	uchar		c_len[NC];
	uchar		pt_len[NPT];
	uint		blocksize;
	ushort		c_table[4096];
	ushort		pt_table[256];
	int			with_error;

	uint		fillbuf_i;			// NOTE: these ones are not initialized at constructor time but inside the fillbuf and decode func.
	uint		decode_i;
};

