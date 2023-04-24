#define BUILD_M6502 0
#define BUILD_M65C02 1
#define BUILD_HUC6280 0
#define BUILD_2A03 0

#include "km65c02.h"

#define C_FLAG K65C02_C_FLAG
#define Z_FLAG K65C02_Z_FLAG
#define I_FLAG K65C02_I_FLAG
#define D_FLAG K65C02_D_FLAG
#define B_FLAG K65C02_B_FLAG
#define R_FLAG K65C02_R_FLAG
#define V_FLAG K65C02_V_FLAG
#define N_FLAG K65C02_N_FLAG
#define T_FLAG 0

#define BASE_OF_ZERO 0x0000

#define VEC_RESET	0xFFFC
#define VEC_NMI		0xFFFA
#define VEC_INT		0xFFFE

#define VEC_BRK		VEC_INT

#define IRQ_INIT	K65C02_INIT
#define IRQ_RESET	K65C02_RESET
#define IRQ_NMI		K65C02_NMI
#define IRQ_BRK		K65C02_BRK
#define IRQ_INT		K65C02_INT

#define __THIS__	(*pc)
#define __CONTEXT	struct K65C02_Context *pc
#define __CONTEXT_	struct K65C02_Context *pc,
#define __THISP		pc
#define __THISP_	pc,

#define K_EXEC		K65C02_Exec

#if USE_USERPOINTER
#define __THIS_USER_ __THIS__.user,
#else
#define __THIS_USER_
#endif

#if USE_CALLBACK
#if USE_INLINEMMC
static Inline Uword K_READ(__CONTEXT_ Uword adr)
{
	return __THIS__.ReadByte[adr >> USE_INLINEMMC](__THIS_USER_ adr);
}
static Inline void K_WRITE(__CONTEXT_ Uword adr, Uword value)
{
	__THIS__.WriteByte[adr >> USE_INLINEMMC](__THIS_USER_ adr, value);
}
#else
static Inline Uword K_READ(__CONTEXT_ Uword adr)
{
	return __THIS__.ReadByte(__THIS_USER_ adr);
}
static Inline void K_WRITE(__CONTEXT_ Uword adr, Uword value)
{
	__THIS__.WriteByte(__THIS_USER_ adr, value);
}
#endif
#else
static Inline Uword K_READ(__CONTEXT_ Uword adr)
{
	return K65C02_ReadByte(__THIS_USER_ adr);
}
static Inline void K_WRITE(__CONTEXT_ Uword adr, Uword value)
{
	K65C02_WriteByte(__THIS_USER_ adr, value);
}
#endif
#ifndef K_READNP
#define K_READNP K_READ
#define K_WRITENP K_WRITE
#endif
#ifndef K_READZP
#if !USE_DIRECT_ZEROPAGE
#define K_READZP K_READ
#define K_WRITEZP K_WRITE
#else
static Inline Uword K_READZP(__CONTEXT_ Uword adr)
{
	return __THIS__.zeropage[adr];
}
static Inline void K_WRITEZP(__CONTEXT_ Uword adr, Uword value)
{
	__THIS__.zeropage[adr] = value;
}
#endif
#endif

#include "km6502ft.h"
#include "km6502cd.h"
#include "km6502ct.h"
#include "km6502ot.h"
#include "km6502ex.h"
