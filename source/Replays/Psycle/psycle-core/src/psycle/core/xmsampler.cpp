// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "fileio.h"
#include "xmsampler.h"
#include "xminstrument.h"
#include "song.h"
#include "cpu_time_clock.hpp"
#include <psycle/helpers/math.hpp>
#include <psycle/helpers/dsp.hpp>
#include <universalis/stdlib/cstdint.hpp>
#include <algorithm>

namespace psycle { namespace core {

using namespace helpers;
using namespace helpers::math;

std::string XMSampler::_psName = "Sampulse";

const int XMSampler::Voice::m_FineSineData[256] = {
	0,  2,  3,  5,  6,  8,  9, 11, 12, 14, 16, 17, 19, 20, 22, 23,
	24, 26, 27, 29, 30, 32, 33, 34, 36, 37, 38, 39, 41, 42, 43, 44,
	45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59,
	59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60,
	59, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46,
	45, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 30, 29, 27, 26,
	24, 23, 22, 20, 19, 17, 16, 14, 12, 11,  9,  8,  6,  5,  3,  2,
	0, -2, -3, -5, -6, -8, -9,-11,-12,-14,-16,-17,-19,-20,-22,-23,
	-24,-26,-27,-29,-30,-32,-33,-34,-36,-37,-38,-39,-41,-42,-43,-44,
	-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-56,-57,-58,-59,
	-59,-60,-60,-61,-61,-62,-62,-62,-63,-63,-63,-64,-64,-64,-64,-64,
	-64,-64,-64,-64,-64,-64,-63,-63,-63,-62,-62,-62,-61,-61,-60,-60,
	-59,-59,-58,-57,-56,-56,-55,-54,-53,-52,-51,-50,-49,-48,-47,-46,
	-45,-44,-43,-42,-41,-39,-38,-37,-36,-34,-33,-32,-30,-29,-27,-26,
	-24,-23,-22,-20,-19,-17,-16,-14,-12,-11, -9, -8, -6, -5, -3, -2
};

const int XMSampler::Voice::m_FineRampDownData[256] = {
	64, 63, 63, 62, 62, 61, 61, 60, 60, 59, 59, 58, 58, 57, 57, 56,
	56, 55, 55, 54, 54, 53, 53, 52, 52, 51, 51, 50, 50, 49, 49, 48,
	48, 47, 47, 46, 46, 45, 45, 44, 44, 43, 43, 42, 42, 41, 41, 40,
	40, 39, 39, 38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 33, 33, 32,
	32, 31, 31, 30, 30, 29, 29, 28, 28, 27, 27, 26, 26, 25, 25, 24,
	24, 23, 23, 22, 22, 21, 21, 20, 20, 19, 19, 18, 18, 17, 17, 16,
	16, 15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10,  9,  9,  8,
	8,  7,  7,  6,  6,  5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  0,
	0, -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -6, -6, -7, -7, -8,
	-8, -9, -9,-10,-10,-11,-11,-12,-12,-13,-13,-14,-14,-15,-15,-16,
	-16,-17,-17,-18,-18,-19,-19,-20,-20,-21,-21,-22,-22,-23,-23,-24,
	-24,-25,-25,-26,-26,-27,-27,-28,-28,-29,-29,-30,-30,-31,-31,-32,
	-32,-33,-33,-34,-34,-35,-35,-36,-36,-37,-37,-38,-38,-39,-39,-40,
	-40,-41,-41,-42,-42,-43,-43,-44,-44,-45,-45,-46,-46,-47,-47,-48,
	-48,-49,-49,-50,-50,-51,-51,-52,-52,-53,-53,-54,-54,-55,-55,-56,
	-56,-57,-57,-58,-58,-59,-59,-60,-60,-61,-61,-62,-62,-63,-63,-64
};
const int XMSampler::Voice::m_FineSquareTable[256] = {
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,
	-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64 ,-64
};


// Random wave table (randomly choosen values. Not official)
const int XMSampler::Voice::m_RandomTable[256] = {
	48,-64,-21, 45, 51, 20,-32,-57, 62, 13,-35,-43,-33,-16, -8,-48,
	8, 36, 52, -3, 58,-34,-31,-20,  5,-30, 32, 54, -9,-19, -6,-38,
	-11, 43, 10,-47,  2, 53, 11,-56,  3, 55,  9,-44,-15,  4,-63, 59,
	21,-17, 44, -2,-25,-36, 12,-14, 56, 61, 42,-50,-46, 49,-27,-45,
	30, 63,-28, 29, 33, 22,-41, 57, 47, 19,-51,-54,-42,-22, -7,-61,
	14, 25, 34, -4, 40,-49,-40,-26,  7,-39, 24, 37, -10,-24, -5,-53,
	-12, 27, 16,-59,  0, 35, 17, 50,  1, 38, 15,-55,-18,  6, 60, 41,
	23,-23, 28, -1,-29,-52, 18,-13, 39, 46, 26,-62,-58, 31,-37,-59,
	30, 63,-28, 29, 33, 22,-41, 57, 47, 19,-51,-54,-42,-22, -7,-61,
	21,-17, 44, -2,-25,-36, 12,-14, 56, 61, 42,-50,-46, 49,-27,-45,
	14, 25, 34, -4, 40,-49,-40,-26,  7,-39, 24, 37, -10,-24, -5,-53,
	-11, 43, 10,-47,  2, 53, 11,-56,  3, 55,  9,-44,-15,  4,-63, 59,
	-12, 27, 16,-59,  0, 35, 17, 50,  1, 38, 15,-55,-18,  6, 60, 41,
	8, 36, 52, -3, 58,-34,-31,-20,  5,-30, 32, 54, -9,-19, -6,-38,
	23,-23, 28, -1,-29,-52, 18,-13, 39, 46, 26,-62,-58, 31,-37,-59,
	48,-64,-21, 45, 51, 20,-32,-57, 62, 13,-35,-43,-33,-16, -8,-48,
};


// calculated table from the following formula:
// period =  pow(2.0,5.0-double(note)/12.0) * 1712;
// being 5 = the middle octave (on PC), 1712 the middle C period on PC
// (on Amiga it was 428, which was multiplied by 4 on PC, to add fine pitch slide).
const float XMSampler::AmigaPeriod[XMInstrument::NOTE_MAP_SIZE] = {
	54784.00f,	51709.21f,	48806.99f,	46067.67f,	43482.09f,	41041.62f,	38738.13f,	36563.93f,	34511.76f,	32574.76f,	30746.48f,	29020.81f, //Oct 0
	27392.00f,	25854.61f,	24403.50f,	23033.83f,	21741.04f,	20520.81f,	19369.07f,	18281.97f,	17255.88f,	16287.38f,	15373.24f,	14510.41f, //Oct 1
	13696.00f,	12927.30f,	12201.75f,	11516.92f,	10870.52f,	10260.41f,	9684.53f,	9140.98f,	8627.94f,	8143.69f,	7686.62f,	7255.20f, //Oct 2
	6848.00f,	6463.65f,	6100.87f,	5758.46f,	5435.26f,	5130.20f,	4842.27f,	4570.49f,	4313.97f,	4071.85f,	3843.31f,	3627.60f, //Oct 3
	3424.00f,	3231.83f,	3050.44f,	2879.23f,	2717.63f,	2565.10f,	2421.13f,	2285.25f,	2156.98f,	2035.92f,	1921.66f,	1813.80f, //Oct 4
	1712.00f,	1615.91f,	1525.22f,	1439.61f,	1358.82f,	1282.55f,	1210.57f,	1142.62f,	1078.49f,	1017.96f,	960.828f,	906.900f, //Oct 5 (middle)
	856.000f,	807.956f,	762.609f,	719.807f,	679.408f,	641.275f,	605.283f,	571.311f,	539.246f,	508.981f,	480.414f,	453.450f, //Oct 6
	428.000f,	403.978f,	381.305f,	359.904f,	339.704f,	320.638f,	302.642f,	285.656f,	269.623f,	254.490f,	240.207f,	226.725f, //Oct 7
	214.000f,	201.989f,	190.652f,	179.952f,	169.852f,	160.319f,	151.321f,	142.828f,	134.812f,	127.245f,	120.103f,	113.363f, //Oct 8
	107.000f,	100.995f,	95.3262f,	89.9759f,	84.9259f,	80.1594f,	75.6604f,	71.4139f,	67.4058f,	63.6226f,	60.0517f,	56.6813f //Oct 9
};
#if 0
// The original table, which takes the lower octave values and multiplies them by two,  
// failing to take care of the roundings of the values.
const int XMSampler::AmigaPeriod[XMInstrument::NOTE_MAP_SIZE] = {
	54784, 51712, 48768, 46080, 43392, 40960, 38656, 36480, 34432, 32512, 30720, 29008,
	27392, 25856, 24384, 23040, 21696, 20480, 19328, 18240, 17216, 16256, 15360, 14504,
	13696, 12928, 12192, 11520, 10848, 10240,  9664,  9120,  8608,  8128,  7680,  7252,
	6848,   6464,  6096,  5760,  5424,  5120,  4832,  4560,  4304,  4064,  3840,  3626,
	3424,   3232,  3048,  2880,  2712,  2560,  2416,  2280,  2152,  2032,  1920,  1813,
	1712,   1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906,
	856,     808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453,
	428,     404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226,
	214,     202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
	107,     101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56
};
#endif

//////////////////////////////////////////////////////////////////////////
// XMSampler::WaveDataController Implementation
void XMSampler::WaveDataController::Init(const XMInstrument::WaveData* const wave, const int layer)
{
	m_Layer = layer;
	m_pWave = wave;
	m_Position=0;
	m_Speed=0;
	m_Playing=false;

	m_pL = const_cast<short *>(wave->pWaveDataL());
	m_pR = const_cast<short *>(wave->pWaveDataR());

	_stereo=wave->IsWaveStereo();
	_length=wave->WaveLength();

	if ( SustainLoopType() != XMInstrument::WaveData::LoopType::DO_NOT)
	{
		m_CurrentLoopType = SustainLoopType();
		m_CurrentLoopStart = SustainLoopStart();
		m_CurrentLoopEnd = SustainLoopEnd();

	} else {
		m_CurrentLoopType = LoopType();
		m_CurrentLoopStart = LoopStart();
		m_CurrentLoopEnd = LoopEnd();
	}

	CurrentLoopDirection(LoopDirection::FORWARD);
}
void XMSampler::WaveDataController::NoteOff(void)
{
	if ( SustainLoopType() != XMInstrument::WaveData::LoopType::DO_NOT)
	{
		m_CurrentLoopType = LoopType();
		m_CurrentLoopStart = LoopStart();
		m_CurrentLoopEnd = LoopEnd();
	}
}

//////////////////////////////////////////////////////////////////////////
//      XMSampler::EnvelopeController Implementation
void XMSampler::EnvelopeController::Init()
{
	m_Samples = 0;
	m_Mode = EnvelopeMode::TICK;
	m_PositionIndex = 0;
	m_ModulationAmount = 0;
	m_Step = 0;
	m_NextEventSample=0;
	m_Stage = EnvelopeStage::OFF;
	m_sRateDeviation=0;
}

/// NoteOn EnvelopeStage
/// Explanation:
/// First, the stage is set to off. Then, if the envelope is enabled and there are points,
/// we check if it has sustain or normal loop points, enabling the corresponding flags in m_Stage
/// Second, if there are more points in the envelope, let's enable the joy!
void XMSampler::EnvelopeController::NoteOn()
{
	m_Samples = -1;
	m_PositionIndex = -1;
	m_NextEventSample = 0;
	m_Stage = EnvelopeStage::OFF;
	RecalcDeviation();

	// if there are no points, there is nothing to do.
	if ( m_pEnvelope->NumOfPoints() > 0 && m_pEnvelope->IsEnabled())
	{
		m_ModulationAmount = m_pEnvelope->GetValue(0);
		if ( m_pEnvelope->SustainBegin() != XMInstrument::Envelope::INVALID )
		{
			m_Stage = EnvelopeStage::Type(m_Stage | EnvelopeStage::HASSUSTAIN);
		}
		if (m_pEnvelope->LoopStart() != XMInstrument::Envelope::INVALID )
		{
			m_Stage = EnvelopeStage::Type(m_Stage | EnvelopeStage::HASLOOP);
		}

		if(m_pEnvelope->GetTime(1) != XMInstrument::Envelope::INVALID )
		{
			// Since we do not make a CalcStep, m_NextEventSample=0 and the first "Work()" call enters
			// the check of what to do, forcing a new decision.
			m_Stage = EnvelopeStage::Type(m_Stage | EnvelopeStage::DOSTEP);
		}
	} else { m_ModulationAmount = 1.0f; }

}

/// NoteOff EnvelopeStage
// Explanation:
// First we check if the envelope is active. If it is, we release the envelope.
// Then if the envelope state is paused, we check if it was because of a sustain loop and in that case
// we reenable it. The next work call, it will recheck the status and decide what does it need to do.
void XMSampler::EnvelopeController::NoteOff()
{
	if ( m_Stage != EnvelopeStage::OFF && !(m_Stage&EnvelopeStage::RELEASE))
	{
		m_Stage = EnvelopeStage::Type(m_Stage | EnvelopeStage::RELEASE);

		// If we are paused, check why
		if (!(m_Stage&EnvelopeStage::DOSTEP))
		{
			if ( m_Stage&EnvelopeStage::HASSUSTAIN)
			{
				m_Stage = EnvelopeStage::Type(m_Stage | EnvelopeStage::DOSTEP);
				m_Samples=m_NextEventSample-1;
				m_PositionIndex--;
			}
		}
	}
}

void XMSampler::EnvelopeController::Pause()
{
	m_Stage = EnvelopeStage::Type(m_Stage & ~EnvelopeStage::DOSTEP);
}
void XMSampler::EnvelopeController::Continue()
{
	m_Stage = EnvelopeStage::Type(m_Stage | EnvelopeStage::DOSTEP);
}


void XMSampler::EnvelopeController::RecalcDeviation()
{
	if ( m_Mode == EnvelopeMode::TICK )	{
		m_sRateDeviation = (m_pCallbacks->timeInfo().sampleRate() *60) / (24 * m_pCallbacks->timeInfo().bpm());
	} else if ( m_Mode == EnvelopeMode::MILIS ) {
		m_sRateDeviation = m_pCallbacks->timeInfo().sampleRate() / 1000.0f;
	}
}

void XMSampler::EnvelopeController::CalcStep(const int start,const int  end)
{
	const XMInstrument::Envelope::ValueType ystep = (m_pEnvelope->GetValue(end) - m_pEnvelope->GetValue(start));
	const XMInstrument::Envelope::ValueType xstep = (m_pEnvelope->GetTime(end) - m_pEnvelope->GetTime(start));
	RecalcDeviation();
	m_Samples = m_pEnvelope->GetTime(start) * SRateDeviation();
	m_NextEventSample = m_pEnvelope->GetTime(end)* SRateDeviation();
	m_ModulationAmount = m_pEnvelope->GetValue(start);
	if ( xstep != 0) m_Step = ystep / (xstep * SRateDeviation());
	else m_Step=0;
}
void XMSampler::EnvelopeController::SetPositionInSamples(const int samplePos)
{
	int i=0;
	while (m_pEnvelope->GetTime(i) != XMInstrument::Envelope::INVALID)
	{
		if (m_pEnvelope->GetTime(i)* SRateDeviation() > samplePos ) break;
		i++;
	}
	if ( i==0 ) return; //Invalid Envelope. GetTime(0) is either zero or INVALID, and samplePos is positive.
	m_PositionIndex=i-2;
	m_Stage = EnvelopeStage::Type(m_Stage|EnvelopeStage::DOSTEP);
	m_Samples=m_NextEventSample-1; // This forces a recalc when calling work()
	Work();
	m_Samples=samplePos;//and this sets the real position, once all vars are setup.
	//TRACE("ModAmount Before:%f.",m_ModulationAmount);
	m_ModulationAmount+= m_Step*(samplePos-m_pEnvelope->GetTime(m_PositionIndex)* SRateDeviation());
	//TRACE("Set pos to:%d, i=%d,t=%f .ModAmount After:%f\n",samplePos,i,m_pEnvelope->GetTime(i)* SRateDeviation(),m_ModulationAmount);
	//TRACE("SET: Idx:=%d, Step:%f .Amount:%f, smp:%d,psmp:%d\n",m_PositionIndex,m_Step,m_ModulationAmount,samplePos,m_pEnvelope->GetTime(m_PositionIndex));
}
int XMSampler::EnvelopeController::GetPositionInSamples() const
{
	//TRACE("Requested Pos:%d. Idx:%d, Current Amount:%f\n",m_Samples,m_PositionIndex,m_ModulationAmount);
	//TRACE("-GET-Idx:%d, Step:%f, Current Amount:%f\n",m_PositionIndex,m_Step,m_ModulationAmount);
	return m_Samples;
}


//////////////////////////////////////////////////////////////////////////
// XMSampler::Voice  Implementation
XMSampler::Voice::~Voice()
{
}
void XMSampler::Voice::Reset()
{
	m_ChannelNum = -1;
	m_pChannel = NULL;

	_instrument = -1;
	m_pInstrument = NULL;

	m_AmplitudeEnvelope.Init();
	m_FilterEnvelope.Init();
	m_PitchEnvelope.Init();
	m_PanEnvelope.Init();

	m_FilterIT.Reset();
	m_FilterClassic.Reset();
	m_Filter = &m_FilterIT;
	m_CutOff = 127;
	m_Ressonance = 0;
	_coModify = 0;


	m_bPlay =false;
	m_Background = false;
	m_Stopping = false;
	m_Period=0;
	m_Note = notetypes::empty;
	m_Volume = 128;
	m_RealVolume = 1.0f;

	m_PanFactor=0.5f;
	m_PanRange=1;
	m_Surround=false;

	ResetEffects();
}
void XMSampler::Voice::DisposeResampleData(helpers::dsp::resampler& resampler) 
{
	if (resampler_data != NULL ) {
		resampler.DisposeResamplerData(resampler_data);
		resampler_data = NULL;
	}
}
void XMSampler::Voice::RecreateResampleData(helpers::dsp::resampler& resampler)
{
	if (IsPlaying()) {
        resampler_data = resampler.GetResamplerData();
        resampler.UpdateSpeed(resampler_data,static_cast<double>(m_WaveDataController.Speed())/4294967296.0);
	}
}


void XMSampler::Voice::ResetEffects()
{
	m_Slide2NoteDestPeriod=0;
	m_PitchSlideSpeed=0;

	m_VolumeFadeSpeed = 0;
	m_VolumeFadeAmount = 1;

	m_VolumeSlideSpeed=0;

	m_VibratoSpeed=0;
	m_VibratoDepth=0;
	m_VibratoPos=0;
	m_VibratoAmount =0.0;

	m_TremoloSpeed=0;
	m_TremoloDepth=0;
	m_TremoloAmount =0.0f;
	m_TremoloPos=0;

	// Panbrello
	m_PanbrelloSpeed=0;
	m_PanbrelloDepth=0;
	m_PanbrelloAmount=0.0f;
	m_PanbrelloPos=0;
	m_PanbrelloRandomCounter = 0;

	// Tremor
	m_TremorOnTicks = 0;
	m_TremorOffTicks = 0;
	m_TremorTickChange = 0;
	m_bTremorMute = false;

	m_AutoVibratoAmount = 0.0;
	m_AutoVibratoPos = 0;
	m_AutoVibratoDepth = 0;

	m_RetrigTicks=0;

}
void XMSampler::Voice::VoiceInit(const XMInstrument & _inst, int channelNum, int instrumentNum)
{
	IsBackground(false);
	IsStopping(false);
	srand(0);
	m_ChannelNum = channelNum;
	pChannel(&pSampler()->rChannel(channelNum));
	InstrumentNum(instrumentNum);
	m_pInstrument = &_inst;

	// Envelopes
	m_AmplitudeEnvelope.Init(_inst.AmpEnvelope(),pSampler()->pCallbacks());
	m_PanEnvelope.Init(_inst.PanEnvelope(),pSampler()->pCallbacks());
	m_PitchEnvelope.Init(_inst.PitchEnvelope(),pSampler()->pCallbacks());
	m_FilterEnvelope.Init(_inst.FilterEnvelope(),pSampler()->pCallbacks());

	m_Filter->Init(pSampler()->pCallbacks()->timeInfo().sampleRate());

	if (_inst.FilterCutoff() < 127 || _inst.FilterResonance() > 0)
	{
		FilterType(_inst.FilterType());
		//\todo: add the missing  Random options
		#if 0
		if (_inst.RandomCutoff())
		{
			CutOff(_inst.FilterCutoff()* (float)rand() * _inst.RandomCutoff() / 3276800.0f);
		}
		else
		#endif
 		{
			CutOff(_inst.FilterCutoff());
		}
		#if 0
		if (_inst.RandomResonance())
		{
			Ressonance(_inst.FilterResonance() * (float)rand()* _inst.RandomResonance() / 3276800.f);
		}
		else
		#endif
		{
			Ressonance(_inst.FilterResonance());
		}
	}
	else if ( rChannel().Cutoff() < 127 || rChannel().Ressonance() > 0)
	{
		FilterType(rChannel().FilterType());
		CutOff(rChannel().Cutoff());
		Ressonance(rChannel().Ressonance());
	}
	//TODO: Study this situation for precedence.
	else if (_inst.FilterEnvelope().IsEnabled())
	{
		FilterType(_inst.FilterType());
		CutOff(127);
		Ressonance(0);
	}
	else
	{
		CutOff(127);
		Ressonance(0);
	}

	ResetEffects();

}// XMSampler::Voice::VoiceInit)

void XMSampler::Voice::Work(int numSamples,float * pSamplesL,float * pSamplesR, const dsp::resampler & resampler) {
	float left_output = 0.0f;
	float right_output = 0.0f;

	if (!rInstrument().IsEnabled())
	{
		IsPlaying(false);
		return;
	}
	while (numSamples)
	{
	//////////////////////////////////////////////////////////////////////////
	//  Step 1 : Get the unprocessed wave data.

		m_WaveDataController.Work(&left_output, &right_output, resampler.work, resampler_data);

	//////////////////////////////////////////////////////////////////////////
	//  Step 2 : Process the Envelopes.

		// Amplitude Envelope
		// Voice::RealVolume() returns the calculated volume out of "WaveData.WaveGlobVol() * Instrument.Volume() * Voice.NoteVolume()"

		float volume = RealVolume() * rChannel().Volume();
		if(m_AmplitudeEnvelope.Envelope().IsEnabled())
		{
			m_AmplitudeEnvelope.Work();
			volume *= m_AmplitudeEnvelope.ModulationAmount();
			if (m_AmplitudeEnvelope.Stage() == EnvelopeController::EnvelopeStage::OFF)
			{
				if ( m_AmplitudeEnvelope.ModulationAmount() <= 0.0f){ IsPlaying(false); return; }
				else if (m_VolumeFadeSpeed == 0.0f) NoteFadeout();
			}
		}
		// Volume Fade Out
		if(m_VolumeFadeSpeed > 0.0f)
		{
			UpdateFadeout();
			if ( m_VolumeFadeAmount <= 0) { IsPlaying(false); return; }
			volume *= m_VolumeFadeAmount;
		}

		// Panning Envelope
		// (actually, the correct word for panning is panoramization. "panning" comes from the diminutive "pan")
		// PanFactor() contains the pan calculated at note start ( pan of note, wave pan, instrument pan, NoteModPan sep, and channel pan)
		float rvol = PanFactor() + m_PanbrelloAmount;

		if(m_PanEnvelope.Envelope().IsEnabled()){
			m_PanEnvelope.Work();
			// PanRange() is a Range delimiter for the envelope, which is set whenever the pan is changed.
			rvol += (m_PanEnvelope.ModulationAmount()*PanRange());
		}

		float lvol=0;
		if ( m_pSampler->PanningMode()== PanningMode::Linear) {
			lvol = (1.0f - rvol);
		} else if ( m_pSampler->PanningMode()== PanningMode::TwoWay) {
			//using std::min;
			lvol = std::min(1.0f, (1.0f - rvol) * 2);
		} else if ( m_pSampler->PanningMode()== PanningMode::EqualPower) {
			//lvol = powf((1.0f-rvol),0.5f); // This is the commonly used one
			lvol = std::log10(((1.0f - rvol)*9.0f)+1.0f); // This is a faster approximation
		}

		// PanningMode::Linear is already on rvol, so we omit the case.
		if ( m_pSampler->PanningMode()== PanningMode::TwoWay) {
			rvol = std::min(1.0f, rvol*2.0f);
		} else if ( m_pSampler->PanningMode()== PanningMode::EqualPower) {
			//rvol = powf(rvol, 0.5f);// This is the commonly used one
			rvol = std::log10((rvol*9.0f)+1.0f); // This is a faster approximation.
		}

		left_output *=  volume;
		right_output *= volume;

		// Filter section
		if (m_Filter->Type() != dsp::F_NONE)
		{
			if(m_FilterEnvelope.Envelope().IsEnabled()){
				m_FilterEnvelope.Work();
				int tmpCO = int(m_CutOff * m_FilterEnvelope.ModulationAmount());
				if (tmpCO < 0) { tmpCO = 0; }
				else if (tmpCO > 127) { tmpCO = 127; }
				m_Filter->Cutoff(tmpCO);
			}
			if ( m_pSampler->UseFilters() )
			{
				if (m_WaveDataController.IsStereo())
				{
					m_Filter->WorkStereo(left_output, right_output);
				}
				else
				{
					left_output = m_Filter->Work(left_output);
				}
			}
		}

		// Picth Envelope. Currently, the pitch envelope Amount is only updated on NewLine().
		if(m_PitchEnvelope.Envelope().IsEnabled()){
			m_PitchEnvelope.Work();
		}


	//////////////////////////////////////////////////////////////////////////
	//  Step 3: Add the processed data to the sampler's buffer.
		if(!m_WaveDataController.IsStereo()){
			//Monoaural output. Copy left to right output.
			right_output = left_output;
		}

		if(IsSurround()){
			*pSamplesL++ += left_output;
			*pSamplesR++ -= right_output;
		} else if (!m_pChannel->IsMute()){
			*pSamplesL++ += left_output*lvol;
			*pSamplesR++ += right_output*rvol;
		}
		else
		{
			pSamplesL++;
			pSamplesR++;
		}

		if (!m_WaveDataController.Playing()) {
			IsPlaying(false); return;
		}

		numSamples--;
	}
}
// This one is Tracker-Tick (Mod-Tick).
void XMSampler::Voice::Tick()
{
	UpdateSpeed();
}
void XMSampler::Voice::NewLine()
{
	m_bTremorMute = false;
	m_VibratoAmount = 0;
	if ( IsAutoVibrato()) AutoVibrato();
	UpdateSpeed();
	m_AmplitudeEnvelope.RecalcDeviation();
	m_PanEnvelope.RecalcDeviation();
	m_PitchEnvelope.RecalcDeviation();
	m_FilterEnvelope.RecalcDeviation();
}

void XMSampler::Voice::NoteOn(const uint8_t note,const int16_t playvol,bool reset)
{
	XMInstrument::NotePair pair = rInstrument().NoteToSample(note);
    const SampleList & samples = pSampler()->pCallbacks()->song().m_rWaveLayers;
	int wavelayer = pair.second;
	if ( samples.IsEnabled(wavelayer) == false ) return;

	const XMInstrument::WaveData& wave = samples[wavelayer];
	m_WaveDataController.Init(&wave,wavelayer);
	m_Note = note;
	m_Period=NoteToPeriod(pair.first, false);
	m_NNA = rInstrument().NNA();
	//\todo : add pInstrument().LinesMode
	if (resampler_data != NULL) {
		m_pSampler->Resampler().DisposeResamplerData(resampler_data);
		resampler_data = NULL;
	}
    m_pSampler->Resampler().UpdateSpeed(resampler_data,1.0);
	resampler_data = m_pSampler->Resampler().GetResamplerData();

	ResetVolAndPan(playvol,reset);

	if ( rWave().Wave().IsAutoVibrato())
	{
		m_AutoVibratoPos=0;
		m_AutoVibratoDepth=rWave().Wave().VibratoDepth();
		AutoVibrato();
	}
	//Important, put it after m_PitchEnvelope.NoteOn(); (currently done inside ResetVolAndPan)
	UpdateSpeed();

	m_WaveDataController.Playing(true);
	IsPlaying(true);
}

void XMSampler::Voice::ResetVolAndPan(int16_t playvol,bool reset)
{
	float fpan=0.5f;
	if ( reset)
	{
		if ( playvol != -1)
		{
			Volume(playvol);
		} else {
			Volume(rWave().Wave().WaveVolume());
		}
		// Impulse Tracker panning had the following pan controls. All these are bypassed if
		// a panning command is explicitely put in a channel.
		// Note : m_pChannel->PanFactor() returns the panFactor of the last panning command (if any) or
		// in its absence, the pan position of the channel.
		if ( rWave().Wave().IsSurround() || rChannel().IsSurround()) {
			IsSurround(true);
		}
		else {
			IsSurround(false);
		}
		//\todo :
		//	In a related note, check range of Panbrello.
		if ( rWave().Wave().PanEnabled() ) fpan = rWave().Wave().PanFactor();
		else if ( rInstrument().PanEnabled() ) fpan = rInstrument().Pan();
		else fpan = m_pChannel->PanFactor();
		//NoteModPansep is in the range -32..32, being 8=one step (0..64) each seminote.
		fpan += (m_Note-rInstrument().NoteModPanCenter())*rInstrument().NoteModPanSep()/512.0f;
		fpan += (float)(rand()-16384.0f) * rInstrument().RandomPanning() / 1638400.0f;

		if ( fpan > 1.0f ) fpan = 1.0f;
		else if ( fpan < 0.0f ) fpan = 0.0f;
	}
	else
	{
		Volume(rChannel().LastVoiceVolume());
		fpan = rChannel().LastVoicePanFactor();
		IsSurround(rChannel().IsSurround());
	}
	PanFactor(fpan);

	if(m_AmplitudeEnvelope.Envelope().IsEnabled()){
		m_AmplitudeEnvelope.NoteOn();
		if (m_AmplitudeEnvelope.Envelope().IsCarry() || !reset)
			m_AmplitudeEnvelope.SetPositionInSamples(rChannel().LastAmpEnvelopePosInSamples());
	}

	if(m_PanEnvelope.Envelope().IsEnabled()){
		m_PanEnvelope.NoteOn();
		if (m_PanEnvelope.Envelope().IsCarry() || !reset)
			m_PanEnvelope.SetPositionInSamples(rChannel().LastPanEnvelopePosInSamples());
	}

	if(m_FilterEnvelope.Envelope().IsEnabled()){
		m_FilterEnvelope.NoteOn();
		if (m_FilterEnvelope.Envelope().IsCarry() || !reset)
			m_FilterEnvelope.SetPositionInSamples(rChannel().LastFilterEnvelopePosInSamples());
	}

	if(m_PitchEnvelope.Envelope().IsEnabled()){
		m_PitchEnvelope.NoteOn();
		if (m_PitchEnvelope.Envelope().IsCarry() || !reset)
			m_PitchEnvelope.SetPositionInSamples(rChannel().LastPitchEnvelopePosInSamples());
	}

}

void XMSampler::Voice::NoteOff()
{
	if(!IsPlaying()){
		return;
	}
	IsStopping(true);
	if(m_AmplitudeEnvelope.Envelope().IsEnabled())
	{
		m_AmplitudeEnvelope.NoteOff();
		// IT Type envelopes only do a fadeout() when it reaches the end of the envelope, except if it is looped.
		if ( m_AmplitudeEnvelope.Stage() & EnvelopeController::EnvelopeStage::HASLOOP)
		{
			NoteFadeout();
		}
	} else if ( rInstrument().VolumeFadeSpeed() >0.0f )
	{
		NoteFadeout();
	}else {
		NoteOffFast();
		return;
	}

	m_PanEnvelope.NoteOff();
	m_FilterEnvelope.NoteOff();
	m_PitchEnvelope.NoteOff();
	m_WaveDataController.NoteOff();
}
void XMSampler::Voice::NoteOffFast()
{
	if(!IsPlaying()){
		return;
	}
	IsStopping(true);
	if(m_AmplitudeEnvelope.Envelope().IsEnabled()){
		m_AmplitudeEnvelope.NoteOff();
	}
	// Fade Out Volume
	m_VolumeFadeSpeed = 64.0f;
	m_VolumeFadeAmount = 1.0f;

	m_PanEnvelope.NoteOff();
	m_FilterEnvelope.NoteOff();
	m_PitchEnvelope.NoteOff();
	m_WaveDataController.NoteOff();
}

void XMSampler::Voice::NoteFadeout()
{
	IsStopping(true);
	m_VolumeFadeSpeed = m_pInstrument->VolumeFadeSpeed()/m_pSampler->pCallbacks()->timeInfo().samplesPerTick();
	m_VolumeFadeAmount = 1.0f;
	if ( RealVolume() * rChannel().Volume() == 0.0f ) IsPlaying(false);
	else if ( m_AmplitudeEnvelope.Envelope().IsEnabled() && m_AmplitudeEnvelope.ModulationAmount() == 0.0f) IsPlaying(false);
}
void XMSampler::Voice::UpdateFadeout()
{
	if ( RealVolume() == 0.0f ) IsPlaying(false);
	m_VolumeFadeAmount -= m_VolumeFadeSpeed;
	if( m_VolumeFadeAmount <= 0){
		IsPlaying(false);
	}
}

void XMSampler::Voice::Slide2Note()
{
	if(m_Slide2NoteDestPeriod > m_Period)
	{
		m_Period += m_PitchSlideSpeed;
		if(m_Period > m_Slide2NoteDestPeriod ){
			m_Period = m_Slide2NoteDestPeriod;
		}
		UpdateSpeed();
	}
	else if(m_Slide2NoteDestPeriod < m_Period)
	{
		// m_PitchSlide is signed. (+/-)
		m_Period += m_PitchSlideSpeed;
		if(m_Period < m_Slide2NoteDestPeriod){
			m_Period = m_Slide2NoteDestPeriod;
		}
		UpdateSpeed();
	}
}

void XMSampler::Voice::VolumeSlide()
{
	if(m_VolumeSlideSpeed >0){
		VolumeUp(m_VolumeSlideSpeed);
	} else {
		VolumeDown(m_VolumeSlideSpeed);
	}
}
void XMSampler::Voice::VolumeDown(const int value){
	int vol = Volume() +value;
	if(vol < 0){
		vol = 0;
	}
	Volume(vol);
}
void XMSampler::Voice::VolumeUp(const int value){
	int vol = Volume() +value;
	if(vol > 0x80){
		vol = 0x80;
	}
	Volume(vol);
}

void XMSampler::Voice::AutoVibrato()
{
	int vdelta = GetDelta(rWave().Wave().VibratoType(),m_AutoVibratoPos);

	if(rWave().Wave().VibratoAttack())
	{
		m_AutoVibratoDepth += rWave().Wave().VibratoAttack()<<1;
		if((m_AutoVibratoDepth) > rWave().Wave().VibratoDepth()<<8)
		{
			m_AutoVibratoDepth = rWave().Wave().VibratoDepth()<<8;
		}
	} else {
		m_AutoVibratoDepth = rWave().Wave().VibratoDepth()<<8;
	}

	vdelta = vdelta * (m_AutoVibratoDepth>>8);
	m_AutoVibratoAmount=(double)vdelta / 128.0;
	m_AutoVibratoPos = (m_AutoVibratoPos - (rWave().Wave().VibratoSpeed())) & 0xFF;
	UpdateSpeed();

}

void XMSampler::Voice::Vibrato()
{
	int vdelta = GetDelta(rChannel().VibratoType(),m_VibratoPos);

	vdelta = vdelta * m_VibratoDepth;
	m_VibratoAmount=(double)vdelta / 32.0;
	m_VibratoPos = (m_VibratoPos - m_VibratoSpeed) & 0xFF;
	UpdateSpeed();

}// Vibrato() -------------------------------------

void XMSampler::Voice::Tremolo()
{
	//\todo: verify that final volume doesn't go out of range (Redo RealVolume() ?)
	int vdelta = GetDelta(rChannel().TremoloType(),m_TremoloPos);

	vdelta = (vdelta * m_TremoloDepth);
	m_TremoloAmount = (double)vdelta / 2048.0;
	m_TremoloPos = (m_TremoloPos + m_TremoloSpeed) & 0xFF;


}// Tremolo() -------------------------------------------

void XMSampler::Voice::Panbrello()
{
//Yxy   Panbrello with speed x, depth y.
//The random pan position can be achieved by setting the
//waveform to 3 (ie. a S53 command). In this case *ONLY*, the
//speed actually is interpreted as a delay in frames before
//another random value is found. so Y14 will be a very QUICK
//panbrello, and Y44 will be a slower panbrello.

	//\todo: verify that final pan doesn't go out of range (make a RealPan() similar to RealVolume() ?)
	int vdelta = GetDelta(rChannel().PanbrelloType(),m_PanbrelloPos);

	vdelta = vdelta * m_PanbrelloDepth;
	m_PanbrelloAmount = vdelta / 2048.0f; // 64*16*2
	if (rChannel().PanbrelloType() != XMInstrument::WaveData::WaveForms::RANDOM)
	{
	m_PanbrelloPos = (m_PanbrelloPos + m_PanbrelloSpeed) & 0xFF;
	}
	else if (++m_PanbrelloRandomCounter >= m_PanbrelloSpeed )
	{
		m_PanbrelloPos++;
		m_PanbrelloRandomCounter = 0;
	}

}// Panbrello() -------------------------------------------

void XMSampler::Voice::Tremor()
{
	//\todo: according to Impulse Tracker, this command uses its own counter, so with
	// speed 3, we can specify the command I41 ( x/y > speed), which, with the current implementation, doesn't work,
	if ( pSampler()->CurrentTick() >= m_TremorTickChange )
	{
		if ( m_bTremorMute )
		{
			m_TremorTickChange = pSampler()->CurrentTick()+m_TremorOnTicks;
			m_bTremorMute = false;
		}
		else
		{
			m_TremorTickChange = pSampler()->CurrentTick()+m_TremorOffTicks;
			m_bTremorMute = true;
		}
	}
}

void XMSampler::Voice::Retrig()
{
	if ( pSampler()->CurrentTick()%m_RetrigTicks == 0 )
	{
		NoteOn(m_Note,-1,false);
		}
	}

int XMSampler::Voice::GetDelta(const int wavetype,const int wavepos) const
{
	switch (wavetype)
	{
	case XMInstrument::WaveData::WaveForms::SAWDOWN:
		return m_FineRampDownData[wavepos];
	case XMInstrument::WaveData::WaveForms::SAWUP:
		return m_FineRampDownData[0xFF - wavepos];
	case XMInstrument::WaveData::WaveForms::SQUARE:
		return m_FineSquareTable[wavepos];
	case XMInstrument::WaveData::WaveForms::RANDOM:
		return m_RandomTable[wavepos];
	case XMInstrument::WaveData::WaveForms::SINUS:
	default:
		return m_FineSineData[wavepos];
	}
}

void XMSampler::Voice::UpdateSpeed()
{
	double _period = m_Period;

	if(rChannel().IsArpeggio()){
		_period = rChannel().ArpeggioPeriod();
	}

	if(rChannel().IsGrissando())
	{
		int _note = PeriodToNote(_period);
		_period = NoteToPeriod(_note, false);
	}
	//\todo: Attention, AutoVibrato uses linear slides with IT. This needs to be fixed.
	_period = _period + AutoVibratoAmount() +  VibratoAmount();
	if ( _period > 65535.0) {
		NoteOffFast();
	}
	else if ( _period < 1.0 ) {
		//This behaviour exists in ST3 and IT, and is in fact documented for slide up command in the latter.
		NoteOffFast();
	}
	else {
		const double speed=PeriodToSpeed(_period);
		rWave().Speed(speed);
		m_pSampler->Resampler().UpdateSpeed(resampler_data, speed);
	}
}

double XMSampler::Voice::PeriodToSpeed(int period) const
{
	if(m_pSampler->IsAmigaSlides()){
		// Amiga period mode. Original conversion:
		//	PAL:   7093789.2 / (428*2) = 8287.14 Hz   ( Amiga clock frequency, middle C period and final samplerate)
		//	NSTC:  7159090.5 / (428*2) = 8363.42 Hz   ( *2 is used to convert clock frequency to ticks).
		// in PC, the middle C period is 1712. It was increased by 4 to add extra fine pitch slides.
		// so 1712 *8363 = 14317456, which is used in IT and FT2 (and in ST3, if the value that exists in their doc is a typo).
		// One could also use 7159090.5 /2 *4 = 14318181
		return ( 14317456.0 / period ) * pow(2.0,(m_PitchEnvelope.ModulationAmount()*16.0)/12.0) / (double)pSampler()->pCallbacks()->timeInfo().sampleRate();
	} else {
		// Linear Frequency
		// base_samplerate * 2^((7*12*64 - Period) / (12*64))
		// 12*64 = 12 notes * 64 finetune steps.
		// 7 = 12 - middle_C ( if C-4 is middle_C, then 8*12*64, if C-3, then 9*12*64, etc..)
		return	pow(2.0,
				((5376 - period + m_PitchEnvelope.ModulationAmount()*1024.0)
				/768.0)
			)
			* rWave().Wave().WaveSampleRate() / (double)pSampler()->pCallbacks()->timeInfo().sampleRate();
	}
}

double XMSampler::Voice::NoteToPeriod(const int noteIn, bool correctNote) const
{
	const XMInstrument::WaveData& _wave = m_WaveDataController.Wave();
	int note = (correctNote)? rInstrument().NoteToSample(noteIn).first : noteIn;

	if(m_pSampler->IsAmigaSlides())
	{
		// Amiga Period.
		double c5speed = (double)rWave().Wave().WaveSampleRate()*pow(2.0,(_wave.WaveTune()+(_wave.WaveFineTune()*0.01))/12.0);
		return AmigaPeriod[note]*8363.0/c5speed;
	} else {
		// 9216 = 12octaves*12notes*64fine.
		return 9216 - ((double)(note + _wave.WaveTune()) * 64.0)
			- ((double)(_wave.WaveFineTune()) * 0.64); // 0.64 since the range is +-100 for XMSampler as opposed to +-128 for FT.
	}
}

int XMSampler::Voice::PeriodToNote(const double period) const
{
	const XMInstrument::WaveData& _wave = m_WaveDataController.Wave();

	if(m_pSampler->IsAmigaSlides()){
		// period_t (table) = pow(2.0,5.0-note/12.0) * 1712.0;
		// final_period = period_t*8363.0/ ( _wave.WaveSampleRate()*pow(2.0,(_wave.WaveTune()+(_wave.WaveFineTune()*0.01))/12.0)  )
		// final_period * _wave.WaveSampleRate()*pow(2.0,(_wave.WaveTune()+(_wave.WaveFineTune()*0.01))/12.0) = pow(2.0,5.0-note/12.0) * 1712.0 *8363.0
		// final_period * _wave.WaveSampleRate()/(1712.0 *8363.0)*pow(2.0,(_wave.WaveTune()+(_wave.WaveFineTune()*0.01))/12.0) = pow(2.0,5.0-note/12.0)
		// log2(final_period * _wave.WaveSampleRate()/(1712.0 *8363.0)) + log2(pow(2.0,(_wave.WaveTune()+(_wave.WaveFineTune()*0.01))/12.0)) = 5.0-note/12.0
		// note = 60 - 12*log2(final_period * _wave.WaveSampleRate()/(1712.0 *8363.0)) + (_wave.WaveTune()+(_wave.WaveFineTune()*0.01))/12.0
		int _note = 60 -12*(log(period * _wave.WaveSampleRate()) * 3.3219280948873623478703194294894 /*1/log(2)*/ 
					-23.77127183403184445503933415201/*log2(1.0/(1712.0 *8363.0))*/) + (_wave.WaveTune()+(_wave.WaveFineTune()*0.01))/12.0;
		return _note+12;
	} else {
		// period = ((12.0 * 12.0 * 64.0 - ((double)note + (double)_wave.WaveTune()) * 64.0)
		//	- (_wave.WaveFineTune() *0.01) * 64.0);
		// period / 64.0 = 12.0 * 12.0  - ((double)note + (double)_wave.WaveTune()) - _wave.WaveFineTune()*0.01;
		// note = (int)(12.0 * 12.0  - (double)_wave.WaveTune() - _wave.WaveFineTune() *0.01 - period / 64.0 + 0.5);

		return (int)(144 - (double)_wave.WaveTune() - ((double)_wave.WaveFineTune() *0.01)  - (period *0.015625)); // Apparently,  (int)(x.5) rounds to x+1, so no need for +0.5
	}
}

//////////////////////////////////////////////////////////////////////////
// Channel Class Implementation
//////////////////////////////////////////////////////////////////////////
void XMSampler::Channel::Init()
{
	m_InstrumentNo = 255;
	m_pForegroundVoice = NULL;

	m_Note = notetypes::empty;
	m_Period = 0;

	m_Volume = 1.0f;
	m_ChannelDefVolume = 200;//
	m_LastVoiceVolume = 0;
	m_bMute = false;

	m_PanFactor = 0.5f;
	m_DefaultPanFactor = 100;
	m_LastVoicePanFactor = 0.0f;
	m_bSurround = false;

	m_LastAmpEnvelopePosInSamples=0;
	m_LastPanEnvelopePosInSamples=0;
	m_LastFilterEnvelopePosInSamples=0;
	m_LastPitchEnvelopePosInSamples=0;

	m_bGrissando = false;
	m_VibratoType = XMInstrument::WaveData::WaveForms::SINUS;
	m_TremoloType = XMInstrument::WaveData::WaveForms::SINUS;
	m_PanbrelloType = XMInstrument::WaveData::WaveForms::SINUS;

	m_PanSlideMem = 0;
	m_ChanVolSlideMem = 0;
	m_PitchSlideMem = 0;
	m_TremorMem = 0;
	m_TremorOnTime = 0;
	m_TremorOffTime = 0;
	m_VibratoDepthMem = 0;
	m_VibratoSpeedMem = 0;
	m_TremoloDepthMem = 0;
	m_TremoloSpeedMem = 0;
	m_PanbrelloDepthMem = 0;
	m_PanbrelloSpeedMem = 0;
	m_VolumeSlideMem = 0;
	m_ArpeggioMem = 0;
	m_RetrigMem = 0;
	m_OffsetMem = 0;

	m_EffectFlags = 0;

	m_PitchSlideSpeed = 0;

	m_GlobalVolSlideSpeed = 0.0f;
	m_ChanVolSlideSpeed = 0.0f;
	m_PanSlideSpeed = 0.0f;

	m_TremoloSpeed = 0;
	m_TremoloDepth = 0;
	m_TremoloDelta = 0;
	m_TremoloPos = 0;

	m_PanbrelloSpeed = 0;
	m_PanbrelloDepth = 0;
	m_PanbrelloDelta = 0;
	m_PanbrelloPos = 0;

	m_RetrigOperation = 0;
	m_RetrigVol = 0;

	m_ArpeggioPeriod[0] = 0.0;
	m_ArpeggioPeriod[1] = 0.0;

	m_NoteCutTick = 0;

	m_MIDI_Set = 0;
	m_DefaultCutoff= 127;
	m_DefaultRessonance =0;
	m_DefaultFilterType = dsp::F_NONE;

}

void XMSampler::Channel::EffectInit()
{
	#if 0
		m_VibratoPos = 0;
		m_TremoloPos = 0;
		m_TremoloDepth = 0;
		m_VibratoAmount = 0;
		m_AutoVibratoAmount = 0;
		m_PanbrelloPos = 0;
	#endif
}
void XMSampler::Channel::Restore()
{
	m_Volume = (m_ChannelDefVolume&0xFF)/200.0f;
	if ((m_ChannelDefVolume&0x100)) m_bMute = true;

	m_PanFactor = (m_DefaultPanFactor&0xFF)/200.0f;
	if ((m_DefaultPanFactor&0x100)) m_bSurround = true;

	m_PanSlideMem = 0;
	m_ChanVolSlideMem = 0;
	m_PitchSlideMem = 0;
	m_TremorMem = 0;
	m_TremorOnTime = 0;
	m_TremorOffTime = 0;
	m_VibratoDepthMem = 0;
	m_VibratoSpeedMem = 0;
	m_TremoloDepthMem = 0;
	m_TremoloSpeedMem = 0;
	m_PanbrelloDepthMem = 0;
	m_PanbrelloSpeedMem = 0;
	m_VolumeSlideMem = 0;
	m_ArpeggioMem = 0;
	m_GlobalVolSlideMem = 0;
	m_RetrigMem = 0;
	m_OffsetMem = 0;
	m_Cutoff = m_DefaultCutoff;
	m_Ressonance = m_DefaultRessonance;
	m_FilterType = m_DefaultFilterType;
	m_DelayedNote.clear();
}
void XMSampler::Channel::SetEffect(Voice* voice,int volcmd,int cmd,int parameter)
{
	int realSet=0;
	int realValue=0;

	//1st check: Channel ( They can appear without an existing playing note and are persistent when a new one comes)
	switch(volcmd&0xF0)
	{
	case CMD_VOL::VOL_PANNING:
		PanFactor((volcmd&0x0F)/15.0f);
		break;
	case CMD_VOL::VOL_PANSLIDELEFT:
		//this command is actually fine pan slide
		PanningSlide((volcmd&0x0F)<<4);
		break;
	case CMD_VOL::VOL_PANSLIDERIGHT:
		//this command is actually fine pan slide
		PanningSlide(volcmd&0x0F);
		break;
	default:
		break;
	}

	switch(cmd)
	{
	case CMD::PANNING:
		IsSurround(false);
		PanFactor(parameter/255.0f);
		break;
	case CMD::SET_CHANNEL_VOLUME:
		Volume((parameter<64)?(parameter/64.0f):1.0f);
		break;
	case CMD::PANNINGSLIDE:
		PanningSlide(parameter);
		break;
	case CMD::CHANNEL_VOLUME_SLIDE:
		ChannelVolumeSlide(parameter);
		break;
	case CMD::SET_GLOBAL_VOLUME:
		m_pSampler->GlobalVolume(parameter<0x80?parameter:0x80);
		break;
	case CMD::GLOBAL_VOLUME_SLIDE:
		GlobalVolSlide(parameter);
		break;
	case CMD::EXTENDED:
		switch(parameter&0xF0)
		{
		case CMD_E::E9:
			switch(parameter&0x0F)
			{
			case CMD_E9::E9_SURROUND_OFF:
				IsSurround(false);
				break;
			case CMD_E9::E9_SURROUND_ON:
				IsSurround(true);
				break;
			case CMD_E9::E9_REVERB_OFF:
				break;
			case CMD_E9::E9_REVERB_FORCE:
				break;
			case CMD_E9::E9_STANDARD_SURROUND:
				break;
			case CMD_E9::E9_QUAD_SURROUND:
				break;
			case CMD_E9::E9_GLOBAL_FILTER:
				break;
			case CMD_E9::E9_LOCAL_FILTER:
				break;
			default:
				break;
			}
			break;
		case CMD_E::E_SET_PAN:
			PanFactor((parameter&0xf)/15.0f);
			break;
		case CMD_E::E_SET_MIDI_MACRO:
			m_MIDI_Set = parameter&0x0F;
			//\todo : implement.
			break;
		case CMD_E::E_GLISSANDO_TYPE:
			IsGrissando(parameter != 0);
			break;
		case CMD_E::E_VIBRATO_WAVE:
			VibratoType(parameter);
			break;
		case CMD_E::E_PANBRELLO_WAVE:
			PanbrelloType(parameter);
			break;
		case CMD_E::E_TREMOLO_WAVE:
			TremoloType(parameter);
			break;
		default:
			break;
		}
		break;
	case CMD::MIDI_MACRO:
		if ( parameter < 0x80)
		{
			realSet = m_MIDI_Set;
			realValue = parameter;
		}
		else
		{
			realSet = m_pSampler->GetMap(parameter-0x80).mode;
			realValue = m_pSampler->GetMap(parameter-0x80).value;
		}
		switch(realSet)
		{
		case 0:
			m_Cutoff=realValue;
			if ( m_FilterType == dsp::F_NONE) m_FilterType = dsp::F_ITLOWPASS;
			if ( voice )
			{
				voice->FilterType(m_FilterType);
				voice->CutOff(m_Cutoff);
			}
			break;
		case 1:
			m_Ressonance=realValue;
			if ( m_FilterType == dsp::F_NONE) m_FilterType = dsp::F_ITLOWPASS;
			if ( voice )
			{
				voice->FilterType(m_FilterType);
				voice->Ressonance(m_Ressonance);
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	// 2nd Check. Commands that require a voice.
	int slidval=0;
	if ( voice )
	{
		switch(volcmd&0xF0)
		{
		case CMD_VOL::VOL_VOLUME0:
		case CMD_VOL::VOL_VOLUME1:
		case CMD_VOL::VOL_VOLUME2:
		case CMD_VOL::VOL_VOLUME3:
			voice->Volume(volcmd<<1);
			break;
		case CMD_VOL::VOL_VOLSLIDEUP:
			VolumeSlide((volcmd&0x0F)<<4);
			break;
		case CMD_VOL::VOL_VOLSLIDEDOWN:
			VolumeSlide(volcmd&0x0F);
			break;
		case CMD_VOL::VOL_FINEVOLSLIDEUP:
			voice->m_VolumeSlideSpeed = (volcmd & 0x0F)<<1;
			voice->VolumeSlide();
			break;
		case CMD_VOL::VOL_FINEVOLSLIDEDOWN:
			voice->m_VolumeSlideSpeed = -((volcmd & 0x0F)<<1);
			voice->VolumeSlide();
			break;
		//case CMD_VOL::VOL_VIBRATO_SPEED:
			//Vibrato(volcmd&0x0F,0); //\todo: vibrato_speed does not activate the vibrato if it isn't running.
			//break;
		case CMD_VOL::VOL_VIBRATO:
			Vibrato(0,(volcmd & 0x0F)<<2);
			break;
		case CMD_VOL::VOL_TONEPORTAMENTO:
			// Portamento to (Gx) affects the memory for Gxx and has the equivalent
			// slide given by this table:
			// SlideTable      DB      1, 4, 8, 16, 32, 64, 96, 128, 255
			if ( (volcmd&0x0F) == 0 ) slidval=0;
			else if ( (volcmd&0x0F) == 1)  slidval=1;
			else if ( (volcmd&0x0F) < 9) slidval=std::pow(2.0f,volcmd&0x0F);
			else slidval=255;
			PitchSlide(voice->Period()>voice->NoteToPeriod(Note()),slidval,Note());
			break;
		case CMD_VOL::VOL_PITCH_SLIDE_DOWN:
			// Pitch slide up/down affect E/F/(G)'s memory - a Pitch slide
			// up/down of x is equivalent to a normal slide by x*4
			PitchSlide(false,(volcmd&0x0F)<<2);
			break;
		case CMD_VOL::VOL_PITCH_SLIDE_UP:
			PitchSlide(true,(volcmd&0x0F)<<2);
			break;
		default:
			break;
		}

		switch(cmd)
		{
		// Class A: Voice ( They can apply to an already playing voice, or a new coming one).
		case CMD::VOLUME:
			voice->Volume(parameter);
			break;
		case CMD::SET_ENV_POSITION:
			if (voice->AmplitudeEnvelope().Envelope().IsEnabled())
				voice->AmplitudeEnvelope().SetPositionInSamples(parameter*m_pSampler->pCallbacks()->timeInfo().samplesPerTick());
			if (voice->PanEnvelope().Envelope().IsEnabled())
				voice->PanEnvelope().SetPositionInSamples(parameter*m_pSampler->pCallbacks()->timeInfo().samplesPerTick());
			if (voice->PitchEnvelope().Envelope().IsEnabled())
				voice->PitchEnvelope().SetPositionInSamples(parameter*m_pSampler->pCallbacks()->timeInfo().samplesPerTick());
			if (voice->FilterEnvelope().Envelope().IsEnabled())
				voice->FilterEnvelope().SetPositionInSamples(parameter*m_pSampler->pCallbacks()->timeInfo().samplesPerTick());
			break;
		case CMD::EXTENDED:
			switch(parameter&0xF0)
			{
			case CMD_E::E9:
				switch(parameter&0x0F)
				{
				case CMD_E9::E9_PLAY_FORWARD:
					voice->rWave().CurrentLoopDirection(WaveDataController::LoopDirection::FORWARD);
					break;
				case CMD_E9::E9_PLAY_BACKWARD:
					if (voice->rWave().LoopType() == XMInstrument::WaveData::LoopType::DO_NOT &&
						voice->rWave().Position() == 0
					) {
						voice->rWave().Position(voice->rWave().Length());
					}
					voice->rWave().CurrentLoopDirection(WaveDataController::LoopDirection::BACKWARD);
					break;
				}
				break;
			case CMD_E::EE:
				switch(parameter&0x0F)
				{
				case CMD_EE::EE_SETNOTECUT:
					voice->NNA(XMInstrument::NewNoteAction::STOP);
					break;
				case CMD_EE::EE_SETNOTECONTINUE:
					voice->NNA(XMInstrument::NewNoteAction::CONTINUE);
					break;
				case CMD_EE::EE_SETNOTEOFF:
					voice->NNA(XMInstrument::NewNoteAction::NOTEOFF);
					break;
				case CMD_EE::EE_SETNOTEFADE:
					voice->NNA(XMInstrument::NewNoteAction::FADEOUT);
					break;
				case CMD_EE::EE_BACKGROUNDNOTECUT:
					StopBackgroundNotes(XMInstrument::NewNoteAction::STOP);
					break;
				case CMD_EE::EE_BACKGROUNDNOTEOFF:
					StopBackgroundNotes(XMInstrument::NewNoteAction::NOTEOFF);
					break;
				case CMD_EE::EE_BACKGROUNDNOTEFADE:
					StopBackgroundNotes(XMInstrument::NewNoteAction::FADEOUT);
					break;
				}
				break;
			case CMD_E::E_DELAYED_NOTECUT:
				NoteCut(parameter&0x0F);
				break;
			}
			break;

		// Class B Channel ( Just like Class A, but remember its old value if it is called again with  00 as parameter  )

		case CMD::PORTAMENTO_UP:
			PitchSlide(true,parameter);
			break;
		case CMD::PORTAMENTO_DOWN:
			PitchSlide(false,parameter);
			break;
		case CMD::PORTA2NOTE:
			PitchSlide(voice->Period()>voice->NoteToPeriod(Note()),parameter,Note());
			break;
		case CMD::VOLUMESLIDE:
			VolumeSlide(parameter);
			break;
		case CMD::TONEPORTAVOL:
			VolumeSlide(parameter);
			PitchSlide(voice->Period()>voice->NoteToPeriod(Note()),0,Note());
			break;
		case CMD::VIBRATOVOL:
			VolumeSlide(parameter);
			Vibrato(0);
			break;
		case CMD::VIBRATO:
			Vibrato(((parameter >> 4) & 0x0F),(parameter & 0x0F)<<2);
			break;
		case CMD::FINE_VIBRATO:
			Vibrato(((parameter >> 4) & 0x0F),(parameter & 0x0F));
			break;
		case CMD::TREMOR:
			Tremor(parameter);
			break;
		case CMD::TREMOLO:
			Tremolo((parameter>> 4) & 0x0F,(parameter & 0x0F));
			break;
		case CMD::RETRIG:
			Retrigger(parameter);
			break;
		case CMD::PANBRELLO:
			Panbrello((parameter>> 4) & 0x0F,(parameter & 0x0F));
			break;
		case CMD::ARPEGGIO:
			Arpeggio(parameter);
			break;
		}
	}
	//3rd check: It is not needed that the voice is playing, but it applies to the last instrument.
	if ( InstrumentNo() != 255 && cmd == CMD::EXTENDED && (parameter&0xF0) == CMD_E::EE)
	{
		switch(parameter&0x0F)
		{
		case CMD_EE::EE_VOLENVOFF:
					voice->AmplitudeEnvelope().Pause();
			break;
		case CMD_EE::EE_VOLENVON:
					voice->AmplitudeEnvelope().Continue();
			break;
		case CMD_EE::EE_PANENVOFF:
					voice->PanEnvelope().Pause();
			break;
		case CMD_EE::EE_PANENVON:
					voice->PanEnvelope().Continue();
			break;
		case CMD_EE::EE_PITCHENVON:
					voice->PitchEnvelope().Pause();
			break;
		case CMD_EE::EE_PITCHENVOFF:
					voice->PitchEnvelope().Continue();
			break;
		}
	}
}

// Add Here those commands that have an effect each tracker tick ( 1 and onwards) .
// tick "0" is worked on in channel.SetEffect();
void XMSampler::Channel::PerformFx()
{
	for ( int i=0;i< m_pSampler->NumVoices();i++)
	{
		if ( m_pSampler->rVoice(i).ChannelNum() == m_Index && m_pSampler->rVoice(i).IsPlaying())  m_pSampler->rVoice(i).Tick();
	}
	if(ForegroundVoice()) // Effects that need a voice to be active.
	{
		if(ForegroundVoice()->IsAutoVibrato())
		{
			ForegroundVoice()->AutoVibrato();
		}
		if(EffectFlags() & EffectFlag::PITCHSLIDE)
		{
			ForegroundVoice()->PitchSlide();
		}
		if(EffectFlags() & EffectFlag::SLIDE2NOTE)
		{
			ForegroundVoice()->Slide2Note();
		}
		if(EffectFlags() & EffectFlag::VIBRATO)
		{
			ForegroundVoice()->Vibrato();
		}
		if(EffectFlags() & EffectFlag::TREMOLO)
		{
			ForegroundVoice()->Tremolo();
		}
		if(EffectFlags() & EffectFlag::PANBRELLO)
		{
			ForegroundVoice()->Panbrello();
		}
		if(EffectFlags() & EffectFlag::TREMOR)
		{
			ForegroundVoice()->Tremor();
		}
		if(EffectFlags() & EffectFlag::VOLUMESLIDE)
		{
			ForegroundVoice()->VolumeSlide();
		}
		if(EffectFlags() & EffectFlag::NOTECUT)
		{
			NoteCut();
		}
		if(EffectFlags() & EffectFlag::ARPEGGIO)
		{
			ForegroundVoice()->UpdateSpeed();
		}
		if(EffectFlags() & EffectFlag::RETRIG)
		{
			LastVoicePanFactor(ForegroundVoice()->PanFactor());

			if ( m_RetrigOperation < 2)
			{
				int tmp = ForegroundVoice()->Volume()+m_RetrigVol;
				if ( tmp <0 ) tmp =0;
				else if ( tmp > 128 ) tmp=128;
				LastVoiceVolume(tmp);
			}
			else //if ( m_RetrigOperation == 2)
			{
				int tmp = ForegroundVoice()->Volume()*m_RetrigVol;
				if ( tmp <0 ) tmp =0;
				else if ( tmp > 128 ) tmp=128;
				LastVoiceVolume(tmp);
			}

			if (ForegroundVoice()->AmplitudeEnvelope().Envelope().IsEnabled())
				LastAmpEnvelopePosInSamples(ForegroundVoice()->AmplitudeEnvelope().GetPositionInSamples());
			if (ForegroundVoice()->PanEnvelope().Envelope().IsEnabled())
				LastPanEnvelopePosInSamples(ForegroundVoice()->PanEnvelope().GetPositionInSamples());
			if (ForegroundVoice()->FilterEnvelope().Envelope().IsEnabled())
				LastFilterEnvelopePosInSamples(ForegroundVoice()->FilterEnvelope().GetPositionInSamples());
			if (ForegroundVoice()->PitchEnvelope().Envelope().IsEnabled())
				LastPitchEnvelopePosInSamples(ForegroundVoice()->PitchEnvelope().GetPositionInSamples());
			ForegroundVoice()->Retrig();
		}
	}
	if(EffectFlags() & EffectFlag::CHANNELVOLSLIDE)
	{
		ChannelVolumeSlide();
	}
	if(EffectFlags()  & EffectFlag::PANSLIDE)
	{
		PanningSlide();
	}
	if(EffectFlags() & EffectFlag::NOTEDELAY)
	{
		if(m_pSampler->CurrentTick() == m_NoteCutTick)
		{
			for (std::vector<PatternEvent>::iterator ite = m_DelayedNote.begin(); ite != m_DelayedNote.end(); ++ite) {
				m_pSampler->Tick(m_Index,*ite);
			}
			m_DelayedNote.clear();
		}
	}
	if(EffectFlags() & EffectFlag::GLOBALVOLSLIDE)
	{
		m_pSampler->SlideVolume(m_GlobalVolSlideSpeed);
	}

}

void XMSampler::Channel::GlobalVolSlide(int speed)
{
	if(speed == 0){
		if ( m_GlobalVolSlideMem == 0 ) return;
		speed = m_GlobalVolSlideMem;
	}
	else m_GlobalVolSlideMem = speed;

			if ( ISSLIDEUP(speed) ){ // Slide up
				speed = GETSLIDEUPVAL(speed);
		m_EffectFlags |= EffectFlag::GLOBALVOLSLIDE;
		m_GlobalVolSlideSpeed = speed;
		if (speed == 0xF ) m_pSampler->SlideVolume(m_GlobalVolSlideSpeed);
	}
			else if ( ISSLIDEDOWN(speed))  { // Slide down
				speed = GETSLIDEDOWNVAL(speed);
		m_EffectFlags |= EffectFlag::GLOBALVOLSLIDE;
		m_GlobalVolSlideSpeed = -speed;
		if (speed == 0xF ) m_pSampler->SlideVolume(m_GlobalVolSlideSpeed);
	}
			else if ( ISFINESLIDEUP(speed) ) { // FineSlide up
				m_GlobalVolSlideSpeed = GETSLIDEUPVAL(speed);
		m_pSampler->SlideVolume(m_GlobalVolSlideSpeed);
	}
	else if ( (speed & 0xF0) == 0xF0 ) { // FineSlide down
				m_GlobalVolSlideSpeed = -GETSLIDEDOWNVAL(speed);
		m_pSampler->SlideVolume(m_GlobalVolSlideSpeed);
	}
}
void XMSampler::Channel::PanningSlide(int speed)
{
	if(speed == 0){
		if ( m_PanSlideMem == 0 ) return;
		speed = m_PanSlideMem;
	}
	else m_PanSlideMem = speed;

			if ( ISSLIDEUP(speed) ){ // Slide Left
				speed = GETSLIDEUPVAL(speed);
		m_EffectFlags |= EffectFlag::PANSLIDE;
		m_PanSlideSpeed = -speed/64.0f;
		if (speed == 0xF ) PanningSlide();
	}
			else if ( ISSLIDEDOWN(speed) )  { // Slide Right
				speed = GETSLIDEDOWNVAL(speed);
		m_EffectFlags |= EffectFlag::PANSLIDE;
		m_PanSlideSpeed = speed/64.0f;
		if (speed == 0xF ) PanningSlide();
	}
			else if ( ISFINESLIDEUP(speed) ) { // FineSlide left
				m_PanSlideSpeed = -(GETSLIDEUPVAL(speed))/64.0f;
		PanningSlide();
	}
			else if ( ISFINESLIDEDOWN(speed) ) { // FineSlide right
				m_PanSlideSpeed = GETSLIDEDOWNVAL(speed)/64.0f;
		PanningSlide();
	}
}
void XMSampler::Channel::ChannelVolumeSlide(int speed)
{
	if(speed == 0){
		if ( m_ChanVolSlideMem == 0 ) return;
		speed = m_ChanVolSlideMem;
	}
	else m_ChanVolSlideMem = speed;

			if ( ISSLIDEUP(speed) ){ // Slide up
				speed = GETSLIDEUPVAL(speed);
		m_EffectFlags |= EffectFlag::CHANNELVOLSLIDE;
		m_ChanVolSlideSpeed = speed/64.0f;
		if (speed == 0xF ) ChannelVolumeSlide();
	}
			else if ( ISSLIDEDOWN(speed) )  { // Slide down
				speed = GETSLIDEDOWNVAL(speed);
		m_EffectFlags |= EffectFlag::CHANNELVOLSLIDE;
		m_ChanVolSlideSpeed = -speed/64.0f;
		if (speed == 0xF ) ChannelVolumeSlide();
	}
			else if ( ISFINESLIDEUP(speed) ) { // FineSlide up
				m_ChanVolSlideSpeed = (GETSLIDEUPVAL(speed))/64.0f;
		ChannelVolumeSlide();
	}
			else if ( ISFINESLIDEDOWN(speed)) { // FineSlide down
				m_ChanVolSlideSpeed = -GETSLIDEDOWNVAL(speed)/64.0f;
		ChannelVolumeSlide();
	}
}
void XMSampler::Channel::PitchSlide(bool bUp,int speed,int note)
{
	if ( speed == 0 ) {
		if ( m_PitchSlideMem == 0 ) return;
		speed = m_PitchSlideMem;
	}
	else m_PitchSlideMem = speed &0xff;

	if ( speed < 0xE0 || note !=notetypes::empty) // Portamento , Fine porta ("f0", and Extra fine porta "e0" ) (*)
	{ // Porta to note does not have Fine.
		speed<<=2;
		if ( ForegroundVoice()) { ForegroundVoice()->m_PitchSlideSpeed= bUp?-speed:speed; }
		if ( note != notetypes::empty )
		{
			if ( ForegroundVoice()) { ForegroundVoice()->m_Slide2NoteDestPeriod = ForegroundVoice()->NoteToPeriod(note); }
			m_EffectFlags |= EffectFlag::SLIDE2NOTE;
		}
		else m_EffectFlags |= EffectFlag::PITCHSLIDE;
	} else if ( speed < 0xF0) {
		speed= speed&0xf;
		if ( ForegroundVoice())
		{
			ForegroundVoice()->m_PitchSlideSpeed= bUp?-speed:speed;
			ForegroundVoice()->PitchSlide();
		}
	} else  {
		speed= (speed&0xf)<<2;
		if ( ForegroundVoice())
		{
			ForegroundVoice()->m_PitchSlideSpeed= bUp?-speed:speed;
			ForegroundVoice()->PitchSlide();
		}
	}
}

void XMSampler::Channel::VolumeSlide(int speed)
{
	if(speed == 0){
		if ( m_VolumeSlideMem == 0 ) return;
		speed = m_VolumeSlideMem;
	}
	else m_VolumeSlideMem = speed;

			if ( ISSLIDEUP(speed)){ // Slide Up
				speed = GETSLIDEUPVAL(speed);
		m_EffectFlags |= EffectFlag::VOLUMESLIDE;
		if ( ForegroundVoice())
		{
			ForegroundVoice()->m_VolumeSlideSpeed = speed<<1;
			if (speed == 0xF ) ForegroundVoice()->VolumeSlide();
		}
	}
			else if ( ISSLIDEDOWN(speed) )  { // Slide Down
				speed = GETSLIDEDOWNVAL(speed);
		m_EffectFlags |= EffectFlag::VOLUMESLIDE;
		if ( ForegroundVoice())
		{
			ForegroundVoice()->m_VolumeSlideSpeed = -(speed<<1);
			if (speed == 0xF ) ForegroundVoice()->VolumeSlide();
		}
	}
			else if ( ISFINESLIDEUP(speed)) { // FineSlide Up
		if ( ForegroundVoice())
		{
					ForegroundVoice()->m_VolumeSlideSpeed = GETSLIDEUPVAL(speed)<<1;
			ForegroundVoice()->VolumeSlide();
		}
	}
			else if ( ISFINESLIDEDOWN(speed)) { // FineSlide Down
		if ( ForegroundVoice())
		{
					ForegroundVoice()->m_VolumeSlideSpeed = -(GETSLIDEDOWNVAL(speed)<<1);
			ForegroundVoice()->VolumeSlide();
		}
	}
}

void XMSampler::Channel::Tremor(int parameter)
{
	if(parameter == 0){
		if (m_TremorMem == 0 ) return;
		parameter = m_TremorMem;
	}
	else m_TremorMem = parameter;
	if ( ForegroundVoice())
	{
		ForegroundVoice()->m_TremorOnTicks = ((parameter >> 4) & 0xF) + 1;
		ForegroundVoice()->m_TremorOffTicks = (parameter & 0xF) + 1;
		ForegroundVoice()->m_TremorTickChange = ForegroundVoice()->m_TremorOnTicks;
	}
	m_EffectFlags |= EffectFlag::TREMOR;
}
void XMSampler::Channel::Vibrato(int speed,int depth)
{
	if(depth == 0){
		if ( m_VibratoDepthMem == 0 ) return;
		depth = m_VibratoDepthMem;
	}
	else m_VibratoDepthMem = depth;

	if(speed == 0){
		if ( m_VibratoSpeedMem == 0 ) return;
		speed = m_VibratoSpeedMem;
	}
	else m_VibratoSpeedMem = speed;
	if ( ForegroundVoice())
	{
		ForegroundVoice()->m_VibratoSpeed=speed<<2;
		ForegroundVoice()->m_VibratoDepth=depth;
	}
	m_EffectFlags |= EffectFlag::VIBRATO;

}// XMSampler::Voice::Vibrato(const int depth,const int speed) ------------------------
void XMSampler::Channel::Tremolo(int speed,int depth)
{
	if(depth == 0){
		if ( m_TremoloDepthMem == 0 ) return;
		depth = m_TremoloDepthMem;
	}
	else m_TremoloDepthMem = depth;

	if(speed == 0){
		if ( m_TremoloSpeedMem == 0 ) return;
		speed = m_TremoloSpeedMem;
	}
	else m_TremoloSpeedMem = speed;

	if ( ForegroundVoice())
	{
		ForegroundVoice()->m_TremoloSpeed=speed<<2;
		ForegroundVoice()->m_TremoloDepth=depth;
	}
	m_EffectFlags |= EffectFlag::TREMOLO;
}
void XMSampler::Channel::Panbrello(int speed,int depth)
{
	if(depth == 0){
		if ( m_PanbrelloDepthMem == 0 ) return;
		depth = m_PanbrelloDepthMem;
	}
	else m_PanbrelloDepthMem = depth;

	if(speed == 0){
		if ( m_PanbrelloSpeedMem == 0 ) return;
		speed = m_PanbrelloSpeedMem;
	}
	else m_PanbrelloSpeedMem = speed;

	if ( ForegroundVoice())
	{
		ForegroundVoice()->m_PanbrelloSpeed=speed<<2;
		ForegroundVoice()->m_PanbrelloDepth=depth;
	}
	m_EffectFlags |= EffectFlag::PANBRELLO;
}

void XMSampler::Channel::Arpeggio(int param)
{
	if ( param != 0 )
	{
		m_ArpeggioMem = param;
	}
	else param = m_ArpeggioMem;
	if ( ForegroundVoice())
	{
		m_ArpeggioPeriod[0] = ForegroundVoice()->NoteToPeriod(Note() + ((param & 0xf0) >> 4));
		m_ArpeggioPeriod[1] = ForegroundVoice()->NoteToPeriod(Note() + (param & 0xf));
	}
	m_EffectFlags |= EffectFlag::ARPEGGIO;
}
void XMSampler::Channel::Retrigger(const int parameter)
{
	int ticks,volumeModifier;
	int effretVol,effretMode;

	if ( parameter == 0 )
	{
		ticks = (m_RetrigMem & 0x0F);
		volumeModifier = (m_RetrigMem>> 4) & 0x0F;
	}
	else {
		ticks = (parameter & 0x0F);
		volumeModifier = (parameter>> 4) & 0x0F;
		m_RetrigMem = parameter;
	}
	switch (volumeModifier)
	{
	case 1://fallthrouhg
	case 2://fallthrouhg
	case 3://fallthrouhg
	case 4://fallthrouhg
	case 5: effretVol = -(int)std::pow(2.,volumeModifier-1); effretMode=1; break;
	case 6: effretVol = 0.66666666f; effretMode = 2; break;
	case 7: effretVol = 0.5f; effretMode = 2; break;

	case 9://fallthrouhg
	case 10://fallthrouhg
	case 11://fallthrouhg
	case 12://fallthrouhg
	case 13: effretVol = (int)std::pow(2.,volumeModifier - 9); effretMode=1; break;
	case 14: effretVol = 1.5f;effretMode = 2; break;
	case 15: effretVol = 2.0f;effretMode = 2; break;

	case 0://fallthrouhg
	case 8:	//fallthrouhg
	default: effretVol = 0; effretMode=0; break;
	}
	if ( ForegroundVoice())
	{
		ForegroundVoice()->m_RetrigTicks = ticks!=0?ticks:1;
	}
	m_RetrigVol = effretVol;
	m_RetrigOperation = effretMode;
	m_EffectFlags |= EffectFlag::RETRIG;
}
void XMSampler::Channel::NoteCut(const int ntick){
	m_NoteCutTick = ntick;
	if(ntick == 0){
		if ( ForegroundVoice())
		{
			ForegroundVoice()->Volume(0);
		}
		return;
	}
	m_EffectFlags |= EffectFlag::NOTECUT;
}
void XMSampler::Channel::DelayedNote(PatternEvent data)
{
	if (data.command() == CMD::EXTENDED && (data.parameter() & 0xf0) == CMD_E::E_NOTE_DELAY) {
		m_NoteCutTick=data.parameter()&0x0f;
		data.setCommand(XMSampler::CMD::NONE);
		data.setParameter(0);
		m_EffectFlags |= EffectFlag::NOTEDELAY;
	}
	m_DelayedNote.push_back(data);
}

void XMSampler::Channel::PanningSlide(){
	m_PanFactor += m_PanSlideSpeed;
	if(m_PanFactor < 0.0f) { m_PanFactor = 0.0f;}
	else if(m_PanFactor > 1.0f){m_PanFactor = 1.0f;}
	if (ForegroundVoice()) ForegroundVoice()->PanFactor(m_PanFactor);
}
void XMSampler::Channel::ChannelVolumeSlide(){
	m_Volume += m_ChanVolSlideSpeed;
	if(m_Volume < 0.0f){ m_Volume = 0.0f;}
	else if(m_Volume > 1.0f){m_Volume = 1.0f;}
}
void XMSampler::Channel::NoteCut()
{
	if(m_pSampler->CurrentTick() == m_NoteCutTick)
	{
		if (ForegroundVoice()) ForegroundVoice()->Volume(0);
		m_EffectFlags &= ~EffectFlag::NOTECUT;

	}
}
void XMSampler::Channel::StopBackgroundNotes(XMInstrument::NewNoteAction::Type action)
{
	for(int current = 0;current < m_pSampler->NumVoices();current++)
	{
		if ( m_pSampler->rVoice(current).ChannelNum() == m_Index && m_pSampler->rVoice(current).IsPlaying())
		{
			switch(action)
			{
			case XMInstrument::NewNoteAction::NOTEOFF:
				m_pSampler->rVoice(current).NoteOff();
				break;
			case XMInstrument::NewNoteAction::FADEOUT:
				m_pSampler->rVoice(current).NoteFadeout();
				break;
			case XMInstrument::NewNoteAction::STOP:
				m_pSampler->rVoice(current).NoteOffFast();
				break;
            case XMInstrument::NewNoteAction::CONTINUE:
                break;
			}
		}
	}
}
bool XMSampler::Channel::Load(RiffFile& riffFile)
{
	char temp[8];
	int size=0;
	riffFile.ReadArray(temp,4); temp[4]='\0';
	riffFile.Read(size);
	if (strcmp(temp,"CHAN")) return false;

	riffFile.Read(m_ChannelDefVolume);
	riffFile.Read(m_DefaultPanFactor);
	riffFile.Read(m_DefaultCutoff);
	riffFile.Read(m_DefaultRessonance);
	{ uint32_t i(0); riffFile.Read(i); m_DefaultFilterType = static_cast<dsp::FilterType>(i); }

	return true;
}
void XMSampler::Channel::Save(RiffFile& riffFile) const
{
	int size=5*sizeof(int);
	riffFile.WriteArray("CHAN",4);
	riffFile.Write(size);
	riffFile.Write(m_ChannelDefVolume);
	riffFile.Write(m_DefaultPanFactor);
	riffFile.Write(m_DefaultCutoff);
	riffFile.Write(m_DefaultRessonance);
	{ uint32_t i = m_DefaultFilterType; riffFile.Write(i); }
}



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
///    XMSampler
XMSampler::XMSampler(MachineCallbacks* callb,Machine::id_type id)
	:Machine(callb,id) {
	SetEditName(_psName);
	defineOutputAsStereo();
	SetAudioRange(32768.0f);
	//DefineStereoOutput(1);

	_numPars = 0;
	_numVoices=0;
	resampler_.quality(dsp::resampler::quality::linear);

	m_bAmigaSlides = false;
	m_UseFilters = true;
	m_GlobalVolume = 128;
	m_PanningMode = PanningMode::Linear;
	m_TickCount = 0;
	m_NextSampleTick = 0;
	_sampleCounter = 0;

	int i;
	for (i = 0; i < XMSampler::MAX_POLYPHONY; i++)
	{
		m_Voices[i].pSampler(this);
		m_Voices[i].Reset();
	}

	for (i = 0; i < MAX_TRACKS; i++)
	{
		m_Channel[i].pSampler(this);
		m_Channel[i].Index(i);
	}

			for(i = 0; i < 16;i++)
			{
				zxxMap[i].mode=1;
				zxxMap[i].value=i*8;
			}
			for(i = 16; i < 128;i++)
	{
				zxxMap[i].mode=-1;
		zxxMap[i].value=0;
	}
}


void XMSampler::Init(void)
{
	Machine::Init();

	_numVoices = XMSampler::MAX_POLYPHONY;
			multicmdMem.resize(0);
	int i;
	for (i=0; i < _numVoices; i++)
	{
		m_Voices[i].Reset();
	}

	for(i = 0; i < MAX_TRACKS;i++)
	{
		m_Channel[i].Init();
		m_Channel[i].Restore();
	}
}
void XMSampler::SetSampleRate(int sr)
{
	Machine::SetSampleRate(sr);
	//\todo
	//update envelopes
	//
}

void XMSampler::Tick()
{
	SampleCounter(0);
	m_TickCount=0;

	NextSampleTick(callbacks->timeInfo().samplesPerTick()+1);// +1 is to avoid one Tick too much at the end, because m_DeltaTick is rounded down.

	for (int channel=0;channel<MAX_TRACKS;channel++)
	{
		rChannel(channel).EffectFlags(0);
	}

	for (int voice = 0; voice < _numVoices ; voice++)
	{
		if ( m_Voices[voice].IsPlaying()) m_Voices[voice].NewLine();
	}
}
void XMSampler::PostNewLine()
{
	multicmdMem.clear();
}


void XMSampler::Tick(int channelNum, const PatternEvent & event)
{
	if(event.note() == notetypes::midi_cc && event.instrument() < MAX_TRACKS)
	{
		multicmdMem.push_back(event);
		return;
	}
	// don't process twk , twf, Mcm Commands, or empty lines.
	else if ( event.note() > notetypes::release  && event.note() < notetypes::empty)
	{
		return;
	}

	// define some variables to ease the case checking.
	bool bInstrumentSet = (event.instrument() < 255);
	bool bPortaEffect = false;
	for (std::vector<PatternEvent>::const_iterator ite = multicmdMem.begin(); ite != multicmdMem.end(); ++ite) {
		if(ite->instrument() == channelNum) {
			bPortaEffect |= (ite->command() == CMD::PORTA2NOTE) 	
				|| (ite->command() == CMD::SENDTOVOLUME && (ite->parameter()&0xF0) == CMD_VOL::VOL_TONEPORTAMENTO)
				|| ((ite->volume()&0xF0) == CMD_VOL::VOL_TONEPORTAMENTO);
		}
	}
	bPortaEffect |= (event.command() == CMD::PORTA2NOTE) || ((event.volume()&0xF0) == CMD_VOL::VOL_TONEPORTAMENTO);
	bool bPorta2Note = (event.note() <= notetypes::b9) && bPortaEffect;
	bool bNoteOn = (event.note() <= notetypes::b9) && !bPorta2Note;


	Voice* currentVoice = NULL;
	Voice* newVoice = NULL;
	XMSampler::Channel& thisChannel = rChannel(channelNum);
	if(bInstrumentSet)
	{
		if ( event.instrument() != thisChannel.InstrumentNo() && thisChannel.Note() !=0)
		{
			bNoteOn=true;
		}
		thisChannel.InstrumentNo(event.instrument()); // Instrument is always set, even if no new note comes.
	}

	// STEP A: Look for an existing (foreground) playing voice in the current channel.
	currentVoice = GetCurrentVoice(channelNum);
	if ( currentVoice )
	{
		// Is a new note coming? Then apply the NNA to the playing one.
		if (bNoteOn)
		{
			switch (currentVoice->rInstrument().DCT())
			{
			case XMInstrument::DCType::DCT_INSTRUMENT:
				if ( event.instrument() == thisChannel.InstrumentNo())
				{
					if ( currentVoice->rInstrument().DCA() < currentVoice->NNA() ) currentVoice->NNA(currentVoice->rInstrument().DCA());
				}
				break;
			case XMInstrument::DCType::DCT_SAMPLE:
				//\todo: Implement DCType Sample.
				if ( event.instrument() == thisChannel.InstrumentNo())
				{
					if ( currentVoice->rInstrument().DCA() < currentVoice->NNA() ) currentVoice->NNA(currentVoice->rInstrument().DCA());
				}
				break;
			case XMInstrument::DCType::DCT_NOTE:
				if ( event.note() == thisChannel.Note() && event.instrument() == thisChannel.InstrumentNo())
				{
					if ( currentVoice->rInstrument().DCA() < currentVoice->NNA() ) currentVoice->NNA(currentVoice->rInstrument().DCA());
				}
				break;
			default:
				break;
			}

			switch (currentVoice->NNA())
			{
			case XMInstrument::NewNoteAction::STOP:
				currentVoice->NoteOffFast();
				break;
			case XMInstrument::NewNoteAction::NOTEOFF:
				currentVoice->NoteOff();
				break;
			case XMInstrument::NewNoteAction::FADEOUT:
				currentVoice->NoteFadeout();
				break;
			default:
				break;
			}
		} else if(event.note() == notetypes::release ){
			currentVoice->NoteOff();
		}
		else
		{
			if (bInstrumentSet)
		{
						//\todo: Fix: Set the wave and instrument to the one in the entry.
				currentVoice->ResetVolAndPan(-1);
			}
			if ( bPorta2Note )
			{
				///\todo : portamento to note, if the note corresponds to a new sample, the sample gets changed and the position reset to 0.
				thisChannel.Note(event.note());
			}
		}
	}
	else if ( bPorta2Note )
	{
		// If there is a Porta2Note command, but there is no voice playing, this is converted to a noteOn.
		bPorta2Note=false; bNoteOn = true;
	}

	// STEP B: Get a Voice to work with, and initialize it if needed.
	if(bNoteOn)
	{
		bool delayed=false;
		for (std::vector<PatternEvent>::iterator ite = multicmdMem.begin(); ite != multicmdMem.end(); ++ite) {
			if(ite->instrument() == channelNum) {
				if ( ite->command() == CMD::EXTENDED && (ite->parameter() & 0xf0) == CMD_E::E_NOTE_DELAY) {
					delayed=true;
				}
			}
		}
		if (( event.command() == CMD::EXTENDED) && ((event.parameter() & 0xf0) == CMD_E::E_NOTE_DELAY))
		{
			delayed=true;
		}
		if (delayed ) {
			for (std::vector<PatternEvent>::iterator ite = multicmdMem.begin(); ite != multicmdMem.end(); ++ite) {
				if(ite->instrument() == channelNum) {
					thisChannel.DelayedNote(*ite);
				}
			}
			thisChannel.DelayedNote(event);
		}
		else
		{
			if ( event.note() != notetypes::empty ) thisChannel.Note(event.note()); // If instrument set and no note, we don't want to reset the note.
			newVoice = GetFreeVoice(thisChannel.Index());
			if ( newVoice )
			{
				if(thisChannel.InstrumentNo() == 255
					|| callbacks->song().m_Instruments.IsEnabled(thisChannel.InstrumentNo())==false)
				{	// this is a note to an undefined instrument. we can't continue.
					///\todo : actually, we should check for commands!
					return;
				}

				const XMInstrument & _inst = callbacks->song().rInstrument(thisChannel.InstrumentNo());
				int _layer = _inst.NoteToSample(thisChannel.Note()).second;
				if(callbacks->song().m_rWaveLayers.IsEnabled(_layer))
				{
					const XMInstrument::WaveData& wave = callbacks->song().SampleData(_layer);
					int twlength = wave.WaveLength();
					newVoice->VoiceInit(_inst, channelNum,thisChannel.InstrumentNo());
					thisChannel.ForegroundVoice(newVoice);
					if (currentVoice)
					{
						thisChannel.LastVoicePanFactor(currentVoice->PanFactor());
						thisChannel.LastVoiceVolume(currentVoice->Volume());

						{
						const XMInstrument::Envelope & pEnv = currentVoice->AmplitudeEnvelope().Envelope();
						if (pEnv.IsEnabled() && pEnv.IsCarry())
							thisChannel.LastAmpEnvelopePosInSamples(currentVoice->AmplitudeEnvelope().GetPositionInSamples());
						else thisChannel.LastAmpEnvelopePosInSamples(0);
						}
						{
						const XMInstrument::Envelope & pEnv = currentVoice->PanEnvelope().Envelope();
						if (pEnv.IsEnabled() && pEnv.IsCarry())
							thisChannel.LastPanEnvelopePosInSamples(currentVoice->PanEnvelope().GetPositionInSamples());
						else thisChannel.LastPanEnvelopePosInSamples(0);
						}
						{
						const XMInstrument::Envelope & pEnv = currentVoice->FilterEnvelope().Envelope();
						if (pEnv.IsEnabled() && pEnv.IsCarry())
							thisChannel.LastFilterEnvelopePosInSamples(currentVoice->FilterEnvelope().GetPositionInSamples());
						else thisChannel.LastFilterEnvelopePosInSamples(0);
						}
						{
						const XMInstrument::Envelope & pEnv = currentVoice->PitchEnvelope().Envelope();
						if (pEnv.IsEnabled() && pEnv.IsCarry())
							thisChannel.LastPitchEnvelopePosInSamples(currentVoice->PitchEnvelope().GetPositionInSamples());
						else thisChannel.LastPitchEnvelopePosInSamples(0);
						}
					}
					int vol = -1;
					int offset = 0;
					for (std::vector<PatternEvent>::const_iterator ite = multicmdMem.begin(); ite != multicmdMem.end(); ++ite) {
						if(ite->instrument() == channelNum) {
							if(ite->command() == CMD::SENDTOVOLUME && (ite->parameter()&0xF0) <= CMD_VOL::VOL_VOLUME3) {
								vol = ite->parameter()<<1;
							}
							else if ((ite->command()&0xF0) == CMD::OFFSET) {
                                offset = (((ite->command()&0x0F) << 16) +ite->parameter())<<8;
								if (offset == 0) offset = thisChannel.OffsetMem();
							}
						}
					}
					if ( event.volume()<0x40) vol = event.volume()<<1;
					else if (event.command() == CMD::VOLUME) vol = event.parameter();
					else if (event.command() == CMD::SENDTOVOLUME && (event.parameter()&0xF0) <= CMD_VOL::VOL_VOLUME3) vol = event.parameter()<<1;
					newVoice->NoteOn(thisChannel.Note(),vol,bInstrumentSet);
					thisChannel.Note(thisChannel.Note()); //this forces a recalc of the m_Period.

					// Add Here any command that is limited to the scope of the new note.
					// An offset is a good candidate, but a volume is not (volume can apply to an existing note)
					if ((event.command()&0xF0) == CMD::OFFSET)
					{
                        offset = (((event.command()&0x0F) << 16) +event.parameter())<<8;
						if (offset == 0) offset = thisChannel.OffsetMem();
					}
					if ( offset != 0 ) {
						thisChannel.OffsetMem(offset);
						if ( offset < twlength) { newVoice->rWave().Position(offset); }
						else { newVoice->rWave().Position(twlength); }
					}
					else{ newVoice->rWave().Position(0); }
				}
				else
				{
					bNoteOn=false;
					newVoice = NULL;
					///\TODO: multicommand
					if ( event.command() == 0 ) return;
				}
			}
			else
			{
				// This is a noteon command, but we are out of voices. We will try to process the effect.
				bNoteOn=false;
					///\TODO: multicommand
				if ( event.command() == 0 ) return;
			}
		}
		}
	if ( newVoice == NULL ) newVoice = currentVoice;
	// Effect Command
	for (std::vector<PatternEvent>::const_iterator ite = multicmdMem.begin(); ite != multicmdMem.end(); ++ite) {
		if (ite->instrument() == channelNum) {
			if (ite->command() == CMD::SENDTOVOLUME) {
				thisChannel.SetEffect(newVoice,ite->parameter(),0,0);
			}
			else {
				thisChannel.SetEffect(newVoice,255,ite->command(),ite->parameter());
			}
		}
	}
	if (event.command() == CMD::SENDTOVOLUME) {
		thisChannel.SetEffect(newVoice,event.parameter(),0,0);
	}
	else {
		thisChannel.SetEffect(newVoice,event.volume(),event.command(),event.parameter());
	}
}

int XMSampler::GenerateAudioInTicks( int startSample, int numSamples )
{
	//nanoseconds const t0(cpu_time_clock());

	const PlayerTimeInfo & timeInfo = callbacks->timeInfo();

	if (!_mute)
	{
		Standby(false);
		int ns = numSamples;
		int nextevent;

		while (ns)
		{
			nextevent = ns + 1;
			for (unsigned int i=0; i < callbacks->song().tracks(); i++)
			{
				if (TriggerDelay[i].command() )
				{
					if (TriggerDelayCounter[i] < nextevent)
					{
						nextevent = TriggerDelayCounter[i];
					}
				}
			}
			if (nextevent > ns)
			{
				for (unsigned int i=0; i < callbacks->song().tracks(); i++)
				{
					// come back to this
					if (TriggerDelay[i].command() )
					{
						TriggerDelayCounter[i] -= ns;
					}
				}
				WorkVoices(startSample, ns);
				ns = 0;
			}
			else
			{
				if (nextevent)
				{
					ns -= nextevent;
					WorkVoices(startSample, nextevent);
				}

				for (unsigned int i=0; i < callbacks->song().tracks(); i++)
				{
					// come back to this
					if (TriggerDelay[i].command() == commandtypes::NOTE_DELAY)
					{
						if (TriggerDelayCounter[i] == nextevent)
						{
							// do event
							Tick(i, TriggerDelay[i] );
							TriggerDelay[i].setCommand( 0 );
						}
						else
						{
							TriggerDelayCounter[i] -= nextevent;
						}
					}
					else if (TriggerDelay[i].command() == commandtypes::RETRIGGER)
					{
						if (TriggerDelayCounter[i] == nextevent)
						{
							// do event
							Tick(i, TriggerDelay[i] );
							TriggerDelayCounter[i] = static_cast<int>( (RetriggerRate[i]*timeInfo.samplesPerTick())/256 );
						}
						else
						{
							TriggerDelayCounter[i] -= nextevent;
						}
					}
					else if (TriggerDelay[i].command() == commandtypes::RETR_CONT)
					{
						if (TriggerDelayCounter[i] == nextevent)
						{
							// do event
							Tick(i, TriggerDelay[i] );
							TriggerDelayCounter[i] = static_cast<int>( (RetriggerRate[i]*timeInfo.samplesPerTick())/256 );
							int parameter = TriggerDelay[i].parameter() & 0x0f;
							if (parameter < 9)
							{
								RetriggerRate[i]+= 4*parameter;
							}
							else
							{
								RetriggerRate[i]-= 2*(16-parameter);
								if (RetriggerRate[i] < 16)
								{
									RetriggerRate[i] = 16;
								}
							}
						}
						else
						{
							TriggerDelayCounter[i] -= nextevent;
						}
					}
					else if (TriggerDelay[i].command() == commandtypes::ARPEGGIO)
					{
						if (TriggerDelayCounter[i] == nextevent)
						{
							PatternEvent entry = TriggerDelay[i];
							switch(ArpeggioCount[i])
							{
							case 0:
								Tick(i,TriggerDelay[i]);
								ArpeggioCount[i]++;
								break;
							case 1:
								entry.setNote(entry.note()+((TriggerDelay[i].parameter()&0xF0)>>4));
								Tick(i,entry);
								ArpeggioCount[i]++;
								break;
							case 2:
								entry.setNote(entry.note()+(TriggerDelay[i].parameter()&0x0F));
								Tick(i,entry);
								ArpeggioCount[i]=0;
								break;
							}
							TriggerDelayCounter[i] = timeInfo.samplesPerTick()*timeInfo.ticksSpeed()/24;
						}
						else
						{
							TriggerDelayCounter[i] -= nextevent;
						}
					}
				}
			}
		}

		UpdateVuAndStanbyFlag(numSamples);
	}

	else Standby(true);

	//nanoseconds const t1(cpu_time_clock());
	//accumulate_processing_time(t1 - t0);

	recursive_processed_ = true;
	return numSamples;
}

void XMSampler::WorkVoices(int startSample, int numsamples)
{
	float* psamL = _pSamplesL + startSample;
	float* psamR = _pSamplesR + startSample;
	int origsamples = numsamples;
	//////////////////////////////////////////////////////////////////////////
	//  If there is a tick change in this "numsamples" period, process it.
	if ( _sampleCounter + numsamples > NextSampleTick())
	{
		// Work the remaining samples
		int remainingticks = NextSampleTick()-_sampleCounter;
		for (int voice = 0; voice < _numVoices; voice++)
		{
			//VoiceWork(ns, voice);
			if(m_Voices[voice].IsPlaying()){
				m_Voices[voice].Work(remainingticks, psamL, psamR, resampler_);
			}
		}
		// Do the Tick jump.
		m_NextSampleTick += callbacks->timeInfo().samplesPerTick();
		m_TickCount++;
		for (int channel=0; channel<MAX_TRACKS; channel++)
		{
			rChannel(channel).PerformFx();
		}
		psamL+=remainingticks;
		psamR+=remainingticks;
		numsamples-=remainingticks;
		_sampleCounter+=remainingticks;
	}
	// Process the samples for each voice.
	if ( numsamples != 0 )
	{
		for (int voice = 0; voice < _numVoices; voice++)
		{
			//VoiceWork(ns, voice);
			if(m_Voices[voice].IsPlaying()){
				m_Voices[voice].Work(numsamples,psamL,psamR, resampler_);
			}
		}
	}
	// Doing this here is faster than in voice, because it would be done once per voice then.
	float const multip = m_GlobalVolume / 128.0f;
	psamL = _pSamplesL + startSample;
	psamR = _pSamplesR + startSample;
	for(int i = 0; i < origsamples; ++i) {
		*psamL *= multip; ++psamL;
		*psamR *= multip; ++psamR;
	}
	_sampleCounter+=numsamples;
}

void XMSampler::Stop(void)
{
	//\todo: check that all needed variables/objects are reset.
	int i;
	for (i = 0; i < _numVoices; i++)
	{
		m_Voices[i].NoteOffFast();
	}
	for (i = 0; i < MAX_TRACKS; i++)
	{
		rChannel(i).Restore();
	}
}// XMSampler::Stop(void)

int XMSampler::CalcLPBFromSpeed(int trackerspeed, int &outextraticks)
{
	 int lpb;
	 if (trackerspeed == 0) {
		 trackerspeed = 6;
		 lpb = 4;
	 }
	 else if (trackerspeed == 5) {
		 lpb = 6;
	 }
	 else {
		 lpb = ceil(24.f/trackerspeed);
	 }
	 outextraticks = trackerspeed - 24/lpb;
	 return lpb;
}

bool XMSampler::LoadPsy2FileFormat(RiffFile* /*riffFile*/)
{
	assert(false);
	//The function "Load()" is used for Songs made with Psycle earlier than 1.7
	// It cannot happen that one of those has this new Sampler.
	return false;
}


void XMSampler::SaveSpecificChunk(RiffFile* riffFile) const
{
	int temp;
	// we cannot calculate the size previous to save, so we write a placeholder
	// and seek back to write the correct value.
	//size is saved in 32bits.
	unsigned int size = 0;
	size_t filepos = riffFile->GetPos();
	riffFile->Write(size);
	riffFile->Write(VERSION);
	riffFile->Write(_numVoices); // numSubtracks
	switch(resampler_.quality()) {
		case helpers::dsp::resampler::quality::zero_order: temp = 0; break;
		case helpers::dsp::resampler::quality::spline: temp = 2; break;
		case helpers::dsp::resampler::quality::sinc: temp = 3; break;
		case helpers::dsp::resampler::quality::linear: //fallthrough
		default: temp = 1;
	}
	riffFile->Write(temp); // quality

	for (int i=0; i < 128; i++) {
		riffFile->Write(zxxMap[i].mode);
		riffFile->Write(zxxMap[i].value);
	}
	riffFile->Write(m_bAmigaSlides);
	riffFile->Write(m_UseFilters);
	riffFile->Write(m_GlobalVolume);
	riffFile->Write(m_PanningMode);

	for(int i = 0;i < MAX_TRACKS;i++){
		m_Channel[i].Save(*riffFile);
	}

	#if 0
		// Instrument Data Save
		int numInstruments = 0;
		for(int i = 0;i < MAX_INSTRUMENT;i++){
			if(m_Instruments[i].IsEnabled()){
				numInstruments++;
			}
		}

		riffFile->Write(numInstruments);

		for(int i = 0;i < MAX_INSTRUMENT;i++){
			if(m_Instruments[i].IsEnabled()){
				riffFile->Write(i);
				m_Instruments[i].Save(*riffFile);
			}
		}

		// Sample Data Save
		int numSamples = 0;
		for(int i = 0;i < MAX_INSTRUMENT;i++){
			if(m_rWaveLayer[i].WaveLength() != 0){
				numSamples++;
			}
		}

		riffFile->Write(numSamples);

		for(int i = 0;i < MAX_INSTRUMENT;i++){
			if(m_rWaveLayer[i].WaveLength() != 0){
				riffFile->Write(i);
				m_rWaveLayer[i].Save(*riffFile);
			}
		}
	#endif

	size_t endpos = riffFile->GetPos();
	riffFile->Seek(filepos);
	size = static_cast<uint32_t>(endpos - filepos -sizeof(size));
	riffFile->Write(size);
	riffFile->Seek(endpos);

}//void SaveSpecificChunk(RiffFile& riffFile)

bool XMSampler::LoadSpecificChunk(RiffFile* riffFile, int /*version*/)
{
	int temp;
	bool wrongState=false;
	uint32_t filevers= 0;
	size_t filepos;
	//size is read in 32bits;
	int size=0;
	riffFile->Read(size);
	filepos=riffFile->GetPos();
	riffFile->Read(filevers);

	// Check higher bits of version (AAAABBBB).
	// different A, incompatible, different B, compatible
	if ( (filevers&0xFFFF0000) == VERSION_ONE )
	{
		riffFile->Read(_numVoices); // numSubtracks
		riffFile->Read(temp); // quality

		switch(temp) {
			case 2:	resampler_.quality(helpers::dsp::resampler::quality::spline); break;
			case 3:	resampler_.quality(helpers::dsp::resampler::quality::sinc); break;
			case 0:	resampler_.quality(helpers::dsp::resampler::quality::zero_order); break;
			case 1:
			default: resampler_.quality(helpers::dsp::resampler::quality::linear);
		}

		for (int i=0; i < 128; i++) {
			riffFile->Read(zxxMap[i].mode);
			riffFile->Read(zxxMap[i].value);
		}

		riffFile->Read(m_bAmigaSlides);
		riffFile->Read(m_UseFilters);
		riffFile->Read(m_GlobalVolume);
		riffFile->Read(m_PanningMode);

		for(int i = 0;i < MAX_TRACKS;i++) m_Channel[i].Load(*riffFile);
		#if 0
			//This has been moved to the PSY3 loader
			// Instrument Data Load
			int numInstruments;
			riffFile->Read(numInstruments);
			int idx;
			for(int i = 0;i < numInstruments;i++)
			{
				riffFile->Read(idx);
				if (!m_Instruments[idx].Load(*riffFile)) { wrongState=true; break; }
				//m_Instruments[idx].IsEnabled(true); // done in the loader.
			}
			if (!wrongState)
			{
				int numSamples;
				riffFile->Read(numSamples);
				int idx;
				for(int i = 0;i < numSamples;i++)
				{
					riffFile->Read(idx);
					if (!m_rWaveLayer[idx].Load(*riffFile)) { wrongState=true; break; }
				}
			}
		#endif
	}
	else wrongState = true;

	if (!wrongState) return true;
	else
	{
		riffFile->Seek(filepos+size);
		return false;
	}
}

}}
