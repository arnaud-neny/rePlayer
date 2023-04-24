#ifndef KMZ80_H_
#define KMZ80_H_

/*
  KMZ80 common header
  by Mamiya
*/

#include "kmtypes.h"
#include "kmevent.h"
#ifdef __cplusplus
extern "C" {
#endif

enum {
	REGID_B,
	REGID_C,
	REGID_D,
	REGID_E,
	REGID_H,
	REGID_L,
	REGID_F,
	REGID_A,
	REGID_IXL,
	REGID_IXH,
	REGID_IYL,
	REGID_IYH,
	REGID_R,
	REGID_R7,
	REGID_I,
	REGID_IFF1,
	REGID_IFF2,
	REGID_IMODE,
	REGID_NMIREQ,
	REGID_INTREQ,
	REGID_HALTED,
	REGID_M1CYCLE,
	REGID_MEMCYCLE,
	REGID_IOCYCLE,
	REGID_STATE,
	REGID_FDMG,
	REGID_INTMASK,
	REGID_MAX,
	REGID_REGS8SIZE = ((REGID_MAX + (sizeof(int) - 1)) & ~(sizeof(int) - 1))
};

typedef struct KMZ80_CONTEXT_TAG KMZ80_CONTEXT;
struct KMZ80_CONTEXT_TAG {
	uint8_t regs8[REGID_REGS8SIZE];
	uint32_t sp;
	uint32_t pc;
	/* 裏レジスタ */
	uint32_t saf;
	uint32_t sbc;
	uint32_t sde;
	uint32_t shl;
	/* テンポラリフラグレジスタ(暗黙のキャリーフラグ) */
	uint32_t t_fl;
	/* テンポラリデーターレジスタ(暗黙の35フラグ) */
	uint32_t t_dx;
	/* ここまでは保存するべき */
	/* テンポラリプログラムカウンタ */
	uint32_t t_pc;
	/* テンポラリオペランドレジスタ */
	uint32_t t_op;
	/* テンポラリアドレスレジスタ */
	uint32_t t_ad;
	/* サイクルカウンタ */
	uint32_t cycle;
	/* オペコードテーブル */
	void *opt;
	/* オペコードCBテーブル */
	void *optcb;
	/* オペコードEDテーブル */
	void *opted;
	/* 追加サイクルテーブル */
	void *cyt;
	/* R800メモリーページ(ページブレイクの確認用) */
	uint32_t mempage;
	/* 特殊用途割り込みベクタ */
	uint32_t vector[5];
	/* RST飛び先基本アドレス */
	uint32_t rstbase;
	//---+ [changes_rough.txt]
	uint32_t playbase;
	uint32_t playflag;
	//---+
	/* 追加フラグ */
	/*   bit0: 暗黙のキャリー有効 */
	/*   bit1: 割り込み要求自動クリア */
	uint32_t exflag;
	/* 内部定義コールバック */
	uint32_t (*sysmemfetch)(KMZ80_CONTEXT *context);
	uint32_t (*sysmemread)(KMZ80_CONTEXT *context, uint32_t a);
	void (*sysmemwrite)(KMZ80_CONTEXT *context, uint32_t a, uint32_t d);
	/* ユーザーデーターポインタ */
	void *user;
	/* ユーザー定義コールバック */
	uint32_t (*memread)(void *u, uint32_t a);
	void (*memwrite)(void *u, uint32_t a, uint32_t d);
	uint32_t (*ioread)(void *u, uint32_t a);
	void (*iowrite)(void *u, uint32_t a, uint32_t d);
	uint32_t (*busread)(void *u, uint32_t mode);
	uint32_t (*checkbreak)(void *u, KMZ80_CONTEXT *context, void *obj);
	uint32_t (*patchedfe)(void *u, KMZ80_CONTEXT *context);
	void *object;
	/* ユーザー定義イベントタイマ */
	KMEVENT *kmevent;
};

PROTECTED void kmz80_reset(KMZ80_CONTEXT *context);
#if 0
PROTECTED void kmr800_reset(KMZ80_CONTEXT *context);
#endif
PROTECTED void kmdmg_reset(KMZ80_CONTEXT *context);
PROTECTED uint32_t kmz80_exec(KMZ80_CONTEXT *context, uint32_t cycle);

#ifdef __cplusplus
}
#endif
#endif
