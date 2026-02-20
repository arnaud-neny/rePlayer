#ifndef PORTABILITY_OPNA_H
#define PORTABILITY_OPNA_H


typedef unsigned int	uint;


#ifdef _WIN32
	#include <tchar.h>
	
	#if _MSC_VER >= 1600
	
		#include <stdint.h>
	
	#elif _MSC_VER >= 1310

		typedef unsigned char		uint8_t;
		typedef unsigned short		uint16_t;
		typedef unsigned int		uint32_t;
		typedef unsigned long long	uint64_t;
		typedef signed char			int8_t;
		typedef signed short		int16_t;
		typedef signed int			int32_t;
		typedef signed long long	int64_t;

	#else
		typedef unsigned char		uint8_t;
		typedef unsigned short		uint16_t;
		typedef unsigned int		uint32_t;
		typedef unsigned __int64	uint64_t;
		typedef signed char			int8_t;
		typedef signed short		int16_t;
		typedef signed int			int32_t;
		typedef __int64				int64_t;
		
	#endif
	

#else
	#include <stdio.h>
	#include <stdint.h>
	#include <limits.h>
	
	#define _T(x)			x
	typedef char			TCHAR;
	
	#define	_MAX_PATH	((PATH_MAX) > ((FILENAME_MAX)*4) ? (PATH_MAX) : ((FILENAME_MAX)*4))
	#define	_MAX_DIR	FILENAME_MAX
	#define	_MAX_FNAME	FILENAME_MAX
	#define	_MAX_EXT	FILENAME_MAX
	

#endif


#endif	// PORTABILITY_OPNA_H
