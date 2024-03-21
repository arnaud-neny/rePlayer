#include <stdint.h>
#include "./nec/necintrf.h"
#include "ws_memory.h"
#include "ws_io.h"
#include "ws_audio.h"
#include "wsr_player.h"

//variables from ws_player.c
extern int SampleRate;
extern unsigned int ChannelMuting;

#define SNDP	ws_ioRam[0x80]
#define SNDV	ws_ioRam[0x88]
#define SNDSWP	ws_ioRam[0x8C]
#define SWPSTP	ws_ioRam[0x8D]
#define NSCTL	ws_ioRam[0x8E]
#define WAVDTP	ws_ioRam[0x8F]
#define SNDMOD	ws_ioRam[0x90]
#define SNDOUT	ws_ioRam[0x91]
#define PCSRL	ws_ioRam[0x92]
#define PCSRH	ws_ioRam[0x93]
#define DMASL	ws_ioRam[0x40]
#define DMASH	ws_ioRam[0x41]
#define DMASB	ws_ioRam[0x42]
#define DMADB	ws_ioRam[0x43]
#define DMADL	ws_ioRam[0x44]
#define DMADH	ws_ioRam[0x45]
#define DMACL	ws_ioRam[0x46]
#define DMACH	ws_ioRam[0x47]
#define DMACTL	ws_ioRam[0x48]
#define SDMASL	ws_ioRam[0x4A]
#define SDMASH	ws_ioRam[0x4B]
#define SDMASB	ws_ioRam[0x4C]
#define SDMACL	ws_ioRam[0x4E]
#define SDMACH	ws_ioRam[0x4F]
#define SDMACTL	ws_ioRam[0x52]

//SoundDMA �̓]���Ԋu
// ���ۂ̐��l��������Ȃ��̂ŁA�\�z�ł�
// �T���v�����O��������l���Ă݂Ĉȉ��̂悤�ɂ���
// 12KHz = 1.00HBlank = 256cycles�Ԋu
// 16KHz = 0.75HBlank = 192cycles�Ԋu
// 20KHz = 0.60HBlank = 154cycles�Ԋu
// 24KHz = 0.50HBlank = 128cycles�Ԋu
const int DMACycles[4] = { 256, 192, 154, 128 };

typedef struct
{
	int wave;
	int lvol;
	int rvol;
	long offset;
	long delta;
	long pos;
} WS_AUDIO;

static WS_AUDIO ws_audio[4];
static int SweepTime;
static int SweepStep;
static int SweepCount;
static int SweepFreq;
static int NoiseType;
static int NoiseRng;
static int MainVolume;
static int PCMVolumeLeft;
static int PCMVolumeRight;
unsigned long WaveAdrs;

void ws_audio_init(void)
{
}

void ws_audio_reset(void)
{
	memset(&ws_audio, 0, sizeof(ws_audio));
	SweepTime = 0;
	SweepStep = 0;
	NoiseType = 0;
	NoiseRng = 1;
	MainVolume = 0x04;
	PCMVolumeLeft = 0;
	PCMVolumeRight = 0;
}

void ws_audio_done(void)
{
}

void ws_audio_update(short *buffer, int length)
{
	int i, ch, cnt;
	long w, l, r;

	for (i=0; i<length; i++)
	{
		l = r = 0;

		for (ch=0; ch<4; ch++)
		{
			if ((ch==1) && (SNDMOD&0x20))
			{
				if (!(ChannelMuting & (1 << ch)))
				{
					// Voice�o��
					w = ws_ioRam[0x89];
					w -= 0x80;
					l += PCMVolumeLeft  * w;
					r += PCMVolumeRight * w;
				}
			}
			else if (SNDMOD&(1<<ch))
			{
				if ((ch==3) && (SNDMOD&0x80))
				{
					//Noise

					//OSWAN�̋[�������̏����Ɠ����̂���
#define BIT(n) (1<<n)
					const unsigned long noise_mask[8] =
					{
						BIT(0)|BIT(1),
						BIT(0)|BIT(1)|BIT(4)|BIT(5),
						BIT(0)|BIT(1)|BIT(3)|BIT(4),
						BIT(0)|BIT(1)|BIT(4)|BIT(6),
						BIT(0)|BIT(2),
						BIT(0)|BIT(3),
						BIT(0)|BIT(4),
						BIT(0)|BIT(2)|BIT(3)|BIT(4)
					};

					const unsigned long noise_bit[8] =
					{
						BIT(15),
						BIT(14),
						BIT(13),
						BIT(12),
						BIT(11),
						BIT(10),
						BIT(9),
						BIT(8)
					};

					int Masked, XorReg;

					ws_audio[ch].offset += ws_audio[ch].delta;
					cnt = ws_audio[ch].offset>>16;
					ws_audio[ch].offset &= 0xffff;
					while (cnt > 0)
					{
						cnt--;

						NoiseRng &= noise_bit[NoiseType]-1;
						if (!NoiseRng) NoiseRng = noise_bit[NoiseType]-1;

						Masked = NoiseRng & noise_mask[NoiseType];
						XorReg = 0;
						while (Masked)
						{
							XorReg ^= Masked&1;
							Masked >>= 1;
						}
						if (XorReg)
							NoiseRng |= noise_bit[NoiseType];
						NoiseRng >>= 1;
					}

					PCSRL = (uint8_t)(NoiseRng&0xff);
					PCSRH = (uint8_t)((NoiseRng>>8)&0x7f);

					if (!(ChannelMuting & (1 << ch)))
					{
						w = (NoiseRng & 1) ? 0x7f : -0x80;
						l += ws_audio[ch].lvol * w;
						r += ws_audio[ch].rvol * w;
					}
				}
				else
				{
					ws_audio[ch].offset += ws_audio[ch].delta;
					cnt = ws_audio[ch].offset>>16;
					ws_audio[ch].offset &= 0xffff;
					ws_audio[ch].pos += cnt;
					ws_audio[ch].pos &= 0x1f;
					w = ws_internalRam[(ws_audio[ch].wave&0xFFF0) + (ws_audio[ch].pos>>1)];
					if ((ws_audio[ch].pos&1) == 0)
						w = (w<<4)&0xf0;	//���ʃj�u��
					else
						w = w&0xf0;			//��ʃj�u��
					w -= 0x80;
					if (!(ChannelMuting & (1 << ch)))
					{
						l += ws_audio[ch].lvol * w;
						r += ws_audio[ch].rvol * w;
					}
				}
			}
		}

		l = l * MainVolume;
		if (l > 0x7fff) l = 0x7fff;
		else if (l < -0x8000) l = -0x8000;
		r = r * MainVolume;
		if (r > 0x7fff) r = 0x7fff;
		else if (r < -0x8000) r = -0x8000;

		*buffer++ = (short)l;
		*buffer++ = (short)r;
	}
}

void ws_audio_port_write(uint8_t port,uint8_t value)
{
	int i;
	long freq;

	Update_SampleData();

	ws_ioRam[port]=value;

	switch (port)
	{
	// 0x80-0x87�̎��g�����W�X�^�ɂ���
	// - ���b�N�}��&�t�H���e��0x0f�̋Ȃł́A���g��=0xFFFF �̉����s�v
	// - �f�W�����f�B�[�v���W�F�N�g��0x0d�̋Ȃ̃m�C�Y�� ���g��=0x07FF �ŉ����o��
	// ���܂�A0xFFFF �̎����������o���Ȃ����Ă��Ƃ��낤���B
	//   �ł��A0x07FF �̎��������o���Ȃ����ǁA�m�C�Y���������o���̂����B
	case 0x80:
	case 0x81:	i=(((unsigned int)ws_ioRam[0x81])<<8)+((unsigned int)ws_ioRam[0x80]);
				if (i == 0xffff)
					freq = 0;
				else
					freq = 3072000/(2048-(i&0x7ff));
				ws_audio[0].delta = (long)((float)freq*(float)65536/(float)SampleRate);
				break;
	case 0x82:
	case 0x83:	i=(((unsigned int)ws_ioRam[0x83])<<8)+((unsigned int)ws_ioRam[0x82]);
				if (i == 0xffff)
					freq = 0;
				else
					freq = 3072000/(2048-(i&0x7ff));
				ws_audio[1].delta = (long)((float)freq*(float)65536/(float)SampleRate);
				break;
	case 0x84:
	case 0x85:	i=(((unsigned int)ws_ioRam[0x85])<<8)+((unsigned int)ws_ioRam[0x84]);
				SweepFreq = i;
				if (i == 0xffff)
					freq = 0;
				else
					freq = 3072000/(2048-(i&0x7ff));
				ws_audio[2].delta = (long)((float)freq*(float)65536/(float)SampleRate);
				break;
	case 0x86:
	case 0x87:	i=(((unsigned int)ws_ioRam[0x87])<<8)+((unsigned int)ws_ioRam[0x86]);
				if (i == 0xffff)
					freq = 0;
				else
					freq = 3072000/(2048-(i&0x7ff));
				ws_audio[3].delta = (long)((float)freq*(float)65536/(float)SampleRate);
				break;
	case 0x88:
				ws_audio[0].lvol = (value>>4)&0xf;
				ws_audio[0].rvol = value&0xf;
				break;
	case 0x89:
				ws_audio[1].lvol = (value>>4)&0xf;
				ws_audio[1].rvol = value&0xf;
				break;
	case 0x8A:
				ws_audio[2].lvol = (value>>4)&0xf;
				ws_audio[2].rvol = value&0xf;
				break;
	case 0x8B:
				ws_audio[3].lvol = (value>>4)&0xf;
				ws_audio[3].rvol = value&0xf;
				break;
	case 0x8C:
				SweepStep = (signed char)value;
				break;
	case 0x8D:
				//Sweep�̊Ԋu�� 1/375[s] = 2.666..[ms]
				//CPU Clock�Ō����� 3072000/375 = 8192[cycles]
				//�����̐ݒ�l��n�Ƃ���ƁA8192[cycles]*(n+1) �Ԋu��Sweep���邱�ƂɂȂ�
				//
				//����� HBlank (256cycles) �̊Ԋu�Ō����ƁA
				//�@8192/256 = 32
				//�Ȃ̂ŁA32[HBlank]*(n+1) �Ԋu�ƂȂ�
				SweepTime = (((unsigned int)value)+1)<<5;
				SweepCount = SweepTime;
				break;
	case 0x8E:
				NoiseType = value&7;
				if (value&8) NoiseRng = 1;	//�m�C�Y�J�E���^�[���Z�b�g
				break;
	case 0x8F:
				WaveAdrs = (unsigned int)value<<6;
				ws_audio[0].wave = (unsigned int)value<<6;
				ws_audio[1].wave = ws_audio[0].wave + 0x10;
				ws_audio[2].wave = ws_audio[1].wave + 0x10;
				ws_audio[3].wave = ws_audio[2].wave + 0x10;
				break;
	case 0x90:
				break;
	case 0x91:
				//�����ł̃{�����[�������́A����Speaker�ɑ΂��Ă̒��������炵���̂ŁA
				//�w�b�h�t�H���ڑ�����Ă���ƔF��������Ζ�薳���炵���B
				ws_ioRam[port] |= 0x80;
				break;
	case 0x92:
	case 0x93:
				break;
	case 0x94:
				PCMVolumeLeft  = (value&0xc)*2;
				PCMVolumeRight = ((value<<2)&0xc)*2;
				break;
	case 0x52:
				if (value&0x80)
					ws_timer_set(2, DMACycles[value&3]);
				break;
	}
}

uint8_t ws_audio_port_read(uint8_t port)
{
	return (ws_ioRam[port]);
}

// HBlank�Ԋu�ŌĂ΂��
void ws_audio_process(void)
{
	long freq;

	if (SweepStep && (SNDMOD&0x40))
	{
		if (SweepCount < 0)
		{
			SweepCount = SweepTime;
			SweepFreq += SweepStep;
			SweepFreq &= 0x7FF;

			Update_SampleData();

			freq = 3072000/(2048-SweepFreq);
			ws_audio[2].delta = (long)((float)freq*(float)65536/(float)SampleRate);
		}
		SweepCount--;
	}
}

void ws_audio_sounddma(void)
{
	int i, j, b;

	if ((SDMACTL&0x88)==0x80)
	{
		i=(SDMACH<<8)|SDMACL;
		j=(SDMASB<<16)|(SDMASH<<8)|SDMASL;
		b=cpu_readmem20(j);

		Update_SampleData();

		ws_ioRam[0x89]=b;
		i--;
		j++;
		if(i<32)
		{
			i=0;
			SDMACTL&=0x7F;
		}
		else
		{
			ws_timer_set(2, DMACycles[SDMACTL&3]);
		}
		SDMASB=(uint8_t)((j>>16)&0xFF);
		SDMASH=(uint8_t)((j>>8)&0xFF);
		SDMASL=(uint8_t)(j&0xFF);
		SDMACH=(uint8_t)((i>>8)&0xFF);
		SDMACL=(uint8_t)(i&0xFF);
	}
}
