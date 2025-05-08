/*

		Copyright (C) 1999 Juhana Sadeharju
						kouhia at nic.funet.fi

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	*/

#include "gverb.h"
#include "gverbdsp.h"
#include <cstdlib>
#include <malloc.h>
#include <cmath>
#include <cstring>

namespace psycle::plugins::gverb {
gverb::gverb(
	int rate, float maxroomsize, float roomsize,
	float revtime, float fdndamping, float spread,
	float inputbandwidth, float earlylevel, float taillevel
)
:
	rate_(rate),
	inputbandwidth_(inputbandwidth),
	taillevel_(taillevel),
	earlylevel_(earlylevel),
	inputdamper_(1.0 - inputbandwidth),
	maxroomsize_(maxroomsize),
	roomsize_(roomsize),
	revtime_(revtime),
	maxdelay_(rate * maxroomsize / 340.0),
	largestdelay_(rate * roomsize / 340.0),
	fdndamping_(fdndamping),
	tapdelay_(44000)
{
	// FDN section
	{
		fdndels_ = new fixeddelay*[FDNORDER];
		for(int i = 0; i < FDNORDER; ++i) {
			fdndels_[i] = new fixeddelay(maxdelay_ + 1000);
		}
		fdngains_ = new float[FDNORDER];
		fdnlens_ = new int[FDNORDER];

		fdndamps_ = new damper*[FDNORDER];
		for(int i = 0; i < FDNORDER; ++i) {
			fdndamps_[i] = new damper(fdndamping_);
		}

		float ga = 60;
		float gt = revtime_;
		ga = std::pow(10.0f, -ga / 20.0f);
		int n = rate_ * gt;
		alpha_ = std::pow((double) ga, 1.0 / n);

		float gb = 0;
		for(int i = 0; i < FDNORDER; ++i) {
			if (i == 0) gb = 1.000000 * largestdelay_;
			if (i == 1) gb = 0.816490 * largestdelay_;
			if (i == 2) gb = 0.707100 * largestdelay_;
			if (i == 3) gb = 0.632450 * largestdelay_;

			#if 0
				fdnlens_[i] = nearest_prime(gb, 0.5);
			#else
				fdnlens_[i] = std::floor(gb);
			#endif

			fdngains_[i] = -std::pow(alpha_, fdnlens_[i]);
		}

		d_ = new float[FDNORDER];
		u_ = new float[FDNORDER];
		f_ = new float[FDNORDER];
	}

	// Diffuser section
	{
		float diffscale = (float) fdnlens_[3] / (210 + 159 + 562 + 410);
		float spread1 = spread;
		float spread2 = spread * 3.0f;

		int a, b, c, cc, d, dd, e;
		float r;
		b = 210;
		r = 0.125541f;
		a = spread1 * r;
		c = 210 + 159 + a;
		cc = c-b;
		r = 0.854046f;
		a = spread2 * r;
		d = 210 + 159 + 562 + a;
		dd = d - c;
		e = 1341 - d;

		ldifs_ = new diffuser*[4];
		ldifs_[0] = new diffuser(diffscale * b, 0.75);
		ldifs_[1] = new diffuser(diffscale * cc, 0.75);
		ldifs_[2] = new diffuser(diffscale * dd, 0.625);
		ldifs_[3] = new diffuser(diffscale * e, 0.625);

		b = 210;
		r = -0.568366f;
		a = spread1 * r;
		c = 210 + 159 + a;
		cc = c - b;
		r = -0.126815f;
		a = spread2 * r;
		d = 210 + 159 + 562 + a;
		dd = d - c;
		e = 1341 - d;

		rdifs_ = new diffuser*[4];
		rdifs_[0] = new diffuser(diffscale * b, 0.75);
		rdifs_[1] = new diffuser(diffscale * cc, 0.75);
		rdifs_[2] = new diffuser(diffscale * dd, 0.625);
		rdifs_[3] = new diffuser(diffscale * e, 0.625);
	}

	// Tapped delay section

	taps_ = new int[FDNORDER];
	tapgains_ = new float[FDNORDER];

	taps_[0] = 5 + 0.410 * largestdelay_;
	taps_[1] = 5 + 0.300 * largestdelay_;
	taps_[2] = 5 + 0.155 * largestdelay_;
	taps_[3] = 5 + 0.000 * largestdelay_;

	for(int i = 0; i < FDNORDER; ++i) {
		tapgains_[i] = std::pow(alpha_, taps_[i]);
	}
}

gverb::~gverb() {
	for(int i = 0; i < FDNORDER; ++i) {
		delete fdndels_[i];
		delete fdndamps_[i];
		delete ldifs_[i];
		delete rdifs_[i];
	}
	delete[] fdndels_;
	delete[] fdngains_;
	delete[] fdnlens_;
	delete[] fdndamps_;
	delete[] d_;
	delete[] u_;
	delete[] f_;
	delete[] ldifs_;
	delete[] rdifs_;
	delete[] taps_;
	delete[] tapgains_;
}

void gverb::flush() {
	inputdamper_.flush();
	for(int i = 0; i < FDNORDER; ++i) {
		fdndels_[i]->flush();
		fdndamps_[i]->flush();
		ldifs_[i]->flush();
		rdifs_[i]->flush();
	}
	std::memset(d_, 0, FDNORDER * sizeof *d_);
	std::memset(u_, 0, FDNORDER * sizeof *u_);
	std::memset(f_, 0, FDNORDER * sizeof *f_);
	tapdelay_.flush();
}
}