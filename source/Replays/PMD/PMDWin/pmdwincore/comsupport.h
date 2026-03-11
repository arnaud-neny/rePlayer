//=============================================================================
//		COM Support header
//=============================================================================

#ifndef COMSUPPORT_H
#define COMSUPPORT_H

#include "portability_fmpmd.h"

#define	pmd_interface struct

class pmd_IUnknown {
public:
	virtual int AddRef(void) = 0;
		
	virtual int Release(void) = 0;
};

#endif	// COMSUPPORT_H
