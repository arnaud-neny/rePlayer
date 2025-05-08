// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2000 Ross Bencina and Phil Burk
// copyright ????-???? Bjorn Roche, XO Audio, LLC
// copyright 2008-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>
//
// This source code is partially based on Apache Harmony.
// see apache harmony barrier http://svn.apache.org/viewvc/harmony/enhanced/drlvm/trunk/vm/port/include/port_barriers.h
//
// This source code is partially based on the pa_memorybarrier.h header of PortAudio (Portable Audio I/O Library).
// See portaudio memory barrier http://portaudio.com/trac/browser/portaudio/trunk/src/common/pa_memorybarrier.h
// For more information on PortAudio, see http://www.portaudio.com
// The PortAudio community makes the following non-binding requests:
// Any person wishing to distribute modifications to the Software
// is requested to send the modifications to the original developer
// so that they can be incorporated into the canonical version.
// It is also requested that these non-binding requests be included along with the license above.

///\file \brief memory barrier utilities
/// Some memory barrier primitives based on the system.
/// In addition to providing memory barriers, these functions should ensure that data cached in registers
/// is written out to cache where it can be snooped by other CPUs. (ie, the volatile keyword should not be required).
///
/// The primitives defined here are:
/// universalis::cpu::memory_barriers::full()
/// universalis::cpu::memory_barriers::read()
/// universalis::cpu::memory_barriers::write()

#pragma once
#include <universalis/detail/project.hpp>

#if defined DIVERSALIS__OS__APPLE
	#include <libkern/OSAtomic.h>
	// Mac OS X only provides full memory barriers, so the three types of barriers are the same,
	// However, these barriers are superior to compiler-based ones.
	namespace universalis { namespace cpu { namespace memory_barriers {
		void inline  full() { OSMemoryBarrier(); }
		void inline  read() { full(); }
		void inline write() { full(); }
	}}}
	#define universalis__cpu__memory_barriers__defined
#elif defined DIVERSALIS__COMPILER__GNU
	#if DIVERSALIS__COMPILER__VERSION >= 40100 // 4.1.0
		// GCC >= 4.1 has built-in intrinsics. We'll use those.
		namespace universalis { namespace cpu { namespace memory_barriers {
			void inline  full() { __sync_synchronize(); }
			void inline  read() { full(); }
			void inline write() { full(); }
		}}}
		#define universalis__cpu__memory_barriers__defined
	#else
		// As a fallback, GCC understands volatile asm and "memory"
		// to mean it should not reorder memory read/writes.
		#if defined DIVERSALIS__CPU__POWER_PC
			namespace universalis { namespace cpu { namespace memory_barriers {
				void inline  full() { asm volatile("sync" ::: "memory"); }
				void inline  read() { full(); }
				void inline write() { full(); }
			}}}
			#define universalis__cpu__memory_barriers__defined
		#elif defined DIVERSALIS__CPU__X86
			// [bohan] hardware fences are not always needed on x86 >= i686 memory model,
			//         which is a cache-coherent, write-through one, except for SSE instructions!
			namespace universalis { namespace cpu { namespace memory_barriers {
				void inline  full() {
					#if DIVERSALIS__CPU__X86__SSE >= 3
						asm volatile("mfence" ::: "memory");
					#else
						// The lock is what's needed, so the 'add' is setup, essentially, as a no-op.
						asm volatile ("lock; addl $0, 0(%%esp)" ::: "memory");
					#endif
				}
				void inline  read() { asm volatile("lfence" ::: "memory"); }
				void inline write() { asm volatile("sfence" ::: "memory"); }
			}}}
			#define universalis__cpu__memory_barriers__defined
		#elif defined DIVERSALIS__CPU__IA
			// asm volatile("mf" ::: "memory");
		#endif
	#endif
#elif defined DIVERSALIS__COMPILER__MICROSOFT
	#include <intrin.h>
	#include <emmintrin.h>
	#if !defined DIVERSALIS__COMPILER__INTEL
		#pragma intrinsic(_ReadWriteBarrier)
		#pragma intrinsic(_ReadBarrier)
		#pragma intrinsic(_WriteBarrier)
	#endif
	namespace universalis { namespace cpu { namespace memory_barriers {
		// [bohan] hardware fences are not always needed on x86 >= i686 memory model,
		//         which is a cache-coherent, write-through one, except for SSE instructions!
		//         So on x86 the _Read/WriteBarrier() and __memory_barrier() instrinsics might be for the compiler only,
		//         i.e. do the same as what the volatile keyword does, and don't emit any extra cpu instructions!
		//         But on other CPU targets, like PowerPC and Itanium, they ought to emit the needed extra CPU instructions.
		//         Hence, we also add _mm_*fence() to emit the needed CPU instructions.
		//         What has not been checked is whether we would end up with doubled hardware fences on non-x86 targets.
		void inline  full() {
			#if DIVERSALIS__CPU__X86__SSE >= 3 || DIVERSALIS__CPU__X86 >= 64
				_mm_mfence();
			#elif defined DIVERSALIS__CPU__X86
				// The lock is what's needed, so the 'add' is setup, essentially, as a no-op.
				__asm { lock add [esp], 0 }
			#else
				#error "sorry"
			#endif
			#if defined DIVERSALIS__COMPILER__INTEL
				__memory_barrier();
			#else
				_ReadWriteBarrier();
			#endif
		}
		void inline  read() {
			_mm_lfence();
			#if defined DIVERSALIS__COMPILER__INTEL
				__memory_barrier();
			#else
				_ReadBarrier();
			#endif
		}
		void inline write() {
			_mm_sfence();
			#if defined DIVERSALIS__COMPILER__INTEL
				__memory_barrier();
			#else
				_WriteBarrier();
			#endif
		}
	}}}
	#define universalis__cpu__memory_barriers__defined
#elif defined DIVERSALIS__COMPILER__BORLAND && 0 ///\todo needs hardware fence
	namespace universalis { namespace cpu { namespace memory_barriers {
		// [bohan] hardware fences are not always needed on x86 >= i686 memory model,
		//         which is a cache-coherent, write-through one, except for SSE instructions!
		//         I'm not sure whether these instructions are for the cpu only or if the compiler sees them,
		//         but we do need to tell the compiler not to reorder memory read/writes,
		//         (i.e. do the same as what the volatile keyword does).
		// The lock is what's needed, so the 'add' is setup, essentially, as a no-op.
		void inline  full() {
			#if defined DIVERSALIS__CPU__X86
				_asm { lock add [esp], 0 }
			#else
				#error "sorry"
			#endif
		}
		void inline  read() { full(); }
		void inline write() { full(); }
	}}}
	#define universalis__cpu__memory_barriers__defined
#endif

#if !defined universalis__cpu__memory_barriers__defined
	UNIVERSALIS__COMPILER__WARNING("Memory barriers are not defined for this system. Will use compare_and_swap to emulate.")
	#include "atomic_compare_and_swap.hpp"
	namespace universalis { namespace cpu { namespace memory_barriers {
		void inline  full() { static int dummy = 0; atomic_compare_and_swap(&dummy, 0, 0); }
		void inline  read() { full(); }
		void inline write() { write(); }
	}}}
#endif
#undef universalis__cpu__memory_barriers__defined
