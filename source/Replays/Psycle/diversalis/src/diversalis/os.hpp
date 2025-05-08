// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\file
///\brief compiler-independant meta-information about the operating system.
#pragma once
#include "compiler.hpp"

#if defined DIVERSALIS__COMPILER__DOXYGEN

	/**********************************************************************************/
	// documentation about what is defined in this file

	///\name operating system kind
	///\{
		/// unix, autodetected via __unix__.
		#define DIVERSALIS__OS__UNIX
		#undef DIVERSALIS__OS__UNIX // was just defined to insert documentation.

		/// the posix standard, autodetected.
		#define DIVERSALIS__OS__POSIX
		#undef DIVERSALIS__OS__POSIX // was just defined to insert documentation.

		/// linux kernel, autodetected via __linux__.
		#define DIVERSALIS__OS__LINUX
		#undef DIVERSALIS__OS__LINUX // was just defined to insert documentation.

		/// bsd kernel, autodetected for apple's darwin/macos mach/bsd kernel via __STRICT_BSD__, \see DIVERSALIS__OS__DARWIN.
		#define DIVERSALIS__OS__BSD
		#undef DIVERSALIS__OS__BSD // was just defined to insert documentation.

		/// hurd kernel, \see DIVERSALIS__OS__MACH.
		#define DIVERSALIS__OS__HURD
		#undef DIVERSALIS__OS__HURD // was just defined to insert documentation.

		/// mach kernel.
		#define DIVERSALIS__OS__MACH
		#undef DIVERSALIS__OS__MACH // was just defined to insert documentation.

		/// darwin/mach kernel, \see DIVERSALIS__OS__MACH and \see DIVERSALIS__OS__BSD, and for apple's darwin/macos \see DIVERSALIS__OS__APPLE.
		#define DIVERSALIS__OS__DARWIN
		#undef DIVERSALIS__OS__DARWIN // was just defined to insert documentation.

		/// apple's macosx darwin mach/bsd kernel. \see DIVERSALIS__OS__DARWIN.
		#define DIVERSALIS__OS__APPLE
		#undef DIVERSALIS__OS__APPLE // was just defined to insert documentation.

		/// cygwin, autodetected via __CYGWIN__.
		#define DIVERSALIS__OS__CYGWIN
		#undef DIVERSALIS__OS__CYGWIN // was just defined to insert documentation.

		/// gnu operating system, autodetected.
		#define DIVERSALIS__OS__GNU
		#undef DIVERSALIS__OS__GNU // was just defined to insert documentation.

		/// microsoft's windows.
		/// note: on cygwin, DIVERSALIS__OS__MICROSOFT is not defined, as it's considered to be in the unix family.
		#define DIVERSALIS__OS__MICROSOFT
		#undef DIVERSALIS__OS__MICROSOFT // was just defined to insert documentation.

		/// google's native-client (NaCl aka Pepper)
		#define DIVERSALIS__OS__NACL
		#undef DIVERSALIS__OS__NACL // was just defined to insert documentation.
	///\}

	///\name operating system version
	///\{
		/// operating system version, as an integral number.
		/// This combines the major, minor and patch numbers into a single integral number.
		#define DIVERSALIS__OS__VERSION
		#undef DIVERSALIS__OS__VERSION // was just defined to insert documentation.
		/// operating system version, major number.
		#define DIVERSALIS__OS__VERSION__MAJOR
		#undef DIVERSALIS__OS__VERSION__MAJOR // was just defined to insert documentation.
		/// operating system version, minor number.
		#define DIVERSALIS__OS__VERSION__MINOR
		#undef DIVERSALIS__OS__VERSION__MINOR // was just defined to insert documentation.
		/// operating system version, patch number.
		#define DIVERSALIS__OS__VERSION__PATCH
		#undef DIVERSALIS__OS__VERSION__PATCH // was just defined to insert documentation.
	///\}

	///\name operating system binary format
	///\{
		/// ELF binary format
		#define DIVERSALIS__OS__BIN_FMT__ELF
		#undef DIVERSALIS__OS__BIN_FMT__ELF // was just defined to insert documentation.

		/// mac-o binary format
		#define DIVERSALIS__OS__BIN_FMT__MAC_O
		#undef DIVERSALIS__OS__BIN_FMT__MAC_O // was just defined to insert documentation.

		/// PE binary format
		#define DIVERSALIS__OS__BIN_FMT__PE
		#undef DIVERSALIS__OS__BIN_FMT__PE // was just defined to insert documentation.
	///\}

#endif

/**********************************************************************************/
// now the real work

/////////
// linux

#if defined __linux__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__LINUX

////////////
// gnu hurd

#elif defined __GNU__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__GNU
	#define DIVERSALIS__OS__HURD

////////
// *bsd

#elif defined __FreeBSD__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__BSD
	#define DIVERSALIS__OS__FREEBSD

#elif defined __NetBSD__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__BSD
	#define DIVERSALIS__OS__NETBSD

#elif defined __OpenBSD__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__BSD
	#define DIVERSALIS__OS__OPENBSD

////////////////////////
// darwin/apple's macos

#elif defined __APPLE__ && defined __MACH__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#if defined __STRICT_BSD__
		#define DIVERSALIS__OS__BSD
	#endif
	#define DIVERSALIS__OS__MACH
	#define DIVERSALIS__OS__DARWIN
	#define DIVERSALIS__OS__APPLE
	
///////////////
// sun solaris

#elif defined _sun
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#if defined __SVR4
		#define DIVERSALIS__OS__SOLARIS
	#else
		#define DIVERSALIS__OS__SUNOS // ancient system!
	#endif

////////////
// ibm aix

#elif defined _AIX
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__AIX

////////////
// sgi irix

#elif defined __sgi
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__IRIX

////////
// hpux

#elif defined __hpux
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__HPUX

//////////
// cygwin

#elif defined __CYGWIN__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__CYGWIN

////////
// msys

#elif defined __MSYS__
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__MSYS

////////
// uwin

#elif defined _UWIN
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
	#define DIVERSALIS__OS__UWIN

///////////////////////
// microsoft's windows

#elif defined _WIN32
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__MICROSOFT
	#define DIVERSALIS__OS__VERSION WINVER

	//////////////////
	// WINVER
	// _WIN32_WINDOWS
	// _WIN32_NT
	//////////////////

	#if defined DIVERSALIS__OS__MICROSOFT__REQUIRED_VERSION
		// WINVER
		#if !defined WINVER
			#define WINVER DIVERSALIS__OS__MICROSOFT__REQUIRED_VERSION
		#elif WINVER < DIVERSALIS__OS__MICROSOFT__REQUIRED_VERSION
			#error "WINVER too low."
		#endif
		#if !WINVER
			#error "WINVER is zero."
		#endif

		// _WIN32_WINDOWS
		#if !defined _WIN32_WINDOWS
			#define _WIN32_WINDOWS DIVERSALIS__OS__MICROSOFT__REQUIRED_VERSION
		#elif _WIN32_WINDOWS < DIVERSALIS__OS__MICROSOFT__REQUIRED_VERSION
			#error "_WIN32_WINDOWS too low."
		#endif
		#if !_WIN32_WINDOWS
			#error "_WIN32_WINDOWS is zero."
		#endif

		// _WIN32_WINNT
		#if !defined _WIN32_WINNT
			#define _WIN32_WINNT DIVERSALIS__OS__MICROSOFT__REQUIRED_VERSION
		#elif _WIN32_WINNT < DIVERSALIS__OS__MICROSOFT__REQUIRED_VERSION
			#error "_WIN32_WINNT too low."
		#endif
		#if !_WIN32_WINNT
			#error "_WIN32_WINNT is zero."
		#endif
	#endif

/////////////////////////////////////////////
/// google's native-client (NaCl aka Pepper)

#elif defined __native_client__
	#define DIVERSALIS__OS__NACL

////////////////
// generic unix

#elif defined __unix__ || defined unix
	#define DIVERSALIS__OS
	#define DIVERSALIS__OS__UNIX
#endif

/**********************************************************************************/
// consistency check

#if !defined DIVERSALIS__OS && !defined DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
	#error "unknown os"
#endif

/**********************************************************************************/
// inferences

// operating system binary format
#if defined __ELF__
	#define DIVERSALIS__OS__BIN_FMT
	#define DIVERSALIS__OS__BIN_FMT__ELF
#elif defined DIVERSALIS__OS__DARWIN
	#define DIVERSALIS__OS__BIN_FMT
	#define DIVERSALIS__OS__BIN_FMT__MAC_O
#elif \
	defined DIVERSALIS__OS__MICROSOFT || \
	defined DIVERSALIS__OS__CYGWIN || \
	defined DIVERSALIS__OS__MSYS || \
	defined DIVERSALIS__OS__UWIN
	#define DIVERSALIS__OS__BIN_FMT
	#define DIVERSALIS__OS__BIN_FMT__PE
#endif

// operating system kernels for gnu operating system applications
#if \
	defined DIVERSALIS__OS__HURD || \
	defined DIVERSALIS__OS__LINUX || \
	defined DIVERSALIS__OS__CYGWIN
	#define DIVERSALIS__OS__GNU
#endif

// operating systems following the posix standard
#if \
	defined DIVERSALIS__OS__GNU || \
	defined DIVERSALIS__OS__BSD || \
	defined DIVERSALIS__OS__MACH
	#define DIVERSALIS__OS__POSIX
#endif
