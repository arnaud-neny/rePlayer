/*								Blitz (C)2005 by jme
		Programm is based on Arguru Bass. 
		Filter is a chebishev filter. Got information from this guide:
		http://www-users.cs.york.ac.uk/~fisher/mkfilter

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

#include "aaf.h"
#include <psycle/helpers/math.hpp>
#include <stdexcept>
#include <sstream>
#include <cstdio>
namespace psycle::plugins::jme::gamefxn {
using psycle::helpers::math::pi;
using psycle::helpers::math::asinh;

std::complex<double> blt(std::complex<double>& pz)
{
	return (std::complex<double>(2.0,0.0) + pz) / (std::complex<double>(2.0,0.0) - pz);
}

std::complex<double> expj(double theta)
{
	return std::complex<double>(cos(theta), sin(theta));
}

// evaluate polynomial in z, substituting for z
std::complex<double> eval(std::complex<double> coeffs[], int npz, std::complex<double>& myz)
{
	std::complex<double> sum(0.0, 0.0);
	for (int i = npz; i >= 0; i--) sum = (sum * myz) + coeffs[i];
	return sum;
}
// evaluate response, substituting for z
std::complex<double> evaluate(std::complex<double> topco[], int nz, std::complex<double> botco[], int np, std::complex<double>& myz)
{
	std::complex<double> numerator = eval(topco, nz, myz);
	std::complex<double> denominator = eval(botco, np, myz);
	return numerator / denominator;
}

// multiply factor (z-w) into coeffs
void multin(std::complex<double>& w, int npz, std::complex<double> coeffs[])
{
	std::complex<double> nw = -w;
	for (int i = npz; i >= 1; i--) coeffs[i] = (nw * coeffs[i]) + coeffs[i-1];
	coeffs[0] = nw * coeffs[0];
}

// compute product of poles or zeros as a polynomial of z
static void expand(std::complex<double> pz[], int npz, std::complex<double> coeffs[])
{
	double const epsilon = 1e-10;
	coeffs[0] = 1.0;
	for(int i = 0; i < npz; ++i) coeffs[i + 1] = 0.0;
	for(int i = 0; i < npz; ++i) multin(pz[i], npz, coeffs);
	// check computed coeffs of z^k are all real
	for(int i = 0; i < npz + 1; ++i) {
		if(std::abs(coeffs[i].imag()) > epsilon) {
			std::ostringstream s;
			s << "coeff of z^" << i << " is not real; poles/zeros are not complex conjugates.";
			throw std::runtime_error(s.str());
		}
	}
}

// Chebyshev Lowpass, 3rd order
// Cutoff: 16 KHz
// Samplerate: 705KHz (44KHz x 16)
// -3dB Pass Band Ripple
AAF16::AAF16(){
	for (int c=0; c<4; c++){
		x[c]=0.0f;
		y[c]=0.0f;
	}
	xcoef[3] = 0.00008706096478462893f;
	xcoef[2] = 0.00026118289435388679f;
	xcoef[1] = 0.00026118289435388679f;
	xcoef[0] = 0.00008706096478462893f;
	ycoef[2] = 2.90004956629741750000f;
	ycoef[1] = -2.81919480076970650000f;
	ycoef[0] = 0.91844977163113484000f;
}

AAF16::~AAF16(){
}

void AAF16::Setup(int frequency, mode_t mode, float ripple, /*int order,*/ int sampleRate) {
	int order = 3;

	compute_s(order, mode, ripple);
	normalize((double)frequency/sampleRate, mode);
	compute_z_blt();
	expandpoly((double)frequency/sampleRate, mode);
}

 // compute S-plane poles for prototype LP filter
void AAF16::compute_s(int order, mode_t mode, float chebrip )
{ 
	splane.numpoles = 0;

	for (int i = 0; i < 2*order; i++)
	{ double theta = (order & 1) ? (i*pi) / order : ((i+0.5)*pi) / order;
		std::complex<double> pole = expj(theta);
		if (pole.real() < 0.0) {
			splane.poles[splane.numpoles++] = pole;
		}
	}
	// modify for Chebyshev (p. 136 DeFatta et al.)
	if (chebrip >= 0.0)
	{ fprintf(stderr, "mkfilter: Chebyshev ripple is %g dB; must be .lt. 0.0\n", chebrip);
		throw((int)1);
	}
	double rip = pow(10.0, -chebrip / 10.0);
	double eps = sqrt(rip - 1.0);
	double y = asinh(1.0 / eps) / (double) order;
	if (y <= 0.0)
	{ fprintf(stderr, "mkfilter: bug: Chebyshev y=%g; must be .gt. 0.0\n", y);
		throw((int)1);
	}
	for (int i = 0; i < splane.numpoles; i++)
	{ splane.poles[i] = std::complex<double>(splane.poles[i].real() * sinh(y),
							splane.poles[i].imag() * cosh(y));
	}
}		


void AAF16::normalize(double raw_alpha, mode_t mode)
{ 
	// for bilinear transform, perform pre-warp on alpha values
	double 	warped_alpha = tan(pi * raw_alpha) / pi;

	double w1 = 2.0*pi * warped_alpha;
	switch(mode) {
	// transform prototype into appropriate filter type
	case lowpass:
		{ for (int i = 0; i < splane.numpoles; i++) splane.poles[i] = splane.poles[i] * w1;
			splane.numzeros = 0;
			break;
		}
	case highpass:
		{
			for (int i = 0; i < splane.numpoles; i++) splane.poles[i] = w1 / splane.poles[i];
			for (int i = 0; i < splane.numpoles; i++) splane.zeros[i] = 0.0;	 /* also N zeros at (0,0) */
			splane.numzeros = splane.numpoles;
			break;
		}
	}
}

 // given S-plane poles & zeros, compute Z-plane poles & zeros, by bilinear transform
void AAF16::compute_z_blt()
{
	zplane.numpoles = splane.numpoles;
	zplane.numzeros = splane.numzeros;
	for (int i=0; i < zplane.numpoles; i++) zplane.poles[i] = blt(splane.poles[i]);
	for (int i=0; i < zplane.numzeros; i++) zplane.zeros[i] = blt(splane.zeros[i]);
	while (zplane.numzeros < zplane.numpoles) zplane.zeros[zplane.numzeros++] = -1.0;
}


// given Z-plane poles & zeros, compute top & bot polynomials in Z, and then recurrence relation
void AAF16::expandpoly(double raw_alpha, mode_t mode)
{
	std::complex<double> topcoeffs[MAXPOLES_ZEROS+1], botcoeffs[MAXPOLES_ZEROS+1];
	expand(zplane.zeros, zplane.numzeros, topcoeffs);
	expand(zplane.poles, zplane.numpoles, botcoeffs);
	for (int i = 0; i <= zplane.numzeros; i++) xcoef[i] = +(topcoeffs[i].real() / botcoeffs[zplane.numpoles].real());
	for (int i = 0; i <= zplane.numpoles; i++) ycoef[i] = -(botcoeffs[i].real() / botcoeffs[zplane.numpoles].real());

	// correct gain.
	switch(mode) {
	case lowpass:
		{ 
			std::complex<double> mydouble(1.0,0.0);
			std::complex<double> dc_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, mydouble);
			double gain = hypot(dc_gain.imag(), dc_gain.real());
			for (int i = 0; i <= zplane.numzeros; i++) xcoef[i] /= gain;
			break;
		}
	case highpass:	
		{
			std::complex<double> mydouble(-1.0,0.0);
			std::complex<double> hf_gain = evaluate(topcoeffs, zplane.numzeros, botcoeffs, zplane.numpoles, mydouble);
			double gain = hypot(hf_gain.imag(), hf_gain.real());
			for (int i = 0; i <= zplane.numzeros; i++) xcoef[i] /= gain;
			break;
		}
	}
}


}