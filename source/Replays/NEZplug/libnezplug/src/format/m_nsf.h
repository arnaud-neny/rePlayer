#ifndef M_NSF_H_
#define M_NSF_H_

#include "../include/nezplug/nezplug.h"
#include "../normalize.h"
#include "nsf6502.h"
#include "../device/kmsnddev.h"
#include "../cpu/km6502/km6502.h"

#ifdef __cplusplus
extern "C" {
#endif

struct NSF6502 {
	uint32_t cleft;	/* fixed point */
	uint32_t cps;		/* cycles per sample:fixed point */
	uint32_t cycles;
	uint32_t fcycles;
	uint32_t cpf[2];		/* cycles per frame */
	uint32_t total_cycles;
	uint32_t iframe;
	uint8_t  breaked;
	uint8_t  palntsc;
	uint8_t  rom[0x10];		/* nosefart rom */
};

typedef struct NSFNSF_TAG {
	NES_READ_HANDLER  *(nprh[0x10]);
	NES_WRITE_HANDLER *(npwh[0x10]);
	struct K6502_Context work6502;
	struct NSF6502 nsf6502;
	uint32_t work6502_BP;		/* Break Point */
	uint32_t work6502_start_cycles;
	uint8_t *bankbase;
	uint32_t banksw;
	uint32_t banknum;
	uint8_t head[0x80];		/* nsf header */
	uint8_t ram[0x800];
#if !NSF_MAPPER_STATIC
	uint8_t *bank[8];
	uint8_t static_area[0x8000 - 0x6000];
	uint8_t zero_area[0x1000];
#else
	uint8_t static_area[0x10000 - 0x6000];
#endif
	unsigned fds_type;
	int32_t apu_volume;
	int32_t dpcm_volume;
	void* apu;
	void* fdssound;
	void* n106s;
	void* vrc6s;
	void* sndp;
	void* mmc5;
	void* psgs;

	uint8_t counter2002;		/* 暫定 */
	int32_t dpcmirq_ct;
	uint8_t vsyncirq_fg;	/* $4015の6bit目を立たせるやつ */
    NES_READ_HANDLER nsf_mapper_read_handler[14];
    NES_WRITE_HANDLER nsf_mapper_write_handler[5];
    NES_WRITE_HANDLER nsf_mapper_write_handler_fds[13];
    NES_WRITE_HANDLER nsf_mapper_write_handler2[2];
    NEZ_NES_RESET_HANDLER nsf_mapper_reset_handler[2];
    NEZ_NES_TERMINATE_HANDLER nsf_mapper_terminate_handler[2];
} NSFNSF;

/* NSF player */
PROTECTED uint32_t NSFLoad(NEZ_PLAY*, const uint8_t *pData, uint32_t uSize);
PROTECTED uint32_t NSFELoad(NEZ_PLAY*, const uint8_t *pData, uint32_t uSize);
PROTECTED uint8_t *NSFGetHeader(NEZ_PLAY*);
PROTECTED uint32_t NSFDeviceInitialize(NEZ_PLAY*);

#ifdef __cplusplus
}
#endif

#endif /* M_NSF_H__ */
