// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>
#if __cplusplus >= 201103L
	#include <mutex>
	namespace universalis { namespace stdlib {
		using std::mutex;
		using std::recursive_mutex;
		using std::timed_mutex;
		using std::recursive_timed_mutex;
		using std::defer_lock;
		using std::try_to_lock;
		using std::adopt_lock;
		using std::unique_lock;
		using std::lock_guard;
		using std::once_flag;
		using std::call_once;
		#define UNIVERSALIS__STDLIB__ONCE_FLAG(flag) std::once_flag flag
	}}
#else
	#include "chrono.hpp"
	#include <boost/thread/mutex.hpp>
	#include <boost/thread/recursive_mutex.hpp>
	#include <boost/thread/once.hpp>
	#if BOOST_VERSION >= 105300
		#include <boost/thread/lock_guard.hpp>
	#endif
	namespace universalis { namespace stdlib {
		namespace detail {
			template<typename Timed_Mutex>
			class try_lock_for_until_adapter : public Timed_Mutex {
				public:
					template<typename Duration>
					bool try_lock_for(Duration const & d) {
						chrono::nanoseconds::rep const ns = chrono::nanoseconds(d).count();
						return this->timed_lock(
							boost::posix_time::
								#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
									nanoseconds(ns)
								#else
									microseconds(ns / 1000)
								#endif
						);
					}

					template<typename Clock, typename Duration>
					bool try_lock_until(chrono::time_point<Clock, Duration> const & t) { return try_lock_for(t - Clock::now()); }
			};
		}

		using boost::mutex;
		using boost::recursive_mutex;
		typedef detail::try_lock_for_until_adapter<boost::timed_mutex> timed_mutex;
		typedef detail::try_lock_for_until_adapter<boost::recursive_timed_mutex> recursive_timed_mutex;
		using boost::defer_lock;
		using boost::try_to_lock;
		using boost::adopt_lock;
		using boost::lock_error;
		template<typename Mutex> class unique_lock : public boost::unique_lock<Mutex> {
			typedef boost::unique_lock<Mutex> base_impl;
			public:
				unique_lock() {}
				explicit unique_lock(Mutex & m) : base_impl(m) {}
				unique_lock(Mutex & m, boost::adopt_lock_t) : base_impl(m, adopt_lock) {}
				unique_lock(Mutex & m, boost::defer_lock_t) : base_impl(m, defer_lock) {}
				unique_lock(Mutex & m, boost::try_to_lock_t) : base_impl(m, try_to_lock) {}
				
				template<typename Duration>
				bool try_lock_for(Duration const & d) {
					chrono::nanoseconds::rep const ns = chrono::nanoseconds(d).count();
					return this->timed_lock(
						boost::posix_time::
							#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
								nanoseconds(ns)
							#else
								microseconds(ns / 1000)
							#endif
					);
				}

				template<typename Clock, typename Duration>
				bool try_lock_until(chrono::time_point<Clock, Duration> const & t) { return try_lock_for(t - Clock::now()); }
		};
		using boost::lock_guard;
		using boost::once_flag;
		using boost::call_once;
		#define UNIVERSALIS__STDLIB__ONCE_FLAG(flag) boost::once_flag flag = BOOST_ONCE_INIT
	}}
#endif
