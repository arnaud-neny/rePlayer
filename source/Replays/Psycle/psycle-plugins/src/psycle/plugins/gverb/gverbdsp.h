#ifndef PSYCLE__PLUGINS__GVERB__GVERB_DSP__INCLUDED
#define PSYCLE__PLUGINS__GVERB__GVERB_DSP__INCLUDED
#pragma once

#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>
namespace psycle::plugins::gverb {
/**********************************************************/
// diffuser

class diffuser {
	public:
		diffuser(int size, float coeff);
		~diffuser();
		void flush();
		float process(float x) {
			float & buf(buf_[idx_]);
			float w = x - buf * coeff_;
			psycle::helpers::math::erase_all_nans_infinities_and_denormals(w);
			float y = buf + w * coeff_;
			buf = w;
			++idx_;
			idx_ %= size_;
			///\todo: denormal check on y too?
			return y;
		}
	private:
		int size_;
		float coeff_;
		int idx_;
		float * buf_;
};

/**********************************************************/
// damper

class damper {
	public:
		damper(float damping) : damping_(damping), delay_() {}
		void flush() { delay_ = 0.0f; }
		void set(float damping) { damping_ = damping; }
		inline float process(float x) {
			float y = x * (1 - damping_) + delay_ * damping_;
			psycle::helpers::math::erase_all_nans_infinities_and_denormals(y);
			delay_ = y;
			return y;
		}
	private:
		float damping_;
		float delay_;
};

/**********************************************************/
// fixeddelay

class fixeddelay {
	public:
		fixeddelay(int size);
		~fixeddelay();
		void flush();
		float read(int n) { return buf_[(idx_ - n + size_) % size_]; }
		void write(float x) { buf_[idx_] = x; ++idx_; idx_ %= size_; }
	private:
		int size_;
		int idx_;
		float * buf_;
};

/**********************************************************/
//int isprime(int);
//int nearest_prime(int, float);
}	
#endif
