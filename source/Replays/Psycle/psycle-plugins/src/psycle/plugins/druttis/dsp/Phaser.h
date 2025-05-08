//////////////////////////////////////////////////////////////////////
//
//				Phaser.h
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DspAlgs.h"
namespace psycle::plugins::druttis {
//////////////////////////////////////////////////////////////////////
//				Phaser class
//////////////////////////////////////////////////////////////////////

class Phaser
{
	//////////////////////////////////////////////////////////////////
	//				Variables
	//////////////////////////////////////////////////////////////////

private:

	float				a;
	float				zm0;
	float				zm1;
	float				zm2;
	float				zm3;
	float				zm4;
	float				zm5;
	float				zm6;
	float				fb;
	float				depth;
	float				ndepth;

	//////////////////////////////////////////////////////////////////
	//				Methods
	//////////////////////////////////////////////////////////////////

public:

	Phaser()
	{
		Init();
	}

	virtual ~Phaser()
	{
	}

	void Init()
	{
		SetFeedback(0.7f);
		SetDepth(1.0f);
		SetDelay(0.1f);
		Reset();
	}

	void Reset()
	{
		zm0 = zm1 = zm2 = zm3 = zm4 = zm5 = zm6 = 0.0f;
	}

	void SetFeedback(float value)
	{
		fb = value;
	}

	void SetDepth(float value)
	{
		depth = value;
		ndepth = 1.0f - value;
	}

	inline void SetDelay(float value)
	{
		if (value < 0.0f)
			value = 0.0f;
		if (value > 1.0f)
			value = 1.0f;
		a = (1.0f - value) / (1.0f + value);
	}

	inline float Tick(float in)
	{
		float y;
		float out = in + zm0 * fb;
		//				6
		y = zm6 - out * a;
		zm6 = y * a + out;
		//				5
		out = zm5 - y * a;
		zm5 = out * a + y;
		//				4
		y = zm4 - out * a;
		zm4 = y * a + out;
		//				3
		out = zm3 - y * a;
		zm3 = out * a + y;
		//				2
		y = zm2 - out * a;
		zm2 = y * a + out;
		//				1
		out = zm1 - y * a;
		zm1 = out * a + y;
		//				0
		zm0 = out;
		return ndepth * in + out * depth;
	}
};
}