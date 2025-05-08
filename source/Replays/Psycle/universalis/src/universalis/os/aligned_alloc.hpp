// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface universalis::os::aligned_memory_alloc

#pragma once

#include "exception.hpp"
#include <diversalis/os.hpp>
#include <diversalis/compiler.hpp>
#include <universalis/compiler/location.hpp>
#include <limits>

#if defined DIVERSALIS__OS__POSIX
	#include <cstdlib> // for posix_memalign
#endif

namespace universalis { namespace os {

template<typename X>
void aligned_memory_alloc(std::size_t alignment, X *& x, std::size_t count) {
	std::size_t const size(count * sizeof(X));
	#if defined DIVERSALIS__OS__POSIX
			void * address;
			int const err(posix_memalign(&address, alignment, size));
			if(err) throw exception(err, UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
			x = static_cast<X*>(address);
			// note: free with std::free
	#elif 0///\todo defined DIVERSALIS__OS__MICROSOFT && defined DIVERSALIS__COMPILER__GNU
			x = static_cast<X*>(__mingw_aligned_malloc(size, alignment));
			// note: free with _mingw_aligned_free
	#elif defined DIVERSALIS__OS__MICROSOFT && defined DIVERSALIS__COMPILER__MICROSOFT
			x = static_cast<X*>(_aligned_malloc(size, alignment));
			// note: free with _aligned_free
	#else
		// could also try _mm_malloc (#include <xmmintr.h> or <emmintr.h>?)
		// memalign on Solaris but not BSD (#include both <cstdlib> and <cmalloc>)
		// note that memalign is declared obsolete and does not specify how to free the allocated memory.
		
		size; // unused
		x = new X[count];
		// note: free with delete[]
		#error "TODO alloc some extra mem and store orig pointer or alignment to the x[-1]"
	#endif
}

template<typename X>
void aligned_memory_dealloc(X *& address) {
	#if defined DIVERSALIS__OS__POSIX
		std::free(address);
	#elif 0///\todo: defined DIVERSALIS__OS__MICROSOFT && defined DIVERSALIS__COMPILER__GNU
		_aligned_free(address);
	#elif defined DIVERSALIS__OS__MICROSOFT && defined DIVERSALIS__COMPILER__MICROSOFT
		_aligned_free(address);
	#else
		delete[] address;
		#error "TODO alloc some extra mem and store orig pointer or alignment to the x[-1]"
	#endif
}

template<typename T, std::size_t Alignment>
class aligned_allocator {
	public:
		// type definitions
		typedef T value_type;
		typedef T* pointer;
		typedef const T* const_pointer;
		typedef T& reference;
		typedef const T& const_reference;
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;

		// rebind allocator to type U
		template<typename T2>
		class rebind {
			public: typedef aligned_allocator<T2, Alignment> other;
		};

		// return address of values
		pointer address(reference value) const { return &value; }
		const_pointer address(const_reference value) const { return &value; }

		// constructors and destructor. nothing to do because the allocator has no state
		aligned_allocator() throw() {}
		aligned_allocator(const aligned_allocator&) throw() {}
		template<typename T2> aligned_allocator(const aligned_allocator<T2, Alignment>&) throw() {}
		~aligned_allocator() throw() {}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() { return std::numeric_limits<std::size_t>::max() / sizeof(T); }

		// allocate but don't initialize num elements of type T
		pointer allocate(size_type num, const void* = 0) {
			pointer p;
			aligned_memory_alloc(Alignment, p, num);
			return p;
		}

		// initialize elements of allocated storage p with value value
		void construct(pointer p, const T& value) {
			// initialize memory with placement new
			new(p) T(value);
		}

		// destroy elements of initialized storage p
		void destroy(pointer p) {
			// destroy objects by calling their destructor
			p->~T();
		}

		// deallocate storage p of deleted elements
		void deallocate(pointer p, size_type /*num*/) {
			aligned_memory_dealloc(p);
		}
};

}}

// return that all specializations of this allocator with the same alignment are interchangeable

template<typename T1, std::size_t Alignment1, typename T2, std::size_t Alignment2>
bool operator==(const universalis::os::aligned_allocator<T1, Alignment1>&, const universalis::os::aligned_allocator<T2, Alignment2>&) throw() {
	return Alignment1 == Alignment2;
}

template<typename T1, std::size_t Alignment1, typename T2, std::size_t Alignment2>
bool operator!=(const universalis::os::aligned_allocator<T1, Alignment1>&, const universalis::os::aligned_allocator<T2, Alignment2>&) throw() {
	return Alignment1 != Alignment2;
}

/*****************************************************************************/
#if defined BOOST_AUTO_TEST_CASE
	#include <vector>
	namespace universalis { namespace os {
		BOOST_AUTO_TEST_CASE(aligned_allocator_test) {
			std::size_t const alignment = 16;
			std::vector<float, aligned_allocator<float, alignment> > v;
			for(std::size_t s = 0; s < 100; ++s) {
				v.push_back(0);
				union {
					float * p;
					unsigned long long int i;
				} u;
				u.p = &v[0];
				BOOST_CHECK(!(u.i % alignment));
			}
		}
	}}
#endif
