// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>
#if __cplusplus >= 201103L
	#include <condition_variable>
	namespace universalis { namespace stdlib {
		using std::condition_variable;
		using std::condition_variable_any;
		using std::cv_status;
	}}
#else
	#include "mutex.hpp"
	#include "chrono.hpp"
	#include <boost/thread/condition_variable.hpp>
	namespace universalis { namespace stdlib {
		namespace cv_status { enum type { no_timeout, timeout }; }

		class condition_variable : public boost::condition_variable {
			public:
				template<typename Duration>
				cv_status::type wait_for(unique_lock<mutex> & lock, Duration const & d) {
					chrono::nanoseconds::rep const ns = chrono::nanoseconds(d).count();
					return timed_wait(lock,
						boost::posix_time::
							#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
								nanoseconds(ns)
							#else
								microseconds(ns / 1000)
							#endif
					) ? cv_status::no_timeout : cv_status::timeout;
				}

				template<typename Duration, typename Predicate>
				cv_status::type wait_for(unique_lock<mutex> & lock, Duration const & d, Predicate p) {
					chrono::nanoseconds::rep const ns = chrono::nanoseconds(d).count();
					return timed_wait(lock,
						boost::posix_time::
							#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
								nanoseconds(ns)
							#else
								microseconds(ns / 1000)
							#endif
						, p
					) ? cv_status::no_timeout : cv_status::timeout;
				}

				template<typename Clock, typename Duration>
				cv_status::type wait_until(unique_lock<mutex> & lock, chrono::time_point<Clock, Duration> const & t) {
					return wait_for(lock, t - Clock::now());
				}

				template<typename Clock, typename Duration, typename Predicate>
				cv_status::type wait_until(unique_lock<mutex> & lock, chrono::time_point<Clock, Duration> const & t, Predicate p) {
					return wait_for(lock, t - Clock::now(), p);
				}
		};

		class condition_variable_any : public::boost::condition_variable_any {
			public:
				template<typename Lock, typename Duration>
				bool wait_for(Lock & lock, Duration const & d) {
					chrono::nanoseconds::rep const ns = chrono::nanoseconds(d).count();
					return timed_wait(lock,
						boost::posix_time::
							#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
								nanoseconds(ns)
							#else
								microseconds(ns / 1000)
							#endif
					);
				}

				template<typename Lock, typename Duration, typename Predicate>
				bool wait_for(Lock & lock, Duration const & d, Predicate p) {
					chrono::nanoseconds::rep const ns = chrono::nanoseconds(d).count();
					return timed_wait(lock,
						boost::posix_time::
							#ifdef BOOST_DATE_TIME_HAS_NANOSECONDS
								nanoseconds(ns)
							#else
								microseconds(ns / 1000)
							#endif
						, p
					);
				}

				template<typename Lock, typename Clock, typename Duration>
				bool wait_until(Lock & lock, chrono::time_point<Clock, Duration> const & t) {
					return wait_for(lock, t - Clock::now());
				}

				template<typename Lock, typename Clock, typename Duration, typename Predicate>
				bool wait_until(Lock & lock, chrono::time_point<Clock, Duration> const & t, Predicate p) {
					return wait_for(lock, t - Clock::now(), p);
				}
		};
	}}
#endif

/******************************************************************************************/
#ifdef BOOST_AUTO_TEST_CASE
	#include "chrono.hpp"
	#include <universalis/os/clocks.hpp>
	#include <boost/bind.hpp>
	#include <vector>
	#include <sstream>
	namespace universalis { namespace stdlib { namespace test {
		class condition_variable_test_class {
			private:
				void thread_function() {
					{ lock_guard_type l(m);
						i = 1;
					}
					c.notify_one();
					{ lock_guard_type l(m);
						while(i != 2) c.wait(l); 
					}
					#if 0 // timeout tests disabled for now. need to rethink the logic of these. we don't use that feature anyway.
					{ // timeout in the future
						lock_guard_type l(m);
						using namespace chrono;
						system_clock::time_point const timeout = system_clock::now() + seconds(1);
						while(c.wait_until(l, timeout) == cv_status::no_timeout) {
							system_clock::time_point const now = system_clock::now();
							BOOST_CHECK(now < timeout + milliseconds(500));
							if(now >= timeout) break;
						}
					}
					{ // timeout in the past
						lock_guard_type l(m);
						using namespace chrono;
						system_clock::time_point const timeout = system_clock::now() - hours(1);
						while(c.wait_until(l, timeout) == cv_status::no_timeout) {
							system_clock::time_point const now = system_clock::now();
							BOOST_CHECK(now < timeout + milliseconds(500));
							if(now >= timeout) break;
						}
					}
					#endif
				}

				mutex m;
				condition_variable c;
				typedef unique_lock<mutex> lock_guard_type;
				int i;

			public:
				void test() {
					i = 0;
					thread t(boost::bind(&condition_variable_test_class::thread_function, this));
					{ lock_guard_type l(m);
						while(i != 1) c.wait(l);
						i = 2;
					}
					c.notify_one();
					t.join();
				}

		};

		BOOST_AUTO_TEST_CASE(condition_variable_test) {
			condition_variable_test_class test;
			test.test();
		}

		class condition_variable_speed_test_class {
			private:
				typedef os::clocks::steady clock;

				class tls {
					public:
						tls(int i) : d(), count(), i(i) {}
						clock::duration d;
						unsigned int count;
						int i;
				};

				void thread_function(tls * const tls_pointer) {
					tls & tls(*tls_pointer);
					{ lock_guard_type l(m);
						--shared_start;
						while(shared_start) c.wait(l);
					}
					c.notify_all();
					while(tls.count < end) {
						clock::time_point const t0(clock::now());
						for(unsigned int i(0); i < inner_loop; ++i) {
							{ lock_guard_type l(m);
								if(shared == -1) return;
								shared = tls.i;
							}
							c.notify_all();
							{ lock_guard_type l(m);
								while(shared == tls.i) c.wait(l);
							}
						}
						clock::time_point const t1(clock::now());
						if(++tls.count > start) tls.d += t1 - t0;
					}
					{ lock_guard_type l(m);
						shared = -1;
					}
					c.notify_all();
				}

				mutex m;
				condition_variable c;
				typedef unique_lock<mutex> lock_guard_type;
				unsigned int inner_loop, start, end;
				int shared, shared_start;

			public:
				void test(unsigned int threads) {
					inner_loop = 250;
					start = 50;
					end = start + 50;
					shared_start = threads;
					shared = 0;

					std::vector<tls*> tls_(threads);
					for(unsigned int i(0); i < threads; ++i) tls_[i] = new tls(i);

					std::vector<thread*> threads_(threads);
					for(unsigned int i(0); i < threads; ++i)
						threads_[i] = new thread(boost::bind(&condition_variable_speed_test_class::thread_function, this, tls_[i]));

					for(unsigned int i(0); i < threads; ++i) threads_[i]->join();

					for(unsigned int i(0); i < threads; ++i) {
						delete threads_[i];
						{
							std::ostringstream s;
							s << threads << " threads: " << i << ": " << chrono::nanoseconds(tls_[i]->d).count() * 1e-9 / (tls_[i]->count - start) / inner_loop << 's';
							BOOST_MESSAGE(s.str());
						}
						delete tls_[i];
					}
				}
		};
		
		BOOST_AUTO_TEST_CASE(condition_variable_speed_test) {
			condition_variable_speed_test_class test;
			test.test(2);
			test.test(4);
			test.test(8);
		}
	}}}
#endif
