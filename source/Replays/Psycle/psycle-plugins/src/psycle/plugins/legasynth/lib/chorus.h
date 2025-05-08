/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov LegaSynth plugins for PSYCLE

/***************************************************************************
                          chorus.h  -  description
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

#ifndef CHORUS_H
#define CHORUS_H

#include <vector>
#define INITIAL_BUFFER_SIZE 8192

#ifndef M_PI
	#define M_PI 3.14159265359
#endif
namespace psycle::plugins::legasynth {
/**
  *@author red
  */
class Chorus {

public:
	// Instance process: Create one with new or variable declaration, and call set_mixfreq before any other call.
	Chorus();
	~Chorus();

	void set_mixfreq(int i_mixfreq_hz);

	void process(float *p_lest,float *p_right,int p_amount);

	void set_delay(float idelay_ms);
	void set_lfo_speed(float ilfo_speed_hz);
	void set_lfo_depth(float ilfo_depth_ms);
	void set_feedback(float ifeedback_100);
	void set_width(float iwidth_100);
private:
	void recalc_delay();
	float mixfreq_hz;
		
	float delay_ms;
	float lfo_speed_hz;
	float lfo_depth_ms;
	float feedback_1;
	float width_1;

	float delay_samples;
	float depth_samples;
  
	int lfo_index; //lfo osc index
	int write_index; //write index.

	std::vector<float> ringbuffer_l;
	std::vector<float> ringbuffer_r;

};
}
#endif
