// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include <diversalis.hpp>
#if defined DIVERSALIS__OS__MICROSOFT
	#if defined DIVERSALIS__COMPILER__MICROSOFT
		#pragma warning(push) // don't let microsoft mess around with our warning settings
	#endif

	#if !defined WIN32_LEAN_AND_MEAN
		/// excludes rarely used stuff from windows headers.
		/// beware,
		/// when including <gdiplus.h>, MIDL_INTERFACE is not declared, needs <atlbase.h>.
		/// when including <mmsystem.h>, WAVEFORMATEX is not declared
		#define WIN32_LEAN_AND_MEAN
	#endif

	#if !defined VC_EXTRA_LEAN
		/// excludes some more of the rarely used stuff from windows headers.
		#define VC_EXTRA_LEAN
	#endif

	#ifndef _SECURE_ATL
		/// ???
		#define _SECURE_ATL 1
	#endif

	#if !defined NOMINMAX
		/// tells microsoft's headers not to pollute the global namespace with min and max macros (which break a lot of libraries, including the standard c++ library!)
		#define NOMINMAX
	#endif

	#if defined _AFXDLL // when mfc is used
		DIVERSALIS__MESSAGE("pre-compiling microsoft MFC headers.")
		#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // some CString constructors will be explicit
		#define _AFX_ALL_WARNINGS // turns off mfc's hiding of some common and often safely ignored warning messages
		#include <afxwin.h> // mfc core and standard components
		#include <afxext.h> // mfc extensions
		//#include <afxdisp.h> // mfc automation classes
		#ifndef _AFX_NO_OLE_SUPPORT
			#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
		#endif
		#ifndef _AFX_NO_AFXCMN_SUPPORT
			#include <afxcmn.h>                     // MFC support for Windows Common Controls
		#endif // _AFX_NO_AFXCMN_SUPPORT
		#include <afxmt.h> // multithreading?
		DIVERSALIS__MESSAGE("done pre-compiling microsoft MFC headers.")
	#else
		#if !defined WIN32_EXTRA_LEAN
			#define WIN32_EXTRA_LEAN // for mfc apps, we would get unresolved symbols
		#endif
		#include <windows.h>
	#endif

	// gdi+, must be included after <windows.h> or <afxwin.h>
	///\todo is gdi+ available with other compilers than microsoft's? by default, it's safer to assume it's not, as usual.
	#if defined DIVERSALIS__COMPILER__MICROSOFT ///\todo is gdi+ available with other compilers than microsoft's? by default, it's safer to assume it's not, as usual.
		#if defined _AFXDLL // when mfc is used, also include <gdiplus.h>
			//#include <atlbase.h> // for MIDL_INTERFACE used by <gdiplus.h>
			// gdi+ needs min and max in the root namespace :-(
			#ifndef max
			#define max(a, b) (((a) > (b)) ? (a) : (b))
			#endif
			#ifndef min
			#define min(a, b) (((a) < (b)) ? (a) : (b))
			#endif
			#include <gdiplus.h>
			#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
				#pragma comment(lib, "gdiplus")
			#endif
			#include <afxcontrolbars.h>
			#undef max
			#undef min
		#endif
	#endif

	#if defined min || defined max
		#error "min and/or max macros managed to creep up!"
	#endif

	#if defined DIVERSALIS__COMPILER__MICROSOFT
		#pragma warning(pop) // don't let microsoft mess around with our warning settings
	#endif
#endif
