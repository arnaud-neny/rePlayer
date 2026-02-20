#ifndef PORTABILITY_FMPMDCORE_H
#define PORTABILITY_FMPMDCORE_H

#include "portability_fmpmd.h"
#include "opna.h"



#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif


#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif



typedef struct stereosampletag
{
	FM::Sample left;
	FM::Sample right;
} StereoSample;

#pragma pack( push, enter_include1 )
#pragma pack(2)

typedef struct stereo16bittag
{
	short left;
	short right;
} Stereo16bit;

#pragma pack( pop, enter_include1 )




#endif	// PORTABILITY_FMPMDCORE_H
