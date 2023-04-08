#include "synthasm.h"
#include <cstddef>

//#define VUMETER
#define RONAN
#define FULLMIDI
#define FIXDENORMALS 1 // there's really no good reason to turn this off.

#ifdef RONAN

struct syWRonan;
extern "C" void ronanCBInit(syWRonan * pthis);
extern "C" void ronanCBTick(syWRonan * pthis);
extern "C" void ronanCBNoteOn(syWRonan * pthis);
extern "C" void ronanCBNoteOff(syWRonan * pthis);
extern "C" void ronanCBSetCtl(syWRonan * pthis, uint32_t ctl, uint32_t val);
extern "C" void ronanCBProcess(syWRonan * pthis, float* buf, uint32_t len);
extern "C" void ronanCBSetSR(syWRonan * pthis, int32_t samplerate);

#endif

namespace V2
{
    static constexpr uint32_t POLY = 64;            // maximum polyphony
    static constexpr uint32_t LOWEST = 0x39000000;  // out of 16bit range
    static constexpr uint32_t MDMAXOFFS = 1024;

    static constexpr uint32_t MAX_FRAME_SIZE = 280;

    static constexpr uint32_t COMPDLEN = 5700;

    struct CHANINFO
    {
        uint8_t pgm;                                // resb 1
        uint8_t ctl[7];                             // resb 7
    };

    struct syVOsc
    {
        float mode;                                 // resd 1;OSC_* (as float. it's all floats in here)
        float ring;                                 // resd 1
        float pitch;                                // resd 1
        float detune;                               // resd 1
        float color;                                // resd 1
        float gain;                                 // resd 1
    };

    struct syWOsc
    {
        int32_t mode;                               // resd 1; dword: mode(0 = tri / saw 1 = pulse 2 = sin 3 = noise)
        uint32_t ring;                              // resd 1; dword: ringmod on / off
        uint32_t cnt;                               // resd 1; dword: wave counter(32bit)
        uint32_t freq;                              // resd 1; dword: wave counter inc(8x / sample)
        uint32_t brpt;                              // resd 1; dword: break point f²r tri / pulse(32bit)
        float nffrq;                                // resd 1; noise: filter freq
        float nfres;                                // resd 1; noise: filter reso
        uint32_t nseed;                             // resd 1; noise: random seed
        float gain;                                 // resd 1; output gain
        uint32_t tmp;                               // resd 1; temp
        float nfl;                                  // resd 1; noise filter L buffer
        float nfb;                                  // resd 1; noise filter B buffer
        float note;                                 // resd 1; note
        float pitch;                                // resd 1; pitch
    };

    struct syVFlt
    {
        float mode;                                 // resd 1
        float cutoff;                               // resd 1
        float reso;                                 // resd 1
    };

    struct syVDist
    {
        float mode;                                 // resd 1; 0: off, 1 : overdrive, 2 : clip, 3 : bitcrusher, 4 : decimator
        // modes 4 to 8: filters(see syVFlt)
        float ingain;                               // resd 1; -12dB ... 36dB
        float param1;                               // resd 1; outgain / crush / outfreq
        float param2;                               // resd 1; offset / offset / xor / jitter
    };

    struct syVEnv
    {
        float ar;                                   // resd 1
        float dr;                                   // resd 1
        float sl;                                   // resd 1
        float sr;                                   // resd 1
        float rr;                                   // resd 1
        float vol;                                  // resd 1
    };

    struct syVLFO
    {
        float mode;                                 // resd 1; 0: saw, 1 : tri, 2 : pulse, 3 : sin, 4 : s & h
        float sync;                                 // resd 1; 0: free  1 : in sync with keyon
        float egmode;                               // resd 1; 0: continuous  1 : one - shot(EG mode)
        float rate;                                 // resd 1; rate(0Hz .. ~43hz)
        float phase;                                // resd 1; start phase shift
        float pol;                                  // resd 1; polarity: +, ., +/ -
        float amp;                                  // resd 1; amplification(0 .. 1)
    };

    struct syVV2
    {
        float panning;                              // resd 1; panning
        float transp;                               // resd 1; transpose
        syVOsc osc1;                                // resb syVOsc.size; oszi 1
        syVOsc osc2;                                // resb syVOsc.size; oszi 2
        syVOsc osc3;                                // resb syVOsc_size; oszi 3
        syVFlt vcf1;                                // resb offsetof(syVFlt, size; filter 1
        syVFlt vcf2;                                // resb offsetof(syVFlt, size; filter 2
        float routing;                              // resd 1; 0: single  1 : serial  2 : parallel
        float fltbal;                               // resd 1; parallel filter balance
        syVDist dist;                               // resb syVDist, size; distorter
        syVEnv aenv;                                // resb offsetof(syVEnv, size; amplitude env
        syVEnv env2;                                // resb offsetof(syVEnv, size; EG 2
        syVLFO lfo1;                                // resb syVLFO, size; LFO 1
        syVLFO lfo2;                                // resb syVLFO, size; LFO 2
        float oscsync;                              // resd 1; osc keysync flag
    };

    struct syWFlt
    {
        uint32_t mode;                              // resd 1; int: 0 - bypass, 1 - low, 2 - band, 3 - high, 4 - notch, 5 - all, 6 - moogl, 7 - moogh
        float cfreq;                                // resd 1; float: frq(0 - 1)
        float res;                                  // resd 1; float: res(0 - 1)
        float moogf;                                // resd 1; moog flt f coefficient(negative!)
        float moogp;                                // resd 1; moog flt p coefficient
        float moogq;                                // resd 1; moog flt q coefficient
        float l;                                    // resd 1
        float b;                                    // resd 1
        float mb0;                                  // resd 1
        float mb1;                                  // resd 1
        float mb2;                                  // resd 1
        float mb3;                                  // resd 1
        float mb4;                                  // resd 1
        uint32_t step;                              // resd 1
    };

    struct syWEnv
    {
        float out;                                  // resd 1
        uint32_t state;                             // resd 1; int state - 0: off, 1 : attack, 2 : decay, 3 : sustain, 4 : release
        float val;                                  // resd 1; float outval (0.0-128.0)
        float atd;                                  // resd 1; float attack delta (added every frame in phase 1, transition -> 2 at 128.0)
        float dcf;                                  // resd 1; float decay factor(mul'ed every frame in phase 2, transision -> 3 at .sul)
        float sul;                                  // resd 1; float sustain level(defines phase 2->3 transition point)
        float suf;                                  // resd 1; float sustain factor(mul'ed every frame in phase 3, transition -> 4 at gate off or ->0 at 0.0)
        float ref;                                  // resd 1; float release(mul'ed every frame in phase 4, transition ->0 at 0.0)
        float gain;                                 // resd 1; float gain(0.1 .. 1.0)
    };

    struct syWLFO
    {
        float out;                                  // resd 1; float: out
        uint32_t mode;                              // resd 1; int: mode
        uint32_t fs;                                // resd 1; int: sync flag
        uint32_t feg;                               // resd 1; int: eg flag
        uint32_t freq;                              // resd 1; int: freq
        uint32_t cntr;                              // resd 1; int: counter
        uint32_t cph;                               // resd 1; int: counter sync phase
        float gain;                                 // resd 1; float: output gain
        float dc;                                   // resd 1; float: output DC
        uint32_t nseed;                             // resd 1; int: random seed
        uint32_t last;                              // resd 1; int: last counter value(for s& h transition)
    };

    struct syWDist
    {
        uint32_t mode;                              // resd 1
        float gain1;                                // resd 1; float: input gain for all fx
        float gain2;                                // resd 1; float: output gain for od / clip
        float offs;                                 // resd 1; float: offs for od / clip
        float crush1;                               // resd 1; float: crush factor ^ -1
        int32_t crush2;                             // resd 1; int:  crush factor ^ 1
        int32_t crxor;                              // resd 1; int:xor value for crush
        int32_t dcount;                             // resd 1; int:   decimator counter
        int32_t dfreq;                              // resd 1; int:   decimator frequency
        float dvall;                                // resd 1; float: last decimator value(mono / left)
        float dvalr;                                // resd 1; float: last decimator value(right)
        float dlp1c;                                // resd 1; float: decimator pre - filter coefficient
        float dlp1bl;                               // resd 1; float: decimator pre - filter buffer(mono / left)
        float dlp1br;                               // resd 1; float: decimator pre - filter buffer(right)
        float dlp2c;                                // resd 1; float: decimator post - filter coefficient
        float dlp2bl;                               // resd 1; float: decimator post - filter buffer(mono / left)
        float dlp2br;                               // resd 1; float: decimator post - filter buffer(right)
        syWFlt fw1;                                 // resb syWFlt.size; left / mono filter workspace
        syWFlt fw2;                                 // resb syWFlt.size; right filter workspace
    };

    struct syWDCF
    {
        float xm1l;                                 // resd 1
        float ym1l;                                 // resd 1
        float xm1r;                                 // resd 1
        float ym1r;                                 // resd 1
    };

    struct syWV2
    {
        int32_t note;                               // resd 1
        float velo;                                 // resd 1
        uint32_t gate;                              // resd 1
        float curvol;                               // resd 1
        float volramp;                              // resd 1
        float xpose;                                // resd 1
        int32_t fmode;                              // resd 1
        float lvol;                                 // resd 1
        float rvol;                                 // resd 1
        float f1gain;                               // resd 1
        float f2gain;                               // resd 1
        int32_t oks;                                // resd 1
        syWOsc osc1;                                // resb syWOsc.size
        syWOsc osc2;                                // resb syWOsc.size
        syWOsc osc3;                                // resb syWOsc.size
        syWFlt vcf1;                                // resb syWFlt.size; filter 1
        syWFlt vcf2;                                // resb syWFlt.size; filter 2
        syWEnv aenv;                                // resb syWEnv.size
        syWEnv env2;                                // resb syWEnv.size
        syWLFO lfo1;                                // resb syWLFO.size; LFO 1
        syWLFO lfo2;                                // resb syWLFO.size; LFO 2
        syWDist dist;                               // resb syWDist.size; distorter
        syWDCF dcf;                                 // resb syWDCF.size; post dc filter
    };

    struct syVBoost
    {
        float amount;                               // resd 1; boost in dB(0..18)
    };

    struct syWBoost
    {
        uint32_t ena;                               // resd 1
        float a1;                                   // resd 1
        float a2;                                   // resd 1
        float b0;                                   // resd 1
        float b1;                                   // resd 1
        float b2;                                   // resd 1
        float x1[2];                                // resd 2
        float x2[2];                                // resd 2
        float y1[2];                                // resd 2
        float y2[2];                                // resd 2
    };

    struct syVModDel
    {
        float amount;                               // resd 1; dry / eff value(0 = -eff, 64 = dry, 127 = eff)
        float fb;                                   // resd 1; feedback(0 = -100 %, 64 = 0 %, 127 = ~+ 100 %)
        float llength;                              // resd 1; length of left delay
        float rlength;                              // resd 1; length of right delay
        float mrate;                                // resd 1; modulation rate
        float mdepth;                               // resd 1; modulation depth
        float mphase;                               // resd 1; modulation stereo phase (0=-180², 64=0², 127=180²)
    };

    struct syWModDel
    {
        float* db1;                                 // resd 1; ptr: delay buffer 1
        float* db2;                                 // resd 1; ptr: delay buffer 2
        uint32_t dbufmask;                          // resd 1; int: delay buffer mask
        uint32_t dbptr;                             // resd 1; int: buffer write pos
        uint32_t db1offs;                           // resd 1; int: buffer 1 offset
        uint32_t db2offs;                           // resd 1; int: buffer 2 offset
        uint32_t mcnt;                              // resd 1; mod counter
        uint32_t mfreq;                             // resd 1; mod freq
        uint32_t mphase;                            // resd 1; mod phase
        uint32_t mmaxoffs;                          // resd 1; mod max offs(2048samples * depth)
        float fbval;                                // resd 1; float: feedback val
        float dryout;                               // resd 1; float: dry out
        float effout;                               // resd 1; float: eff out
    };

    struct syVComp
    {
        float mode;                                 // resd 1; 0: off / 1 : Peak / 2 : RMS
        float stereo;                               // resd 1; 0: mono / 1 : stereo
        float autogain;                             // resd 1; 0: off / 1 : on
        float lookahead;                            // resd 1; lookahead in ms
        float threshold;                            // resd 1; threshold(-54dB .. 6dB ? )
        float ratio;                                // resd 1; ratio(0 : 1 : 1 ... 127 : 1 : inf)
        float attack;                               // resd 1; attack value
        float release;                              // resd 1; release value
        float outgain;                              // resd 1; output gain
    };

    struct syWComp
    {
        uint32_t mode;                              // resd 1; int: mode(bit0 : peak / rms, bit1 : stereo, bit2 : off)
        uint32_t oldmode;                           // resd 1; int: last mode
        float invol;                                // resd 1; flt: input gain(1 / threshold, internal threshold is always 0dB)
        float ratio;                                // resd 1; flt: ratio
        float outvol;                               // resd 1; flt: output gain(outgain* threshold)
        float attack;                               // resd 1; flt: attack(lpf coeff, 0..1)
        float release;                              // resd 1; flt: release(lpf coeff, 0..1)
        uint32_t dblen;                             // resd 1; int: lookahead buffer length
        uint32_t dbcnt;                             // resd 1; int: lookahead buffer offset
        float curgain1;                             // resd 1; flt: left current gain
        float curgain2;                             // resd 1; flt: right current gain
        float pkval1;                               // resd 1; flt: left / mono peak value
        float pkval2;                               // resd 1; flt: right peak value
        uint32_t rmscnt;                            // resd 1; int: RMS buffer offset
        float rmsval1;                              // resd 1; flt: left / mono RMS current value
        float rmsval2;                              // resd 1; flt: right RMS current value
        float dbuf[2 * COMPDLEN];                   // resd 2 * COMPDLEN; lookahead delay buffer
        float rmsbuf[2 * 8192];                     // resd 2 * 8192; RMS ring buffer
    };

    struct syVChan
    {
        float chanvol;                              // resd 1
        float auxarcv;                              // resd 1; CHAOS aux a receive
        float auxbrcv;                              // resd 1; CHAOS aux b receive
        float auxasnd;                              // resd 1; CHAOS aux a send
        float auxbsnd;                              // resd 1; CHAOS aux b send
        float aux1;                                 // resd 1
        float aux2;                                 // resd 1
        float fxroute;                              // resd 1
        syVBoost boost;                             // resb syVBoost.size
        syVDist cdist;                              // resb syVDist.size
        syVModDel chorus;                           // resb syVModDel.size
        syVComp compr;                              // resb syVComp.size
    };

    struct syWChan
    {
        float chgain;                               // resd 1
        float a1gain;                               // resd 1
        float a2gain;                               // resd 1
        float aasnd;                                // resd 1
        float absnd;                                // resd 1
        float aarcv;                                // resd 1
        float abrcv;                                // resd 1
        uint32_t fxr;                               // resd 1
        syWDCF dcf1w;                               // resb syWDCF.size
        syWBoost boostw;                            // resb syWBoost.size
        syWDist distw;                              // resb syWDist.size
        syWDCF dcf2w;                               // resb syWDCF.size
        syWModDel chrw;                             // resb syWModDel.size
        syWComp compw;                              // resb syWComp.size
    };

    struct syVReverb
    {
        float revtime;                              // resd 1
        float highcut;                              // resd 1
        float lowcut;                               // resd 1
        float vol;                                  // resd 1
    };

    struct syCReverb
    {
        float gainc0;                               // resd 1; feedback gain for comb filter delay 0
        float gainc1;                               // resd 1; feedback gain for comb filter delay 1
        float gainc2;                               // resd 1; feedback gain for comb filter delay 2
        float gainc3;                               // resd 1; feedback gain for comb filter delay 3
        float gaina0;                               // resd 1; feedback gain for allpass delay 0
        float gaina1;                               // resd 1; feedback gain for allpass delay 1
        float gainin;                               // resd 1; input gain
        float damp;                                 // resd 1; high cut(1 - val²)
        float lowcut;                               // resd 1; low cut(val²)
    };

    // lengths of delay lines in samples
    static constexpr uint32_t lencl0 = 1309;        // left comb filter delay 0
    static constexpr uint32_t lencl1 = 1635;           // left comb filter delay 1
    static constexpr uint32_t lencl2 = 1811;        // left comb filter delay 2
    static constexpr uint32_t lencl3 = 1926;        // left comb filter delay 3
    static constexpr uint32_t lenal0 = 220;         // left all pass delay 0
    static constexpr uint32_t lenal1 = 74;          // left all pass delay 1
    static constexpr uint32_t lencr0 = 1327;        // right comb filter delay 0
    static constexpr uint32_t lencr1 = 1631;        // right comb filter delay 1
    static constexpr uint32_t lencr2 = 1833;        // right comb filter delay 2
    static constexpr uint32_t lencr3 = 1901;        // right comb filter delay 3
    static constexpr uint32_t lenar0 = 205;         // right all pass delay 0
    static constexpr uint32_t lenar1 = 77;          // right all pass delay 1

    struct syWReverb
    {
        syCReverb setup;                            // resb syCReverb.size
        // positions of delay lines
        uint32_t poscl0;                            // resd 1
        uint32_t poscl1;                            // resd 1
        uint32_t poscl2;                            // resd 1
        uint32_t poscl3;                            // resd 1
        uint32_t posal0;                            // resd 1
        uint32_t posal1;                            // resd 1
        uint32_t poscr0;                            // resd 1
        uint32_t poscr1;                            // resd 1
        uint32_t poscr2;                            // resd 1
        uint32_t poscr3;                            // resd 1
        uint32_t posar0;                            // resd 1
        uint32_t posar1;                            // resd 1
        // comb delay low pass filter buffers (y(k-1))
        float lpfcl0;                               // resd 1
        float lpfcl1;                               // resd 1
        float lpfcl2;                               // resd 1
        float lpfcl3;                               // resd 1
        float lpfcr0;                               // resd 1
        float lpfcr1;                               // resd 1
        float lpfcr2;                               // resd 1
        float lpfcr3;                               // resd 1
        // memory for low cut filters
        float hpfcl;                                // resd 1
        float hpfcr;                                // resd 1
        // memory for the delay lines
        float linecl0[lencl0];                      // resd lencl0
        float linecl1[lencl1];                      // resd lencl1
        float linecl2[lencl2];                      // resd lencl2
        float linecl3[lencl3];                      // resd lencl3
        float lineal0[lenal0];                      // resd lenal0
        float lineal1[lenal1];                      // resd lenal1
        float linecr0[lencr0];                      // resd lencr0
        float linecr1[lencr1];                      // resd lencr1
        float linecr2[lencr2];                      // resd lencr2
        float linecr3[lencr3];                      // resd lencr3
        float linear0[lenar0];                      // resd lenar0
        float linear1[lenar1];                      // resd lenar1
    };

    struct SYN
    {
        uint64_t patchmap;                          // resd 1 <- ptr to resq
        uint32_t mrstat;                            // resd 1
        uint32_t curalloc;                          // resd 1
        uint32_t samplerate;                        // resd 1
        int32_t chanmap[POLY];                      // resd POLY
        uint32_t allocpos[POLY];                    // resd POLY
        int32_t voicemap[16];                       // resd 16
        int32_t tickd;                              // resd 1

        CHANINFO chans[16];                         // resb 16 * CHANINFO.size
        syVV2 voicesv[POLY];                        // resb POLY * syVV2.size
        syWV2 voicesw[POLY];                        // resb POLY * syWV2.size
        syVChan chansv[16];                         // resb 16 * syVChan.size
        syWChan chansw[16];                         // resb 16 * syWChan.size

        struct Globals
        {
            syVReverb rvbparm;                      // resb syVReverb.size
            syVModDel delparm;                      // resb syVModDel.size
            float vlowcut;                          // resd 1
            float vhighcut;                         // resd 1
            syVComp cprparm;                        // resb syVComp.size
            uint8_t guicolor;                       // resb 1; CHAOS gui logo color(has nothing to do with sound but should be saved with patch)
        } globals;

        syWReverb reverb;                           // resb syWReverb.size
        syWModDel delay;                            // resb syWModDel.size
        syWDCF dcf;                                 // resb syWDCF.size
        syWComp compr;                              // resb syWComp.size
        float lcfreq;                               // resd 1
        float lcbufl;                               // resd 1
        float lcbufr;                               // resd 1
        float hcfreq;                               // resd 1
        float hcbufl;                               // resd 1
        float hcbufr;                               // resd 1

        float maindelbuf[2 * 32768];                // resd 2 * 32768
        float chandelbuf[16 * 2 * 2048];            // resd 16 * 2 * 2048
        float vcebuf[MAX_FRAME_SIZE];               // resd MAX_FRAME_SIZE
        float vcebuf2[MAX_FRAME_SIZE];              // resd MAX_FRAME_SIZE
        float chanbuf[2 * MAX_FRAME_SIZE];          // resd 2 * MAX_FRAME_SIZE
        float aux1buf[MAX_FRAME_SIZE];              // resd MAX_FRAME_SIZE
        float aux2buf[MAX_FRAME_SIZE];              // resd MAX_FRAME_SIZE
        float mixbuf[2 * MAX_FRAME_SIZE];           // resd 2 * MAX_FRAME_SIZE
        float auxabuf[2 * MAX_FRAME_SIZE];          // resd 2 * MAX_FRAME_SIZE
        float auxbbuf[2 * MAX_FRAME_SIZE];          // resd 2 * MAX_FRAME_SIZE

        uint8_t ronanw[65536];                      // resb 65536; that should be enough

#ifdef VUMETER
        uint32_t vumode;                            // resd 1
        uint32_t chanvu[2 * 16];                    // resd 2 * 16
        uint32_t mainvu[2];                         // resd 2
#endif
    };

    static constexpr uint32_t icnoisemul = 196314165;
    static constexpr uint32_t icnoiseadd = 907633515;

    //#####################################################################################
    // Oberdings
    // SOUNDDEFINITIONEN F²R DEN WELTFRIEDEN
    //#####################################################################################

    struct v2mod
    {
        uint8_t source;                             // resb 1; source: vel / ctl1 - 7 / aenv / env2 / lfo1 / lfo2
        uint8_t val;                                // resb 1; 0 = -1 .. 128 = 1
        uint8_t dest;                               // resb 1; destination(index into v2sound)
    };

    struct v2sound
    {
        // voice dependent sachen

        uint8_t voice[sizeof(syVV2) / 4];           // resb syVV2.size/4
        //endvdvals

        // globale pro-channel-sachen
        uint8_t chan[sizeof(syVChan) / 4];          // resb syVChan.size/4
        //.endcdvals

        // manuelles rumgefake
        uint8_t maxpoly;                            // resb 1

        // modmatrix
        uint8_t modnum;                             // resb 1

        v2mod modmatrix;
    };
    static constexpr uint64_t v2sound_endvdvals = offsetof(v2sound, voice) + sizeof(syVV2) / 4;
    static constexpr uint64_t v2sound_endcdvals = offsetof(v2sound, chan) + sizeof(syVChan) / 4;

    SynthAsm::SynthAsm()
        : m_mem(new uint8_t[sizeof(SYN)])
    {}

    SynthAsm::~SynthAsm()
    {
        delete[] m_mem;
    }

    //#####################################################################################
    //  Helper Routines
    //#####################################################################################

    // fast atan
    // value in st0, -1 in st1, high 16bits in ax, "8" in ebx
    void SynthAsm::fastatan()
    {
        SHL(ax, 1);
        FLD1();             // <1> <val> <-1>
        FCMOVB(st0, st2);   // <sign> <val> <-1>

        XOR(edx, edx);
        CMP(ah, 0x7f);
        FMUL(st1, st0);     // <sign> <absval> <-1>
        CMOVGE(edx, ebx);
        FXCH(st1);          // <x> <sign> <-1>

        // r(x) = (cx1 * x + cx3 * x ^ 3) / (cxm0 + cxm2 * x ^ 2 + cxm4 * x ^ 4)
        FLD(st0);                       // <x> <x> <sign> <-1>
        FMUL(st0, st0);                 // <x²> <x> <sign> <-1>
        FLD(st0);                       // <x²> <x²> <x> <sign> <-1>
        FLD(st0);                       // <x²> <x²> <x²> <x> <sign> <-1>
        FMUL(qword(m_fcatanx3 + edx.data / 8)); // <x²* cx3> <x²> <x²> <x> <sign> <-1>
        FXCH(st1);                      // <x²> <x²(cx3)> <x²> <x> <sign> <-1>
        FMUL(qword(m_fcatanxm4 + edx.data / 8)); // <x²(cxm4)> <x²(cx3)> <x²> <x> <sign> <-1>
        FXCH(st1);                      // <x²(cx3)> <x²(cxm4)> <x²> <x> <sign> <-1>
        FADD(qword(m_fcatanx1 + edx.data / 8)); // <cx1 + x²(cx3)> <x²(cxm4)> <x²> <x> <sign> <-1>
        FXCH(st1);                      // <x²(cxm4)> <cx1 + x²(cx3)> <x²> <x> <sign> <-1>
        FADD(qword(m_fcatanxm2));       // <cxm2 + x²(cxm4)> <cx1 + x²(cx3)> <x²> <x> <sign> <-1>
        FXCH(st1);                      // <cx1 + x²(cx3)> <cxm2 + x²(cxm4)> <x²> <x> <sign> <-1>
        FMULP(st3, st0);                // <cxm2 + x²(cxm4)> <x²> <x(cx1) + x²(cx3)> <sign> <-1>
        FMULP(st1, st0);                // <x²(cxm2) + x ^ 4(cxm4)> <x(cx1) + x²(cx3)> <sign> <-1>
        FADD(qword(m_fcatanxm0 + edx.data / 8)); // <cxm0 + x²(cxm2) + x ^ 4(cxm4)> <x(cx1) + x²(cx3)> <sign> <-1>
        FDIVP(st1, st0);                // <r(x)> <sign> <-1>
        FADD(qword(m_fcatanadd + edx.data / 8)); // <r(x) + adder> <sign> <-1>

        FMULP(st1, st0);                // <sign*r'(x)> <-1>
    }

    // x + ax3 + bx5 + cx7
    // ((((c* x²) + b)* x² + a)* x² + 1)* x

    // fast sinus with range check
    void SynthAsm::fastsinrc()
    {
        FLD(dword(m_fc2pi));    // <2pi> <x>
        FXCH(st1);              // <x> <2pi>
        FPREM();                // <x'> <2pi>
        FXCH(st1);              // <2pi> <x'>
        FSTP(st0);              // <x'>

        FLD1();                 // <1> <x>
        FLDZ();                 // <0> <1> <x>
        FSUB(st0, st1);         // <mul> <1> <x>
        FLDPI();                // <sub> <mul> <1> <x>

        FLD(dword(m_fcpi_2));   // <pi / 2> <sub> <mul> <1> <x>
        FCOMI(st0, st4);
        FSTP(st0);              // <sub> <mul> <1> <x>
        FLDZ();                 // <0> <sub> <mul> <1> <x>
        FXCH(st1);              // <sub> <0> <mul> <1> <x>
        FCMOVNB(st0, st1);      // < sub'> <0> <mul> <1> <x>
        FXCH(st1);              // <0> <sub'> <mul> <1> <x>
        FSTP(st0);              // <sub'> <mul> <1> <x>
        FXCH(st1);              // <mul> <sub'> <1> <x>
        FCMOVNB(st0, st2);      // <mul'> <sub'> <1> <x>

        FLD(dword(m_fc1p5pi));  // <1.5pi> <mul'> <sub'> <1> <x>
        FCOMI(st0, st4);
        FSTP(st0);              // <mul'> <sub'> <1> <x>
        FLD(dword(m_fc2pi));    // <2pi> <mul'> <sub'> <1> <x>
        FXCH(st1);              // <mul'> <2pi> <sub'> <1> <x>
        FCMOVB(st0, st3);       // <mul''> <2pi> < sub'> <1> <x>
        FXCH(st2);              // <sub'> <2pi> <mul''> <1> <x>
        FCMOVB(st0, st1);       // <sub''> <2pi> <mul''> <1> <x>
        FSUBP(st4, st0);        // <2pi> <mul''> <1> <x - sub>
        FSTP(st0);              // <mul''> <1> <x - sub>
        FMULP(st2, st0);        // <1> <mul(x - sub)>
        FSTP(st0);              // <mul(x - sub)>

        fastsin();
     }

    // ; fast sinus approximation(st0->st0) from - pi / 2 to pi / 2, about - 80dB error, should be ok
    void SynthAsm::fastsin()
    {
        FLD(st0);               // <x> <x>
        FMUL(st0, st1);         // <x²> <x>
        FLD(qword(m_fcsinx7));  // <c> <x²> <x>
        FMUL(st0, st1);         // <cx²> <x²> <x>
        FADD(qword(m_fcsinx5)); // <b + cx²> <x²> <x>
        FMUL(st0, st1);         // <x²(b + cx²)> <x²> <x>
        FADD(qword(m_fcsinx3)); // <a + x²(b + cx²)> <x²> <x>
        FMULP(st1, st0);        // <x²(a + x²(b + cx²)> <x>
        FLD1();                 // <1> <x²(a + x²(b + cx²)> <x>
        FADDP(st1, st0);        // <1 + x²(a + x²(b + cx²)> <x>
        FMULP(st1, st0);        // <x(1 + x²(a + x²(b + cx²))>
    }

    // new SR in eax
    void SynthAsm::calcNewSampleRate()
    {
        MOV(m_temp, eax);
        FILD(dword(m_temp));            // <sr>
        FLD1();                         // <1> <sr>
        FDIV(st0, st1);                 // <1/sr> <sr>
        FLD(dword(m_fcoscbase));        // <oscbHz> <1/sr> <sr>
        FMUL(dword(m_fc32bit));         // <oscb32>  <1/sr> <sr>
        FMUL(st0, st1);                 // <oscbout> <1/sr> <sr>
        FSTP(dword(m_SRfcobasefrq));    // <1/sr> <sr>
        FLD(dword(m_fcsrbase));         // <srbase> <1/sr> <sr>
        FMUL(st0, st1);                 // <linfrq> <1/sr> <sr>
        FSTP(dword(m_SRfclinfreq));     // <1/sr> <sr>
        FLD(st0);                       // <1/sr> <1/sr> <sr>
        FMUL(dword(m_fc2pi));           // <boalpha> <1/sr> <sr>
        FMUL(dword(m_fcboostfreq));     // <bof/sr> <1/sr> <sr>
        FSINCOS();                      // <cos> <sin> <1/sr> <sr>
        FSTP(dword(m_SRfcBoostCos));    // <sin> <1/sr> <sr>
        FSTP(dword(m_SRfcBoostSin));    // <1/sr> <sr>
        FMUL(dword(m_fcdcflt));         // <126/sr> <sr>
        FLD1();                         // <1> <126/sr> <sr>
        FSUBRP(st1, st0);               // <dcfilterR> <sr>
        FSTP(dword(m_SRfcdcfilter));    // <sr>
        FMUL(dword(m_fcframebase));     // <framebase*sr>
        FDIV(dword(m_fcsrbase));        // <framelen>
        FADD(dword(m_fci2));            // <round framelen>
        FISTP(dword(m_SRcFrameSize));   // -
        FILD(dword(m_SRcFrameSize));    // <framelen>
        FLD1();                         // <1> <framelen>
        FDIVRP(st1, st0);               // <1/framelen>
        FSTP(dword(m_SRfciframe));      // -
    }

    void SynthAsm::calcfreq2()
    {
        FLD1();                     // 1 <0..1>
        FSUBP(st1, st0);            // <-1..0>
        FMUL(dword(m_fccfframe));   // <-11..0> -> <1/2048..1>

        FLD1();
        FLD(st1);
        FPREM();
        F2XM1();
        FADDP(st1, st0);
        FSCALE();
        FSTP(st1);
    }

    // (0..1 linear zu 2 ^ -10..1)
    void SynthAsm::calcfreq()
    {
        FLD1();                 // 1 <0..1>
        FSUBP(st1, st0);        // <-1..0>
        FMUL(dword(m_fc10));    // <-10..0> -> <1/1024..1>

        FLD1();
        FLD(st1);
        FPREM();
        F2XM1();
        FADDP(st1, st0);
        FSCALE();
        FSTP(st1);
    }

    void SynthAsm::pow2()
    {
        FLD1();
        FLD(st1);
        FPREM();
        F2XM1();
        FADDP(st1, st0);
        FSCALE();
        FSTP(st1);
    }

    void SynthAsm::pow()
    {
        FYL2X();
        FLD1();
        FLD(st1);
        FPREM();
        F2XM1();
        FADDP(st1, st0);
        FSCALE();
        FSTP(st1);
    }

//#####################################################################################
//  Oszillator
//#####################################################################################

    // ecx: osc index for noise seed
    void SynthAsm::syOscInit()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(rbp[offsetof(syWOsc, cnt)], eax);
        MOV(rbp[offsetof(syWOsc, nfl)], eax);
        MOV(rbp[offsetof(syWOsc, nfb)], eax);
        MOV(eax, dword(m_oscseeds + /*4 * */ecx.data));
        MOV(rbp[offsetof(syWOsc, nseed)], eax);
        POPAD();
    }

    void SynthAsm::syOscNoteOn()
    {
        PUSHAD();
        syOscChgPitch();
        POPAD();
    }

    void SynthAsm::syOscChgPitch()
    {
        FLD(dword(rbp[offsetof(syWOsc, pitch)]));
        FLD(st0);
        FADD(dword(m_fc64));
        FMUL(dword(m_fci128));
        calcfreq();
        FMUL(dword(m_SRfclinfreq));
        FSTP(dword(rbp[offsetof(syWOsc, nffrq)]));
        FADD(dword(rbp[offsetof(syWOsc, note)]));
        FSUB(dword(m_fcOscPitchOffs));
        FMUL(dword(m_fci12));
        pow2();
        FMUL(dword(m_SRfcobasefrq));
        FISTP(dword(rbp[offsetof(syWOsc, freq)]));
    }

    void SynthAsm::syOscSet()
    {
        PUSHAD();
        FLD(dword(rsi[offsetof(syVOsc, mode)]));
        FISTP(dword(rbp[offsetof(syWOsc, mode)]));

        FLD(dword(rsi[offsetof(syVOsc, ring)]));
        FISTP(dword(rbp[offsetof(syWOsc, ring)]));

        FLD(dword(rsi[offsetof(syVOsc, pitch)]));
        FSUB(dword(m_fc64));
        FLD(dword(rsi[offsetof(syVOsc, detune)]));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci128));
        FADDP(st1, st0);
        FSTP(dword(rbp[offsetof(syWOsc, pitch)]));
        syOscChgPitch();

        FLD(dword(rsi[offsetof(syVOsc, gain)]));
        FMUL(dword(m_fci128));
        FSTP(dword(rbp[offsetof(syWOsc, gain)]));

        FLD(dword(rsi[offsetof(syVOsc, color)]));   // <c>
        FMUL(dword(m_fci128));                      // <c'>
        FLD(st0);                                   // <c'> <c'>
        FMUL(dword(m_fc32bit));                     // <bp> <c'>
        FISTP(dword(rbp[offsetof(syWOsc, brpt)]));  // <c'>
        SHL(dword(rbp[offsetof(syWOsc, brpt)]), 1);

        FSQRT();                                    // <rc'>
        FLD1();                                     // <1> <rc'>
        FSUBRP(st1, st0);                           // <1-rc'>
        FSTP(dword(rbp[offsetof(syWOsc, nfres)]));  // -

        POPAD();
    }

    // rdi: dest buf
    // rsi: source buf
    // ecx: # of samples
    // rbp: workspace
    void SynthAsm::syOscRender()
    {
        // oscjtab dd syOscRender.off, syOscRender.mode0, syOscRender.mode1, syOscRender.mode2,
        //         dd syOscRender.mode3, syOscRender.mode4, syOscRender.auxa, syOscRender.auxb

        PUSHAD();
        rdi.data += 4 * ecx.data;// lea edi, [edi + 4 * ecx]
        NEG(ecx);
        MOVZX(eax, byte(rbp[offsetof(syWOsc, mode)]));
        AND(al, 7);
        //jmp   dword [oscjtab + 4*eax]
        if (eax.data == 0) goto off; if (eax.data == 1) goto mode0; if (eax.data == 2) goto mode1; if (eax.data == 3) goto mode2;
        if (eax.data == 4) goto mode3; if (eax.data == 5) goto mode4; if (eax.data == 6) goto auxa; goto auxb;


        /*
        .m0casetab   dd syOscRender.m0c2,     ; ...
                     dd syOscRender.m0c1,     ; ..n , INVALID!
                     dd syOscRender.m0c212,   ; .c.
                     dd syOscRender.m0c21,    ; .cn
                     dd syOscRender.m0c12,    ; o..
                     dd syOscRender.m0c1,     ; o.n
                     dd syOscRender.m0c2,     ; oc. , INVALID!
                     dd syOscRender.m0c121    ; ocn
        */

    mode0:     // tri/saw
        MOV(eax, rbp[offsetof(syWOsc, cnt)]);
        MOV(esi, rbp[offsetof(syWOsc, freq)]);

        // calc float helper values
        MOV(ebx, esi);
        SHR(ebx, 9);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);
        FLD(dword(rbp[offsetof(syWOsc, gain)]));    // <g>
        FLD1();                                     // 1 <g>
        FSUBR(dword(rbp[offsetof(syWOsc, tmp)]));   // <f> <g>
        FLD1();                                     // <1> <f> <g>
        FDIV(st0, st1);                             // <1/f> <f> <g>
        FLD(st2);                                   // <g> <1/f> <f> <g>
        MOV(ebx, rbp[offsetof(syWOsc, brpt)]);
        SHR(ebx, 9);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);
        FLD(dword(rbp[offsetof(syWOsc, tmp)]));     // <b> <g> <1/f> <f> <g>
        FLD1();                                     // <1> <b> <g> <1/f> <f> <g>
        FSUBR(st0, st1);                            // <col> <b> <g> <1/f> <f> <g>

        // m1=2/col
        // m2=-2/(1-col)
        // c1=gain/2*m1 = gain/col
        // c2=gain/2*m2 = -gain/(1-col)
        FLD(st0);                                   // <col> <col> <b> <g> <1/f> <f> <g>
        FDIVR(st0, st3);                            // <c1> <col> <b> <g> <1/f> <f> <g>
        FLD1();                                     // <1> <c1> <col> <b> <g> <1/f> <f> <g>
        FSUBRP(st2, st0);                           // <c1> <1-col> <b> <g> <1/f> <f> <g>
        FXCH(st1);                                  // <1-col> <c1> <b> <g> <1/f> <f> <g>

        FDIVP(st3, st0);                            // <c1> <b> <g/(1-col)> <1/f> <f> <g>
        FXCH(st2);                                  // <g/(1-col)> <b> <c1> <1/f> <f> <g>
        FCHS();                                     // <c2> <b> <c1> <1/f> <f> <g>

        // calc state
        MOV(ebx, eax);
        SUB(ebx, esi);                              // ................................  c
        RCR(edx, 1);                                // c...............................  .
        CMP(ebx, rbp[offsetof(syWOsc, brpt)]);      // c...............................  n
        RCL(edx, 2);                                // ..............................nc  .


    m0loop:
        MOV(ebx, eax);
        SHR(ebx, 9);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);

        FLD(dword(rbp[offsetof(syWOsc, tmp)]));     // <p+b> <c2> <b> <c1> <1/f> <f> <g>
        FSUB(st0, st2);                             // <p> <c2> <b> <c1> <1/f> <f> <g>
        CMP(eax, rbp[offsetof(syWOsc, brpt)]);      // ..............................oc  n
        RCL(edx, 1);                                // .............................ocn  .
        AND(edx, 7);                                // 00000000000000000000000000000ocn
        //jmp  dword [cs: .m0casetab + 4*edx]
        if (edx.data == 0) goto m0c2; if (edx.data == 1) goto m0c1; if (edx.data == 2) goto m0c212; if (edx.data == 3) goto m0c21;
        if (edx.data == 4) goto m0c12; if (edx.data == 5) goto m0c1; if (edx.data == 6) goto m0c2; goto m0c121;

        // cases: on entry <p> <c2> <b> <c1> <1/f> <f>, on exit: <y> <c2> <b> <c1> <1/f> <f>

    m0c21: // y=-(g+c2(p-f+1)²-c1p²)*(1/f)
        FLD1();                                     // <1> <p> <c2> <b> <c1> <1/f> <f> <g>
        FADD(st0, st1);                             // <p+1> <p> <c2> <b> <c1> <1/f> <f> <g>
        FSUB(st0, st6);                             // <p+1-f> <p> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st0);                             // <(p+1-f)²> <p> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st2);                             // <c2(p+1-f)²> <p> <c2> <b> <c1> <1/f> <f> <g>
        FXCH(st1);                                  // <p> <c2(p+1-f)²> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st0);                             // <p²> <c2(p+1-f)²> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st4);                             // <c1p²> <c2(p+1-f)²> <c2> <b> <c1> <1/f> <f> <g>
        FSUBP(st1, st0);                            // <c2(p-f+1)²-c1p²> <c2> <b> <c1> <1/f> <f> <g>
        FADD(st0, st6);                             // <g+c2(p-f+1)²-c1p²> <c2> <b> <c1> <1/f> <f> <g>
        FCHS();                                     // <-(g+c2(p-f+1)²-c1p²)> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st4);                             // <y> <c2> <b> <c1> <1/f> <f> <g>
        goto m0pl;


    m0c121: // y=(-g-c1(1-f)(2p+1-f))*(1/f)
        FADD(st0, st0);                             // <2p> <c2> <b> <c1> <1/f> <f> <g>
        FLD1();                                     // <1> <2p> <c2> <b> <c1> <1/f> <f> <g>
        FSUB(st0, st6);                             // <1-f> <2p> <c2> <b> <c1> <1/f> <f> <g>
        FXCH(st1);                                  // <2p> <1-f> <c2> <b> <c1> <1/f> <f> <g>
        FADD(st0, st1);                             // <2p+1-f> <1-f> <c2> <b> <c1> <1/f> <f> <g>
        FMULP(st1, st0);                            // <(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st3);                             // <c1(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FADD(st0, st6);                             // <g+c1(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FCHS();                                     // <-g-c1(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st4);                             // <y> <c2> <b> <c1> <1/f> <f> <g>
        goto m0pl;


    m0c212: // y=(g-c2(1-f)(2p+1-f))*(1/f)
        FADD(st0, st0);                             // <2p> <c2> <b> <c1> <1/f> <f> <g>
        FLD1();                                     // <1> <2p> <c2> <b> <c1> <1/f> <f> <g>
        FSUB(st0, st6);                             // <1-f> <2p> <c2> <b> <c1> <1/f> <f> <g>
        FXCH(st1);                                  // <2p> <1-f> <c2> <b> <c1> <1/f> <f> <g>
        FADD(st0, st1);                             // <2p+1-f> <1-f> <c2> <b> <c1> <1/f> <f> <g>
        FMULP(st1, st0);                            // <(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st1);                             // <c2(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FADD(st0, st6);                             // <g+c2(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FCHS();                                     // <-g-c2(1-f)(2p+1-f)> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st4);                             // <y> <c2> <b> <c1> <1/f> <f> <g>
        goto m0pl;


    m0c12: // y=(c2(p²)-c1((p-f)²))*(1/f)
        FLD(st0);                                   // <p> <p> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st0);                             // <p²> <p> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st2);                             // <c2(p²)> <p> <c2> <b> <c1> <1/f> <f> <g>
        FXCH(st1);                                  // <p> <c2(p²)> <c2> <b> <c1> <1/f> <f> <g>
        FSUB(st0, st6);                             // <p-f> <c2(p²)> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st0);                             // <(p-f)²> <c2(p²)> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st4);                             // <c1*(p-f)²> <c2(p²)> <c2> <b> <c1> <1/f> <f> <g>
        FSUBP(st1, st0);                            // <c2(p²)-c1*(p-f)²> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st4);                             // <y> <c2> <b> <c1> <1/f> <f> <g>
        goto m0pl;


    m0c1: // y=c1(2p-f)
        FADD(st0, st0);                             // <2p> <c2> <b> <c1> <1/f> <f> <g>
        FSUB(st0, st5);                             // <2p-f> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st3);                             // <y> <c2> <b> <c1> <1/f> <f> <g>
        goto m0pl;

    m0c2: // y=c2(2p-f)
        FADD(st0, st0);                             // <2p> <c2> <b> <c1> <1/f> <f> <g>
        FSUB(st0, st5);                             // <2p-f> <c2> <b> <c1> <1/f> <f> <g>
        FMUL(st0, st1);                             // <y> <c2> <b> <c1> <1/f> <f> <g>

    m0pl:
        FADD(st0, st6);                             // <out> <c2> <b> <c1> <1/f> <f> <g>
        ADD(eax, esi);                              // ...............................n  c
        RCL(edx, 1);                                // ..............................nc  .
        TEST(byte(rbp[offsetof(syWOsc, ring)]), 1);
        if (JZ())
            goto m0noring;
        FMUL(dword(rdi[4 * ecx.data]));             // <out'> <c2> <b> <c1> <1/f> <f> <g>
        goto m0store;
    m0noring:
        FADD(dword(rdi[4 * ecx.data]));             // <out'> <c2> <b> <c1> <1/f> <f> <g>
    m0store:
        FSTP(dword(rdi[4 * ecx.data]));             // <c2> <b> <c1> <1/f> <f> <g>
        INC(ecx);
        if (JZ())
            goto m0end;
        goto m0loop;
    m0end:
        MOV(rbp[offsetof(syWOsc, cnt)], eax);
        FSTP(st0);                                  // <b> <c1> <1/f> <f> <g>
        FSTP(st0);                                  // <c1> <1/f> <f> <g>
        FSTP(st0);                                  // <1/f> <f> <g>
        FSTP(st0);                                  // <f> <g>
        FSTP(st0);                                  // <g>
        FSTP(st0);                                  // -
    off:
        POPAD();
        return;

        /*
        .m1casetab   dd syOscRender.m1c2,     ; ...
                     dd syOscRender.m1c1,     ; ..n , INVALID!
                     dd syOscRender.m1c212,   ; .c.
                     dd syOscRender.m1c21,    ; .cn
                     dd syOscRender.m1c12,    ; o..
                     dd syOscRender.m1c1,     ; o.n
                     dd syOscRender.m1c2,     ; oc. , INVALID!
                     dd syOscRender.m1c121    ; ocn
        */

    mode1: // pulse
        MOV(eax, rbp[offsetof(syWOsc, cnt)]);
        MOV(esi, rbp[offsetof(syWOsc, freq)]);

        // calc float helper values
        MOV(ebx, esi);
        SHR(ebx, 9);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);
        FLD(dword(rbp[offsetof(syWOsc, gain)]));    // <g>
        FLD(dword(rbp[offsetof(syWOsc, tmp)]));     // <f+1> <g>
        FLD1();                                     // <1> <f+1> <g>
        FSUBP(st1, st0);                            // <f> <g>
        FDIVR(st0, st1);                            // <gdf> <g>
        MOV(ebx, rbp[offsetof(syWOsc, brpt)]);
        SHR(ebx, 9);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);
        FLD(dword(rbp[offsetof(syWOsc, tmp)]));     // <b> <gdf> <g>
        FLD(st0);                                   // <b> <b> <gdf> <g>
        FADD(st0, st0);                             // <2b> <b> <gdf> <g>
        FLD(st0);                                   // <2b> <2b> <b> <gdf> <g>
        FLD1();                                     // <1> <2b> <2b> <b> <gdf> <g>
        FADD(st0, st0);                             // <2> <2b> <2b> <b> <gdf> <g>
        FSUB(st1, st0);                             // <2> <2b-2> <2b> <b> <gdf> <g>
        FADD(st0, st0);                             // <4> <2b-2> <2b> <b> <gdf> <g>
        FSUBP(st2, st0);                            // <2b-2> <2b-4> <b> <gdf> <g>
        FMUL(st0, st3);                             // <gdf(2b-2)> <2b-4> <b> <gdf> <g>
        FSUB(st0, st4);                             // <cc212> <2b-4> <b> <gdf> <g>
        FXCH(st1);                                  // <2b-4> <cc212> <b> <gdf> <g>
        FMUL(st0, st3);                             // <gdf(2b-4)> <cc212> <b> <gdf> <g>
        FADD(st0, st4);                             // <cc121> <cc212> <b> <gdf> <g>

        // calc state
        MOV(ebx, eax);
        SUB(ebx, esi);                              // ................................  c
        RCR(edx, 1);                                // c...............................  .
        CMP(ebx, rbp[offsetof(syWOsc, brpt)]);      // c...............................  n
        RCL(edx, 2);                                // ..............................nc  .


    m1loop:
        MOV(ebx, eax);
        SHR(ebx, 9);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);
        CMP(eax, rbp[offsetof(syWOsc, brpt)]);      // ..............................oc  n
        RCL(edx, 1);                                // .............................ocn  .
        AND(edx, 7);                                // 00000000000000000000000000000ocn
        //jmp  dword [cs: .m1casetab + 4*edx]
        if (edx.data == 0) goto m1c2; if (edx.data == 1) goto m1c1; if (edx.data == 2) goto m1c212; if (edx.data == 3) goto m1c21;
        if (edx.data == 4) goto m1c12; if (edx.data == 5) goto m1c1; if (edx.data == 6) goto m1c2; goto m1c121;

        // cases: on entry <cc121> <cc212> <b> <gdf> <g>, on exit: <out> <cc121> <cc212> <b> <gdf> <g>
    m1c21: // p2->p1 : 2(p-1)/f - 1
        FLD(dword(rbp[offsetof(syWOsc, tmp)]));     // <p> <cc121> <cc212> <b> <gdf> <g>
        FLD1();                                     // <1> <p> <cc121> <cc212> <b> <gdf> <g>
        FSUBP(st1, st0);                            // <p-1> <cc121> <cc212> <b> <gdf> <g>
        FADD(st0, st0);                             // <2(p-1)> <cc121> <cc212> <b> <gdf> <g>
        FMUL(st0, st4);                             // <gdf*2(p-1)> <cc121> <cc212> <b> <gdf> <g>
        FSUB(st0, st5);                             // <out> <cc121> <cc212> <b> <gdf> <g>
        goto m1pl;

    m1c12: // p1->p2 : 2(b-p)/f + 1
        FLD(st2);                                   // <b> <cc121> <cc212> <b> <gdf> <g>
        FSUB(dword(rbp[offsetof(syWOsc, tmp)]));    // <b-p> <cc121> <cc212> <b> <gdf> <g>
        FADD(st0, st0);                             // <2(b-p)> <cc121> <cc212> <b> <gdf> <g>
        FMUL(st0, st4);                             // <gdf*2(b-p)> <cc121> <cc212> <b> <gdf> <g>
        FADD(st0, st5);                             // <out> <cc121> <cc212> <b> <gdf> <g>
        goto m1pl;

    m1c121: // p1->p2->p1 : (2b-4)/f + 1
        FLD(st0);                                   // <out> <cc121> <cc212> <b> <gdf> <g>
        goto m1pl;

    m1c212: // p2->p1->p2 : (2b-2)/f - 1
        FLD(st1);                                   // <out> <cc121> <cc212> <b> <gdf> <g>
        goto m1pl;

    m1c1: // p1 : 1
        FLD(st4);                                   // <out> <cc121> <cc212> <b> <gdf> <g>
        goto m1pl;

    m1c2: // p2: -1
        FLD(st4);                                   // <out> <cc121> <cc212> <b> <gdf> <g>
        FCHS();

    m1pl:
        ADD(eax, esi);                              // ...............................n  c
        RCL(edx, 1);                                // ..............................nc  .
        TEST(byte(rbp[offsetof(syWOsc, ring)]), 1);
        if (JZ())
            goto m1noring;
        FMUL(dword(rdi[4 * ecx.data]));
        goto m1store;
    m1noring:
        FADD(dword(rdi[4 * ecx.data]));
    m1store:
        FSTP(dword(rdi[4 * ecx.data]));
        INC(ecx);
        if (JNZ())
            goto m1loop;
        MOV(rbp[offsetof(syWOsc, cnt)], eax);
        FSTP(st0);                                  // <cc212> <b> <gdf> <g>
        FSTP(st0);                                  // <b> <gdf> <g>
        FSTP(st0);                                  // <gdf> <g>
        FSTP(st0);                                  // <g>
        FSTP(st0);                                  // -
        POPAD();
        return;


    mode2: // sin
        MOV(eax, rbp[offsetof(syWOsc, cnt)]);
        MOV(edx, rbp[offsetof(syWOsc, freq)]);

        FLD(qword(m_fcsinx7));                      // <cx7>
        FLD(qword(m_fcsinx5));                      // <cx5> <cx7>
        FLD(qword(m_fcsinx3));                      // <cx3> <cx5> <cx7>

    m2loop1:
        MOV(ebx, eax);
        ADD(ebx, 0x40000000);
        MOV(esi, ebx);
        SAR(esi, 31);
        XOR(ebx, esi);
        SHR(ebx, 8);
        ADD(eax, edx);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);
        FLD(dword(rbp[offsetof(syWOsc, tmp)]));     // <x> <cx3> <cx5> <cx7>

        // scale/move to (-pi/4 .. pi/4)
        FMUL(dword(m_fcpi));                        // <x'> <cx3> <cx5> <cx7>
        FSUB(dword(m_fc1p5pi));                     // <x''> <cx3> <cx5> <cx7>

        // inline fastsin
        FLD(st0);                                   // <x> <x> <cx3> <cx5> <cx7>
        FMUL(st0, st1);                             // <x²> <x> <cx3> <cx5> <cx7>
        FLD(st4);                                   // <c> <x²> <x> <cx3> <cx5> <cx7>
        FMUL(st0, st1);                             // <cx²> <x²> <x> <cx3> <cx5> <cx7>
        FADD(st0, st4);                             // <b+cx²> <x²> <x> <cx3> <cx5> <cx7>
        FMUL(st0, st1);                             // <x²(b+cx²)> <x²> <x> <cx3> <cx5> <cx7>
        FADD(st0, st3);                             // <a+x²(b+cx²)> <x²> <x> <cx3> <cx5> <cx7>
        FMULP(st1, st0);                            // <x²(a+x²(b+cx²)> <x> <cx3> <cx5> <cx7>
        FLD1();                                     // <1> <x²(a+x²(b+cx²)> <x> <cx3> <cx5> <cx7>
        FADDP(st1, st0);                            // <1+x²(a+x²(b+cx²)> <x> <cx3> <cx5> <cx7>
        FMULP(st1, st0);                            // <x(1+x²(a+x²(b+cx²))> <cx3> <cx5> <cx7>

        FMUL(dword(rbp[offsetof(syWOsc, gain)]));   // <gain(y)> <cx3> <cx5> <cx7>
        TEST(byte(rbp[offsetof(syWOsc, ring)]), 1);
        if (JZ())
            goto m2noring;
        FMUL(dword(rdi[4 * ecx.data]));             // <out> <cx3> <cx5> <cx7>
        goto m2store;
    m2noring:
        FADD(dword(rdi[4 * ecx.data]));             // <out> <cx3> <cx5> <cx7>
    m2store:
        FSTP(dword(rdi[4 * ecx.data]));             // <cx3> <cx5> <cx7>
        INC(ecx);
        if (JNZ())
            goto m2loop1;
        MOV(rbp[offsetof(syWOsc, cnt)], eax);

        FSTP(st0);                                  // <cx5> <cx7>
        FSTP(st0);                                  // <cx7>
        FSTP(st0);                                  // -
        POPAD();
        return;


    mode3: // noise
        MOV(esi, rbp[offsetof(syWOsc, nseed)]);
        FLD(dword(rbp[offsetof(syWOsc, nfres)]));   // <r>
        FLD(dword(rbp[offsetof(syWOsc, nffrq)]));   // <f> <r>
        FLD(dword(rbp[offsetof(syWOsc, nfl)]));     // <l> <f> <r>
        FLD(dword(rbp[offsetof(syWOsc, nfb)]));     // <b> <l> <f> <r>
    m3loop1:
        IMUL(esi, icnoisemul);
        ADD(esi, icnoiseadd);
        MOV(eax, esi);
        SHR(eax, 9);
        OR(eax, 0x40000000);
        MOV(rbp[offsetof(syWOsc, tmp)], eax);
        FLD(dword(rbp[offsetof(syWOsc, tmp)]));     // <n> <b> <l> <f> <r>
        FLD1();                                     // <1> <n> <b> <l> <f> <r>
        FSUB(st1, st0);                             // <1> <n-1> <b> <l> <f> <r>
        FSUB(st1, st0);                             // <1> <n-2> <b> <l> <f> <r>
        FSUBP(st1, st0);                            // <n'> <b> <l> <f> <r>

        FLD(st1);                                   // <b> <n'> <b> <l> <f> <r>
        FMUL(st0, st4);                             // <b*f> <n'> <b> <l> <f> <r>
        FXCH(st1);                                  // <n'> <b*f> <b> <l> <f> <r>
        FLD(st2);                                   // <b> <n'> <b*f> <b> <l> <f> <r>
        FMUL(st0, st6);                             // <b*r> <n'> <b*f> <b> <l> <f> <r>
        FSUBP(st1, st0);                            // <n-(b*r)> <b*f> <b> <l> <f> <r>
        FXCH(st1);                                  // <b*f> <n-(b*r)> <b> <l> <f> <r>
        FADDP(st3, st0);                            // <n-(b*r)> <b> <l'> <f> <r>
        FSUB(st0, st2);                             // <h> <b> <l'> <f> <r>
        FLD(st0);                                   // <h> <h> <b> <l'> <f> <r>
        FMUL(st0, st4);                             // <h*f> <h> <b> <l'> <f> <r>
        FADDP(st2, st0);                            // <h> <b'> <l'> <f> <r>
        FLD(st2);                                   // <l'> <h> <b'> <l'> <f> <r>
        FADDP(st1, st0);                            // <l+h> <b'> <l'> <f> <r>
        FMUL(st0, st4);                             // <r(l+h)> <b'> <l'> <f> <r>
        FADD(st0, st1);                             // <r(l+h)+b> <b'> <l'> <f> <r>

        FMUL(dword(rbp[offsetof(syWOsc, gain)]));   // <out> <b'> <l'> <f> <r>
        TEST(byte(rbp[offsetof(syWOsc, ring)]), 1);
        if (JZ())
            goto m3noring;
        FMUL(dword(rdi[4 * ecx.data]));
        goto m3store;
    m3noring:
        FADD(dword(rdi[4 * ecx.data]));
    m3store:
        FSTP(dword(rdi[4 * ecx.data]));
        INC(ecx);
        if (JNZ())
            goto m3loop1;
        FSTP(dword(rbp[offsetof(syWOsc, nfb)]));    // <l> <f> <r>
        FSTP(dword(rbp[offsetof(syWOsc, nfl)]));    // <f> <r>
        FSTP(st0);                                  // <r>
        FSTP(st0);                                  // <->
        MOV(rbp[offsetof(syWOsc, nseed)], esi);
        POPAD();
        return;


    mode4: // fm sin
        MOV(eax, rbp[offsetof(syWOsc, cnt)]);
        MOV(edx, rbp[offsetof(syWOsc, freq)]);
    m4loop1:
        MOV(ebx, eax);
        FLD(dword(rdi[4 * ecx.data]));              // -1 .. 1
        FMUL(dword(m_fcfmmax));
        SHR(ebx, 9);;
        ADD(eax, edx);
        OR(ebx, 0x3f800000);
        MOV(rbp[offsetof(syWOsc, tmp)], ebx);
        FADD(dword(rbp[offsetof(syWOsc, tmp)]));
        FMUL(dword(m_fc2pi));
        fastsinrc();
        FMUL(dword(rbp[offsetof(syWOsc, gain)]));
        TEST(byte(rbp[offsetof(syWOsc, ring)]), 1);
        if (JZ())
            goto m4store;
        FMUL(dword(rdi[4 * ecx.data]));
    m4store:
        FSTP(dword(rdi[4 * ecx.data]));
        INC(ecx);
        if (JNZ())
            goto m4loop1;
        MOV(rbp[offsetof(syWOsc, cnt)], eax);
        POPAD();
        return;

        // CHAOS

    auxa: // copy
        MOV(rsi, this);
        rsi.data = rsi.data + offsetof(SYN, auxabuf);// lea esi,[esi + SYN.auxabuf]
    auxaloop:
        FLD(dword(rsi[0]));
        FADD(dword(rsi[4]));
        ADD(rsi, 8);
        FMUL(dword(rbp[offsetof(syWOsc, gain)]));
        FMUL(dword(m_fcgain));
        TEST(byte(rbp[offsetof(syWOsc, ring)]), 1);
        if (JZ())
            goto auxastore;
        FMUL(dword(rdi[4 * ecx.data]));
    auxastore:
        FSTP(dword(rdi[4 * ecx.data]));
        INC(ecx);
        if (JNZ())
            goto auxaloop;
        POPAD();
        return;

    auxb: // copy
        MOV(rsi, this);
        rsi.data = rsi.data + offsetof(SYN, auxabuf);// lea esi,[esi + SYN.auxabuf]
        goto auxaloop;
    }

    //#####################################################################################
    //  Envelope Generator
    //#####################################################################################


    // init
    // rbp: workspace
    void SynthAsm::syEnvInit()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(rbp[offsetof(syWEnv, state)], eax);     // reset state to "off"
        POPAD();
    }

    // set
    // rsi: values
    // rbp: workspace
    void SynthAsm::syEnvSet()
    {
        PUSHAD();

        // ar: 2^7 (128) bis 2^-4 (0.03, ca. 10secs bei 344frames/sec)
        FLD(dword(rsi[offsetof(syVEnv, ar)]));      // 0..127
        FMUL(dword(m_fcattackmul));                 // 0..-12
        FADD(dword(m_fcattackadd));                 // 7..-5
        pow2();
        FSTP(dword(rbp[offsetof(syWEnv, atd)]));

        // dcf: 0 (5msecs dank volramping) bis fast 1 (durchgehend)
        FLD(dword(rsi[offsetof(syVEnv, dr)]));      // 0..127
        FMUL(dword(m_fci128));                      // 0..~1
        FLD1();                                     // 1  0..~1
        FSUBRP(st1, st0);                           // 1..~0
        calcfreq2();
        FLD1();
        FSUBRP(st1, st0);
        FSTP(dword(rbp[offsetof(syWEnv, dcf)]));    // 0..~1

        // sul: 0..127 ist schon ok
        FLD(dword(rsi[offsetof(syVEnv, sl)]));      // 0..127
        FSTP(dword(rbp[offsetof(syWEnv, sul)]));    // 0..127

        // suf: 1/128 (15msecs bis weg) bis 128 (15msecs bis voll da)
        FLD(dword(rsi[offsetof(syVEnv, sr)]));      // 0..127
        FSUB(dword(m_fc64));                        // -64 .. 63
        FMUL(dword(m_fcsusmul));                    // -7 .. ~7
        pow2();
        FSTP(dword(rbp[offsetof(syWEnv, suf)]));    // 1/128 .. ~128

        // ref: 0 (5msecs dank volramping) bis fast 1 (durchgehend)
        FLD(dword(rsi[offsetof(syVEnv, rr)]));      // 0..127
        FMUL(dword(m_fci128));
        FLD1();
        FSUBRP(st1, st0);
        calcfreq2();
        FLD1();
        FSUBRP(st1, st0);
        FSTP(dword(rbp[offsetof(syWEnv, ref)]));    // 0..~1

        FLD(dword(rsi[offsetof(syVEnv, vol)]));
        FMUL(dword(m_fci128));
        FSTP(dword(rbp[offsetof(syWEnv, gain)]));

        POPAD();
    }

/*
syETTab  dd syEnvTick.state_off, syEnvTick.state_atk, syEnvTick.state_dec,
                  dd syEnvTick.state_sus, syEnvTick.state_rel
*/

    // tick
    // epb: workspace
    // ATTENTION: eax: gate
    void SynthAsm::syEnvTick()
    {
        PUSHAD();
        MOV(ebx, rbp[offsetof(syWEnv, state)]);
        // call[syETTab + 4 * ebx]
        if (ebx.data == 0) syEnvTick_state_off(); else if (ebx.data == 1) syEnvTick_state_atk(); else if (ebx.data == 2) syEnvTick_state_dec();
        else if (ebx.data == 3) syEnvTick_state_sus(); else syEnvTick_state_rel();
        FLD(dword(rbp[offsetof(syWEnv, val)]));
        FMUL(dword(rbp[offsetof(syWEnv, gain)]));
        FSTP(dword(rbp[offsetof(syWEnv, out)]));
        POPAD();
    }

    void SynthAsm::syEnvTick_state_off() // envelope off
    {
        OR(eax, eax);                               // gate on -> attack
        if (JZ())
            goto s0ok;
        INC(byte(rbp[offsetof(syWEnv, state)]));
        syEnvTick_state_atk();
        return;
    s0ok:
        FLDZ();
        FSTP(dword(rbp[offsetof(syWEnv, val)]));
    }

    void SynthAsm::syEnvTick_state_atk() // attack
    {
        OR(eax, eax);                               // gate off -> release
        if (JNZ())
            goto s1ok;
        MOV(byte(rbp[offsetof(syWEnv, state)]), 4);
        syEnvTick_state_rel();
        return;
    s1ok:
        FLD(dword(rbp[offsetof(syWEnv, val)]));
        FADD(dword(rbp[offsetof(syWEnv, atd)]));
        FSTP(dword(rbp[offsetof(syWEnv, val)]));
        MOV(ecx, 0x43000000);                       // 128.0
        CMP(ecx, rbp[offsetof(syWEnv, val)]);
        if (JA())
            return;                                 // val above -> decay
        MOV(rbp[offsetof(syWEnv, val)], ecx);
        INC(byte(rbp[offsetof(syWEnv, state)]));
    }

    void SynthAsm::syEnvTick_state_dec()
    {
        OR(eax, eax);                               // gate off -> release
        if (JNZ())
            goto s2ok;
        MOV(byte(rbp[offsetof(syWEnv, state)]), 4);
        syEnvTick_state_rel();
        return;
    s2ok:
        FLD(dword(rbp[offsetof(syWEnv, val)]));
        FMUL(dword(rbp[offsetof(syWEnv, dcf)]));
        FSTP(dword(rbp[offsetof(syWEnv, val)]));
        MOV(ecx, rbp[offsetof(syWEnv, sul)]);       // sustain level
        CMP(ecx, rbp[offsetof(syWEnv, val)]);
        if (JB())
        {
            //goto s4checkrunout;
            MOV(ecx, LOWEST);                       // val below->sustain
            CMP(ecx, rbp[offsetof(syWEnv, val)]);
            if (JB())
                return;
            XOR(ecx, ecx);
            MOV(rbp[offsetof(syWEnv, val)], ecx);
            MOV(rbp[offsetof(syWEnv, state)], ecx);
            return;
        }
        MOV(rbp[offsetof(syWEnv, val)], ecx);
        INC(byte(rbp[offsetof(syWEnv, state)]));
    }

    void SynthAsm::syEnvTick_state_sus()
    {
        OR(eax, eax);                               // gate off -> release
        if (JNZ())
            goto s3ok;
        INC(byte(rbp[offsetof(syWEnv, state)]));
        syEnvTick_state_rel();
        return;
    s3ok:
        FLD(dword(rbp[offsetof(syWEnv, val)]));
        FMUL(dword(rbp[offsetof(syWEnv, suf)]));
        FSTP(dword(rbp[offsetof(syWEnv, val)]));
        MOV(ecx, LOWEST);
        CMP(ecx, rbp[offsetof(syWEnv, val)]);
        if (JB())
            goto s3not2low;                         // val below -> off
        XOR(ecx, ecx);
        MOV(rbp[offsetof(syWEnv, val)], ecx);
        MOV(rbp[offsetof(syWEnv, state)], ecx);
        return;
    s3not2low:
        MOV(ecx, 0x43000000);                       // 128.0
        CMP(ecx, rbp[offsetof(syWEnv, val)]);
        if (JA())
            return;                                 // val above -> decay
        MOV(rbp[offsetof(syWEnv, val)], ecx);
    }

    void SynthAsm::syEnvTick_state_rel()
    {
        OR(eax, eax);                               // gate off -> release
        if (JZ())
            goto s4ok;
        MOV(byte(rbp[offsetof(syWEnv, state)]), 1);
        syEnvTick_state_atk();
        return;
    s4ok:
        FLD(dword(rbp[offsetof(syWEnv, val)]));
        FMUL(dword(rbp[offsetof(syWEnv, ref)]));
        FSTP(dword(rbp[offsetof(syWEnv, val)]));
    //s4checkrunout:
        MOV(ecx, LOWEST);
        CMP(ecx, rbp[offsetof(syWEnv, val)]);
        if (JB())
            return;
        XOR(ecx, ecx);
        MOV(rbp[offsetof(syWEnv, val)], ecx);
        MOV(rbp[offsetof(syWEnv, state)], ecx);
    }

    //#####################################################################################
    //  Filter
    //#####################################################################################

    void SynthAsm::syFltInit()
    {
        PUSHAD();
        XOR(eax, eax);
        rdi.data = rbp.data + offsetof(syWFlt, l);// lea edi, [ebp + offsetof(syWFlt, l]
        ecx.data = eax.data + 7;// lea ecx, [eax + 7]
        REP_STOSD();
        MOV(al, 4);
        MOV(rbp[offsetof(syWFlt, step)], eax);
        POPAD();
    }

    void SynthAsm::syFltSet()
    {
        PUSHAD();
        FLD(dword(rsi[offsetof(syVFlt, mode)]));
        FISTP(dword(rbp[offsetof(syWFlt, mode)]));

        FLD(dword(rsi[offsetof(syVFlt, cutoff)]));
        FMUL(dword(m_fci128));
        calcfreq();                                 // <f>
        FMUL(dword(m_SRfclinfreq));

        CMP(byte(rbp[offsetof(syWFlt, mode)]), 6);
        if (JGE())
            goto moogflt;

        FLD(dword(rsi[offsetof(syVFlt, reso)]));    // <r> <f>
        FMUL(dword(m_fci128));
        FLD1();                                     // <1> <r> <f>
        FSUBRP(st1, st0);                           // <1-res> <f>
        FSTP(dword(rbp[offsetof(syWFlt, res)]));    // <f>

        FSTP(dword(rbp[offsetof(syWFlt, cfreq)]));  // -

        POPAD();
        return;

    moogflt:
        // t = 1.0f - frequency;
        // p = frequency + 0.8f * frequency * t;
        // f = p + p - 1.0f;
        // q = resonance * (1.0f + 0.5f * t * (1.0f - t + 5.6f * t * t));
        FMUL(dword(m_fci2));                        // <0.5f>
        FMUL(dword(m_fci2));                        // <0.5f>
        FLD1();                                     // <1> <f>
        FSUB(st0, st1);                             // <t> <f>
        FLD(st0);                                   // <t> <t> <f>
        FMUL(st0, st2);                             // <f(t)> <t> <f>
        FMUL(dword(m_fc0p8));                       // <0.8f(t)> <t> <f>
        FADDP(st2, st0);                            // <t> <p>
        FXCH(st1);                                  // <p> <t>
        FST(dword(rbp[offsetof(syWFlt, moogp)]));
        FADD(st0, st0);                             // <2p> <t>
        FLD1();                                     // <1> <2p> <t>
        FSUBP(st1, st0);                            // <f> <t>
        FCHS();                                     // <-f> <t>
        FSTP(dword(rbp[offsetof(syWFlt, moogf)]));  // <t>
        FLD(st0);                                   // <t> <t>
        FMUL(st0, st0);                             // <t²> <t>
        FMUL(dword(m_fc5p6));                       // <5.6t²> <t>
        FLD1();                                     // <1> <5.6t²> <t>
        FSUB(st0, st2);                             // <1-t> <5.6t²> <t>
        FADDP(st1, st0);                            // <1-t+5.6t²> <t>
        FMULP(st1, st0);                            // <t(1-t+5.6t²)>
        FMUL(dword(m_fci2));                        // <0.5t(1-t+5.6t²)>
        FLD1();
        FADDP(st1, st0);                            // <1+0.5t(1-t+5.6t²)>
        FLD(dword(rsi[offsetof(syVFlt, reso)]));    // <r> <1+0.5t(1-t+5.6t²)>
        FMUL(dword(m_fci128));                      // <r'> <1+0.5t(1-t+5.6t²)>
        FMULP(st1, st0);                            // <q>
        FADD(st0, st0);
        FADD(st0, st0);
        FSTP(dword(rbp[offsetof(syWFlt, moogq)]));  // -

        POPAD();
    }

/*
syFRTab dd syFltRender.mode0, syFltRender.mode1, syFltRender.mode2
                dd syFltRender.mode3, syFltRender.mode4, syFltRender.mode5
                dd syFltRender.modemlo, syFltRender.modemhi
*/


// rbp: workspace
// rcx: count
// rsi: source
// rdi: dest
    void SynthAsm::syFltRender()
    {
        PUSHAD();

        MOVZX(eax, byte(rbp[offsetof(syWFlt, mode)]));
        AND(al, 7);
        CMP(al, 6);
        if (JGE())
            goto moogflt;

        FLD(dword(rbp[offsetof(syWFlt, res)]));     // <r>
        FLD(dword(rbp[offsetof(syWFlt, cfreq)]));   // <f> <r>
        FLD(dword(rbp[offsetof(syWFlt, l)]));       // <l> <f> <r>
        FLD(dword(rbp[offsetof(syWFlt, b)]));       // <b> <l> <f> <r>
        //call  [syFRTab + 4*eax]
        if (eax.data == 0) syFltRender_mode0(); else if(eax.data == 1) syFltRender_mode1(); else if (eax.data == 2) syFltRender_mode2();
        else if (eax.data == 3) syFltRender_mode3(); else if (eax.data == 4) syFltRender_mode4(); else if (eax.data == 5) syFltRender_mode5();
        else if (eax.data == 6) syFltRender_modemlo(); else syFltRender_modemhi();
        FSTP(dword(rbp[offsetof(syWFlt, b)]));      // <l''> <f> <r>
        FSTP(dword(rbp[offsetof(syWFlt, l)]));      // <f> <r>
        FSTP(st0);
        FSTP(st0);
        POPAD();
        return;

    moogflt:
        FLD(dword(rbp[offsetof(syWFlt, mb4)]));     // <b4>
        FLD(dword(rbp[offsetof(syWFlt, mb3)]));     // <b3> <b4>
        FLD(dword(rbp[offsetof(syWFlt, mb2)]));     // <b2> <b3> <b4>
        FLD(dword(rbp[offsetof(syWFlt, mb1)]));     // <b1> <b2> <b3> <b4>
        FLD(dword(rbp[offsetof(syWFlt, mb0)]));     // <b0> <b1> <b2> <b3> <b4>
        //call  [syFRTab + 4*eax]
        if (eax.data == 0) syFltRender_mode0(); else if(eax.data == 1) syFltRender_mode1(); else if (eax.data == 2) syFltRender_mode2();
        else if (eax.data == 3) syFltRender_mode3(); else if (eax.data == 4) syFltRender_mode4(); else if (eax.data == 5) syFltRender_mode5();
        else if (eax.data == 6) syFltRender_modemlo(); else syFltRender_modemhi();
        FSTP(dword(rbp[offsetof(syWFlt, mb0)]));    // <b1> <b2> <b3> <b4>
        FSTP(dword(rbp[offsetof(syWFlt, mb1)]));    // <b2> <b3> <b4>
        FSTP(dword(rbp[offsetof(syWFlt, mb2)]));    // <b3> <b4>
        FSTP(dword(rbp[offsetof(syWFlt, mb3)]));    // <b4>
        FSTP(dword(rbp[offsetof(syWFlt, mb4)]));    // -
        POPAD();
    }

    void SynthAsm::syFltRender_process()            // <b> <l> <f> <r>
    {
        FLD(dword(rsi[0]));                         // <in> <b> <l> <f> <r>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        FLD(st1);                                   // <b> <in> <b> <l> <f> <r>
        FLD(st2);                                   // <b> <b> <in> <b> <l> <f> <r>
        FMUL(st0, st5);                             // <b*f> <b> <in> <b> <l> <f> <r>
        FXCH(st1);                                  // <b> <b*f> <in> <b> <l> <f> <r>
        FMUL(st0, st6);                             // <b*r> <b*f> <in> <b> <l> <f> <r>
        FXCH(st1);                                  // <b*f> <b*r> <in> <b> <l> <f> <r>
        ADD(rsi, dword(rbp[offsetof(syWFlt, step)]));
        FSUB(dword(m_fcdcoffset));
        FADDP(st4, st0);                            // <b*r> <in> <b> <l'> <f> <r>
        FSUBR(st0, st1);                            // <in-b*r> <in> <b> <l'> <f> <r>
        FSUB(st0, st3);                             // <h> <in> <b> <l'> <f> <r>
        FMUL(st0, st4);                             // <f*h> <in> <b> <l'> <f> <r>
        FADDP(st2, st0);                            // <in> <b'> <l'> <f> <r>
        FLD(st1);                                   // <b'> <in> <b'> <l'> <f> <r>
        FLD(st2);                                   // <b'> <b'> <in> <b'> <l'> <f> <r>
        FMUL(st0, st5);                             // <b'*f> <b'> <in> <b'> <l'> <f> <r>
        FXCH(st1);                                  // <b'> <b'*f> <in> <b'> <l'> <f> <r>
        FMUL(st0, st6);                             // <b'*r> <b'*f> <in> <b'> <l'> <f> <r>
        FXCH(st1);                                  // <b'*f> <b'*r> <in> <b'> <l'> <f> <r>
        FADDP(st4, st0);                            // <b'*r> <in> <b'> <l''> <f> <r>
        FSUBP(st1, st0);                            // <in-b'*r> <b'> <l''> <f> <r>
        FSUB(st0, st2);                             // <h> <b'> <l''> <f> <r>
        FLD(st0);                                   // <h> <h> <b'> <l''> <f> <r>
        FMUL(st0, st4);                             // <h*f> <h> <b'> <l''> <f> <r>
        FADDP(st2, st0);                            // <h> <b''> <l''> <f> <r>
    }


    void SynthAsm::syFltRender_mode0() // bypass
    {
        CMP(rsi, rdi);
        if (JE())
            goto m0end;
        REP_MOVSD();
    m0end:
        return;
    }

    void SynthAsm::syFltRender_mode1() // low
    {
    mode1:
        syFltRender_process();                      // <h> <b''> <l''> <f> <r>
        FSTP(st0);                                  // <b''> <l''> <f> <r>
        FXCH(st1);                                  // <l''> <b''> <f> <r>
        FST(dword(rdi[0]));                         // -> l'' stored
        FXCH(st1);                                  // <b''> <l''> <f> <r>
        ADD(rdi, dword(rbp[offsetof(syWFlt, step)]));
        DEC(ecx);
        if (JNZ())
            goto mode1;
    }

    void SynthAsm::syFltRender_mode2() // band
    {
    mode2:
        syFltRender_process();                      // <h> <b''> <l''> <f> <r>
        FSTP(st0);                                  // <b''> <l''> <f> <r>
        FST(dword(rdi[0]));                         // -> b'' stored
        ADD(rdi, dword(rbp[offsetof(syWFlt, step)]));
        DEC(ecx);
        if (JNZ())
            goto mode2;
    }

    void SynthAsm::syFltRender_mode3() // high
    {
    mode3:
        syFltRender_process();                      // <h> <b''> <l''> <f> <r>
        FSTP(dword(rdi[0]));                        // <b''> <l''> <f> <r> -> h stored
        ADD(rdi, dword(rbp[offsetof(syWFlt, step)]));
        DEC(ecx);
        if (JNZ())
            goto mode3;
    }

    void SynthAsm::syFltRender_mode4() // notch
    {
    mode4:
        syFltRender_process();                      // <h> <b''> <l''> <f> <r>
        FADD(st2);                                  // <h+l> <b''> <l''> <f> <r>
        FSTP(dword(rdi[0]));                        // <b''> <l''> <f> <r> -> h+l'' stored
        ADD(rdi, dword(rbp[offsetof(syWFlt, step)]));
        DEC(ecx);
        if (JNZ())
            goto mode4;
    }

    void SynthAsm::syFltRender_mode5() // allpass
    {
    mode5:
        syFltRender_process();                      // <h> <b''> <l''> <f> <r>
        FADD(st1);                                  // <h+b> <b''> <l''> <f> <r>
        FADD(st2);                                  // <h+b+l> <b''> <l''> <f> <r>
        FSTP(dword(rdi[0]));                        // <b''> <l''> <f> <r> -> h+b''+l'' stored
        ADD(rdi, dword(rbp[offsetof(syWFlt, step)]));
        DEC(ecx);
        if (JNZ())
            goto mode5;
    }

    //  in -= q * b4;                          //feedback
    //  t1 = b1;  b1 = (in + b0) * p - b1 * f;
    //  t2 = b2;  b2 = (b1 + t1) * p - b2 * f;
    //  t3 = b3;  b3 = (b2 + t2) * p - b3 * f;
    //            b4 = (b3 + t3) * p - b4 * f;
    //  b4 = b4 - b4 * b4 * b4 * 0.166667f;    //clipping
    //  b0 = in;
    void SynthAsm::syFltRender_processmoog() // moog lowpass <b0> <b1> <b2> <b3> <b4>
    {
        FLD(dword(rsi[0]));                         // <in> <b0> <b1> <b2> <b3> <b4>
        FADD(dword(m_fcdcoffset));
        FLD(st5);                                   // <b4> <in> <b0> <b1> <b2> <b3> <b4>
        FMUL(dword(rbp[offsetof(syWFlt, moogq)]));  // <q*b4> <in> <b0> <b1> <b2> <b3> <b4>
        FSUBP(st1, st0);                            // <in'> <b0> <b1> <b2> <b3> <b4>
        FLD(st2);                                   // <b1> <in'> <b0> <t1:=b1> <b2> <b3> <b4>
        FMUL(dword(rbp[offsetof(syWFlt, moogf)]));  // <-f*b1> <in'> <b0> <t1> <b2> <b3> <b4>
        FXCH(st2);                                  // <b0> <in'> <-f*b1> <t1> <b2> <b3> <b4>
        FADDP(st1, st0);                            // <in'+b0> <-f*b1> <t1> <b2> <b3> <b4>
        FMUL(dword(rbp[offsetof(syWFlt, moogp)]));  // <p(in'+b0)> <-f*b1> <t1> <b2> <b3> <b4>
        FADDP(st1, st0);                            // <b1> <t1> <b2> <b3> <b4>
        FLD(st2);                                   // <b2> <b1> <t1> <t2:=b2> <b3> <b4>
        FMUL(dword(rbp[offsetof(syWFlt, moogf)]));  // <-f*b2> <b1> <t1> <t2> <b3> <b4>
        FXCH(st2);                                  // <t1> <b1> <-f*b2> <t2> <b3> <b4>
        FADD(st0, st1);                             // <t1+b1> <b1> <-f*b2> <t2> <b3> <b4>
        FMUL(dword(rbp[offsetof(syWFlt, moogp)]));  // <p(t1+b1)> <b1> <-f*b2> <t2> <b3> <b4>
        FADDP(st2, st0);                            // <b1> <b2> <t2> <b3> <b4>
        FLD(st3);                                   // <b3> <b1> <b2> <t2> <t3:=b3> <b4>
        FMUL(dword(rbp[offsetof(syWFlt, moogf)]));  // <-f*b3> <b1> <b2> <t2> <t3> <b4>
        FXCH(st3);                                  // <t2> <b1> <b2> <-f*b3> <t3> <b4>
        FADD(st0, st2);                             // <t2+b2> <b1> <b2> <-f*b3> <t3> <b4>
        FMUL(dword(rbp[offsetof(syWFlt, moogp)]));  // <p(t2+b2)> <b1> <b2> <-f*b3> <t3> <b4>
        FADDP(st3, st0);                            // <b1> <b2> <b3> <t3> <b4>
        FXCH(st4);                                  // <b4> <b2> <b3> <t3> <b1>
        FMUL(dword(rbp[offsetof(syWFlt, moogf)]));  // <-f*b4> <b2> <b3> <t3> <b1>
        FXCH(st3);                                  // <t3> <b2> <b3> <-f*b4> <b1>
        FADD(st0, st2);                             // <t3+b3> <b2> <b3> <-f*b4> <b1>
        FMUL(dword(rbp[offsetof(syWFlt, moogp)]));  // <p(t3+b3)> <b2> <b3> <-f*b4> <b1>
        FADDP(st3, st0);                            // <b2> <b3> <b4> <b1>
        FXCH(st2);                                  // <b4> <b3> <b2> <b1>
        FLD(st0);                                   // <b4> <b4> <b3> <b2> <b1>
        FMUL(st0, st0);                             // <b4²> <b4> <b3> <b2> <b1>
        FMUL(st0, st1);                             // <b4²> <b4> <b3> <b2> <b1>
        FMUL(dword(m_fci6));                        // <b4²/6> <b4> <b3> <b2> <b1>
        FSUBP(st1, st0);                            // <b4'> <b3> <b2> <b1>
        FSUB(dword(m_fcdcoffset));
        // todo: improve this
        FXCH(st1);                                  // <b3> <b4> <b2> <b1>
        FXCH(st2);                                  // <b2> <b4> <b3> <b1>
        FXCH(st1);                                  // <b4> <b2> <b3> <b1>
        FXCH(st3);                                  // <b1> <b2> <b3> <b4>
        FLD(dword(rsi[0]));                         // <b0:=in> <b1> <b2> <b3> <b4>
        FLD(st4);                                   // <out:=b4> <b1> <b2> <b3> <b4>
    }

    void SynthAsm::syFltRender_modemlo()
    {
    modemlo:
        syFltRender_processmoog();
        FSTP(st0);
        syFltRender_processmoog();
        FSTP(dword(rdi[0]));
        ADD(rsi, dword(rbp[offsetof(syWFlt, step)]));
        ADD(rdi, dword(rbp[offsetof(syWFlt, step)]));
        DEC(ecx);
        if (JNZ())
            goto modemlo;
    }

    void SynthAsm::syFltRender_modemhi()
    {
    modemhi:
        syFltRender_processmoog();
        FSTP(st0);
        syFltRender_processmoog();
        FSUBR(dword(rsi[0]));
        FSTP(dword(rdi[0]));
        ADD(rsi, dword(rbp[offsetof(syWFlt, step)]));
        ADD(rdi, dword(rbp[offsetof(syWFlt, step)]));
        DEC(ecx);
        if (JNZ())
            goto modemhi;
    }

    //#####################################################################################
    // Low Frequency Oscillator
    //#####################################################################################

    void SynthAsm::syLFOInit()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(rbp[offsetof(syWLFO, cntr)], eax);
        MOV(rbp[offsetof(syWLFO, last)], eax);
        RDTSC();
        MOV(rbp[offsetof(syWLFO, nseed)], eax);
        POPAD();
    }

    void SynthAsm::syLFOSet()
    {
        PUSHAD();

        FLD(dword(rsi[offsetof(syVLFO, mode)]));
        FISTP(dword(rbp[offsetof(syWLFO, mode)]));
        FLD(dword(rsi[offsetof(syVLFO, sync)]));
        FISTP(dword(rbp[offsetof(syWLFO, fs)]));
        FLD(dword(rsi[offsetof(syVLFO, egmode)]));
        FISTP(dword(rbp[offsetof(syWLFO, feg)]));

        FLD(dword(rsi[offsetof(syVLFO, rate)]));
        FMUL(dword(m_fci128));
        calcfreq();
        FMUL(dword(m_fc32bit));
        FMUL(dword(m_fci2));
        FISTP(dword(rbp[offsetof(syWLFO, freq)]));

        FLD(dword(rsi[offsetof(syVLFO, phase)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fc32bit));
        FISTP(dword(rbp[offsetof(syWLFO, cph)]));
        SHL(dword(rbp[offsetof(syWLFO, cph)]), 1);

        FLD(dword(rsi[offsetof(syVLFO, amp)]));
        FLD(dword(rsi[offsetof(syVLFO, pol)]));
        FISTP(dword(m_temp));
        MOV(eax, m_temp);
        FLD(st0);
        FCHS();
        FMUL(dword(m_fci2));
        CMP(al, 2);                                 // +/- polarity?
        if (JZ())
            goto isp2;
        FSUB(st0, st0);
    isp2:
        FSTP(dword(rbp[offsetof(syWLFO, dc)]));
        CMP(al, 1);                                 // +/- polarity?
        if (JNZ())
            goto isntp1;
        FCHS();
    isntp1:
        FSTP(dword(rbp[offsetof(syWLFO, gain)]));

        POPAD();
    }

    void SynthAsm::syLFOKeyOn()
    {
        PUSHAD();
        MOV(eax, rbp[offsetof(syWLFO, fs)]);
        OR(eax, eax);
        if (JZ())
            goto end;
        MOV(eax, rbp[offsetof(syWLFO, cph)]);
        MOV(rbp[offsetof(syWLFO, cntr)], eax);
        XOR(eax, eax);
        NOT(eax);
        MOV(rbp[offsetof(syWLFO, last)], eax);
    end:
        POPAD();
    }

/*
syLTTab  dd syLFOTick.mode0, syLFOTick.mode1, syLFOTick.mode2, syLFOTick.mode3
                  dd syLFOTick.mode4, syLFOTick.mode0, syLFOTick.mode0, syLFOTick.mode0
*/

    void SynthAsm::syLFOTick()
    {
        PUSHAD();
        MOV(eax, rbp[offsetof(syWLFO, cntr)]);
        MOV(edx, rbp[offsetof(syWLFO, mode)]);
        AND(dl, 7);
        //call    [syLTTab + 4*edx]
        if (edx.data == 0) syLFOTick_mode0(); else if (edx.data == 1) syLFOTick_mode1(); else if (edx.data == 2) syLFOTick_mode2(); else if (edx.data == 3) syLFOTick_mode3();
        else if(edx.data == 4) syLFOTick_mode4(); else syLFOTick_mode0();
        FMUL(dword(rbp[offsetof(syWLFO, gain)]));
        FADD(dword(rbp[offsetof(syWLFO, dc)]));
        FSTP(dword(rbp[offsetof(syWLFO, out)]));
        MOV(eax, rbp[offsetof(syWLFO, cntr)]);
        ADD(eax, rbp[offsetof(syWLFO, freq)]);
        if (JNC())
            goto isok;
        MOV(edx, rbp[offsetof(syWLFO, feg)]);
        OR(edx, edx);
        if (JZ())
            goto isok;
        XOR(eax, eax);
        NOT(eax);
    isok:
        MOV(rbp[offsetof(syWLFO, cntr)], eax);
        POPAD();
    }

    void SynthAsm::syLFOTick_mode0() // saw
    {
        SHR(eax, 9);
        OR(eax, 0x3f800000);
        MOV(m_temp, eax);
        FLD(dword(m_temp));                         // <1..2>
        FLD1();                                     // <1> <1..2>
        FSUBP(st1, st0);                            // <0..1>
    }

    void SynthAsm::syLFOTick_mode1() // tri
    {
        SHL(eax, 1);
        SBB(ebx, ebx);
        XOR(eax, ebx);
        syLFOTick_mode0();
    }

    void SynthAsm::syLFOTick_mode2() // pulse
    {
        SHL(eax, 1);
        SBB(eax, eax);
        syLFOTick_mode0();
    }

    void SynthAsm::syLFOTick_mode3() // sin
    {
        syLFOTick_mode0();
        FMUL(dword(m_fc2pi));                       // <0..2pi>
        fastsinrc();                                // <-1..1>
        FMUL(dword(m_fci2));                        // <-0.5..0.5>
        FADD(dword(m_fci2));                        // <0..1>
    }

    void SynthAsm::syLFOTick_mode4() // s&h
    {
        CMP(eax, rbp[offsetof(syWLFO, last)]);
        MOV(rbp[offsetof(syWLFO, last)], eax);
        if (JAE())
            goto nonew;
        MOV(eax, rbp[offsetof(syWLFO, nseed)]);
        IMUL(eax, icnoisemul);
        ADD(eax, icnoiseadd);
        MOV(rbp[offsetof(syWLFO, nseed)], eax);
    nonew:
        MOV(eax, rbp[offsetof(syWLFO, nseed)]);
        syLFOTick_mode0();
    }

    //#####################################################################################
    // INDUSTRIALGEL²T UND UNKAPUTTBARE ORGELN
    //  Das Verzerr- und Ver²b-Modul!
    //#####################################################################################

    // mode1:  overdrive ...  input gain, output gain, offset
    // mode2:  clip...        input gain, output gain, offset
    // mode3:  bitcrusher...  input gain, crush, xor
    // mode4:  decimator...   -,  resamplingfreq, -
    // mode5..9:  filters     -,  cutoff, reso

    void SynthAsm::syDistInit()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(rbp[offsetof(syWDist, dcount)], eax);
        MOV(rbp[offsetof(syWDist, dvall)], eax);
        MOV(rbp[offsetof(syWDist, dvalr)], eax);
        MOV(rbp[offsetof(syWDist, dlp1bl)], eax);
        MOV(rbp[offsetof(syWDist, dlp1br)], eax);
        MOV(rbp[offsetof(syWDist, dlp2bl)], eax);
        MOV(rbp[offsetof(syWDist, dlp2br)], eax);
        rbp.data += offsetof(syWDist, fw1);// lea ebp, rbp[offsetof(syWDist, fw1]
        syFltInit();
        rbp.data += offsetof(syWDist, fw2) - offsetof(syWDist, fw1); // lea ebp, rbp[offsetof(syWDist, fw2 - syWDist, fw1]
        syFltInit();
        POPAD();
    }

/*
syDSTab   dd syDistSet.mode0, syDistSet.mode1, syDistSet.mode2, syDistSet.mode3, syDistSet.mode4
                        dd syDistSet.modeF, syDistSet.modeF, syDistSet.modeF, syDistSet.modeF, syDistSet.modeF
                        dd syDistSet.modeF, syDistSet.modeF, syDistSet.modeF, syDistSet.modeF, syDistSet.modeF
                        dd syDistSet.modeF
*/

    void SynthAsm::syDistSet()
    {
        PUSHAD();
        FLD(dword(rsi[offsetof(syVDist, mode)]));
        FISTP(dword(rbp[offsetof(syWDist, mode)]));

        FLD(dword(rsi[offsetof(syVDist, ingain)]));
        FSUB(dword(m_fc32));
        FMUL(dword(m_fci16));
        pow2();
        FSTP(dword(rbp[offsetof(syWDist, gain1)]));

        FLD(dword(rsi[offsetof(syVDist, param1)]));
        MOV(eax, rbp[offsetof(syWDist, mode)]);
        AND(al, 15);
        //call    [syDSTab + 4*eax]
        if (eax.data == 0) syDistSet_mode0(); else if (eax.data == 1) syDistSet_mode1(); else if (eax.data == 2) syDistSet_mode2(); else if (eax.data == 3) syDistSet_mode3(); else if (eax.data == 4) syDistSet_mode4();
        else syDistSet_modeF();
        POPAD();
    }

    void SynthAsm::syDistSet_mode0()
    {
        FSTP(st0);
    }

    void SynthAsm::syDistSet_mode1() // overdrive
    {
        FMUL(dword(m_fci128));
        FLD(dword(rbp[offsetof(syWDist, gain1)]));
        FLD1();
        FPATAN();
        FDIVP(st1, st0);
        syDistSet_mode2b();
    }

    void SynthAsm::syDistSet_mode2() // clip
    {
        FMUL(dword(m_fci128));
        syDistSet_mode2b();
    }

    void SynthAsm::syDistSet_mode2b()
    {
        FSTP(dword(rbp[offsetof(syWDist, gain2)]));
        FLD(dword(rsi[offsetof(syVDist, param2)]));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci128));
        FADD(st0, st0);
        FMUL(dword(rbp[offsetof(syWDist, gain1)]));
        FSTP(dword(rbp[offsetof(syWDist, offs)]));
    }

    void SynthAsm::syDistSet_mode3() // bitcrusher
    {
        FMUL(dword(m_fc256));                       // 0 .. 32xxx
        FLD1();
        FADDP(st1, st0);                            // 1 .. 32xxx
        FLD(dword(m_fc32768));                      // 32768 x
        FXCH(st1);                                  // x 32768
        FDIV(st1, st0);                             // x 32768/x
        FISTP(dword(rbp[offsetof(syWDist, crush2)]));
        FMUL(dword(rbp[offsetof(syWDist, gain1)]));
        FSTP(dword(rbp[offsetof(syWDist, crush1)]));
        FLD(dword(rsi[offsetof(syVDist, param2)]));
        FISTP(dword(m_temp));
        MOV(eax, m_temp);
        SHL(eax, 9);
        MOV(rbp[offsetof(syWDist, crxor)], eax);
    }

    void SynthAsm::syDistSet_mode4() // decimator
    {
        FMUL(dword(m_fci128));
        calcfreq();
        FMUL(dword(m_fc32bit));
        FISTP(dword(rbp[offsetof(syWDist, dfreq)]));
        SHL(dword(rbp[offsetof(syWDist, dfreq)]), 1);
        FLD(dword(rsi[offsetof(syVDist, ingain)]));
        FMUL(dword(m_fci127));
        FMUL(st0, st0);
        FSTP(dword(rbp[offsetof(syWDist, dlp1c)]));
        FLD(dword(rsi[offsetof(syVDist, param2)]));
        FMUL(dword(m_fci127));
        FMUL(st0, st0);
        FSTP(dword(rbp[offsetof(syWDist, dlp2c)]));
    }

    void SynthAsm::syDistSet_modeF() // filters
    {
        FSTP(dword(m_temp + offsetof(syVFlt, cutoff)));
        FLD(dword(rsi[offsetof(syVDist, param2)]));
        FSTP(dword(m_temp + offsetof(syVFlt, reso)));
        MOV(eax, rbp[offsetof(syWDist, mode)]);
        eax.data -= 4;// lea     eax, [eax-4]
        MOV(m_temp + offsetof(syVFlt, mode), eax);
        FILD(dword(m_temp + offsetof(syVFlt, mode)));
        FSTP(dword(m_temp + offsetof(syVFlt, mode)));
        rsi.data = reinterpret_cast<uint64_t>(m_temp); // lea esi, [temp]
        rbp.data += offsetof(syWDist, fw1); // lea ebp, rbp[offsetof(syWDist.fw1]
        syFltSet();
        rbp.data += offsetof(syWDist, fw2) - offsetof(syWDist, fw1); // lea ebp, rbp[offsetof(syWDist, fw2 - syWDist, fw1]
        syFltSet();
    }

/*
syDRMTab  dd syDistRenderMono.mode0, syDistRenderMono.mode1, syDistRenderMono.mode2, syDistRenderMono.mode3
                        dd syDistRenderMono.mode4, syDistRenderMono.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF
            dd syDistRenderMono.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF
            dd syDistRenderMono.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF
*/

    // esi : sourceptr
    // edi : destptr
    // ecx : size
    void SynthAsm::syDistRenderMono()
    {
        PUSHAD();
        MOV(edx, rbp[offsetof(syWDist, mode)]);
        //call    [syDRMTab + 4*edx]
        if (edx.data == 0) syDistRenderMono_mode0(); else if (edx.data == 1) syDistRenderMono_mode1(); else if (edx.data == 2) syDistRenderMono_mode2(); else if (edx.data == 3) syDistRenderMono_mode3();
        else if (edx.data == 4) syDistRenderMono_mode4(); else syDistRenderMono_modeF();
        POPAD();
    }

    void SynthAsm::syDistRenderMono_mode0() // bypass
    {
        CMP(rsi, rdi);
        if (JE())
            goto m0end;
        REP_MOVSD();
    m0end:
        return;
    }

    void SynthAsm::syDistRenderMono_mode1() // overdrive
    {
        FLD1();                                     // <1>
        FCHS();                                     // <-1>
        XOR(ebx, ebx);
        MOV(bl, 8);
    m1loop:
        FLD(dword(rsi[0]));
        rsi.data += 4; // lea esi, [esi+4]
        FMUL(dword(rbp[offsetof(syWDist, gain1)]));
        FADD(dword(rbp[offsetof(syWDist, offs)]));
        FST(dword(m_temp));
        MOV(ax, m_temp + 2);

        fastatan();

        FMUL(dword(rbp[offsetof(syWDist, gain2)]));
        FSTP(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi+4]
        DEC(ecx);
        if (JNZ())
            goto m1loop;
        FSTP(st0);                                  // -
    }

    void SynthAsm::syDistRenderMono_mode2() // clip
    {
    mode2:
        FLD(dword(rsi[0]));
        rsi.data += 4; // lea esi, [esi+4]
        FMUL(dword(rbp[offsetof(syWDist, gain1)]));
        FADD(dword(rbp[offsetof(syWDist, offs)]));

        FLD1();
        FCOMI(st0, st1);
        FXCH(st1);
        FCMOVB(st0, st1);
        FXCH(st1);
        FCHS();
        FCOMI(st0, st1);
        FCMOVB(st0, st1);
        FXCH(st1);
        FSTP(st0);

        FMUL(dword(rbp[offsetof(syWDist, gain2)]));
        FSTP(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi+4]
        DEC(ecx);
        if (JNZ())
            goto mode2;
    }

    void SynthAsm::syDistRenderMono_mode3() // bitcrusher
    {
        MOV(ebx, 0x7fff);
        MOV(edx, static_cast<uint32_t>(-0x7fff));
    m3loop:
        FLD(dword(rsi[0]));
        rsi.data += 4; // lea esi, [esi+4]
        FMUL(dword(rbp[offsetof(syWDist, crush1)]));
        FISTP(dword(m_temp));
        MOV(eax, m_temp);
        IMUL(eax, rbp[offsetof(syWDist, crush2)]);
        CMP(ebx, eax);
        CMOVLE(eax, ebx);
        CMP(edx, eax);
        CMOVGE(eax, edx);
        XOR(eax, rbp[offsetof(syWDist, crxor)]);
        MOV(m_temp, eax);
        FILD(dword(m_temp));
        FMUL(dword(m_fci32768));
        FSTP(dword(rdi[0]));
        edi.data += 4; // lea edi, [edi+4]
        DEC(ecx);
        if (JNZ())
            goto m3loop;
    }


    void SynthAsm::syDistRenderMono_mode4() // decimator
    {
        MOV(eax, rbp[offsetof(syWDist, dvall)]);
        MOV(edx, rbp[offsetof(syWDist, dfreq)]);
        MOV(ebx, rbp[offsetof(syWDist, dcount)]);
    m4loop:
        ADD(ebx, edx);
        CMOVC(eax, rsi[0]);
        rsi.data += 4;//add esi, byte 4
        STOSD();
        DEC(ecx);
        if (JNZ())
            goto m4loop;
        MOV(rbp[offsetof(syWDist, dcount)], ebx);
        MOV(rbp[offsetof(syWDist, dvall)], eax);
    }

    void SynthAsm::syDistRenderMono_modeF() // filters
    {
        rbp.data += offsetof(syWDist, fw1);// lea ebp, ebp + syWDist.fw1
        syFltRender();
    }

/*
syDRSTab  dd syDistRenderMono.mode0, syDistRenderMono.mode1, syDistRenderMono.mode2, syDistRenderMono.mode3
                        dd syDistRenderStereo.mode4, syDistRenderStereo.modeF, syDistRenderStereo.modeF, syDistRenderStereo.modeF
            dd syDistRenderStereo.modeF, syDistRenderStereo.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF
            dd syDistRenderMono.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF, syDistRenderMono.modeF
*/

    void SynthAsm::syDistRenderStereo()
    {
        PUSHAD();
        SHL(ecx, 1);
        MOV(edx, rbp[offsetof(syWDist, mode)]);
        //call    [syDRSTab + 4*edx]
        if (edx.data == 0) syDistRenderMono_mode0(); else if (edx.data == 1) syDistRenderMono_mode1(); else if (edx.data == 2) syDistRenderMono_mode2(); else if (edx.data == 3) syDistRenderMono_mode3();
        else if (edx.data == 4) syDistRenderStereo_mode4(); else if (edx.data == 5) syDistRenderStereo_modeF(); else if (edx.data == 6) syDistRenderStereo_modeF(); else if (edx.data == 7) syDistRenderStereo_modeF();
        else if (edx.data == 8) syDistRenderStereo_modeF(); else if (edx.data == 9) syDistRenderStereo_modeF(); else syDistRenderMono_modeF();
        POPAD();
    }

    void SynthAsm::syDistRenderStereo_mode4() // decimator
    {
        SHR(ecx, 1);
        MOV(eax, rbp[offsetof(syWDist, dvall)]);
        MOV(edx, rbp[offsetof(syWDist, dvalr)]);
        MOV(ebx, rbp[offsetof(syWDist, dcount)]);
    m4loop:
        ADD(ebx, rbp[offsetof(syWDist, dfreq)]);
        CMOVC(eax, rsi[0]);
        CMOVC(edx, rsi[4]);
        esi.data += 8; // add esi, byte 8
        STOSD();
        XCHG(eax, edx);
        STOSD();
        XCHG(eax, edx);
        DEC(ecx);
        if (JNZ())
            goto m4loop;
        MOV(rbp[offsetof(syWDist, dcount)], ebx);
        MOV(rbp[offsetof(syWDist, dvall)], eax);
        MOV(rbp[offsetof(syWDist, dvalr)], edx);
    }

    void SynthAsm::syDistRenderStereo_modeF() // filters
    {
        SHR(ecx, 1);
        XOR(eax, eax);
        MOV(al, 8);
        MOV(rbp[offsetof(syWDist, fw1) + offsetof(syWFlt, step)], eax);
        MOV(rbp[offsetof(syWDist, fw2) + offsetof(syWFlt, step)], eax);
        rbp.data += offsetof(syWDist, fw1); // lea ebp, [ebp + syWDist.fw1]
        syFltRender();
        rbp.data += offsetof(syWDist, fw2) - offsetof(syWDist, fw1); // lea ebp, [ebp + syWDist.fw2 - syWDist.fw1]
        rsi.data += 4;// lea esi, [esi + 4]
        rdi.data += 4;// lea edi, [edi + 4]
        syFltRender();
    }

    //#####################################################################################
    // DC filter
    //#####################################################################################

    // rbp: workspace
    void SynthAsm::syDCFInit()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(rdi, rbp);
        MOV(ecx, sizeof(syWDCF) / 4);
        REP_STOSD();
        POPAD();
    }

    // rbp: workspace
    // rsi: source
    // rdi: dest
    // ecx: count
    // y(n) = x(n) - x(n-1) + R * y(n-1)
    void SynthAsm::syDCFRenderMono()
    {
        PUSHAD();
        FLD(dword(m_fcdcoffset));                   // <dx>
        FLD(dword(m_SRfcdcfilter));                 // <R> <dc>
        FLD(dword(rbp[offsetof(syWDCF, xm1l)]));    // <xm1> <R> <dc>
        FLD(dword(rbp[offsetof(syWDCF, ym1l)]));    // <ym1> <xm1> <R> <dc>
    floop:
        FMUL(st0, st2);                             // <R*ym1> <xm1> <R> <dc>
        FSUBRP(st1, st0);                           // <R*ym1-xm1> <R> <dc>
        FLD(dword(rsi[0]));                         // <x> <R*ym1-xm1> <R>   <dc>
        rsi.data += 4; // lea esi, [esi+4]
        FXCH(st1);                                  // <R*ym1-xm1> <x> <R>   <dc>
        FADD(st0, st1);                             // <y> <x> <R> <dc>
        FADD(st0, st3);                             // <y+dc> <x> <R> <dc>
        FSUB(st0, st3);                             // <y> <x> <R> <dc>
        FST(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi+4]
        DEC(ecx);
        if (JNZ())
            goto floop;
        FSTP(dword(rbp[offsetof(syWDCF, ym1l)]));   // <xm1> <R> <dc>
        FSTP(dword(rbp[offsetof(syWDCF, xm1l)]));   // <R> <dc>
        FSTP(st0);                                  // <dc>
        FSTP(st0);                                  // -
        POPAD();
    }

    // rbp: workspace
    // rsi: source
    // rdi: dest
    // ecx: count
    // y(n) = x(n) - x(n-1) + R * y(n-1)
    void SynthAsm::syDCFRenderStereo()
    {
        PUSHAD();
        FLD(dword(m_fcdcoffset));                   // <dc>
        FLD(dword(m_SRfcdcfilter));                 // <R> <dc>
        FLD(dword(rbp[offsetof(syWDCF, xm1r)]));    // <xm1r> <R> <dc>
        FLD(dword(rbp[offsetof(syWDCF, xm1l)]));    // <xm1l> <xm1r> <R> <dc>
        FLD(dword(rbp[offsetof(syWDCF, ym1r)]));    // <ym1r> <xm1l> <xm1r> <R> <dc>
        FLD(dword(rbp[offsetof(syWDCF, ym1l)]));    // <ym1l> <ym1r> <xm1l> <xm1r> <R> <dc>
    floop:
        FMUL(st0, st4);                             // <R*ym1l> <ym1r> <xm1l> <xm1r> <R> <dc>
        FSUBRP(st2, st0);                           // <ym1r> <R*ym1l-xm1l> <xm1r> <R> <dc>
        FMUL(st0, st3);                             // <R*ym1r> <R*ym1l-xm1l> <xm1r> <R> <dc>
        FSUBRP(st2, st0);                           // <R*ym1l-xm1l> <R*ym1r-xm1r> <R> <dc>
        FLD(dword(rsi[0]));                         // <xl> <R*ym1l-xm1l> <R*ym1r-xm1r> <R> <dc>
        FXCH(st1);                                  // <R*ym1l-xm1l> <xl> <R*ym1r-xm1r> <R> <dc>
        FADD(st0, st1);                             // <yl> <xl> <R*ym1r-xm1r> <R> <dc>
        FADD(st0, st4);                             // <yl+dc> <xl> <R*ym1r-xm1r> <R> <dc>
        FSUB(st0, st4);                             // <yl> <xl> <R*ym1r-xm1r> <R> <dc>
        FST(dword(rdi[0]));
        FLD(dword(rsi[4]));                         // <xr> <yl> <xl> <R*ym1r-xm1r> <R> <dc>
        FXCH(st3);                                  // <R*ym1r-xm1r> <yl> <xl> <xr> <R> <dc>
        FADD(st0, st3);                             // <yr> <yl> <xl> <xr> <R> <dc>
        FADD(st0, st5);                             // <yr+dc> <yl> <xl> <xr> <R> <dc>
        FSUB(st0, st5);                             // <yr> <yl> <xl> <xr> <R> <dc>
        FST(dword(rdi[4]));
        FXCH(st1);                                  // <yl> <yr> <xl> <xr> <R> <dc>
        rsi.data += 8; // lea esi, [esi+8]
        rdi.data += 8; // lea edi, [edi+8]
        DEC(ecx);
        if (JNZ())
            goto floop;
        FSTP(dword(rbp[offsetof(syWDCF, ym1l)]));   // <ym1r> <xm1l> <xm1r> <R> <dc>
        FSTP(dword(rbp[offsetof(syWDCF, ym1r)]));   // <xm1l> <xm1r> <R> <dc>
        FSTP(dword(rbp[offsetof(syWDCF, xm1l)]));   // <xm1r> <R> <dc>
        FSTP(dword(rbp[offsetof(syWDCF, xm1r)]));   // <R> <dc>
        FSTP(st0);                                  // <dc>
        FSTP(st0);                                  // -
        POPAD();
    }

    //#####################################################################################
    // V2 - Voice
    //#####################################################################################

    // rbp: workspace
    void SynthAsm::syV2Init()
    {
        PUSHAD();
        rbp.data += offsetof(syWV2, osc1) - 0; // lea ebp, [ebp + syWV2.osc1 - 0]
        XOR(ecx, ecx);
        syOscInit();
        rbp.data += offsetof(syWV2, osc2) - offsetof(syWV2, osc1); // lea ebp, [ebp + syWV2.osc2 - syWV2.osc1]
        INC(ecx);
        syOscInit();
        rbp.data += offsetof(syWV2, osc3) - offsetof(syWV2, osc2); // lea ebp, [ebp + syWV2.osc3 - syWV2.osc2]
        INC(ecx);
        syOscInit();
        rbp.data += offsetof(syWV2, aenv) - offsetof(syWV2, osc3); // lea ebp, [ebp + syWV2.aenv - syWV2.osc3]
        syEnvInit();
        rbp.data += offsetof(syWV2, env2) - offsetof(syWV2, aenv); // lea ebp, [ebp + syWV2.env2 - syWV2.aenv]
        syEnvInit();
        rbp.data += offsetof(syWV2, vcf1) - offsetof(syWV2, env2); // lea ebp, [ebp + syWV2.vcf1 - syWV2.env2]
        syFltInit();
        rbp.data += offsetof(syWV2, vcf2) - offsetof(syWV2, vcf1); // lea ebp, [ebp + syWV2.vcf2 - syWV2.vcf1]
        syFltInit();
        rbp.data += offsetof(syWV2, lfo1) - offsetof(syWV2, vcf2); // lea ebp, [ebp + syWV2.lfo1 - syWV2.vcf2]
        syLFOInit();
        rbp.data += offsetof(syWV2, lfo2) - offsetof(syWV2, lfo1); // lea ebp, [ebp + syWV2.lfo2 - syWV2.lfo1]
        syLFOInit();
        rbp.data += offsetof(syWV2, dist) - offsetof(syWV2, lfo2); // lea ebp, [ebp + syWV2.dist - syWV2.lfo2]
        syDistInit();
        rbp.data += offsetof(syWV2, dcf) - offsetof(syWV2, dist); // lea ebp, [ebp + syWV2.dcf - syWV2.dist]
        syDCFInit();
        POPAD();
    }

    // tick
    // rbp: workspace
    void SynthAsm::syV2Tick()
    {
        PUSHAD();

        // 1. EGs
        MOV(eax, rbp[offsetof(syWV2, gate)]);
        rbp.data += offsetof(syWV2, aenv) - 0; // lea ebp, [ebp + syWV2.aenv - 0]
        syEnvTick();
        rbp.data += offsetof(syWV2, env2) - offsetof(syWV2, aenv); // lea ebp, [ebp + syWV2.env2 - syWV2.aenv]
        syEnvTick();

        // 2. LFOs
        rbp.data += offsetof(syWV2, lfo1) - offsetof(syWV2, env2); // lea ebp, [ebp + syWV2.lfo1 - syWV2.env2]
        syLFOTick();
        rbp.data += offsetof(syWV2, lfo2) - offsetof(syWV2, lfo1);// lea ebp, [ebp + syWV2.lfo2 - syWV2.lfo1]
        syLFOTick();

        rbp.data += 0 - offsetof(syWV2, lfo2); // lea ebp, [ebp + 0 - syWV2.lfo2]

        // 3. Volume Ramping
        FLD(dword(rbp[offsetof(syWV2, aenv) + offsetof(syWEnv, out)]));
        FMUL(dword(m_fci128));
        FSUB(dword(rbp[offsetof(syWV2, curvol)]));
        FMUL(dword(m_SRfciframe));
        FSTP(dword(rbp[offsetof(syWV2, volramp)]));

        POPAD();
    }


    // render
    // edi: dest buf
    // ecx: # of samples
    // ebp: workspace
    void SynthAsm::syV2Render()
    {
        PUSHAD();

        MOV(rbx, &m_this);
        ADD(rbx, offsetof(SYN, vcebuf));

        // clear buffer
        PUSH(ecx);
        MOV(rdi, rbx);
        XOR(eax, eax);
        REP_STOSD();
        POP(ecx);

        MOV(rdi, rbx);
        rbp.data += -0 + offsetof(syWV2, osc1); // lea ebp, [ebp - 0 + syWV2.osc1]
        syOscRender();
        rbp.data += offsetof(syWV2, osc2) - offsetof(syWV2, osc1); // lea ebp, [ebp - syWV2.osc1 + syWV2.osc2]
        syOscRender();
        rbp.data += offsetof(syWV2, osc3) - offsetof(syWV2, osc2); // lea ebp, [ebp - syWV2.osc2 + syWV2.osc3]
        syOscRender();
        rbp.data += 0 - offsetof(syWV2, osc3); // lea ebp, [ebp - syWV2.osc3 + 0]

        // Filter + Routing
        // wenn parallel -> erst filter 2 in buf2 rendern
        MOV(edx, rbp[offsetof(syWV2, fmode)]);
        CMP(dl, 2);                                 // parallel?
        if (JNE())
            goto nopar1;
        rbp.data += offsetof(syWV2, vcf2) - 0; // lea ebp, [ebp - 0 + syWV2.vcf2]
        MOV(rsi, rbx);
        rdi.data = rbx.data + offsetof(SYN, vcebuf2) - offsetof(SYN, vcebuf); // lea edi, [ebx + SYN.vcebuf2-SYN.vcebuf]
        syFltRender();
        rbp.data += 0 - offsetof(syWV2, vcf2); // lea ebp, [ebp - syWV2.vcf2 + 0]
    nopar1:
        // dann auf jeden fall filter 1 rendern
        rbp.data += offsetof(syWV2, vcf1) - 0; // lea ebp, [ebp - 0 + syWV2.vcf1]
        MOV(rsi, rbx);
        MOV(rdi, rbx);
        syFltRender();
        rbp.data += 0 - offsetof(syWV2, vcf1); // lea ebp, [ebp - syWV2.vcf1 + 0]
        // dann fallunterscheidung...
        OR(dl, dl);                                 // single?
        if (JZ())
            goto fltend;
        CMP(dl, 2);                                 // parallel?
        if (JNZ())
            goto nopar2;
        PUSH(ecx);                                  // ja -> buf2 auf buf adden
        rsi.data = rbx.data + offsetof(SYN, vcebuf2) - offsetof(SYN, vcebuf); // lea esi, [ebx + SYN.vcebuf2-SYN.vcebuf]
        MOV(rdi, rbx);
        FLD(dword(rbp[offsetof(syWV2, f1gain)]));   // <g1>
        FLD(dword(rbp[offsetof(syWV2, f2gain)]));   // <g2> <g1>
    parloop:
        FLD(dword(rsi[0]));                         // <v2> <g2> <g1>
        FMUL(st0, st1);                             // <v2'> <g2> <g1>
        rsi.data += 4; // add esi, byte 4
        FLD(dword(rdi[0]));                         // <v1> <v2'> <g2> <g1>
        FMUL(st0, st3);                             // <v1'> <v2'> <g2> <g1>
        FADDP(st1, st0);                            // <out> <g2> <g1>
        FSTP(dword(rdi[0]));                        // <g2> <g1>
        rdi.data += 4; // add edi, byte 4
        DEC(ecx);
        if (JNZ())
            goto parloop;
        FSTP(st0);                                  // <g1>
        FSTP(st0);                                  // -
        POP(ecx);
        goto fltend;
    nopar2:
        // ... also seriell ... filter 2 dr²berrechnen
        rbp.data += offsetof(syWV2, vcf2) - 0; // lea ebp, [ebp - 0 + syWV2.vcf2]
        MOV(rsi, rbx);
        MOV(rdi, rbx);
        syFltRender();
        rbp.data += 0 - offsetof(syWV2, vcf2); // lea ebp, [ebp - syWV2.vcf2 + 0]

    fltend:

        // distortion
        MOV(rsi, rbx);
        MOV(rdi, rbx);
        rbp.data += offsetof(syWV2, dist) - 0; // lea ebp, [ebp - 0 + syWV2.dist]
        syDistRenderMono();

        // dc filter
        rbp.data += offsetof(syWV2, dcf) - offsetof(syWV2, dist); // lea ebp, [ebp + syWV2.dcf - syWV2.dist]
        syDCFRenderMono();

        rbp.data += 0 - offsetof(syWV2, dcf); // lea ebp, [ebp - syWV2.dcf + 0]

        // vcebuf (mono) nach chanbuf(stereo) kopieren
        rdi.data = rbx.data + offsetof(SYN, chanbuf) - offsetof(SYN, vcebuf); // lea edi, [ebx + SYN.chanbuf-SYN.vcebuf]
        MOV(rsi, rbx);
        FLD(dword(rbp[offsetof(syWV2, curvol)]));   // cv
    copyloop1:
        FLD(dword(rsi[0]));                         // out cv
        FMUL(st1);                                  // out' cv
        FXCH(st1);                                  // cv out'
        FADD(dword(rbp[offsetof(syWV2, volramp)])); // cv' out'
        FXCH(st1);                                  // out' cv'
        FLD(st0);                                   // out out cv
        FMUL(dword(rbp[offsetof(syWV2, lvol)]));    // outl out cv
        FXCH(st1);                                  // out outl cv
        FMUL(dword(rbp[offsetof(syWV2, rvol)]));    // outr outl cv
        FXCH(st1);                                  // outl outr cv
#ifdef FIXDENORMALS
        FADD(dword(m_fcdcoffset));
        FXCH(st1);
        FADD(dword(m_fcdcoffset));
        FXCH(st1);
#endif
        FADD(dword(rdi[0]));                        // l outr cv
        FXCH(st1);                                  // outr l cv
        FADD(dword(rdi[4]));                        // r l cv
        FXCH(st1);                                  // l r cv
        FSTP(dword(rdi[0]));                        // r cv
        FSTP(dword(rdi[4]));                        // cv
        rsi.data += 4; // add esi, 4
        rdi.data += 8; // add edi, 8
        DEC(ecx);
        if (JNZ())
            goto copyloop1;
        FSTP(dword(rbp[offsetof(syWV2, curvol)]));  // -

        POPAD();
    }



    // set
    // esi: values
    // ebp: workspace
    void SynthAsm::syV2Set()
    {
        PUSHAD();

        FLD(dword(rsi[offsetof(syVV2, transp)]));
        FSUB(dword(m_fc64));
        FST(dword(rbp[offsetof(syWV2, xpose)]));

        FIADD(dword(rbp[offsetof(syWV2, note)]));
        FST(dword(rbp[offsetof(syWV2, osc1) + offsetof(syWOsc, note)]));
        FST(dword(rbp[offsetof(syWV2, osc2) + offsetof(syWOsc, note)]));
        FSTP(dword(rbp[offsetof(syWV2, osc3) + offsetof(syWOsc, note)]));

        FLD(dword(rsi[offsetof(syVV2, routing)]));
        FISTP(dword(rbp[offsetof(syWV2, fmode)]));

        FLD(dword(rsi[offsetof(syVV2, oscsync)]));
        FISTP(dword(rbp[offsetof(syWV2, oks)]));

        // ... denn EQP - Panning rult.
        FLD(dword(rsi[offsetof(syVV2, panning)]));  // <p>
        FMUL(dword(m_fci128));                      // <p'>
        FLD(st0);                                   // <p'> <p'>
        FLD1();                                     // <1> <p'> <p'>
        FSUBRP(st1, st0);                           // <1-p'> <p'>
        FSQRT();                                    // <lv> <p'>
        FSTP(dword(rbp[offsetof(syWV2, lvol)]));    // <p'>
        FSQRT();                                    // <rv>
        FSTP(dword(rbp[offsetof(syWV2, rvol)]));

        // filter balance f²r parallel
        FLD(dword(rsi[offsetof(syVV2, fltbal)]));
        FSUB(dword(m_fc64));
        FIST(dword(m_temp));
        MOV(eax, m_temp);
        FMUL(dword(m_fci64));                       // <x>
        OR(eax, eax);
        if (JS())
            goto fbmin;
        FLD1();                                     // <1> <x>
        FSUBR(st1, st0);                            // <1> <1-x>
        goto fbgoon;
    fbmin:
        FLD1();                                     // <1> <x>
        FADD(st1, st0);                             // <g1> <g2>
        FXCH(st1);                                  // <g2> <g1>
    fbgoon:
        FSTP(dword(rbp[offsetof(syWV2, f2gain)]));  // <g1>
        FSTP(dword(rbp[offsetof(syWV2, f1gain)]));  // -

        rbp.data += offsetof(syWV2, osc1) - 0; // lea ebp, [ebp + syWV2.osc1 - 0]
        rsi.data += offsetof(syVV2, osc1) - 0; // lea esi, [esi + syVV2.osc1 - 0]
        syOscSet();
        rbp.data += offsetof(syWV2, osc2) - offsetof(syWV2, osc1); // lea ebp, [ebp + syWV2.osc2 - syWV2.osc1]
        rsi.data += offsetof(syVV2, osc2) - offsetof(syVV2, osc1); // lea esi, [esi + syVV2.osc2 - syVV2.osc1]
        syOscSet();
        rbp.data += offsetof(syWV2, osc3) - offsetof(syWV2, osc2); // lea ebp, [ebp + syWV2.osc3 - syWV2.osc2]
        rsi.data += offsetof(syVV2, osc3) - offsetof(syVV2, osc2); // lea esi, [esi + syVV2.osc3 - syVV2.osc2]
        syOscSet();
        rbp.data += offsetof(syWV2, aenv) - offsetof(syWV2, osc3); // lea ebp, [ebp + syWV2.aenv - syWV2.osc3]
        rsi.data += offsetof(syVV2, aenv) - offsetof(syVV2, osc3); // lea esi, [esi + syVV2.aenv - syVV2.osc3]
        syEnvSet();
        rbp.data += offsetof(syWV2, env2) - offsetof(syWV2, aenv); // lea ebp, [ebp + syWV2.env2 - syWV2.aenv]
        rsi.data += offsetof(syVV2, env2) - offsetof(syVV2, aenv); // lea esi, [esi + syVV2.env2 - syVV2.aenv]
        syEnvSet();
        rbp.data += offsetof(syWV2, vcf1) - offsetof(syWV2, env2); // lea ebp, [ebp + syWV2.vcf1 - syWV2.env2]
        rsi.data += offsetof(syVV2, vcf1) - offsetof(syVV2, env2); // lea esi, [esi + syVV2.vcf1 - syVV2.env2]
        syFltSet();
        rbp.data += offsetof(syWV2, vcf2) - offsetof(syWV2, vcf1); // lea ebp, [ebp + syWV2.vcf2 - syWV2.vcf1]
        rsi.data += offsetof(syVV2, vcf2) - offsetof(syVV2, vcf1); // lea esi, [esi + syVV2.vcf2 - syVV2.vcf1]
        syFltSet();
        rbp.data += offsetof(syWV2, lfo1) - offsetof(syWV2, vcf2); // lea ebp, [ebp + syWV2.lfo1 - syWV2.vcf2]
        rsi.data += offsetof(syVV2, lfo1) - offsetof(syVV2, vcf2); // lea esi, [esi + syVV2.lfo1 - syVV2.vcf2]
        syLFOSet();
        rbp.data += offsetof(syWV2, lfo2) - offsetof(syWV2, lfo1); // lea ebp, [ebp + syWV2.lfo2 - syWV2.lfo1]
        rsi.data += offsetof(syVV2, lfo2) - offsetof(syVV2, lfo1); // lea esi, [esi + syVV2.lfo2 - syVV2.lfo1]
        syLFOSet();
        rbp.data += offsetof(syWV2, dist) - offsetof(syWV2, lfo2); // lea ebp, [ebp + syWV2.dist - syWV2.lfo2]
        rsi.data += offsetof(syVV2, dist) - offsetof(syVV2, lfo2); // lea esi, [esi + syVV2.dist - syVV2.lfo2]
        syDistSet();
        POPAD();
    }


    // note on
    // eax: note
    // ebx: vel
    // ebp: workspace
    void SynthAsm::syV2NoteOn()
    {
        PUSHAD();
        MOV(rbp[offsetof(syWV2, note)], eax);
        FILD(dword(rbp[offsetof(syWV2, note)]));
        FADD(dword(rbp[offsetof(syWV2, xpose)]));

        FST(dword(rbp[offsetof(syWV2, osc1) + offsetof(syWOsc, note)]));
        FST(dword(rbp[offsetof(syWV2, osc2) + offsetof(syWOsc, note)]));
        FSTP(dword(rbp[offsetof(syWV2, osc3) + offsetof(syWOsc, note)]));
        MOV(m_temp, ebx);
        FILD(dword(m_temp));
        FSTP(dword(rbp[offsetof(syWV2, velo)]));
        XOR(eax, eax);
        INC(eax);
        MOV(rbp[offsetof(syWV2, gate)], eax);
        // reset EGs
        MOV(rbp[offsetof(syWV2, aenv) + offsetof(syWEnv, state)], eax);
        MOV(rbp[offsetof(syWV2, env2) + offsetof(syWEnv, state)], eax);

        MOV(ebx, rbp[offsetof(syWV2, oks)]);
        OR(ebx, ebx);
        if (JZ())
            goto nosync;

        XOR(eax, eax);
        MOV(rbp[offsetof(syWV2, osc1) + offsetof(syWOsc, cnt)], eax);
        MOV(rbp[offsetof(syWV2, osc2) + offsetof(syWOsc, cnt)], eax);
        MOV(rbp[offsetof(syWV2, osc3) + offsetof(syWOsc, cnt)], eax);

        CMP(bl, 2);
        if (JNZ())
            goto nosync;

        // HARDSYNC
        MOV(rbp[offsetof(syWV2, aenv) + offsetof(syWEnv, val)], eax);
        MOV(rbp[offsetof(syWV2, env2) + offsetof(syWEnv, val)], eax);
        MOV(rbp[offsetof(syWV2, curvol)], eax);

        rbp.data += offsetof(syWV2, osc1); // lea ebp, [ebp + syWV2.osc1]
        XOR(ecx, ecx);
        syOscInit();
        rbp.data += offsetof(syWV2, osc2) - offsetof(syWV2, osc1); // lea ebp, [ebp + syWV2.osc2 - syWV2.osc1]
        INC(ecx);
        syOscInit();
        rbp.data += offsetof(syWV2, osc3) - offsetof(syWV2, osc2); // lea ebp, [ebp + syWV2.osc3 - syWV2.osc2]
        INC(ecx);
        syOscInit();
        rbp.data += offsetof(syWV2, vcf1) - offsetof(syWV2, osc3); // lea ebp, [ebp + syWV2.vcf1 - syWV2.osc3]
        syFltInit();
        rbp.data += offsetof(syWV2, vcf2) - offsetof(syWV2, vcf1); // lea ebp, [ebp + syWV2.vcf2 - syWV2.vcf1]
        syFltInit();
        rbp.data += offsetof(syWV2, dist) - offsetof(syWV2, vcf2); // lea ebp, [ebp + syWV2.dist - syWV2.vcf2]
        syDistInit();
        rbp.data -= offsetof(syWV2, dist); // lea ebp, [ebp - syWV2.dist]

    nosync:

        rbp.data += offsetof(syWV2, osc1); // lea ebp, [ebp + syWV2.osc1 - 0]
        syOscChgPitch();
        rbp.data += offsetof(syWV2, osc2) - offsetof(syWV2, osc1); // lea ebp, [ebp + syWV2.osc2 - syWV2.osc1]
        syOscChgPitch();
        rbp.data += offsetof(syWV2, osc3) - offsetof(syWV2, osc2); // lea ebp, [ebp + syWV2.osc3 - syWV2.osc2]
        syOscChgPitch();

        rbp.data += offsetof(syWV2, lfo1) - offsetof(syWV2, osc3); // lea ebp, [ebp + syWV2.lfo1 - syWV2.osc3]
        syLFOKeyOn();
        rbp.data += offsetof(syWV2, lfo2) - offsetof(syWV2, lfo1); // lea ebp, [ebp + syWV2.lfo2 - syWV2.lfo1]
        syLFOKeyOn();

        rbp.data += offsetof(syWV2, dcf) - offsetof(syWV2, lfo2); // lea ebp, [ebp + syWV2.dcf - syWV2.lfo2]
        syDCFInit();

        POPAD();
    }


    // note off
    // ebp: workspace
    void SynthAsm::syV2NoteOff()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(rbp[offsetof(syWV2, gate)], eax);
        POPAD();
    }

    // table for mod sources
    static uint32_t sVTab[]{ offsetof(syWV2, aenv) + offsetof(syWEnv, out)
                , offsetof(syWV2, env2) + offsetof(syWEnv, out)
                , offsetof(syWV2, lfo1) + offsetof(syWLFO, out)
                , offsetof(syWV2, lfo2) + offsetof(syWLFO, out) };

    void SynthAsm::storeV2Values() // rbp: pointer auf globals / edx : voice
    {
        PUSHAD();

        MOV(ebx, rbp[offsetof(SYN, chanmap) + 4 * edx.data]); // ebx = channel
        OR(ebx, ebx);
        if (JNS())
            goto doit;
        goto end;                                   // voice gar ned belegt?
    doit:
        MOVZX(eax, byte(rbp[offsetof(SYN, chans) + 8 * ebx.data])); // pgmnummer
        MOV(rdi, rbp[offsetof(SYN, patchmap)]);
        rdi.data = *dword(rdi[4 * eax.data]); // mov edi, [edi + 4*eax]        ; edi -> sounddaten
        rdi.data += *reinterpret_cast<uint64_t*>(rbp[offsetof(SYN, patchmap)]); // add   edi, [ebp + SYN.patchmap]

        MOV(eax, edx);
        IMUL(eax, sizeof(syVV2));
        rsi.data = rbp.data + offsetof(SYN, voicesv) + eax.data; // lea esi, [ebp + SYN.voicesv + eax]      ; esi -> values
        MOV(eax, edx);
        IMUL(eax, sizeof(syWV2));
        rbx.data = rbp.data + offsetof(SYN, voicesw) + eax.data; // lea ebx, [ebp + SYN.voicesw + eax]      ; ebx -> workspace

        // voicedependent daten ²bertragen
        XOR(ecx, ecx);
    goloop:
        MOVZX(eax, byte(rdi[ecx.data]));
        MOV(m_temp, eax);
        FILD(dword(m_temp));
        FSTP(dword(rsi[4 * ecx.data]));
        INC(ecx);
        CMP(cl, v2sound_endvdvals);
        if (JNE())
            goto goloop;

        // MODMATRIX!
        MOVZX(ecx, byte(rdi[offsetof(v2sound, modnum)]));
        rdi.data += offsetof(v2sound, modmatrix); // lea edi, [edi + v2sound.modmatrix]
        OR(ecx, ecx);
        if (JNZ())
            goto modloop;
        goto modend;

    modloop:
        MOVZX(eax, byte(rdi[offsetof(v2mod, source)])); // source
        OR(eax, eax);
        if (JNZ())
            goto mnotvel;
        FLD(dword(rbx[offsetof(syWV2, velo)]));
        goto mdo;
    mnotvel:
        CMP(al, 8);
        if (JAE())
            goto mnotctl;
        PUSH(ebx);
        MOV(ebx, rbp[offsetof(SYN, chanmap) + 4 * edx.data]);
        ebx.data = ebx.data * 8 + eax.data; //lea     ebx, [8*ebx + eax]
        MOVZX(eax, byte(rbp[offsetof(SYN, chans) + ebx.data]));
        POP(ebx);
        MOV(dword(m_temp), eax);
        FILD(dword(m_temp));
        goto mdo;
    mnotctl:
        CMP(al, 12);
        if (JAE())
            goto mnotvm;
        AND(al, 3);
        MOV(eax, sVTab[eax.data]);
        FLD(dword(rbx[eax.data]));
        goto mdo;
    mnotvm:
        CMP(al, 13);
        if (JNE())
            goto mnotnote;
    mnotnote:
        FILD(dword(rbx[offsetof(syWV2, note)]));
        FSUB(dword(m_fc48));
        FADD(st0, st0);
        goto mdo;
    mdo:
        MOVZX(eax, byte(rdi[offsetof(v2mod, val)]));
        MOV(dword(m_temp), eax);
        FILD(dword(m_temp));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci128));
        FADD(st0, st0);
        FMULP(st1, st0);
        MOVZX(eax, byte(rdi[offsetof(v2mod, dest)]));
        CMP(eax, v2sound_endvdvals);
        if (JB())
            goto misok;
        FSTP(st0);
        goto mlend;
    misok:
        FADD(dword(rsi[4 * eax.data]));
        FSTP(dword(m_temp));
        // clippen
        PUSH(edx);
        MOV(edx, m_temp);
        OR(edx, edx);
        if (JNS())
            goto mnoclip1;
        XOR(edx, edx);
    mnoclip1:
        CMP(edx, 0x43000000);
        if (JBE())
            goto mnoclip2;
        MOV(edx, 0x43000000);
    mnoclip2:
        MOV(dword(rsi[4 * eax.data]), edx);
        POP(edx);
    mlend:
        rdi.data += 3; // lea edi, [edi+3]
        DEC(ecx);
        if (JZ())
            goto modend;
        goto modloop;
    modend:

        MOV(rbp, rbx);
        syV2Set();

    end:
        POPAD();
    }

    //#####################################################################################
    // Bass, Bass, wir brauchen Bass
    // BASS BOOST (fixed low shelving EQ)
    //#####################################################################################

    void SynthAsm::syBoostInit()
    {
        PUSHAD();
        POPAD();
    }
    // fixed frequency: 150Hz -> omega is 0,0213713785958489335949839685937381
    void SynthAsm::syBoostSet()
    {
        PUSHAD();

        FLD(dword(rsi[offsetof(syVBoost, amount)]));
        FIST(dword(m_temp));
        MOV(eax, m_temp);
        MOV(rbp[offsetof(syWBoost, ena)], eax);
        OR(eax, eax);
        if (JNZ())
            goto isena;
        FSTP(st0);
        POPAD();
        return;

    isena:
        // A = 10^(dBgain/40) bzw ne stark gefakete version davon
        FMUL(dword(m_fci128));
        pow2();                                     // <A>

        // beta = sqrt[ (A^2 + 1)/S - (A-1)^2 ]    (for shelving EQ filters only)
        FLD(st0);                                   // <A> <A>
        FMUL(st0, st0);                             // <A²> <A>
        FLD1();                                     // <1> <A²> <A>
        FADDP(st1, st0);                            // <A²+1> <A>
        FLD(st1);                                   // <A> <A²+1> <A>
        FLD1();                                     // <1> <A> <A²+1> <A>
        FSUBP(st1, st0);                            // <A-1> <A²+1> <A>
        FMUL(st0, st0);                             // <(A-1)²> <A²+1> <A>
        FSUBP(st1, st0);                            // <beta²> <A>
        FSQRT();                                    // <beta> <A>

        // zwischenvars: beta*sin, A+1, A-1, A+1*cos, A-1*cos
        FMUL(dword(m_SRfcBoostSin));                // <bs> <A>
        FLD1();                                     // <1> <bs> <A>
        FLD(st2);                                   // <A> <1> <bs> <A>
        FLD(st0);                                   // <A> <A> <1> <bs> <A>
        FSUB(st0, st2);                             // <A-> <A> <1> <bs> <A>
        FXCH(st1);                                  // <A> <A-> <1> <bs> <A>
        FADDP(st2, st0);                            // <A-> <A+> <bs> <A>
        FXCH(st1);                                  // <A+> <A-> <bs> <A>
        FLD(st0);                                   // <A+> <A+> <A-> <bs> <A>
        FMUL(dword(m_SRfcBoostCos));                // <cA+> <A+> <A-> <bs> <A>
        FLD(st2);                                   // <A-> <cA+> <A+> <A-> <bs> <A>
        FMUL(dword(m_SRfcBoostCos));                // <cA-> <cA+> <A+> <A-> <bs> <A>

        // a0 = (A+1) + (A-1)*cos + beta*sin
        FLD(st4);                                   // <bs> <cA-> <cA+> <A+> <A-> <bs> <A>
        FADD(st0, st1);                             // <bs+cA-> <cA-> <cA+> <A+> <A-> <bs> <A>
        FADD(st0, st3);                             // <a0> <cA-> <cA+> <A+> <A-> <bs> <A>

        // zwischenvar: 1/a0
        FLD1();                                     // <1> <a0> <cA-> <cA+> <A+> <A-> <bs> <A>
        FDIVRP(st1, st0);                           // <ia0> <cA-> <cA+> <A+> <A-> <bs> <A>

        // b1 = 2*A*[ (A-1) - (A+1)*cos
        FLD(st4);                                   // <A-> <ia0> <cA-> <cA+> <A+> <A-> <bs> <A>
        FSUB(st0, st3);                             // <A- - cA+> <ia0> <cA-> <cA+> <A+> <A-> <bs> <A>
        FADD(st0, st0);                             // <2(A- - cA+)> <ia0> <cA-> <cA+> <A+> <A-> <bs> <A>
        FMUL(st0, st7);                             // <b1> <ia0> <cA-> <cA+> <A+> <A-> <bs> <A>
        FMUL(st0, st1);                             // <b1'> <ia0> <cA-> <cA+> <A+> <A-> <bs> <A>
        FSTP(dword(rbp[offsetof(syWBoost, b1)]));   // <ia0> <cA-> <cA+> <A+> <A-> <bs> <A>


        // a1 = -2*[ (A-1) + (A+1)*cos
        FXCH(st4);                                  // <A-> <cA-> <cA+> <A+> <ia0> <bs> <A>
        FADDP(st2, st0);                            // <cA-> <cA+ + A-> <A+> <ia0> <bs> <A>
        FXCH(st1);                                  // <cA+ + A-> <cA-> <A+> <ia0> <bs> <A>
        FADD(st0, st0);                             // <2*(cA+ + A-)> <cA-> <A+> <ia0> <bs> <A>
        FCHS();                                     // <a1> <cA-> <A+> <ia0> <bs> <A>
        FMUL(st0, st3);                             // <a1'> <cA-> <A+> <ia0> <bs> <A>
        FSTP(dword(rbp[offsetof(syWBoost, a1)]));   // <cA-> <A+> <ia0> <bs> <A>


        // a2 = (A+1) + (A-1)*cos - beta*sin
        FLD(st0);                                   // <cA-> <cA-> <A+> <ia0> <bs> <A>
        FADD(st0, st2);                             // <A+ + cA-> <cA-> <A+> <ia0> <bs> <A>
        FSUB(st0, st4);                             // <a2> <cA-> <A+> <ia0> <bs> <A>
        FMUL(st0, st3);                             // <a2'> <cA-> <A+> <ia0> <bs> <A>
        FSTP(dword(rbp[offsetof(syWBoost, a2)]));   // <cA-> <A+> <ia0> <bs> <A>

        // b0 = A*[ (A+1) - (A-1)*cos + beta*sin ]
        FSUBP(st1, st0);                            // <A+ - cA-> <ia0> <bs> <A>
        FXCH(st1);                                  // <ia0> <A+ - cA-> <bs> <A>
        FMULP(st3, st0);                            // <A+ - cA-> <bs> <A*ia0>
        FLD(st0);                                   // <A+ - cA-> <A+ - cA-> <bs> <A*ia0>
        FADD(st0, st2);                             // <A+ - ca- + bs> <A+ - cA-> <bs> <A*ia0>
        FMUL(st0, st3);                             // <b0'> <A+ - cA-> <bs> <A*ia0>
        FSTP(dword(rbp[offsetof(syWBoost, b0)]));   // <A+ - cA-> <bs> <A*ia0>

        // b2 = A*[ (A+1) - (A-1)*cos - beta*sin ]
        FSUBRP(st1, st0);                           // <A+ - cA- - bs> <A*ia0>
        FMULP(st1, st0);                            // <b2'>
        FSTP(dword(rbp[offsetof(syWBoost, b2)]));   // -

        POPAD();
    }

    // rsi: src/dest buffer
    // ecx: # of samples

    // y[n] = (b0/a0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2] - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]
    void SynthAsm::syBoostProcChan()                // <y2> <x2> <y1> <x2>
    {
        PUSHAD();
    doloop:
        // y0 = b0'*in + b1'*x1 + b2'*x2 + a1'*y1 + a2'*y2
        FMUL(dword(rbp[offsetof(syWBoost, a2)]));   // <y2a2> <x2> <y1> <x1>
        FXCH(st1);                                  // <x2> <y2a2> <y1> <x1>
        FMUL(dword(rbp[offsetof(syWBoost, b2)]));   // <x2b2> <y2a2> <y1> <x1>
        FLD(st3);                                   // <x1> <x2b2> <y2a2> <y1> <x1>
        FMUL(dword(rbp[offsetof(syWBoost, b1)]));   // <x1b1> <x2b2> <y2a2> <y1> <x1>
        FLD(st3);                                   // <y1> <x1b1> <x2b2> <y2a2> <y1> <x1>
        FMUL(dword(rbp[offsetof(syWBoost, a1)]));   // <y1a1> <x1b1> <x2b2> <y2a2> <y1> <x1>
        FXCH(st3);                                  // <y2a2> <x1b1> <x2b2> <y1a1> <y1> <x1>
        FSUBP(st2, st0);                            // <x1b1> <x2b2-y2a2> <y1a1> <y1> <x1>
        FSUBRP(st2, st0);                           // <x2b2-y2a2> <x1b1-y1a1> <y1> <x1>
        FLD(dword(rsi[0]));                         // <x0> <x2b2-y2a2> <x1b1-y1a1> <y1> <x1>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        FXCH(st4);                                  // <x1> <x2b2-y2a2> <x1b1-y1a1> <y1> <x0>
        FLD(st4);                                   // <x0> <x1> <x2b2-y2a2> <x1b1-y1a1> <y1> <x0>
        FMUL(dword(rbp[offsetof(syWBoost, b0)]));   // <b0x0> <x1> <x2b2-y2a2> <x1b1-y1a1> <y1> <x0>
        FXCH(st2);                                  // <x2b2-y2a2> <x1> <b0x0> <x1b1-y1a1> <y1> <x0>
        FADDP(st3, st0);                            // <x1> <b0x0> <x1b1-y1a1+x2b2-y2a2> <y1> <x0>
        FXCH(st2);                                  // <x1b1-y1a1+x2b2-y2a2> <b0x0> <x1> <y1> <x0>
        FADDP(st1, st0);                            // <y0> <x1> <y1> <x0>
        FST(dword(rsi[0]));
        FXCH(st2);                                  // <y1> <x1> <y0> <x0>
        rsi.data += 8;// lea esi, [esi + 8]         // ... = <y2'> <x2'> <y1'> <x1'>
        DEC(ecx);
        if (JNZ())
            goto doloop;
        POPAD();
    }

    void SynthAsm::syBoostRender()
    {
        PUSHAD();

        TEST(byte(rbp[offsetof(syWBoost, ena)]), 255);
        if (JZ())
            goto nooo;

        // left channel
        FLD(dword(rbp[offsetof(syWBoost, x1)]));    // <x1>
        FLD(dword(rbp[offsetof(syWBoost, y1)]));    // <y1> <x1>
        FLD(dword(rbp[offsetof(syWBoost, x2)]));    // <x2> <y1> <x1>
        FLD(dword(rbp[offsetof(syWBoost, y2)]));    // <y2> <x2> <y1> <x1>
        syBoostProcChan();
        FSTP(dword(rbp[offsetof(syWBoost, y2)]));   // <x2'> <y1> <x1>
        FSTP(dword(rbp[offsetof(syWBoost, x2)]));   // <y1> <x1>
        FSTP(dword(rbp[offsetof(syWBoost, y1)]));   // <x1>
        FSTP(dword(rbp[offsetof(syWBoost, x1)]));   // -

        rsi.data += 4; // lea esi,  [esi+4]

        // right channel
        FLD(dword(rbp[offsetof(syWBoost, x1) + 4])); // <x1>
        FLD(dword(rbp[offsetof(syWBoost, y1) + 4])); // <y1> <x1>
        FLD(dword(rbp[offsetof(syWBoost, x2) + 4])); // <x2> <y1> <x1>
        FLD(dword(rbp[offsetof(syWBoost, y2) + 4])); // <y2> <x2> <y1> <x1>
        syBoostProcChan();
        FSTP(dword(rbp[offsetof(syWBoost, y2) + 4])); // <x2'> <y1> <x1>
        FSTP(dword(rbp[offsetof(syWBoost, x2) + 4])); // <y1> <x1>
        FSTP(dword(rbp[offsetof(syWBoost, y1) + 4])); // <x1>
        FSTP(dword(rbp[offsetof(syWBoost, x1) + 4])); // -

    nooo:
        POPAD();
    }

    //#####################################################################################
    // B²se Dinge, die man mit Jan Delay anstellen kann:
    //  MODULATING DELAYTEIL
    // (f²r chorus, flanger, aber auch als "gro²es" stereo delay. Mit Liebe verpackt, ganz f²r sie.)
    //#####################################################################################

    void SynthAsm::syModDelInit()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(rbp[offsetof(syWModDel, dbptr)], eax);
        MOV(rbp[offsetof(syWModDel, mcnt)], eax);
        MOV(rsi, rbp[offsetof(syWModDel, db1)]);
        MOV(rdi, rbp[offsetof(syWModDel, db2)]);
        MOV(ecx, rbp[offsetof(syWModDel, dbufmask)]);
    clloop:
        STOSD();
        MOV(rsi[4 * ecx.data], eax);
        DEC(ecx);
        if (JNS())
            goto clloop;
        POPAD();
    }

    void SynthAsm::syModDelSet()
    {
        PUSHAD();
        FLD(dword(rsi[offsetof(syVModDel, amount)]));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci128));
        FADD(st0, st0);
        FST(dword(rbp[offsetof(syWModDel, effout)]));
        FABS();
        FLD1();
        FSUBRP(st1, st0);
        FSTP(dword(rbp[offsetof(syWModDel, dryout)]));
        FLD(dword(rsi[offsetof(syVModDel, fb)]));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci128));
        FADD(st0, st0);
        FSTP(dword(rbp[offsetof(syWModDel, fbval)]));

        FILD(dword(rbp[offsetof(syWModDel, dbufmask)]));
        FSUB(dword(m_fc1023));
        FMUL(dword(m_fci128));
        FLD(dword(rsi[offsetof(syVModDel, llength)]));
        FMUL(st0, st1);
        FISTP(dword(rbp[offsetof(syWModDel, db1offs)]));
        FLD(dword(rsi[offsetof(syVModDel, rlength)]));
        FMULP(st1, st0);
        FISTP(dword(rbp[offsetof(syWModDel, db2offs)]));

        FLD(dword(rsi[offsetof(syVModDel, mrate)]));
        FMUL(dword(m_fci128));
        calcfreq();
        FMUL(dword(m_fcmdlfomul));
        FMUL(dword(m_SRfclinfreq));
        FISTP(dword(rbp[offsetof(syWModDel, mfreq)]));

        FLD(dword(rsi[offsetof(syVModDel, mdepth)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fc1023));
        FISTP(dword(rbp[offsetof(syWModDel, mmaxoffs)]));

        FLD(dword(rsi[offsetof(syVModDel, mphase)]));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fc32bit));
        FISTP(dword(rbp[offsetof(syWModDel, mphase)]));
        SHL(dword(rbp[offsetof(syWModDel, mphase)]), 1);

        POPAD();
    }

    void SynthAsm::syModDelProcessSample()
    {
        // fpu: <r> <l> <eff> <dry> <fb>
        // edx: buffer index

        PUSH(edx);

        // step 1: rechtes dingens holen
        MOV(eax, rbp[offsetof(syWModDel, mcnt)]);
        ADD(eax, rbp[offsetof(syWModDel, mphase)]);
        SHL(eax, 1);
        SBB(ebx, ebx);
        XOR(eax, ebx);
        MOV(ebx, rbp[offsetof(syWModDel, mmaxoffs)]);
        MUL(ebx);
        ADD(edx, rbp[offsetof(syWModDel, db2offs)]);
        MOV(ebx, rsp[0]);
        SUB(ebx, edx);
        DEC(ebx);
        AND(ebx, rbp[offsetof(syWModDel, dbufmask)]);
        SHR(eax, 9);
        OR(eax, 0x3f800000);
        MOV(m_temp, eax);
        FLD(dword(m_temp));                         // <1..2> <r> <l> <eff> <dry> <fb>
        FSUBR(dword(m_fc2));                        // <x> <r> <l> <eff> <dry> <fb>
        MOV(rax, rbp[offsetof(syWModDel, db2)]);
        FLD(dword(rax[4 * ebx.data]));              // <in1> <x> <r> <l> <eff> <dry> <fb>
        INC(ebx);
        AND(ebx, rbp[offsetof(syWModDel, dbufmask)]);
        FLD(dword(rax[4 * ebx.data]));              // <in2> <in1> <x> <r> <l> <eff> <dry> <fb>
        FSUB(st0, st1);                             // <in2-in1> <in1> <x> <r> <l> <eff> <dry> <fb>
        MOV(ebx, rsp[0]);
        FMULP(st2, st0);                            // <in1> <x*(in2-in1)> <r> <l> <eff> <dry> <fb>
        FADDP(st1, st0);                            // <in> <r> <l> <eff> <dry> <fb>
        FLD(st1);                                   // <r> <in> <r> <l> <eff> <dry> <fb>
        FMUL(st0, st5);                             // <r*dry> <in> <r> <l> <eff> <dry> <fb>
        FLD(st1);                                   // <in> <r*dry> <in> <r> <l> <eff> <dry> <fb>
        FMUL(st0, st5);                             // <in*eff> <r*dry> <in> <r> <l> <eff> <dry> <fb>
        FXCH(st2);                                  // <in> <in*eff> <r*dry> <r> <l> <eff> <dry> <fb>
        FMUL(st0, st7);                             // <in*fb> <in*eff> <r*dry> <r> <l> <eff> <dry> <fb>
        FXCH(st1);                                  // <in*eff> <in*fb> <r*dry> <r> <l> <eff> <dry> <fb>
        FADDP(st2, st0);                            // <in*fb> <r'> <r> <l> <eff> <dry> <fb>
        FADDP(st2, st0);                            // <r'> <rb> <l> <eff> <dry> <fb>
        FXCH(st1);                                  // <rb> <r'> <l> <eff> <dry> <fb>
        FSTP(dword(rax[4 * ebx.data]));             // <r'> <l> <eff> <dry> <fb>
        FXCH(st1);                                  // <l> <r'> <eff> <dry> <fb>

        // step 2: linkes dingens holen
        MOV(eax, rbp[offsetof(syWModDel, mcnt)]);
        SHL(eax, 1);
        SBB(ebx, ebx);
        XOR(eax, ebx);
        MOV(ebx, rbp[offsetof(syWModDel, mmaxoffs)]);
        MUL(ebx);
        ADD(edx, rbp[offsetof(syWModDel, db1offs)]);
        MOV(ebx, rsp[0]);
        SUB(ebx, edx);
        AND(ebx, rbp[offsetof(syWModDel, dbufmask)]);
        SHR(eax, 9);
        OR(eax, 0x3f800000);
        MOV(m_temp, eax);
        FLD(dword(m_temp));                         // <1..2> <l> <r'> <eff> <dry> <fb>
        FSUBR(dword(m_fc2));                        // <x> <l> <r'> <eff> <dry> <fb>
        MOV(rax, rbp[offsetof(syWModDel, db1)]);
        FLD(dword(rax[4 * ebx.data]));              // <in1> <x> <l> <r'> <eff> <dry> <fb>
        INC(ebx);
        AND(ebx, rbp[offsetof(syWModDel, dbufmask)]);
        FLD(dword(rax[4 * ebx.data]));              // <in2> <in1> <x> <l> <r'> <eff> <dry> <fb>
        FSUB(st0, st1);                             // <in2-in1> <in1> <x> <l> <r'> <eff> <dry> <fb>
        MOV(ebx, rsp[0]);
        FMULP(st2, st0);                            // <in1> <x*(in2-in1)> <l> <r'> <eff> <dry> <fb>
        FADDP(st1, st0);                            // <in> <l> <r'> <eff> <dry> <fb>
        FLD(st1);                                   // <l> <in> <l> <r'> <eff> <dry> <fb>
        FMUL(st0, st5);                             // <l*dry> <in> <l> <r'> <eff> <dry> <fb>
        FLD(st1);                                   // <in> <l*dry> <in> <l> <r'> <eff> <dry> <fb>
        FMUL(st0, st5);                             // <in*eff> <l*dry> <in> <l> <r'> <eff> <dry> <fb>
        FXCH(st2);                                  // <in> <in*eff> <l*dry> <l> <r'> <eff> <dry> <fb>
        FMUL(st0, st7);                             // <in*fb> <in*eff> <l*dry> <l> <r'> <eff> <dry> <fb>
        FXCH(st1);                                  // <in*eff> <in*fb> <l*dry> <l> <r'> <eff> <dry> <fb>
        FADDP(st2, st0);                            // <in*fb> <l'> <l> <r'> <eff> <dry> <fb>
        FADDP(st2, st0);                            // <l'> <lb> <r'> <eff> <dry> <fb>
        FXCH(st1);                                  // <lb> <l'> <r'> <eff> <dry> <fb>
        FSTP(dword(rax[4 * ebx.data]));             // <l'> <r'> <eff> <dry> <fb>

        POP(edx);
        MOV(eax, rbp[offsetof(syWModDel, mfreq)]);
        ADD(rbp[offsetof(syWModDel, mcnt)], eax);
        INC(edx);
        AND(edx, rbp[offsetof(syWModDel, dbufmask)]);
    }


    void SynthAsm::syModDelRenderAux2Main()
    {
        PUSHAD();

        MOV(eax, rbp[offsetof(syWModDel, effout)]);
        OR(eax, eax);
        if (JZ())
            goto dont;

        FLD(dword(rbp[offsetof(syWModDel, fbval)])); // <fb>
        FLDZ();                                     // <"dry"=0> <fb>
        FLD(dword(rbp[offsetof(syWModDel, effout)])); // <eff> <dry> <fb>
        MOV(edx, rbp[offsetof(syWModDel, dbptr)]);
        MOV(rsi, &m_this);
        ADD(rsi, offsetof(SYN, aux2buf));
    rloop:
        FLD(dword(rsi[0]));                         // <m> <eff> <dry> <fb>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        FLD(st0);                                   // <m> <m> <eff> <dry> <fb>
        rsi.data += 4; // lea esi, [esi + 4]
        syModDelProcessSample();                    // <l'> <r'> <eff> <dry> <fb>
        FADD(dword(rdi[0]));                        // <lout> <r'> <eff> <dry> <fb>
        FSTP(dword(rdi[0]));                        // <r'> <eff> <dry> <fb>
        FADD(dword(rdi[4]));                        // <rout> <eff> <dry> <fb>
        FSTP(dword(rdi[4]));                        // <eff> <dry> <fb>
        rdi.data += 8;// lea edi, [edi+8]

        DEC(ecx);
        if (JNZ())
            goto rloop;
        MOV(rbp[offsetof(syWModDel, dbptr)], edx);
        FSTP(st0);                                  // <dry> <fb>
        FSTP(st0);                                  // <fb>
        FSTP(st0);                                  // -

    dont:
        POPAD();
    }

    void SynthAsm::syModDelRenderChan()
    {
        PUSHAD();

        MOV(eax, rbp[offsetof(syWModDel, effout)]);
        OR(eax, eax);
        if (JZ())
            goto dont;

        FLD(dword(rbp[offsetof(syWModDel, fbval)])); // <fb>
        FLD(dword(rbp[offsetof(syWModDel, dryout)])); // <dry> <fb>
        FLD(dword(rbp[offsetof(syWModDel, effout)])); // <eff> <dry> <fb>
        MOV(edx, rbp[offsetof(syWModDel, dbptr)]);
        MOV(rsi, &m_this);
        ADD(rsi, offsetof(SYN, chanbuf));
    rloop:
        FLD(dword(rsi[0]));                         // <l> <eff> <dry> <fb>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        FLD(dword(rsi[4]));                         // <r> <l> <eff> <dry> <fb>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        syModDelProcessSample();                    // <l'> <r'> <eff> <dry> <fb>
        FSTP(dword(rsi[0]));                        // <r'> <eff> <dry> <fb>
        FSTP(dword(rsi[4]));                        // <eff> <dry> <fb>
        rsi.data += 8; // lea esi, [esi+8]
        DEC(ecx);
        if (JNZ())
            goto rloop;
        MOV(rbp[offsetof(syWModDel, dbptr)], edx);
        FSTP(st0);                                  // <dry> <fb>
        FSTP(st0);                                  // <fb>
        FSTP(st0);                                  // -

    dont:
        POPAD();
    }

    //#####################################################################################
    // F²r Ronny und die Freunde des Lauten:
    //  STEREO COMPRESSOR
    //#####################################################################################

    //PLAN:

    // 1. Pegeldetection
    // - entweder Peak mit fixem Falloff...
    // - oder RMS ²ber einen 8192-Samples-Buffer
    // MODE: stereo/mono
    // Zukunft: sidechain? low/highcut daf²r?

    // 2. Lookahead:
    // - Delayline f²rs Signal, L²nge einstellbar (127msecs)

    // 3. Pegelangleicher
    // Werte: Threshold, Ratio (1:1 ... 1:inf), Attack (0..?), Decay (0..?)
    // Zukunft:  Transientdingens (releasetime-anpassung)? Enhancer (high shelving EQ mit boost 1/reduction)?
    //           Knee? (ATAN!)


    void SynthAsm::syCompInit()
    {
        PUSHAD();
        MOV(al, 2);
        MOV(rbp[offsetof(syWComp, mode)], al);
        POPAD();
    }

    void SynthAsm::syCompSet()
    {
        PUSHAD();
        FLD(dword(rsi[offsetof(syVComp, mode)]));
        FISTP(dword(m_temp));
        MOV(eax, m_temp);
        DEC(eax);
        AND(eax, 5);
        FLD(dword(rsi[offsetof(syVComp, stereo)]));
        FISTP(dword(m_temp));
        MOV(ebx, m_temp);
        ADD(ebx, ebx);
        ADD(eax, ebx);
        MOV(rbp[offsetof(syWComp, mode)], eax);
        CMP(eax, rbp[offsetof(syWComp, oldmode)]);
        if (JE())
            goto norst;
        MOV(rbp[offsetof(syWComp, oldmode)], eax);
        MOV(ecx, 2 * 8192);
        XOR(eax, eax);
        MOV(rbp[offsetof(syWComp, pkval1)], eax);
        MOV(rbp[offsetof(syWComp, pkval2)], eax);
        MOV(rbp[offsetof(syWComp, rmsval1)], eax);
        MOV(rbp[offsetof(syWComp, rmsval2)], eax);
        rdi.data = rbp.data + offsetof(syWComp, rmsbuf); // lea edi, [ebp + syWComp.rmsbuf]
        REP_STOSD();
        FLD1();
        FST(dword(rbp[offsetof(syWComp, curgain1)]));
        FSTP(dword(rbp[offsetof(syWComp, curgain2)]));

    norst:
        FLD(dword(rsi[offsetof(syVComp, lookahead)]));
        FMUL(dword(m_fcsamplesperms));
        FISTP(dword(rbp[offsetof(syWComp, dblen)]));
        FLD(dword(rsi[offsetof(syVComp, threshold)]));
        FMUL(dword(m_fci128));
        calcfreq();
        FADD(st0, st0);
        FADD(st0, st0);
        FADD(st0, st0);
        FLD1();
        FDIV(st0, st1);
        FSTP(dword(rbp[offsetof(syWComp, invol)]));

        FLD(dword(rsi[offsetof(syVComp, autogain)]));
        FISTP(dword(m_temp));
        MOV(eax, m_temp);
        OR(eax, eax);
        if (JZ())
            goto noag;
        FSTP(st0);
        FLD1();
    noag:
        FLD(dword(rsi[offsetof(syVComp, outgain)]));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci16));
        pow2();
        FMULP(st1, st0);
        FSTP(dword(rbp[offsetof(syWComp, outvol)]));

        FLD(dword(rsi[offsetof(syVComp, ratio)]));
        FMUL(dword(m_fci128));
        FSTP(dword(rbp[offsetof(syWComp, ratio)]));

        // attack: 0 (!) ... 200msecs (5Hz)
        FLD(dword(rsi[offsetof(syVComp, attack)]));
        FMUL(dword(m_fci128));                      // 0 .. fast1
        FMUL(dword(m_fcm12));                       // 0 .. fastminus12
        pow2();                                     // 1 .. 2^(fastminus12)
        FSTP(dword(rbp[offsetof(syWComp, attack)]));

        // release: 5ms bis 5s
        FLD(dword(rsi[offsetof(syVComp, release)]));
        FMUL(dword(m_fci128));                      // 0 .. fast1
        FMUL(dword(m_fcm16));                       // 0 .. fastminus16
        pow2();                                     // 1 .. 2^(fastminus16)
        FSTP(dword(rbp[offsetof(syWComp, release)]));

        POPAD();
    }

    void SynthAsm::syCompLDMonoPeak()
    {
        PUSHAD();
        FLD(dword(rbp[offsetof(syWComp, pkval1)]));  // <pv>
    lp:
        FLD(dword(rsi[0]));                         // <l> <pv>
        FADD(dword(rsi[4]));                        // <l+r> <pv>
        FMUL(dword(m_fci2));                        // <in> <pv>
        FSTP(dword(m_temp));                        // <pv>
        FMUL(dword(m_fccpdfalloff));                // <pv'>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        rsi.data += 8; // lea esi, [esi + 8]
        FSTP(dword(m_temp + 4));                    // -
        MOV(eax, m_temp);
        AND(eax, 0x7fffffff);                       // fabs()
        CMP(eax, m_temp + 4);
        if (JBE())
            goto nonp;
        MOV(m_temp + 4, eax);
    nonp:
        FLD(dword(m_temp + 4));                     // <npv>
        FLD(st0);
        FMUL(dword(rbp[offsetof(syWComp, invol)]));
        FST(dword(rdi[0]));
        FSTP(dword(rdi[4]));
        rdi.data += 8; // lea edi, [edi+8]
        DEC(ecx);
        if (JNZ())
            goto lp;
        FSTP(dword(rbp[offsetof(syWComp, pkval1)])); // -
        POPAD();
    }

    void SynthAsm::syCompLDMonoRMS()
    {
        PUSHAD();
        FLD(dword(rbp[offsetof(syWComp, rmsval1)])); // <rv>
        MOV(eax, rbp[offsetof(syWComp, rmscnt)]);
        rdx.data = rbp.data + offsetof(syWComp, rmsbuf);// lea edx, [ebp + syWComp.rmsbuf]
    lp:
        FSUB(dword(rdx[4 * eax.data]));             // <rv'>
        FLD(dword(rsi[0]));                         // <l> <rv'>
        FADD(dword(rsi[4]));                        // <l+r> <rv'>
        FMUL(dword(m_fci2));                        // <in> <rv'>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        rsi.data += 8; // lea esi, [esi+8]
        FMUL(st0, st0);                             // <in²> <rv'>
        FADD(st1, st0);                             // <in²> <rv''>
        FSTP(dword(rdx[4 * eax.data]));             // <rv''>
        FLD(st0);                                   // <rv''> <rv''>
        FSQRT();                                    // <sqrv> <rv''>
        FMUL(dword(m_fci8192));
        INC(eax);
        AND(ah, 0x1f);
        FMUL(dword(rbp[offsetof(syWComp, invol)]));
        FST(dword(rdi[0]));
        FSTP(dword(rdi[4]));                        // <rv''>
        rdi.data += 8; // lea edi, [edi+8]
        DEC(ecx);
        if (JNZ())
            goto lp;
        MOV(rbp[offsetof(syWComp, rmscnt)], eax);
        FSTP(dword(rbp[offsetof(syWComp, rmsval1)])); // -
        POPAD();
    }

    void SynthAsm::syCompLDStereoPeak()
    {
        PUSHAD();
        FLD(dword(rbp[offsetof(syWComp, pkval2)])); // <rpv>
        FLD(dword(rbp[offsetof(syWComp, pkval1)])); // <lpv> <rpv>
    lp:
        FMUL(dword(m_fccpdfalloff));                // <lpv'> <rpv>
        FXCH(st1);                                  // <rpv> <lpv'>
        FMUL(dword(m_fccpdfalloff));                // <rpv'> <lpv'>
        FXCH(st1);                                  // <lpv'> <rpv'>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
        FXCH(st1);
        FADD(dword(m_fcdcoffset));
        FXCH(st1);
#endif
        FSTP(dword(m_temp));                        // <rpv'>
        FSTP(dword(m_temp + 4));                    // -
        MOV(eax, rsi[0]);
        AND(eax, 0x7fffffff);                       // fabs()
        CMP(eax, m_temp);
        if (JBE())
            goto nonp1;
        MOV(m_temp, eax);
    nonp1:
        MOV(eax, rsi[4]);
        AND(eax, 0x7fffffff);                       // fabs()
        CMP(eax, m_temp + 4);
        if (JBE())
            goto nonp2;
        MOV(m_temp + 4, eax);
    nonp2:
        rsi.data += 8; // lea esi, [esi+8]
        FLD(dword(m_temp + 4));                     // <nrpv>
        FLD(st0);
        FMUL(dword(rbp[offsetof(syWComp, invol)]));
        FSTP(dword(rdi[4]));
        FLD(dword(m_temp));                         // <nlpv> <nrpv>
        FLD(st0);
        FMUL(dword(rbp[offsetof(syWComp, invol)]));
        FSTP(dword(rdi[0]));
        rdi.data += 8; // lea edi, [edi+8]
        DEC(ecx);
        if (JNZ())
            goto lp;
        FSTP(dword(rbp[offsetof(syWComp, pkval1)])); // <nrpv>
        FSTP(dword(rbp[offsetof(syWComp, pkval2)])); // -
        POPAD();
    }

    void SynthAsm::syCompLDStereoRMS()
    {
        PUSHAD();
        FLD(dword(rbp[offsetof(syWComp, rmsval2)])); // <rrv>
        FLD(dword(rbp[offsetof(syWComp, rmsval1)])); // <lrv> <rrv>
        MOV(eax, rbp[offsetof(syWComp, rmscnt)]);
        rdx.data = rbp.data + offsetof(syWComp, rmsbuf); // lea edx, [ebp + syWComp.rmsbuf]
    lp:
        FSUB(dword(rdx[8 * eax.data]));             // <lrv'> <rrv>
        FXCH(st1);                                  // <rrv> <lrv'>
        FSUB(dword(rdx[8 * eax.data + 4]));         // <rrv'> <lrv'>
        FXCH(st1);                                  // <lrv'> <rrv'>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
        FXCH(st1);
        FADD(dword(m_fcdcoffset));
        FXCH(st1);
#endif

        FLD(dword(rsi[0]));                         // <l> <lrv'> <rrv'>
        FMUL(st0, st0);                             // <l²> <lrv'> <rrv'>
        FADD(st1, st0);                             // <l²> <lrv''> <rrv'>
        FSTP(dword(rdx[8 * eax.data]));             // <lrv''> <rrv'>
        FLD(st0);                                   // <lrv''> <lrv''> <rrv'>
        FSQRT();                                    // <sqlrv> <lrv''> <rrv'>
        FMUL(dword(m_fci8192));
        FMUL(dword(rbp[offsetof(syWComp, invol)]));
        FSTP(dword(rdi[0]));                        // <lrv''> <rrv'>

        FLD(dword(rsi[4]));                         // <r> <lrv''> <rrv'>
        FMUL(st0, st0);                             // <r²> <lrv''> <rrv'>
        FADD(st2, st0);                             // <r²> <lrv''> <rrv''>
        FSTP(dword(rdx[8 * eax.data + 4]));         // <lrv''> <rrv''>
        FLD(st1);                                   // <rrv''> <lrv''> <rrv''>
        FSQRT();                                    // <sqrrv> <lrv''> <rrv''>
        FMUL(dword(m_fci8192));
        FMUL(dword(rbp[offsetof(syWComp, invol)]));
        FSTP(dword(rdi[4]));                        // <lrv''> <rrv''>

        rsi.data += 8; // lea esi, [esi+8]
        INC(eax);
        AND(ah, 0x1f);
        rdi.data += 8; // lea edi, [edi+8]
        DEC(ecx);
        if (JNZ())
            goto lp;
        MOV(rbp[offsetof(syWComp, rmscnt)], eax);
        FSTP(dword(rbp[offsetof(syWComp, rmsval1)])); // <nrrv>
        FSTP(dword(rbp[offsetof(syWComp, rmsval2)])); // -
        POPAD();
    }

    // rbp: this
    // rbx: ptr to lookahead line
    // ecx: # of samples to process
    // edx: offset into lookahead line
    // rsi: ptr to in/out buffer
    // rdi: ptr to level buffer
    // st0: current gain value
    void SynthAsm::syCompProcChannel()
    {
        PUSHAD();
    cloop:

        FST(dword(m_temp));

        // lookahead
        FLD(dword(rbx[8 * edx.data]));              // <v> <gain>
        FLD(dword(rsi[0]));                         // <nv> <v> <gain>
        FMUL(dword(rbp[offsetof(syWComp, invol)])); // <nv'> <v> <gain>
        FSTP(dword(rbx[8 * edx.data]));             // <v> <gain>
        FMUL(dword(rbp[offsetof(syWComp, outvol)])); // <v'> <gain>
        INC(edx);
        CMP(edx, rbp[offsetof(syWComp, dblen)]);
        if (JBE())
            goto norst;
        XOR(edx, edx);
    norst:

        // destgain ermitteln
        MOV(eax, rdi[0]);
        CMP(eax, 0x3f800000);                       // 1.0
        if (JAE())
            goto docomp;
        FLD1();                                     // <dgain> <v> <gain>
        goto cok;
    docomp:
        FLD(dword(rdi[0]));                         // <lvl> <v> <gain>
        FLD1();                                     // <1> <lvl> <v> <gain>
        FSUBP(st1, st0);                            // <lvl-1> <v> <gain>
        FMUL(dword(rbp[offsetof(syWComp, ratio)])); // <r*(lvl-1)> <v> <gain>
        FLD1();                                     // <1> <r*(lvl-1)> <v> <gain>
        FADDP(st1, st0);                            // <1+r*(lvl-1)> <v> <gain>
        FLD1();                                     // <1> <1+r*(lvl-1)> <v> <gain>
        FDIVRP(st1, st0);                           // <dgain> <v> <gain>
    cok:
        rdi.data += 8; // lea edi, [edi+8]

        FST(dword(m_temp + 4));
        MOV(eax, m_temp + 4);
        CMP(eax, m_temp);
        if (JB())
            goto attack;
        FLD(dword(rbp[offsetof(syWComp, release)])); // <spd> <dgain> <v> <gain>
        goto cok2;
    attack:
        FLD(dword(rbp[offsetof(syWComp, attack)])); // <spd> <dgain> <v> <gain>

    cok2:
        // und compressen
        FXCH(st1);                                  // <dg> <spd> <v> <gain>
        FSUB(st0, st3);                             // <dg-gain> <spd> <v> <gain>
        FMULP(st1, st0);                            // <spd*(dg-d)> <v> <gain>
        FADDP(st2, st0);                            // <v> <gain'>
        FMUL(st0, st1);                             // <out> <gain'>
        FSTP(dword(rsi[0]));                        // <gain'>
        rsi.data += 8; // lea esi, [esi+8]

        DEC(ecx);
        if (JNZ())
            goto cloop;
        MOV(m_temp, edx);
        POPAD();
    }
    // on exit: [temp] = new dline count

/*
syCRMTab  dd syCompLDMonoPeak, syCompLDMonoRMS, syCompLDStereoPeak, syCompLDStereoRMS
*/

    // rsi: input/output buffer
    // ecx: # of samples
    void SynthAsm::syCompRender()
    {
        PUSHAD();

        FCLEX();                                    // clear exceptions

        MOV(eax, rbp[offsetof(syWComp, mode)]);
        TEST(al, 4);
        if (JZ())
            goto doit;

        POPAD();
        return;

    doit:
        // STEP 1: level detect (fills LD buffers)

        MOV(rdi, &m_this);
        ADD(rdi, offsetof(SYN, vcebuf));
        AND(al, 3);
        // call [syCRMTab + 4*eax]
        if (eax.data == 0) syCompLDMonoPeak(); else if (eax.data == 1) syCompLDMonoRMS(); else if (eax.data == 2) syCompLDStereoPeak(); else syCompLDStereoRMS();

        // check for FPU exception
        FSTSW(ax);
        OR(al, al);
        if (JNS())
            goto fpuok;
        // if occured, clear LD buffer
        PUSH(ecx);
        PUSH(edi);
        FLDZ();
        ADD(ecx, ecx);
        XOR(eax, eax);
        REP_STOSD();
        FSTP(st0);
        POP(edi);
        POP(ecx);
    fpuok:

        // STEP 2: compress!
        rbx.data = rbp.data + offsetof(syWComp, dbuf); // lea ebx, [ebp + syWComp.dbuf]
        MOV(edx, rbp[offsetof(syWComp, dbcnt)]);
        FLD(dword(rbp[offsetof(syWComp, curgain1)]));
        syCompProcChannel();
        FSTP(dword(rbp[offsetof(syWComp, curgain1)]));
        rsi.data += 4; // lea esi, [esi+4]
        rdi.data += 4; // lea edi, [edi+4]
        rbx.data += 4; // lea ebx, [ebx+4]
        FLD(dword(rbp[offsetof(syWComp, curgain2)]));
        syCompProcChannel();
        FSTP(dword(rbp[offsetof(syWComp, curgain2)]));
        MOV(edx, m_temp);
        MOV(rbp[offsetof(syWComp, dbcnt)], edx);

        POPAD();
    }

    //#####################################################################################
    //#
    //#                            E L I T E G R O U P
    //#                             we are very good.
    //#
    //# World Domination Intro Sound System
    //# -> Stereo reverb plugin (reads aux1)
    //#
    //# Written and (C) 1999 by The Artist Formerly Known As Doctor Roole
    //#
    //# This is a modified  Schroeder reverb (as found in  csound et al) consisting
    //# of four parallel comb filter delay lines (with low pass filtered feedback),
    //# followed by two  allpass filter delay lines  per channel. The  input signal
    //# is feeded directly into half of the comb delays, while it's inverted before
    //# being feeded into the other half to  minimize the response to DC offsets in
    //# the incoming signal, which was a  great problem of the original implementa-
    //# tion. Also, all of the comb delays are routed through 12dB/oct IIR low pass
    //# filters before feeding the output  signal back to the input to simulate the
    //# walls' high damping, which makes this  reverb sound a lot smoother and much
    //# more realistic.
    //#
    //# This leaves nothing but the conclusion that we're simply better than you.
    //#
    //#####################################################################################


    static float syRvDefs[]{ 0.966384599f, 0.958186359f, 0.953783929f, 0.950933178f, 0.994260075f, 0.998044717f,
        1.0f,  // input gain
        0.8f };// high cut

    void SynthAsm::syReverbInit()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(ecx, sizeof(syWReverb));
        MOV(rdi, rbp);
        REP_STOSB();
        POPAD();
    }

    void SynthAsm::syReverbReset()
    {
        PUSHAD();
        XOR(eax, eax);
        MOV(ecx, sizeof(syWReverb) - offsetof(syWReverb, poscl0));// offsetof(syWReverb, dyn));
        rdi.data = rbp.data + offsetof(syWReverb, poscl0);// offsetof(syWReverb, dyn); // lea edi, [ebp + syWReverb.dyn]
        REP_STOSB();
        POPAD();
    }

    void SynthAsm::syReverbSet()
    {
        PUSHAD();

        FLD(dword(rsi[offsetof(syVReverb, revtime)]));
        FLD1();
        FADDP(st1, st0);
        FLD(dword(m_fc64));
        FDIVRP(st1, st0);
        FMUL(st0, st0);
        FMUL(dword(m_SRfclinfreq));
        XOR(ecx, ecx);
    rtloop:
        FLD(st0);
        FLD(dword(syRvDefs + ecx.data));
        pow();
        FSTP(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc0) + 4 * ecx.data]));
        INC(ecx);
        CMP(cl, 6);
        if (JNE())
            goto rtloop;
        FSTP(st0);

        FLD(dword(rsi[offsetof(syVReverb, highcut)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_SRfclinfreq));
        FSTP(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, damp)]));
        FLD(dword(rsi[offsetof(syVReverb, vol)]));
        FMUL(dword(m_fci128));
        FSTP(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainin)]));

        FLD(dword(rsi[offsetof(syVReverb, lowcut)]));
        FMUL(dword(m_fci128));
        FMUL(st0, st0);
        FMUL(st0, st0);
        FMUL(dword(m_SRfclinfreq));
        FSTP(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, lowcut)]));

        POPAD();
    }

    void SynthAsm::syReverbProcess()
    {
        PUSHAD();

        FCLEX();

        MOV(rsi, &m_this);
        ADD(rsi, offsetof(SYN, aux1buf));
        FLD(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, lowcut)])); // <lc> <0>
        FLD(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, damp)])); // <damp> <lc> <0>
        XOR(ebx, ebx);
        MOV(eax, ecx);

    sloop: // prinzipiell nur ne gro²e schleife
        // step 1: get input sample
        FLD(dword(rsi[0]));                         // <in'> <damp> <lc> <0>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainin)])); // <in> <damp> <lc> <0>
#if FIXDENORMALS
        FADD(dword(m_fcdcoffset));
#endif
        rsi.data += 4; // lea esi, [esi+4]

        // step 2a: process the 4 left lpf filtered comb delays
        // left comb 0
        MOV(edx, rbp[offsetof(syWReverb, poscl0)]);
        FLD(dword(rbp[offsetof(syWReverb, linecl0) + 4 * edx.data])); // <dv> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc0)])); // <dv'> <in> <damp> <lc> <chk>
        FADD(st0, st1);                             // <nv>  <in> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcl0)])); // <v-lp> <in> <damp> <lc> <chk>
        FMUL(st0, st2);                             // <d*(v-lp)> <in> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcl0)])); // <dout> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcl0)])); // <dout> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecl0) + 4 * edx.data])); // <asuml> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencl0);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscl0)], edx);

        // left comb 1
        MOV(edx, rbp[offsetof(syWReverb, poscl1)]);
        FLD(dword(rbp[offsetof(syWReverb, linecl1) + 4 * edx.data])); // <dv> <asuml> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc1)])); // <dv'> <asuml> <in> <damp> <lc> <chk>
        FSUB(st0, st2);                             // <nv>  <asuml> <in> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcl1)])); // <v-lp> <asuml> <in> <damp> <lc> <chk>
        FMUL(st0, st3);                             // <d*(v-lp)> <asuml> <in> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcl1)])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcl1)])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecl1) + 4 * edx.data])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <asuml'> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencl1);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscl1)], edx);

        // left comb 2
        MOV(edx, rbp[offsetof(syWReverb, poscl2)]);
        FLD(dword(rbp[offsetof(syWReverb, linecl2) + 4 * edx.data])); // <dv> <asuml> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc2)])); // <dv'> <asuml> <in> <damp> <lc> <chk>
        FADD(st0, st2);                             // <nv>  <asuml> <in> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcl2)])); // <v-lp> <asuml> <in> <damp> <lc> <chk>
        FMUL(st0, st3);                             // <d*(v-lp)> <asuml> <in> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcl2)])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcl2)])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecl2) + 4 * edx.data])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <asuml'> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencl2);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscl2)], edx);

        // left comb 3
        MOV(edx, rbp[offsetof(syWReverb, poscl3)]);
        FLD(dword(rbp[offsetof(syWReverb, linecl3) + 4 * edx.data])); // <dv> <asuml> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc3)])); // <dv'> <asuml> <in> <damp> <lc> <chk>
        FSUB(st0, st2);                             // <nv>  <asuml> <in> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcl3)])); // <v-lp> <asuml> <in> <damp> <lc> <chk>
        FMUL(st0, st3);                             // <d*(v-lp)> <asuml> <in> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcl3)])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcl3)])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecl3) + 4 * edx.data])); // <dout> <asuml> <in> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <asuml'> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencl3);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscl3)], edx);

        // step 2b: process the 2 left allpass delays
        // left allpass 0
        MOV(edx, rbp[offsetof(syWReverb, posal0)]);
        FLD(dword(rbp[offsetof(syWReverb, lineal0) + 4 * edx.data])); // <d0v> <asuml> <in> <damp> <lc> <chk>
        FLD(st0);                                   // <d0v> <d0v> <asuml> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina0)])); // <d0v'> <d0v> <asuml> <in> <damp> <lc> <chk>
        FADDP(st2, st0);                            // <d0v> <d0z> <in> <damp> <lc> <chk>
        FXCH(st1);                             // <d0z> <d0v> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lineal0) + 4 * edx.data]));
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina0)])); // <d0z'> <d0v> <in> <damp> <lc> <chk>
        FSUBP(st1, st0);                            // <d0o> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dl, lenal0);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, posal0)], edx);

        // left allpass 1
        MOV(edx, rbp[offsetof(syWReverb, posal1)]);
        FLD(dword(rbp[offsetof(syWReverb, lineal1) + 4 * edx.data])); // <d1v> <d0o> <in> <damp> <lc> <chk>
        FLD(st0);                                   // <d1v> <d1v> <d0o> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina1)])); // <d1v'> <d1v> <d0o> <in> <damp> <lc> <chk>
        FADDP(st2, st0);                            // <d1v> <d1z> <in> <damp> <lc> <chk>
        FXCH(st1);                             // <d1z> <d1v> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lineal1) + 4 * edx.data]));
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina1)])); // <d1z'> <d1v> <in> <damp> <lc> <chk>
        FSUBP(st1, st0);                            // <d1o> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dl, lenal1);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, posal1)], edx);

        // step 2c: low cut
        FLD(dword(rbp[offsetof(syWReverb, hpfcl)])); // <hpf> <d1o> <in> <damp> <lc> <chk>
        FLD(st0);                                   // <hpf> <hpf> <d1o> <in> <damp> <lc> <chk>
        FSUBR(st0, st2);                            // <d1o-hpf> <hpf> <d1o> <in> <damp> <lc> <chk>
        FMUL(st0, st5);                             // <lc(d1o-hpf)> <hpf> <d1o> <in> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <hpf'> <d1o> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, hpfcl)]));
        FSUBP(st1, st0);                            // <outl> <in> <damp> <lc> <chk>

        // step 2d: update left mixing buffer
        FADD(dword(rdi[0]));
        FSTP(dword(rdi[0]));                        // <in> <damp> <lc> <chk>

        // step 3a: process the 4 right lpf filtered comb delays
        // right comb 0
        MOV(edx, rbp[offsetof(syWReverb, poscr0)]);
        FLD(dword(rbp[offsetof(syWReverb, linecr0) + 4 * edx.data])); // <dv> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc0)])); // <dv'> <in> <damp> <lc> <chk>
        FADD(st0, st1);                             // <nv>  <in> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcr0)])); // <v-lp> <in> <damp> <lc> <chk>
        FMUL(st0, st2);                             // <d*(v-lp)> <in> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcr0)])); // <dout> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcr0)])); // <dout> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecr0) + 4 * edx.data])); // <asumr> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencr0);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscr0)], edx);

        // right comb 1
        MOV(edx, rbp[offsetof(syWReverb, poscr1)]);
        FLD(dword(rbp[offsetof(syWReverb, linecr1) + 4 * edx.data])); // <dv> <asumr> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc1)])); // <dv'> <asumr> <in> <damp> <lc> <chk>
        FSUB(st0, st2);                             // <nv>  <asumr> <in> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcr1)])); // <v-lp> <asumr> <in> <damp> <lc> <chk>
        FMUL(st0, st3);                             // <d*(v-lp)> <asumr> <in> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcr1)])); // <dout> <asumr> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcr1)])); // <dout> <asumr> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecr1) + 4 * edx.data])); // <dout> <asumr> <in> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <asumr'> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencr1);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscr1)], edx);

        // right comb 2
        MOV(edx, rbp[offsetof(syWReverb, poscr2)]);
        FLD(dword(rbp[offsetof(syWReverb, linecr2) + 4 * edx.data])); // <dv> <asumr> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc2)])); // <dv'> <asumr> <in> <damp> <lc> <chk>
        FADD(st0, st2);                             // <nv>  <asumr> <in> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcr2)])); // <v-lp> <asumr> <in> <damp> <lc> <chk>
        FMUL(st0, st3);                             // <d*(v-lp)> <asumr> <in> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcr2)])); // <dout> <asumr> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcr2)])); // <dout> <asumr> <in> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecr2) + 4 * edx.data])); // <dout> <asumr> <in> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <asumr'> <in> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencr2);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscr2)], edx);

        // right comb 3
        MOV(edx, rbp[offsetof(syWReverb, poscr3)]);
        FLD(dword(rbp[offsetof(syWReverb, linecr3) + 4 * edx.data])); // <dv> <asumr> <in> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gainc3)])); // <dv'> <asumr> <in> <damp> <lc> <chk>
        FSUBRP(st2, st0);                           // <asumr> <nv> <damp> <lc> <chk>
        FXCH(st1);                             // <nv> <asumr> <damp> <lc> <chk>
        FSUB(dword(rbp[offsetof(syWReverb, lpfcr3)])); // <v-lp> <asumr> <damp> <lc> <chk>
        FMUL(st0, st2);                             // <d*(v-lp)> <asumr> <damp> <lc> <chk>
        FADD(dword(rbp[offsetof(syWReverb, lpfcr3)])); // <dout> <asumr> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, lpfcr3)])); // <dout> <asumr> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linecr3) + 4 * edx.data])); // <dout> <asumr> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <asumr'> <damp> <lc> <chk>
        INC(edx);
        CMP(dx, lencr3);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, poscr3)], edx);

        // step 2b: process the 2 right allpass delays
        // right allpass 0
        MOV(edx, rbp[offsetof(syWReverb, posar0)]);
        FLD(dword(rbp[offsetof(syWReverb, linear0) + 4 * edx.data])); // <d0v> <asumr> <damp> <lc> <chk>
        FLD(st0);                                   // <d0v> <d0v> <asumr> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina0)])); // <d0v'> <d0v> <asumr> <damp> <lc> <chk>
        FADDP(st2, st0);                            // <d0v> <d0z> <damp> <lc> <chk>
        FXCH(st1);                             // <d0z> <d0v> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linear0) + 4 * edx.data]));
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina0)])); // <d0z'> <d0v> <damp> <lc> <chk>
        FSUBP(st1, st0);                            // <d0o> <damp> <lc> <chk>
        INC(edx);
        CMP(dl, lenar0);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, posar0)], edx);

        // right allpass 1
        MOV(edx, rbp[offsetof(syWReverb, posar1)]);
        FLD(dword(rbp[offsetof(syWReverb, linear1) + 4 * edx.data])); // <d1v> <d0o> <damp> <lc> <chk>
        FLD(st0);                                   // <d1v> <d1v> <d0o> <damp> <lc> <chk>
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina1)])); // <d1v'> <d1v> <d0o> <damp> <lc> <chk>
        FADDP(st2, st0);                            // <d1v> <d1z> <damp> <lc> <chk>
        FXCH(st1);                             // <d1z> <d1v> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, linear1) + 4 * edx.data]));
        FMUL(dword(rbp[offsetof(syWReverb, setup) + offsetof(syCReverb, gaina1)])); // <d1z'> <d1v> <damp> <lc> <chk>
        FSUBP(st1, st0);                            // <d1o> <damp> <lc> <chk>
        INC(edx);
        CMP(dl, lenar1);
        CMOVE(edx, ebx);
        MOV(rbp[offsetof(syWReverb, posar1)], edx);

        // step 2c: low cut
        FLD(dword(rbp[offsetof(syWReverb, hpfcr)])); // <hpf> <d1o> <damp> <lc> <chk>
        FLD(st0);                                   // <hpf> <hpf> <d1o>  <damp> <lc> <chk>
        FSUBR(st0, st2);                            // <d1o-hpf> <hpf> <d1o> <damp> <lc> <chk>
        FMUL(st0, st4);                             // <lc(d1o-hpf)> <hpf> <d1o> <damp> <lc> <chk>
        FADDP(st1, st0);                            // <hpf'> <d1o> <damp> <lc> <chk>
        FST(dword(rbp[offsetof(syWReverb, hpfcr)]));
        FSUBP(st1, st0);                            // <outr> <damp> <lc> <chk>

        // step 2d: update left mixing buffer
        FADD(dword(rdi[4]));
        FSTP(dword(rdi[4]));                        // <damp> <lc> <chk>

        rdi.data += 8; // lea edi, [edi+8]
        DEC(ecx);
        if (JNZ())
            goto sloop;

        FSTP(st0);                                  // <lc> <chk>
        FSTP(st0);                                  // <chk>

        // FPU underflow protection
        FSTSW(ax);
        AND(eax, 16);
        if (JZ())
            goto dontpanic;
        syReverbReset();

    dontpanic:

        POPAD();
    }

    //#####################################################################################
    // Oberdings
    // CHANNELS RULEN
    //#####################################################################################

    // rbp: wchan
    void SynthAsm::syChanInit()
    {
        PUSHAD();
        rbp.data += offsetof(syWChan, distw); // lea ebp, rbp[offsetof(syWChan,distw]
        syDistInit();
        rbp.data += offsetof(syWChan, chrw) - offsetof(syWChan, distw); // lea ebp, rbp[offsetof(syWChan,chrw - syWChan.distw]
        syModDelInit();
        rbp.data += offsetof(syWChan, compw) - offsetof(syWChan, chrw); // lea ebp, rbp[offsetof(syWChan,compw - syWChan.chrw]
        syCompInit();
        rbp.data += offsetof(syWChan, boostw) - offsetof(syWChan, compw); // ebp, rbp[offsetof(syWChan,boostw - syWChan.compw]
        syBoostInit();
        rbp.data += offsetof(syWChan, dcf1w) - offsetof(syWChan, boostw); // ebp, rbp[offsetof(syWChan,dcf1w - syWChan.boostw]
        syDCFInit();
        rbp.data += offsetof(syWChan, dcf2w) - offsetof(syWChan, dcf1w); // ebp, rbp[offsetof(syWChan,dcf2w - syWChan.dcf1w]
        syDCFInit();
        POPAD();
    }

    // esi: vchan
    // ebp: wchan
    void SynthAsm::syChanSet()
    {
        PUSHAD();
        FLD(dword(rsi[offsetof(syVChan, auxarcv)]));
        FMUL(dword(m_fci128));
        FSTP(dword(rbp[offsetof(syWChan, aarcv)]));
        FLD(dword(rsi[offsetof(syVChan, auxbrcv)]));
        FMUL(dword(m_fci128));
        FSTP(dword(rbp[offsetof(syWChan, abrcv)]));
        FLD(dword(rsi[offsetof(syVChan, auxasnd)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fcgain));
        FSTP(dword(rbp[offsetof(syWChan, aasnd)]));
        FLD(dword(rsi[offsetof(syVChan, auxbsnd)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fcgain));
        FSTP(dword(rbp[offsetof(syWChan, absnd)]));
        FLD(dword(rsi[offsetof(syVChan, chanvol)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fcgain));
        FST(dword(rbp[offsetof(syWChan, chgain)]));
        FLD(dword(rsi[offsetof(syVChan, aux1)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fcgainh));
        FMUL(st0, st1);
        FSTP(dword(rbp[offsetof(syWChan, a1gain)]));
        FLD(dword(rsi[offsetof(syVChan, aux2)]));
        FMUL(dword(m_fci128));
        FMUL(dword(m_fcgainh));
        FMULP(st1, st0);
        FSTP(dword(rbp[offsetof(syWChan, a2gain)]));
        FLD(dword(rsi[offsetof(syVChan, fxroute)]));
        FISTP(dword(rbp[offsetof(syWChan, fxr)]));
        rsi.data += offsetof(syVChan, cdist); // lea esi, [esi + syVChan.cdist]
        rbp.data += offsetof(syWChan, distw); // lea ebp, [ebp + syWChan.distw]
        syDistSet();
        rsi.data += offsetof(syVChan, chorus) - offsetof(syVChan, cdist); // lea esi, [esi + syVChan.chorus - syVChan.cdist]
        rbp.data += offsetof(syWChan, chrw) - offsetof(syWChan, distw); // lea ebp, [ebp + syWChan.chrw - syWChan.distw]
        syModDelSet();
        rsi.data += offsetof(syVChan, compr) - offsetof(syVChan, chorus); // lea esi, [esi + syVChan.compr - syVChan.chorus]
        rbp.data += offsetof(syWChan, compw) - offsetof(syWChan, chrw); // lea ebp, [ebp + syWChan.compw - syWChan.chrw]
        syCompSet();
        rsi.data += offsetof(syVChan, boost) - offsetof(syVChan, compr); // lea esi, [esi + syVChan.boost - syVChan.compr]
        rbp.data += offsetof(syWChan, boostw) - offsetof(syWChan, compw); // lea ebp, [ebp + syWChan.boostw - syWChan.compw]
        syBoostSet();
        POPAD();
    }

    void SynthAsm::syChanProcess()
    {
        PUSHAD();

        MOV(rbx, &m_this);
        ADD(rbx, offsetof(SYN, chanbuf));

        // AuxA Receive  CHAOS
        MOV(eax, rbp[offsetof(syWChan, aarcv)]);
        OR(eax, eax);
        if (JZ())
            goto noauxarcv;
        PUSH(ecx);
        MOV(rdi, rbx);
        rsi.data = rbx.data + offsetof(SYN, auxabuf) - offsetof(SYN, chanbuf); // lea esi, [ebx + SYN.auxabuf-SYN.chanbuf]
    auxarcvloop:
        FLD(dword(rsi[0]));
        FMUL(dword(rbp[offsetof(syWChan, aarcv)]));
        FLD(dword(rsi[4]));
        FMUL(dword(rbp[offsetof(syWChan, aarcv)]));
        rsi.data += 8; // lea esi, [esi + 8]
        FXCH(st1);
        FADD(dword(rdi[0]));
        FXCH(st1);
        FADD(dword(rdi[4]));
        FXCH(st1);
        FSTP(dword(rdi[0]));
        FSTP(dword(rdi[4]));
        rdi.data += 8;// lea edi, [edi + 8]
        DEC(ecx);
        if (JNZ())
            goto auxarcvloop;
        POP(ecx);
    noauxarcv:

        // AuxB Receive  CHAOS
        MOV(eax, rbp[offsetof(syWChan, abrcv)]);
        OR(eax, eax);
        if (JZ())
            goto noauxbrcv;
        PUSH(ecx);
        MOV(rdi, rbx);
        rsi.data = rbx.data + offsetof(SYN, auxbbuf) - offsetof(SYN, chanbuf); // lea esi, [ebx + SYN.auxbbuf-SYN.chanbuf]
    auxbrcvloop:
        FLD(dword(rsi[0]));
        FMUL(dword(rbp[offsetof(syWChan, abrcv)]));
        FLD(dword(rsi[4]));
        FMUL(dword(rbp[offsetof(syWChan, abrcv)]));
        rsi.data += 8; // lea esi, [esi + 8]
        FXCH(st1);
        FADD(dword(rdi[0]));
        FXCH(st1);
        FADD(dword(rdi[4]));
        FXCH(st1);
        FSTP(dword(rdi[0]));
        FSTP(dword(rdi[4]));
        rdi.data += 8; // lea edi, [edi + 8]
        DEC(ecx);
        if (JNZ())
            goto auxbrcvloop;
        POP(ecx);
    noauxbrcv:

        MOV(rsi, rbx);
        MOV(rdi, rsi);

        rbp.data += offsetof(syWChan, dcf1w) - 0;// lea ebp, [ebp + syWChan.dcf1w - 0]
        syDCFRenderStereo();
        rbp.data += offsetof(syWChan, compw) - offsetof(syWChan, dcf1w); // lea ebp, [ebp + syWChan.compw - syWChan.dcf1w]
        syCompRender();
        rbp.data += offsetof(syWChan, boostw) - offsetof(syWChan, compw); // lea ebp, [ebp + syWChan.boostw - syWChan.compw]
        syBoostRender();
        rbp.data += 0 - offsetof(syWChan, boostw); // lea ebp, [ebp + 0 - syWChan.boostw]

        MOV(eax, rbp[offsetof(syWChan, fxr)]);
        OR(eax, eax);
        if (JNZ())
            goto otherway;
        rbp.data += offsetof(syWChan, distw); // lea ebp, [ebp - 0 + syWChan.distw]
        syDistRenderStereo();
        rbp.data += offsetof(syWChan, dcf2w) - offsetof(syWChan, distw); // lea ebp, [ebp - syWChan.distw + syWChan.dcf2w]
        syDCFRenderStereo();
        rbp.data += offsetof(syWChan, chrw) - offsetof(syWChan, dcf2w); // lea ebp, [ebp - syWChan.dcf2w + syWChan.chrw]
        syModDelRenderChan();
        rbp.data += 0 - offsetof(syWChan, chrw); // lea ebp, [ebp - syWChan.chrw + 0]
        goto weiterso;
    otherway:
        rbp.data += offsetof(syWChan, chrw); // lea ebp, [ebp + syWChan.chrw - 0]
        syModDelRenderChan();
        rbp.data += offsetof(syWChan, distw) - offsetof(syWChan, chrw); // lea ebp, [ebp + syWChan.distw - syWChan.chrw]
        syDistRenderStereo();
        rbp.data += offsetof(syWChan, dcf2w) - offsetof(syWChan, distw); // lea ebp, [ebp - syWChan.distw + syWChan.dcf2w]
        syDCFRenderStereo();
        rbp.data += 0 - offsetof(syWChan, dcf2w); // lea ebp, [ebp + 0 - syWChan.dcf2w]

    weiterso:

        // Aux1...
        MOV(eax, rbp[offsetof(syWChan, a1gain)]);
        OR(eax, eax);
        if (JZ())
            goto noaux1;

        PUSH(ecx);
        PUSH(esi);
        rdi.data = rbx.data + offsetof(SYN, aux1buf) - offsetof(SYN, chanbuf); // lea edi, [ebx + SYN.aux1buf-SYN.chanbuf]
    aux1loop:
        FLD(dword(rsi[0]));
        FADD(dword(rsi[4]));
        rsi.data += 8; // lea esi, [esi+8]
        FMUL(dword(rbp[offsetof(syWChan, a1gain)]));
        FADD(dword(rdi[0]));
        FSTP(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi + 4]
        DEC(ecx);
        if (JNZ())
            goto aux1loop;
        POP(esi);
        POP(ecx);

    noaux1:

        // ... und Aux2.
        MOV(eax, rbp[offsetof(syWChan, a2gain)]);
        OR(eax, eax);
        if (JZ())
            goto noaux2;
        PUSH(ecx);
        PUSH(esi);
        rdi.data = rbx.data + offsetof(SYN, aux2buf) - offsetof(SYN, chanbuf); // lea edi, [ebx + SYN.aux2buf-SYN.chanbuf]
    aux2loop:
        FLD(dword(rsi[0]));
        FADD(dword(rsi[4]));
        rsi.data += 8; // lea esi, [esi+8]
        FMUL(dword(rbp[offsetof(syWChan, a2gain)]));
        FADD(dword(rdi[0]));
        FSTP(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi+4]
        DEC(ecx);
        if (JNZ())
            goto aux2loop;
        POP(esi);
        POP(ecx);

    noaux2:

        // AuxA...  CHAOS
        MOV(eax, rbp[offsetof(syWChan, aasnd)]);
        OR(eax, eax);
        if (JZ())
            goto noauxa;
        PUSH(ecx);
        PUSH(esi);
        rdi.data = rbx.data + offsetof(SYN, auxabuf) - offsetof(SYN, chanbuf); // lea edi, [ebx + SYN.auxabuf-SYN.chanbuf]
    auxaloop:
        FLD(dword(rsi[0]));
        FMUL(dword(rbp[offsetof(syWChan, aasnd)]));
        FLD(dword(rsi[4]));
        FMUL(dword(rbp[offsetof(syWChan, aasnd)]));
        rsi.data += 8; // lea esi, [esi + 8]
        FXCH(st1);
        FADD(dword(rdi[0]));
        FXCH(st1);
        FADD(dword(rdi[4]));
        FXCH(st1);
        FSTP(dword(rdi[0]));
        FSTP(dword(rdi[4]));
        rdi.data += 8; // lea edi, [edi + 8]
        DEC(ecx);
        if (JNZ())
            goto auxaloop;
        POP(esi);
        POP(ecx);
    noauxa:

        // AuxB...  CHAOS
        MOV(eax, rbp[offsetof(syWChan, absnd)]);
        OR(eax, eax);
        if (JZ())
            goto noauxb;
        PUSH(ecx);
        PUSH(esi);
        rdi.data = rbx.data + offsetof(SYN, auxbbuf) - offsetof(SYN, chanbuf); // lea edi, [ebx + SYN.auxbbuf-SYN.chanbuf]
    auxbloop:
        FLD(dword(rsi[0]));
        FMUL(dword(rbp[offsetof(syWChan, absnd)]));
        FLD(dword(rsi[4]));
        FMUL(dword(rbp[offsetof(syWChan, absnd)]));
        rsi.data += 8; // lea esi, [esi + 8]
        FXCH(st1);
        FADD(dword(rdi[0]));
        FXCH(st1);
        FADD(dword(rdi[4]));
        FXCH(st1);
        FSTP(dword(rdi[0]));
        FSTP(dword(rdi[4]));
        rdi.data += 8; // lea edi, [edi + 8]
        DEC(ecx);
        if (JNZ())
            goto auxbloop;
        POP(esi);
        POP(ecx);
    noauxb:

        // Chanbuf in Mainbuffer kopieren
        rdi.data = rbx.data + offsetof(SYN, mixbuf) - offsetof(SYN, chanbuf); // lea edi, [ebx + SYN.mixbuf-SYN.chanbuf]
    ccloop:
        FLD(dword(rsi[0]));
        FMUL(dword(rbp[offsetof(syWChan, chgain)]));
        FLD(dword(rsi[4]));
        FMUL(dword(rbp[offsetof(syWChan, chgain)]));
        rsi.data += 8; // lea esi, [esi + 8]
        FXCH(st1);
        FADD(dword(rdi[0]));
        FXCH(st1);
        FADD(dword(rdi[4]));
        FXCH(st1);
        FSTP(dword(rdi[0]));
        FSTP(dword(rdi[4]));
        rdi.data += 8; // lea edi, [edi + 8]
        DEC(ecx);
        if (JNZ())
            goto ccloop;

        POPAD();
    }

    void SynthAsm::storeChanValues() // ebx: channel, rbp : globals
    {
        PUSHAD();

        MOVZX(eax, byte(rbp[offsetof(SYN, chans) + 8 * ebx.data])); // pgmnummer
        MOV(rdi, rbp[offsetof(SYN, patchmap)]);
        rdi.data = *reinterpret_cast<uint32_t*>(rdi[4 * eax.data]); // mov   edi, [edi + 4*eax] ; edi->sounddaten
        rdi.data += *reinterpret_cast<uint64_t*>(rbp[offsetof(SYN, patchmap)]); // add edi, [ebp + SYN.patchmap]

        MOV(eax, ebx);
        IMUL(eax, sizeof(syVChan));
        rsi.data = rbp.data + offsetof(SYN, chansv) + eax.data; // lea esi, [ebp + SYN.chansv + eax] ; esi -> values
        MOV(eax, ebx);
        IMUL(eax, sizeof(syWChan));
        rax.data = rbp.data + offsetof(SYN, chansw) + eax.data; // lea eax, [ebp + SYN.chansw + eax] ; eax -> workspace
        PUSH(eax);

        // channeldependent daten ²bertragen
        PUSH(esi);
        XOR(ecx, ecx);
        MOV(cl, offsetof(v2sound, chan));
    goloop:
        MOVZX(eax, byte(rdi[ecx.data]));
        MOV(m_temp, eax);
        FILD(dword(m_temp));
        FSTP(dword(rsi[0]));
        rsi.data += 4; // lea esi, [esi+4]
        INC(ecx);
        CMP(cl, v2sound_endcdvals);
        if (JNE())
            goto goloop;
        POP(esi);

        // rbp: ptr auf voice
        MOV(eax, rbp[offsetof(SYN, voicemap) + 4 * ebx.data]);
        IMUL(eax, sizeof(syWV2));
        rbp.data += offsetof(SYN, voicesw) + eax.data; // lea ebp, [ebp + SYN.voicesw + eax] ; ebp -> workspace

        // MODMATRIX!
        MOVZX(ecx, byte(rdi[offsetof(v2sound, modnum)]));
        rdi.data += offsetof(v2sound, modmatrix); // lea edi, [edi + v2sound.modmatrix]
        OR(ecx, ecx);
        if (JNZ())
            goto modloop;
        goto modend;

    modloop:
        MOVZX(eax, byte(rdi[offsetof(v2mod, dest)]));
        CMP(al, offsetof(v2sound, chan));
        if (JB())
            goto mlegw;
        CMP(al, v2sound_endcdvals);
        if (JB())
            goto mok;
    mlegw:
        goto mlend;
    mok:
        MOVZX(eax, byte(rdi[offsetof(v2mod, source)])); // source
        OR(eax, eax);
        if (JNZ())
            goto mnotvel;
        FLD(dword(rbp[offsetof(syWV2, velo)]));
        goto mdo;
    mnotvel:
        CMP(al, 8);
        if (JAE())
            goto mnotctl;
        PUSH(ebp);
        MOV(rbp, &m_this);
        rbp.data += offsetof(SYN, chans) + 8 * ebx.data; //lea ebp, [ebp + SYN.chans + 8*ebx]
        MOVZX(eax, byte(rbp[eax.data]));
        POP(ebp);
        MOV(m_temp, eax);
        FILD(dword(m_temp));
        goto mdo;
    mnotctl:
        CMP(al, 12);
        if (JAE())
            goto mnotvm;
        AND(al, 3);
        MOV(eax, sVTab + eax.data);
        FLD(dword(rbp[eax.data]));
        goto mdo;
    mnotvm:
        CMP(al, 13);
        if (JNE())
            goto mnotnote;
    mnotnote:
        FILD(dword(rbp[offsetof(syWV2, note)]));
        FSUB(dword(m_fc48));
        FADD(st0, st0);
    mdo:
        MOVZX(eax, byte(rdi[offsetof(v2mod, val)]));
        MOV(m_temp, eax);
        FILD(dword(m_temp));
        FSUB(dword(m_fc64));
        FMUL(dword(m_fci128));
        FADD(st0, st0);
        FMULP(st1, st0);
        MOVZX(eax, byte(rdi[offsetof(v2mod, dest)]));
        SUB(al, offsetof(v2sound, chan));
        FADD(dword(rsi[4 * eax.data]));
        FSTP(dword(m_temp));
        // clippen
        MOV(edx, m_temp);
        OR(edx, edx);
        if (JNS())
            goto mnoclip1;
        XOR(edx, edx);
    mnoclip1:
        CMP(edx, 0x43000000);
        if (JBE())
            goto mnoclip2;
        MOV(edx, 0x43000000);
    mnoclip2:
        MOV(rsi[4 * eax.data], edx);
    mlend:
        rdi.data += 3; // lea edi, [edi+3]
        DEC(ecx);
        if (JZ())
            goto modend;
        goto modloop;
    modend:

        POP(ebp);
        syChanSet();

    // end:
        POPAD();
    }

    //#####################################################################################
    //
    // RONAN, DER FREUNDLICHE SPRECHROBOTER
    //
    //#####################################################################################

    // FILTERBANK:

#ifdef RONAN

    // rbp: this
    void SynthAsm::syRonanInit()
    {
        // PUSHAD();
        // mov  eax, [this]
        // add  eax, SYN.ronanw
        // push eax
        // call _ronanCBInit@4
        // POPAD();
        ronanCBInit(reinterpret_cast<syWRonan*>(m_this + offsetof(SYN,ronanw)));
    }

    // rbp: this
    void SynthAsm::syRonanNoteOn()
    {
        // PUSHAD();
        // mov  eax, [this]
        // add  eax, SYN.ronanw
        // push eax
        // call _ronanCBNoteOn@4
        // POPAD();
        ronanCBNoteOn(reinterpret_cast<syWRonan*>(m_this + offsetof(SYN, ronanw)));
    }

    // rbp: this
    void SynthAsm::syRonanNoteOff()
    {
        // PUSHAD();
        // mov  eax, [this]
        // add  eax, SYN.ronanw
        // push eax
        // call _ronanCBNoteOff@4
        // POPAD();
        ronanCBNoteOff(reinterpret_cast<syWRonan*>(m_this + offsetof(SYN, ronanw)));
    }

    // rbp: this
    void SynthAsm::syRonanTick()
    {
        // PUSHAD();
        // mov  eax, [this]
        // add  eax, SYN.ronanw
        // push eax
        // call _ronanCBTick@4
        // POPAD();
        ronanCBTick(reinterpret_cast<syWRonan*>(m_this + offsetof(SYN, ronanw)));
    }

    // ebp: this
    // esi: buffer
    // ecx: count
    void SynthAsm::syRonanProcess()
    {
        // PUSHAD();
        // push    ecx
        // push    esi
        // mov  eax, [this]
        // add  eax, SYN.ronanw
        // push eax
        // call    _ronanCBProcess@12
        // POPAD();
        ronanCBProcess(reinterpret_cast<syWRonan*>(m_this + offsetof(SYN, ronanw)), reinterpret_cast<float*>(rsi.data), ecx.data);
    }

#endif

    //#####################################################################################
    // Oberdings
    // ES GEHT LOS
    //#####################################################################################

    //-------------------------------------------------------------------------------------
    // Init

    void SynthAsm::synthInit(const void* patchmap, int samplerate)
    {
        m_fcsamplesperms = samplerate / 1000.0f;

        PUSHAD();

        rbp.data = reinterpret_cast<uint64_t>(m_mem); // ebp, [esp + 36]
        m_this = rbp.data; // mov [this], ebp

        XOR(eax, eax);
        MOV(ecx, sizeof(SYN));
        MOV(rdi, rbp);
        REP_STOSB();

        MOV(eax, &samplerate);// mov eax, [esp + 44]
        MOV(dword(rbp[offsetof(SYN, samplerate)]), eax);
        calcNewSampleRate();

#ifdef RONAN
        // PUSHAD();
        // mov eax, [ebp + SYN.samplerate]
        // push eax
        // lea  eax, [ebp + SYN.ronanw]
        // push eax
        // call _ronanCBSetSR@8
        // POPAD();
        ronanCBSetSR(reinterpret_cast<syWRonan*>(rbp.data + offsetof(SYN, ronanw)), samplerate);
#endif

        MOV(rax, &patchmap); // mov eax, [esp + 40]
        MOV(rbp[offsetof(SYN, patchmap)], rax);

        MOV(cl, POLY);
        XOR(eax, eax);
        NOT(eax);
        rdi.data = rbp.data + offsetof(SYN, chanmap); // lea edi, [ebp + SYN.chanmap]
        rbp.data += offsetof(SYN, voicesw); // lea ebp, [ebp + SYN.voicesw]
    siloop1:
        STOSD();
        syV2Init();
        rbp.data += sizeof(syWV2); // lea ebp, [ebp+syWV2.size]
        DEC(ecx);
        if (JNZ())
            goto siloop1;

        MOV(cl, static_cast<uint8_t>(0));
        MOV(rbx, &m_this);
        rbp.data = rbx.data + offsetof(SYN, chansw); // lea ebp, [ebx + SYN.chansw]
    siloop2:
        MOV(byte(rbx[offsetof(SYN, chans) + 8 * ecx.data + 7]), 0x7f);
        MOV(eax, ecx);
        SHL(eax, 14);
        rax.data = rbx.data + offsetof(SYN, chandelbuf) + eax.data; // lea eax, [ebx + SYN.chandelbuf + eax]
        MOV(rbp[offsetof(syWChan, chrw) + offsetof(syWModDel, db1)], rax);
        ADD(eax, 8192);
        MOV(rbp[offsetof(syWChan, chrw) + offsetof(syWModDel, db2)], rax);
        MOV(eax, 2047);
        MOV(rbp[offsetof(syWChan, chrw) + offsetof(syWModDel, dbufmask)], eax);
        syChanInit();
        rbp.data += sizeof(syWChan);// lea ebp, [ebp+syWChan.size]
        INC(ecx);
        CMP(cl, 16);
        if (JNE())
            goto siloop2;

        rbp.data = rbx.data + offsetof(SYN, reverb); // lea ebp, [ebx + SYN.reverb]
        syReverbInit();

        rbp.data = rbx.data + offsetof(SYN, delay); // lea ebp, [ebx + SYN.delay]
        rax.data = rbx.data + offsetof(SYN, maindelbuf); // lea eax, [ebx + SYN.maindelbuf]
        MOV(rbp[offsetof(syWModDel, db1)], rax);
        ADD(rax, 131072);
        MOV(rbp[offsetof(syWModDel, db2)], rax);
        MOV(eax, 32767);
        MOV(rbp[offsetof(syWModDel, dbufmask)], eax);
        syModDelInit();

#ifdef RONAN
        syRonanInit();
#endif

        rbp.data = rbx.data + offsetof(SYN, compr); // lea ebp, [ebx + SYN.compr]
        syCompInit();
        rbp.data = rbx.data + offsetof(SYN, dcf); // lea ebp, [ebx + SYN.dcf]
        syDCFInit();

        POPAD();
    }

    //-------------------------------------------------------------------------------------
    // Renderloop!

    void SynthAsm::synthRender(void* buf, int smp, void* buf2, int add)
    {
        PUSHAD();

        rbp.data = reinterpret_cast<uint64_t>(m_mem); // ebp, [esp + 36]
        m_this = rbp.data; // mov [this], ebp

        // FPUsetup...
        // fstcw [oldfpcw]
        // mov  ax, [oldfpcw]
        // and  ax, 0f0ffh
        // or  ax, 3fh
        // mov  [temp], ax
        // finit
        // fldcw [temp]

        MOV(ecx, &smp); // mov   ecx, [esp+44]
        MOV(m_todo, ecx);
        MOV(edi, &add); // mov   edi, [esp+52]
        MOV(&m_addflg, edi);
        MOV(rdi, &buf2); // mov   edi, [esp+48]
        MOV(m_outptr + 1, rdi);
        MOV(rdi, &buf); // mov   edi, [esp+40]
        MOV(m_outptr, rdi);

    fragloop:
        // step 1: fragmentieren
        MOV(eax, m_todo);
        OR(eax, eax);
        if (JNZ())
            goto doit;

        // fldcw [oldfpcw] ; nix zu tun? -> ende
        POPAD();
        return; // ret 20

    doit:
        // neuer Frame n²tig?
        MOV(ebx, rbp[offsetof(SYN, tickd)]);
        OR(ebx, ebx);
        if (JNZ())
            goto nonewtick;

        // tick abspielen
        XOR(edx, edx);
    tickloop:
        MOV(eax, rbp[offsetof(SYN, chanmap) + 4 * edx.data]);
        OR(eax, eax);
        if (JS())
            goto tlnext;
        MOV(eax, edx);
        IMUL(eax, sizeof(syWV2));
        storeV2Values();
        rbp.data += offsetof(SYN, voicesw) + eax.data; // lea ebp, [ebp + SYN.voicesw + eax]
        syV2Tick();
        // checken ob EG ausgelaufen
        MOV(eax, rbp[offsetof(syWV2, aenv) + offsetof(syWEnv, state)]);
        rbp.data = m_this;//mov   ebp, [this]
        OR(eax, eax);
        if (JNZ())
            goto stillok;
        // wenn ja -> ausmachen!
        NOT(eax);
        MOV(rbp[offsetof(SYN, chanmap) + 4 * edx.data], eax);
        goto tlnext;
    stillok:
    tlnext:
        INC(edx);
        CMP(dl, POLY);
        if (JNE())
            goto tickloop;

        XOR(ebx, ebx);
    tickloop2:
        storeChanValues();
        INC(ebx);
        CMP(bl, 16);
        if (JNE())
            goto tickloop2;

#ifdef RONAN
        syRonanTick();
#endif

        // frame rendern
        MOV(ecx, &m_SRcFrameSize);
        MOV(rbp[offsetof(SYN, tickd)], ecx);
        RenderBlock();

    nonewtick:

        // daten in destination umkopieren
        MOV(eax, m_todo);
        MOV(ebx, rbp[offsetof(SYN, tickd)]);
        MOV(ecx, ebx);

        MOV(esi, &m_SRcFrameSize);
        SUB(esi, ecx);
        SHL(esi, 3);
        rsi.data = rbp.data + offsetof(SYN, mixbuf) + esi.data; // lea esi, [ebp + SYN.mixbuf + esi]

        CMP(eax, ecx);
        if (JGE())
            goto tickdg;
        MOV(ecx, eax);
    tickdg:
        SUB(eax, ecx);
        SUB(ebx, ecx);
        MOV(m_todo, eax);
        MOV(rbp[offsetof(SYN, tickd)], ebx);

        MOV(eax, &m_addflg);
        MOV(rdi, m_outptr);
        MOV(rbx, m_outptr + 1);
        OR(ebx, ebx);
        if (JZ())
            goto interleaved;

        // separate channels
        OR(eax, eax);
        if (JZ())
            goto seploopreplace;

    seploopadd:
        FLD(dword(rbx[0]));
        FLD(dword(rdi[0]));
        FADD(dword(rsi[0]));
        FSTP(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi+4]
        FADD(dword(rsi[4]));
        rsi.data += 8; // lea esi, [esi+8]
        FSTP(dword(rbx[0]));
        rbx.data += 4; // lea ebx, [ebx+4]
        DEC(ecx);
        if (JNZ())
            goto seploopadd;
        if (JZ())
            goto seploopend;

    seploopreplace:
        MOV(eax, rsi[0]);
        MOV(edx, rsi[4]);
        rsi.data += 8; // lea esi, [esi+8]
        MOV(rdi[0], eax);
        rdi.data += 4; // lea edi, [edi+4]
        MOV(rbx[0], edx);
        rbx.data += 4; // lea ebx, [ebx+4]
        DEC(ecx);
        if (JNZ())
            goto seploopreplace;

    seploopend:
        MOV(m_outptr, rdi);
        MOV(m_outptr + 1, rbx);
        goto fragloop;

    interleaved:
        SHL(ecx, 1);
        OR(eax, eax);
        if (JZ())
            goto intloopreplace;

    intloopadd:
        FLD(dword(rdi[0]));
        FADD(dword(rsi[0]));
        rsi.data += 4; // lea esi, [esi+4]
        FSTP(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi+4]
        DEC(ecx);
        if (JNZ())
            goto intloopadd;
        if (JZ())
            goto intloopend;

    intloopreplace:
        REP_MOVSD();

    intloopend:
        MOV(m_outptr, rdi);
        goto fragloop;
    }

    void SynthAsm::RenderBlock()
    {
        PUSHAD();

        // clear output buffer
        MOV(m_todo + 1, ecx);
        rdi.data = rbp.data + offsetof(SYN, mixbuf); // lea edi, [ebp + SYN.mixbuf]
        XOR(eax, eax);
        SHL(ecx, 1);
        REP_STOSD();

        // clear aux1
        MOV(ecx, m_todo + 1);
        rdi.data = rbp.data + offsetof(SYN, aux1buf); // lea edi, [ebp + SYN.aux1buf]
        REP_STOSD();

        // clear aux2
        MOV(ecx, m_todo + 1);
        rdi.data = rbp.data + offsetof(SYN, aux2buf); // lea edi, [ebp + SYN.aux2buf]
        REP_STOSD();

        // clear aux a / b
        MOV(ecx, m_todo + 1);
        ADD(ecx, ecx);
        PUSH(ecx);//MOV(eax, ecx);
        rdi.data = rbp.data + offsetof(SYN, auxabuf); // // lea edi, [ebp + SYN.auxabuf]
        REP_STOSD();
        POP(ecx);//MOV(ecx, eax);
        rdi.data = rbp.data + offsetof(SYN, auxbbuf); // // lea edi, [ebp + SYN.auxbbuf]
        REP_STOSD();

        // process all channels
        XOR(ecx, ecx);
    chanloop:
        PUSH(ecx);
        // check if voices are active
        XOR(edx, edx);
    cchkloop:
        CMP(ecx, rbp[offsetof(SYN, chanmap) + 4 * edx.data]);
        if (JE())
            goto dochan;
        INC(edx);
        CMP(dl, POLY);
        if (JNE())
            goto cchkloop;
#ifdef VUMETER
        XOR(eax, eax);
        MOV(rbp[offsetof(SYN, chanvu) + 8 * ecx.data], eax);
        MOV(rbp[offsetof(SYN, chanvu) + 8 * ecx.data + 4], eax);
#endif
        goto chanend;
    dochan:
        // clear channel buffer
        rdi.data = rbp.data + offsetof(SYN, chanbuf); // lea edi, [ebp + SYN.chanbuf]
        MOV(ecx, m_todo + 1);
        SHL(ecx, 1);
        XOR(eax, eax);
        REP_STOSD();

        // process all voices belonging to that channel
        XOR(edx, edx);
    vceloop:
        POP(ecx);
        PUSH(ecx);
        CMP(ecx, rbp[offsetof(SYN, chanmap) + 4 * edx.data]);
        if (JNE())
            goto vceend;

        // alle Voices rendern
        MOV(eax, edx);
        IMUL(eax, sizeof(syWV2));
        rbp.data += offsetof(SYN, voicesw) + eax.data; // lea ebp, [ebp + SYN.voicesw + eax]
        MOV(ecx, m_todo + 1);
        syV2Render();
        MOV(rbp, &m_this);

    vceend:
        INC(edx);
        CMP(dl, POLY);
        if (JNE())
            goto vceloop;

#ifdef RONAN
        // Wenn channel 16, dann Sprachsynth!
        POP(ecx);
        PUSH(ecx);
        CMP(cl, 15);
        if (JNE())
            goto noronan;

        MOV(ecx, m_todo + 1);
        rsi.data = rbp.data + offsetof(SYN, chanbuf); // lea esi, [ebp + SYN.chanbuf]
        syRonanProcess();
    noronan:
#endif

        POP(ecx);
        PUSH(ecx);
        IMUL(ecx, sizeof(syWChan));
        rbp.data += offsetof(SYN, chansw) + ecx.data; // lea ebp, [ebp + SYN.chansw + ecx]
        MOV(ecx, m_todo + 1);
        syChanProcess();
        MOV(rbp, &m_this);

#ifdef VUMETER
        // VU-Meter der Channels bearbeiten
        POP(edx);
        PUSH(edx);
        rsi.data = rbp.data + offsetof(SYN, chanbuf); //lea esi, [ebp + SYN.chanbuf]
        rdi.data = rbp.data + offsetof(SYN, chanvu) + 8 * edx.data; //lea edi, [ebp + SYN.chanvu + 8*edx]
        vumeter();
#endif

    chanend:
        POP(ecx);
        INC(ecx);
        CMP(cl, 16);
        if (JE())
            goto clend;
        goto chanloop;

    clend:

        // Reverb/Delay draufrechnen
        MOV(ecx, m_todo + 1);
        rdi.data = rbp.data + offsetof(SYN, mixbuf); // lea edi, [ebp + SYN.mixbuf]
        rbp.data += offsetof(SYN, reverb); // lea ebp, [ebp + SYN.reverb]
        syReverbProcess();
        rbp.data += offsetof(SYN, delay) - offsetof(SYN, reverb); // lea ebp, [ebp + SYN.delay - SYN.reverb]
        syModDelRenderAux2Main();
        MOV(ebp, &m_this);

        // DC filter
        rsi.data = rbp.data + offsetof(SYN, mixbuf); // lea esi, [ebp + SYN.mixbuf]
        MOV(rdi, rsi);
        MOV(ecx, m_todo + 1);
        rbp.data += offsetof(SYN, dcf); // lea ebp, [ebp + SYN.dcf]
        syDCFRenderStereo();
        rbp.data -= offsetof(SYN, dcf); // lea ebp, [ebp - SYN.dcf]

        // lowcut/highcut
        MOV(eax, rbp[offsetof(SYN, hcfreq)]);
        FLD(dword(rbp[offsetof(SYN, hcbufr)]));     // <hcbr>
        FLD(dword(rbp[offsetof(SYN, lcbufr)]));     // <lcbr> <hcbr>
        FLD(dword(rbp[offsetof(SYN, hcbufl)]));     // <hcbl> <lcbr> <hcbr>
        FLD(dword(rbp[offsetof(SYN, lcbufl)]));     // <lcbl> <hcbl> <lcbr> <hcbr>
    lhcloop:
        // low cut
        FLD(st0);                                   // <lcbl> <lcbl> <hcbl> <lcbr> <hcbr>
        FSUBR(dword(rsi[0]));                       // <in-l=lcoutl> <lcbl> <hcbl> <lcbr> <hcbr>
        FLD(st0);                                   // <in-l> <lcoutl> <lcbl> <hcbl> <lcbr> <hcbr>
        FMUL(dword(rbp[offsetof(SYN, lcfreq)]));    // <f*(in-l)> <lcoutl> <lcbl> <hcbl> <lcbr> <hcbr>
        FADDP(st2, st0);                            // <lcoutl> <lcbl'> <hcbl> <lcbr> <hcbr>
        FLD(st3);                                   // <lcbr> <lcoutl> <lcbl'> <hcbl> <lcbr> <hcbr>
        FSUBR(dword(rsi[4]));                       // <in-l=lcoutr> <lcoutl> <lcbl'> <hcbl> <lcbr> <hcbr>
        FLD(st0);                                   // <in-l> <lcoutr> <lcoutl> <lcbl'> <hcbl> <lcbr> <hcbr>
        FMUL(dword(rbp[offsetof(SYN, lcfreq)]));    // <f*(in-l)> <lcoutr> <lcoutl> <lcbl'> <hcbl> <lcbr> <hcbr>
        FADDP(st5, st0);                            // <lcoutr> <lcoutl> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FXCH(st1);                                  // <lcoutl> <lcoutr> <lcbl'> <hcbl> <lcbr'> <hcbr>
        CMP(eax, 0x3f800000);
        if (JE())
            goto nohighcut;
        // high cut
        FLD(st3);                                   // <hcbl> <lcoutl> <lcoutr> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FSUBP(st1, st0);                            // <inl-hcbl> <lcoutr> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FLD(st5);                                   // <hcbr> <inl-hcbl> <lcoutr> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FSUBP(st2, st0);                            // <inl-hcbl> <inr-hcbr> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FMUL(dword(rbp[offsetof(SYN, hcfreq)]));    // <f*(inl-l)> <inr-hcbr> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FXCH(st1);                                  // <inr-hcbr> <f*(inl-l)> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FMUL(dword(rbp[offsetof(SYN, hcfreq)]));    // <f*(inr-l)> <f*(inl-l)> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FXCH(st1);                                  // <f*(inl-l)> <f*(inr-l)> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FADDP(st3, st0);                            // <f*(inr-l)> <lcbl'> <hcbl'> <lcbr'> <hcbr>
        FADDP(st4, st0);                            // <lcbl'> <hcbl'> <lcbr'> <hcbr'>
        FLD(st3);                                   // <hcoutr> <lcbl'> <hcbl'> <lcbr'> <hcbr'>
        FLD(st2);                                   // <hcoutl> <hcoutr> <lcbl'> <hcbl'> <lcbr'> <hcbr'>
    nohighcut:
        FSTP(dword(rsi[0]));                        // <outr> <lcbl'> <hcbl> <lcbr'> <hcbr>
        FSTP(dword(rsi[4]));                        // <lcbl'> <hcbl> <lcbr'> <hcbr>
        rsi.data += 8; // lea esi, [esi+8]

        DEC(ecx);
        if (JNZ())
            goto lhcloop;

        FSTP(dword(rbp[offsetof(SYN, lcbufl)]));    // <hcbl> <lcbr> <hcbr>
        FSTP(dword(rbp[offsetof(SYN, hcbufl)]));    // <lcbr> <hcbr>
        FSTP(dword(rbp[offsetof(SYN, lcbufr)]));    // <hcbr>
        FSTP(dword(rbp[offsetof(SYN, hcbufr)]));    // -

        // Kompressor
        MOV(ecx, m_todo + 1);
        rdi.data = rbp.data + offsetof(SYN, mixbuf); // lea edi, [ebp + SYN.mixbuf]
        MOV(rsi, rdi);
        rbp.data += offsetof(SYN, compr); // lea ebp, [ebp + SYN.compr]
        syCompRender();
        rbp.data -= offsetof(SYN, compr); // lea ebp, [ebp - SYN.compr]

    // renderend:
#ifdef VUMETER
        MOV(ecx, m_todo + 1);
        rsi.data = rbp.data + offsetof(SYN, mixbuf); // lea esi, [ebp + SYN.mixbuf]
        rdi.data = rbp.data + offsetof(SYN, mainvu); // lea edi, [ebp + SYN.mainvu]
        vumeter();
#endif

        POPAD();
    }

    void SynthAsm::vumeter()
    {
#ifdef VUMETER
        // esi: srcbuf, ecx : len, edi : &outv
        PUSHAD();
        MOV(m_temp, ecx);
        MOV(eax, rbp[offsetof(SYN, vumode)]);
        if (JNZ())
            goto vurms;

        // peak vu meter
    vploop:
        MOV(eax, rsi[0]);
        AND(eax, 0x7fffffff);
        CMP(eax, rdi[0]);
        if (JBE())
            goto vplbe1;
        MOV(rdi[0], eax);
    vplbe1:
        MOV(eax, rsi[4]);
        AND(eax, 0x7fffffff);
        CMP(eax, rdi[4]);
        if (JBE())
            goto vplbe2;
        MOV(rdi[4], eax);
    vplbe2:
        FLD(dword(rdi[0]));
        FMUL(dword(m_fcvufalloff));
        FSTP(dword(rdi[0]));
        FLD(dword(rdi[4]));
        FMUL(dword(m_fcvufalloff));
        FSTP(dword(rdi[4]));
        rsi.data += 8; // lea esi, [esi+8]
        DEC(ecx);
        if (JNZ())
            goto vploop;
        POPAD();
        return;

    vurms:
        // rms vu meter
        FLDZ();                                     // <lsum>
        FLDZ();                                     // <rsum> <lsum>
    vrloop:
        FLD(dword(rsi[0]));                         // <l> <rsum> <lsum>
        FMUL(st0, st0);                             // <l²> <rsum> <lsum>
        FADDP(st2, st0);                            // <rsum> <lsum'>
        FLD(dword(rsi[4]));                         // <r> <rsum> <lsum>
        rsi.data += 8; // lea esi, [esi+8]
        FMUL(st0, st0);                             // <r²> <rsum> <lsum>
        FADDP(st1, st0);                            // <rsum'> <lsum'>
        DEC(ecx);
        if (JNZ())
            goto vrloop;                            // <rsum> <lsum>
        FLD1();                                     // <1> <rsum> <lsum>
        FIDIV(dword(m_temp));                       // <1/fl> <rsum> <lsum>
        FMUL(st2, st0);                             // <1/fl> <rsum> <lout>
        FMULP(st1, st0);                            // <rout> <lout>
        FXCH(st1);                                  // <lout> <rout>
        FSTP(dword(rdi[0]));                        // <rout>
        FSTP(dword(rdi[4]));                        // -
        POPAD();
#endif
    }

    // ------------------------------------------------------------------------
    // PROCESS MIDI MESSAGES

/*
spMTab dd ProcessNoteOff
             dd ProcessNoteOn
             dd ProcessPolyPressure
             dd ProcessControlChange
             dd ProcessProgramChange
             dd ProcessChannelPressure
             dd ProcessPitchBend
             dd ProcessRealTime
*/

    void SynthAsm::synthProcessMIDI(const void* ptr)
    {
        PUSHAD();

        rbp.data = reinterpret_cast<uint64_t>(m_mem); // ebp, [esp + 36]
        m_this = rbp.data; // mov [this], ebp

        // fstcw [oldfpcw]
        // mov  ax, [oldfpcw]
        // and  ax, 0f0ffh
        // or  ax, 3fh
        // mov  [temp], ax
        // finit
        // fldcw [temp]

        rsi.data = reinterpret_cast<uint64_t>(ptr); // mov esi, [esp + 40]
    mainloop:
        XOR(eax, eax);
        MOV(al, rsi[0]);
        TEST(al, 0x80);
        if (JZ())
            goto nocmd;
        CMP(al, static_cast<int8_t>(0xfd));         // end of stream ?
        if (JNE())
            goto noend;
    end:
        //fldcw [oldfpcw]
        POPAD();
        return;
    noend:

        MOV(rbp[offsetof(SYN, mrstat)], eax);       // update running status
        INC(rsi);
        MOV(al, rsi[0]);
    nocmd:
        MOV(ebx, rbp[offsetof(SYN, mrstat)]);
        MOV(ecx, ebx);
        AND(cl, 0x0f);
        SHR(ebx, 4);
        TEST(bl, 8);
        if (JZ())
            goto end;
        AND(ebx, 0x07);
        //call  [spMTab + 4*ebx]
        if (ebx.data == 0) ProcessNoteOff(); else if (ebx.data == 1) ProcessNoteOn(); else if (ebx.data == 2) ProcessPolyPressure(); else if (ebx.data == 3) ProcessControlChange();
        else if (ebx.data == 4) ProcessProgramChange(); else if (ebx.data == 5) ProcessChannelPressure(); else if (ebx.data == 6) ProcessPitchBend(); else ProcessRealTime();
        goto mainloop;
    }

    // rsi: ptr auf midistream (mu² nachher richtig sein!)
    // ecx: channel aus commandbyte

    // ------------------------------------------------------------------------
    // Note Off

    void SynthAsm::ProcessNoteOff()
    {
        MOVZX(edi, byte(rsi[0]));                   // Note
        // Nach Voice suchen, die die Note spielt
        XOR(edx, edx);

#ifdef RONAN
        CMP(cl, 15);                                // Speechsynth - Channel ?
        if (JNE())
            goto srvloop;
        PUSHAD();
        rax.data = rbp.data + offsetof(SYN, ronanw); // lea eax, [ebp+SYN.ronanw]
        ronanCBNoteOff(reinterpret_cast<syWRonan*>(rax.data));
        POPAD();
#endif

    srvloop:
        CMP(ecx, rbp[offsetof(SYN, chanmap) + 4 * edx.data]); // richtiger channel ?
        if (JNE())
            goto srvlno;
        MOV(eax, edx);
        IMUL(eax, sizeof(syWV2));
        rbp.data += offsetof(SYN, voicesw) + eax.data; // lea ebp, [ebp + SYN.voicesw + eax]
        CMP(edi, rbp[offsetof(syWV2, note)]);
        if (JNE())
            goto srvlno;
        TEST(byte(rbp[offsetof(syWV2, gate)]), 255);
        if (JZ())
            goto srvlno;

        syV2NoteOff();
        MOV(rbp, &m_this);
        goto end;
    srvlno:
        MOV(rbp, &m_this);
        INC(edx);
        CMP(dl, POLY);
        if (JNE())
            goto srvloop;

    end:
        ADD(rsi, 2);
    }

    // ------------------------------------------------------------------------
    // Note On

    void SynthAsm::ProcessNoteOn()
    {
        // auf noteoff checken
        MOV(al, rsi[1]);
        OR(al, al);
        if (JNZ())
            goto isnoteon;
        ProcessNoteOff();
        return;
    isnoteon:

#ifdef RONAN
        CMP(cl, 15);                                // Speechsynth - Channel ?
        if (JNE())
            goto noronan;
        PUSHAD();
        rax.data = rbp.data + offsetof(SYN, ronanw); // lea eax, [ebp+SYN.ronanw]
        ronanCBNoteOn(reinterpret_cast<syWRonan*>(rax.data));
        POPAD();
    noronan:
#endif

        MOVZX(eax, byte(rbp[offsetof(SYN, chans) + 8 * ecx.data])); // pgmnummer
        MOV(rdi, rbp[offsetof(SYN, patchmap)]);
        ADD(rdi, dword(rdi[4 * eax.data]));         // sounddef
        MOVZX(edi, byte(rdi[offsetof(v2sound, maxpoly)]));

        // maximale polyphonie erreicht?
        XOR(ebx, ebx);
        MOV(edx, POLY);
    chmploop:
        MOV(eax, rbp[offsetof(SYN, chanmap) + 4 * edx.data - 4]);
        CMP(eax, ecx);
        if (JNE())
            goto notthischan;
        INC(ebx);
    notthischan:
        DEC(edx);
        if (JNZ())
            goto chmploop;

        CMP(ebx, edi);                              // poly erreicht ?
        if (JAE())
            goto killvoice;                         // ja->alte voice t²ten

        //; freie voice suchen
        XOR(edx, edx);
    sfvloop1:
        MOV(eax, rbp[offsetof(SYN, chanmap) + 4 * edx.data]);
        OR(eax, eax);
        if (JS())
            goto foundfv;
        INC(edx);
        CMP(dl, POLY);
        if (JNE())
            goto sfvloop1;

        // keine freie? gut, dann die ²lteste mit gate aus
        MOV(edi, rbp[offsetof(SYN, curalloc)]);
        XOR(ebx, ebx);
        XOR(edx, edx);
        NOT(edx);
    sfvloop2:
        MOV(eax, ebx);
        IMUL(eax, sizeof(syWV2));
        MOV(eax, rbp[offsetof(SYN, voicesw) + eax.data + offsetof(syWV2, gate)]);
        OR(eax, eax);
        if (JNZ())
            goto sfvl2no;
        CMP(edi, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        if (JBE())
            goto sfvl2no;
        MOV(edi, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        MOV(edx, ebx);
    sfvl2no:
        INC(ebx);
        CMP(bl, POLY);
        if (JNE())
            goto sfvloop2;

        OR(edx, edx);
        if (JNS())
            goto foundfv;

        // immer noch keine freie? gut, dann die ganz ²lteste
        MOV(eax, rbp[offsetof(SYN, curalloc)]);
        XOR(ebx, ebx);
    sfvloop3:
        CMP(eax, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        if (JBE())
            goto sfvl3no;
        MOV(eax, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        MOV(edx, ebx);
    sfvl3no:
        INC(ebx);
        CMP(bl, POLY);
        if (JNE())
            goto sfvloop3;
        // ok... die nehmen wir jezz.
    foundfv:
        goto donoteon;

    killvoice:
        // ²lteste voice dieses channels suchen
        MOV(edi, rbp[offsetof(SYN, curalloc)]);
        XOR(ebx, ebx);
        XOR(edx, edx);
        NOT(edx);
    skvloop2:
        CMP(ecx, rbp[offsetof(SYN, chanmap) + 4 * ebx.data]);
        if (JNE())
            goto skvl2no;
        MOV(eax, ebx);
        IMUL(eax, sizeof(syWV2));
        MOV(eax, rbp[offsetof(SYN, voicesw) + eax.data + offsetof(syWV2, gate)]);
        OR(eax, eax);
        if (JNZ())
            goto skvl2no;
        CMP(edi, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        if (JBE())
            goto skvl2no;
        MOV(edi, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        MOV(edx, ebx);
    skvl2no:
        INC(ebx);
        CMP(bl, POLY);
        if (JNE())
            goto skvloop2;

        OR(edx, edx);
        if (JNS())
            goto donoteon;

        // nein? -> ²lteste voice suchen
        MOV(eax, rbp[offsetof(SYN, curalloc)]);
        XOR(ebx, ebx);
    skvloop:
        CMP(ecx, rbp[offsetof(SYN, chanmap) + 4 * ebx.data]);
        if (JNE())
            goto skvlno;
        CMP(eax, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        if (JBE())
            goto skvlno;
        MOV(eax, rbp[offsetof(SYN, allocpos) + 4 * ebx.data]);
        MOV(edx, ebx);
    skvlno:
        INC(ebx);
        CMP(bl, POLY);
        if (JNE())
            goto skvloop;
        // ... und damit haben wir die voice, die fliegt.

    donoteon:
        // (ecx immer noch chan, edx voice, ebp=this)
        MOV(rbp[offsetof(SYN, chanmap) + 4 * edx.data], ecx); // channel
        MOV(rbp[offsetof(SYN, voicemap) + 4 * ecx.data], edx); // current voice
        MOV(eax, rbp[offsetof(SYN, curalloc)]);
        MOV(rbp[offsetof(SYN, allocpos) + 4 * edx.data], eax); // allocpos
        INC(dword(rbp[offsetof(SYN, curalloc)]));

        storeV2Values();                            // Values der Voice updaten
        MOV(eax, edx);
        IMUL(eax, sizeof(syWV2));
        rbp.data += offsetof(SYN, voicesw) + eax.data; // lea ebp, [ebp + SYN.voicesw + eax]
        MOVZX(eax, byte(rsi[0]));                   // Note
        MOVZX(ebx, byte(rsi[1]));                   // Velo
        syV2NoteOn();
        MOV(rbp, &m_this);

        ADD(rsi, 2);
    }

    // ------------------------------------------------------------------------
    // Aftertouch

    void SynthAsm::ProcessChannelPressure()
    {
        ADD(rsi, 1);
    }

    // ------------------------------------------------------------------------
    // Control Change

    void SynthAsm::ProcessControlChange()
    {
        MOVZX(eax, byte(rsi[0]));
        OR(eax, eax);                               // und auf 1 - 7 checken
        if (JZ())
            goto end;
        CMP(al, 7);
#ifdef FULLMIDI
        if (JA())
            goto chanmode;
#else
        if (JA())
            goto end;
#endif

        rdi.data = rbp.data + offsetof(SYN, chans) + 8 * ecx.data; // lea edi, [ebp + SYN.chans + 8*ecx]
        MOVZX(ebx, byte(rsi[1]));
        MOV(rdi[eax.data], bl);

        CMP(cl, 15);                                // sprachsynth?
        if (JNE())
            goto end;
#ifdef RONAN
        PUSHAD();
        ecx.data = eax.data;
        rax.data = rbp.data + offsetof(SYN, ronanw); // lea eax, [ebp+SYN.ronanw]
        ronanCBSetCtl(reinterpret_cast<syWRonan*>(rax.data), ecx.data, ebx.data);
        POPAD();
#endif


    end:
        ADD(rsi, 2);
        return;

#ifdef FULLMIDI
    chanmode:
        CMP(al, 120);
        if (JB())
            goto end;
        if (JA())
            goto noalloff;

        // CC #120 : All Sound Off
        XOR(edx, edx);
        MOV(rax, rbp);
        ADD(rbp, offsetof(SYN, voicesw));
    siloop1:
        CMP(byte(rax[offsetof(SYN, chanmap) - 4 + 4 * edx.data]), cl);
        if (JNZ())
            goto noreset;
        syV2Init();
        MOV(dword(rax[offsetof(SYN, chanmap) - 4 + 4 * edx.data]), static_cast<uint32_t>(-1));
    noreset:
        rbp.data += sizeof(syWV2); // lea ebp, [ebp+syWV2.size]
        INC(dl);
        CMP(dl, POLY);
        if (JNZ())
            goto siloop1;

        MOV(rbp, rax);
        goto end;

    noalloff:
        CMP(al, 121);
        if (JA())
            goto noresetcc;
        // CC #121 : Reset All Controllers
        // evtl TODO, who knows

        goto end;

        noresetcc:
            CMP(al, 123);
        if (JB())
            goto end;
        // CC #123 : All Notes Off
        // and CC #124-#127 - channel mode changes (ignored)

#ifdef RONAN
        CMP(cl, 15);                                // Speechsynth-Channel?
        if (JNE())
            goto noronanoff;
        PUSHAD();
        rax.data = rbp.data + offsetof(SYN, ronanw); // lea eax, [ebp+SYN.ronanw]
        ronanCBNoteOff(reinterpret_cast<syWRonan*>(rax.data));
        POPAD();
    noronanoff:
#endif

        MOV(rax, rbp);
        XOR(edx, edx);
        ADD(rbp, offsetof(SYN, voicesw));
    noffloop1:
        CMP(byte(rax[offsetof(SYN, chanmap) - 4 + 4 * edx.data]), cl);
        if (JNZ())
            goto nonoff;
        syV2NoteOff();
    nonoff:
        rbp.data += sizeof(syWV2); // lea ebp, [ebp+syWV2.size]
        INC(dl);
        CMP(dl, POLY);
        if (JNZ())
            goto noffloop1;
        MOV(rbp, rax);
        goto end;
#endif
    }


    // ------------------------------------------------------------------------
    // Program change


    void SynthAsm::ProcessProgramChange()
    {
        // check ob selbes program
        rdi.data = rbp.data + offsetof(SYN, chans) + 8 * ecx.data; // lea edi, [ebp + SYN.chans + 8*ecx]
        MOV(al, rsi[0]);
        AND(al, 0x7f);
        CMP(al, rdi[0]);
        if (JZ())
            goto sameprg;
        MOV(rdi[0], al);

        // ok, alle voices, die was mit dem channel zu tun haben... NOT-AUS!
        XOR(edx, edx);
        XOR(eax, eax);
        NOT(eax);
    notausloop:
        CMP(ecx, rbp[offsetof(SYN, chanmap) + 4 * edx.data]);
        if (JNZ())
            goto nichtaus;
        MOV(rbp[offsetof(SYN, chanmap) + 4 * edx.data], eax);
    nichtaus:
        INC(edx);
        CMP(dl, POLY);
        if (JNE())
            goto notausloop;

        // pgm setzen und controller resetten
    sameprg:
        MOV(cl, 6);
        INC(rsi);
        INC(rdi);
        XOR(eax, eax);
        REP_STOSB();
    }

    // ------------------------------------------------------------------------
    // Pitch Bend

    void SynthAsm::ProcessPitchBend()
    {
       ADD(rsi, 2);
    }

    // ------------------------------------------------------------------------
    // Poly Aftertouch

    void SynthAsm::ProcessPolyPressure() // unsupported
    {
        ADD(rsi, 2);
    }

    // ------------------------------------------------------------------------
    // Realtime/System messages

    void SynthAsm::ProcessRealTime()
    {
#ifdef FULLMIDI
        CMP(cl, 0x0f);
        if (JNE())
            goto noreset;

        PUSHAD();
        synthInit(*reinterpret_cast<uint64_t**>(rbp[offsetof(SYN, patchmap)]),
            *reinterpret_cast<int*>(rbp[offsetof(SYN, samplerate)]));
        POPAD();

    noreset:
        return;
#endif
    }

    // ------------------------------------------------------------------------
    // Noch wichtiger.

    void SynthAsm::synthSetGlobals(const void* ptr)
    {
        PUSHAD();

        rbp.data = reinterpret_cast<uint64_t>(m_mem); // ebp, [esp + 36]
        m_this = rbp.data; // mov [this], ebp

        XOR(eax, eax);
        XOR(ecx, ecx);
        rsi.data = reinterpret_cast<uint64_t>(ptr); // mov esi, [esp+40]
        rdi.data = rbp.data + offsetof(SYN, globals) + offsetof(SYN::Globals, rvbparm); // lea edi, [ebp + SYN.rvbparm]
        MOV(cl, sizeof(SYN::Globals) / 4); // mov cl, (SYN.globalsend-SYN.globalsstart)/4
    cvloop:
        LODSB();
        MOV(m_temp, eax);
        FILD(dword(m_temp));
        FSTP(dword(rdi[0]));
        rdi.data += 4; // lea edi, [edi+4]
        DEC(ecx);
        if (JNZ())
            goto cvloop;

        rsi.data = rbp.data + offsetof(SYN, globals) + offsetof(SYN::Globals, rvbparm); // lea esi, [ebp + SYN.rvbparm]
        rbp.data += offsetof(SYN, reverb); // lea ebp, [ebp + SYN.reverb]
        syReverbSet();
        MOV(rbp, &m_this);
        rsi.data = rbp.data + offsetof(SYN, globals) + offsetof(SYN::Globals, delparm); // lea esi, [ebp + SYN.delparm]
        rbp.data += offsetof(SYN, delay); // lea ebp, [ebp + SYN.delay]
        syModDelSet();
        MOV(rbp, &m_this);
        rsi.data = rbp.data + offsetof(SYN, globals) + offsetof(SYN::Globals, cprparm); // lea esi, [ebp + SYN.cprparm]
        rbp.data += offsetof(SYN, compr); // lea ebp, [ebp + SYN.compr]
        syCompSet();
        MOV(rbp, &m_this);

        FLD(dword(rbp[offsetof(SYN, globals) + offsetof(SYN::Globals, vlowcut)]));
        FLD1();
        FADDP(st1, st0);
        FMUL(dword(m_fci128));
        FMUL(st0, st0);
        FSTP(dword(rbp[offsetof(SYN, lcfreq)]));
        FLD(dword(rbp[offsetof(SYN, globals) + offsetof(SYN::Globals, vhighcut)]));
        FLD1();
        FADDP(st1, st0);
        FMUL(dword(m_fci128));
        FMUL(st0, st0);
        FSTP(dword(rbp[offsetof(SYN, hcfreq)]));

        POPAD();
    }

    // ------------------------------------------------------------------------
    // VU retrieval

#ifdef VUMETER

global _synthSetVUMode@8
_synthSetVUMode@8:
  PUSHAD();
  mov ebp, [esp + 36]
  mov eax, [esp + 40]
  MOV(rbp[SYN.vumode], eax
  POPAD();
  ret 8


global _synthGetChannelVU@16
_synthGetChannelVU@16:
  PUSHAD();
  mov  ebp, [esp + 36]
  mov  ecx, [esp + 40]
  mov  esi, [esp + 44]
  mov  edi, [esp + 48]
  mov  eax, [ebp + SYN.chanvu + 8*ecx]
  mov  [esi], eax
  mov  eax, [ebp + SYN.chanvu + 8*ecx + 4]
  mov  [edi], eax
  POPAD();
  ret 16

global _synthGetMainVU@12
_synthGetMainVU@12:
  PUSHAD();
  mov  ebp, [esp + 36]
  mov  esi, [esp + 40]
  mov  edi, [esp + 44]
  mov  eax, [ebp + SYN.mainvu]
  mov  [esi], eax
  mov  eax, [ebp + SYN.mainvu+4]
  mov  [edi], eax
  POPAD();
  ret 12

#endif


    // ------------------------------------------------------------------------
    // Debugkram

    void SynthAsm::synthGetPoly(void* dest)
    {
        PUSHAD();
        MOV(ecx, 17);
        rbp.data = reinterpret_cast<uint64_t>(m_mem); // mov ebp, [esp + 36]
        rdi.data = reinterpret_cast<uint64_t>(dest); // mov edi, [esp + 40]
        XOR(eax, eax);
        REP_STOSD();

        rdi.data = reinterpret_cast<uint64_t>(dest); // mov edi, [esp + 40]
    gploop:
        MOV(ebx, rbp[offsetof(SYN, chanmap) + 4 * eax.data]);
        OR(ebx, ebx);
        if (JS())
            goto gplend;
        INC(dword(rdi[4 * ebx.data]));
        INC(dword(rdi[4 * 16]));
    gplend:
        INC(eax);
        CMP(al, POLY);
        if (JNE())
            goto gploop;

        POPAD();
    }

    void SynthAsm::synthGetPgm(void* dest)
    {
        PUSHAD();
        rbp.data = reinterpret_cast<uint64_t>(m_mem); // mov ebp, [esp + 36]
        rdi.data = reinterpret_cast<uint64_t>(dest); // mov edi, [esp + 40]
        XOR(ecx, ecx);

    gploop:
        MOVZX(eax, byte(rbp[offsetof(SYN, chans) + 8 * ecx.data]));
        STOSD();
        INC(ecx);
        CMP(cl, 16);
        if (JNE())
            goto gploop;

        POPAD();
    }

    uint32_t SynthAsm::synthGetFrameSize()
    {
        return m_SRcFrameSize;
    }

    void* SynthAsm::synthGetSpeechMem()
    {
#ifdef RONAN
        return m_mem + offsetof(SYN, ronanw);
#else
        return nullptr;
#endif
    }
}
//namespace V2