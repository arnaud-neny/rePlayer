/*								Blitz (C)2005 by jme
		Programm is based on Arguru Bass. Filter seems to be Public Domain.

		This plugin is free software; you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation; either version 2 of the License, or
		(at your option) any later version.\n"\

		This plugin is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with this program; if not, write to the Free Software
		Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


// pwm.cpp: implementation of the pwm class.
//
//////////////////////////////////////////////////////////////////////

#include "pwm.h"
#include <assert.h>
namespace psycle::plugins::jme::blitzn {
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

pwm::pwm()
:
	once(false),
	twice(false),
	sym(0),
	range(0),
	frange(0.f),
	speed(0),
	srcorrection(1.f),
	skipstep(1),
	topvalue(2047),
	pos(0),
	realpos(0),
	last(0),
	move(0),
	direction(0)
{}

int pwm::getPosition() { return realpos; }
int pwm::getLast(){ return last; }
void pwm::setRange(int val) { range= val; frange = val * 1.f/(float)topvalue; }
void pwm::setSpeed(int val) { assert(val>=0); speed=2*val; }
void pwm::setSampleRate(int val) {
 	assert(val>0); 
	srcorrection=(float)val/44100.f;
	topvalue=2047.f*srcorrection*skipstep;
	if(pos>topvalue) pos=topvalue;
	setRange(range);
}
void pwm::setSkipStep(int val) {
 	assert(val>0); 
	skipstep=val;
	topvalue=2047.f*srcorrection*skipstep;
	if(pos>topvalue) pos=topvalue;
	setRange(range);
}
void pwm::reset(){ last=9999; pos=0; direction=0;}
}