/***************************************************************************
                          mtptypes.h  -  basic types for MTPng
                             -------------------
    begin                : Thu Dec 9 1999
    copyright            : (C) 1999 by Ian Schmidt
    email                : ischmidt@cfl.rr.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __MTPTYPES_H__
#define __MTPTYPES_H__

/* provide basic types that we can guarantee.
 */

typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed long int32;
typedef unsigned long uint32;

// also define the platform name here
#ifdef AMIGA
#define PLAT_NAME "AmigaOS"
#else
#define PLAT_NAME "Linux/Unix"
#endif

// other includes everything needs
#include "baseclass.h"

#endif
