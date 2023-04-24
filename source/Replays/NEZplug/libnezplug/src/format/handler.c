#include "handler.h"
#include "../normalize.h"

/* --------------- */
/*  Reset Handler  */
/* --------------- */

PROTECTED void NESReset(NEZ_PLAY *pNezPlay)
{
	NEZ_NES_RESET_HANDLER *ph;
	uint32_t prio;
	if (!pNezPlay) return;
	for (prio = 0; prio < 0x10; prio++)
		for (ph = pNezPlay->nrh[prio]; ph; ph = ph->next) ph->Proc(pNezPlay);
}

static void InstallPriorityResetHandler(NEZ_NES_RESET_HANDLER **nrh, const NEZ_NES_RESET_HANDLER *ph)
{
	NEZ_NES_RESET_HANDLER *nh;
	uint32_t prio = ph->priority;
	if (prio > 0xF) prio = 0xF;

	/* Add to tail of list*/
	nh = XMALLOC(sizeof(NEZ_NES_RESET_HANDLER));
	nh->priority = prio;
	nh->Proc = ph->Proc;
	nh->next = 0;

	if (nrh[prio])
	{
		NEZ_NES_RESET_HANDLER *p = nrh[prio];
		while (p->next) p = p->next;
		p->next = nh;
	}
	else
	{
		nrh[prio] = nh;
	}
}

PROTECTED void NESResetHandlerInstall(NEZ_NES_RESET_HANDLER** nrh, const NEZ_NES_RESET_HANDLER *ph)
{
	for (; ph->Proc; ph++) InstallPriorityResetHandler(nrh, ph);
}

static void NESResetHandlerInitialize(NEZ_NES_RESET_HANDLER **nrh)
{
	uint32_t prio;
	for (prio = 0; prio < 0x10; prio++) nrh[prio] = 0;
}
static void NESResetHandlerTerminate(NEZ_NES_RESET_HANDLER **nrh)
{
	uint32_t prio;
	NEZ_NES_RESET_HANDLER *next, *p;
	for (prio = 0; prio < 0x10; prio++) {
		if (nrh[prio]) {
			if (nrh[prio]->next) {
				p = nrh[prio]->next;
				while (p) {
					next = p->next;
					XFREE(p);
					p = next;
				}
			}
			XFREE(nrh[prio]);
		}
	}
}


/* ------------------- */
/*  Terminate Handler  */
/* ------------------- */
PROTECTED void NESTerminate(NEZ_PLAY *pNezPlay)
{
	NEZ_NES_TERMINATE_HANDLER *ph;
	if (!pNezPlay) return;
	for (ph = pNezPlay->nth; ph; ph = ph->next) ph->Proc(pNezPlay);
	NESHandlerTerminate(((NEZ_PLAY*)pNezPlay)->nrh, ((NEZ_PLAY*)pNezPlay)->nth);
}

PROTECTED void NESTerminateHandlerInstall(NEZ_NES_TERMINATE_HANDLER **nth, const NEZ_NES_TERMINATE_HANDLER *ph)
{
	/* Add to head of list*/
	NEZ_NES_TERMINATE_HANDLER *nh;
	nh = XMALLOC(sizeof(NEZ_NES_TERMINATE_HANDLER));
	nh->Proc = ph->Proc;
	nh->next = *nth;
	*nth = nh;
}

static void NESTerminateHandlerInitialize(NEZ_NES_TERMINATE_HANDLER **nth)
{
	*nth = 0;
}

static void NESTerminateHandlerTerminate(NEZ_NES_TERMINATE_HANDLER **nth)
{
	NEZ_NES_TERMINATE_HANDLER *p = *nth, *next;
	while (p) {
		next = p->next;
		XFREE(p);
		p = next;
	}
}

PROTECTED void NESHandlerInitialize(NEZ_NES_RESET_HANDLER** nrh, NEZ_NES_TERMINATE_HANDLER* nth)
{
	NESResetHandlerInitialize(nrh);
	NESTerminateHandlerInitialize(&nth);
}

PROTECTED void NESHandlerTerminate(NEZ_NES_RESET_HANDLER** nrh, NEZ_NES_TERMINATE_HANDLER* nth)
{
	NESResetHandlerTerminate(nrh);
	NESTerminateHandlerTerminate(&nth);
}
