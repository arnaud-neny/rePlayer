// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "attribute.hpp"

// see http://www.programmersheaven.com/2/Calling-conventions (or http://www.angelcode.com/dev/callconv/callconv.html)

// calling convention | modifier keyword | parameters stack push            | parameters stack pop | extern "C" symbol name mangling                   | extern "C++" symbol name mangling
// register           | fastcall         | 3 registers then pushed on stack | callee               | '@' and arguments' byte size in decimal prepended | no standard
// pascal             | pascal           | left to right                    | callee               | uppercase                                         | no standard
// standard call      | stdcall          | right to left                    | callee               | preserved                                         | no standard
// c declaration      | cdecl            | right to left, variable count    | caller               | '_' prepended                                     | no standard

// note: the register convention is different from one compiler to another (for example left to right for borland, right to left for microsoft).
// for borland's compatibility with microsoft's register calling convention: #define fastcall __msfastcall.
// note: on the gnu compiler, one can use the #define __USER_LABEL_PREFIX__ to know what character is prepended to extern "C" symbols.
// note: on microsoft's compiler, with cdecl, there's is no special decoration for extern "C" declarations, i.e., no '_' prepended.
// member functions defaults to thiscall
// non-member functions defaults to cdecl

#if defined DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__C
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__PASCAL
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__STANDARD
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__FAST
	// for pseudo compilers, we just define all this to nothing to avoid errors in case the compiler would not know about them and it's found in external headers.
	#define __cdecl
	#define __pascal
	#define __stdcall
	#define __fastcall
	#define __msfastcall
	#define __thiscall
#elif defined DIVERSALIS__COMPILER__GNU
	// cdecl is only meaningful on the 32-bit x86 targets (excludes x86_64)
	#if defined DIVERSALIS__CPU__X86 && DIVERSALIS__CPU__SIZEOF_POINTER < 8
		#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__C UNIVERSALIS__COMPILER__ATTRIBUTE(__cdecl__)
	#else
		#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__C
	#endif
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__PASCAL   UNIVERSALIS__COMPILER__ATTRIBUTE(__pascal__)
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__STANDARD UNIVERSALIS__COMPILER__ATTRIBUTE(__stdcall__)
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__FAST     UNIVERSALIS__COMPILER__ATTRIBUTE(__fastcall__)
#elif defined DIVERSALIS__COMPILER__BORLAND
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__C        __cdecl
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__PASCAL   __pascal
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__STANDARD __stdcall
	/// borland's compatibility with microsoft's register calling convention
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__FAST     __msfastcall
#elif defined DIVERSALIS__COMPILER__MICROSOFT
	/// \todo [bohan] don't know why windef.h defines cdecl to nothing... because of this, functions with cdecl in their signature aren't considered to be of the same type as ones with __cdecl in their signature.
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__C        __cdecl
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__PASCAL   __pascal
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__STANDARD __stdcall
	#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__FAST     __fastcall
#else
	#error "Unsupported compiler ; please add support for function calling conventions for your compiler in the file where this error is triggered."
	/*
		#elif defined DIVERSALIS__COMPILER__<your_compiler_name>
			#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__C ...
			#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__PASCAL ...
			#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__STANDARD ...
			#define UNIVERSALIS__COMPILER__CALLING_CONVENTION__FAST ...
		#endif
	*/
#endif
