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

// lfo.h: interface for the lfo class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
namespace psycle::plugins::jme::blitzn {
class lfo {
	public:
		lfo();
		virtual ~lfo() {}
		int getPosition();
		int getLast();
		int getWhenNext();
		void setDelay(int val);
		void setLevel(int val);
		void setSpeed(int val);
		void setSkipStep(int val);
		void reset();
		inline bool next();
	private:
		int delay;
		int level;
		int speed;

		int skipstep;
		int pause;
		int phaseHi;
		int phaseLo;
		int offset;
		int coeff;
		int count;
		int current;
		int last;
};

inline bool lfo::next() {
	if (pause) {
		pause--;
		return false;
	}
	else {
		phaseLo++;
		if (count < 0){
			count=speed;
			phaseHi=(phaseHi+1)&3;
			phaseLo=0;
			if (((phaseHi&2)>>1)^(phaseHi&1)) coeff=0-level;
			else coeff=level;
			switch(phaseHi){
				case 1: offset=speed*level; break;
				case 3: offset=0-speed*level;break;
				default: offset=0; break;
			}
		}
		count--;
		last=current;
		current=phaseLo*coeff+offset;
		if(skipstep) pause=skipstep;
		return true;
	}
}
}