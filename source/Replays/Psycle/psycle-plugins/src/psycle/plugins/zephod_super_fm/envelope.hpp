#pragma once
namespace psycle::plugins::zephod_super_fm {
enum {
	ENV_ATT=1,
	ENV_DEC,
	ENV_FINITE_SUS,
	ENV_NOTEOFF_SUS,
	ENV_REL,
	ENV_ANTICLICK,

	ENV_NONE=99
};

class envelope  
{
public:
	envelope();
	virtual ~envelope();

	inline float res(void);
	void reset();
	void attack(int newv);
	void decay(int newv);
	void sustain(int newv);
	void sustainv(float newv);
	void release(int newv);
	void stop();
	void noteoff();

public:
	int a,d,s,r;
	int envstate;
	float envvol;
	float susvol;
	float envcoef;
	int suscounter;

};

// Envelope get function inline

float envelope::res(void)
{
	if (envstate!=ENV_NONE)
	{
		// Attack
		if(envstate==ENV_ATT)
		{
			envvol+=envcoef;
			
			if(envvol>1.0f)
			{
				envvol=1.0f;
				envstate=ENV_DEC;
				envcoef=(1.0f-susvol)/(float)d;
			}
		}

		// Decay
		if(envstate==ENV_DEC)
		{
			envvol-=envcoef;
			
			if(envvol<susvol)
			{
				envvol=susvol;
				if(s == 0) envstate=ENV_NOTEOFF_SUS;
				else envstate=ENV_FINITE_SUS;
				suscounter=0;
			}
		}

		// Sustain
		if(envstate==ENV_FINITE_SUS)
		{
			suscounter++;
			
			if(suscounter>s)
			{
				envstate=ENV_REL;
				envcoef=envvol/(float)r;
			}
		}

		// Release
		if(envstate==ENV_REL)
		{
			envvol-=envcoef;

			if(envvol<0)
			{
				envvol=0;
				envstate=ENV_NONE;
			}
		}

		if(envstate==ENV_ANTICLICK)
		{
			envvol-=envcoef;

			if(envvol<0)
			{
				envvol=0;
				envstate=ENV_ATT;
				envcoef=1.0f/(float)a;
			}
		}

		return envvol;
	}
	else 
		return 0;

}
}