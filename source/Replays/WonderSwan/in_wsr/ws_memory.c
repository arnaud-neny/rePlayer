#include <malloc.h>
#include <stdint.h>
#include "ws_io.h"
#include "ws_audio.h"
#include "wsr_player.h"

uint8_t	*ws_rom;
uint8_t	*ws_internalRam = NULL;
uint8_t	*ws_staticRam = NULL;
uint32_t	romAddressMask;
uint32_t	romSize;
uint32_t	baseBank;

void ws_memory_init(uint8_t *rom, uint32_t wsRomSize)
{
	ws_rom = rom;
	romSize = wsRomSize;
	romAddressMask = romSize-1;
	baseBank = 0x100 - (romSize>>16);
	if (ws_internalRam == NULL)
		ws_internalRam = (uint8_t*)malloc(0x10000);
	if (ws_staticRam == NULL)
		ws_staticRam = (uint8_t*)malloc(0x10000);
}

void ws_memory_reset(void)
{
	memset(ws_internalRam, 0, 0x10000);
	memset(ws_staticRam, 0, 0x10000);
}

void ws_memory_done(void)
{
	if (ws_internalRam)
		free(ws_internalRam);
	ws_internalRam = NULL;
	if (ws_staticRam)
		free(ws_staticRam);
	ws_staticRam = NULL;
}

uint8_t cpu_readmem20(uint32_t addr)
{
	uint32_t	offset = addr&0xffff;
	uint32_t	bank = (addr>>16)&0xf;
	uint32_t romBank;

	switch (bank)
	{
	case 0:		// 0 - RAM - 16 KB (WS) / 64 KB (WSC) internal RAM
				return ws_internalRam[offset];
	case 1:		// 1 - SRAM (cart)
				return ws_staticRam[offset];
	case 2:
	case 3:
				romBank = ws_ioRam[0xC0+bank];
				if (romBank < baseBank)
					return 0xff;
				else
					return ws_rom[(unsigned)(offset + ((romBank-baseBank)<<16))];
	default:
				romBank = ((ws_ioRam[0xC0]&0xf)<<4)|(bank&0xf);
				if (romBank < baseBank)
					return 0xff;
				else
					return ws_rom[(unsigned)(offset + ((romBank-baseBank)<<16))];
	}
	return (0xff);
}

void cpu_writemem20(uint32_t addr,uint8_t value)
{
	uint32_t	offset = addr&0xffff;
	uint32_t	bank = (addr>>16)&0xf;

	switch (bank)
	{
	case 0:		// 0 - RAM - 16 KB (WS) / 64 KB (WSC) internal RAM

				//�M���e�B�M�A�v�`2�̃T���v�����O�h�����́APCMVoice���g�킸
				//�g�`��������HBlank�Ԋu�ōX�V�����Ă����Ă�
				//���̑΍�Ƃ��āA�g�`���������X�V�����ꍇ�́A�o�͔g�`���X�V������
				if (WaveAdrs <= offset && offset < (WaveAdrs+0x40))
					Update_SampleData();

				ws_internalRam[offset] = value;
				break;
	case 1:		// 1 - SRAM (cart)
				ws_staticRam[offset] = value;
				break;
				// other banks are read-only
	}
}
