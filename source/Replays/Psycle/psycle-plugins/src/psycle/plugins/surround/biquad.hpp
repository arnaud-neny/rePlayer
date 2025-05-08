#pragma once
#include <cmath>
#include <cstdlib>

/* Simple implementation of Biquad filters -- Tom St Denis
	*
	* Based on the work

Cookbook formulae for audio EQ biquad filter coefficients
---------------------------------------------------------
by Robert Bristow-Johnson, pbjrbj@viconet.com  a.k.a. robert@audioheads.com

	* Available on the web at

http://www.smartelectronix.com/musicdsp/text/filters005.txt

	* Enjoy.
	*
	* This work is hereby placed in the public domain for all purposes, whether
	* commercial, free [as in speech] or educational, etc.  Use the code and please
	* give me credit if you wish.
	*
	* Tom St Denis -- http://tomstdenis.home.dhs.org
*/


// KarLKoX : thanx to Tom St Denis for this code !


#ifndef M_LN2
#define M_LN2				   0.69314718055994530942
#endif

#ifndef M_PI
#define M_PI								3.14159265358979323846
#endif
namespace psycle::plugins::surround {
/* whatever sample type you want */
typedef double smp_type;

/* this holds the data required to update samples thru a filter */
typedef struct {
	smp_type a0, a1, a2, a3, a4;
	smp_type x1, x2, y1, y2;
}
biquad;

smp_type BiQuad(smp_type sample, biquad * b);
void BiQuad_new(int type, smp_type dbGain, /* gain of filter */
				smp_type freq,             /* center frequency */
				smp_type srate,            /* sampling rate */
				smp_type bandwidth,        /* bandwidth in octaves */
				biquad *bq,
				bool resetmemory=true);

/* filter types */
enum {
	LPF, /* low pass filter */
	HPF, /* High pass filter */
	BPF, /* band pass filter */
	NOTCH, /* Notch Filter */
	PEQ, /* Peaking band EQ filter */
	LSH, /* Low shelf filter */
	HSH /* High shelf filter */
};

// Modification with denormalization and memory NOT reset by [JAZ] 

smp_type id = smp_type(1.0E-20);
static inline smp_type Undenormalize(smp_type psamp)
{
	id = - id;
	return psamp + id;
}


/* Below this would be biquad.c */
/* Computes a BiQuad filter on a sample */
smp_type BiQuad(smp_type sample, biquad * b)
{
	smp_type result;

	/* compute result */
	result = b->a0 * sample + b->a1 * b->x1 + b->a2 * b->x2 -
		b->a3 * b->y1 - b->a4 * b->y2;

	/* shift x1 to x2, sample to x1 */
	b->x2 = b->x1;
	b->x1 = sample;

	/* shift y1 to y2, result to y1 */
	b->y2 = b->y1;
	b->y1 = Undenormalize(result);

	return result;
}

/* sets up a BiQuad Filter */
void BiQuad_new(int type, smp_type dbGain, smp_type freq,
smp_type srate, smp_type bandwidth, biquad *bq,bool reset_memory)
{
	smp_type A, omega, sn, cs, alpha, beta;
	smp_type a0, a1, a2, b0, b1, b2;

	/* setup variables */
	A = std::pow(10., dbGain / 40.);
	omega = 2 * M_PI * freq /srate;
	sn = sin(omega);
	cs = cos(omega);
	alpha = sn; // sn/(2*Q)
//    alpha = sn * sinh(M_LN2 /2 * bandwidth * omega /sn);
	beta = sqrt(A + A);

	switch (type) {
	case LPF:
		b0 = (1 - cs) /2;
		b1 = 1 - cs;
		b2 = (1 - cs) /2;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case HPF:
		b0 = (1 + cs) /2;
		b1 = -(1 + cs);
		b2 = (1 + cs) /2;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case BPF:
		b0 = alpha;
		b1 = 0;
		b2 = -alpha;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case NOTCH:
		b0 = 1;
		b1 = -2 * cs;
		b2 = 1;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case PEQ:
		b0 = 1 + (alpha * A);
		b1 = -2 * cs;
		b2 = 1 - (alpha * A);
		a0 = 1 + (alpha /A);
		a1 = -2 * cs;
		a2 = 1 - (alpha /A);
		break;
	case LSH:
		b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
		b1 = 2 * A * ((A - 1) - (A + 1) * cs);
		b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
		a0 = (A + 1) + (A - 1) * cs + beta * sn;
		a1 = -2 * ((A - 1) + (A + 1) * cs);
		a2 = (A + 1) + (A - 1) * cs - beta * sn;
		break;
	case HSH:
		b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
		b1 = -2 * A * ((A - 1) + (A + 1) * cs);
		b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
		a0 = (A + 1) - (A - 1) * cs + beta * sn;
		a1 = 2 * ((A - 1) - (A + 1) * cs);
		a2 = (A + 1) - (A - 1) * cs - beta * sn;
		break;
	default:
		return;
	}

	/* precompute the coefficients */
	bq->a0 = b0 /a0;
	bq->a1 = b1 /a0;
	bq->a2 = b2 /a0;
	bq->a3 = a1 /a0;
	bq->a4 = a2 /a0;

	if ( reset_memory)
	{
		/* zero initial samples */
		bq->x1 = bq->x2 = 0;
		bq->y1 = bq->y2 = 0;
	}
}
}