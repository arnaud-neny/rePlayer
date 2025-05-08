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

#include "gverbdsp.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace psycle::plugins::gverb {
/**********************************************************/
// diffuser

diffuser::diffuser(int size, float coeff)
:
	size_(size),
	coeff_(coeff),
	idx_(),
	buf_(new float[size])
{
	flush();
}

diffuser::~diffuser() {
	delete[] buf_;
}

void diffuser::flush() {
	std::memset(buf_, 0, size_ * sizeof *buf_);
}

/**********************************************************/
// fixeddelay

fixeddelay::fixeddelay(int size)
:
	size_(size),
	idx_(),
	buf_(new float[size])
{
	flush();
}

fixeddelay::~fixeddelay() {
	delete[] buf_;
}

void fixeddelay::flush() {
	std::memset(buf_, 0, size_ * sizeof *buf_);
}

#if 0
	int isprime(int n) {
		if(n == 2) return true;
		if((n & 1) == 0) return false;
		const unsigned int lim = (int) std::sqrt((float)n);
		for(unsigned int i = 3; i <= lim; i += 2)
			if((n % i) == 0) return false;
		return true;
	}

	/// relative error: new prime will be in range [n - n * rerror, n + n * rerror];
	int nearest_prime(int n, float rerror) {
		if(isprime(n)) return n;
		// assume n is large enough and n*rerror enough smaller than n
		int bound = n * rerror;
		for(int k = 1; k <= bound; ++k) {
			if(isprime(n + k)) return n + k;
			if(isprime(n - k)) return n - k;
		}
		return -1;
	}
#endif
}