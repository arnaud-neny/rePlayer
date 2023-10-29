#ifndef _NES_VRC6_H_
#define _NES_VRC6_H_

namespace xgm
{

  class NES_VRC6
  {
  public:
    enum
    {
      OPT_END
    };
  protected:
    unsigned int counter[3]; // frequency divider
    unsigned int phase[3];   // phase counter
    unsigned int freq2[3];   // adjusted frequency
    int count14;       // saw 14-stage counter

    //int option[OPT_END];
    int mask;
    int sm[2][3]; // stereo mix
    int duty[2];
    int volume[3];
    int enable[3];
    int gate[3];
    unsigned int freq[3];
    short calc_sqr (int i, unsigned int clocks);
    short calc_saw (unsigned int clocks);
    bool halt;
    int freq_shift;
    double clock, rate;
    int out[3];

  public:
      NES_VRC6 ();
     ~NES_VRC6 ();

    void Reset ();
    void Tick (unsigned int clocks);
    unsigned int Render (int b[2]);
    bool Read (unsigned int adr, unsigned int & val, unsigned int id=0);
    bool Write (unsigned int adr, unsigned int val, unsigned int id=0);
    void SetClock (double);
    void SetRate (double);
    void SetOption (int, int);
    void SetMask (int m){ mask = m; }
    void SetStereoMix (int trk, short mixl, short mixr);
  };

}                               // namespace

#endif
