// ronan -- portable port of the V2 speech synthesizer (fork of v2/ronan.cpp,
// task 6.1 of the portable-version-native-player change).
//
// "ronan heisst in wirklichkeit lisa. ich war nur zu faul zum renamen." --kb
//
// Changes vs the lab original:
//  - the three MSVC __asm helpers are replaced by v2math owned kernels
//    (fistp, the agner-fog pow via log2/exp2 cores, exp via exp2 core) and
//    libm cos by a cos built on the same Cody-Waite reduction as vm::sinCore
//    -- no approximating libm in the audio path (portable-determinism spec)
//  - all file-scope mutable state moved into syWRonan (per-instance,
//    multi-instance requirement): the samplerate block `g`, the post-EQ
//    coefficient set d_peq1, SetFrame's static scratch, Process's static
//    deltaframe. The decoded phoneme table stays file-scope: it is CONSTANT
//    data (idempotently re-decoded from rawphonemes on every init).
//  - guarded by V2_RONAN (default on, v2redux.h) instead of RONAN
//
// STATUS: WORKING (2026-06-06). The earlier "audibly off" voice was a single
// porting bug: reset() (called by a CC4 text-select mid-song) must zero the
// whole sequencer/DSP state like the lab's memset(workspace,0) -- it was only
// resetting curp/spos, leaving wait4on stuck, so the phoneme sequencer lagged
// the candytron binary by one syllable for the entire song. Fixed (see reset()).
// Verified vs the candytron speech oracle (c2_oracle_solo + synthSetLyrics):
// ch15 speech solo corr 0.99966 (rms 0.0016), whole-song josie corr 0.99916
// (rms 0.0090 = the music-bed eps floor). V2_RONAN defaults to 1 (speech songs
// route ch15 through the vocal tract; with it OFF that channel leaks raw noise);
// build a speech-silent variant with -DV2_RONAN=0.

#include "v2redux.h" // V2_RONAN

#if V2_RONAN

#include "v2core.h"  // portable type aliases (sF32, sU32, ...)
#include "v2math.h"

#include <math.h>    // fabsf only (exact); no approximating libm calls
#include <string.h>  // memset (reset())

namespace Ronan
{
using namespace v2redux;

	int mystrnicmp1(const char *a, const char *b)
	{
		sInt l=0;
		while (*a && *b)
			if ((*a++ | 0x20)!=(*b++ | 0x20))
				return 0;
			else
				l++;
		return *a?0:l;
	}

	static const sF32 PI = 3.1415926535897932384626433832795f;

	#include "phonemtab.h"

	// decoded phoneme table: constant data, written with identical values on
	// every ronanCBInit (delta-decode of rawphonemes) -- safe to share across
	// instances, all mutable STATE lives in syWRonan.
	static Phoneme phonemes[NPHONEMES];

	static const char *nix="";

	static const struct syldef
	{
		char syl[4];
		sS8  ptab[4];
	} syls[] = {
		{"sil",{50,-1,-1,-1}},
		{"ng",{38,-1,-1,-1}},
		{"th",{57,-1,-1,-1}},
		{"sh",{55,-1,-1,-1}},
		{"dh",{12,51,13,-1}},
		{"zh",{67,51,67,-1}},
		{"ch",{ 9,10,-1,-1}},
		{"ih",{25,-1,-1,-1}},
		{"eh",{16,-1,-1,-1}},
		{"ae",{ 1,-1,-1,-1}},
		{"ah",{60,-1,-1,-1}},
		{"oh",{39,-1,-1,-1}},
		{"uh",{42,-1,-1,-1}},
		{"ax",{ 0,-1,-1,-1}},
		{"iy",{17,-1,-1,-1}},
		{"er",{19,-1,-1,-1}},
		{"aa",{ 4,-1,-1,-1}},
		{"ao",{ 5,-1,-1,-1}},
		{"uw",{61,-1,-1,-1}},
		{"ey",{ 2,25,-1,-1}},
		{"ay",{28,25,-1,-1}},
		{"oy",{41,25,-1,-1}},
		{"aw",{45,46,-1,-1}},
		{"ow",{40,46,-1,-1}},
		{"ia",{26,27,-1,-1}},
		{"ea",{ 3,27,-1,-1}},
		{"ua",{43,27,-1,-1}},
		{"ll",{35,-1,-1,-1}},
		{"wh",{63,-1,-1,-1}},
		{"ix",{ 0,-1,-1,-1}},
		{"el",{34,-1,-1,-1}},
		{"rx",{53,-1,-1,-1}},
		{"h",{24,-1,-1,-1}},
		{"p",{47,48,49,-1}},
		{"t",{56,58,59,-1}},
		{"k",{31,32,33,-1}},
		{"b",{ 6, 7, 8,-1}},
		{"d",{11,14,15,-1}},
		{"g",{21,22,23,-1}},
		{"m",{36,-1,-1,-1}},
		{"n",{37,-1,-1,-1}},
		{"f",{20,-1,-1,-1}},
		{"s",{54,-1,-1,-1}},
		{"v",{62,51,62,-1}},
		{"z",{66,51,68,-1}},
		{"l",{34,-1,-1,-1}},
		{"r",{52,-1,-1,-1}},
		{"w",{63,-1,-1,-1}},
		{"q",{51,-1,-1,-1}},
		{"y",{65,-1,-1,-1}},
		{"j",{29,30,51,30}},
		{" ",{18,-1,-1,-1}},
	};
	#define NSYLS (sizeof(syls)/sizeof(syldef))

	// ---- owned transcendental helpers (replace the lab's __asm) -------------

	// the lab's sFtol: x87 fistp (round to nearest even)
	static inline sInt sFtol(const sF32 f) { return vm::fistp(f); }

	// the lab's sFPow (agner-fog x87 pow: fyl2x feeding a 2^frac*2^int split).
	// Same structure on the v2math double cores; base <= 0 returns 0 like the
	// asm's ftst/jz zero path (ronan never passes negatives).
	static sF64 sFPow(sF64 a, sF64 b)
	{
		if (!(a > 0.0)) return 0.0;
		double t = b * vm::log2Core((float)a);
		double ti = trunc(t);
		return ldexp(vm::exp2Core(t - ti), (int)ti);
	}

	// the lab's sFExp: e^f = 2^(f*log2(e)) on the exp2 core
	static sF64 sFExp(sF64 f)
	{
		double t = f * 1.4426950408889634074; // log2(e), correctly rounded double
		double ti = trunc(t);
		return ldexp(vm::exp2Core(t - ti), (int)ti);
	}

	// cos on the same Cody-Waite reduction as vm::sinCore (no libm cos)
	static sF64 sFCos(sF64 x)
	{
		int n = (int)lrint(x * vm::kTwoOverPi);
		double t = x - (double)n * vm::kPio2A;
		t -= (double)n * vm::kPio2B;
		t -= (double)n * vm::kPio2C;
		switch (n & 3) {
		default:
		case 0: return vm::cosPoly(t);
		case 1: return -vm::sinPoly(t);
		case 2: return -vm::cosPoly(t);
		case 3: return vm::sinPoly(t);
		}
	}

	// filter type 1: 2-pole resonator (coefficient set needs the per-instance
	// samplerate constants -- passed in, the lab read file-scope `g`)
	struct ResDef
	{
		sF32 a,b,c;  // coefficients

		void set(sF32 f, sF32 bw, sF32 gain, sF32 fcminuspi_sr, sF32 fc2pi_sr)
		{
			sF32 r=(sF32)sFExp(fcminuspi_sr*bw);
			c=-(r*r);
			b=r*(sF32)sFCos(fc2pi_sr*f)*2.0f;
			a=gain*(1.0f-b-c);
		}
	};

	struct Resonator
	{
		ResDef *def;
		sF32 p1, p2; // delay buffers

		inline void setdef(ResDef &a_def) { def=&a_def; }

		sF32 tick(sF32 in)
		{
			sF32 x=def->a*in+def->b*p1+def->c*p2;
			p2=p1;
			p1=x;
			return x;
		}
	};

	static sF32 flerp(const sF32 a,const sF32 b,const sF32 x) { return a+x*(b-a); }
	static const sF32 f4=3200;
	static const sF32 f5=4000;
	static const sF32 f6=6000;
	static const sF32 bn=100;
	static const sF32 b4=200;
	static const sF32 b5=500;
	static const sF32 b6=800;

	struct syVRonan
	{
		ResDef rdef[7]; // nas,f1,f2,f3,f4,f5,f6;
		sF32 a_voicing;
		sF32 a_aspiration;
		sF32 a_frication;
		sF32 a_bypass;
	};

	struct syWRonan : syVRonan
	{
		syVRonan newframe;

		Resonator res[7];  // 0:nas, 1..6: 1..6

		sF32 lastin2;

		// settings
		const char *texts[64];
		sF32  pitch;
		sInt  framerate;

		// noise
		sU32 nseed;
		sF32 nout;

		// phonem seq
		sInt framecount;  // frame rate divider
		sInt spos;        // pos within syl definition (0..3)
		sInt scounter;    // syl duration divider
		sInt cursyl;      // current syl
		sInt durfactor;   // duration modifier
		sF32 invdur;      // 1.0 / current duration
		const char *baseptr; // pointer to start of text
		const char *ptr;  // pointer to text
		sInt curp1, curp2;  // current/last phonemes

		// sync
		sInt wait4on;
		sInt wait4off;

		// post EQ
		sF32 hpb1, hpb2;
		Resonator peq1;

		// per-instance samplerate block (the lab's file-scope `g`)
		sU32 samplerate;
		sF32 fcminuspi_sr;
		sF32 fc2pi_sr;

		// per-instance post-EQ coefficient set (the lab's static d_peq1)
		ResDef d_peq1;

		sF32 db2lin(sF32 db1, sF32 db2, sF32 x)
		{ return (sF32)sFPow(2.0,(flerp(db1,db2,x)-70)/6.0); }

		void SetFrame(const Phoneme &p1s, const Phoneme &p2s, const sF32 x, syVRonan &dest)
		{
			// (the lab keeps p1/p2 + the pointer tables static; locals here --
			// same values, per-instance/thread safe)
			Phoneme p1,p2;

			const sF32 * const p1f[]={&p1.fnf,&p1.f1f,&p1.f2f,&p1.f3f,&f4    ,&f5     ,&f6};
			const sF32 * const p1b[]={&bn    ,&p1.f1b,&p1.f2b,&p1.f3b,&b4    ,&b5     ,&b6};
			const sF32 * const p1a[]={&p1.a_n,&p1.a_1,&p1.a_2,&p1.a_3,&p1.a_4,&p1.a_56,&p1.a_56};

			const sF32 * const p2f[]={&p2.fnf,&p2.f1f,&p2.f2f,&p2.f3f,&f4    ,&f5     ,&f6};
			const sF32 * const p2b[]={&bn    ,&p2.f1b,&p2.f2b,&p2.f3b,&b4    ,&b5     ,&b6};
			const sF32 * const p2a[]={&p2.a_n,&p2.a_1,&p2.a_2,&p2.a_3,&p2.a_4,&p2.a_56,&p2.a_56};

			p1=p1s;
			p2=p2s;

			for (sInt i=0; i<7; i++)
				dest.rdef[i].set(flerp(*p1f[i],*p2f[i],x)*pitch,flerp(*p1b[i],*p2b[i],x),
				                 db2lin(*p1a[i],*p2a[i],x),fcminuspi_sr,fc2pi_sr);

			dest.a_voicing=db2lin(p1.a_voicing,p2.a_voicing,x);
			dest.a_aspiration=db2lin(p1.a_aspiration,p2.a_aspiration,x);
			dest.a_frication=db2lin(p1.a_frication,p2.a_frication,x);
			dest.a_bypass=db2lin(p1.a_bypass,p2.a_bypass,x);
		}

		#define NOISEGAIN 0.25f
		sF32 noise()
		{
			union { sU32 i; sF32 f; } val;

			// random...
			nseed=(nseed*196314165)+907633515;

			// convert to float between 2.0 and 4.0
			val.i=(nseed>>9)|0x40000000;

			// slight low pass filter...
			nout=(val.f-3.0f)+0.75f*nout;
			return nout*NOISEGAIN;
		}

		void reset()
		{
			// The lab's reset() does memset(workspace,0): it zeroes ALL sequencer
			// + DSP state (wait4on/wait4off, framecount/scounter/spos, the filter
			// delay lines, baseptr/ptr, ...). That memset is ESSENTIAL -- a CC4
			// text-select calls reset() mid-song and MUST clear wait4on, else the
			// phoneme sequencer stays stalled and the speech desyncs by a syllable
			// (candytron josie: portable lagged the binary by one note all song).
			// The lab kept texts/pitch/framerate/the samplerate block/d_peq1
			// file-scope, so its memset left them intact; this port relocated them
			// INTO syWRonan (per-instance), so we save+restore them around the
			// memset to reproduce the lab's semantics exactly.
			const char *sv_texts[64];
			for (sInt i=0; i<64; i++) sv_texts[i]=texts[i];
			sF32 sv_pitch=pitch; sInt sv_framerate=framerate;
			sU32 sv_sr=samplerate; sF32 sv_fcm=fcminuspi_sr, sv_fc2=fc2pi_sr;
			ResDef sv_peq=d_peq1;

			memset((void*)this, 0, sizeof(*this));

			for (sInt i=0; i<64; i++) texts[i]=sv_texts[i];
			pitch=sv_pitch; framerate=sv_framerate;
			samplerate=sv_sr; fcminuspi_sr=sv_fcm; fc2pi_sr=sv_fc2; d_peq1=sv_peq;

			for (sInt i=0; i<7; i++) res[i].setdef(rdef[i]);
			peq1.setdef(d_peq1);
			SetFrame(phonemes[18],phonemes[18],0,*this); // off
			SetFrame(phonemes[18],phonemes[18],0,newframe); // off
			curp1=curp2=18;
			spos=4;
		}
	};

} // namespace Ronan

using namespace Ronan;

extern "C" void __stdcall ronanCBSetSR(syWRonan *wsptr,sInt sr)
{
	wsptr->samplerate=(sU32)sr;
	wsptr->fc2pi_sr=2.0f*PI/(sF32)sr;
	wsptr->fcminuspi_sr=-wsptr->fc2pi_sr*0.5f;
}

extern "C" void __stdcall ronanCBInit(syWRonan *wsptr)
{
	// convert phoneme table to a usable format (constant result; see above)
	const sS8 *ptr=(const sS8*)rawphonemes;
	sS32 val=0;
	for (sInt f=0; f<(PTABSIZE/NPHONEMES); f++)
	{
		sF32 *dest=((sF32*)phonemes)+f;
		for (sInt p=0; p<NPHONEMES; p++)
		{
			*dest=multipliers[f]*(sF32)(val+=*ptr++);
			dest+=PTABSIZE/NPHONEMES;
		}
	}

	wsptr->reset();

	wsptr->framerate=3;
	wsptr->pitch=1.0f;

	if (!wsptr->texts[0])
		wsptr->baseptr=wsptr->ptr=nix;
	else
		wsptr->baseptr=wsptr->ptr=wsptr->texts[0];

	wsptr->lastin2=0;

	wsptr->d_peq1.set(12000,4000,2.0f,wsptr->fcminuspi_sr,wsptr->fc2pi_sr);
}

extern "C" void __stdcall ronanCBTick(syWRonan *wsptr)
{
	if (wsptr->wait4off || wsptr->wait4on) return;

	if (!wsptr->ptr) return;

	if (wsptr->framecount<=0)
	{
		wsptr->framecount=wsptr->framerate;
		// let current phoneme expire
		if (wsptr->scounter<=0)
		{
			// set to next phoneme
			wsptr->spos++;
			if (wsptr->spos >=4 || syls[wsptr->cursyl].ptab[wsptr->spos]==-1)
			{
				// go to next syllable

				if ((wsptr->ptr==0) || (wsptr->ptr[0]==0)) // empty text: silence!
				{
					wsptr->durfactor=1;
					wsptr->framecount=1;
					wsptr->cursyl=NSYLS-1;
					wsptr->spos=0;
					wsptr->ptr=wsptr->baseptr;
				}
				else if (*wsptr->ptr=='!') // wait for noteon
				{
					wsptr->framecount=0;
					wsptr->scounter=0;
					wsptr->wait4on=1;
					wsptr->ptr++;
					return;
				}
				else if (*wsptr->ptr=='_') // noteoff
				{
					wsptr->framecount=0;
					wsptr->scounter=0;
					wsptr->wait4off=1;
					wsptr->ptr++;
					return;
				}

				if (*wsptr->ptr && *wsptr->ptr!='!' && *wsptr->ptr!='_')
				{
					wsptr->durfactor=0;
					while (*wsptr->ptr>='0' && *wsptr->ptr<='9') wsptr->durfactor=10*wsptr->durfactor+(*wsptr->ptr++ - '0');
					if (!wsptr->durfactor) wsptr->durfactor=1;

					sInt fs,len=1,len2;
					for (fs=0; fs<(sInt)NSYLS-1; fs++)
					{
						const syldef &s=syls[fs];
						if ((len2=mystrnicmp1(s.syl,wsptr->ptr)))
						{
							len=len2;
							break;
						}
					}
					wsptr->cursyl=fs;
					wsptr->spos=0;
					wsptr->ptr+=len;
				}
			}

			wsptr->curp1=wsptr->curp2;
			wsptr->curp2=syls[wsptr->cursyl].ptab[wsptr->spos];
			wsptr->scounter=sFtol(phonemes[wsptr->curp2].duration*wsptr->durfactor);
			if (!wsptr->scounter) wsptr->scounter=1;
			wsptr->invdur=1.0f/((sF32)wsptr->scounter*wsptr->framerate);
		}
		wsptr->scounter--;
	}

	wsptr->framecount--;
	sF32 x=(sF32)(wsptr->scounter*wsptr->framerate+wsptr->framecount)*wsptr->invdur;
	const Phoneme &p1=phonemes[wsptr->curp1];
	const Phoneme &p2=phonemes[wsptr->curp2];
	x=(sF32)sFPow(x,(sF32)p1.rank/(sF32)p2.rank);
	wsptr->SetFrame(p2,(fabsf(p2.rank-p1.rank)>8.0f)?p2:p1,x,wsptr->newframe);
}

extern "C" void __stdcall ronanCBNoteOn(syWRonan *wsptr)
{
	wsptr->wait4on=0;
}

extern "C" void __stdcall ronanCBNoteOff(syWRonan *wsptr)
{
	wsptr->wait4off=0;
}

extern "C" void __stdcall ronanCBSetCtl(syWRonan *wsptr,sU32 ctl, sU32 val)
{
	// controller 4, 0-63			: set text #
	// controller 4, 64-127		: set frame rate
	// controller 5					: set pitch
	switch (ctl)
	{
	case 4:
		if (val<63)
		{
			wsptr->reset();

			if (wsptr->texts[val])
				wsptr->ptr=wsptr->baseptr=wsptr->texts[val];
			else
				wsptr->ptr=wsptr->baseptr=nix;
		}
		else
			wsptr->framerate=val-63;
		break;
	case 5:
		wsptr->pitch=(sF32)sFPow(2.0f,(val-64.0f)/128.0f);
		break;

	}
}

extern "C" void __stdcall ronanCBProcess(syWRonan *wsptr,sF32 *buf, sU32 len)
{
	syVRonan deltaframe; // per-call scratch (the lab keeps it static)

	// prepare interpolation
	{
		sF32 *src1=(sF32*)wsptr;
		sF32 *src2=(sF32*)&wsptr->newframe;
		sF32 *dest=(sF32*)&deltaframe;
		sF32 mul  =1.0f/(sF32)len;
		for (sU32 i=0; i<(sizeof(syVRonan)/sizeof(sF32)); i++)
			dest[i]=(src2[i]-src1[i])*mul;
	}

	for (sU32 i=0; i<len; i++)
	{
		// interpolate all values
		{
			sF32 *src=(sF32*)&deltaframe;
			sF32 *dest=(sF32*)wsptr;
			for (sU32 j=0; j<(sizeof(syVRonan)/sizeof(sF32)); j++)
				dest[j]+=src[j];
		}

		sF32 in=buf[2*i];

		// add aspiration noise
		in=in*wsptr->a_voicing+wsptr->noise()*wsptr->a_aspiration;

		// process complete input signal with f1/nasal filters
		sF32 out=wsptr->res[0].tick(in)+wsptr->res[1].tick(in);

		// differentiate input signal, add frication noise
		sF32 lin=in;
		in=(wsptr->noise()*wsptr->a_frication)+in-wsptr->lastin2;
		wsptr->lastin2=lin;

		// process diff/fric input signal with f2..f6 and bypass (phase inverted)
		for (sInt r=2; r<7; r++)
			out=wsptr->res[r].tick(in)-out;

		out=in*wsptr->a_bypass-out;

		// high pass filter
		wsptr->hpb1+=0.012f*(out=out-wsptr->hpb1);
		wsptr->hpb2+=0.012f*(out=out-wsptr->hpb2);

		// EQ
		out=wsptr->peq1.tick(out)-out;

		buf[2*i]=buf[2*i+1]=out;
	}
}

// the engine's speech blob is 64KB ("that should be enough" --synth.asm);
// the real state must fit it
static_assert(sizeof(syWRonan) <= 64*1024, "syWRonan exceeds the engine blob");

extern "C" void* __stdcall synthGetSpeechMem(void *a_pthis);

extern "C" void __stdcall synthSetLyrics(void *a_pthis,const char **a_ptr)
{
	syWRonan *wsptr=(syWRonan*)synthGetSpeechMem(a_pthis);
	for (sInt i=0; i<64; i++) wsptr->texts[i]=a_ptr[i];
	wsptr->baseptr=wsptr->ptr=wsptr->texts[0];
}

#endif // V2_RONAN
