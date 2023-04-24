/*
  KMxxx event timer
  by Mamiya
*/

#include "kmevent.h"

enum {
	KMEVENT_FLAG_BREAKED = (1 << 0),
	KMEVENT_FLAG_DISPATCHED = (1 << 1),
	KMEVENT_FLAG_ALLOCED = (1 << 7)
};

PROTECTED void kmevent_reset(KMEVENT *kme)
{
	KMEVENT_ITEM_ID id;
	kme->item[0].count = 0;
	for (id = 0; id <= KMEVENT_ITEM_MAX; id++)
	{
		kme->item[id].sysflag &= ~KMEVENT_FLAG_ALLOCED;
		kme->item[id].count = 0;
		kme->item[id].next = id;
		kme->item[id].prev = id;
	}
}

PROTECTED void kmevent_init(KMEVENT *kme)
{
	KMEVENT_ITEM_ID id;
	for (id = 0; id <= KMEVENT_ITEM_MAX; id++)
	{
		kme->item[id].sysflag = 0;
	}
	kmevent_reset(kme);
}

PROTECTED KMEVENT_ITEM_ID kmevent_alloc(KMEVENT *kme)
{
	KMEVENT_ITEM_ID id;
	for (id = 1; id <= KMEVENT_ITEM_MAX; id++)
	{
		if (kme->item[id].sysflag == 0)
		{
			kme->item[id].sysflag = KMEVENT_FLAG_ALLOCED;
			return id;
		}
	}
	return 0;
}

/* リストから取り外す */
static void kmevent_itemunlist(KMEVENT *kme, KMEVENT_ITEM_ID curid)
{
	KMEVENT_ITEM *cur, *next, *prev;
	cur = &kme->item[curid];
	next = &kme->item[cur->next];
	prev = &kme->item[cur->prev];
	next->prev = cur->prev;
	prev->next = cur->next;
}

/* リストの指定位置(baseid)の直前に挿入 */
static void kmevent_itemlist(KMEVENT *kme, KMEVENT_ITEM_ID curid, KMEVENT_ITEM_ID baseid)
{
	KMEVENT_ITEM *cur, *next, *prev;
	cur = &kme->item[curid];
	next = &kme->item[baseid];
	prev = &kme->item[next->prev];
	cur->next = baseid;
	cur->prev = next->prev;
	prev->next = curid;
	next->prev = curid;
}

/* ソート済リストに挿入 */
static void kmevent_iteminsert(KMEVENT *kme, KMEVENT_ITEM_ID curid)
{
	KMEVENT_ITEM_ID baseid;
	for (baseid = kme->item[0].next; baseid; baseid = kme->item[baseid].next)
	{
		if (kme->item[baseid].count)
		{
			if (kme->item[baseid].count > kme->item[curid].count) break;
		}
	}
	kmevent_itemlist(kme, curid, baseid);
}

#if 0
/* it looks like this is never called ? */
PROTECTED void kmevent_free(KMEVENT *kme, KMEVENT_ITEM_ID curid)
{
	kmevent_itemunlist(kme, curid);
	kme->item[curid].sysflag = 0;
}
#endif

PROTECTED void kmevent_settimer(KMEVENT *kme, KMEVENT_ITEM_ID curid, uint32_t time)
{
	kmevent_itemunlist(kme, curid);	/* 取り外し */
	kme->item[curid].count = time ? kme->item[0].count + time : 0;
	if (kme->item[curid].count) kmevent_iteminsert(kme, curid);	/* ソート */
}

PROTECTED uint32_t kmevent_gettimer(KMEVENT *kme, KMEVENT_ITEM_ID curid, uint32_t *time)
{
	uint32_t nextcount;
	nextcount = kme->item[curid ? curid : kme->item[0].next].count;
	if (!nextcount) return 0;
	nextcount -= kme->item[0].count;
	if (time) *time = nextcount;
	return 1;
}

PROTECTED void kmevent_setevent(KMEVENT *kme, KMEVENT_ITEM_ID curid, void (*proc)(), void *user)
{
	kme->item[curid].proc = (void (*)(KMEVENT *,uint32_t ,void *))proc;
	kme->item[curid].user = user;
}

/* 指定サイクル分実行 */
PROTECTED void kmevent_process(KMEVENT *kme, uint32_t cycles)
{
	KMEVENT_ITEM_ID id;
	uint32_t nextcount;
	kme->item[0].count += cycles;
	if (kme->item[0].next == 0)
	{
		/* リストが空なら終わり */
		kme->item[0].count = 0;
		return;
	}
	nextcount = kme->item[kme->item[0].next].count;
	while (nextcount && kme->item[0].count >= nextcount)
	{
		/* イベント発生済フラグのリセット */
		for (id = kme->item[0].next; id; id = kme->item[id].next)
		{
			kme->item[id].sysflag &= ~(KMEVENT_FLAG_BREAKED + KMEVENT_FLAG_DISPATCHED);
		}
		/* nextcount分進行 */
		kme->item[0].count -= nextcount;
		for (id = kme->item[0].next; id; id = kme->item[id].next)
		{
			if (!kme->item[id].count) continue;
			kme->item[id].count -= nextcount;
			if (kme->item[id].count) continue;
			/* イベント発生フラグのセット */
			kme->item[id].sysflag |= KMEVENT_FLAG_BREAKED;
		}
		for (id = kme->item[0].next; id; id = kme->item[id].next)
		{
			/* イベント発生済フラグの確認 */
			if (kme->item[id].sysflag & KMEVENT_FLAG_DISPATCHED) continue;
			kme->item[id].sysflag |= KMEVENT_FLAG_DISPATCHED;
			/* イベント発生フラグの確認 */
			if (!(kme->item[id].sysflag & KMEVENT_FLAG_BREAKED)) continue;
			/* 対象イベント起動 */
			kme->item[id].proc(kme, id, kme->item[id].user);
			/* 先頭から再走査 */
			id = 0;
		}
		nextcount = kme->item[kme->item[0].next].count;
	}
}

