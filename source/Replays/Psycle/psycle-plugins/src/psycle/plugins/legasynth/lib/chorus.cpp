/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov LegaSynth plugins for PSYCLE

/***************************************************************************
                          chorus.cpp  -  description
                             -------------------
    begin                : Wed Jul 17 2002
    copyright            : (C) 2002 by red
    email                : red@server
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "chorus.h"
#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>
#include <cmath>
namespace psycle::plugins::legasynth {
Chorus::Chorus(){
	delay_ms=3;
	lfo_speed_hz=2;
	lfo_depth_ms=0.1f;
	feedback_1=0.64f;
	width_1=-0.078f;

	lfo_index=0;
	write_index=0;

	ringbuffer_l.assign(INITIAL_BUFFER_SIZE, 0.0f);
	ringbuffer_r.assign(INITIAL_BUFFER_SIZE, 0.0f);
}

Chorus::~Chorus(){
}

void Chorus::set_mixfreq(int i_mixfreq_hz) {
	mixfreq_hz=i_mixfreq_hz;
	recalc_delay();
}

void Chorus::set_delay(float idelay_ms) {
	delay_ms=idelay_ms;
	recalc_delay();
}
void Chorus::set_lfo_speed(float ilfo_speed_hz) {
	lfo_speed_hz=ilfo_speed_hz;
}
void Chorus::set_lfo_depth(float ilfo_depth_ms) {
	lfo_depth_ms=ilfo_depth_ms;
	recalc_delay();
}
void Chorus::set_feedback(float ifeedback_100) {
	feedback_1=ifeedback_100*0.01f;
}
void Chorus::set_width(float iwidth_100) {
	width_1=iwidth_100*0.01f;
}

void Chorus::recalc_delay() {

	delay_samples = delay_ms*mixfreq_hz * 0.001f;
	depth_samples = lfo_depth_ms*mixfreq_hz * 0.001f;

	if (delay_samples+depth_samples+1>=ringbuffer_l.size()){
		ringbuffer_l.resize(delay_samples+depth_samples+1, 0.0f);
		ringbuffer_r.resize(delay_samples+depth_samples+1, 0.0f);
	}
}
//todo: Improve CPU Usage. (comes from the std:sin and from the vector)
void Chorus::process(float *p_left,float *p_right,int p_amount) {

	int const BUF_SIZE = ringbuffer_l.size();

	double lfo_speed_l = lfo_speed_hz * 2.0 * M_PI/mixfreq_hz;
	double lfo_speed_r = lfo_speed_hz * 2.0 * (M_PI+M_PI*0.25)/mixfreq_hz;

	while (p_amount--) {
		ringbuffer_l[write_index]=*p_left;
		ringbuffer_r[write_index]=*p_right;

		float val=0, left=0, right=0;
		{
			float fraction_part = depth_samples * std::sin(lfo_index*lfo_speed_l);
			const int integral_part = std::floor(fraction_part);
			fraction_part = fraction_part - integral_part;

			int read = write_index - (delay_samples+depth_samples) + integral_part;
			read = (read + BUF_SIZE) % BUF_SIZE;
			int next_value = (read + 1) % BUF_SIZE;

			val = ringbuffer_l[read]*(1.0f-fraction_part) + ringbuffer_l[next_value] * fraction_part;
			left=val*feedback_1+*p_left;
			psycle::helpers::math::erase_all_nans_infinities_and_denormals(left);
		}
		{
			float fraction_part = depth_samples * std::sin(lfo_index*lfo_speed_r);
			const int integral_part = std::floor(fraction_part);
			fraction_part = fraction_part - integral_part;
			
			int read = write_index - (delay_samples+depth_samples) + integral_part;
			read = (read + BUF_SIZE) % BUF_SIZE;
			int next_value = (read + 1) % BUF_SIZE;

			val = ringbuffer_r[read] * ( 1.0f - fraction_part) + ringbuffer_r[next_value] * fraction_part;
			right=val*feedback_1+*p_right;
			psycle::helpers::math::erase_all_nans_infinities_and_denormals(right);
		}

		val=right;
		right=(right * (1-width_1)) + (left * width_1);
		left=(val * width_1) + (left * (1-width_1));

		*p_left++ =left;
		*p_right++ =right;

		lfo_index++;

		write_index++;
		write_index%=BUF_SIZE;
    }
}
}