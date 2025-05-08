/**********************************************************************

Audacity: A Digital Audio Editor

Compressor.h

Dominic Mazzoni


Based on svn revision 10189
**********************************************************************/

#ifndef __AUDACITY_EFFECT_COMPRESSOR__
#define __AUDACITY_EFFECT_COMPRESSOR__

namespace psycle::plugins::audacity_compressor {

class EffectCompressor {

public:

	EffectCompressor();
	~EffectCompressor();

	void Init(int max_samples);
	void Process(float *samples, int num);

	void setSampleRate(double rate);
	void setThresholddB(double threshold);
	void setNoiseFloordB(double noisefloor);
	void setRatio(double ratio);
	void setAttackSec(double attack);
	void setDecaySec(double decay);
	void setNormalize(bool normalize);
	void setPeakAnalisys(bool peak);
	float lastLevel();

private:
	inline bool ProcessPass2(float *buffer, int len);

	bool NewTrackPass1();
	bool InitPass1(int max_samples);

	inline void FreshenCircle();
	inline float AvgCircle(float x);

	void setAttackFactor();
	void setDecayFactor();
	void calcGainDb();
	void setGain(bool gain);
	inline void Follow(float *buffer, float *env, int len, float *previous, int previous_len);
	inline float DoCompression(float x, double env);

	double    mAttackTime;
	double    mThresholdDB;
	double    mNoiseFloorDB;
	double    mRatio;
	bool      mNormalize;        //MJS
	bool      mUsePeak;

	double    mDecayTime;
	double    mAttackFactor;
	double    mAttackInverseFactor;
	double    mDecayFactor;
	double    mThreshold;
	double    mCompression;
	double    mNoiseFloor;
	int       mNoiseCounter;
	double    mGain;
	double    mRMSSum;
	int       mCircleSize;
	int       mCirclePos;
	double   *mCircle;
	double    mLastLevel;
	float    *mFollow1;
	int       mFollowLen;

	double	  mCurRate;

};
}
#endif
