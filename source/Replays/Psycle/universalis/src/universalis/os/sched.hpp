// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2008-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface universalis::os::sched

#pragma once
#include "exception.hpp"
#if defined DIVERSALIS__OS__POSIX
	#include <pthread.h>
#elif defined DIVERSALIS__OS__MICROSOFT
	#include "include_windows_without_crap.hpp"
#else
	#error "unsupported operating system"
#endif
#if defined BOOST_AUTO_TEST_CASE
	#include <sstream>
#endif

namespace universalis { namespace os { namespace sched {

/// cpu affinity mask
class UNIVERSALIS__DECL affinity_mask {
	public:
		/// initialises a zeroed-out mask
		affinity_mask();

		/// returns the number active cpus in the mask
		///\seealso std::thread::hardware_concurrency (which is supposed to be process-independent)
		unsigned int active_count() const;

		/// returns the max cpu index
		unsigned int size() const;

		/// returns whether cpu at index is active; the index range is [0, size()[.
		bool operator()(unsigned int cpu_index) const;
		/// sets whether cpu at index is active; the index range is [0, size()[.
		void operator()(unsigned int cpu_index, bool);

	private:
		typedef
			#if defined DIVERSALIS__OS__POSIX
				cpu_set_t
			#elif defined DIVERSALIS__OS__MICROSOFT
				DWORD_PTR
			#else
				#error "unsupported operating system"
			#endif
			native_mask_type;
		native_mask_type native_mask_;
		friend class thread;
		friend class process;
};

/// process
class UNIVERSALIS__DECL process {
	public:
		/// native handle type
		typedef
			#if defined DIVERSALIS__OS__POSIX
				pid_t
			#elif defined DIVERSALIS__OS__MICROSOFT
				HANDLE
			#else
				#error "unsupported operating system"
			#endif
			native_handle_type;
	private:
		native_handle_type native_handle_;
		
	public:
		///\name ctors
		///\{
			/// initialises an instance that represents the current process
			process();
			/// initialises an instance that represents the given process
			process(native_handle_type native_handle) : native_handle_(native_handle) {}
		///\}

		///\name cpu affinity
		///\{
			/// affinity mask type
			typedef class affinity_mask affinity_mask_type;
			/// gets the affinity mask against the set of cpu available to the process.
			affinity_mask_type affinity_mask() const;
			/// sets the affinity mask against the set of cpu available to the process.
				#if defined DIVERSALIS__OS__MICROSOFT
					// Special case for Windows: we use an inline implementation in the header.
					// Why? Fear of doing something horribly wrong with the blackbox that Windows is.
					// The doc says:
					// "Do not call SetProcessAffinityMask in a DLL that may be called by processes other than your own."
					// Of course it might just be their way to say a DLL must be a good citizen and not do weird things that clients don't expect,
					// but, due to lack of specification from microsoft, we are doomed to do stupid and probably useless things like this.
					// Thank you Microsoft, once again.
					inline
				#endif
			void affinity_mask(affinity_mask_type const &);
		///\}

		///\name scheduling policy and priority
		///\{
			/// boosts the policy and priority so the thread becomes (something close to) "realtime".
			/// http://gitorious.org/sched_deadline/pages/Home
			void become_realtime(/* realtime constraints: deadline, period, bugdet, runtime ... */);
		///\}
};

/// thread
class UNIVERSALIS__DECL thread {
	public:
		/// native handle type
		typedef
			#if defined DIVERSALIS__OS__POSIX
				::pthread_t
			#elif defined DIVERSALIS__OS__MICROSOFT
				HANDLE
			#else
				#error "unsupported operating system"
			#endif
			native_handle_type;
	private:
		native_handle_type native_handle_;

	public:
		///\name ctors
		///\{
			/// initialises an instance that represents the current thread
			thread();
			/// initialises an instance that represents the given thread
			thread(native_handle_type native_handle) : native_handle_(native_handle) {}
		///\}

		///\name cpu affinity
		///\{
			/// affinity mask type
			typedef class affinity_mask affinity_mask_type;
			/// gets the affinity mask against the set of cpu available to the process.
			class affinity_mask affinity_mask() const;
			/// sets the affinity mask against the set of cpu available to the process.
			void affinity_mask(class affinity_mask const &);
		///\}

		///\name scheduling policy and priority
		///\{
			/// boosts the policy and priority so the thread becomes (something close to) "realtime".
			/// http://gitorious.org/sched_deadline/pages/Home
			void become_realtime(/* realtime constraints: deadline, period, bugdet, runtime ... */);
		///\}
};

/****************************************************/
// inline implementation

#if defined DIVERSALIS__OS__MICROSOFT
	// Special case for Windows: we use an inline implementation in the header.
	// Why? Fear of doing something horribly wrong with the blackbox that Windows is.
	// The doc says:
	// "Do not call SetProcessAffinityMask in a DLL that may be called by processes other than your own."
	// Of course it might just be their way to say a DLL must be a good citizen and not do weird things that clients don't expect,
	// but, due to lack of specification from microsoft, we are doomed to do stupid and probably useless things like this.
	// Thank you Microsoft, once again.
	void inline process::affinity_mask(class affinity_mask const & affinity_mask) {
		if(!SetProcessAffinityMask(native_handle_, affinity_mask.native_mask_))
			throw exception(UNIVERSALIS__COMPILER__LOCATION);
	}
#endif

/****************************************************/
// test cases

#if defined BOOST_AUTO_TEST_CASE
	BOOST_AUTO_TEST_CASE(affinity_test) {
		{
			process p;
			std::ostringstream s; s << "process affinity mask active count: " << p.affinity_mask().active_count();
			BOOST_MESSAGE(s.str());
		}
		{
			thread t;
			std::ostringstream s; s << "thread affinity mask active count: " << t.affinity_mask().active_count();
			BOOST_MESSAGE(s.str());
		}
	}

	#if 0 // need a way not to leave the thread with realtime scheduling, e.g. save and restore settings.
		// otherwise other test cases which use multiple threads will just starve and never complete because one thread eats everything.
	BOOST_AUTO_TEST_CASE(thread_realtime_test) {
		try {
			thread t;
			t.become_realtime();
		} catch(exceptions::operation_not_permitted e) {
			std::ostringstream s;
			s << "could not set thread scheduling policy and priority to realtime: " << e.what();
			BOOST_MESSAGE(s.str());
		}
	}
	#endif
#endif

}}}
