#ifndef NSF6502_H__
#define NSF6502_H__

#include "../include/nezplug/nezplug.h"
#include "handler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*READHANDLER)(void *, uint32_t a);
typedef void (*WRITEHANDLER)(void *, uint32_t a, uint32_t v);

typedef struct NES_READ_HANDLER_TAG {
	uint32_t min;
	uint32_t max;
	READHANDLER Proc;
	struct NES_READ_HANDLER_TAG *next;
} NES_READ_HANDLER;

typedef struct NES_WRITE_HANDLER_TAG {
	uint32_t min;
	uint32_t max;
	WRITEHANDLER Proc;
	struct NES_WRITE_HANDLER_TAG *next;
} NES_WRITE_HANDLER;

PROTECTED void NESMemoryHandlerInitialize(NEZ_PLAY *);
PROTECTED void NESReadHandlerInstall(NEZ_PLAY *, const NES_READ_HANDLER *ph);
PROTECTED void NESWriteHandlerInstall(NEZ_PLAY *, const NES_WRITE_HANDLER *ph);

PROTECTED uint32_t NSF6502Install(NEZ_PLAY*);
#if 0
PROTECTED uint32_t NES6502GetCycles(NEZ_PLAY*);
#endif

PROTECTED void NES6502Irq(NEZ_PLAY*);
// PROTECTED void NES6502SetIrqCount(NEZ_PLAY *pNezPlay, int32_t A);
PROTECTED uint32_t NES6502ReadDma(NEZ_PLAY*, uint32_t A);

#if 0
PROTECTED uint32_t NES6502Read(NEZ_PLAY*, uint32_t A);
#endif

PROTECTED void NES6502Write(NEZ_PLAY*, uint32_t A, uint32_t V);


#ifdef __cplusplus
}
#endif

#endif /* NSF6502_H__ */
