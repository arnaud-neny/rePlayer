#ifndef KMEVENT_H_
#define KMEVENT_H_

/*
  KMxxx event timer header
  by Mamiya
*/

#include "kmtypes.h"
#define KMEVENT_ITEM_MAX 31 /* MAX 255 */

typedef struct KMEVENT_TAG KMEVENT;
typedef struct KMEVENT_ITEM_TAG KMEVENT_ITEM;
typedef uint32_t KMEVENT_ITEM_ID;
struct KMEVENT_ITEM_TAG {
	/* メンバ直接アクセス禁止 */
	void *user;
	void (*proc)(KMEVENT *event, KMEVENT_ITEM_ID curid, void *user);
	uint32_t count;	/* イベント発生時間 */
	uint8_t prev;		/* 双方向リンクリスト */
	uint8_t next;		/* 双方向リンクリスト */
	uint8_t sysflag;	/* 内部状態フラグ */
	uint8_t flag2;	/* 未使用 */
};
struct KMEVENT_TAG {
	/* メンバ直接アクセス禁止 */
	KMEVENT_ITEM item[KMEVENT_ITEM_MAX + 1];
};

#ifdef __cplusplus
extern "C" {
#endif

PROTECTED void kmevent_init(KMEVENT *kme);
PROTECTED KMEVENT_ITEM_ID kmevent_alloc(KMEVENT *kme);
#if 0
PROTECTED void kmevent_free(KMEVENT *kme, KMEVENT_ITEM_ID curid);
#endif
PROTECTED void kmevent_settimer(KMEVENT *kme, KMEVENT_ITEM_ID curid, uint32_t time);
PROTECTED uint32_t kmevent_gettimer(KMEVENT *kme, KMEVENT_ITEM_ID curid, uint32_t *time);
PROTECTED void kmevent_setevent(KMEVENT *kme, KMEVENT_ITEM_ID curid, void (*proc)(), void *user);
PROTECTED void kmevent_process(KMEVENT *kme, uint32_t cycles);

#ifdef __cplusplus
}
#endif
#endif
