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


// lfo.cpp: implementation of the lfo class.
//
//////////////////////////////////////////////////////////////////////

#include "lfo.h"
namespace psycle::plugins::jme::blitz12 {
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

lfo::lfo()
:
	delay(0),
	pause(0),
	phaseHi(0),
	phaseLo(0),
	offset(0),
	count(0),
	current(0),
	last(0)
{}

int lfo::getPosition() { return current; }
int lfo::getLast() { return 0; }
void lfo::setDelay(int val) { delay = val; }
void lfo::setLevel(int val) { level = val; if (val==0) coeff=0; }
void lfo::setSpeed(int val) { speed = val; }
void lfo::reset(){ count=-1; phaseHi=3; pause=delay; current=0; }
}