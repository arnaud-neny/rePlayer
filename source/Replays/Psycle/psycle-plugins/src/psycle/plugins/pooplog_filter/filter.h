// filter.h: interface for the filter class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <cmath>
namespace psycle::plugins::pooplog_filter {
typedef float SIG;
#define TWOPI_F (2.0f*3.141592665f)
#define PI 3.14159265358979323846
#define CUTOFFCONV(x)				((96*powf(32,x/192.0f))-86)
//static const double PI=4*atan(1.0);
#define THREESEL(sel,a,b,c) ((sel)<120)?((a)+((b)-(a))*(sel)/120):((b)+((c)-(b))*((sel)-120)/120)


class CBiquad
{
public:
	float m_a1, m_a2, m_b0, m_b1, m_b2;
	float m_x1, m_x2, m_y1, m_y2;
	CBiquad() { m_x1=0.0; m_y1=0.0; m_x2=0.0; m_y2=0.0; }
	inline SIG ProcessSample(SIG dSmp) 
	{ 
		SIG dOut=m_b0*dSmp+m_b1*m_x1+m_b2*m_x2-m_a1*m_y1-m_a2*m_y2;
		if (dOut>=-0.00001 && dOut<=0.00001) dOut=0.0;
		if (dOut>900000) dOut=900000.0;
		if (dOut<-900000) dOut=-900000.0;
		m_x2=m_x1;
		m_x1=dSmp;
		m_y2=m_y1;
		m_y1=dOut;
		return dOut;
	}
	float PreWarp(float dCutoff, float dSampleRate)
	{
		if (dCutoff>dSampleRate*0.4) dCutoff=(float)(dSampleRate*0.4);
		//return (float)(/*1.0/*/tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
		return (float)(tan(3.1415926*dCutoff/dSampleRate));
	}
	float PreWarp2(float dCutoff, float dSampleRate)
	{
		if (dCutoff>dSampleRate*0.4) dCutoff=(float)(dSampleRate*0.4);
		//return (float)(/*1.0/*/tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
		return (float)(tan(3.1415926/2.0-3.1415926*dCutoff/dSampleRate));
	}
	void SetBilinear(float B0, float B1, float B2, float A0, float A1, float A2)
	{
		float q=(float)(1.0/(A0+A1+A2));
		m_b0=(B0+B1+B2)*q;
		m_b1=2*(B0-B2)*q;
		m_b2=(B0-B1+B2)*q;
		m_a1=2*(A0-A2)*q;
		m_a2=(A0-A1+A2)*q;
	}
	// Zoelzer's Parmentric Equalizer Filters - rodem z Csound'a
	void SetLowShelf(float fc, float q, float v, float esr)
	{
		float sq = (float)sqrt(2.0*(double)v);
		float omega = TWOPI_F*fc/esr;
		float k = (float) tan((double)omega*0.5);
		float kk = k*k;
		float vkk = v*kk;
		float oda0 =  1.0f/(1.0f + k/q +kk);
		m_b0 =  oda0*(1.0f + sq*k + vkk);
		m_b1 =  oda0*(2.0f*(vkk - 1.0f));
		m_b2 =  oda0*(1.0f - sq*k + vkk);
		m_a1 =  oda0*(2.0f*(kk - 1.0f));
		m_a2 =  oda0*(1.0f - k/q + kk);
	}
	void SetHighShelf(float fc, float q, float v, float esr)
	{
		float sq = (float)sqrt(2.0*(double)v);
		float omega = TWOPI_F*fc/esr;
		float k = (float) tan((PI - (double)omega)*0.5);
		float kk = k*k;
		float vkk = v*kk;
		float oda0 = 1.0f/( 1.0f + k/q +kk);
		m_b0 = oda0*( 1.0f + sq*k + vkk);
		m_b1 = oda0*(-2.0f*(vkk - 1.0f));
		m_b2 = oda0*( 1.0f - sq*k + vkk);
		m_a1 = oda0*(-2.0f*(kk - 1.0f));
		m_a2 = oda0*( 1.0f - k/q + kk);
	}
	void SetParametricEQ(float fc, float q, float v, float esr, float gain=1.0f)
	{
//								float sq = (float)sqrt(2.0*(double)v);
		float omega = TWOPI_F*fc/esr;
		float k = (float) tan((double)omega*0.5);
		float kk = k*k;
		float vk = v*k;
		float vkdq = vk/q;
		float oda0 =  1.0f/(1.0f + k/q +kk);
		m_b0 =  gain*oda0*(1.0f + vkdq + kk);
		m_b1 =  gain*oda0*(2.0f*(kk - 1.0f));
		m_b2 =  gain*oda0*(1.0f - vkdq + kk);
		m_a1 =  oda0*(2.0f*(kk - 1.0f));
		m_a2 =  oda0*(1.0f - k/q + kk);
	}
	void SetLowpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(a, 0, 0, a, 1, 0);
	}
	void SetHighpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(0, 1, 0, a, 1, 0);
	}
	void SetIntegHighpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(0, 1, 1, 0, 2*a, 2);
	}
	void SetAllpass1(float dCutoff, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		SetBilinear(1, -a, 0, 1, a, 0);
	}
	void SetAllpass2(float dCutoff, float fPoleR, float dSampleRate)
	{
		float a=PreWarp(dCutoff, dSampleRate);
		float q=fPoleR;
		SetBilinear(a*a+q*q, -2.0f*a, 1, a*a+q*q, 2.0f*a, 1);
	}
	void SetBandpass(float dCutoff, float dBandwith, float dSampleRate)
	{
		float b=(float)(2.0*PI*dBandwith/dSampleRate);
		float a=(float)(PreWarp2(dCutoff, dSampleRate));
		SetBilinear(0, b*a, 0, 1, b*a, a*a);
	}
	void SetBandreject(float dCutoff, float dSampleRate)
	{
		float a=(float)PreWarp(dCutoff, dSampleRate);
		SetBilinear(1, 0, 1, 1, a, 1);
	}
	void SetIntegrator()
	{
		m_b0=1.0;
		m_a1=-1.0;
		m_a2=0.0;
		m_b1=0.0;
		m_b2=0.0;
	}
	void SetResonantLP(float dCutoff, float Q, float dSampleRate)
	{
		float a=(float)PreWarp2(dCutoff, dSampleRate);
		// wspó³czynniki filtru analogowego
		float B=(float)(sqrt(Q*Q-1)/Q);
		float A=(float)(2*B*(1-B));
		SetBilinear(1, 0, 0, 1, A*a, B*a*a);
	}
	void SetResonantHP(float dCutoff, float Q, float dSampleRate)
	{
		float a=(float)PreWarp2((dSampleRate/2)/dCutoff, dSampleRate);
		// wspó³czynniki filtru analogowego
		float B=(float)(sqrt(Q*Q-1)/Q);
		float A=(float)(2*B*(1-B));
		SetBilinear(0, 0, 1, B*a*a, A*a, 1);
	}
	void Reset()
	{
		m_x1=m_y1=m_x2=m_y2=0.0f;
	}
};


class filter
{
public:
	filter();
	virtual ~filter();
	void init(int s) 
	{
		sr=(float)s;
		invert=false;
	}
	float res(float in)
	{
		float out=Biquad.ProcessSample(in);
		if (invert) return in-out;
		return out;
	}
	float res2(float in)
	{
		float out=Biquad2.ProcessSample(Biquad.ProcessSample(in));
		if (invert) return in-out;
		return out;
	}
	void reset()
	{
		Biquad.Reset();
		Biquad2.Reset();
		for( int i=0; i<16; i++ )
			_alps[i].Wipe();
		in1=in2=in3=in4=b0= b1= b2= b3= b4=0;
	}
	void SetFilter_4PoleLP(float cf, int Resonance);
	void SetFilter_4PoleEQ1(float cf, int Resonance);
	void SetFilter_4PoleEQ2(float cf, int Resonance);
	void SetFilter_Vocal1(float cf, int Resonance);
	void SetFilter_Vocal2(float cf, int Resonance);
	
	void setfilter(int type, float c,int r)

	{
		switch(type)
		{
		case 0: SetFilter_4PoleLP(c,r); invert=false; break;
		case 1: SetFilter_4PoleLP(c,r); invert=true; break;
		case 2: SetFilter_4PoleEQ1(c,r); invert=false; break;
		case 3: SetFilter_4PoleEQ1(c,r); invert=true; break;
		case 4: SetFilter_4PoleEQ2(c,r); invert=false; break;
		case 5: SetFilter_4PoleEQ2(c,r); invert=true; break;
		case 6: SetFilter_Vocal1(c,r);invert=false; break;
		case 7: SetFilter_Vocal1(c,r);invert=true; break;
		case 8: SetFilter_Vocal2(c,r);invert=false; break;
		case 9: SetFilter_Vocal2(c,r);invert=true; break;
		}
		_type=type;
	}
	float cutoff;
	int type;
	float resonance;
	float depth;
	float in1,in2,in3,in4;
		float f, p, q;             //filter coefficients 
		float b0, b1, b2, b3, b4;  //filter buffers (beware denormals!) 


	void setmoog1(int t, float c, int r)
	{
		type=t;
		//in[x] and out[x] are member variables, init to 0.0 the controls: 

		//fc = cutoff, nearly linear [0,1] -> [0, fs/2] 
		//res = resonance [0, 4] -> [no resonance, self-oscillation]
		cutoff=CUTOFFCONV(c)*1.16f/(sr/2.0f);
		resonance = (r/60.0f) * (1.0f - (0.15f * cutoff * cutoff)); 
	}

	float moog1(float input) 
	{ 
		input-=(b4 * resonance)+denormal;
		denormal = -denormal;
		float in0 = input * 0.35013f * (cutoff*cutoff)*(cutoff*cutoff) +denormal; 
		b1 = in0 + (0.3f * in1) + ((1.0f - cutoff) * b1) +denormal; // Pole 1 
		in1  = in0; 
		b2 = b1 + (0.3f * in2) + ((1.0f - cutoff) * b2)+denormal;  // Pole 2 
		in2  = b1; 
		b3 = b2 + (0.3f * in3) + ((1.0f - cutoff) * b3)+denormal;  // Pole 3 
		in3  = b2; 
		b4 = b3 + (0.3f * in4) + ((1.0f - cutoff) * b4)+denormal;  // Pole 4 
		// safety check

		if (b4 > 128.0f) 
			b4 = 128.0f;
		else if (b4 < -128.0f) 
			b4 = -128.0f;
		
		//
		in4  = b3; 
		switch (type)
		{
		case 0:
	// Lowpass  output:  b4 
			return b4; 
			break;
		case 1:
	// Highpass output:  in - b4; 
			return (input-b4); 
			break;
		default:
	// Bandpass output:  3.0f * (b3 - b4);
			return (b3 - b4)*3.0f; 
			break;
		}
	}


	
	void setmoog2(int t,float c, int r)
	{
		// Set coefficients given frequency & resonance [0.0...1.0] 
		type=t;

		cutoff=CUTOFFCONV(c)/(sr/2.0f);
		resonance = r/256.0f;
			q = 1.0f - cutoff; 
			p = cutoff + 0.8f * cutoff * q; 
			f = p + p - 1.0f; 
			q = resonance * (1.0f + 0.5f * q * (1.0f - q + 5.6f * q * q)); 
//								  b0= b1= b2= b3= b4 = 0;
	}



	float moog2(float in) 
	{ 
			float t1, t2;              //temporary buffers 
		// Filter (in [-1.0...+1.0]) 
			in -= (q * b4)+denormal;                          //feedback 
			t1 = b1;  
			t2 = b2;  
			t1 = b3;  
			b1 = (in + b0) * p - b1 * f +denormal;  
			b2 = (b1 + t1) * p - b2 * f +denormal; 
			b3 = (b2 + t2) * p - b3 * f +denormal; 
			b4 = (b3 + t1) * p - b4 * f +denormal; 
			b4 = b4 - b4 * b4 * b4 * 0.166667f +denormal;    //clipping 
			b0 = in; 

		denormal = -denormal;
			// safety check

			if (b4 > 1.0f)
			{
				b4 = 1+((b4-1)/16.0f);
				if (b4 > 4.0f) 
					b4 = 4.0f;
			}
			else  if (b4 < -1.0f) 
			{
				b4 = -1+((b4+1)/16.0f);
				if (b4 < -4.0f) 
					b4 = -4.0f;
			}

		// Lowpass  output:  b4 
		// Highpass output:  in - b4; 
		// Bandpass output:  3.0f * (b3 - b4);

			switch (type)
			{
			case 0:
		// Lowpass  output:  b4 
				return b4; 
				break;
			case 1:
		// Highpass output:  in - b4; 
				return (in-b4); 
				break;
			default:
		// Bandpass output:  3.0f * (b3 - b4);
				return (b3 - b4)*3.0f; 
				break;
			}
	}


	void setphaser(int s, float c, int r)
	{
		cutoff=CUTOFFCONV(c)/(sr/2.0f);

		type = s;
		resonance = r/360.0f;
		depth = (r+120)/240.0f;
		if (depth > 1.0f)
		{
			depth = 1.0f;
		}
	}
	float phaser(float inSamp)
	{
		for( int i=0; i<type; i++ )
			_alps[i].Delay( cutoff );
		float y = 0.f;
		switch (type)
		{
		case 1:
			y = 				_alps[0].Update(inSamp + denormal + (_zm1 * resonance));
			break;
		case 2:
			y = 				_alps[0].Update(
						_alps[1].Update(inSamp + denormal + (_zm1 * resonance)));
			break;
		case 3:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(inSamp + denormal + (_zm1 * resonance))));
			break;
		case 4:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(inSamp + denormal + (_zm1 * resonance)))));
			break;
		case 5:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(inSamp + denormal + (_zm1 * resonance))))));
			break;
		case 6:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update(inSamp + denormal + (_zm1 * resonance)))))));
			break;
		case 7:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update(inSamp + denormal + (_zm1 * resonance))))))));
			break;
		case 8:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update(inSamp + denormal + (_zm1 * resonance)))))))));
			break;
		case 9:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update(inSamp + denormal + (_zm1 * resonance))))))))));
			break;
		case 10:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update( 
								_alps[9].Update(inSamp + denormal + (_zm1 * resonance)))))))))));
			break;
		case 11:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update( 
								_alps[9].Update( 
								_alps[10].Update(inSamp + denormal + (_zm1 * resonance))))))))))));
			break;
		case 12:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update( 
								_alps[9].Update( 
								_alps[10].Update(
								_alps[11].Update(inSamp + denormal + (_zm1 * resonance)))))))))))));
			break;
		case 13:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update( 
								_alps[9].Update( 
								_alps[10].Update(
								_alps[11].Update( 
								_alps[12].Update(inSamp + denormal + (_zm1 * resonance))))))))))))));
			break;
		case 14:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update( 
								_alps[9].Update( 
								_alps[10].Update(
								_alps[11].Update( 
								_alps[12].Update( 
									_alps[13].Update(inSamp + denormal + (_zm1 * resonance)))))))))))))));
			break;
		case 15:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update( 
								_alps[9].Update( 
								_alps[10].Update(
								_alps[11].Update( 
								_alps[12].Update( 
									_alps[13].Update( 
									_alps[14].Update(inSamp + denormal + (_zm1 * resonance))))))))))))))));
			break;
		case 16:
			y = 				_alps[0].Update(
						_alps[1].Update(
						_alps[2].Update(
						_alps[3].Update(
						_alps[4].Update(
							_alps[5].Update( 
							_alps[6].Update( 
							_alps[7].Update( 
							_alps[8].Update( 
								_alps[9].Update( 
								_alps[10].Update(
								_alps[11].Update( 
								_alps[12].Update( 
									_alps[13].Update( 
									_alps[14].Update( 
									_alps[15].Update(inSamp + denormal + (_zm1 * resonance)))))))))))))))));
			break;
		}
		_zm1 = y;
		denormal = -denormal;
		return inSamp + y * depth;
	}
	class AllpassDelay{
	public:
		AllpassDelay()
			: _a1( 0.f )
			, _zm1( 0.f )
			{}

		void Wipe()
		{
			_a1 = _zm1 = 0;
		}
		void Delay( float delay )
		{ //sample delay time
			const static float 				denormal = (float)1.0E-18;

			_a1 = (1.f - delay) / (1.f + delay)+denormal;
		}

		float Update( float inSamp )
		{
			const static float 				denormal = (float)1.0E-18;
			float y = inSamp * -_a1 + _zm1+denormal;
			_zm1 = y * _a1 + inSamp+denormal;

			return y;
		}
	private:
		float _a1, _zm1;
	};

	AllpassDelay _alps[16];
	float _zm1;
	CBiquad Biquad,Biquad2;
	
protected:
	bool invert;
	int _type;
	float sr;
	float denormal;
};
}