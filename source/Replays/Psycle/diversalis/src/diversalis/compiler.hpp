// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2013 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\file
///\brief compiler-independant meta-information about the compiler
#pragma once
#include "detail/stringize.hpp"

#if defined DIVERSALIS__COMPILER__DOXYGEN

	/**********************************************************************************/
	// documentation about what is defined in this file

	///\name meta-information about the compiler's version
	///\{
		/// compiler name
		#define DIVERSALIS__COMPILER__NAME <string>
		#undef DIVERSALIS__COMPILER__NAME // was just defined to insert documentation.

		/// compiler name and version, as a string.
		#define DIVERSALIS__COMPILER__VERSION__STRING <string>
		#undef DIVERSALIS__COMPILER__VERSION__STRING // was just defined to insert documentation.

		/// compiler version, as an integral number.
		/// This combines the major, minor and patch numbers into a single integral number.
		#define DIVERSALIS__COMPILER__VERSION <number>
		#undef DIVERSALIS__COMPILER__VERSION // was just defined to insert documentation.

		/// compiler version, major number.
		#define DIVERSALIS__COMPILER__VERSION__MAJOR <number>
		#undef DIVERSALIS__COMPILER__VERSION__MAJOR // was just defined to insert documentation.

		/// compiler version, minor number.
		#define DIVERSALIS__COMPILER__VERSION__MINOR <number>
		#undef DIVERSALIS__COMPILER__VERSION__MINOR // was just defined to insert documentation.

		/// compiler version, patch number.
		#define DIVERSALIS__COMPILER__VERSION__PATCH <number>
		#undef DIVERSALIS__COMPILER__VERSION__PATCH // was just defined to insert documentation.

		/// compiler version, application binary interface number.
		#define DIVERSALIS__COMPILER__VERSION__ABI <number>
		#undef DIVERSALIS__COMPILER__VERSION__ABI // was just defined to insert documentation.
	///\}

	///\name meta-information about the compiler's features
	///\{
		/// indicates the compiler does not generate code for a processor.
		/// e.g.: netbeans-cnd or eclipse-cdt c++ indexers, the doxygen documentation compiler, resource compilers, etc.
		#define DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
		#undef DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE // was just defined to insert documentation.

		/// indicates the compiler supports run-time type information. i.e.: typeid, dynamic_cast<>
		#define DIVERSALIS__COMPILER__FEATURE__RTTI
		#undef DIVERSALIS__COMPILER__FEATURE__RTTI // was just defined to insert documentation.

		/// indicates the compiler supports exception handling. i.e.: try, catch, throw
		#define DIVERSALIS__COMPILER__FEATURE__EXCEPTION
		#undef DIVERSALIS__COMPILER__FEATURE__EXCEPTION // was just defined to insert documentation.

		/// indicates the compiler supports pre-compilation.
		#define DIVERSALIS__COMPILER__FEATURE__PRE_COMPILATION
		#undef DIVERSALIS__COMPILER__FEATURE__PRE_COMPILTION // was just defined to insert documentation.
	
		/// indicates the compiler supports auto-linking. i.e.: #pragma comment(lib, "foo")
		#define DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
		#undef DIVERSALIS__COMPILER__FEATURE__AUTO_LINK // was just defined to insert documentation.

		/// indicates the compiler has optimisations enabled.
		#define DIVERSALIS__COMPILER__FEATURE__OPTIMIZE
		#undef DIVERSALIS__COMPILER__FEATURE__OPTIMIZE // was just defined to insert documentation.

		///\todo doc
		#define DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
		#undef DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS // was just defined to insert documentation.

		/// indicates the compiler supports open-mp.
		/// returns an integer representing the date of the OpenMP specification implemented.
		#define DIVERSALIS__COMPILER__FEATURE__OPEN_MP _OPENMP
		#undef DIVERSALIS__COMPILER__FEATURE__OPEN_MP // was just defined to insert documentation.

		/// indicates the compiler supports some assembler language.
		#define DIVERSALIS__COMPILER__FEATURE__ASSEMBLER
		#undef DIVERSALIS__COMPILER__FEATURE__ASSEMBLER // was just defined to insert documentation.
	///\}

	///\name meta-information about the compiler's assembler language syntax
	///\{
		/// indicates the compiler's assembler language has at&t's syntax.
		#define DIVERSALIS__COMPILER__ASSEMBLER__ATT
		#undef DIVERSALIS__COMPILER__ASSEMBLER__ATT // was just defined to insert documentation.

		/// indicates the compiler's assembler language has intel's syntax.
		#define DIVERSALIS__COMPILER__ASSEMBLER__INTEL
		#undef DIVERSALIS__COMPILER__ASSEMBLER__INTEL // was just defined to insert documentation.
	///\}

	///\name meta-information about the compiler's brand
	///\{
		/// gnu g++/gcc, g++, autodetected via __GNUG__ (equivalent to __GNUC__ && __cplusplus).
		/// To see the predefined macros, run: g++ -xc++ -std=c++11 -dM -E /dev/null
		#define DIVERSALIS__COMPILER__GNU
		#undef DIVERSALIS__COMPILER__GNU // was just defined to insert documentation.

		/// llvm clang, autodetected via __clang__.
		/// To see the predefined macros, run: clang++ -xc++ -std=c++11 -dM -E /dev/null
		/// Note that clang disguises itself as gcc, so you will also have
		/// DIVERSALIS__COMPILER__GNU defined.
		#define DIVERSALIS__COMPILER__CLANG
		#undef DIVERSALIS__COMPILER__CLANG // was just defined to insert documentation.

		/// intel icc, autodetected via __INTEL_COMPILER.
		/// Note that icc disguises itself as gcc or msvc, so you will also have
		/// DIVERSALIS__COMPILER__GNU or DIVERSALIS__COMPILER__MICROSOFT defined.
		#define DIVERSALIS__COMPILER__INTEL
		#undef DIVERSALIS__COMPILER__INTEL // was just defined to insert documentation.

		/// bcc, autodetected via __BORLAND__.
		#define DIVERSALIS__COMPILER__BORLAND
		#undef DIVERSALIS__COMPILER__BORLAND // was just defined to insert documentation.

		/// msvc, autodetected via _MSC_VER.
		#define DIVERSALIS__COMPILER__MICROSOFT
		#undef DIVERSALIS__COMPILER__MICROSOFT // was just defined to insert documentation.

		#undef DIVERSALIS__COMPILER__DOXYGEN // redefine to insert documentation.
		/// doxygen documentation compiler. This is not autodetected and has to be manually defined.
		#define DIVERSALIS__COMPILER__DOXYGEN

		/// netbeans-cnd c++ indexer. This is not autodetected and has to be manually defined.
		#define DIVERSALIS__COMPILER__NETBEANS
		#undef DIVERSALIS__COMPILER__NETBEANS // was just defined to insert documentation.

		/// eclipse-cdt c++ indexer. This is not autodetected and has to be manually defined.
		#define DIVERSALIS__COMPILER__ECLIPSE
		#undef DIVERSALIS__COMPILER__ECLIPSE // was just defined to insert documentation.

		/// resource compiler (only relevant on microsoft's operating system), autodetected via RC_INVOKED.
		#define DIVERSALIS__COMPILER__RESOURCE
		#undef DIVERSALIS__COMPILER__RESOURCE // was just defined to insert documentation.
	///\}
#endif

/**********************************************************************************/
// now the real work

//////////////////////////////////
// doxygen documentation compiler

#if defined DIVERSALIS__COMPILER__DOXYGEN
	#define DIVERSALIS__COMPILER
	#define DIVERSALIS__COMPILER__NAME "doxygen"
	#define DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
	#if !defined __cplusplus
		#define __cplusplus
	#endif

////////////////////////////
// netbeans-cnd c++ indexer

#elif defined DIVERSALIS__COMPILER__NETBEANS
	#define DIVERSALIS__COMPILER
	#define DIVERSALIS__COMPILER__NAME "netbeans"
	#define DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE

///////////////////////////
// eclipse-cdt c++ indexer

#elif defined DIVERSALIS__COMPILER__ECLIPSE
	#define DIVERSALIS__COMPILER
	#define DIVERSALIS__COMPILER__NAME "eclipse"
	#define DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE

//////////////////////////////////////////////////////////////////////////////////////////
// gnu compiler, and other compilers that disguise as gnu gcc (llvm clang, intel icc ...)

#elif defined __GNUC__
	#define DIVERSALIS__COMPILER
	#define DIVERSALIS__COMPILER__GNU
	#if defined __clang__
		#define DIVERSALIS__COMPILER__CLANG
		#if 0 ///\todo  use gnu version for now
			#define DIVERSALIS__COMPILER__VERSION__MAJOR __clang__major__
			#define DIVERSALIS__COMPILER__VERSION__MINOR __clang__minor__
			#define DIVERSALIS__COMPILER__VERSION__PATCH __clang__patchlevel__
		#endif
		#define DIVERSALIS__COMPILER__NAME "clang"
	#elif defined __INTEL_COMPILER
		#define DIVERSALIS__COMPILER__INTEL
		#define DIVERSALIS__COMPILER__NAME "icc-gcc"
	#else
		#define DIVERSALIS__COMPILER__NAME "gcc"
	#endif
	#if (!defined __GNUG__ || !defined __cplusplus) && !defined DIVERSALIS__COMPILER__RESOURCE
		#if defined __GNUG__ || defined __cplusplus
			#error "weird settings... we should have both __GNUG__ and __cplusplus"
		#else
			#error "please invoke gcc with the language option set to c++ (or invoke gcc via the g++ driver)"
		#endif
	#endif
	#define DIVERSALIS__COMPILER__VERSION__STRING DIVERSALIS__COMPILER__NAME "-" __VERSION__
	#define DIVERSALIS__COMPILER__VERSION  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
	#define DIVERSALIS__COMPILER__VERSION__MAJOR __GNUC__
	#define DIVERSALIS__COMPILER__VERSION__MINOR __GNUC_MINOR__
	#define DIVERSALIS__COMPILER__VERSION__PATCH __GNUC_PATCHLEVEL__
	#define DIVERSALIS__COMPILER__VERSION__ABI __GXX_ABI_VERSION

	#define DIVERSALIS__COMPILER__FEATURE__ASSEMBLER
	#define DIVERSALIS__COMPILER__ASSEMBLER__ATT ///\todo needs autodetection since gcc also supports intel asm

	#define DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS

	#ifdef __OPTIMIZE__ // when -O flag different than -O0
		#define DIVERSALIS__COMPILER__FEATURE__OPTIMIZE
	#endif
	
	#ifdef __GXX_RTTI
		#define DIVERSALIS__COMPILER__FEATURE__RTTI
	#endif
	
	#ifdef __EXCEPTIONS
		#define DIVERSALIS__COMPILER__FEATURE__EXCEPTION
	#endif

	// check if version supports pre-compilation. gcc < 3.4 does not support pre-compilation.
	#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
		#define DIVERSALIS__COMPILER__FEATURE__PRE_COMPILATION
	#endif
	
//////////////////////
// borland's compiler

#elif defined __BORLAND__
	#define DIVERSALIS__COMPILER
	#define DIVERSALIS__COMPILER__BORLAND
	#define DIVERSALIS__COMPILER__NAME "borland"
	#define DIVERSALIS__COMPILER__FEATURE__PRE_COMPILATION
	#define DIVERSALIS__COMPILER__FEATURE__AUTO_LINK

////////////////////////
// microsoft's compiler

#elif defined _MSC_VER
	// see http://msdn.microsoft.com/en-us/library/b0084kay.aspx
	
	#define DIVERSALIS__COMPILER
	#define DIVERSALIS__COMPILER__MICROSOFT
	#if defined __INTEL_COMPILER
		#define DIVERSALIS__COMPILER__INTEL
		#define DIVERSALIS__COMPILER__NAME "icc-msvc"
	#else
		#define DIVERSALIS__COMPILER__NAME "msvc"
	#endif
	
	#define DIVERSALIS__COMPILER__VERSION _MSC_VER // first 2 components, e.g. 15.00.20706.01 -> 1500
	#define DIVERSALIS__COMPILER__VERSION__MAJOR (_MSC_VER / 100)
	#define DIVERSALIS__COMPILER__VERSION__MINOR ((_MSC_VER - _MSC_VER / 100 * 100) / 10)
	#define DIVERSALIS__COMPILER__VERSION__PATCH (_MSC_VER - _MSC_VER / 10 * 10)
	#if defined _MSC_FULL_VER
		#define DIVERSALIS__COMPILER__VERSION__FULL _MSC_FULL_VER // first 3 components, e.g. 15.00.20706.01 -> 150020706
	#else
		#define DIVERSALIS__COMPILER__VERSION__FULL DIVERSALIS__COMPILER__VERSION
	#endif
	#if defined _MSC_BUILD
		#define DIVERSALIS__COMPILER__VERSION__BUILD _MSC_BUILD // last, 4th, component, e.g. 15.00.20706.01 -> 1
	#else
		#define DIVERSALIS__COMPILER__VERSION__BUILD 0
	#endif
	#if defined _WIN64
		#define DIVERSALIS__COMPILER__VERSION__STRING \
			DIVERSALIS__COMPILER__NAME \
			"-" DIVERSALIS__STRINGIZE(DIVERSALIS__COMPILER__VERSION__FULL) \
			"." DIVERSALIS__STRINGIZE(DIVERSALIS__COMPILER__VERSION__BUILD) \
			"-64-bit"
	#else
		#define DIVERSALIS__COMPILER__VERSION__STRING \
			DIVERSALIS__COMPILER__NAME \
			"-" DIVERSALIS__STRINGIZE(DIVERSALIS__COMPILER__VERSION__FULL) \
			"." DIVERSALIS__STRINGIZE(DIVERSALIS__COMPILER__VERSION__BUILD) \
			"-32-bit"
	#endif

	#pragma conform(forScope, on) // ISO conformance of the scope of variables declared inside the parenthesis of a loop instruction.
	#define DIVERSALIS__COMPILER__FEATURE__PRE_COMPILATION
// 	#define DIVERSALIS__COMPILER__FEATURE__AUTO_LINK // rePlayer
	#ifndef _WIN64
		#define DIVERSALIS__COMPILER__FEATURE__ASSEMBLER
		#define DIVERSALIS__COMPILER__ASSEMBLER__INTEL
	#endif
	#define DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
	
	#if defined _CPPRTTI // defined for code compiled with -GR (Enable Run-Time Type Information).
		#define DIVERSALIS__COMPILER__FEATURE__RTTI
	#else
		///\todo _CPPRTTI is not defined in msvc8!?
		//#error "please enable rtti"
	#endif

	#if defined _CPPUNWIND // defined for code compiled with -GX (Enable Exception Handling).
		#define DIVERSALIS__COMPILER__FEATURE__EXCEPTION
	#else
		///\todo _CPPUNWIND is not defined in msvc8!?
		//#error "please enable exception handling"
	#endif
#endif

//////////////////////////////////////////////////////////////////////
// resource compilers (only relevant on microsoft's operating system)

// RC_INVOKED is defined by resource compilers (only relevant on microsoft's operating system).
#if defined RC_INVOKED
	#define DIVERSALIS__COMPILER__RESOURCE
	#define DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
	#if !defined DIVERSALIS__COMPILER
		// Microsoft uses a separate (bogus) preprocessor for ressource compilation,
		// called "rc", which as nothing to do with msvc's "cl".
		#define DIVERSALIS__COMPILER
		#define DIVERSALIS__COMPILER__NAME "rc"
		#define DIVERSALIS__COMPILER__VERSION__STRING DIVERSALIS__COMPILER__NAME
	#endif
#endif

/////////
// C++11

// Detecting which version of the standard is supported is standardised itself.
// So, there is no need to define our own macro, we can just use "#if __cplusplus >= 201103L" directly.
// There you can find a table that lists C++11 features and their support in popular compilers:
// http://wiki.apache.org/stdcxx/C++0xCompilerSupport
// A standard-conformant compiler shall only define __cplusplus >= 201103L once it implements *all* of the standard.
// Such violation happened e.g. with gcc 4.7 in which the option -std=c++11 or -std=gnu++11 defines __cplusplus to 201103L,
// but 'thread_local' is not a keyword (partial support is obtained with the old '__thread' nonstandard keyword).
// If it happens that this rule is more often blatantly violated, we might then have to define our own trustable macro here,
// or fined-grained macros about specific C++11 features.

///////////////////
// openmp standard

// defined when compiling with:
// -fopenmp on gcc or alike,
// -openmp on msvc.
// returns an integer representing the date of the OpenMP specification implemented.
#if defined _OPENMP
	#define DIVERSALIS__COMPILER__FEATURE__OPEN_MP _OPENMP
#endif

/**********************************************************************************/
// consistency check

#if !defined DIVERSALIS__COMPILER
	#error "unknown compiler"
#endif
	
#if !defined __cplusplus && !defined DIVERSALIS__COMPILER__RESOURCE
	#error "Not a c++ compiler. For the gnu compiler, please invoke gcc with the -x language option set to c++ (or invoke gcc via the g++ driver)."
#endif
