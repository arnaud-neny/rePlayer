#include "dspslide.hpp"

#include <algorithm>

namespace psycle { namespace helpers { namespace dsp {

	std::size_t Slider::m_length;
	float Slider::l_table_[2048];


	void Slider::SetLength(std::size_t length)
	{
		length=std::min((std::size_t)2048,length);
		if (m_length!=length) {
			double const resdouble = 1.0/(double)length;
            for(std::size_t i = 0; i <= length; ++i) {
				l_table_[i]=static_cast<float>((double)i * resdouble);
			}
			m_length=length;
		}
	}

	Slider::Slider()
		:m_source(0.0f)
		,m_target(0.f)
		,m_diff(0.f)
		,m_position(0)
	{
	}
	void Slider::ResetTo(float target)
	{
		m_source=m_target=target;
		m_diff=0.f;
		m_position=0;
	}
	void Slider::SetTarget(float target)
	{
		if (m_target != target) {
			if (m_position >= m_length) {
				m_position=0;
				m_source=m_target;
			}
			m_target=target;
			m_diff=m_target-m_source;
		}
	}
}}}
