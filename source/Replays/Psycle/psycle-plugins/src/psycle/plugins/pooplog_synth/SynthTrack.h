// SynthTrack.h: interface for the CSynthTrack class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "filter.h"
#include <psycle/helpers/math.hpp>
#ifdef SYNTH_LIGHT
#ifdef SYNTH_ULTRALIGHT
namespace psycle::plugins::pooplog_synth_ultralight {
#else
namespace psycle::plugins::pooplog_synth_light {
#endif
#else
namespace psycle::plugins::pooplog_synth {
#endif
    using namespace psycle::helpers::math;

#define OVERDRIVEDIVISOR 512.0f

#define FILTER_CALC_TIME				32
#define FASTRELEASETICKS				8
#define MAXANTIALIAS 32

#ifdef SYNTH_LIGHT
#define MAXOSC 4
#else
#define MAXOSC 6
#endif
#ifdef SYNTH_ULTRALIGHT
#define MAXWAVE 17
#define WHITENOISEGEN 13
#define BROWNNOISEGEN 15
#else
#define MAXWAVE 32
#define WHITENOISEGEN 26
#define BROWNNOISEGEN 29
#endif

#define MAX_VCF_CUTOFF 301
#define MAX_VCF_CUTOFF_22050 263

#define TWOPI																6.28318530717958647692528676655901f
#define MAXARP 37
#define BASEOVERDRIVE 9
#define SAMPLE_LENGTH  4096

#define MAX_ENV_TIME				16384
#define MIN_ENV_TIME				1
#define MAX_RATE								4096
#define NUMCOLUMNS 5
#define MAXVCFMODE 21
#define MAXLFOWAVE 15
#define MAXPHASEMIX 9
#define MAXOVERDRIVEMETHOD 18
#define MAXENVTYPE 2
#define MAXVCFTYPE 42
#define MAXMOOG 6
#define MAXFILTER 26
#define MAXVCF 2

#define MAXSYNCMODES				12

#define TFX_PitchUp												0x01
#define TFX_PitchDown								0x02
#define TFX_Portamento								0x03
#define TFX_Vibrato												0x04
#define TFX_BypassEnv								0x05
#define TFX_PanLeft												0x06
#define TFX_PanRight								0x07
#define TFX_Pan																0x08
#define TFX_PanDest												0x09
#define TFX_VolUp												0x0a
#define TFX_VolDown												0x0b
#define TFX_Vol																0x0c
#define TFX_VolDest												0x0d
#define TFX_NoteCut												0x0e
#define TFX_Cancel												0x0f

#define TFX_CutoffUp								0x10
#define TFX_CutoffDown								0x11
#define TFX_Cutoff												0x12
#define TFX_CutoffDest								0x13

#define TFX_NoteDelay								0x1c
#define TFX_NoteRetrig								0x1d
#define TFX_TrackerArpeggioRate 0x1e
#define TFX_TrackerArpeggio 0x1f


#define TFX_OSC_WavePhase												0x20
#define TFX_OSC_WidthLFOPhase								0x30
#define TFX_OSC_PhaseLFOPhase								0x40
#define TFX_OSC_FrqLFOPhase												0x50
#define TFX_VCF_LFOPhase												0x60
#define TFX_Gain_LFOPhase												0x70
#define TFX_Tremolo_LFOPhase								0x71
#define TFX_Vibrato_LFOPhase								0x72


#define SOLVE_A(x)  float(std::pow(2.0f,((7.0f-std::log((float)x)/std::log(2.0f)))))
#define SOLVE_A_LOW(x)  float(SOLVE_A(x)*(SAMPLE_LENGTH/2048))
#define SOLVE_A_HIGH(x) float(1.0/(2.0-(1.0/SOLVE_A(x)))*(SAMPLE_LENGTH/2048))
#define WRAP_AROUND(x) if ((x < 0) || (x >= SAMPLE_LENGTH*2)) x = (x-rint<int>(x))+(rint<int>(x)&((SAMPLE_LENGTH*2)-1));

		

extern int song_freq;
extern float freq_mul;
extern float max_vcf_cutoff;

extern int vibrato_delay;
#ifndef SYNTH_LIGHT
extern int tremolo_delay;
#endif

extern float SourceWaveTable[MAXLFOWAVE+1][(SAMPLE_LENGTH*2)+256];
extern float SyncAdd[MAXSYNCMODES+1];

#ifndef SYNTH_ULTRALIGHT
const float WidthMultiplierA[257] = 
{
	SOLVE_A_LOW(  0.001),
	SOLVE_A_LOW(  1),SOLVE_A_LOW(  2),SOLVE_A_LOW(  3),SOLVE_A_LOW(  4),SOLVE_A_LOW(  5),SOLVE_A_LOW(  6),SOLVE_A_LOW(  7),SOLVE_A_LOW(  8),
	SOLVE_A_LOW(  9),SOLVE_A_LOW( 10),SOLVE_A_LOW( 11),SOLVE_A_LOW( 12),SOLVE_A_LOW( 13),SOLVE_A_LOW( 14),SOLVE_A_LOW( 15),SOLVE_A_LOW( 16),
	SOLVE_A_LOW( 17),SOLVE_A_LOW( 18),SOLVE_A_LOW( 19),SOLVE_A_LOW( 20),SOLVE_A_LOW( 21),SOLVE_A_LOW( 22),SOLVE_A_LOW( 23),SOLVE_A_LOW( 24),
	SOLVE_A_LOW( 25),SOLVE_A_LOW( 26),SOLVE_A_LOW( 27),SOLVE_A_LOW( 28),SOLVE_A_LOW( 29),SOLVE_A_LOW( 30),SOLVE_A_LOW( 31),SOLVE_A_LOW( 32),
	SOLVE_A_LOW( 33),SOLVE_A_LOW( 34),SOLVE_A_LOW( 35),SOLVE_A_LOW( 36),SOLVE_A_LOW( 37),SOLVE_A_LOW( 38),SOLVE_A_LOW( 39),SOLVE_A_LOW( 40),
	SOLVE_A_LOW( 41),SOLVE_A_LOW( 42),SOLVE_A_LOW( 43),SOLVE_A_LOW( 44),SOLVE_A_LOW( 45),SOLVE_A_LOW( 46),SOLVE_A_LOW( 47),SOLVE_A_LOW( 48),
	SOLVE_A_LOW( 49),SOLVE_A_LOW( 50),SOLVE_A_LOW( 51),SOLVE_A_LOW( 52),SOLVE_A_LOW( 53),SOLVE_A_LOW( 54),SOLVE_A_LOW( 55),SOLVE_A_LOW( 56),
	SOLVE_A_LOW( 57),SOLVE_A_LOW( 58),SOLVE_A_LOW( 59),SOLVE_A_LOW( 60),SOLVE_A_LOW( 61),SOLVE_A_LOW( 62),SOLVE_A_LOW( 63),SOLVE_A_LOW( 64),
	SOLVE_A_LOW( 65),SOLVE_A_LOW( 66),SOLVE_A_LOW( 67),SOLVE_A_LOW( 68),SOLVE_A_LOW( 69),SOLVE_A_LOW( 70),SOLVE_A_LOW( 71),SOLVE_A_LOW( 72),
	SOLVE_A_LOW( 73),SOLVE_A_LOW( 74),SOLVE_A_LOW( 75),SOLVE_A_LOW( 76),SOLVE_A_LOW( 77),SOLVE_A_LOW( 78),SOLVE_A_LOW( 79),SOLVE_A_LOW( 80),
	SOLVE_A_LOW( 81),SOLVE_A_LOW( 82),SOLVE_A_LOW( 83),SOLVE_A_LOW( 84),SOLVE_A_LOW( 85),SOLVE_A_LOW( 86),SOLVE_A_LOW( 87),SOLVE_A_LOW( 88),
	SOLVE_A_LOW( 89),SOLVE_A_LOW( 90),SOLVE_A_LOW( 91),SOLVE_A_LOW( 92),SOLVE_A_LOW( 93),SOLVE_A_LOW( 94),SOLVE_A_LOW( 95),SOLVE_A_LOW( 96),
	SOLVE_A_LOW( 97),SOLVE_A_LOW( 98),SOLVE_A_LOW( 99),SOLVE_A_LOW(100),SOLVE_A_LOW(101),SOLVE_A_LOW(102),SOLVE_A_LOW(103),SOLVE_A_LOW(104),
	SOLVE_A_LOW(105),SOLVE_A_LOW(106),SOLVE_A_LOW(107),SOLVE_A_LOW(108),SOLVE_A_LOW(109),SOLVE_A_LOW(110),SOLVE_A_LOW(111),SOLVE_A_LOW(112),
	SOLVE_A_LOW(113),SOLVE_A_LOW(114),SOLVE_A_LOW(115),SOLVE_A_LOW(116),SOLVE_A_LOW(117),SOLVE_A_LOW(118),SOLVE_A_LOW(119),SOLVE_A_LOW(120),
	SOLVE_A_LOW(121),SOLVE_A_LOW(122),SOLVE_A_LOW(123),SOLVE_A_LOW(124),SOLVE_A_LOW(125),SOLVE_A_LOW(126),SOLVE_A_LOW(127),
	(SAMPLE_LENGTH/2048),
	SOLVE_A_HIGH(128-  1),SOLVE_A_HIGH(128-  2),SOLVE_A_HIGH(128-  3),SOLVE_A_HIGH(128-  4),SOLVE_A_HIGH(128-  5),SOLVE_A_HIGH(128-  6),SOLVE_A_HIGH(128-  7),SOLVE_A_HIGH(128-  8),
	SOLVE_A_HIGH(128-  9),SOLVE_A_HIGH(128- 10),SOLVE_A_HIGH(128- 11),SOLVE_A_HIGH(128- 12),SOLVE_A_HIGH(128- 13),SOLVE_A_HIGH(128- 14),SOLVE_A_HIGH(128- 15),SOLVE_A_HIGH(128- 16),
	SOLVE_A_HIGH(128- 17),SOLVE_A_HIGH(128- 18),SOLVE_A_HIGH(128- 19),SOLVE_A_HIGH(128- 20),SOLVE_A_HIGH(128- 21),SOLVE_A_HIGH(128- 22),SOLVE_A_HIGH(128- 23),SOLVE_A_HIGH(128- 24),
	SOLVE_A_HIGH(128- 25),SOLVE_A_HIGH(128- 26),SOLVE_A_HIGH(128- 27),SOLVE_A_HIGH(128- 28),SOLVE_A_HIGH(128- 29),SOLVE_A_HIGH(128- 30),SOLVE_A_HIGH(128- 31),SOLVE_A_HIGH(128- 32),
	SOLVE_A_HIGH(128- 33),SOLVE_A_HIGH(128- 34),SOLVE_A_HIGH(128- 35),SOLVE_A_HIGH(128- 36),SOLVE_A_HIGH(128- 37),SOLVE_A_HIGH(128- 38),SOLVE_A_HIGH(128- 39),SOLVE_A_HIGH(128- 40),
	SOLVE_A_HIGH(128- 41),SOLVE_A_HIGH(128- 42),SOLVE_A_HIGH(128- 43),SOLVE_A_HIGH(128- 44),SOLVE_A_HIGH(128- 45),SOLVE_A_HIGH(128- 46),SOLVE_A_HIGH(128- 47),SOLVE_A_HIGH(128- 48),
	SOLVE_A_HIGH(128- 49),SOLVE_A_HIGH(128- 50),SOLVE_A_HIGH(128- 51),SOLVE_A_HIGH(128- 52),SOLVE_A_HIGH(128- 53),SOLVE_A_HIGH(128- 54),SOLVE_A_HIGH(128- 55),SOLVE_A_HIGH(128- 56),
	SOLVE_A_HIGH(128- 57),SOLVE_A_HIGH(128- 58),SOLVE_A_HIGH(128- 59),SOLVE_A_HIGH(128- 60),SOLVE_A_HIGH(128- 61),SOLVE_A_HIGH(128- 62),SOLVE_A_HIGH(128- 63),SOLVE_A_HIGH(128- 64),
	SOLVE_A_HIGH(128- 65),SOLVE_A_HIGH(128- 66),SOLVE_A_HIGH(128- 67),SOLVE_A_HIGH(128- 68),SOLVE_A_HIGH(128- 69),SOLVE_A_HIGH(128- 70),SOLVE_A_HIGH(128- 71),SOLVE_A_HIGH(128- 72),
	SOLVE_A_HIGH(128- 73),SOLVE_A_HIGH(128- 74),SOLVE_A_HIGH(128- 75),SOLVE_A_HIGH(128- 76),SOLVE_A_HIGH(128- 77),SOLVE_A_HIGH(128- 78),SOLVE_A_HIGH(128- 79),SOLVE_A_HIGH(128- 80),
	SOLVE_A_HIGH(128- 81),SOLVE_A_HIGH(128- 82),SOLVE_A_HIGH(128- 83),SOLVE_A_HIGH(128- 84),SOLVE_A_HIGH(128- 85),SOLVE_A_HIGH(128- 86),SOLVE_A_HIGH(128- 87),SOLVE_A_HIGH(128- 88),
	SOLVE_A_HIGH(128- 89),SOLVE_A_HIGH(128- 90),SOLVE_A_HIGH(128- 91),SOLVE_A_HIGH(128- 92),SOLVE_A_HIGH(128- 93),SOLVE_A_HIGH(128- 94),SOLVE_A_HIGH(128- 95),SOLVE_A_HIGH(128- 96),
	SOLVE_A_HIGH(128- 97),SOLVE_A_HIGH(128- 98),SOLVE_A_HIGH(128- 99),SOLVE_A_HIGH(128-100),SOLVE_A_HIGH(128-101),SOLVE_A_HIGH(128-102),SOLVE_A_HIGH(128-103),SOLVE_A_HIGH(128-104),
	SOLVE_A_HIGH(128-105),SOLVE_A_HIGH(128-106),SOLVE_A_HIGH(128-107),SOLVE_A_HIGH(128-108),SOLVE_A_HIGH(128-109),SOLVE_A_HIGH(128-110),SOLVE_A_HIGH(128-111),SOLVE_A_HIGH(128-112),
	SOLVE_A_HIGH(128-113),SOLVE_A_HIGH(128-114),SOLVE_A_HIGH(128-115),SOLVE_A_HIGH(128-116),SOLVE_A_HIGH(128-117),SOLVE_A_HIGH(128-118),SOLVE_A_HIGH(128-119),SOLVE_A_HIGH(128-120),
	SOLVE_A_HIGH(128-121),SOLVE_A_HIGH(128-122),SOLVE_A_HIGH(128-123),SOLVE_A_HIGH(128-124),SOLVE_A_HIGH(128-125),SOLVE_A_HIGH(128-126),SOLVE_A_HIGH(128-127),
	SOLVE_A_HIGH(  0.001),
};

const float WidthMultiplierB[257] = 
{
	SOLVE_A_HIGH(  0.001),
	SOLVE_A_HIGH(  1),SOLVE_A_HIGH(  2),SOLVE_A_HIGH(  3),SOLVE_A_HIGH(  4),SOLVE_A_HIGH(  5),SOLVE_A_HIGH(  6),SOLVE_A_HIGH(  7),SOLVE_A_HIGH(  8),
	SOLVE_A_HIGH(  9),SOLVE_A_HIGH( 10),SOLVE_A_HIGH( 11),SOLVE_A_HIGH( 12),SOLVE_A_HIGH( 13),SOLVE_A_HIGH( 14),SOLVE_A_HIGH( 15),SOLVE_A_HIGH( 16),
	SOLVE_A_HIGH( 17),SOLVE_A_HIGH( 18),SOLVE_A_HIGH( 19),SOLVE_A_HIGH( 20),SOLVE_A_HIGH( 21),SOLVE_A_HIGH( 22),SOLVE_A_HIGH( 23),SOLVE_A_HIGH( 24),
	SOLVE_A_HIGH( 25),SOLVE_A_HIGH( 26),SOLVE_A_HIGH( 27),SOLVE_A_HIGH( 28),SOLVE_A_HIGH( 29),SOLVE_A_HIGH( 30),SOLVE_A_HIGH( 31),SOLVE_A_HIGH( 32),
	SOLVE_A_HIGH( 33),SOLVE_A_HIGH( 34),SOLVE_A_HIGH( 35),SOLVE_A_HIGH( 36),SOLVE_A_HIGH( 37),SOLVE_A_HIGH( 38),SOLVE_A_HIGH( 39),SOLVE_A_HIGH( 40),
	SOLVE_A_HIGH( 41),SOLVE_A_HIGH( 42),SOLVE_A_HIGH( 43),SOLVE_A_HIGH( 44),SOLVE_A_HIGH( 45),SOLVE_A_HIGH( 46),SOLVE_A_HIGH( 47),SOLVE_A_HIGH( 48),
	SOLVE_A_HIGH( 49),SOLVE_A_HIGH( 50),SOLVE_A_HIGH( 51),SOLVE_A_HIGH( 52),SOLVE_A_HIGH( 53),SOLVE_A_HIGH( 54),SOLVE_A_HIGH( 55),SOLVE_A_HIGH( 56),
	SOLVE_A_HIGH( 57),SOLVE_A_HIGH( 58),SOLVE_A_HIGH( 59),SOLVE_A_HIGH( 60),SOLVE_A_HIGH( 61),SOLVE_A_HIGH( 62),SOLVE_A_HIGH( 63),SOLVE_A_HIGH( 64),
	SOLVE_A_HIGH( 65),SOLVE_A_HIGH( 66),SOLVE_A_HIGH( 67),SOLVE_A_HIGH( 68),SOLVE_A_HIGH( 69),SOLVE_A_HIGH( 70),SOLVE_A_HIGH( 71),SOLVE_A_HIGH( 72),
	SOLVE_A_HIGH( 73),SOLVE_A_HIGH( 74),SOLVE_A_HIGH( 75),SOLVE_A_HIGH( 76),SOLVE_A_HIGH( 77),SOLVE_A_HIGH( 78),SOLVE_A_HIGH( 79),SOLVE_A_HIGH( 80),
	SOLVE_A_HIGH( 81),SOLVE_A_HIGH( 82),SOLVE_A_HIGH( 83),SOLVE_A_HIGH( 84),SOLVE_A_HIGH( 85),SOLVE_A_HIGH( 86),SOLVE_A_HIGH( 87),SOLVE_A_HIGH( 88),
	SOLVE_A_HIGH( 89),SOLVE_A_HIGH( 90),SOLVE_A_HIGH( 91),SOLVE_A_HIGH( 92),SOLVE_A_HIGH( 93),SOLVE_A_HIGH( 94),SOLVE_A_HIGH( 95),SOLVE_A_HIGH( 96),
	SOLVE_A_HIGH( 97),SOLVE_A_HIGH( 98),SOLVE_A_HIGH( 99),SOLVE_A_HIGH(100),SOLVE_A_HIGH(101),SOLVE_A_HIGH(102),SOLVE_A_HIGH(103),SOLVE_A_HIGH(104),
	SOLVE_A_HIGH(105),SOLVE_A_HIGH(106),SOLVE_A_HIGH(107),SOLVE_A_HIGH(108),SOLVE_A_HIGH(109),SOLVE_A_HIGH(110),SOLVE_A_HIGH(111),SOLVE_A_HIGH(112),
	SOLVE_A_HIGH(113),SOLVE_A_HIGH(114),SOLVE_A_HIGH(115),SOLVE_A_HIGH(116),SOLVE_A_HIGH(117),SOLVE_A_HIGH(118),SOLVE_A_HIGH(119),SOLVE_A_HIGH(120),
	SOLVE_A_HIGH(121),SOLVE_A_HIGH(122),SOLVE_A_HIGH(123),SOLVE_A_HIGH(124),SOLVE_A_HIGH(125),SOLVE_A_HIGH(126),SOLVE_A_HIGH(127),
	(SAMPLE_LENGTH/2048),
	SOLVE_A_LOW(128-  1),SOLVE_A_LOW(128-  2),SOLVE_A_LOW(128-  3),SOLVE_A_LOW(128-  4),SOLVE_A_LOW(128-  5),SOLVE_A_LOW(128-  6),SOLVE_A_LOW(128-  7),SOLVE_A_LOW(128-  8),
	SOLVE_A_LOW(128-  9),SOLVE_A_LOW(128- 10),SOLVE_A_LOW(128- 11),SOLVE_A_LOW(128- 12),SOLVE_A_LOW(128- 13),SOLVE_A_LOW(128- 14),SOLVE_A_LOW(128- 15),SOLVE_A_LOW(128- 16),
	SOLVE_A_LOW(128- 17),SOLVE_A_LOW(128- 18),SOLVE_A_LOW(128- 19),SOLVE_A_LOW(128- 20),SOLVE_A_LOW(128- 21),SOLVE_A_LOW(128- 22),SOLVE_A_LOW(128- 23),SOLVE_A_LOW(128- 24),
	SOLVE_A_LOW(128- 25),SOLVE_A_LOW(128- 26),SOLVE_A_LOW(128- 27),SOLVE_A_LOW(128- 28),SOLVE_A_LOW(128- 29),SOLVE_A_LOW(128- 30),SOLVE_A_LOW(128- 31),SOLVE_A_LOW(128- 32),
	SOLVE_A_LOW(128- 33),SOLVE_A_LOW(128- 34),SOLVE_A_LOW(128- 35),SOLVE_A_LOW(128- 36),SOLVE_A_LOW(128- 37),SOLVE_A_LOW(128- 38),SOLVE_A_LOW(128- 39),SOLVE_A_LOW(128- 40),
	SOLVE_A_LOW(128- 41),SOLVE_A_LOW(128- 42),SOLVE_A_LOW(128- 43),SOLVE_A_LOW(128- 44),SOLVE_A_LOW(128- 45),SOLVE_A_LOW(128- 46),SOLVE_A_LOW(128- 47),SOLVE_A_LOW(128- 48),
	SOLVE_A_LOW(128- 49),SOLVE_A_LOW(128- 50),SOLVE_A_LOW(128- 51),SOLVE_A_LOW(128- 52),SOLVE_A_LOW(128- 53),SOLVE_A_LOW(128- 54),SOLVE_A_LOW(128- 55),SOLVE_A_LOW(128- 56),
	SOLVE_A_LOW(128- 57),SOLVE_A_LOW(128- 58),SOLVE_A_LOW(128- 59),SOLVE_A_LOW(128- 60),SOLVE_A_LOW(128- 61),SOLVE_A_LOW(128- 62),SOLVE_A_LOW(128- 63),SOLVE_A_LOW(128- 64),
	SOLVE_A_LOW(128- 65),SOLVE_A_LOW(128- 66),SOLVE_A_LOW(128- 67),SOLVE_A_LOW(128- 68),SOLVE_A_LOW(128- 69),SOLVE_A_LOW(128- 70),SOLVE_A_LOW(128- 71),SOLVE_A_LOW(128- 72),
	SOLVE_A_LOW(128- 73),SOLVE_A_LOW(128- 74),SOLVE_A_LOW(128- 75),SOLVE_A_LOW(128- 76),SOLVE_A_LOW(128- 77),SOLVE_A_LOW(128- 78),SOLVE_A_LOW(128- 79),SOLVE_A_LOW(128- 80),
	SOLVE_A_LOW(128- 81),SOLVE_A_LOW(128- 82),SOLVE_A_LOW(128- 83),SOLVE_A_LOW(128- 84),SOLVE_A_LOW(128- 85),SOLVE_A_LOW(128- 86),SOLVE_A_LOW(128- 87),SOLVE_A_LOW(128- 88),
	SOLVE_A_LOW(128- 89),SOLVE_A_LOW(128- 90),SOLVE_A_LOW(128- 91),SOLVE_A_LOW(128- 92),SOLVE_A_LOW(128- 93),SOLVE_A_LOW(128- 94),SOLVE_A_LOW(128- 95),SOLVE_A_LOW(128- 96),
	SOLVE_A_LOW(128- 97),SOLVE_A_LOW(128- 98),SOLVE_A_LOW(128- 99),SOLVE_A_LOW(128-100),SOLVE_A_LOW(128-101),SOLVE_A_LOW(128-102),SOLVE_A_LOW(128-103),SOLVE_A_LOW(128-104),
	SOLVE_A_LOW(128-105),SOLVE_A_LOW(128-106),SOLVE_A_LOW(128-107),SOLVE_A_LOW(128-108),SOLVE_A_LOW(128-109),SOLVE_A_LOW(128-110),SOLVE_A_LOW(128-111),SOLVE_A_LOW(128-112),
	SOLVE_A_LOW(128-113),SOLVE_A_LOW(128-114),SOLVE_A_LOW(128-115),SOLVE_A_LOW(128-116),SOLVE_A_LOW(128-117),SOLVE_A_LOW(128-118),SOLVE_A_LOW(128-119),SOLVE_A_LOW(128-120),
	SOLVE_A_LOW(128-121),SOLVE_A_LOW(128-122),SOLVE_A_LOW(128-123),SOLVE_A_LOW(128-124),SOLVE_A_LOW(128-125),SOLVE_A_LOW(128-126),SOLVE_A_LOW(128-127),
	SOLVE_A_LOW(  0.001),
};
#endif

#ifndef SYNTH_LIGHT
const signed char ArpNote[MAXARP][16] = {

	// basic major 1
	{0,4,7,12,0,4,7,12,0,4,7,12,0,4,7,12},

	// basic major 2
	{0,4,7,12,16,12,7,4,0,4,7,12,16,12,7,4},

	// basic major 3
	{0,-5,0,4,7,12,7,4,0,-5,0,4,7,12,7,4},

	// basic minor 1
	{0,3,7,12,0,3,7,12,0,3,7,12,0,3,7,12},

	// basic minor 2
	{0,3,7,12,15,12,7,3,0,3,7,12,15,12,7,3},

	// basic minor 3
	{0,-5,0,3,7,12,7,3,0,-5,0,3,7,12,7,3},

	// minor 1
	{0,3,7,12,15,19,24,27,31,36,39,43,48,51,55,60},

	// major 1
	{0,4,7,12,16,19,24,28,31,36,40,43,48,52,55,60},

	// minor 2
	{0,3,7,10,12,15,19,22,24,27,31,34,36,39,43,46},

	// major 2
	{0,4,7,10,12,16,19,22,24,28,31,34,36,40,43,46},

	// riffer 1
	{0,12,0,-12,12,0,12,0,0,12,-12,0,12,0,12,-12},

	// riffer 2
	{0,12,24,0,12,24,12,0,24,12,0,0,12,0,0,24},

	// riffer 3
	{0,12,19,0,0,7,0,7,0,12,19,0,12,19,0,-12},

	// minor bounce
	{0,3,7,12,15,19,24,27,31,27,24,19,15,12,7,3},

	// major bounce
	{0,4,7,12,16,19,24,28,31,28,24,19,16,12,7,4},

	// major pentatonic C D E G A C
	{0,2,4,7,9,12,7,2,9,4,0,12,9,7,4,2},

	// minor pentatonic C d# F G a# C
	{0,3,5,7,8,12,7,3,8,5,0,12,8,7,5,3},

	// blues C d# F f# G a# C
	{0,3,5,6,7,10,12,7,5,6,3,12,10,7,6,5},

	// dorian C D d# F G A a# C
	{0,2,3,5,7,9,10,12,10,9,7,5,3,2,0,2},

	// phrygian C c# d# F G g# a# C
	{0,1,3,5,7,10,12,10,3,7,5,1,10,3,5,7},

	// lydian C D E F# G A B C
	{0,2,4,6,7,9,12,9,7,6,4,6,4,2,4,2},

	// mixolydian C D E F G A a# C
	{0,2,4,5,7,10,12,10,5,2,7,4,0,10,2,4},

	// locrian C c# d# F f# g# a# C
	{0,1,3,5,6,8,10,12,10,6,3,8,5,1,8,6},

	// melodic minor C D Eb F G A B C
	{0,2,3,5,7,9,11,12,9,5,2,11,7,3,9,3},

	// harmonic minor C D Eb F G Ab B C
	{0,2,3,5,7,8,11,12,8,5,2,11,7,3,2,5},

	// major (Ionian) c d e f g a b c
	{0,2,4,5,7,9,11,12,9,5,2,11,7,4,5,2},

	// minor (Aeolian) C D Eb F G Ab Bb C
	{0,2,3,5,7,8,10,12,10,7,3,2,8,5,3,2},

	// whole tone C D E F# G# A Bb C
	{0,2,4,6,8,9,10,12,10,9,8,6,4,2,10,8},

	// half-whole diminished - C Db D# E F# G A Bb C
	{0, 1,3,4,6,7,9,10,12,9,4,1,3,6,7,9},

	// whole-half diminished - C D Eb F Gb Ab A B C
	{0,2,3,5,6,8,9,11,12,8,5,2,3,9,5,2},

	// c 0
	// c# 1
	// d 2
	// d# 3
	// e 4
	// f 5
	// f# 6
	// g 7
	// g# 8
	// a 9
	// a# 10
	// b 11
	// c 12

	// ksn one
	{0,9-12,4,0,9-12,4,0,4,0,9-12,4,0,9-12,4,0},

	// ksn two
	{7,0,4,14,12,0,4,2,7,0,4,1,12,0,5,4},

	// ksn three
	{0,10,0,12,10,3,0,7,3,7,0,2,12,3,2,3},

	// ksn four
	{0,0,3,7,0,7,0,3,0,0,0,9,7,0,3,9},

	// ksn five
	{0,0,0,7,0,7,0,7,7,5,0,0,0,0,7,5},

	// ksn six
	{0,3,0,7,2,12,10,3,0,3,0,7,2,15,12,7},
};
#endif

#define VCFPARSKIPSIZE 4
struct VCFPAR
{
	float *pvcflfowave;
	int vcflfowave;
	int vcfenvdelay;
	int vcfenvattack;
	int vcfenvdecay;
	int vcfenvsustain;
	int vcfenvrelease;
	int vcflfospeed;
	int vcflfoamplitude;
	int vcfcutoff;
	int vcfresonance;
	int vcftype;
	int vcfenvmod;
	int vcfenvtype;
};

struct VCFVALS
{
	int VcfEnvStage;
	float VcfCutoff;
	float vcflfophase;
	float VcfEnvValue;
	float VcfEnvDValue;
	float VcfEnvCoef;
	float VcfEnvDelay;
	float VcfEnvSustainLevel;
};

#ifdef SYNTH_ULTRALIGHT
#define OSCPARSKIPSIZE 4
// ultralight synth
struct OSCPAR
{
	float *pWave;
	int Wave;
	int oscfinetune;
	int osctune;
	unsigned int oscsync;
	int oscvol;
	int oscmixtype;
};

struct OSCVALS
{
	float OSCSpeed;
	float OSCPosition;
	float OSCVol;
	float Last;
	int rand;
};

#define SYNPARSKIPSIZE 8
struct SYNPAR
{
	int version;
	float *pvibrato_wave;
	float *pgain_lfo_wave;
	int curOsc;
	unsigned int curVcf;
	unsigned int vcfmixmode;
	unsigned int vibrato_wave;
	unsigned int gain_lfo_wave;
	int amp_env_attack;
	int amp_env_decay;
	int amp_env_sustain;
	int amp_env_release;
	int out_vol;
	int vibrato_speed;
	int vibrato_amplitude;
	int globaltune;
	int globalfinetune;
	int synthporta;
	unsigned int interpolate;
	int overdrive;
	int song_sync; 
	int overdrivegain;
	OSCPAR gOscp[MAXOSC];
	VCFPAR gVcfp[MAXVCF];
};
#else
#ifndef SYNTH_LIGHT
// normal synth
#define OSCPARSKIPSIZE 20
struct OSCPAR
{
	float *pWave[2];
	float *poscplfowave;
	float *poscwlfowave;
	float *poscflfowave;
	int Wave[2];
	int oscplfowave;
	int oscwlfowave;
	int oscflfowave;
	int oscwidth;
	int oscphasemix;
	int oscphase;
	int oscplfospeed;
	int oscplfoamplitude;
	int oscwlfospeed;
	int oscwlfoamplitude;
	int oscflfospeed;
	int oscflfoamplitude;
	int oscpenvmod;
	int oscpenvtype;
	int oscpdelay;
	int oscpattack;
	int oscpdecay;
	int oscpsustain;
	int oscprelease;
	int oscwenvmod;
	int oscwenvtype;
	int oscwdelay;
	int oscwattack;
	int oscwdecay;
	int oscwsustain;
	int oscwrelease;
	int oscfenvtype;
	int oscfenvmod;
	int oscfdelay;
	int oscfattack;
	int oscfdecay;
	int oscfsustain;
	int oscfrelease;
	int oscfinetune;
	int osctune;
	unsigned int oscsync;
	int oscvol[2];
	int oscmixtype;
};

struct OSCVALS
{
	float OSCSpeed[2];
	float oscplfophase;
	float oscwlfophase;
	float oscflfophase;
	float OSCPosition;
	float OscpEnvValue;
	float OscpEnvDValue;
	float OscpEnvCoef;
	float OscpEnvSustainLevel;
	float OscwEnvValue;
	float OscwEnvDValue;
	float OscwEnvCoef;
	float OscwEnvSustainLevel;
	float OscfEnvValue;
	float OscfEnvDValue;
	float OscfEnvCoef;
	float OscfEnvSustainLevel;
	float OSCVol[2];
	float Last[2];
	int rand[2];
	int OscpEnvStage;
	int OscwEnvStage;
	int OscfEnvStage;
	unsigned int oscdir;
	int oscphase;
	int oscwidth;
};

#define SYNPARSKIPSIZE 12
struct SYNPAR
{
	int version;
	float *ptremolo_wave;
	float *pvibrato_wave;
	float *pgain_lfo_wave;
	int curOsc;
	unsigned int curVcf;
	unsigned int vcfmixmode;
	unsigned int tremolo_wave;
	unsigned int vibrato_wave;
	unsigned int gain_lfo_wave;
	int amp_env_attack;
	int amp_env_decay;
	int amp_env_sustain;
	int amp_env_release;
	int out_vol;
	int arp_mod;
	int arp_bpm;
	unsigned int arp_cnt;
	int tremolo_speed;
	int tremolo_amplitude;
	int vibrato_speed;
	int vibrato_amplitude;
	int gain_env_delay;
	int gain_env_attack;
	int gain_env_decay;
	int gain_env_sustain;
	int gain_env_release;
	int gain_envmod;
	int gain_envtype;
	int gain_lfo_speed;
	int gain_lfo_amplitude;
	int globaltune;
	int globalfinetune;
	int synthporta;
	unsigned int interpolate;
	int overdrive;
	int song_sync; 
	int overdrivegain;
	OSCPAR gOscp[MAXOSC];
	VCFPAR gVcfp[MAXVCF];
};
#else
// light synth
#define OSCPARSKIPSIZE 8
struct OSCPAR
{
	float *pWave[2];
	int Wave[2];
	int oscwidth;
	int oscfinetune;
	int osctune;
	unsigned int oscsync;
	int oscvol[2];
	int oscmixtype;
};

struct OSCVALS
{
	float OSCSpeed[2];
	float OSCPosition;
	float OSCVol[2];
	float Last[2];
	int rand[2];
	unsigned int oscdir;
	int oscphase;
};
#define SYNPARSKIPSIZE 8
struct SYNPAR
{
	int version;
	float *pvibrato_wave;
	float *pgain_lfo_wave;
	int curOsc;
	unsigned int curVcf;
	unsigned int vcfmixmode;
	unsigned int vibrato_wave;
	unsigned int gain_lfo_wave;
	int amp_env_attack;
	int amp_env_decay;
	int amp_env_sustain;
	int amp_env_release;
	int out_vol;
	int vibrato_speed;
	int vibrato_amplitude;
	int gain_env_delay;
	int gain_env_attack;
	int gain_env_decay;
	int gain_env_sustain;
	int gain_env_release;
	int gain_envmod;
	int gain_envtype;
	int gain_lfo_speed;
	int gain_lfo_amplitude;
	int globaltune;
	int globalfinetune;
	int synthporta;
	unsigned int interpolate;
	int overdrive;
	int song_sync; 
	int overdrivegain;
	OSCPAR gOscp[MAXOSC];
	VCFPAR gVcfp[MAXVCF];
};
#endif
#endif

class CSynthTrack  
{
public:

	void InitEffect(int cmd,int val);
	void PerformFx();
	float Filter(float x);
	void NoteOff();
	float GetSample();
#ifndef SYNTH_ULTRALIGHT
	inline float GetInterpolatedSampleOSC(unsigned int osc, unsigned int dir, float Pos);
#else
	inline float GetInterpolatedSampleOSC(unsigned int osc, float Pos);
#endif
	inline float GetPhaseSampleOSCEven();
	inline float GetPhaseSampleOSCOdd();
	inline float GetPhaseSampleOSC(unsigned int osc);
	inline float GetPhaseSampleMix();
	inline float DoFilter(int num, float input);
	inline float HandleOverdrive(float input);

	void NoteOn(int note, int cmd, int val);
	void Init(SYNPAR *tspar);
	
	CSynthTrack();
	virtual ~CSynthTrack();

	int AmpEnvStage;
	int NoteCutTime;
	bool NoteCut;
	filter m_filter[MAXVCF];

	float in1,in2,in3,in4;
	float b1, b2, b3, b4;  //filter buffers (beware denormals!) 

	inline float antialias(float in0) 
	{ 
		in0 = in0* 0.35013f * (1.16f*1.16f)*(1.16f*1.16f); 
		b1 = in0 + (0.3f * in1) + ((1.0f - 1.16f) * b1); // Pole 1 
		in1  = in0; 
		b2 = b1 + (0.3f * in2) + ((1.0f - 1.16f) * b2);  // Pole 2 
		in2  = b1; 
		b3 = b2 + (0.3f * in3) + ((1.0f - 1.16f) * b3);  // Pole 3 
		in3  = b2; 
		b4 = b3 + (0.3f * in4) + ((1.0f - 1.16f) * b4);  // Pole 4 
		// safety check
		/*
		if (b4 > 128.0f) 
			b4 = 128.0f;
		else if (b4 < -128.0f) 
			b4 = -128.0f;
			*/
		//
		in4  = b3; 
	// Lowpass  output:  b4 
		return b4; 
	}

	inline void antialias_reset()
	{
		in1=in2=in3=in4=b1= b2= b3= b4=0;
	}

	float LMix;
	float RMix;
	OSCVALS lOscv[MAXOSC];
	VCFVALS lVcfv[MAXVCF];
#ifndef SYNTH_LIGHT
	int arpflag;
#endif
	int note_delay;
	int note_delay_counter;
	int note;
	int sp_cmd;

private:
#ifndef SYNTH_LIGHT
	float tremolo_phase;
	float TremoloVol;
	float TremoloEnvValue;
	float TremoloEnvCoef;
	int								TremoloEnvStage;
	// Arpeggiator
	float Arp_tickcounter;
	float Arp_samplespertick;
	unsigned int ArpCounter;
#endif
#ifndef SYNTH_ULTRALIGHT
	float gain_lfo_phase;
#endif
	float Arp_basenote;
	float Arp_destnote;


	short timetocompute;
	void InitEnvelopes();

	int sp_val;

	int TrackerArpeggio[3];
	int TrackerArpeggioIndex;
	int TrackerArpeggioRate;
	int TrackerArpeggioCounter;

	float OutVol;
	float TFXVol;
	float OutPan;
	float OutGain;
	
	float vibrato_phase;
	float VibratoSpeed;
	float VibratoAmplitude;
	float VibratoEnvValue;
	float VibratoEnvCoef;
	int VibratoEnvStage;

	// POrtamento
	float PortaCoef;

	// Envelope [Amplitude]
	float AmpEnvValue;
	float AmpEnvCoef;
	float AmpEnvSustainLevel;

#ifndef SYNTH_ULTRALIGHT
	float GainEnvValue;
	float GainEnvDValue;
	float GainEnvCoef;
	float GainEnvSustainLevel;
	int GainEnvStage;
#endif
	
	SYNPAR *syntp;

	inline void ArpTick(void);
	inline void FilterTick(void);

};

inline void CSynthTrack::FilterTick()
{
	unsigned int i;
	timetocompute=FILTER_CALC_TIME-1;
		// porta
	if (PortaCoef < 0.0f)
	{
		Arp_basenote += PortaCoef;
		if (Arp_basenote <= Arp_destnote)
		{
			Arp_basenote = Arp_destnote;
			PortaCoef=0.0f;
		}
	}
	else if (PortaCoef > 0.0f)
	{
		Arp_basenote += PortaCoef;
		if (Arp_basenote >= Arp_destnote)
		{
			Arp_basenote = Arp_destnote;
			PortaCoef=0.0f;
		}
	}
	// vibrato lfo
	float Vib_basenote = Arp_basenote;
	if (VibratoAmplitude)
	{
		vibrato_phase += ((VibratoSpeed)*(VibratoSpeed))*0.000030517f*freq_mul;
		WRAP_AROUND(vibrato_phase);
		Vib_basenote+=((syntp->vibrato_amplitude*0.00390625f)+(float(syntp->pvibrato_wave[rint<int>(vibrato_phase)])
						*0.015625f
						*float(VibratoAmplitude)));
	}
	else if (syntp->vibrato_amplitude)
	{
		switch (VibratoEnvStage)
		{
		case 0:
			// off with delay
			VibratoEnvValue+=VibratoEnvCoef;
			if (VibratoEnvValue>=1.0f)
			{
				VibratoEnvValue = 0;
				VibratoEnvStage = 2;
			}
			break;
		case 2:
			if (syntp->vibrato_speed <= MAXSYNCMODES)
			{
				vibrato_phase += SyncAdd[syntp->vibrato_speed]*freq_mul;
			}
			else
			{
				vibrato_phase += ((syntp->vibrato_speed-MAXSYNCMODES)*(syntp->vibrato_speed-MAXSYNCMODES))*0.000030517f;
			}
			WRAP_AROUND(vibrato_phase);
			Vib_basenote+=((syntp->vibrato_amplitude*0.00390625f)+(float(syntp->pvibrato_wave[rint<int>(vibrato_phase)])
							*0.015625f
							*float(syntp->vibrato_amplitude)));
			break;
		}
	}

#ifndef SYNTH_LIGHT
	if (syntp->arp_mod)
	{
		if (syntp->arp_bpm <= MAXSYNCMODES)
		{
			Arp_tickcounter+=SyncAdd[syntp->arp_bpm];
		}
		else
		{
			Arp_tickcounter+=Arp_samplespertick;
		}
		if (Arp_tickcounter >= SAMPLE_LENGTH*2)
		{
			Arp_tickcounter -= SAMPLE_LENGTH*2;
			if(++ArpCounter>=syntp->arp_cnt)  
			{
				ArpCounter=0;
			}
			if (arpflag)
			{
				InitEnvelopes();
			}
		}
	}
#endif
	// vcf
	for (i = 0; i < MAXVCF; i++)
	{
		if (syntp->gVcfp[i].vcftype)
		{
			float outcutoff = lVcfv[i].VcfCutoff;
			// vcf 1 adsr
			if (syntp->gVcfp[i].vcfenvmod)
			{
				switch(lVcfv[i].VcfEnvStage)
				{
				case 0: // Delay
					lVcfv[i].VcfEnvDValue+=lVcfv[i].VcfEnvCoef;												
					if(lVcfv[i].VcfEnvDValue>1.0f)
					{
						if (syntp->gVcfp[i].vcfenvattack)
						{
							lVcfv[i].VcfEnvValue = 0.0f;
							lVcfv[i].VcfEnvStage=1;
							lVcfv[i].VcfEnvCoef=freq_mul/syntp->gVcfp[i].vcfenvattack;
						}
						else if (syntp->gVcfp[i].vcfenvdecay)
						{
							lVcfv[i].VcfEnvValue = 1.0f;
							lVcfv[i].VcfEnvCoef=freq_mul*(1.0f-lVcfv[i].VcfEnvSustainLevel)/syntp->gVcfp[i].vcfenvdecay;
							lVcfv[i].VcfEnvStage=2;
						}
						else if (syntp->gVcfp[i].vcfenvsustain)
						{
							lVcfv[i].VcfEnvValue=lVcfv[i].VcfEnvSustainLevel;
							lVcfv[i].VcfEnvStage=3;
						}
						else
						{
							lVcfv[i].VcfEnvValue=0.0f;
							lVcfv[i].VcfEnvStage=-1;
						}
					}
					break;

				case 1: // Attack
					lVcfv[i].VcfEnvValue+=lVcfv[i].VcfEnvCoef;
					
					if(lVcfv[i].VcfEnvValue>=1.0f)
					{
						if (syntp->gVcfp[i].vcfenvdecay)
						{
							lVcfv[i].VcfEnvValue = 1.0f;
							lVcfv[i].VcfEnvCoef=freq_mul*(1.0f-lVcfv[i].VcfEnvSustainLevel)/syntp->gVcfp[i].vcfenvdecay;
							lVcfv[i].VcfEnvStage=2;
						}
						else if (syntp->gVcfp[i].vcfenvsustain)
						{
							lVcfv[i].VcfEnvValue=lVcfv[i].VcfEnvSustainLevel;
							lVcfv[i].VcfEnvStage=3;
						}
						else
						{
							lVcfv[i].VcfEnvValue=0.0f;
							lVcfv[i].VcfEnvStage=-1;
						}
					}
					break;

				case 2: // Decay
					lVcfv[i].VcfEnvValue-=lVcfv[i].VcfEnvCoef;
					
					if(lVcfv[i].VcfEnvValue<=lVcfv[i].VcfEnvSustainLevel)
					{
						if (syntp->gVcfp[i].vcfenvsustain)
						{
							lVcfv[i].VcfEnvValue=lVcfv[i].VcfEnvSustainLevel;
							lVcfv[i].VcfEnvStage=3;
						}
						else
						{
							lVcfv[i].VcfEnvValue=0.0f;
							lVcfv[i].VcfEnvStage=-1;
						}																
					}
					break;

				case 4: // Release
					lVcfv[i].VcfEnvValue-=lVcfv[i].VcfEnvCoef;

					if(lVcfv[i].VcfEnvValue<=0.0f)
					{
						lVcfv[i].VcfEnvDValue=0.0f;
						lVcfv[i].VcfEnvStage=-1;
					}
					break;
				}
				if (syntp->gVcfp[i].vcfenvtype == 0) outcutoff+=lVcfv[i].VcfEnvValue*syntp->gVcfp[i].vcfenvmod;
			}
			// vcf lfo
			if ((syntp->gVcfp[i].vcflfoamplitude) || (syntp->gVcfp[i].vcfenvtype && syntp->gVcfp[i].vcfenvmod))
			{
				if (syntp->gVcfp[i].vcfenvtype == 2) lVcfv[i].vcflfophase += lVcfv[i].VcfEnvValue*syntp->gVcfp[i].vcfenvmod*syntp->gVcfp[i].vcfenvmod*64*0.000030517f;
				if (syntp->gVcfp[i].vcflfospeed <= MAXSYNCMODES)
				{
					lVcfv[i].vcflfophase += SyncAdd[syntp->gVcfp[i].vcflfospeed];
				}
				else
				{
					lVcfv[i].vcflfophase += ((syntp->gVcfp[i].vcflfospeed-MAXSYNCMODES)*(syntp->gVcfp[i].vcflfospeed-MAXSYNCMODES))*0.000030517f*freq_mul;
				}
				WRAP_AROUND(lVcfv[i].vcflfophase);
				if (syntp->gVcfp[i].vcfenvtype == 1)
				{
					outcutoff+= (float(syntp->gVcfp[i].pvcflfowave[rint<int>(lVcfv[i].vcflfophase)])
									*(float(syntp->gVcfp[i].vcflfoamplitude)+(lVcfv[i].VcfEnvValue*syntp->gVcfp[i].vcfenvmod)));
				}
				else
				{
					outcutoff+= (float(syntp->gVcfp[i].pvcflfowave[rint<int>(lVcfv[i].vcflfophase)])
									*float(syntp->gVcfp[i].vcflfoamplitude));
				}
			}
			if (outcutoff<0) outcutoff = 0;
			else if (outcutoff>max_vcf_cutoff) outcutoff=max_vcf_cutoff;

			if (syntp->gVcfp[i].vcftype > MAXFILTER)
			{
				m_filter[i].setphaser(syntp->gVcfp[i].vcftype-MAXFILTER,outcutoff,syntp->gVcfp[i].vcfresonance);
			}
			else if (syntp->gVcfp[i].vcftype > MAXMOOG)
			{
				m_filter[i].setfilter((syntp->gVcfp[i].vcftype-MAXMOOG-1)/2,outcutoff,syntp->gVcfp[i].vcfresonance);
			}
			else if (syntp->gVcfp[i].vcftype & 1)
			{
				m_filter[i].setmoog1((syntp->gVcfp[i].vcftype-1)/2,outcutoff,syntp->gVcfp[i].vcfresonance);
			}
			else
			{
				m_filter[i].setmoog2((syntp->gVcfp[i].vcftype-1)/2,outcutoff,syntp->gVcfp[i].vcfresonance);
			}
		}
	}

	for (i = 0; i < MAXOSC; i++)
	{
#ifndef SYNTH_ULTRALIGHT
		if (syntp->gOscp[i].oscvol[0])
#else
		if (syntp->gOscp[i].oscvol)
#endif
		{
#ifndef SYNTH_LIGHT
			float temp;
			// osc 1 phase
			if (syntp->gOscp[i].oscphasemix)
			{
				// osc 1 phase lfo
				temp = 0;
				// osc1 phase adsr
				if (syntp->gOscp[i].oscpenvmod)
				{
					switch(lOscv[i].OscpEnvStage)
					{
					case 0: // Delay
						lOscv[i].OscpEnvDValue+=lOscv[i].OscpEnvCoef;												
						if(lOscv[i].OscpEnvDValue>1.0f)
						{
							if (syntp->gOscp[i].oscpattack)
							{
								lOscv[i].OscpEnvValue = 0.0f;
								lOscv[i].OscpEnvStage=1;
								lOscv[i].OscpEnvCoef=freq_mul/(float)syntp->gOscp[i].oscpattack;
							}
							else if (syntp->gOscp[i].oscpdecay)
							{
								lOscv[i].OscpEnvValue = 1.0f;
								lOscv[i].OscpEnvCoef=freq_mul*(1.0f-lOscv[i].OscpEnvSustainLevel)/(float)syntp->gOscp[i].oscpdecay;
								lOscv[i].OscpEnvStage=2;
							}
							else if (syntp->gOscp[i].oscpsustain)
							{
								lOscv[i].OscpEnvValue=lOscv[i].OscpEnvSustainLevel;
								lOscv[i].OscpEnvStage=3;
							}
							else
							{
								lOscv[i].OscpEnvValue=0.0f;
								lOscv[i].OscpEnvStage=-1;
							}				
						}
						break;
					case 1: // Attack
						lOscv[i].OscpEnvValue+=lOscv[i].OscpEnvCoef;
						
						if(lOscv[i].OscpEnvValue>=1.0f)
						{
							if (syntp->gOscp[i].oscpdecay)
							{
								lOscv[i].OscpEnvValue = 1.0f;
								lOscv[i].OscpEnvCoef=freq_mul*(1.0f-lOscv[i].OscpEnvSustainLevel)/(float)syntp->gOscp[i].oscpdecay;
								lOscv[i].OscpEnvStage=2;
							}
							else if (syntp->gOscp[i].oscpsustain)
							{
								lOscv[i].OscpEnvValue=lOscv[i].OscpEnvSustainLevel;
								lOscv[i].OscpEnvStage=3;
							}
							else
							{
								lOscv[i].OscpEnvValue=0.0f;
								lOscv[i].OscpEnvStage=-1;
							}				
						}
						break;

					case 2: // Decay
						lOscv[i].OscpEnvValue-=lOscv[i].OscpEnvCoef;
						
						if(lOscv[i].OscpEnvValue<=lOscv[i].OscpEnvSustainLevel)
						{
							if (syntp->gOscp[i].oscpsustain)
							{
								lOscv[i].OscpEnvValue=lOscv[i].OscpEnvSustainLevel;
								lOscv[i].OscpEnvStage=3;
							}
							else
							{
								lOscv[i].OscpEnvValue=0.0f;
								lOscv[i].OscpEnvStage=-1;
							}				
						}
						break;

					case 4: // Release
						lOscv[i].OscpEnvValue-=lOscv[i].OscpEnvCoef;

						if(lOscv[i].OscpEnvValue<=0.0f)
						{
							lOscv[i].OscpEnvDValue=0.0f;
							lOscv[i].OscpEnvStage=-1;
						}
						break;
					}
					if (syntp->gOscp[i].oscpenvtype == 0) temp=lOscv[i].OscpEnvValue*syntp->gOscp[i].oscpenvmod;
				}
				if ((syntp->gOscp[i].oscplfoamplitude) || (syntp->gOscp[i].oscpenvtype && syntp->gOscp[i].oscpenvmod))
				{
					if (syntp->gOscp[i].oscpenvtype == 2) lOscv[i].oscplfophase += lOscv[i].OscpEnvValue*syntp->gOscp[i].oscpenvmod*syntp->gOscp[i].oscpenvmod*64*0.000030517f;
					if (syntp->gOscp[i].oscplfospeed <= MAXSYNCMODES)
					{
						lOscv[i].oscplfophase += SyncAdd[syntp->gOscp[i].oscplfospeed];
					}
					else
					{
						lOscv[i].oscplfophase += ((syntp->gOscp[i].oscplfospeed-MAXSYNCMODES)*(syntp->gOscp[i].oscplfospeed-MAXSYNCMODES))*0.000030517f*freq_mul;
					}
					WRAP_AROUND(lOscv[i].oscplfophase);

					if (syntp->gOscp[i].oscpenvtype == 1)
					{
						temp += ((float(syntp->gOscp[i].poscplfowave[rint<int>(lOscv[i].oscplfophase)])
									*(float(syntp->gOscp[i].oscplfoamplitude))+(lOscv[i].OscpEnvValue*syntp->gOscp[i].oscpenvmod)));
					}
					else
					{
						temp += ((float(syntp->gOscp[i].poscplfowave[rint<int>(lOscv[i].oscplfophase)])
									*float(syntp->gOscp[i].oscplfoamplitude)));
					}
				}
				lOscv[i].oscphase = (syntp->gOscp[i].oscphase+rint<int>(temp))&((SAMPLE_LENGTH*2)-1);
			}

			// osc1w adsr
			temp = (float)syntp->gOscp[i].oscwidth;
			if (syntp->gOscp[i].oscwenvmod)
			{
				switch(lOscv[i].OscwEnvStage)
				{
				case 0: // Delay
					lOscv[i].OscwEnvDValue+=lOscv[i].OscwEnvCoef;												
					if(lOscv[i].OscwEnvDValue>1.0f)
					{
						if (syntp->gOscp[i].oscwattack)
						{
							lOscv[i].OscwEnvValue = 0.0f;
							lOscv[i].OscwEnvStage=1;
							lOscv[i].OscwEnvCoef=freq_mul/(float)syntp->gOscp[i].oscwattack;
						}
						else if (syntp->gOscp[i].oscwdecay)
						{
							lOscv[i].OscwEnvValue = 1.0f;
							lOscv[i].OscwEnvCoef=freq_mul*(1.0f-lOscv[i].OscwEnvSustainLevel)/(float)syntp->gOscp[i].oscwdecay;
							lOscv[i].OscwEnvStage=2;
						}
						else if (syntp->gOscp[i].oscwsustain)
						{
							lOscv[i].OscwEnvValue=lOscv[i].OscwEnvSustainLevel;
							lOscv[i].OscwEnvStage=3;
						}
						else
						{
							lOscv[i].OscwEnvValue=0.0f;
							lOscv[i].OscwEnvStage=-1;
						}				
					}
					break;
				case 1: // Attack
					lOscv[i].OscwEnvValue+=lOscv[i].OscwEnvCoef;
					
					if(lOscv[i].OscwEnvValue>=1.0f)
					{
						if (syntp->gOscp[i].oscwdecay)
						{
							lOscv[i].OscwEnvValue = 1.0f;
							lOscv[i].OscwEnvCoef=freq_mul*(1.0f-lOscv[i].OscwEnvSustainLevel)/(float)syntp->gOscp[i].oscwdecay;
							lOscv[i].OscwEnvStage=2;
						}
						else if (syntp->gOscp[i].oscwsustain)
						{
							lOscv[i].OscwEnvValue=lOscv[i].OscwEnvSustainLevel;
							lOscv[i].OscwEnvStage=3;
						}
						else
						{
							lOscv[i].OscwEnvValue=0.0f;
							lOscv[i].OscwEnvStage=-1;
						}				
					}
					break;

				case 2: // Decay
					lOscv[i].OscwEnvValue-=lOscv[i].OscwEnvCoef;
					
					if(lOscv[i].OscwEnvValue<=lOscv[i].OscwEnvSustainLevel)
					{
						if (syntp->gOscp[i].oscwsustain)
						{
							lOscv[i].OscwEnvValue=lOscv[i].OscwEnvSustainLevel;
							lOscv[i].OscwEnvStage=3;
						}
						else
						{
							lOscv[i].OscwEnvValue=0.0f;
							lOscv[i].OscwEnvStage=-1;
						}				
					}
					break;

				case 4: // Release
					lOscv[i].OscwEnvValue-=lOscv[i].OscwEnvCoef;

					if(lOscv[i].OscwEnvValue<=0.0f)
					{
						lOscv[i].OscwEnvDValue=0.0f;
						lOscv[i].OscwEnvStage=-1;
					}
					break;
				}
				if (syntp->gOscp[i].oscwenvtype == 0) temp+=lOscv[i].OscwEnvValue*syntp->gOscp[i].oscwenvmod*0.0625f;
			}
			// width lfo 1
			if ((syntp->gOscp[i].oscwlfoamplitude) || (syntp->gOscp[i].oscwenvtype && syntp->gOscp[i].oscwenvmod))
			{
				if (syntp->gOscp[i].oscwenvtype == 2) lOscv[i].oscwlfophase += lOscv[i].OscwEnvValue*syntp->gOscp[i].oscwenvmod*syntp->gOscp[i].oscwenvmod*64*0.000030517f;
				if (syntp->gOscp[i].oscwlfospeed <= MAXSYNCMODES)
				{
					lOscv[i].oscwlfophase += SyncAdd[syntp->gOscp[i].oscwlfospeed];
				}
				else
				{
					lOscv[i].oscwlfophase += ((syntp->gOscp[i].oscwlfospeed-MAXSYNCMODES)*(syntp->gOscp[i].oscwlfospeed-MAXSYNCMODES))*0.000030517f*freq_mul;
				}
				WRAP_AROUND(lOscv[i].oscwlfophase);

				if (syntp->gOscp[i].oscwenvtype == 1)
				{
					temp += ((float(syntp->gOscp[i].poscwlfowave[rint<int>(lOscv[i].oscwlfophase)])
								*(float(syntp->gOscp[i].oscwlfoamplitude))+(lOscv[i].OscwEnvValue*syntp->gOscp[i].oscwenvmod*0.0625f)));
				}
				else
				{
					temp += ((float(syntp->gOscp[i].poscwlfowave[rint<int>(lOscv[i].oscwlfophase)])
									*float(syntp->gOscp[i].oscwlfoamplitude)));
				}
			}
			if (temp < 1)
			{
				temp = 1;
			}
			else if (temp > 255)
			{
				temp = 255;
			}
#endif
			float note=Vib_basenote+
				(syntp->globalfinetune+syntp->gOscp[i].oscfinetune-61)*0.0038962f+
				syntp->globaltune+
				syntp->gOscp[i].osctune+
				TrackerArpeggio[TrackerArpeggioIndex];
#ifndef SYNTH_LIGHT
			if (syntp->arp_mod)
			{
				note+=(float)ArpNote[syntp->arp_mod-1][ArpCounter];
			}
			// osc2f adsr
			if (syntp->gOscp[i].oscfenvmod)
			{
				switch(lOscv[i].OscfEnvStage)
				{
				case 0: // Delay
					lOscv[i].OscfEnvDValue+=lOscv[i].OscfEnvCoef;												
					if(lOscv[i].OscfEnvDValue>1.0f)
					{
						if (syntp->gOscp[i].oscfattack)
						{
							lOscv[i].OscfEnvValue = 0.0f;
							lOscv[i].OscfEnvStage=1;
							lOscv[i].OscfEnvCoef=freq_mul/(float)syntp->gOscp[i].oscfattack;
						}
						else if (syntp->gOscp[i].oscfdecay)
						{
							lOscv[i].OscfEnvValue = 1.0f;
							lOscv[i].OscfEnvCoef=freq_mul*(1.0f-lOscv[i].OscfEnvSustainLevel)/(float)syntp->gOscp[i].oscfdecay;
							lOscv[i].OscfEnvStage=2;
						}
						else if (syntp->gOscp[i].oscfsustain)
						{
							lOscv[i].OscfEnvValue=lOscv[i].OscfEnvSustainLevel;
							lOscv[i].OscfEnvStage=3;
						}
						else
						{
							lOscv[i].OscfEnvValue=0.0f;
							lOscv[i].OscfEnvStage=-1;
						}				
					}
					break;
				case 1: // Attack
					lOscv[i].OscfEnvValue+=lOscv[i].OscfEnvCoef;
					
					if(lOscv[i].OscfEnvValue>=1.0f)
					{
						if (syntp->gOscp[i].oscfdecay)
						{
							lOscv[i].OscfEnvValue = 1.0f;
							lOscv[i].OscfEnvCoef=freq_mul*(1.0f-lOscv[i].OscfEnvSustainLevel)/(float)syntp->gOscp[i].oscfdecay;
							lOscv[i].OscfEnvStage=2;
						}
						else if (syntp->gOscp[i].oscfsustain)
						{
							lOscv[i].OscfEnvValue=lOscv[i].OscfEnvSustainLevel;
							lOscv[i].OscfEnvStage=3;
						}
						else
						{
							lOscv[i].OscfEnvValue=0.0f;
							lOscv[i].OscfEnvStage=-1;
						}				
					}
					break;

				case 2: // Decay
					lOscv[i].OscfEnvValue-=lOscv[i].OscfEnvCoef;
					
					if(lOscv[i].OscfEnvValue<=lOscv[i].OscfEnvSustainLevel)
					{
						if (syntp->gOscp[i].oscfsustain)
						{
							lOscv[i].OscfEnvValue=lOscv[i].OscfEnvSustainLevel;
							lOscv[i].OscfEnvStage=3;
						}
						else
						{
							lOscv[i].OscfEnvValue=0.0f;
							lOscv[i].OscfEnvStage=-1;
						}				
					}
					break;

				case 4: // Release
					lOscv[i].OscfEnvValue-=lOscv[i].OscfEnvCoef;

					if(lOscv[i].OscfEnvValue<=0.0f)
					{
						lOscv[i].OscfEnvDValue=0.0f;
						lOscv[i].OscfEnvStage=-1;
					}
					break;
				}
				if (syntp->gOscp[i].oscfenvtype == 0) note+=lOscv[i].OscfEnvValue*syntp->gOscp[i].oscfenvmod*0.00390625f;
			}
			// freq lfo 2
			if ((syntp->gOscp[i].oscflfoamplitude) || (syntp->gOscp[i].oscfenvtype && syntp->gOscp[i].oscfenvmod))
			{
				if (syntp->gOscp[i].oscfenvtype == 2) lOscv[i].oscflfophase += lOscv[i].OscfEnvValue*syntp->gOscp[i].oscfenvmod*syntp->gOscp[i].oscfenvmod*64*0.000030517f;
				if (syntp->gOscp[i].oscflfospeed <= MAXSYNCMODES)
				{
					lOscv[i].oscflfophase += SyncAdd[syntp->gOscp[i].oscflfospeed];
				}
				else
				{
					lOscv[i].oscflfophase += ((syntp->gOscp[i].oscflfospeed-MAXSYNCMODES)*(syntp->gOscp[i].oscflfospeed-MAXSYNCMODES))*0.000030517f*freq_mul;
				}
				WRAP_AROUND(lOscv[i].oscflfophase);

				if (syntp->gOscp[i].oscfenvtype == 1)
				{
					note+=(float(syntp->gOscp[i].poscflfowave[rint<int>(lOscv[i].oscflfophase)])
								*0.015625f
								*(float(syntp->gOscp[i].oscflfoamplitude)+(lOscv[i].OscfEnvValue*syntp->gOscp[i].oscfenvmod)));
				}
				else
				{
					note+=(float(syntp->gOscp[i].poscflfowave[rint<int>(lOscv[i].oscflfophase)])
								*0.015625f
								*float(syntp->gOscp[i].oscflfoamplitude));
				}
			}
			lOscv[i].oscwidth=rint<int>(temp);
			lOscv[i].OSCSpeed[0]=(float)pow(2.0, (note)/12.0)*WidthMultiplierB[lOscv[i].oscwidth]*freq_mul;
			lOscv[i].OSCSpeed[1]=(float)pow(2.0, (note)/12.0)*WidthMultiplierA[lOscv[i].oscwidth]*freq_mul;
			if(lOscv[i].OSCSpeed[0]<0) lOscv[i].OSCSpeed[0]=0;
			if(lOscv[i].OSCSpeed[1]<0) lOscv[i].OSCSpeed[1]=0;
#else
#ifndef SYNTH_ULTRALIGHT
			lOscv[i].OSCSpeed[0]=(float)pow(2.0, (note)/12.0)*WidthMultiplierB[syntp->gOscp[i].oscwidth]*freq_mul;
			lOscv[i].OSCSpeed[1]=(float)pow(2.0, (note)/12.0)*WidthMultiplierA[syntp->gOscp[i].oscwidth]*freq_mul;
			if(lOscv[i].OSCSpeed[0]<0) lOscv[i].OSCSpeed[0]=0;
			if(lOscv[i].OSCSpeed[1]<0) lOscv[i].OSCSpeed[1]=0;
#else
			lOscv[i].OSCSpeed=(float)pow(2.0, (note)/12.0)*2*freq_mul;
			if(lOscv[i].OSCSpeed<0) lOscv[i].OSCSpeed=0;
#endif
#endif
		}
	}

	// overdrive
	if (syntp->overdrive)
	{
		OutGain = 1.0f+(syntp->overdrivegain*syntp->overdrivegain/OVERDRIVEDIVISOR);
		// gain adsr
#ifndef SYNTH_ULTRALIGHT
		if (syntp->gain_envmod)
		{
			switch (GainEnvStage)
			{
			case 0: // Delay
				GainEnvDValue+=GainEnvCoef;												
				if(GainEnvDValue>1.0f)
				{
					if (syntp->gain_env_attack)
					{
						GainEnvValue = 0.0f;
						GainEnvStage=1;
						GainEnvCoef=freq_mul/(float)syntp->gain_env_attack;
					}
					else if (syntp->gain_env_decay)
					{
						GainEnvValue = 1.0f;
						GainEnvCoef=freq_mul*(1.0f-GainEnvSustainLevel)/(float)syntp->gain_env_decay;
						GainEnvStage=2;
					}
					else if (syntp->gain_env_sustain)
					{
						GainEnvValue=GainEnvSustainLevel;
						GainEnvStage=3;
					}
					else
					{
						GainEnvValue=0.0f;
						GainEnvStage=-1;
					}				
				}
				break;

			case 1: // Attack
				GainEnvValue+=GainEnvCoef;
				
				if(GainEnvValue>=1.0f)
				{
					if (syntp->gain_env_decay)
					{
						GainEnvValue = 1.0f;
						GainEnvCoef=freq_mul*(1.0f-GainEnvSustainLevel)/(float)syntp->gain_env_decay;
						GainEnvStage=2;
					}
					else if (syntp->gain_env_sustain)
					{
						GainEnvValue=GainEnvSustainLevel;
						GainEnvStage=3;
					}
					else
					{
						GainEnvValue=0.0f;
						GainEnvStage=-1;
					}				
				}
				break;

			case 2: // Decay
				GainEnvValue-=GainEnvCoef;
				
				if(GainEnvValue<=GainEnvSustainLevel)
				{
					if (syntp->gain_env_sustain)
					{
						GainEnvValue=GainEnvSustainLevel;
						GainEnvStage=3;
					}
					else
					{
						GainEnvValue=0.0f;
						GainEnvStage=-1;
					}
				}
				break;

			case 4: // Release
				GainEnvValue-=GainEnvCoef;

				if(GainEnvValue<=0.0f)
				{
					GainEnvDValue=0.0f;
					GainEnvStage=-1;
				}
				break;
			}
			if (syntp->gain_envtype == 0) 
			{
				if (syntp->gain_envmod > 0)
				{
					OutGain+=GainEnvValue*syntp->gain_envmod*syntp->gain_envmod/OVERDRIVEDIVISOR;
				}
				else
				{
					OutGain-=GainEnvValue*syntp->gain_envmod*syntp->gain_envmod/OVERDRIVEDIVISOR;
				}
			}
		}
		// gain lfo
		if ((syntp->gain_lfo_amplitude) || (syntp->gain_envtype && syntp->gain_envmod))
		{
			if (syntp->gain_envtype == 2) gain_lfo_phase += GainEnvValue*syntp->gain_envmod*syntp->gain_envmod*64*0.000030517f;
			if (syntp->gain_lfo_speed <= MAXSYNCMODES)
			{
				gain_lfo_phase += SyncAdd[syntp->gain_lfo_speed];
			}
			else
			{
				gain_lfo_phase += ((syntp->gain_lfo_speed-MAXSYNCMODES)*(syntp->gain_lfo_speed-MAXSYNCMODES))*0.000030517f;
			}
			WRAP_AROUND(gain_lfo_phase);

			if (syntp->gain_envtype == 1)
			{
				if (syntp->gain_envmod > 0)
				{
					OutGain+= (float(syntp->pgain_lfo_wave[rint<int>(gain_lfo_phase)])
									/OVERDRIVEDIVISOR
									*syntp->gain_lfo_amplitude
									*syntp->gain_lfo_amplitude
									+(GainEnvValue*syntp->gain_envmod*syntp->gain_envmod/OVERDRIVEDIVISOR));
				}
				else
				{
					OutGain-= (float(syntp->pgain_lfo_wave[rint<int>(gain_lfo_phase)])
									/OVERDRIVEDIVISOR
									*syntp->gain_lfo_amplitude
									*syntp->gain_lfo_amplitude
									+(GainEnvValue*syntp->gain_envmod*syntp->gain_envmod/OVERDRIVEDIVISOR));

				}
			}
			else
			{
				OutGain+= (float(syntp->pgain_lfo_wave[rint<int>(gain_lfo_phase)])
								/OVERDRIVEDIVISOR
								*syntp->gain_lfo_amplitude
								*syntp->gain_lfo_amplitude);
			}
		}
#endif
		if (OutGain < 1.0f) OutGain = 1.0f;
		else if (OutGain > 65536/OVERDRIVEDIVISOR) OutGain = 65536/OVERDRIVEDIVISOR;
	}
	// tremolo lfo
#ifndef SYNTH_LIGHT
	if (syntp->tremolo_amplitude)
	{
		switch (TremoloEnvStage)
		{
		case 0:
			// off with delay
			TremoloEnvValue+=TremoloEnvCoef;
			TremoloVol = 1.0f;
			if (TremoloEnvValue>=1.0f)
			{
				TremoloEnvValue = 0;
				TremoloEnvStage = 2;
			}
			break;
		case 2:
			if (syntp->tremolo_speed <= MAXSYNCMODES)
			{
				tremolo_phase += SyncAdd[syntp->tremolo_speed];
			}
			else
			{
				tremolo_phase += ((syntp->tremolo_speed-MAXSYNCMODES)*(syntp->tremolo_speed-MAXSYNCMODES))*0.000030517f;
			}
			WRAP_AROUND(tremolo_phase);

			TremoloVol=1.0f-((float(syntp->ptremolo_wave[rint<int>(tremolo_phase)]+1.0f)
							*0.00390625f
							*0.533333f
							*float(syntp->tremolo_amplitude)));
			if (TremoloVol < 0)
			{
				TremoloVol = 0;
			}
			break;
		}
	}
	else
	{
		TremoloVol = 1.0f;
	}
#endif
}



#ifndef SYNTH_ULTRALIGHT
inline float CSynthTrack::GetInterpolatedSampleOSC(unsigned int osc, unsigned int dir, float Pos)
{
	while (Pos>=SAMPLE_LENGTH)
	{
//								Pos = (Pos-rint<int>(Pos))+(rint<int>(Pos)&(SAMPLE_LENGTH-1));
		Pos -= SAMPLE_LENGTH;
		dir ^= 1;
#ifndef SYNTH_LIGHT
		if (dir)
		{
			Pos *= WidthMultiplierA[lOscv[osc].oscwidth]/WidthMultiplierB[lOscv[osc].oscwidth];
		}
		else
		{
			Pos *= WidthMultiplierB[lOscv[osc].oscwidth]/WidthMultiplierA[lOscv[osc].oscwidth];
		}
#else
		if (dir)
		{
			Pos *= WidthMultiplierA[syntp->gOscp[osc].oscwidth]/WidthMultiplierB[syntp->gOscp[osc].oscwidth];
		}
		else
		{
			Pos *= WidthMultiplierB[syntp->gOscp[osc].oscwidth]/WidthMultiplierA[syntp->gOscp[osc].oscwidth];
		}
#endif
	}

	if (syntp->gOscp[osc].Wave[dir] == WHITENOISEGEN)
	{
		lOscv[osc].rand[dir] = (lOscv[osc].rand[dir]*171)+145;
		return ((lOscv[osc].rand[dir]&0xffff)-0x8000)/32768.0f;
	}
	else if (syntp->gOscp[osc].Wave[dir] == BROWNNOISEGEN)
	{
		lOscv[osc].rand[dir] = (lOscv[osc].rand[dir]*171)+145;
		lOscv[osc].Last[dir] += ((lOscv[osc].rand[dir]&0xffff)-0x8000)/65536.0f;
		// bounce off limits
		if (lOscv[osc].Last[dir] > 1.0f)
		{
			lOscv[osc].Last[dir] -= 2*(lOscv[osc].Last[dir]-1.0f);
		}
		else if (lOscv[osc].Last[dir] < -1.0f)
		{
			lOscv[osc].Last[dir] -= 2*(lOscv[osc].Last[dir]+1.0f);
		}
		return lOscv[osc].Last[dir];
	}
	else
	{
		/***********************INTERPOLOTION CODE!****************************************/
		if (syntp->interpolate > 1)  // Quite Pronounced CPU usage increase...
		{
			float total = 0;
			float add[2] = {lOscv[osc].OSCSpeed[0]/syntp->interpolate,lOscv[osc].OSCSpeed[1]/syntp->interpolate};

			for (unsigned int i = 0; i < syntp->interpolate; i++)
			{
				total+=antialias(syntp->gOscp[osc].pWave[dir][rint<int>(Pos)]);
				Pos += add[dir];
				while (Pos>=SAMPLE_LENGTH)
				{
//																				Pos = (Pos-rint<int>(Pos))+(rint<int>(Pos)&(SAMPLE_LENGTH-1));
					Pos -= SAMPLE_LENGTH;
					dir ^= 1;
#ifndef SYNTH_LIGHT
					if (dir)
					{
						Pos *= WidthMultiplierA[lOscv[osc].oscwidth]/WidthMultiplierB[lOscv[osc].oscwidth];
					}
					else
					{
						Pos *= WidthMultiplierB[lOscv[osc].oscwidth]/WidthMultiplierA[lOscv[osc].oscwidth];
					}
#else
					if (dir)
					{
						Pos *= WidthMultiplierA[syntp->gOscp[osc].oscwidth]/WidthMultiplierB[syntp->gOscp[osc].oscwidth];
					}
					else
					{
						Pos *= WidthMultiplierB[syntp->gOscp[osc].oscwidth]/WidthMultiplierA[syntp->gOscp[osc].oscwidth];
					}
#endif
				}
			}
			return (total/syntp->interpolate);
		}
		else
		{
			return syntp->gOscp[osc].pWave[dir][rint<int>(Pos)];
		}
	}
}
#else
// ultralight
inline float CSynthTrack::GetInterpolatedSampleOSC(unsigned int osc, float Pos)
{
	/* unecessary
	if (Pos>=SAMPLE_LENGTH*2)
	{
		Pos = rint<int>(Pos)&(SAMPLE_LENGTH*2-1);
	}
	*/

	if (syntp->gOscp[osc].Wave == WHITENOISEGEN)
	{
		lOscv[osc].rand = (lOscv[osc].rand*171)+145;
		return ((lOscv[osc].rand&0xffff)-0x8000)/32768.0f;
	}
	else if (syntp->gOscp[osc].Wave == BROWNNOISEGEN)
	{
		lOscv[osc].rand = (lOscv[osc].rand*171)+145;
		lOscv[osc].Last += ((lOscv[osc].rand&0xffff)-0x8000)/65536.0f;
		// bounce off limits
		if (lOscv[osc].Last > 1.0f)
		{
			lOscv[osc].Last -= 2*(lOscv[osc].Last-1.0f);
		}
		else if (lOscv[osc].Last < -1.0f)
		{
			lOscv[osc].Last -= 2*(lOscv[osc].Last+1.0f);
		}
		return lOscv[osc].Last;
	}
	else
	{
		/***********************INTERPOLOTION CODE!****************************************/
		if (syntp->interpolate > 1)  // Quite Pronounced CPU usage increase...
		{
			float total = 0;
			float add = lOscv[osc].OSCSpeed/syntp->interpolate;

			for (unsigned int i = 0; i < syntp->interpolate; i++)
			{
				total+=antialias(syntp->gOscp[osc].pWave[rint<int>(Pos)]);
				Pos += add;
				if (Pos>=SAMPLE_LENGTH)
				{
					Pos = (Pos-rint<int>(Pos))+(rint<int>(Pos)&((SAMPLE_LENGTH*2)-1));
				}
			}
			return (total/syntp->interpolate);
		}
		else
		{
			return syntp->gOscp[osc].pWave[rint<int>(Pos)];
		}
	}
}
#endif

inline float CSynthTrack::GetPhaseSampleOSC(unsigned int osc)
{
#ifndef SYNTH_LIGHT
	switch (syntp->gOscp[osc].oscphasemix)
	{
	case 0:
		// no phase
		return GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)*(lOscv[osc].OSCVol[lOscv[osc].oscdir]);
		break;
	case 1:
		// add phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)
				+GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition+lOscv[osc].oscphase))*(lOscv[osc].OSCVol[lOscv[osc].oscdir]);
		break;
	case 2:
		// subtractive phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)
				-GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition+lOscv[osc].oscphase))*(lOscv[osc].OSCVol[lOscv[osc].oscdir]);
		break;
	case 3:
		// mult phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)
				*GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition+lOscv[osc].oscphase))*(lOscv[osc].OSCVol[lOscv[osc].oscdir]);
		break;
	case 4:
		// inv mult phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)
				*-GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition+lOscv[osc].oscphase))*(lOscv[osc].OSCVol[lOscv[osc].oscdir]);
		break;
	case 5:
		// f add phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)*lOscv[osc].OSCVol[lOscv[osc].oscdir]
				+GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir^1,lOscv[osc].OSCPosition+lOscv[osc].oscphase)*lOscv[osc].OSCVol[lOscv[osc].oscdir^1]);
		break;
	case 6:
		// f subtractive phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)*lOscv[osc].OSCVol[lOscv[osc].oscdir]
				-GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir^1,lOscv[osc].OSCPosition+lOscv[osc].oscphase)*lOscv[osc].OSCVol[lOscv[osc].oscdir^1]);
		break;
	case 7:
		// f mult phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)*lOscv[osc].OSCVol[lOscv[osc].oscdir]
				*GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir^1,lOscv[osc].OSCPosition+lOscv[osc].oscphase)*lOscv[osc].OSCVol[lOscv[osc].oscdir^1]);
		break;
	case 8:
		// f inv mult phase
		return (GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)*lOscv[osc].OSCVol[lOscv[osc].oscdir]
				*-GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir^1,lOscv[osc].OSCPosition+lOscv[osc].oscphase)*lOscv[osc].OSCVol[lOscv[osc].oscdir^1]);
		break;
	case 9:
		// replace
		return GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition+lOscv[osc].oscphase)*(lOscv[osc].OSCVol[lOscv[osc].oscdir]);
		break;
	}
	return 0.0f;
#else
#ifndef SYNTH_ULTRALIGHT
	return GetInterpolatedSampleOSC(osc,lOscv[osc].oscdir,lOscv[osc].OSCPosition)*(lOscv[osc].OSCVol[lOscv[osc].oscdir]);
#else
	return GetInterpolatedSampleOSC(osc,lOscv[osc].OSCPosition)*(lOscv[osc].OSCVol);
#endif
#endif

}

inline float CSynthTrack::GetPhaseSampleMix()
{
	float total = 0;
	for (unsigned int i = 0; i < MAXOSC; i++)
	{
#ifndef SYNTH_ULTRALIGHT
		if (syntp->gOscp[i].oscvol[0])
#else
		if (syntp->gOscp[i].oscvol)
#endif
		{
#ifndef SYNTH_ULTRALIGHT
			lOscv[i].OSCPosition+=lOscv[i].OSCSpeed[lOscv[i].oscdir];
			while (lOscv[i].OSCPosition>=SAMPLE_LENGTH)
			{
//																				lOscv[i].OSCPosition = (lOscv[i].OSCPosition-rint<int>(lOscv[i].OSCPosition))+(rint<int>(lOscv[i].OSCPosition)&(SAMPLE_LENGTH-1));
				lOscv[i].OSCPosition -= SAMPLE_LENGTH;
				lOscv[i].oscdir ^= 1;
#ifndef SYNTH_LIGHT
				if (lOscv[i].oscdir)
				{
					lOscv[i].OSCPosition *= WidthMultiplierA[lOscv[i].oscwidth]/WidthMultiplierB[lOscv[i].oscwidth];
				}
				else
				{
					lOscv[i].OSCPosition *= WidthMultiplierB[lOscv[i].oscwidth]/WidthMultiplierA[lOscv[i].oscwidth];
					if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
					{
						lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
						lOscv[syntp->gOscp[i].oscsync-1].oscdir = 0;
					}
				}
#else
				if (lOscv[i].oscdir)
				{
					lOscv[i].OSCPosition *= WidthMultiplierA[syntp->gOscp[i].oscwidth]/WidthMultiplierB[syntp->gOscp[i].oscwidth];
				}
				else
				{
					lOscv[i].OSCPosition *= WidthMultiplierB[syntp->gOscp[i].oscwidth]/WidthMultiplierA[syntp->gOscp[i].oscwidth];
					if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
					{
						lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
						lOscv[syntp->gOscp[i].oscsync-1].oscdir = 0;
					}
				}
#endif
			}
#else
			lOscv[i].OSCPosition+=lOscv[i].OSCSpeed;
			if (lOscv[i].OSCPosition>=SAMPLE_LENGTH*2)
			{
				lOscv[i].OSCPosition = (lOscv[i].OSCPosition-rint<int>(lOscv[i].OSCPosition))+(rint<int>(lOscv[i].OSCPosition)&(SAMPLE_LENGTH*2-1));
				if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
				{
					lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
				}
			}
#endif
			if (i)
			{
				switch (syntp->gOscp[i].oscmixtype)
				{
				case 0:
					// add
					total+=GetPhaseSampleOSC(i);
					break;
				case 1:
					// subt
					total-=GetPhaseSampleOSC(i);
					break;
				case 2:
					// mul
					total*=GetPhaseSampleOSC(i);
					break;
				case 3:
					// divide
					{
						float temp = GetPhaseSampleOSC(i);
						if (temp)
						{
							total = (total/temp);
							if (total > 2.0f)
							{
								total = 2.0f;
							}
							else if (total < -2.0f)
							{
								total = -2.0f;
							}
						}
						else
						{
							total = (total*temp > 0) ? 2.0f : -2.0f;
						}
					}
					break;
				}
			}
			else
			{
				total = GetPhaseSampleOSC(i);
			}
		}
	}
	return total;
}

inline float CSynthTrack::GetPhaseSampleOSCOdd()
{
	float total = 0;
	for (unsigned int i = 1; i < MAXOSC; i+=2)
	{
#ifndef SYNTH_ULTRALIGHT
		if (syntp->gOscp[i].oscvol[0])
#else
		if (syntp->gOscp[i].oscvol)
#endif
		{
#ifndef SYNTH_ULTRALIGHT
			lOscv[i].OSCPosition+=lOscv[i].OSCSpeed[lOscv[i].oscdir];
			while (lOscv[i].OSCPosition>=SAMPLE_LENGTH)
			{
//																				lOscv[i].OSCPosition = (lOscv[i].OSCPosition-rint<int>(lOscv[i].OSCPosition))+(rint<int>(lOscv[i].OSCPosition)&(SAMPLE_LENGTH-1));
				lOscv[i].OSCPosition -= SAMPLE_LENGTH;
				lOscv[i].oscdir ^= 1;
#ifndef SYNTH_LIGHT
				if (lOscv[i].oscdir)
				{
					lOscv[i].OSCPosition *= WidthMultiplierA[lOscv[i].oscwidth]/WidthMultiplierB[lOscv[i].oscwidth];
				}
				else
				{
					lOscv[i].OSCPosition *= WidthMultiplierB[lOscv[i].oscwidth]/WidthMultiplierA[lOscv[i].oscwidth];
					if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
					{
						lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
						lOscv[syntp->gOscp[i].oscsync-1].oscdir = 0;
					}
				}
#else
				if (lOscv[i].oscdir)
				{
					lOscv[i].OSCPosition *= WidthMultiplierA[syntp->gOscp[i].oscwidth]/WidthMultiplierB[syntp->gOscp[i].oscwidth];
				}
				else
				{
					lOscv[i].OSCPosition *= WidthMultiplierB[syntp->gOscp[i].oscwidth]/WidthMultiplierA[syntp->gOscp[i].oscwidth];
					if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
					{
						lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
						lOscv[syntp->gOscp[i].oscsync-1].oscdir = 0;
					}
				}
#endif
			}
#else
			lOscv[i].OSCPosition+=lOscv[i].OSCSpeed;
			if (lOscv[i].OSCPosition>=SAMPLE_LENGTH*2)
			{
				lOscv[i].OSCPosition = (lOscv[i].OSCPosition-rint<int>(lOscv[i].OSCPosition))+(rint<int>(lOscv[i].OSCPosition)&(SAMPLE_LENGTH*2-1));
				if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
				{
					lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
				}
			}
#endif
			if (i)
			{
				switch (syntp->gOscp[i].oscmixtype)
				{
				case 0:
					// add
					total+=GetPhaseSampleOSC(i);
					break;
				case 1:
					// subt
					total-=GetPhaseSampleOSC(i);
					break;
				case 2:
					// mul
					total*=GetPhaseSampleOSC(i);
					break;
				case 3:
					// divide
					{
						float temp = GetPhaseSampleOSC(i);
						if (temp)
						{
							total = (total/temp);
							if (total > 2.0f)
							{
								total = 2.0f;
							}
							else if (total < -2.0f)
							{
								total = -2.0f;
							}
						}
						else
						{
							total = (total*temp > 0) ? 2.0f : -2.0f;
						}
					}
					break;
				}
			}
			else
			{
				total = GetPhaseSampleOSC(i);
			}
		}
	}
	return total;
}




inline float CSynthTrack::GetPhaseSampleOSCEven()
{
	float total = 0;
	for (unsigned int i = 0; i < MAXOSC; i+=2)
	{
#ifndef SYNTH_ULTRALIGHT
		if (syntp->gOscp[i].oscvol[0])
#else
		if (syntp->gOscp[i].oscvol)
#endif
		{
#ifndef SYNTH_ULTRALIGHT
			lOscv[i].OSCPosition+=lOscv[i].OSCSpeed[lOscv[i].oscdir];
			while (lOscv[i].OSCPosition>=SAMPLE_LENGTH)
			{
//																				lOscv[i].OSCPosition = (lOscv[i].OSCPosition-rint<int>(lOscv[i].OSCPosition))+(rint<int>(lOscv[i].OSCPosition)&(SAMPLE_LENGTH-1));
				lOscv[i].OSCPosition -= SAMPLE_LENGTH;
				lOscv[i].oscdir ^= 1;
#ifndef SYNTH_LIGHT
				if (lOscv[i].oscdir)
				{
					lOscv[i].OSCPosition *= WidthMultiplierA[lOscv[i].oscwidth]/WidthMultiplierB[lOscv[i].oscwidth];
				}
				else
				{
					lOscv[i].OSCPosition *= WidthMultiplierB[lOscv[i].oscwidth]/WidthMultiplierA[lOscv[i].oscwidth];
					if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
					{
						lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
						lOscv[syntp->gOscp[i].oscsync-1].oscdir = 0;
					}
				}
#else
				if (lOscv[i].oscdir)
				{
					lOscv[i].OSCPosition *= WidthMultiplierA[syntp->gOscp[i].oscwidth]/WidthMultiplierB[syntp->gOscp[i].oscwidth];
				}
				else
				{
					lOscv[i].OSCPosition *= WidthMultiplierB[syntp->gOscp[i].oscwidth]/WidthMultiplierA[syntp->gOscp[i].oscwidth];
					if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
					{
						lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
						lOscv[syntp->gOscp[i].oscsync-1].oscdir = 0;
					}
				}
#endif
			}
#else
			lOscv[i].OSCPosition+=lOscv[i].OSCSpeed;
			if (lOscv[i].OSCPosition>=SAMPLE_LENGTH*2)
			{
				lOscv[i].OSCPosition = (lOscv[i].OSCPosition-rint<int>(lOscv[i].OSCPosition))+(rint<int>(lOscv[i].OSCPosition)&(SAMPLE_LENGTH*2-1));
				if ((syntp->gOscp[i].oscsync) && (syntp->gOscp[i].oscsync != i+1))
				{
					lOscv[syntp->gOscp[i].oscsync-1].OSCPosition = 0;
				}
			}
#endif
			if (i)
			{
				switch (syntp->gOscp[i].oscmixtype)
				{
				case 0:
					// add
					total+=GetPhaseSampleOSC(i);
					break;
				case 1:
					// subt
					total-=GetPhaseSampleOSC(i);
					break;
				case 2:
					// mul
					total*=GetPhaseSampleOSC(i);
					break;
				case 3:
					// divide
					{
						float temp = GetPhaseSampleOSC(i);
						if (temp)
						{
							total = (total/temp);
							if (total > 2.0f)
							{
								total = 2.0f;
							}
							else if (total < -2.0f)
							{
								total = -2.0f;
							}
						}
						else
						{
							total = (total*temp > 0) ? 2.0f : -2.0f;
						}
					}
					break;
				}
			}
			else
			{
				total = GetPhaseSampleOSC(i);
			}
		}
	}
	return total;
}

inline float CSynthTrack::DoFilter(int num, float input)
{
	if (syntp->gVcfp[num].vcftype > MAXFILTER)
	{
		return m_filter[num].phaser(input);
	}
	else if (syntp->gVcfp[num].vcftype > MAXMOOG)
	{
		if (syntp->gVcfp[num].vcftype & 1 ) return m_filter[num].res(input);
		return m_filter[num].res2(input);
	}
	else if (syntp->gVcfp[num].vcftype)
	{
		if (syntp->gVcfp[num].vcftype & 1) return m_filter[num].moog1(input);
		return m_filter[num].moog2(input);
	}
	return input;
}

inline float CSynthTrack::HandleOverdrive(float input)
{
#define MAXOVERDRIVEAMP 0.9999f
	// handle Overdrive
	if (syntp->overdrive)
	{
		switch (syntp->overdrive)
		{
		case 1: // soft clip 1
		case 1+BASEOVERDRIVE:
			return (atanf(input*OutGain)/(float)PI);
			break;
		case 2: // soft clip 2
		case 2+BASEOVERDRIVE:
			input *= OutGain;
			input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
			if (input < -1.0f)								return -1.0f;
			else if (input > 1.0f)				return 1.0f;
			return input;
		case 3: // soft clip 3
		case 3+BASEOVERDRIVE:
			input *= OutGain;
			if (input < -0.75f) return -0.75f + ((input+0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
			else if (input < 0.75f) return input;
			else return 0.75f + ((input-0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
			break;
		case 4: // hard clip 1
		case 4+BASEOVERDRIVE:
			input *= OutGain;
			if (input < -1.0f)								return -1.0f;
			else if (input > 1.0f)				return 1.0f;
			return input;
			break;
		case 5: // hard clip 2
		case 5+BASEOVERDRIVE:
			// bounce off limits
			input *= OutGain;
			if (input < -1.0f)								return input-(rint<int>(input)+1)*(input+1.0f);
			else if (input > 1.0f)				return input-(rint<int>(input)+1)*(input-1.0f);
			return input;
			break;
		case 6: // hard clip 3
		case 6+BASEOVERDRIVE:
			// invert, this one is harsh
			input *= OutGain;
			if (input < -1.0f)								return input + (rint<int>(input/(2.0f)))*(2.0f);
			else if (input > 1.0f)				return input - (rint<int>(input/(2.0f)))*(2.0f);
			return input;
			break;
		case 7: // parabolic distortion
		case 7+BASEOVERDRIVE:
			input = (((input * input)*(OutGain*OutGain)))-1.0f;
			if (input > 1.0f)				return 1.0f;
//												return input*MAXOVERDRIVEAMP*(128.0f/OutGain);
			return input;
			break;
		case 8: // parabolic distortion
		case 8+BASEOVERDRIVE:
			input = (((input * input)*(OutGain*OutGain))*32.0f)-1.0f;
			if (input > 1.0f)				return 1.0f;
			return input;
			break;
		case 9: // sin remapper
		case 9+BASEOVERDRIVE:
			return SourceWaveTable[0][rint<int>(input*OutGain*SAMPLE_LENGTH*2)&((SAMPLE_LENGTH*2)-1)];
			break;
			/*
		case 9: // soft clip 5
			return (input*(fabsf(input) + syntp->overdrivethreshold))/((input*input)+((syntp->overdrivethreshold-16384.0f)*fabsf(input)) + 16384.0f);
			break;
			*/
		}
	}
	return input;
}


inline float CSynthTrack::GetSample()
{
	// main adsr env
	switch(AmpEnvStage)
	{
	case 1: // Attack
		AmpEnvValue+=AmpEnvCoef;
		if(AmpEnvValue>=1.0f)
		{
			if (syntp->amp_env_decay)
			{
				AmpEnvValue=1.0f;
				AmpEnvStage=2;
				AmpEnvCoef=freq_mul/(float)(syntp->amp_env_decay*FILTER_CALC_TIME);
			}
			else if(syntp->amp_env_sustain)
			{
				AmpEnvValue=AmpEnvSustainLevel;
				AmpEnvStage=3;
			}
			else
			{
				AmpEnvValue = 0.0f;
				AmpEnvStage=0;
			}								
		}
		break;

	case 2: // Decay
		AmpEnvValue-=AmpEnvCoef;
		
		if(AmpEnvValue<=AmpEnvSustainLevel)
		{
			if(syntp->amp_env_sustain)
			{
				AmpEnvValue=AmpEnvSustainLevel;
				AmpEnvStage=3;
			}
			else
			{
				AmpEnvValue = 0.0f;
				AmpEnvStage=0;
			}
		}
		break;
	case 4: // Release
		AmpEnvValue-=AmpEnvCoef;

		if(AmpEnvValue<=0.0f)
		{
			AmpEnvValue=0.0f;
			AmpEnvStage=0;
		}
		break;

	}
	if(AmpEnvStage)
	{
		if(!timetocompute--) FilterTick();
#ifndef SYNTH_LIGHT
		OutVol = (float)syntp->out_vol*0.00390625f*AmpEnvValue*TFXVol*TremoloVol;
#else
		OutVol = (float)syntp->out_vol*0.00390625f*AmpEnvValue*TFXVol;
#endif
		if (syntp->overdrive > BASEOVERDRIVE)
		{
			if (!syntp->gVcfp[0].vcftype)
			{
				return HandleOverdrive(DoFilter(1,GetPhaseSampleMix())*OutVol);
			}
			else if (!syntp->gVcfp[1].vcftype)
			{
				return HandleOverdrive(DoFilter(0,GetPhaseSampleMix())*OutVol);
			}

			else
			{
				float output;
				switch (syntp->vcfmixmode)
				{
				case 0: //series 1->2->Od
					return HandleOverdrive(DoFilter(1,DoFilter(0,GetPhaseSampleMix()))*OutVol);
					break;
				case 1: //series 1->Od->2
					return DoFilter(1,HandleOverdrive(DoFilter(0,GetPhaseSampleMix())*OutVol));
					break;
				case 2: //series Od->1->2
					return DoFilter(1,DoFilter(0,HandleOverdrive(GetPhaseSampleMix()*OutVol)));
					break;
				case 3: // series 2->1->Od
					return HandleOverdrive(DoFilter(0,DoFilter(1,GetPhaseSampleMix()))*OutVol);
					break;
				case 4: // series 2->Od->1
					return DoFilter(0,HandleOverdrive(DoFilter(1,GetPhaseSampleMix())*OutVol));
					break;
				case 5: // series Od->2->1
					return DoFilter(0,DoFilter(1,HandleOverdrive(GetPhaseSampleMix()*OutVol)));
					break;
				case 6: // parallel +
					output = GetPhaseSampleMix();
					return HandleOverdrive((DoFilter(0,output)+DoFilter(1,output))*OutVol);
					break;
				case 7: // parallel -
					output = GetPhaseSampleMix();
					return HandleOverdrive((DoFilter(0,output)-DoFilter(1,output))*OutVol);
					break;
				case 8: // parallel *
					output = GetPhaseSampleMix();
					return HandleOverdrive((DoFilter(0,output)*DoFilter(1,output))*0.00012207f*OutVol);
					break;
				case 9: // parallel *-
					output = GetPhaseSampleMix();
					return HandleOverdrive(-(DoFilter(0,output)*DoFilter(1,output))*0.00012207f*OutVol);
					break;
				case 10: // filter per osc 1 - 1 2 - 2
					// add
					return HandleOverdrive((DoFilter(0,GetPhaseSampleOSCEven())+DoFilter(1,GetPhaseSampleOSCOdd()))*OutVol);
					break;
				case 11:
					// subt
					return HandleOverdrive((DoFilter(0,GetPhaseSampleOSCEven())-DoFilter(1,GetPhaseSampleOSCOdd()))*OutVol);
					break;
				case 12:
					// mul
					return HandleOverdrive((DoFilter(0,GetPhaseSampleOSCEven())*DoFilter(1,GetPhaseSampleOSCOdd()))*OutVol);
					break;
				case 13:
					// inv mul
					return HandleOverdrive(-(DoFilter(0,GetPhaseSampleOSCEven())*DoFilter(1,GetPhaseSampleOSCOdd()))*OutVol);
					break;
				case 14: // parallel OD +
					output = GetPhaseSampleMix();
					return (HandleOverdrive(DoFilter(0,output)*OutVol)+HandleOverdrive(DoFilter(1,output)*OutVol));
					break;
				case 15: // parallel OD -
					output = GetPhaseSampleMix();
					return (HandleOverdrive(DoFilter(0,output)*OutVol)-HandleOverdrive(DoFilter(1,output)*OutVol));
					break;
				case 16: // parallel OD *
					output = GetPhaseSampleMix();
					return (HandleOverdrive(DoFilter(0,output)*OutVol)*HandleOverdrive(DoFilter(1,output)*OutVol))*0.00012207f;
					break;
				case 17: // parallel OD *-
					output = GetPhaseSampleMix();
					return -(HandleOverdrive(DoFilter(0,output)*OutVol)*HandleOverdrive(DoFilter(1,output)*OutVol))*0.00012207f;
					break;
				case 18: // filter per osc 1 - 1 2 - 2 OD
					// add
					return (HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven())*OutVol)+HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())*OutVol));
					break;
				case 19:
					// subt
					return (HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven())*OutVol)-HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())*OutVol));
					break;
				case 20:
					// mul
					return (HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven())*OutVol)*HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())*OutVol));
					break;
				case 21:
					// inv mul
					return -(HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven())*OutVol)*HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())*OutVol));
					break;

				}
			}
		}
		else
		{
			if (!syntp->gVcfp[0].vcftype)
			{
				return HandleOverdrive(DoFilter(1,GetPhaseSampleMix()))*OutVol;
			}
			else if (!syntp->gVcfp[1].vcftype)
			{
				return HandleOverdrive(DoFilter(0,GetPhaseSampleMix()))*OutVol;
			}

			else
			{
				float output;
				switch (syntp->vcfmixmode)
				{
				case 0: //series 1->2->Od
					return HandleOverdrive(DoFilter(1,DoFilter(0,GetPhaseSampleMix())))*OutVol;
					break;
				case 1: //series 1->Od->2
					return DoFilter(1,HandleOverdrive(DoFilter(0,GetPhaseSampleMix())))*OutVol;
					break;
				case 2: //series Od->1->2
					return DoFilter(1,DoFilter(0,HandleOverdrive(GetPhaseSampleMix())))*OutVol;
					break;
				case 3: // series 2->1->Od
					return HandleOverdrive(DoFilter(0,DoFilter(1,GetPhaseSampleMix())))*OutVol;
					break;
				case 4: // series 2->Od->1
					return DoFilter(0,HandleOverdrive(DoFilter(1,GetPhaseSampleMix())))*OutVol;
					break;
				case 5: // series Od->2->1
					return DoFilter(0,DoFilter(1,HandleOverdrive(GetPhaseSampleMix())))*OutVol;
					break;
				case 6: // parallel +
					output = GetPhaseSampleMix();
					return HandleOverdrive((DoFilter(0,output)+DoFilter(1,output)))*OutVol;
					break;
				case 7: // parallel -
					output = GetPhaseSampleMix();
					return HandleOverdrive((DoFilter(0,output)-DoFilter(1,output)))*OutVol;
					break;
				case 8: // parallel *
					output = GetPhaseSampleMix();
					return HandleOverdrive((DoFilter(0,output)*DoFilter(1,output))*0.00012207f)*OutVol;
					break;
				case 9: // parallel *-
					output = GetPhaseSampleMix();
					return HandleOverdrive(-(DoFilter(0,output)*DoFilter(1,output))*0.00012207f)*OutVol;
					break;
				case 10: // filter per osc 1 - 1 2 - 2
					// add
					return HandleOverdrive((DoFilter(0,GetPhaseSampleOSCEven())+DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				case 11:
					// subt
					return HandleOverdrive((DoFilter(0,GetPhaseSampleOSCEven())-DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				case 12:
					// mul
					return HandleOverdrive((DoFilter(0,GetPhaseSampleOSCEven())*DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				case 13:
					// inv mul
					return HandleOverdrive(-(DoFilter(0,GetPhaseSampleOSCEven())*DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				case 14: // parallel OD +
					output = GetPhaseSampleMix();
					return (HandleOverdrive(DoFilter(0,output))+HandleOverdrive(DoFilter(1,output)))*OutVol;
					break;
				case 15: // parallel OD -
					output = GetPhaseSampleMix();
					return (HandleOverdrive(DoFilter(0,output))-HandleOverdrive(DoFilter(1,output)))*OutVol;
					break;
				case 16: // parallel OD *
					output = GetPhaseSampleMix();
					return (HandleOverdrive(DoFilter(0,output))*HandleOverdrive(DoFilter(1,output)))*0.00012207f*OutVol;
					break;
				case 17: // parallel OD *-
					output = GetPhaseSampleMix();
					return -(HandleOverdrive(DoFilter(0,output))*HandleOverdrive(DoFilter(1,output)))*0.00012207f*OutVol;
					break;
				case 18: // filter per osc 1 - 1 2 - 2 OD
					// add
					return (HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven()))+HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				case 19:
					// subt
					return (HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven()))-HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				case 20:
					// mul
					return (HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven()))*HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				case 21:
					// inv mul
					return -(HandleOverdrive(DoFilter(0,GetPhaseSampleOSCEven()))*HandleOverdrive(DoFilter(1,GetPhaseSampleOSCOdd())))*OutVol;
					break;
				}
			}
		}
	}
#ifndef SYNTH_LIGHT
	if(!timetocompute--) 
	{
		timetocompute=FILTER_CALC_TIME-1;
		if (syntp->arp_mod)
		{
			if (syntp->arp_bpm <= MAXSYNCMODES)
			{
				Arp_tickcounter+=SyncAdd[syntp->arp_bpm];
			}
			else
			{
				Arp_tickcounter+=Arp_samplespertick;
			}
			if (Arp_tickcounter >= SAMPLE_LENGTH*2)
			{
				Arp_tickcounter -= SAMPLE_LENGTH*2;
				if(++ArpCounter>=syntp->arp_cnt)  
				{
					ArpCounter=0;
				}
				if (arpflag)
				{
					InitEnvelopes();
				}
			}
		}
	}
#endif
	return 0;
}
}