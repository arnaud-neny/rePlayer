/*
 * GOATTRACKER reSID interface
 */

#define GSID_C

#include <stdlib.h>
#include <stdio.h>
#include "resid/sid.h"
#include "resid-fp/sidfp.h"

// rePlayer begin
#include <utility>
namespace rePlayer
{
    extern int stereosid;
    template <typename SidType>
    struct SidT
    {
        SidType sid[2];
        int samples;

        void set_chip_model(chip_model model)
        {
            sid[0].set_chip_model(model);
            sid[1].set_chip_model(model);
        }
        template <typename... Types>
        bool set_sampling_parameters(Types&&... args)
        {
            sid[0].set_sampling_parameters(std::forward<Types>(args)...);
            return sid[1].set_sampling_parameters(std::forward<Types>(args)...);
        }
        int clock(cycle_count& delta_t, short* buf, int n)
        {
            cycle_count delta_t_copy = delta_t;
            sid[0].clock(delta_t_copy, buf, n, 2);
            return sid[1].clock(delta_t, buf + 1, n, 2);
        }
        void reset()
        {
            sid[0].reset();
            sid[1].reset();
        }
        void write(reg8 offset, reg8 value)
        {
            if (stereosid && (offset == 0x4 || offset == 0x4 + 7 * 2))
                sid[0].write(offset, 0x08);
            else
                sid[0].write(offset, value);
            if (stereosid && offset == 0x4 + 7)
                sid[1].write(offset, 0x08);
            else
                sid[1].write(offset, value);
        }
    };
}

typedef rePlayer::SidT<SID> SIDS;
typedef rePlayer::SidT<SIDFP> SIDSFP;

extern "C" {
// rePlayer end

#include "gsid.h"
#include "gsound.h"

int clockrate;
int samplerate;
unsigned char sidreg[NUMSIDREGS];

unsigned char sidorder[] =
  {0x15,0x16,0x18,0x17,
   0x05,0x06,0x02,0x03,0x00,0x01,0x04,
   0x0c,0x0d,0x09,0x0a,0x07,0x08,0x0b,
   0x13,0x14,0x10,0x11,0x0e,0x0f,0x12};

unsigned char altsidorder[] =
  {0x15,0x16,0x18,0x17,
   0x04,0x00,0x01,0x02,0x03,0x05,0x06,
   0x0b,0x07,0x08,0x09,0x0a,0x0c,0x0d,
   0x12,0x0e,0x0f,0x10,0x11,0x13,0x14};

SIDS *sid = 0; // rePlayer
SIDSFP *sidfp = 0; // rePlayer

FILTERPARAMS filterparams =
  {0.50f, 3.3e6f, 1.0e-4f,
   1147036.4394268463f, 274228796.97550374f, 1.0066634233403395f, 16125.154840564108f,
   5.5f, 20.f,
   0.9613160610660189f};

extern unsigned residdelay;
extern unsigned adparam;

void sid_init(int speed, unsigned m, unsigned ntsc, unsigned interpolate, unsigned customclockrate, unsigned usefp)
{
  int c;

  if (ntsc) clockrate = NTSCCLOCKRATE;
    else clockrate = PALCLOCKRATE;

  if (customclockrate)
    clockrate = customclockrate;

  samplerate = speed;

  if (!usefp)
  {
    if (sidfp)
    {
      delete sidfp;
      sidfp = NULL;
    }

    if (!sid) sid = new SIDS; // rePlayer
  }
  else
  {
    if (sid)
    {
      delete sid;
      sid = NULL;
    }
    
    if (!sidfp) sidfp = new SIDSFP; // rePlayer
  }

  switch(interpolate)
  {
    case 0:
    if (sid) sid->set_sampling_parameters(clockrate, SAMPLE_FAST, speed);
    if (sidfp) sidfp->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, speed);
    break;

    default:
    if (sid) sid->set_sampling_parameters(clockrate, SAMPLE_INTERPOLATE, speed);
    if (sidfp) sidfp->set_sampling_parameters(clockrate, SAMPLE_RESAMPLE_INTERPOLATE, speed);
    break;
  }

  if (sid) sid->reset();
  if (sidfp) sidfp->reset();
  for (c = 0; c < NUMSIDREGS; c++)
  {
    sidreg[c] = 0x00;
  }
  if (m == 1)
  {
    if (sid) sid->set_chip_model(MOS8580);
    if (sidfp) sidfp->set_chip_model(MOS8580);
  }
  else
  {
    if (sid) sid->set_chip_model(MOS6581);
    if (sidfp) sidfp->set_chip_model(MOS6581);
  }

  if (sidfp)
  {
    // rePlayer begin
    for (c = 0; c < 2; c++)
    {
      sidfp->sid[c].get_filter().set_distortion_properties(
        filterparams.distortionrate,
        filterparams.distortionpoint,
        filterparams.distortioncfthreshold);
      sidfp->sid[c].get_filter().set_type3_properties(
        filterparams.type3baseresistance,
        filterparams.type3offset,
        filterparams.type3steepness,
        filterparams.type3minimumfetresistance);
      sidfp->sid[c].get_filter().set_type4_properties(
        filterparams.type4k,
        filterparams.type4b);
      sidfp->sid[c].set_voice_nonlinearity(
        filterparams.voicenonlinearity);
    }
    // rePlayer end
  }
}

// rePlayer begin
void sid_uninit(void)
{
  if (sidfp)
  {
    delete sidfp;
    sidfp = NULL;
  }
  if (sid)
  {
    delete sid;
    sid = NULL;
  }
}
// rePlayer end

unsigned char sid_getorder(unsigned char index)
{
  if (adparam >= 0xf000)
    return altsidorder[index];
  else
    return sidorder[index];
}

int sid_fillbuffer(short *ptr, int samples)
{
  int tdelta;
  int tdelta2;
  int result = 0;
  int total = 0;
  int c;

  int badline = rand() % NUMSIDREGS;

  tdelta = clockrate * samples / samplerate;
  if (tdelta <= 0) return total;

  for (c = 0; c < NUMSIDREGS; c++)
  {
    unsigned char o = sid_getorder(c);

    // Possible random badline delay once per writing
    if ((badline == c) && (residdelay))
    {
      tdelta2 = residdelay;
      if (sid) result = sid->clock(tdelta2, ptr, samples);
      if (sidfp) result = sidfp->clock(tdelta2, ptr, samples);
      total += result;
      ptr += result * 2; // rePlayer
      samples -= result;
      tdelta -= residdelay;
    }

    if (sid) sid->write(o, sidreg[o]);
    if (sidfp) sidfp->write(o, sidreg[o]);

    tdelta2 = SIDWRITEDELAY;
    if (sid) result = sid->clock(tdelta2, ptr, samples);
    if (sidfp) result = sidfp->clock(tdelta2, ptr, samples);
    total += result;
    ptr += result * 2; // rePlayer
    samples -= result;
    tdelta -= SIDWRITEDELAY;

    if (tdelta <= 0) return total;
  }

  if (sid) result = sid->clock(tdelta, ptr, samples);
  if (sidfp) result = sidfp->clock(tdelta, ptr, samples);
  total += result;
  ptr += result * 2; // rePlayer
  samples -= result;

  // Loop extra cycles until all samples produced
  while (samples)
  {
    tdelta = clockrate * samples / samplerate;
    if (tdelta <= 0) return total;

    if (sid) result = sid->clock(tdelta, ptr, samples);
    if (sidfp) result = sidfp->clock(tdelta, ptr, samples);
    total += result;
    ptr += result * 2; // rePlayer
    samples -= result;
  }

  return total;
}

} // rePlayer