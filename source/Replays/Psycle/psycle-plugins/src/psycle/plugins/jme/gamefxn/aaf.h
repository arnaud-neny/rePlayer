/*								Blitz (C)2005 by jme
		Programm is based on Arguru Bass.
		Filter is a chebishev filter. Got information from this guide:
		http://www-users.cs.york.ac.uk/~fisher/mkfilter
	    A.J. Fisher, University of York   <fisher@minster.york.ac.uk>
	    September 1992

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
#include <complex>

#define MAXPOLES_ZEROS 6 // 2 * MAXORDER
namespace psycle::plugins::jme::gamefxn {
class AAF16 {
public:
	typedef enum {
		lowpass=0,
		highpass=1
	} mode_t;

	AAF16();
	~AAF16();
	inline float process(float NewSample);
	/*frequency Hz, sampleRate Hz, mode lowpass or highpass, ripple in dBs (negative) */
	void Setup(int frequency, mode_t mode, float ripple, /*int order,*/ int sampleRate);

protected:
	typedef struct
	{ std::complex<double> poles[MAXPOLES_ZEROS], zeros[MAXPOLES_ZEROS];
		int numpoles, numzeros;
	} plane_t;
	
	// compute S-plane poles for prototype LP filter
	void compute_s(int order, mode_t mode, float chebrip );
	void normalize(double raw_alpha, mode_t mode);
	void compute_z_blt();
	void expandpoly(double raw_alpha, mode_t mode);

	plane_t splane, zplane;
	float x[4]; //input samples
	float y[4]; //output samples
	float xcoef[4];
	float ycoef[4];
};

inline float AAF16::process(float NewSample){
	x[0] = x[1];
	x[1] = x[2];
	x[2] = x[3];
	x[3] = NewSample;

	y[0] = y[1];
	y[1] = y[2];
	y[2] = y[3];

	y[3]  = xcoef[3] * x[3];
	y[3] += xcoef[2] * x[2] + ycoef[2] * y[2];
	y[3] += xcoef[1] * x[1] + ycoef[1] * y[1];
	y[3] += xcoef[0] * x[0] + ycoef[0] * y[0];
	return y[3];
}
}