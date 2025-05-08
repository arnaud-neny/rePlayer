//interface definitions for dwfilter class

#pragma once
#include <universalis/stdlib/cstdint.hpp>
#include <cmath>
namespace psycle::plugins::dw {
class dwfilter
{
public:
	static const double PI;
	static const double TWO_PI;
	static const double PI_EIGHTHS;
	static const double PI_FOURTHS;
	static const double PI_THIRDS;
	static const double PI_HALVES;
	static const int   FILT_MIN_FREQ;
	static const float FILT_MIN_GAIN;
	static const float FILT_MAX_GAIN;
	static const float FILT_MIN_BW;
	static const float FILT_MAX_BW;
	static const float FILT_MIN_SLOPE;
	static const float FILT_MAX_SLOPE;
	static const float FILT_MIN_Q;
	static const float FILT_MAX_Q;

	int static const NUM_MODES = 4;
	int static const FILT_MAX_CHANS = 2;

	enum filtmode
	{
		nop=0,
		eq_parametric=1,
		eq_hishelf=2,
		eq_loshelf=3
	};

	dwfilter();
	dwfilter(int samprate);

	dwfilter(const dwfilter& rhs);
	dwfilter& operator=(const dwfilter& rhs);
	virtual ~dwfilter() {}
	virtual void CoefUpdate();

	virtual float Process(const float xn, const int chan=0);

	virtual bool SetMode(int _mode);
	virtual bool SetFreq(double _freq);
	virtual bool SetGain(double _gain);
	virtual bool SetBW(double _bw);
	virtual bool SetSlope(double _slope);
	virtual bool SetQ(double _q);
	virtual bool SetSampleRate(int samprate);

	virtual double GetFreq()				{ return freq;								}
	virtual double GetGain()				{ return gain;								}
	virtual double GetBW()								{ return bandwidth;				}
	virtual double GetSlope()				{ return slope;								}
	virtual double GetQ()								{ return q;												}
	virtual int GetMode()				{ return (int)mode;								}


protected:

	virtual void zeroize();
	//virtual bool isdenormal(float num);
	virtual void emptybuffers();

	filtmode mode;

	double freq;
	double gain;
	double bandwidth;
	double q;

	int nyquist;

	double coefa0, coefa1, coefa2;
	double coefb0, coefb1, coefb2;

	float z0[FILT_MAX_CHANS], z1[FILT_MAX_CHANS], z2[FILT_MAX_CHANS];

	// intermediate vars for parametric
	double bwtan;
	double costheta;
	double thetac;

	// intermediate vars for shelves
	double rho;
	double beta;
	double sintheta;
	double costhetaRho;
	double sinthetaBeta;
	double slope;
};

}