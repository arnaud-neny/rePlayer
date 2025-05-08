//============================================================================
//
//				Slicit.cpp
//				----------
//				druttis@darkface.pp.se
//
//============================================================================
#include <psycle/plugin_interface.hpp>
#include <cstring>
#include <cmath>
#include <cstdio>
#include "../dsp/Inertia.h"
namespace psycle::plugins::druttis::slicit {
using namespace psycle::plugin_interface;

//============================================================================
//				Defines
//============================================================================
#define MAC_NAME				"Slicit"
int const MAC_VERSION = 0x0100;
#define MAC_AUTHOR				"Druttis"

//============================================================================
//				Wavetable
//============================================================================
#define NUM_PROGRAMS 16
#define NUM_STEPS 16

#define FTYPE_OFF 0
#define FTYPE_1P_LP 1
#define FTYPE_1P_HP 2

char const *FTYPE_STRING[3] = { "off", "1-Pole LP", "1-Pole HP" };

int SPEED_FACTORS[4] = { 1, 2, 4, 8 };

//============================================================================
//				Parameters
//============================================================================
#define PARAM_PROGRAM_NR 0
CMachineParameter const paramProgramNr = { "Program Nr.", "Program Nr.", 0, 15, MPF_STATE, 0 };

#define PARAM_LEVEL1 1
CMachineParameter const paramLevel1 = { "Level 1", "Level 1", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL2 2
CMachineParameter const paramLevel2 = { "Level 2", "Level 2", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL3 3
CMachineParameter const paramLevel3 = { "Level 3", "Level 3", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL4 4
CMachineParameter const paramLevel4 = { "Level 4", "Level 4", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL5 5
CMachineParameter const paramLevel5 = { "Level 5", "Level 5", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL6 6
CMachineParameter const paramLevel6 = { "Level 6", "Level 6", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL7 7
CMachineParameter const paramLevel7 = { "Level 7", "Level 7", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL8 8
CMachineParameter const paramLevel8 = { "Level 8", "Level 8", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL9 9
CMachineParameter const paramLevel9 = { "Level 9", "Level 9", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL10 10
CMachineParameter const paramLevel10 = { "Level 10", "Level 10", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL11 11
CMachineParameter const paramLevel11 = { "Level 11", "Level 11", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL12 12
CMachineParameter const paramLevel12 = { "Level 12", "Level 12", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL13 13
CMachineParameter const paramLevel13 = { "Level 13", "Level 13", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL14 14
CMachineParameter const paramLevel14 = { "Level 14", "Level 14", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL15 15
CMachineParameter const paramLevel15 = { "Level 15", "Level 15", 0, 256, MPF_STATE, 256 };

#define PARAM_LEVEL16 16
CMachineParameter const paramLevel16 = { "Level 16", "Level 16", 0, 256, MPF_STATE, 256 };

#define PARAM_NO_TICKS 17
CMachineParameter const paramNoTicks = { "No. Steps", "No. Steps", 1, 16, MPF_STATE, 16 };

#define PARAM_ATTACK1 18
CMachineParameter const paramAttack1 = { "Attack 1", "Attack 1", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK2 19
CMachineParameter const paramAttack2 = { "Attack 2", "Attack 2", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK3 20
CMachineParameter const paramAttack3 = { "Attack 3", "Attack 3", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK4 21
CMachineParameter const paramAttack4 = { "Attack 4", "Attack 4", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK5 22
CMachineParameter const paramAttack5 = { "Attack 5", "Attack 5", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK6 23
CMachineParameter const paramAttack6 = { "Attack 6", "Attack 6", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK7 24
CMachineParameter const paramAttack7 = { "Attack 7", "Attack 7", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK8 25
CMachineParameter const paramAttack8 = { "Attack 8", "Attack 8", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK9 26
CMachineParameter const paramAttack9 = { "Attack 9", "Attack 9", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK10 27
CMachineParameter const paramAttack10 = { "Attack 10", "Attack 10", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK11 28
CMachineParameter const paramAttack11 = { "Attack 11", "Attack 11", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK12 29
CMachineParameter const paramAttack12 = { "Attack 12", "Attack 12", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK13 30
CMachineParameter const paramAttack13 = { "Attack 13", "Attack 13", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK14 31
CMachineParameter const paramAttack14 = { "Attack 14", "Attack 14", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK15 32
CMachineParameter const paramAttack15 = { "Attack 15", "Attack 15", 0, 256, MPF_STATE, 256 };

#define PARAM_ATTACK16 33
CMachineParameter const paramAttack16 = { "Attack 16", "Attack 16", 0, 256, MPF_STATE, 256 };

#define PARAM_SPEED_FAC 34
CMachineParameter const paramSpeedFac = { "Speed Factor", "Speed Factor", 0, 3, MPF_STATE, 1 };

#define PARAM_PAN1 35
CMachineParameter const paramPan1 = { "Pan 1", "Pan 1", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN2 36
CMachineParameter const paramPan2 = { "Pan 2", "Pan 2", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN3 37
CMachineParameter const paramPan3 = { "Pan 3", "Pan 3", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN4 38
CMachineParameter const paramPan4 = { "Pan 4", "Pan 4", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN5 39
CMachineParameter const paramPan5 = { "Pan 5", "Pan 5", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN6 40
CMachineParameter const paramPan6 = { "Pan 6", "Pan 6", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN7 41
CMachineParameter const paramPan7 = { "Pan 7", "Pan 7", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN8 42
CMachineParameter const paramPan8 = { "Pan 8", "Pan 8", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN9 43
CMachineParameter const paramPan9 = { "Pan 9", "Pan 9", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN10 44
CMachineParameter const paramPan10 = { "Pan 10", "Pan 10", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN11 45
CMachineParameter const paramPan11 = { "Pan 11", "Pan 11", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN12 46
CMachineParameter const paramPan12 = { "Pan 12", "Pan 12", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN13 47
CMachineParameter const paramPan13 = { "Pan 13", "Pan 13", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN14 48
CMachineParameter const paramPan14 = { "Pan 14", "Pan 14", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN15 49
CMachineParameter const paramPan15 = { "Pan 15", "Pan 15", 0, 256, MPF_STATE, 128 };

#define PARAM_PAN16 50
CMachineParameter const paramPan16 = { "Pan 16", "Pan 16", 0, 256, MPF_STATE, 128 };

#define PARAM_FILTER_TYPE 51
CMachineParameter const paramFilterType = { "Filter Type", "Filter Type", 0, 2, MPF_STATE, 0 };

#define PARAM_FFREQ1 52
CMachineParameter const paramFFreq1 = { "F.Freq 1", "F.Freq 1", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ2 53
CMachineParameter const paramFFreq2 = { "F.Freq 2", "F.Freq 2", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ3 54
CMachineParameter const paramFFreq3 = { "F.Freq 3", "F.Freq 3", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ4 55
CMachineParameter const paramFFreq4 = { "F.Freq 4", "F.Freq 4", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ5 56
CMachineParameter const paramFFreq5 = { "F.Freq 5", "F.Freq 5", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ6 57
CMachineParameter const paramFFreq6 = { "F.Freq 6", "F.Freq 6", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ7 58
CMachineParameter const paramFFreq7 = { "F.Freq 7", "F.Freq 7", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ8 59
CMachineParameter const paramFFreq8 = { "F.Freq 8", "F.Freq 8", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ9 60
CMachineParameter const paramFFreq9 = { "F.Freq 9", "F.Freq 9", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ10 61
CMachineParameter const paramFFreq10 = { "F.Freq 10", "F.Freq 10", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ11 62
CMachineParameter const paramFFreq11 = { "F.Freq 11", "F.Freq 11", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ12 63
CMachineParameter const paramFFreq12 = { "F.Freq 12", "F.Freq 12", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ13 64
CMachineParameter const paramFFreq13 = { "F.Freq 13", "F.Freq 13", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ14 65
CMachineParameter const paramFFreq14 = { "F.Freq 14", "F.Freq 14", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ15 66
CMachineParameter const paramFFreq15 = { "F.Freq 15", "F.Freq 15", 0, 256, MPF_STATE, 128 };

#define PARAM_FFREQ16 67
CMachineParameter const paramFFreq16 = { "F.Freq 16", "F.Freq 16", 0, 256, MPF_STATE, 128 };

#define NUM_PARAMS 68

//============================================================================
//				Parameter list
//============================================================================

CMachineParameter const *pParams[] =
{
	&paramProgramNr,
	&paramLevel1,
	&paramLevel2,
	&paramLevel3,
	&paramLevel4,
	&paramLevel5,
	&paramLevel6,
	&paramLevel7,
	&paramLevel8,
	&paramLevel9,
	&paramLevel10,
	&paramLevel11,
	&paramLevel12,
	&paramLevel13,
	&paramLevel14,
	&paramLevel15,
	&paramLevel16,
	&paramNoTicks,
	&paramAttack1,
	&paramAttack2,
	&paramAttack3,
	&paramAttack4,
	&paramAttack5,
	&paramAttack6,
	&paramAttack7,
	&paramAttack8,
	&paramAttack9,
	&paramAttack10,
	&paramAttack11,
	&paramAttack12,
	&paramAttack13,
	&paramAttack14,
	&paramAttack15,
	&paramAttack16,
	&paramSpeedFac,
	&paramPan1,
	&paramPan2,
	&paramPan3,
	&paramPan4,
	&paramPan5,
	&paramPan6,
	&paramPan7,
	&paramPan8,
	&paramPan9,
	&paramPan10,
	&paramPan11,
	&paramPan12,
	&paramPan13,
	&paramPan14,
	&paramPan15,
	&paramPan16,
	&paramFilterType,
	&paramFFreq1,
	&paramFFreq2,
	&paramFFreq3,
	&paramFFreq4,
	&paramFFreq5,
	&paramFFreq6,
	&paramFFreq7,
	&paramFFreq8,
	&paramFFreq9,
	&paramFFreq10,
	&paramFFreq11,
	&paramFFreq12,
	&paramFFreq13,
	&paramFFreq14,
	&paramFFreq15,
	&paramFFreq16
};

static inline void _byteswap(short& x) {
#ifdef __BIG_ENDIAN__
  x = ((x >> 8) & 0x00ff) + ((x << 8) & 0xff00);
#endif
}

struct ELEM
{
	short				level;
	short				attack;
	short				pan;
	short				ffreq;

  void byteswap() {
    _byteswap(level);
    _byteswap(attack);
    _byteswap(pan);
    _byteswap(ffreq);
  }
};

struct PROG
{
	short				length;				// 0x00ff = length, 0xff00 = last used program nr
	short				speed;
	short				ftype;
	ELEM				elems[NUM_STEPS];

  void byteswap() {
    _byteswap(length);
    _byteswap(speed);
    _byteswap(ftype);
    for(int i=0;i<NUM_STEPS;i++) {
      elems[i].byteswap();
    }
  }
};

//============================================================================
//				Machine info
//============================================================================
CMachineInfo const MacInfo (
	MI_VERSION,
	0x0100,
	EFFECT,
	sizeof pParams / sizeof *pParams,
	pParams,
#ifdef _DEBUG
	MAC_NAME " (Debug)",
#else
	MAC_NAME,
#endif
	MAC_NAME,
	MAC_AUTHOR " on " __DATE__,
	"Command Help",
	4
);

//============================================================================
//				Machine
//============================================================================
class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();
	virtual void Init();
	virtual void Stop();
	virtual void Command();
	virtual void ParameterTweak(int par, int val);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void SequencerTick();
	virtual void SeqTick(int channel, int note, int ins, int cmd, int val);
	virtual void Work(float *psamplesleft, float* psamplesright, int numsamples, int numtracks);
	virtual void PutData(void * pData);
	virtual void GetData(void * pData);
	virtual int GetDataSize();

	// Returns the sampling rate
	inline int GetSamplingRate()
	{
		return pCB->GetSamplingRate();
	}

	// Returns the tick length
	inline int GetTickLength()
	{
		return pCB->GetTickLength();
	}

	// Sets program number
	void SetProgramNr(int nr);

	// Returns program number
	inline int GetProgramNr() { return Vals[PARAM_PROGRAM_NR]; }

	// Makes a step
	inline void DoStep(int sr)
	{
		float ms = (float) Vals[PARAM_ATTACK1 + m_pos] * 0.128f + 5.0f;
		int len = (int) ((float) sr * ms * 0.001f);
		m_level.SetLength(len);
		m_level.SetTarget((float) Vals[PARAM_LEVEL1 + m_pos] * 0.00390625f);
		m_pan.SetLength(len);
		m_pan.SetTarget((float) Vals[PARAM_PAN1 + m_pos] * 0.00390625f);
		m_ffreq.SetLength(len);
		m_ffreq.SetTarget((float) Vals[PARAM_FFREQ1 + m_pos] * 0.00390625f);
	}

	// Makes a step and steps to next step :)
	inline void NextStep()
	{
		m_remainer = pCB->GetTickLength() / SPEED_FACTORS[Vals[PARAM_SPEED_FAC]];
		m_remainer++;
		DoStep(pCB->GetSamplingRate());
		++m_pos;
		while (m_pos >= Vals[PARAM_NO_TICKS])
		{
			m_pos -= Vals[PARAM_NO_TICKS];
		}
	}

	// 1 Pole Lowpass
	inline float filter1PoleLP(int c, float in, float x)
	{
		float p = (1.0f - x) * (1.0f - x);
		m_out[c][0] = (1.0f - p) * in + p * m_out[c][0];
		return m_out[c][0];
	}

	// 1 Pole Highpass
	inline float filter1PoleHP(int c, float in, float x)
	{
		float p = x * x;
		m_out[c][0] = (p - 1.0f) * in - p * m_out[c][0];
		return m_out[c][0];
	}

public:

	// Programs
	PROG				*m_programs;

	// Program position
	int								m_pos;

	// Inertias
	Inertia				m_level;
	Inertia m_pan;
	Inertia m_ffreq;

	// Remainer
	int								m_remainer;

	// Filter buffers
	float				m_in[2][2];
	float				m_out[2][2];
};

PSYCLE__PLUGIN__INSTANTIATOR("slicit", mi, MacInfo)

//////////////////////////////////////////////////////////////////////
//
//				Constructor
//
//////////////////////////////////////////////////////////////////////

mi::mi()
{
	Vals = new int[MacInfo.numParameters];
	m_programs = new PROG[NUM_PROGRAMS];
	Init();
}

//////////////////////////////////////////////////////////////////////
//
//				Destructor
//
//////////////////////////////////////////////////////////////////////

mi::~mi()
{
	Stop();
	delete[] m_programs;
	delete[] Vals;
}

//////////////////////////////////////////////////////////////////////
//
//				SetProgramNr
//
//////////////////////////////////////////////////////////////////////

void mi::SetProgramNr(int nr)
{
	Vals[PARAM_PROGRAM_NR] = nr;
	Vals[PARAM_NO_TICKS] = m_programs[nr].length;
	Vals[PARAM_SPEED_FAC] = m_programs[nr].speed;
	Vals[PARAM_FILTER_TYPE] = m_programs[nr].ftype;
	for (int i = 0; i < NUM_STEPS; i++)
	{
		Vals[PARAM_LEVEL1 + i] = (int) m_programs[nr].elems[i].level;
		Vals[PARAM_ATTACK1 + i] = (int) m_programs[nr].elems[i].attack;
		Vals[PARAM_PAN1 + i] = (int) m_programs[nr].elems[i].pan;
		Vals[PARAM_FFREQ1 + i] = (int) m_programs[nr].elems[i].ffreq;
	}
}

//////////////////////////////////////////////////////////////////////
//
//				PutData
//
//////////////////////////////////////////////////////////////////////
void mi::PutData(void * pData)
{
	memcpy(m_programs, pData, GetDataSize());
	for(int i=0;i<16;i++) { m_programs[i].byteswap();}
	int nr = (int) (m_programs[0].length & 0xff00) >> 8;
	m_programs[0].length &= (short) 0xff;
	SetProgramNr(nr);
}

//////////////////////////////////////////////////////////////////////
//
//				GetData
//
//////////////////////////////////////////////////////////////////////
void mi::GetData(void * pData)
{
	m_programs[0].length |= (short) (Vals[PARAM_PROGRAM_NR] << 8);
	for(int i=0;i<16;i++) { m_programs[i].byteswap();}
	memcpy(pData, m_programs, GetDataSize());
	for(int i=0;i<16;i++) { m_programs[i].byteswap();}
	m_programs[0].length &= (short) 0xff;
}

//////////////////////////////////////////////////////////////////////
//
//				GetDataSize
//
//////////////////////////////////////////////////////////////////////
int mi::GetDataSize()
{
	return sizeof(PROG) * 16;
}

//////////////////////////////////////////////////////////////////////
//
//				Init
//
//////////////////////////////////////////////////////////////////////

void mi::Init()
{
	for (int i = 0; i < NUM_PROGRAMS; i++)
	{
		m_programs[i].length = NUM_STEPS;
		m_programs[i].speed = 1;
		m_programs[i].ftype = FTYPE_OFF;
		for (int j = 0; j < NUM_STEPS; j++)
		{
			m_programs[i].elems[j].level = 256;
			m_programs[i].elems[j].attack = 256;
			m_programs[i].elems[j].pan = 128;
			m_programs[i].elems[j].ffreq = 128;
		}
	}
	SetProgramNr(0);
	Stop();
}

//////////////////////////////////////////////////////////////////////
//
//				Stop
//
//////////////////////////////////////////////////////////////////////

void mi::Stop()
{
	m_pos = 0;
	m_remainer = 44100 / SPEED_FACTORS[Vals[PARAM_SPEED_FAC]];
	DoStep(44100);
	m_level.Reset();
	m_pan.Reset();
	m_ffreq.Reset();
	m_in[0][0] = 0.0f; m_in[0][1] = 0.0f;
	m_in[1][0] = 0.0f; m_in[1][1] = 0.0f;
	m_out[0][0] = 0.0f; m_out[0][1] = 0.0f;
	m_out[1][0] = 0.0f; m_out[1][1] = 0.0f;
}

//////////////////////////////////////////////////////////////////////
//
//				Command
//
//////////////////////////////////////////////////////////////////////

void mi::Command()
{
	pCB->MessBox(
		"Slicit\n\n"
		"Commands : None, so far\n\n"
		"Tweaks : What ever you like, BUT!\n\n"
		"Tweaking parameter 0 will allow you to switch\n"
		"between programs in a song\n\n"
		"Greetz to all psycle doods!\n\n"
		"---------------------------\n"
		"druttis@darkface.pp.se\n",
		MAC_AUTHOR " " MAC_NAME,
		0
	);
}

//////////////////////////////////////////////////////////////////////
//
//				ParameterTweak
//
//////////////////////////////////////////////////////////////////////

void mi::ParameterTweak(int par, int val)
{
	Vals[par] = val;

	switch (par)
	{
		case PARAM_PROGRAM_NR:
			SetProgramNr(val);
			break;
		case PARAM_LEVEL1:
		case PARAM_LEVEL2:
		case PARAM_LEVEL3:
		case PARAM_LEVEL4:
		case PARAM_LEVEL5:
		case PARAM_LEVEL6:
		case PARAM_LEVEL7:
		case PARAM_LEVEL8:
		case PARAM_LEVEL9:
		case PARAM_LEVEL10:
		case PARAM_LEVEL11:
		case PARAM_LEVEL12:
		case PARAM_LEVEL13:
		case PARAM_LEVEL14:
		case PARAM_LEVEL15:
		case PARAM_LEVEL16:
			m_programs[GetProgramNr()].elems[par - PARAM_LEVEL1].level = (short) val;
			break;
		case PARAM_NO_TICKS:
			m_programs[GetProgramNr()].length = (short) val;
			break;
		case PARAM_ATTACK1:
		case PARAM_ATTACK2:
		case PARAM_ATTACK3:
		case PARAM_ATTACK4:
		case PARAM_ATTACK5:
		case PARAM_ATTACK6:
		case PARAM_ATTACK7:
		case PARAM_ATTACK8:
		case PARAM_ATTACK9:
		case PARAM_ATTACK10:
		case PARAM_ATTACK11:
		case PARAM_ATTACK12:
		case PARAM_ATTACK13:
		case PARAM_ATTACK14:
		case PARAM_ATTACK15:
		case PARAM_ATTACK16:
			m_programs[GetProgramNr()].elems[par - PARAM_ATTACK1].attack = (short) val;
			break;
		case PARAM_SPEED_FAC:
			m_programs[GetProgramNr()].speed = (short) val;
			break;
		case PARAM_PAN1:
		case PARAM_PAN2:
		case PARAM_PAN3:
		case PARAM_PAN4:
		case PARAM_PAN5:
		case PARAM_PAN6:
		case PARAM_PAN7:
		case PARAM_PAN8:
		case PARAM_PAN9:
		case PARAM_PAN10:
		case PARAM_PAN11:
		case PARAM_PAN12:
		case PARAM_PAN13:
		case PARAM_PAN14:
		case PARAM_PAN15:
		case PARAM_PAN16:
			m_programs[GetProgramNr()].elems[par - PARAM_PAN1].pan = (short) val;
			break;
		case PARAM_FILTER_TYPE:
			m_programs[GetProgramNr()].ftype = (short) val;
			break;
		case PARAM_FFREQ1:
		case PARAM_FFREQ2:
		case PARAM_FFREQ3:
		case PARAM_FFREQ4:
		case PARAM_FFREQ5:
		case PARAM_FFREQ6:
		case PARAM_FFREQ7:
		case PARAM_FFREQ8:
		case PARAM_FFREQ9:
		case PARAM_FFREQ10:
		case PARAM_FFREQ11:
		case PARAM_FFREQ12:
		case PARAM_FFREQ13:
		case PARAM_FFREQ14:
		case PARAM_FFREQ15:
		case PARAM_FFREQ16:
			m_programs[GetProgramNr()].elems[par - PARAM_FFREQ1].ffreq = (short) val;
			break;
	}
}

//////////////////////////////////////////////////////////////////////
//
//				DescribeValue
//
//////////////////////////////////////////////////////////////////////

bool mi::DescribeValue(char* txt,int const param, int const value) {

	switch (param)
	{
		case PARAM_PROGRAM_NR:
			sprintf(txt, "%d", value);
			break;
		case PARAM_LEVEL1:
		case PARAM_LEVEL2:
		case PARAM_LEVEL3:
		case PARAM_LEVEL4:
		case PARAM_LEVEL5:
		case PARAM_LEVEL6:
		case PARAM_LEVEL7:
		case PARAM_LEVEL8:
		case PARAM_LEVEL9:
		case PARAM_LEVEL10:
		case PARAM_LEVEL11:
		case PARAM_LEVEL12:
		case PARAM_LEVEL13:
		case PARAM_LEVEL14:
		case PARAM_LEVEL15:
		case PARAM_LEVEL16:
			if (m_programs[GetProgramNr()].length < param - PARAM_LEVEL1 +1) {
				sprintf(txt, "------");
			}
			else {
				sprintf(txt, "%.2f %%", (float) (value * 0.390625f));
				
			}
			break;
		case PARAM_NO_TICKS:
			sprintf(txt, "%d", value);
			break;
		case PARAM_PAN1:
		case PARAM_PAN2:
		case PARAM_PAN3:
		case PARAM_PAN4:
		case PARAM_PAN5:
		case PARAM_PAN6:
		case PARAM_PAN7:
		case PARAM_PAN8:
		case PARAM_PAN9:
		case PARAM_PAN10:
		case PARAM_PAN11:
		case PARAM_PAN12:
		case PARAM_PAN13:
		case PARAM_PAN14:
		case PARAM_PAN15:
		case PARAM_PAN16:
			if (m_programs[GetProgramNr()].length < param - PARAM_PAN1 +1) {
				sprintf(txt, "------");
			}
			else if (value < 128)
			{
				sprintf(txt, "]> %.2f %%", (float) (128 - value) / 1.28f);
			}
			else if (value > 128)
			{
				sprintf(txt, "%.2f %% <[", (float) (value - 128) / 1.28f);
			}
			else
			{
				sprintf(txt, "[> center <]");
			}
			break;
		case PARAM_SPEED_FAC:
			sprintf(txt, "x %d", SPEED_FACTORS[value]);
			break;
		case PARAM_ATTACK1:
		case PARAM_ATTACK2:
		case PARAM_ATTACK3:
		case PARAM_ATTACK4:
		case PARAM_ATTACK5:
		case PARAM_ATTACK6:
		case PARAM_ATTACK7:
		case PARAM_ATTACK8:
		case PARAM_ATTACK9:
		case PARAM_ATTACK10:
		case PARAM_ATTACK11:
		case PARAM_ATTACK12:
		case PARAM_ATTACK13:
		case PARAM_ATTACK14:
		case PARAM_ATTACK15:
		case PARAM_ATTACK16:
			if (m_programs[GetProgramNr()].length < param - PARAM_ATTACK1 +1) {
				sprintf(txt, "------");
			}
			else {
				sprintf(txt, "%.2f ms", float(value) * 0.125f + 5.0f);				// 5.0 - 37 ms
			}
			break;
		case PARAM_FILTER_TYPE:
			sprintf(txt, "%s", FTYPE_STRING[value]);
			break;
		case PARAM_FFREQ1:
		case PARAM_FFREQ2:
		case PARAM_FFREQ3:
		case PARAM_FFREQ4:
		case PARAM_FFREQ5:
		case PARAM_FFREQ6:
		case PARAM_FFREQ7:
		case PARAM_FFREQ8:
		case PARAM_FFREQ9:
		case PARAM_FFREQ10:
		case PARAM_FFREQ11:
		case PARAM_FFREQ12:
		case PARAM_FFREQ13:
		case PARAM_FFREQ14:
		case PARAM_FFREQ15:
		case PARAM_FFREQ16:
			if (m_programs[GetProgramNr()].length < param - PARAM_FFREQ1 +1) {
				sprintf(txt, "------");
			}
			else {
				sprintf(txt, "%.2f Hz", (float) pCB->GetSamplingRate() * 0.5f * (float) value * 0.00390625);
			}
			break;
		default:
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//
//				SequencerTick
//
//////////////////////////////////////////////////////////////////////

void mi::SequencerTick()
{
	//				Insert code here for effects.
	NextStep();
}

//////////////////////////////////////////////////////////////////////
//
//				SequencerTick
//
//////////////////////////////////////////////////////////////////////

void mi::SeqTick(int channel, int note, int ins, int cmd, int val)
{
	//				Insert code here for synths.
}

//////////////////////////////////////////////////////////////////////
//
//				Work
//
//////////////////////////////////////////////////////////////////////

void mi::Work(float *psamplesleft, float* psamplesright, int numsamples, int numtracks)
{
	int nsamp;
	float level;
	float pan;
	float ffreq;

	--psamplesleft;
	--psamplesright;


	do
	{
		nsamp = m_level.Clip(numsamples);
		nsamp = m_pan.Clip(nsamp);
		nsamp = m_ffreq.Clip(nsamp);

		int tmp = nsamp;

		if (m_remainer < nsamp)
		{
			nsamp = m_remainer;
		}
		m_remainer -= nsamp;
		if (m_remainer <= 0)
		{
			NextStep();
			nsamp = (tmp < m_remainer ? tmp : m_remainer);
		}

		numsamples -= nsamp;

		switch (Vals[PARAM_FILTER_TYPE])
		{
		case FTYPE_OFF:
			do
			{
				level = m_level.Next();
				pan = m_pan.Next();
				*++psamplesleft *= (level - level * (pan * 2.0f - 1.0f));
				*++psamplesright *= (level + level * (pan * 2.0f - 1.0f));
			}
			while (--nsamp);
			break;
		case FTYPE_1P_LP:
			do
			{
				level = m_level.Next();
				pan = m_pan.Next();
				ffreq = m_ffreq.Next();
				++psamplesleft;
				++psamplesright;
				*psamplesleft = filter1PoleLP(0, *psamplesleft, ffreq);
				*psamplesleft *= (level - level * (pan * 2.0f - 1.0f));
				*psamplesright = filter1PoleLP(1, *psamplesright, ffreq);
				*psamplesright *= (level + level * (pan * 2.0f - 1.0f));
			}
			while (--nsamp);
			break;
		case FTYPE_1P_HP:
			do
			{
				level = m_level.Next();
				pan = m_pan.Next();
				ffreq = m_ffreq.Next();
				++psamplesleft;
				++psamplesright;
				*psamplesleft = filter1PoleHP(0, *psamplesleft, ffreq);
				*psamplesleft *= (level - level * (pan * 2.0f - 1.0f));
				*psamplesright = filter1PoleHP(1, *psamplesright, ffreq);
				*psamplesright *= (level + level * (pan * 2.0f - 1.0f));
			}
			while (--nsamp);
			break;
		}
	}
	while (numsamples);
}
}