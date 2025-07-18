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

// pwm.h: interface for the lfo class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
namespace psycle::plugins::jme::blitzn {
class pwm {
	public:
		pwm();
		virtual ~pwm() {}
		int getPosition();
		int getLast();
		/*Sets the range used by getPosition()/GetLast()*/
		void setRange(int val);
		void setSpeed(int val);
		void setSampleRate(int srate);
		void setSkipStep(int step);
		void reset();
		//Sample tick.
		inline void next();
		bool once;
		bool twice;
	private:
		int sym;
		int range;
		float frange;
		int speed;
		float srcorrection;
		int skipstep;
		int topvalue;
		int pos;
		int realpos;
		int last;
		int move;
		int direction;
};

inline void pwm::next() {
	if (range != 0) move = speed; else speed = 0;
	last=realpos;
	realpos=(int)((float)pos*frange);
	if (direction==1){
		pos-=move;
		if (pos<=0){
			pos=0;
			if (twice) direction+=2;
			else direction=0;
		}
	}
	else if (direction==0)
	{
		pos+=move;
		if (pos>=topvalue){
			pos=topvalue;
			if (once) direction+=2;
			else direction=1;
		}
	}
}
}