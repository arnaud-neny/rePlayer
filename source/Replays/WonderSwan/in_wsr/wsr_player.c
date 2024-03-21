#include <stdio.h>
#include <malloc.h>
#include "./nec/necintrf.h"
#include "../wsr_player_common/wsr_player_intf.h"
#include "wsr_player.h"
#include "ws_memory.h"
#include "ws_io.h"
#include "ws_audio.h"

short* sample_buffer = NULL;
int SampleRate = 44100;
const unsigned int WS_Cpu_Clock = 3072000;
unsigned int ChannelMuting = 0;

uint8_t *ROM = NULL;
int ROMSize;
int ROMBank;

void InitWSR(void)
{
	ws_memory_init(ROM, ROMSize);
	ws_io_init();
	ws_audio_init();
}

int LoadWSR(const void* pFile, unsigned Size)
{
	if (Size <= 0x20 || pFile == NULL) return 0;

	ROMSize = Size;
	ROMBank = (ROMSize + 0xFFFF) >> 16;

	if (ROM) { free(ROM); ROM = NULL; }
	ROM = (uint8_t*)malloc(ROMBank * 0x10000);
	if (!ROM) return 0;

	memcpy(ROM, pFile, ROMSize);
	if (ROM[ROMSize - 0x20] != 'W' || ROM[ROMSize - 0x20 + 1] != 'S' || ROM[ROMSize - 0x20 + 2] != 'R' || ROM[ROMSize - 0x20 + 3] != 'F')
	{
		if (ROM) free(ROM);
		ROM = NULL;
		return 0;
	}

	InitWSR();

	return 1;
}

int GetFirstSong(void)
{
	if (ROM == NULL || ROMSize < 0x20)
		return 0;

	return (unsigned)ROM[ROMSize-0x20+5];
}

unsigned SetFrequency(unsigned int Freq)
{
	if (Freq > 192000 || Freq < 11025)
		SampleRate = 44100;
	else
		SampleRate = Freq;

	return SampleRate;
}

void SetChannelMuting(unsigned int Mute)
{
	ChannelMuting = Mute;
}

unsigned int GetChannelMuting(void)
{
	return ChannelMuting;
}


void ResetWSR(unsigned SongNo)
{
	ws_memory_reset();
	ws_audio_reset();
	ws_io_reset();
	ws_timer_reset();
	nec_reset(NULL);
	nec_set_reg(NEC_SP, 0x2000);
	nec_set_reg(NEC_AW, SongNo);
}

void CloseWSR(void)
{
	if (ROM) free(ROM);
	ROM = NULL;
	ws_memory_done();
	ws_io_done();
	ws_audio_done();
}

int Sample_Enable = 0;
int Sample_Offset;
int Sample_Length;
int CPU_Count;
int CPU_Cycles;
int CPU_Run = 0;

void Init_SampleData(int Length)
{
	Sample_Offset = 0;
	Sample_Length = Length;
	Sample_Enable = 1;
}

void Close_SampleData(void)
{
	Update_SampleData();
	Sample_Enable = 0;
}

void Update_SampleData(void)
{
	int offset, length, cnt;

	if (Sample_Enable)
	{
		cnt = CPU_Count;
		if (CPU_Run)
			cnt += nec_getcycles();
		offset = Sample_Length * cnt / CPU_Cycles;
		if (offset >= Sample_Length)
			offset = Sample_Length;
		length = offset - Sample_Offset;

		if (length > 0)
		{
			ws_audio_update(sample_buffer + (Sample_Offset * 2), length);
			Sample_Offset += length;
		}
	}
}

int UpdateWSR(void* pBuf, unsigned Buflen, unsigned Samples)
{
	int i;

	sample_buffer = (short*)(pBuf);
	if (sample_buffer == NULL || Buflen < Samples * 2 * (16 / 8))
	{
		return 0;
	}

	int Cycles = MulDiv(Samples, WS_Cpu_Clock, SampleRate);
	CPU_Cycles = Cycles;
	CPU_Count = 0;
	Init_SampleData(Samples);

	while (CPU_Count < Cycles)
	{
		i = ws_timer_min(Cycles - CPU_Count);
		CPU_Run = 1;
		nec_execute(i);
		CPU_Run = 0;
		CPU_Count += i;
		ws_timer_count(i);
		ws_timer_update();
	}

	Close_SampleData();
	return 1;
}


#define TOTAL_TIMER 3
int ws_timer[TOTAL_TIMER];
int ws_timer_pending[TOTAL_TIMER];

void ws_timer_reset(void)
{
	int i;

	for (i=0; i<TOTAL_TIMER; i++)
	{
		ws_timer[i] = 0;
		ws_timer_pending[i] = 0;
	}

	ws_timer[0] = 256;		//HBlank Timer
	ws_timer[1] = 256*159;	//VBlank
	ws_timer[2] = 0;		//SoundDMA
}

void ws_timer_count(int Cycles)
{
	int i;

	for (i=0; i<TOTAL_TIMER; i++)
	{
		if (ws_timer[i] > 0)
		{
			ws_timer[i] -= Cycles;
			if (ws_timer[i] <= 0)
			{
				switch (i)
				{
				case 0: // HBlank Timer
					if ((ws_ioRam[0xb2]&0x80) && ws_ioRam[0xa4])
					{
						if (!ws_ioRam[0xa5])
							ws_ioRam[0xa5] = ws_ioRam[0xa4];
						if (ws_ioRam[0xa5])
							ws_ioRam[0xa5]--;
						if (!ws_ioRam[0xa5])
						{
							ws_ioRam[0xb6] |= 0x80;
							nec_int();
						}
					}
					ws_audio_process();
					ws_timer[0] += 256;
					break;
				case 1: // VBlank
					if (ws_ioRam[0xb2]&0x40)
					{
						ws_ioRam[0xb6] |= 0x40;
						nec_int();
					}
					ws_timer[1] += 256*159;
					break;
				case 2: // SoundDMA
					ws_audio_sounddma();
					break;
				}
			}
		}
	}
}

uint16_t cpu_interrupt(void)
{
	int i, f;

	f = ws_ioRam[0xb2] & ws_ioRam[0xb6];

	//���荞�݂̗D�揇�ʂ̍����� bit7,bit6,...,bit0 �̏��炵��
	for (i=7; i>=6; i--)	//���荞�݂͂܂��Q�����������ĂȂ��̂� bit7,bit6 �̂�
	{
		if (f & (1<<i))
		{
			return ((ws_ioRam[0xb0]+i)*4);
		}
	}
	return 0xFFFF;
}

void ws_timer_set(int no, int timer)
{
	if (CPU_Run)
	{
		ws_timer_pending[no] = timer;
		nec_yield();
	}
	else
	{
		ws_timer[no] = timer;
	}
}

void ws_timer_update(void)
{
	int i;

	for (i=0; i<TOTAL_TIMER; i++)
	{
		if (ws_timer_pending[i] > 0)
		{
			ws_timer[i] = ws_timer_pending[i];
			ws_timer_pending[i] = 0;
		}
	}
}

int ws_timer_min(int Cycles)
{
	int i;
	int timer = Cycles;

	for (i=0; i<TOTAL_TIMER; i++)
	{
		if ((ws_timer[i]>0) && (ws_timer[i]<timer))
			timer = ws_timer[i];
	}

	return (timer);
}

WSRPlayerApi g_wsr_player_api =
{
	LoadWSR,
	GetFirstSong,
	SetFrequency,
	SetChannelMuting,
	GetChannelMuting,
	ResetWSR,
	CloseWSR,
	UpdateWSR
};