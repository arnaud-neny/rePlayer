#pragma once

#include <cstdlib>

namespace psycle { namespace helpers { namespace dsp {

	class Slider {
	public:
		Slider();
		~Slider() {};
		//////////////////////////////////////////////////////////////////
		//				Get / Set Methods
		//////////////////////////////////////////////////////////////////
		static std::size_t GetLength() { return m_length; }
		static void SetLength(std::size_t length);

		inline float GetTarget() { return m_target; }
		void SetTarget(float target);
	public:
		//////////////////////////////////////////////////////////////////
		//				Methods
		//////////////////////////////////////////////////////////////////
		void ResetTo(float target);
		inline float GetNext() {
			if (m_position < m_length) {
				const float m_lastVal = m_source + m_diff * l_table_[m_position];
				m_position++;
				return m_lastVal;
			}
			else {
				return m_target;
			}
		}
	private:
		float m_source;
		float m_target;
		float m_diff;
		std::size_t m_position;
		static std::size_t m_length;
		static float l_table_[2048];
	};
}}}
