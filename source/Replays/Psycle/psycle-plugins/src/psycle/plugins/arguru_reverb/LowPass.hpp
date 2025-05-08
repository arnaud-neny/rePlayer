#pragma once

namespace psycle::plugins::arguru_reverb {

class CLowpass {
	public:
		CLowpass();
		virtual ~CLowpass() throw() {}
		void setCutoff(float c);
		inline float Process(float i);

	private:
		float cutoff;
		float o1;
};

inline float CLowpass::Process(float i) {
	const float output= o1 + cutoff * (i-o1);
	o1=output;
	return output;
}
}