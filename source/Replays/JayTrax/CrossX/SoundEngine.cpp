#include "SoundEngine.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef __SYMBIAN32__
#include <memory.h>
#include <string.h>
#endif

#define DllExport   __declspec( dllexport )

//////////////////////////
//INTERNAL DEFINES
//////////////////////////

#define SW(a)  (((a&0xff)<<8)+((a&0xff00)>>8))
#define SL(a)  (((a&0xff)<<24)+((a&0xff00)<<8)+((a&0xff0000)>>8)+((a&0xff000000)>>24))

#define M_PI (3.14159265359)


//////////////////////////
//MACROS
//////////////////////////


#ifdef _WIN32_WCE

//#define LOG(x)  { \
//	  FILE *log; \
//	  log = fopen("\\Program Files\\Syntrax\\log.txt","a"); \
//	  if(log) \
//{ \
//		  fwrite((x), strlen(x), 1, log); \
//		  fclose(log); \
//		  fflush(log); \
//} \
//}
#endif

#ifdef __SYMBIAN32__

#define LOG(x)  { \
	  FILE *log; \
	  log = fopen("c:\\system\\apps\\syntrax\\log.txt","a"); \
	  if(log) \
{ \
		  fwrite((x), strlen(x), 1, log); \
		  fclose(log); \
		  fflush(log); \
} \
}
#endif


//////////////////////////
//CODE
//////////////////////////

inline void* Malloc(size_t s)
{
	void* m = malloc(s);
	if (m)
		memset(m, 0, s);
	return m;
}

SoundEngine::SoundEngine()
{
	int i;
	m_DefaultInst = 0;
	m_EmptyWave = 0;
	m_Presets = 0;
	m_Arpeggios = 0;
	m_CopyPatternBuf = 0;
	m_FrequencyTable = 0;
	m_LeftDelayBuffer = 0;
	m_RightDelayBuffer = 0;
	for(i=0 ;i<SE_MAXCHANS; i++)
	{
		m_ChannelData[i] = 0;
	}
}

SoundEngine::~SoundEngine()
{
	DeAllocate();
}


int SoundEngine::Allocate()
{
	int i;
	m_OverlapCnt = 0;
	for(i=0 ;i<SE_MAXCHANS; i++)
	{
		m_MuteTab[i] = 0;
	}

	m_EmptyWave = new short [256];
	if(!m_EmptyWave)
	{
		DeAllocate();
		return 0;
	}
	memset(m_EmptyWave, 0, sizeof(short) * 256);
	m_Presets = new short[4096];
	if(!m_Presets)
	{
		DeAllocate();
		return 0;
	}
	memset(m_Presets, 0, sizeof(short) * 4096);
	m_Arpeggios = new char[256];
	if(!m_Arpeggios)
	{
		DeAllocate();
		return 0;
	}
	memset(m_Arpeggios, 0, sizeof(char) * 256);

	for(i=0 ;i<SE_MAXCHANS; i++)
	{
		m_ChannelData[i] = new chandat();
		if(!m_ChannelData[i])
		{
			DeAllocate();
			return 0;
		}
	}
	

	m_PatternOffset = 0;	 // offset in de pattern (voor de display)
	m_PlayPosition = 0;	 // waar is de song nu?
	m_PlayStep = 0;
	m_DisplayChannel = 0; // last active channel (to display)

	m_PatternLength = 0;	// when playing a pattern it loops at this length

	m_CurrentPattern = 0;
	m_CurrentSubsong = 0; 
	m_PlayFlg = 0; 
	m_PauseFlg = 0;

	m_PatternDelay = 0;
	m_PlaySpeed = 0;
	m_PlayMode = SE_PM_SONG;
	m_MasterVolume = 256;
	m_NrOfChannels = 0;

	m_Songs = 0;
	m_Patterns = 0;
	m_PatternNames = 0;
	m_Instruments = 0;

	for(i=0;i<SE_MAXCHANS;i++)
	{
		m_WavePosition[i]=0; //init position of channelrenderer in waveform
	}

	//initialize renderingcounters and speed
	m_TimeCnt = 2200;
	m_TimeSpd = 2200;

	// intialize a default instrument
	inst copyinst = {1235,"Empty",0,256,255,2,1,255,0,3,1,0,0,0,{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},4,1,0,0,0,0,0,0,0,0,0,0,0};;
	m_DefaultInst = new inst();
	if(!m_DefaultInst)
	{
		DeAllocate();
		return 0;
	}
	*m_DefaultInst = copyinst;

	// intialize a default pattern
	m_CopyPatternBuf = new patitem[64];	// buffer om pattern in te copieren
	if(!m_CopyPatternBuf)
	{
		DeAllocate();
		return 0;
	}
	for(i=0; i<64; i++)
	{
		m_CopyPatternBuf[i].dstnote = 0;
		m_CopyPatternBuf[i].srcnote = 0;
		m_CopyPatternBuf[i].inst = 0;
		m_CopyPatternBuf[i].param = 0;
		m_CopyPatternBuf[i].script = 0;
	}

	// Allocate and precalculate finetune buffers
	m_FrequencyTable = new int *[SE_NROFFINETUNESTEPS];
	if(!m_FrequencyTable)
	{
		DeAllocate();
		return 0;
	}
	for(i=0; i<SE_NROFFINETUNESTEPS; i++)
	{
		m_FrequencyTable[i] = 0;
	}
	for(i=0; i<SE_NROFFINETUNESTEPS; i++)
	{
		m_FrequencyTable[i] = new int[128];
		if(!m_FrequencyTable[i])
		{
			DeAllocate();
			return 0;
		}
	}
	PreCalcFinetune();

	// Allocate delay buffers
	m_LeftDelayBuffer = new short[65536];
	if(!m_LeftDelayBuffer)
	{
		DeAllocate();
		return 0;
	}
	m_RightDelayBuffer = new short[65536];
	if(!m_RightDelayBuffer)
	{
		DeAllocate();
		return 0;
	}
	ClearSoundBuffers();
	m_DelayCnt=0;
	return 1;
}

void SoundEngine::DeAllocate()
{
	int i;

	delete m_DefaultInst;
	delete [] m_CopyPatternBuf;
	delete [] m_Arpeggios;
	delete [] m_EmptyWave;
	delete [] m_Presets;
	delete [] m_LeftDelayBuffer;
	delete [] m_RightDelayBuffer;
	if(m_FrequencyTable)
	{
		for(i=0; i<SE_NROFFINETUNESTEPS; i++)
		{
			delete m_FrequencyTable[i];
		}
		delete m_FrequencyTable;
	}

	for(i=0 ;i<SE_MAXCHANS; i++)
	{
		delete m_ChannelData[i];
		m_ChannelData[i] = 0;
	}

	m_DefaultInst = 0;
	m_EmptyWave = 0;
	m_Presets = 0;
	m_Arpeggios = 0;
	m_CopyPatternBuf = 0;
	m_FrequencyTable = 0;
	m_LeftDelayBuffer = 0;
	m_RightDelayBuffer = 0;
}

void SoundEngine::ClearSoundBuffers()
{
	int i,j;

	// clear delaybuffers
	if(m_LeftDelayBuffer) memset(m_LeftDelayBuffer,0,65536*sizeof(short));
	if(m_RightDelayBuffer) memset(m_RightDelayBuffer,0,65536*sizeof(short));

	//initialize channel data
	for (i=0;i<SE_MAXCHANS;i++)
	{
		m_ChannelData[i]->songpos = 0;
		m_ChannelData[i]->patpos = 0;
		m_ChannelData[i]->instrument = -1;
		m_ChannelData[i]->volcnt = 0;
		m_ChannelData[i]->arpcnt = 0;
		m_ChannelData[i]->pancnt = 0;
		m_ChannelData[i]->curnote = 0;
		m_ChannelData[i]->curfreq = 0;
		m_ChannelData[i]->bendadd = 0;
		m_ChannelData[i]->destfreq = 0;
		m_ChannelData[i]->bendspd = 0;
		m_ChannelData[i]->freqcnt = 0;
		m_ChannelData[i]->freqdel = 0;
		m_ChannelData[i]->sampledata = 0;
		m_ChannelData[i]->endpoint = 0;
		m_ChannelData[i]->samplepos = 0;
		m_ChannelData[i]->lastplaypos = 0;
		m_ChannelData[i]->curvol = 0;
		m_ChannelData[i]->curpan = 0;
		m_ChannelData[i]->bendtonote = 0;
		m_ChannelData[i]->looppoint = 0;
		m_ChannelData[i]->loopflg = 0;
		m_ChannelData[i]->bidirecflg = 0;
		m_ChannelData[i]->curdirecflg = 0;
		for(j=0;j<4;j++)
		{
			m_ChannelData[i]->fx[j].fxcnt1 = 0;
			m_ChannelData[i]->fx[j].fxcnt2 = 0;
			m_ChannelData[i]->fx[j].osccnt = 0;
			m_ChannelData[i]->fx[j].a0 = 0;
			m_ChannelData[i]->fx[j].b1 = 0;
			m_ChannelData[i]->fx[j].b2 = 0;
			m_ChannelData[i]->fx[j].y1 = 0;
			m_ChannelData[i]->fx[j].y2 = 0;
			m_ChannelData[i]->fx[j].Vhp = 0;
			m_ChannelData[i]->fx[j].Vbp = 0;
			m_ChannelData[i]->fx[j].Vlp = 0;
		}
		memset(m_ChannelData[i]->waves,0,8192);
	}

	m_HasLooped = 0;
}


void SoundEngine::AdvanceSong(void)
{
	int i;
	HandleSong();
	for (i=0; i< m_NrOfChannels; i++)
	{
		HandlePattern(i);
		if(m_ChannelData[i]->instrument!=-1) // muten?
		{
			HandleInstrument(i);    // Do volume and pitch things
			HandleEffects(i); // do effect on channel 1
		}
	}
}


void SoundEngine::HandleEffects(int channr)
{
	int f;
	for (f=0; f<4; f++) // 4 effects per instrument
	{
		short s;
		// oscillator ophogen
		s = (short) m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscspd;
		m_ChannelData[channr]->fx[f].osccnt += s;
		m_ChannelData[channr]->fx[f].osccnt &= 255;

		switch (m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effecttype)
		{
		case 0: //none
			break;
		case 1: //negate
			{
				int dest;
				short *dw;
				short i,s,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				s = (short)m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectspd;
				c = (short)m_ChannelData[channr]->fx[f].fxcnt1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				for (i=0; i<s;i++)
				{
					c++;
					c&=255;
					dw[c] = 0-dw[c];
				}
				m_ChannelData[channr]->fx[f].fxcnt1 = (int)c;
			}
			break;
		case 2:  // warp
			{
				int dest;
				short *dw;
				short i,s,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				s = (short) m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectspd;
				dw = &m_ChannelData[channr]->waves[256*dest];
				c = 0;
				for (i=0; i<256; i++)
				{
					dw[i] += c;
					c+=s;
				}
			}
			break;
		case 3:  // Filter
			{
				int dest,src;
				short *dw,*sw;
				short i,c,s,t;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw = &m_ChannelData[channr]->waves[256*src];
				s = (short) m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectspd;
				c = 0;
				if(s>12) s=12; //not more than 12 times...it slowes down too much
				for (t=0; t<s; t++)
				{
					dw[0] = (sw[255] +sw[1])>>1;
					for (i=1; i<255; i++)
					{
						dw[i] = (sw[i-1] +sw[i+1])>>1;
					}
					dw[255] = (sw[254] +sw[0])>>1;
				}
			}
			break;
		case 4:  // Wavemix
			{
				int dest,src1,src2;
				short *dw,*sw1,*sw2;
				short i,s,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect2;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];
				s = (short) m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectspd;
				m_ChannelData[channr]->fx[f].fxcnt1 += s;
				m_ChannelData[channr]->fx[f].fxcnt1 &= 255;
				c = (short)m_ChannelData[channr]->fx[f].fxcnt1;
				for (i=0; i<256; i++)
				{
					dw[i] = (sw1[i] +sw2[c])>>1;
					c++;
					c&=255;
				}
			}
			break;
		case 5:  // Resonance
			{
				chanfx *fx;
				int dest,src1,src2;
				short *dw,*sw1,*sw2;
				short i,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				double centerFreq,bandwidth;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					centerFreq = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1*20);
					bandwidth = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2*16);
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						centerFreq = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1*20);
						bandwidth = double(sw2[c]+32768)/16;
					}
					else
					{
						centerFreq = double(sw2[c]+32768)/13;
						bandwidth = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2*16);
					}
				}

				fx->b2 = exp(-(2 * M_PI) * (bandwidth / 22000));
				fx->b1 = (-4.0 * fx->b2) / (1.0 + fx->b2) * cos(2 * M_PI * (centerFreq / 22000));
				fx->a0 = (1.0 - fx->b2) * sqrt(1.0 - (fx->b1 * fx->b1) / (4.0 * fx->b2));

				for (i=0; i<256; i++)
				{
					double o;
					o = fx->a0 * (double(sw1[i])/32768) - fx->b1 * fx->y1 - fx->b2 * fx->y2;

					fx->y2 = fx->y1;
					fx->y1 = o;
					if(o>.9999)o=.9999;
					if(o<-.9999)o=-.9999;
					dw[i] = short(o*32768);
				}
			}
			break;
		case 6:  // Reso Whistle
			{
				chanfx *fx;
				int dest,src1,src2;
				short *dw,*sw1,*sw2;
				short i,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				double centerFreq,bandwidth;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					centerFreq = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1*20);
					bandwidth = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2*16);
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						centerFreq = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1*20);
						bandwidth = double(sw2[c]+32768)/16;
					}
					else
					{
						centerFreq = double(sw2[c]+32768)/13;
						bandwidth = double(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2*16);
					}
				}

				fx->b2 = exp(-(2 * M_PI) * (bandwidth / 22000));
				fx->b1 = (-4.0 * fx->b2) / (1.0 + fx->b2) * cos(2 * M_PI * (centerFreq / 22000));
				fx->a0 = (1.0 - fx->b2) * sqrt(1.0 - (fx->b1 * fx->b1) / (4.0 * fx->b2));

				fx->b2*=1.2; // do the reso whistle
				for (i=0; i<256; i++)
				{
					double o;
					o = fx->a0 * (double(sw1[i])/32768) - fx->b1 * fx->y1 - fx->b2 * fx->y2;

					fx->y2 = fx->y1;
					fx->y1 = o;
					if(o>.9999)o=.9999;
					if(o<-.9999)o=-.9999;
					dw[i] = short(o*32768);
				}
			}
			break;
		case 7:  // Morphing
			{
				chanfx *fx;
				int dest,src1,src2,osc;
				short *dw,*sw1,*sw2,*ow;
				short i,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect2;
				osc = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];
				ow = &m_ChannelData[channr]->waves[256*osc];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				short m1,m2;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					m1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						m1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					}
					else
					{
						m1 = (ow[c]+32768)/256;
					}
				}

				m2 = 255-m1;
				for (i=0; i<256; i++)
				{
					int a;
					a=(((int)sw1[i]*m1)/256)+(((int)sw2[i]*m2)/256);
					dw[i] = short(a);
				}
			}
			break;
		case 8:  // Dyna-Morphing
			{
				// we gaan er hier vanuit dat in  presets plek: 1536 altijd een sinus zit

				chanfx *fx;
				int dest,src1,src2,osc;
				short *dw,*sw1,*sw2,*ow,*si;
				short i,c;
				si = &m_Presets[1536];//sinus
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect2;
				osc = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];
				ow = &m_ChannelData[channr]->waves[256*osc];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				short m1,m2,sc; //sc is sincnt
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					sc = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						sc = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					}
					else
					{
						sc = (ow[c]+32768)/256;
					}
				}

				for (i=0; i<256; i++)
				{
					int a;
					m1=(si[sc]>>8)+128;
					m2 = 255-m1;
					a=(((int)sw1[i]*m1)/256)+(((int)sw2[i]*m2)/256);
					dw[i] = short(a);
					sc++;
					sc&=255;
				}
			}
			break;
		case 9:  // Distortion
			{
				chanfx *fx;
				int dest,src1,osc;
				short *dw,*sw1,*ow;
				short i,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				osc = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				ow = &m_ChannelData[channr]->waves[256*osc];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				short m1;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					m1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						m1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					}
					else
					{
						m1 = (ow[c]+32768)/256;
					}
				}

				for (i=0; i<256; i++)
				{
					int a;
					a=((int)sw1[i]*m1)/16;
					a+=32768;
					if(a<0)a=-a;
					a%=131072;
					if(a>65535)
					{
						a = 131071-a;
					}
					a-=32768;
					dw[i] = short(a);
				}
			}
			break;
		case 10:  // Scroll left
			{
				int dest;
				short *dw;
				short i,t;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				dw = &m_ChannelData[channr]->waves[256*dest];

				t=dw[0];
				for (i=0; i<255; i++)
				{
					dw[i] = dw[i+1];
				}
				dw[255]=t;
			}
			break;
		case 11:  // Upsample
			{
				int dest;
				short *dw;
				short i,c;
				c = (short)m_ChannelData[channr]->fx[f].fxcnt1;
				if(c!=0) // timeout afgelopen?
				{
					m_ChannelData[channr]->fx[f].fxcnt1--;
					break;
				}
				m_ChannelData[channr]->fx[f].fxcnt1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				dw = &m_ChannelData[channr]->waves[256*dest];

				for (i=0; i<128; i++)
				{
					dw[i]=dw[i*2];
				}
				memcpy(&dw[128],&dw[0],256);
			}
			break;
		case 12:  // Clipper
			{
				chanfx *fx;
				int dest,src1,osc;
				short *dw,*sw1,*ow;
				short i,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				osc = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				ow = &m_ChannelData[channr]->waves[256*osc];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				short m1;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					m1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						m1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					}
					else
					{
						m1 = (ow[c]+32768)/256;
					}
				}

				for (i=0; i<256; i++)
				{
					int a;
					a=((int)sw1[i]*m1)/16;
					if(a<-32767)a=-32767;
					if(a>32767)a=32767;
					dw[i] = short(a);
				}
			}
			break;
		case 13:  // bandpass
			{
				chanfx *fx;
				int dest,src1,src2;
				short *dw,*sw1,*sw2;
				short i,c;
				int _2_pi_w0;
				int _1000_Q;

				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				int freq,reso;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					freq = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					reso = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
					freq*=16; //(freq 0 - 16000hz)
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						freq = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
						reso = (sw2[c]+32768)>>8;
						freq*=16; //(freq 0 - 16000hz)
					}
					else
					{
						freq = (sw2[c]+32768)/16;
						reso = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
					}
				}
				//calc freq;
				double x = freq - 920.0;
//				double w0 = 228 + 3900/2*(1 + tanh(_copysign(pow(fabs(x), 0.85)/95, x)));
				double w0 = 228+freq;
				_2_pi_w0 = int(2*M_PI*w0);

				//calc Q
				_1000_Q = 707 + 1000*reso/128;

				int _2_pi_w0_delta_t;
				int Vhp_next;
				int Vbp_next;
				int Vlp_next;
				int Vbp;
				int	Vlp;
				int	Vhp;
				int Vi;
				int s;
				int delta_t;

				Vbp = fx->Vbp;
				Vlp = fx->Vlp;
				Vhp = fx->Vhp;
				delta_t=8;
				
				// now let's throw our waveform through the resonator!
				for (i=0; i<256; i++)
				{
					// delta_t is converted to seconds given a 1MHz clock by dividing
					// with 1 000 000. This is done in three operations to avoid integer
					// multiplication overflow.
					_2_pi_w0_delta_t = _2_pi_w0*delta_t/100;

					// Calculate filter outputs.
					Vi=sw1[i];
					Vhp_next = Vbp*1000/_1000_Q - Vlp + Vi;
					Vbp_next = Vbp - _2_pi_w0_delta_t*(Vhp/100)/100;
					Vlp_next = Vlp - _2_pi_w0_delta_t*(Vbp/100)/100;
					Vhp = Vhp_next;
					Vbp = Vbp_next;
					Vlp = Vlp_next;

					s = Vlp;
					if(s>32767)s=32767;
					if(s<-32767)s=-32767;

					dw[i] = short(s);
				}
				fx->Vbp = Vbp;
				fx->Vlp = Vlp;
				fx->Vhp = Vhp;
			}
			break;
		case 14:  // highpass
			{
				chanfx *fx;
				int dest,src1,src2;
				short *dw,*sw1,*sw2;
				short i,c;
				int _2_pi_w0;
				int _1000_Q;

				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				int freq,reso;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					freq = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					reso = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
					freq*=32; //(freq 0 - 16000hz)
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						freq = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
						reso = (sw2[c]+32768)>>8;
						freq*=32; //(freq 0 - 16000hz)
					}
					else
					{
						freq = (sw2[c]+32768)/8;
						reso = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
					}
				}
				//calc freq;
				double x = freq - 920.0;
//				double w0 = 228 + 3900/2*(1 + tanh(_copysign(pow(fabs(x), 0.85)/95, x)));
				double w0 = 228+freq;
				_2_pi_w0 = int(2*M_PI*w0);

				//calc Q
				_1000_Q = 707 + 1000*reso/128;

				int _2_pi_w0_delta_t;
				int Vhp_next;
				int Vbp_next;
				int Vlp_next;
				int Vbp;
				int	Vlp;
				int	Vhp;
				int Vi;
				int s;
				int delta_t;

				Vbp = fx->Vbp;
				Vlp = fx->Vlp;
				Vhp = fx->Vhp;
				delta_t=8;
				
				// now let's throw our waveform through the resonator!
				for (i=0; i<256; i++)
				{
					// delta_t is converted to seconds given a 1MHz clock by dividing
					// with 1 000 000. This is done in three operations to avoid integer
					// multiplication overflow.
					_2_pi_w0_delta_t = _2_pi_w0*delta_t/100;

					// Calculate filter outputs.
					Vi=sw1[i];
					Vhp_next = Vbp*1000/_1000_Q - Vlp + Vi;
					Vbp_next = Vbp - _2_pi_w0_delta_t*(Vhp/100)/100;
					Vlp_next = Vlp - _2_pi_w0_delta_t*(Vbp/100)/100;
					Vhp = Vhp_next;
					Vbp = Vbp_next;
					Vlp = Vlp_next;

					s = Vhp;
					if(s>32767)s=32767;
					if(s<-32767)s=-32767;

					dw[i] = short(s);
				}
				fx->Vbp = Vbp;
				fx->Vlp = Vlp;
				fx->Vhp = Vhp;
			}
			break;
		case 15:  // bandpass
			{
				chanfx *fx;
				int dest,src1,src2;
				short *dw,*sw1,*sw2;
				short i,c;
				int _2_pi_w0;
				int _1000_Q;

				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				src2 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				sw2 = &m_ChannelData[channr]->waves[256*src2];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init

				int freq,reso;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					freq = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					reso = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
					freq*=16; //(freq 0 - 16000hz)
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						freq = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
						reso = (sw2[c]+32768)>>8;
						freq*=16; //(freq 0 - 16000hz)
					}
					else
					{
						freq = (sw2[c]+32768)/16;
						reso = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
					}
				}
				//calc freq;
				double x = freq - 920.0;
//				double w0 = 228 + 3900/2*(1 + tanh(_copysign(pow(fabs(x), 0.85)/95, x)));
				double w0 = 228+freq;
				_2_pi_w0 = int(2*M_PI*w0);

				//calc Q
				_1000_Q = 707 + 1000*reso/128;

				int _2_pi_w0_delta_t;
				int Vhp_next;
				int Vbp_next;
				int Vlp_next;
				int Vbp;
				int	Vlp;
				int	Vhp;
				int Vi;
				int s;
				int delta_t;

				Vbp = fx->Vbp;
				Vlp = fx->Vlp;
				Vhp = fx->Vhp;
				delta_t=8;
				
				// now let's throw our waveform through the resonator!
				for (i=0; i<256; i++)
				{
					// delta_t is converted to seconds given a 1MHz clock by dividing
					// with 1 000 000. This is done in three operations to avoid integer
					// multiplication overflow.
					_2_pi_w0_delta_t = _2_pi_w0*delta_t/100;

					// Calculate filter outputs.
					Vi=sw1[i];
					Vhp_next = Vbp*1000/_1000_Q - Vlp + Vi;
					Vbp_next = Vbp - _2_pi_w0_delta_t*(Vhp/100)/100;
					Vlp_next = Vlp - _2_pi_w0_delta_t*(Vbp/100)/100;
					Vhp = Vhp_next;
					Vbp = Vbp_next;
					Vlp = Vlp_next;

					s = Vbp;
					if(s>32767)s=32767;
					if(s<-32767)s=-32767;

					dw[i] = short(s);
				}
				fx->Vbp = Vbp;
				fx->Vlp = Vlp;
				fx->Vhp = Vhp;
			}
			break;
		case 16:  // Noise
			{
				int dest;
				short *dw;
				short i;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				dw = &m_ChannelData[channr]->waves[256*dest];

				for (i=0; i<256; i++)
				{
					dw[i]=(rand()*2)-32768;
				}
			}
			break;
		case 17:  // Squash
			{
				chanfx *fx;
				int dest,src1,osc;
				short *dw,*sw1,*ow;
				short i,c;
				dest = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].dsteffect;
				src1 = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].srceffect1;
				osc = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect-1;
				dw = &m_ChannelData[channr]->waves[256*dest];
				sw1 = &m_ChannelData[channr]->waves[256*src1];
				ow = &m_ChannelData[channr]->waves[256*osc];

				c = (short)m_ChannelData[channr]->fx[f].osccnt;

// init
				unsigned short m1a,m1b,m2;
				fx = &m_ChannelData[channr]->fx[f];
				if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].osceffect==0)
				{
					m1a = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
					m1b = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
				}
				else
				{
					if(m_Instruments[m_ChannelData[channr]->instrument]->fx[f].oscflg)
					{
						m1a = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar1;
						m1b = (ow[c]+32768)/256;
					}
					else
					{
						m1a = (ow[c]+32768)/256;
						m1b = m_Instruments[m_ChannelData[channr]->instrument]->fx[f].effectvar2;
					}
				}

				m1b<<=8;
				m1a+=m1b; //m1 is nu de opteller voor de squash
				m2=0;    //m2 is de eigenlijke teller welke 256 * te groot is(fixed point)
				for (i=0; i<256; i++)
				{
					int a,b;
					b=sw1[m2>>8];
					a=sw1[(m2>>8)+1];
					a*=(m2&255);
					b*=(255-(m2&255));
					a=(a>>8)+(b>>8);
					dw[i] = a;
					m2+=m1a;
				}
			}
			break;
		}
	}
}

void SoundEngine::HandleInstrument(int channr)
{
	int inst,vol,freq,pan;
	inst = m_ChannelData[channr]->instrument;
//vol
	if(m_Instruments[inst]->amwave == 0) //volume wave?
	{
		vol = 0;
	}
	else
	{
		m_ChannelData[channr]->volcnt += m_Instruments[inst]->amspd;
		if (m_ChannelData[channr]->volcnt >= 256)
		{
			m_ChannelData[channr]->volcnt -= 256;
			m_ChannelData[channr]->volcnt += m_Instruments[inst]->amlooppoint;
			if(m_ChannelData[channr]->volcnt >= 256)
			{
				m_ChannelData[channr]->volcnt = m_Instruments[inst]->amlooppoint;
			}
		}

		vol = m_ChannelData[channr]->waves[(256*(m_Instruments[inst]->amwave-1))+m_ChannelData[channr]->volcnt];
		vol = vol+32768;
		vol /= 6;
		vol = -vol;                //10930;
		if (vol <-10000) vol = -10000;
	}	

// last but not least, the master volume
	vol += 10000;
	vol *= m_Instruments[inst]->mastervol;
	vol >>=8;
	vol *= m_MasterVolume; //and the replayers master master volume
	vol >>=8;
	vol -= 10000;
	m_ChannelData[channr]->curvol=vol;
//	if(m_ChannelData[channr]->buf) m_ChannelData[channr]->buf->SetVolume(vol);

// Panning...similar to volume
	if(m_Instruments[inst]->panwave == 0) //panning wave?
	{
		pan = 0;
	}
	else
	{
		m_ChannelData[channr]->pancnt += m_Instruments[inst]->panspd;
		if (m_ChannelData[channr]->pancnt >= 256)
		{
			m_ChannelData[channr]->pancnt -= 256;
			m_ChannelData[channr]->pancnt += m_Instruments[inst]->panlooppoint;
			if(m_ChannelData[channr]->pancnt >= 256)
			{
				m_ChannelData[channr]->pancnt = m_Instruments[inst]->panlooppoint;
			}
		}

		pan = m_ChannelData[channr]->waves[(256*(m_Instruments[inst]->panwave-1))+m_ChannelData[channr]->pancnt];
		pan >>=7;
	}	
//	if(m_ChannelData[channr]->buf) m_ChannelData[channr]->buf->SetPan(pan);
	m_ChannelData[channr]->curpan = pan;
	
//freq
	int k;
	k = 0;
	k = m_Arpeggios[(m_Instruments[inst]->arpeggio*16)+m_ChannelData[channr]->arpcnt];
	m_ChannelData[channr]->arpcnt++;
	m_ChannelData[channr]->arpcnt&=15;

	freq = m_FrequencyTable[m_Instruments[inst]->finetune][k+m_ChannelData[channr]->curnote]; 

	if(m_ChannelData[channr]->freqdel)
	{
		m_ChannelData[channr]->freqdel--;
	}
	else
	{
		if(m_Instruments[inst]->fmwave != 0) //frequency wave?
		{
			m_ChannelData[channr]->freqcnt += m_Instruments[inst]->fmspd;
			if (m_ChannelData[channr]->freqcnt >= 256)
			{
				m_ChannelData[channr]->freqcnt -= 256;
				m_ChannelData[channr]->freqcnt += m_Instruments[inst]->fmlooppoint;
				if(m_ChannelData[channr]->freqcnt >= 256)
				{
					m_ChannelData[channr]->freqcnt = m_Instruments[inst]->fmlooppoint;
				}
			}
			freq -= m_ChannelData[channr]->waves[(256*(m_Instruments[inst]->fmwave-1))+m_ChannelData[channr]->freqcnt];
		}
	}
	freq += m_ChannelData[channr]->bendadd;
	m_ChannelData[channr]->curfreq = freq;
	
// updaten van pitchbend

	if(m_ChannelData[channr]->bendspd != 0)
	{
		if(m_ChannelData[channr]->bendspd >0)
		{
			if(m_ChannelData[channr]->bendadd < m_ChannelData[channr]->destfreq)
			{
				m_ChannelData[channr]->bendadd += m_ChannelData[channr]->bendspd;
				if(m_ChannelData[channr]->bendadd > m_ChannelData[channr]->destfreq)
				{
					m_ChannelData[channr]->bendadd = m_ChannelData[channr]->destfreq;
				}
			}
		}
		else
		{
			if(m_ChannelData[channr]->bendadd > m_ChannelData[channr]->destfreq)
			{
				m_ChannelData[channr]->bendadd += m_ChannelData[channr]->bendspd;
				if(m_ChannelData[channr]->bendadd < m_ChannelData[channr]->destfreq)
				{
					m_ChannelData[channr]->bendadd = m_ChannelData[channr]->destfreq;
				}
			}
		}
	}
}

// If inst = 0 then do not copy all waveforms to the channel buffer.
// This was one can choose to continue with the possible corrupted
// waves or use the fresh ones.

void SoundEngine::PlayInstrument(int channr, int inst, int note)
{
	int f,oldinst;
// instruments init

	if(inst > m_SongPack.nrofinst) return; // dit mag niet!!!
	if(m_ChannelData[channr]->instrument == -1 && inst == 0) return; //geen instrument 0 op een gemute channel...er was namelijk geen previous instrument
	m_DisplayChannel = channr;
	m_ChannelData[channr]->arpcnt = 0;
	m_ChannelData[channr]->volcnt = 0;
	m_ChannelData[channr]->pancnt = 0;
	m_ChannelData[channr]->freqcnt = 0;
	m_ChannelData[channr]->curnote = note;
	m_ChannelData[channr]->curfreq = 0;
	m_ChannelData[channr]->bendtonote = note;
	m_ChannelData[channr]->bendadd = 0;
	m_ChannelData[channr]->destfreq = 0;
	m_ChannelData[channr]->bendspd = 0;

	if(inst != 0)
	{
		int i;

		if(m_Instruments[inst-1]->sharing==0) // no sample sharing
		{
			m_ChannelData[channr]->sampledata = m_Instruments[inst-1]->sampledata;
		}
		else
		{
			short sharing;
			sharing = m_Instruments[inst-1]->sharing;
			m_ChannelData[channr]->sampledata = m_Instruments[sharing-1]->sampledata;
		}
		m_ChannelData[channr]->samplepos = m_Instruments[inst-1]->startpoint<<8;
		m_ChannelData[channr]->looppoint = m_Instruments[inst-1]->looppoint<<8;
		m_ChannelData[channr]->endpoint = m_Instruments[inst-1]->endpoint<<8;
		m_ChannelData[channr]->loopflg = m_Instruments[inst-1]->loopflg;
		m_ChannelData[channr]->bidirecflg = m_Instruments[inst-1]->bidirecflg;
		m_ChannelData[channr]->curdirecflg = 0;

		m_ChannelData[channr]->lastplaypos = -1; // first time
		m_ChannelData[channr]->freqdel = m_Instruments[inst-1]->fmdelay;
		for (i=0; i<16; i++)
		{
			if (m_Instruments[inst-1]->resetwave[i])
			{
				memcpy(&m_ChannelData[channr]->waves[i*256],&m_Instruments[inst-1]->waves[i*256],512);
			}
		}
		m_ChannelData[channr]->instrument = inst-1;
	}
	oldinst = m_ChannelData[channr]->instrument;
// effects init
	for (f=0; f<4; f++) // 4 effects per instrument
	{
		if (m_Instruments[oldinst]->fx[f].effecttype != 0) // is er een effect?
		{
			if (m_Instruments[oldinst]->fx[f].reseteffect)
			{
				m_ChannelData[channr]->fx[f].osccnt = 0;
				m_ChannelData[channr]->fx[f].fxcnt1 = 0;
				m_ChannelData[channr]->fx[f].fxcnt2 = 0;
				m_ChannelData[channr]->fx[f].y2 = 0;
				m_ChannelData[channr]->fx[f].y1 = 0;
				m_ChannelData[channr]->fx[f].Vhp=0;
				m_ChannelData[channr]->fx[f].Vbp=0;
				m_ChannelData[channr]->fx[f].Vlp=0;
			}
		}
	}
}

void	SoundEngine::HandleSong(void)
{
	short i;
	if (!m_PlayFlg) return; 
	if(m_PauseFlg) return;
	m_PatternDelay--;
	if(m_PatternDelay==0)
	{
		if(m_PlayMode==SE_PM_PATTERN) //pattern editor?
		{
			if(!(m_PatternOffset&1))  // change the groove
			{
				m_PlaySpeed = 8-m_Songs[m_CurrentSubsong]->groove;
			}
			else
			{
				m_PlaySpeed = 8+m_Songs[m_CurrentSubsong]->groove;
			}
//			m_PlaySpeed=8;
		}
		else
		{
			if(!(m_PlayStep&1))  // change the groove
			{
				m_PlaySpeed = 8-m_Songs[m_CurrentSubsong]->groove;
			}
			else
			{
				m_PlaySpeed = 8+m_Songs[m_CurrentSubsong]->groove;
			}
		}
		m_PatternDelay = m_PlaySpeed;
		if(m_PlayMode==SE_PM_PATTERN) // zitten we in de pattern editor?
		{
			m_PatternOffset++;
			m_PatternOffset%=m_PatternLength;
		}
		else // we playen dus de song?
		{
			for (i=0; i< m_NrOfChannels; i++)
			{
				m_ChannelData[i]->patpos++;
				if(m_ChannelData[i]->patpos == m_Songs[m_CurrentSubsong]->songchans[i][m_ChannelData[i]->songpos].patlen || m_ChannelData[i]->songpos == -1) //the ==-1 part is that the song counter always is 1 before the start...so if the song starts at the beginning, the pos is -1
				{
					m_ChannelData[i]->patpos = 0;
					m_ChannelData[i]->songpos++;

					// Refresh mute settings.
					m_Songs[m_CurrentSubsong]->mute[i] = m_MuteTab[i];
				}
			}

			m_PlayStep++;
			if(m_PlayStep==64)
			{
				m_PlayStep=0;
				m_PlayPosition++;
			}

			// checken of we playpos al op endpos zit
			if(m_PlayPosition == m_Songs[m_CurrentSubsong]->endpos && m_PlayStep == m_Songs[m_CurrentSubsong]->endstep)
			{

				// checken of we moeten loopen of stoppen

				if(m_Songs[m_CurrentSubsong]->songloop) //loop??
				{
				// now me must reset all the playpointers to the loop positions


					int maat,pos,t;
					songitem *songpart;
					for(t=0;t<SE_MAXCHANS;t++)
					{
						int endpos;
						int lastmaat;
						maat = 0;
						pos = 0;
						lastmaat=0;

						songpart = m_Songs[m_CurrentSubsong]->songchans[t];
						endpos = (m_Songs[m_CurrentSubsong]->looppos*64)+m_Songs[m_CurrentSubsong]->loopstep;
						while(pos<256)
						{
							if(maat > endpos)
							{
								if(pos != endpos) pos--;
								break;
							}
							lastmaat=maat;
							maat+=songpart[pos].patlen;
							pos++;
						}
						if(pos == 256)//oei dat kan niet....foutje
						{
							m_PlayFlg=0;
							goto nextline;
						}

						endpos-=lastmaat;
						endpos &=63;
						
						m_ChannelData[t]->songpos = pos;
						m_ChannelData[t]->patpos = endpos;

					}

					m_PlayPosition=m_Songs[m_CurrentSubsong]->looppos;
					m_PlayStep=m_Songs[m_CurrentSubsong]->loopstep;

					m_HasLooped = 1; // rePlayer

					//er was iets mis... de startpos stond te ver..?!?
	nextline:
					//dummy instructie
					int dumm=0;
				}
				else  // de song stoppen
				{
					m_PlayFlg = 0;
					m_PauseFlg = 0;
					m_PlayMode = SE_PM_SONG;
					m_PlayPosition=m_Songs[m_CurrentSubsong]->songpos;
					m_PlayStep=m_Songs[m_CurrentSubsong]->songstep;

					m_HasLooped = 1; // rePlayer
				}
			}
		}
	}
}

void SoundEngine::HandlePattern(int channr)
{
	int pat,off;
	int f,d,s,p;
	if(m_PauseFlg) return;
	if (!m_PlayFlg) return; 
	if (m_PatternDelay != m_PlaySpeed) return;

	if (m_PlayMode==SE_PM_PATTERN) // are we in the pattern editor?
	{
		if (channr > 0) return;  // just play channel 0

		pat = m_CurrentPattern;
		off = m_PatternOffset;
	}
	else  // song editor dus...
	{
		if(m_Songs[m_CurrentSubsong]->mute[channr]==1) return;
		off = m_ChannelData[channr]->patpos;
		pat = m_Songs[m_CurrentSubsong]->songchans[channr][m_ChannelData[channr]->songpos].patnr;
	}

// we gaan nu het intrument initialiseren
	f = m_Patterns[(pat*64)+off].srcnote;
	if (f != 0)
	{
		PlayInstrument(channr,m_Patterns[(pat*64)+off].inst,f);
	}

// nu de eventuele special effects
	s = m_Patterns[(pat*64)+off].script;
	d = m_Patterns[(pat*64)+off].dstnote;
	p = m_Patterns[(pat*64)+off].param;
	HandleScript(f,s,d,p,channr);
}

void SoundEngine::HandleScript(int f,int s, int d, int p, int channr) //note, script,dstnote,param,channr
{
	int a;
	if(m_ChannelData[channr]->instrument==-1) return;   //valt niets te veranderen
	switch(s) // welk script???
	{
	default:
	case 0:
		return;
	case 1: //pbend
		if(m_ChannelData[channr]->bendtonote)//hebben we al eens gebend?
		{
			a = m_FrequencyTable[m_Instruments[m_ChannelData[channr]->instrument]->finetune][m_ChannelData[channr]->bendtonote]; // begin frequentie
			m_ChannelData[channr]->curnote = m_ChannelData[channr]->bendtonote;
		}
		else
		{
			a = m_FrequencyTable[m_Instruments[m_ChannelData[channr]->instrument]->finetune][f]; // begin freqeuntie
		}
		m_ChannelData[channr]->bendadd = 0;
		m_ChannelData[channr]->destfreq = m_FrequencyTable[m_Instruments[m_ChannelData[channr]->instrument]->finetune][d] - a;
		m_ChannelData[channr]->bendspd = p*20;
		m_ChannelData[channr]->bendtonote = d;
		break;
	case 2: //waveform
		if(d>15)
		{
			d = 15;
		}

		m_Instruments[m_ChannelData[channr]->instrument]->waveform = d;
		break;
	case 3: //wavelength
		if(d>192)
		{
			d = 256;
		}
		else
		{
			if(d>96)
			{
				d=128;
			}
			else
			{
				if(d>48)
				{
					d=64;
				}
				else
				{
					d=32;
				}
			}
		}

		m_Instruments[m_ChannelData[channr]->instrument]->wavelength = d;
		break;
	case 4: //mastervol
		m_Instruments[m_ChannelData[channr]->instrument]->mastervol = d;
		break;
	case 5: //amwaveform
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->amwave = d;
		break;
	case 6: //amspd
		m_Instruments[m_ChannelData[channr]->instrument]->amspd = d;
		break;
	case 7: //amlooppoint
		m_Instruments[m_ChannelData[channr]->instrument]->amlooppoint = d;
		break;
	case 8: //finetune
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->finetune = d;
		break;
	case 9: //fmwaveform
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fmwave = d;
		break;
	case 10: //fmspd
		m_Instruments[m_ChannelData[channr]->instrument]->fmspd = d;
		break;
	case 11: //fmlooppoint
		m_Instruments[m_ChannelData[channr]->instrument]->fmlooppoint = d;
		break;
	case 12: //fmdelay
		m_Instruments[m_ChannelData[channr]->instrument]->fmdelay = d;
		break;
	case 13: //arpeggio
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->arpeggio = d;
		break;
	case 14: //fxdstwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].dsteffect = d;
		break;
	case 15: //fxsrcwave1
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].srceffect1 = d;
		break;
	case 16: //fxsrcwave2
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].srceffect2 = d;
		break;
	case 17: //fxoscwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].osceffect = d;
		break;
	case 18: //effectvar1
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].effectvar1 = d;
		break;
	case 19: //effectvar2
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].effectvar2 = d;
		break;
	case 20: //effectspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].effectspd = d;
		break;
	case 21: //oscspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].oscspd = d;
		break;
	case 22: //oscflg
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].oscflg = d;
		break;
	case 23: //effecttype
		if(d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].effecttype = d;
		break;
	case 24: //reseteffect
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[0].reseteffect = d;
		break;

	case 25: //fxdstwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].dsteffect = d;
		break;
	case 26: //fxsrcwave1
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].srceffect1 = d;
		break;
	case 27: //fxsrcwave2
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].srceffect2 = d;
		break;
	case 28: //fxoscwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].osceffect = d;
		break;
	case 29: //effectvar1
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].effectvar1 = d;
		break;
	case 30: //effectvar2
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].effectvar2 = d;
		break;
	case 31: //effectspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].effectspd = d;
		break;
	case 32: //oscspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].oscspd = d;
		break;
	case 33: //oscflg
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].oscflg = d;
		break;
	case 34: //effecttype
		if(d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].effecttype = d;
		break;
	case 35: //reseteffect
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[1].reseteffect = d;
		break;
	case 36: //fxdstwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].dsteffect = d;
		break;
	case 37: //fxsrcwave1
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].srceffect1 = d;
		break;
	case 38: //fxsrcwave2
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].srceffect2 = d;
		break;
	case 39: //fxoscwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].osceffect = d;
		break;
	case 40: //effectvar1
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].effectvar1 = d;
		break;
	case 41: //effectvar2
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].effectvar2 = d;
		break;
	case 42: //effectspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].effectspd = d;
		break;
	case 43: //oscspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].oscspd = d;
		break;
	case 44: //oscflg
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].oscflg = d;
		break;
	case 45: //effecttype
		if(d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].effecttype = d;
		break;
	case 46: //reseteffect
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[2].reseteffect = d;
		break;

	case 47: //fxdstwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].dsteffect = d;
		break;
	case 48: //fxsrcwave1
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].srceffect1 = d;
		break;
	case 49: //fxsrcwave2
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].srceffect2 = d;
		break;
	case 50: //fxoscwave
		if(d>15)
		{
			d = 15;
		}
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].osceffect = d;
		break;
	case 51: //effectvar1
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].effectvar1 = d;
		break;
	case 52: //effectvar2
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].effectvar2 = d;
		break;
	case 53: //effectspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].effectspd = d;
		break;
	case 54: //oscspd
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].oscspd = d;
		break;
	case 55: //oscflg
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].oscflg = d;
		break;
	case 56: //effecttype
		if(d>=SE_NROFEFFECTS) d=SE_NROFEFFECTS-1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].effecttype = d;
		break;
	case 57: //reseteffect
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->fx[3].reseteffect = d;
		break;

	case 58: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[0] = d;
		break;
	case 59: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[1] = d;
		break;
	case 60: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[2] = d;
		break;
	case 61: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[3] = d;
		break;
	case 62: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[4] = d;
		break;
	case 63: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[5] = d;
		break;
	case 64: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[6] = d;
		break;
	case 65: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[7] = d;
		break;
	case 66: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[8] = d;
		break;
	case 67: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[9] = d;
		break;
	case 68: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[10] = d;
		break;
	case 69: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[11] = d;
		break;
	case 70: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[12] = d;
		break;
	case 71: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[13] = d;
		break;
	case 72: //resetwave1
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[14] = d;
		break;
	case 73: //resetwave16
		if(d>1) d=1;
		m_Instruments[m_ChannelData[channr]->instrument]->resetwave[15] = d;
		break;
	case 74: //Change bpm
		if(d<=10) d=10;
		if(d>220) d=220;
		m_Songs[m_CurrentSubsong]->songspd = d;

		float t;
		t=(float)m_Songs[m_CurrentSubsong]->songspd; //bpm
		t/=60.0;         //bps
		t*=32.0;
		m_TimeSpd=(int)(44100.0/t);
		break;
	case 75: //Change Groove
		if(d>3) d=3;
		m_Songs[m_CurrentSubsong]->groove = d;
		break;
	case 76: //Fire External Event
		//removed
		break;
	}
}

void SoundEngine::PlayPattern(int PatternNr)
{
	m_PlayFlg = 1;
	m_CurrentPattern = PatternNr;
	m_PatternOffset = 63;
	m_PatternDelay = 1;
	m_PlayMode = SE_PM_PATTERN;
	m_PlaySpeed = m_Songs[0]->songspd - m_Songs[0]->groove;
}

// Deze functie zorgt ervoor dat de playroutine netjes word aangeroepen
// en alles op een goede manier word geinitialiseerd
void SoundEngine::PlaySubSong(int subsongnr)
{
	int maat,pos,t;
	songitem *songpart;

	m_CurrentSubsong = subsongnr;

	m_NrOfChannels=m_Songs[m_CurrentSubsong]->nrofchans;
	ClearSoundBuffers();

	for(t=0;t<SE_MAXCHANS;t++)
	{
		int endpos;
		int lastmaat;
		maat = 0;
		pos = 0;
		lastmaat=0;
		songpart = m_Songs[m_CurrentSubsong]->songchans[t];
		endpos = (m_Songs[m_CurrentSubsong]->songpos*64)+m_Songs[m_CurrentSubsong]->songstep -1; //min 1 omdat we dan meteen met de nieuwe noot beginnen
		while(pos<256)
		{
			if(maat >= endpos)
			{
				if(pos != endpos) pos--;
				break;
			}
			lastmaat=maat;
			maat+=songpart[pos].patlen;
			pos++;
		}
		if(pos == 256) goto nextline;

		endpos-=lastmaat;
//		endpos-=maat;
		endpos &=63;
		
		m_ChannelData[t]->songpos = pos;
		m_ChannelData[t]->patpos = endpos;

		// nieuwe mutestanden overnemen!  (verandering voor de heer alfrink)

		m_Songs[m_CurrentSubsong]->mute[t] = m_MuteTab[t];
	}

	m_PatternDelay = 1;
	m_PlayMode = SE_PM_SONG;
	m_PlayFlg = 1;
	m_PauseFlg = 0;
	m_PlaySpeed = 8+m_Songs[m_CurrentSubsong]->groove;
//	m_PlaySpeed = 8;

	if(m_Songs[m_CurrentSubsong]->songspd)
	{
		float t;
		t=(float)m_Songs[m_CurrentSubsong]->songspd; //bpm
		t/=60.0;         //bps
		t*=32.0;
		m_TimeSpd=(int)(44100.0/t);
		m_TimeCnt = m_TimeSpd;
	}

	if(m_Songs[m_CurrentSubsong]->songstep == 0)
	{
		m_PlayPosition=m_Songs[m_CurrentSubsong]->songpos-1;
	}
	else
	{
		m_PlayPosition=m_Songs[m_CurrentSubsong]->songpos;
	}
	m_PlayStep=m_Songs[m_CurrentSubsong]->songstep-1;
	m_PlayStep&=63;

	//er was iets mis... de startpos stond te ver..?!?
nextline:
	t=0; //dummy instructie
}

void SoundEngine::StopSong(void)
{
	m_PlayFlg = 0;
	m_PauseFlg = 0;
	m_PlayMode = SE_PM_SONG;
	if(m_Songs)
	{
		m_PlayPosition=m_Songs[m_CurrentSubsong]->songpos;
		m_PlayStep=m_Songs[m_CurrentSubsong]->songstep;
	}
}

void SoundEngine::PauseSong(void)
{
	m_PauseFlg = 1;
}

void SoundEngine::ContinueSong(void)
{
	m_PauseFlg = 0;
}

void SoundEngine::CreateEmptySong(void)
{
	int	t;
	int i,j;

// Channeltabs initten

	ClearSoundBuffers();

	m_CurrentPattern = 1;
	m_CurrentSubsong=0;
	
	m_PlaySpeed = 8;
	m_MasterVolume = 256;

// song initten en op 0 zetten

	m_SongPack.nrofsongs=1;
	m_SongPack.mugiversion=3457;
	m_Songs = (subsong **)Malloc(1*sizeof(subsong*));
	m_Songs[0] = (subsong *) Malloc(sizeof(subsong));
	m_Songs[0]->songspd=120;
	m_Songs[0]->groove=0;
	m_Songs[0]->songpos=0;
	m_Songs[0]->songstep=0;
	m_Songs[0]->looppos=0;
	m_Songs[0]->loopstep=0;
	m_Songs[0]->endpos=1;
	m_Songs[0]->endstep=0;
	m_Songs[0]->nrofchans=0;
	m_Songs[0]->delaytime=32768;
	m_Songs[0]->amplification=400;
	for(i=0;i<SE_MAXCHANS;i++) m_Songs[0]->delayamount[i]=0;
	memset(m_Songs[0]->name,0,32);
	strcpy(m_Songs[0]->name, "Empty");
	songitem *songpart;
	for(i=0;i<SE_MAXCHANS;i++)
	{
		m_Songs[0]->mute[i]=0;
		m_Songs[0]->songchans[i] = (songitem *) Malloc(256*sizeof(songitem));

		songpart = m_Songs[0]->songchans[i];
		for (j=0;j<256;j++)
		{
			songpart[j].patnr = 0;
			songpart[j].patlen = 64;
		}
	}

//patterns maken
	m_SongPack.nrofpats = 2; // aantal patterns beschikbaar
	m_Patterns = (patitem *) Malloc(64*m_SongPack.nrofpats*sizeof(patitem));
	memset(m_Patterns,0,128*sizeof(patitem));
	m_PatternNames = (char **) Malloc(sizeof(char*)*2);
	m_PatternNames[0] = (char *) Malloc(10);
	strcpy(m_PatternNames[0], "Empty");
	m_PatternNames[1] = (char *) Malloc(5);
	strcpy(m_PatternNames[1], "Pat1");

// instrument maken
	m_SongPack.nrofinst = 1;
	m_Instruments = (inst **) Malloc(sizeof(inst**));
	m_Instruments[0] = (inst *) Malloc(sizeof(inst));
	memcpy(m_Instruments[0], m_DefaultInst,sizeof(inst));

//zero1
	for (t=0;t<256;t++)
	{
		m_Presets[t]   = 0;
	}
//zero2
	for (t=0;t<256;t++)
	{
		m_Presets[256+t]   = -0x7fff;
	}
//zero3
	for (t=0;t<256;t++)
	{
		m_Presets[512+t]   = 0x7fff;
	}
//square
	for (t=0;t<128;t++)
	{
		m_Presets[768+t]   = -0x7fff;
		m_Presets[768+t+128] = 0x7fff;
	}
//saw
	for (t=0;t<256;t++)
	{
		m_Presets[1024+t]   = -0x7fff+(t*256);
	}
//triangle
	for (t=0;t<128;t++)
	{
		m_Presets[1280+t]   = -0x7fff+(t*512);
		m_Presets[1280+128+t]   = 0x7fff-(t*512);
	}
//sine
	for (t=0;t<256;t++)
	{
		m_Presets[1536+t]   = (short)(sin((M_PI*t)/128)*32760);
	}
//3rdsine
	for (t=0;t<256;t++)
	{
		m_Presets[1792+t]   = (short)(sin((M_PI*t*3)/128)*32760);
	}
//5thsine
	for (t=0;t<256;t++)
	{
		m_Presets[2048+t]   = (short)(sin((M_PI*t*5)/128)*32760);
	}


//	for (t=0;t<256;t++)
//	{
//		dumwave[t]=3;
//	}

	// mutetable wissen
	for(i=0; i<SE_MAXCHANS; i++)
	{
		m_MuteTab[i] = 0;
	}
}


void SoundEngine::FreeSong(void)
{
	int i,j;
	if(m_Patterns)free(m_Patterns);
	m_Patterns=0;
	if(m_PatternNames)
	{
		for(i=0;i<m_SongPack.nrofpats;i++)
		{
			if(m_PatternNames[i])free(m_PatternNames[i]);
			m_PatternNames[i]=0;
		}
		free(m_PatternNames);
		m_PatternNames=0;
	}

// nu de instruments

	if(m_Instruments)
	{
		for(i=0;i<m_SongPack.nrofinst;i++)
		{
			if(m_Instruments[i])
			{
				if(m_Instruments[i]->sampledata) //is there a sample attached?!?
				{
					if(m_Instruments[i]->sampledata)free(m_Instruments[i]->sampledata);
					m_Instruments[i]->sampledata=0;
				}
				free(m_Instruments[i]);
				m_Instruments[i] = 0;
			}
		}
		free(m_Instruments);
		m_Instruments=0;
	}
// nu de subsongs

	if(m_Songs)
	{
		for(i=0;i<m_SongPack.nrofsongs;i++)
		{
			if(m_Songs[i])
			{
				for(j=0;j<SE_MAXCHANS;j++)
				{
					if(m_Songs[i]->songchans[j])free(m_Songs[i]->songchans[j]);
					m_Songs[i]->songchans[j] = 0;
				}
				free(m_Songs[i]);
				m_Songs[i] = 0;
			}
		}
		free(m_Songs);
		m_Songs = 0;
	}
}


// Loads a Synthrax song... returns 0 when all was good
//  Error codes: 1: memory-allocation failed
//               2: not a valid Jaytrax song
//				 3: File is corrupt
//               4: Couldn't open file
//               5: Couldn't create a temporary dir to unzip in
//				 6: Error while unzippig...(is the infozip dll aanwezig?)
//				 7: The archive doesn't contain a mugician song


#define FILEREAD(dest, size) {if(ReadIndex+(size)<ModuleSize) {memcpy((dest), file+ReadIndex, (size)); ReadIndex +=(size);}else{memset((dest),0,(size));}}
static inline auto Align(auto s) { return (s + 3) & ~3; }

int SoundEngine::LoadSongFromMemory(unsigned char *file, int ModuleSize)
{
	int ReadIndex;
	bool curdirflg;
	int i,j;
	int length[6]; //zorgen dat het zeker weten geen register wordt
	bool failed;
	int error;
	bool fileopenflg;

	curdirflg = false;
	int version; // version of song we load... if it is too old we patch it while we load
				// for a description of changes see the playglob.cpp file
	error = 0;
	failed = false;
	fileopenflg=true;
	FreeSong();
	ReadIndex = 0;
	// First let's read the songheader
	FILEREAD(&m_SongPack,sizeof(song));
	if(m_SongPack.mugiversion >= 3456 && m_SongPack.mugiversion<=3457) // currently supported versions
	{
		// Now let's read all the subsongs
		version = m_SongPack.mugiversion;
		m_Songs = (subsong **)Malloc(m_SongPack.nrofsongs*sizeof(subsong*));
		if(m_Songs)//did the Malloc fail?
		{

			for(i=0;i<m_SongPack.nrofsongs;i++)
			{
				m_Songs[i] = (subsong *) Malloc(sizeof(subsong));
				if(m_Songs[i])
				{
					//subsongheader
					ReadIndex += 4 * SE_MAXCHANS; // skip ugly pointer
					FILEREAD(m_Songs[i]->mute, Align(offsetof(subsong, dummy16) - offsetof(subsong, mute) + 2));
					//subsong channeldata
					for(j=0;j<SE_MAXCHANS;j++)
					{
						m_Songs[i]->songchans[j] = (songitem *) Malloc(256*sizeof(songitem));
						if(m_Songs[i]->songchans[j])
						{
							FILEREAD(m_Songs[i]->songchans[j], 256*sizeof(songitem));
						}
						else //Malloc of channeldata failed
						{
							error = 1;
							failed=true;
							goto loaderror;
						}
					}
				}
				else//Malloc of subsong failed
				{
					error = 1;
					failed=true;
					goto loaderror;
				}
			}

		// now let's load the pattern data
			m_Patterns = (patitem *) Malloc(64*m_SongPack.nrofpats*sizeof(patitem));
			if(m_Patterns)
			{
				FILEREAD(m_Patterns, m_SongPack.nrofpats*64*sizeof(patitem));
			}
			else // Malloc of patterndata failed
			{
				error = 1;
				failed=true;
				goto loaderror;
			}

			// And all the patterns names (first we load 1 long for the length(With the added zero which is saved too))
			m_PatternNames = (char **)Malloc(m_SongPack.nrofpats * sizeof(char *));
			if(m_PatternNames)
			{
				for(i=0;i<m_SongPack.nrofpats;i++)
				{
					FILEREAD(&length[0], 4);
					m_PatternNames[i] = (char *) Malloc(length[0]);
					if(m_PatternNames[i])
					{
						FILEREAD(m_PatternNames[i],length[0]);
					}
					else//Malloc of patname failed
					{
						error = 1;
						failed=true;
						goto loaderror;
					}
				}
			}
			else //Malloc of m_PatternNames failed
			{
				error = 1;
				failed=true;
				goto loaderror;
			}

		// And finally we load the instrumentdata with the occasional sample

			m_Instruments = (inst **) Malloc(m_SongPack.nrofinst*sizeof(inst *));
			if(m_Instruments)
			{
				for(i=0;i<m_SongPack.nrofinst;i++)
				{
					m_Instruments[i] = (inst *)Malloc(sizeof(inst));
					if(m_Instruments[i])
					{
// 						FILEREAD(m_Instruments[i], sizeof(inst));
						m_Instruments[i]->sampledata = nullptr;
						FILEREAD(m_Instruments[i], offsetof(inst, sampledata) + 4);
						FILEREAD(&m_Instruments[i]->samplelength, 4 + 2 * 4096);

						// if we are loading an old version tune, we patch the instruemtn data
						// PATCH 3456 to 3457
						if(version == 3456)
						{
							memset(m_Instruments[i]->samplename+192,0,68);
							if(m_Instruments[i]->sampledata)
							{
								m_Instruments[i]->startpoint=0;
								m_Instruments[i]->endpoint=(m_Instruments[i]->samplelength/2);
								m_Instruments[i]->looppoint=0;
							}
						}
						// END PATCH

						if(m_Instruments[i]->sampledata != 0) // there's a sample attached to it?
						{
							m_Instruments[i]->sampledata = (unsigned char *) Malloc(m_Instruments[i]->samplelength);
							if(m_Instruments[i]->sampledata)
							{
								FILEREAD(m_Instruments[i]->sampledata,m_Instruments[i]->samplelength);
							}
							else //Malloc of wavedata failed
							{
								error = 1;
								failed=true;
								goto loaderror;
							}
						}
					}
					else //Malloc of instrument failed
					{
						error = 1;
						failed=true;
						goto loaderror;
					}

				}
			}
			else //Malloc of instruments tabel failed
			{
				error = 1;
				failed=true;
				goto loaderror;
			}

			// forgotten...but finally also the arpeggiotable

			FILEREAD(m_Arpeggios,256);

			// everything read,....now update the mute table with the loaded one

			for(i=0; i<SE_MAXCHANS; i++)
			{
				m_MuteTab[i] = m_Songs[0]->mute[i];
			}

			// YES!!!! Ready...all is saved.. Let's close the file

		}
		else//Malloc of 'songs' failed
		{
			error = 1;
			failed=true;
			goto loaderror;
		} 
	}
	else //if mugiversion
	{
		error = 2;
		failed=true;
		goto loaderror;
	}


loaderror:
	if(failed)
	{
		FreeSong(); //throw all other songdata away
		CreateEmptySong(); // empty with zeroed song
	}
	ClearSoundBuffers(); // reset all channeldata for old instruments

	return error;
}


typedef struct mugiheader mugiheader;
struct mugiheader {
	char name[24];
	short arpflg;
	short	nrofpats;
	int	nrofseqs[8];  //max 8 subsongs
	int	nrofinstr;
	int	nrofwaves;
	int	nrofsamples;
	int	samplebytes;
};



void SoundEngine::PreCalcFinetune(void)
{
	int i,j;
	double f,y;
	y=2;
	for(j=0;j<SE_NROFFINETUNESTEPS;j++)  //fine-tuning
	{
		for(i=0;i<128;i++)
		{
	// formule = wortel(x/12)

			f=((i+3)*16)-j;   // we beginnen met de a want die is 440hz
			f=f/192;
			f=pow(y,f);
			f=f*220;
			m_FrequencyTable[j][i]=(int)(f+0.5);
		}
	}
}

#if 0
void SoundEngine::rendertodisk(CFile *outputfile,int seconds)
{
	//renderbuf shoudl be allocted...not on the sstack!!!
	short renderbuf[30000*2];			// buffer to render sample in(stereo)
	WAVEFORMATEX header;
	char *riffname = "RIFF";
	char *wavename = "WAVEfmt ";
	char *dataname = "data";
	int length;
	int samplelength;
	int i;
	
	PlaySubSong(m_CurrentSubsong);  //playback of song starten
	
	samplelength = seconds*44100*2*2;

	header.wFormatTag = 1;
	header.nChannels = 2;
	header.nSamplesPerSec = 44100;
	header.nAvgBytesPerSec = 44100*2*2;
	header.nBlockAlign = 4;
	header.wBitsPerSample = 16;
	header.cbSize = 0;

	length = samplelength + 8 + 16 + 4 + 4 + 4;

	outputfile->Write(riffname,4);
	outputfile->Write(&length,4);
	outputfile->Write(wavename,8);
	length = 16;
	outputfile->Write(&length,4);   //lengte van header gedeelte
	outputfile->Write(&header,16);
	outputfile->Write(dataname,4);
	length = samplelength;
	outputfile->Write(&length,4);   //lengte van data gedeelte

// hier gaan we de song renderen in hapklare brokken van 1/2 seconde en deze saven we naar schijf

	for(i=0;i<(seconds*2);i++)
	{
		renderchannels1(renderbuf, 22050, 44100);
//		renderchannels(22050);
		outputfile->Write(renderbuf, 22050*2*2);
		renderchannels1(renderbuf, 22050, 44100);
//		renderchannels(22050);
		outputfile->Write(renderbuf, 22050*2*2);
	}

// klaar met saven wav file

	m_PlayFlg = 0;
	m_PauseFlg = 0;
	m_PlayMode = SE_PM_SONG;
	m_PlayPosition=m_Songs[m_CurrentSubsong]->songpos;
	m_PlayStep=m_Songs[m_CurrentSubsong]->songstep;
}
#endif





void SoundEngine::RenderBuffer( void *bufptr, int nrofsamples, int frequency, int routine, int nrofchannels)
{
	switch(nrofchannels)
	{
	case 1:
		{
			switch (routine)
			{
			case 1:
				RenderChannels1Mono((short *)bufptr, nrofsamples, frequency);
				break;
			case 2:
				RenderChannels2Mono((short *)bufptr, nrofsamples, frequency);
				break;
			case 3:
				RenderChannels3Mono((short *)bufptr, nrofsamples, frequency);
				break;
			}
		}
		break;
	case 2:
		{
			switch (routine)
			{
			case 1:
				RenderChannels1((short *)bufptr, nrofsamples, frequency);
				break;
			case 2:
				RenderChannels2((short *)bufptr, nrofsamples, frequency);
				break;
			case 3:
				RenderChannels3((short *)bufptr, nrofsamples, frequency);
				break;
			}
		}
	}
}





