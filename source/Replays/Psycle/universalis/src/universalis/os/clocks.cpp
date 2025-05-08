// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#include <universalis/detail/project.private.hpp>
#include "clocks.hpp"
#include "detail/clocks.hpp"
#include "exception.hpp"
#include <universalis/compiler/thread_local.hpp>
#ifdef DIVERSALIS__COMPILER__FEATURE__OPEN_MP
	#include <omp.h>
#endif
#ifdef DIVERSALIS__OS__POSIX
	#include <unistd.h> // for sysconf
	#include <sys/time.h>
#endif
#include <ctime>
#include <limits>
#include <iostream>
#include <sstream>
namespace universalis { namespace os { namespace clocks {

using namespace stdlib::chrono;

namespace detail {
	#if defined DIVERSALIS__OS__POSIX
		namespace posix {
			bool clock_gettime_supported, clock_getres_supported, monotonic_clock_supported, process_cpu_time_supported, thread_cpu_time_supported;
			::clockid_t monotonic_clock_id, process_cpu_time_clock_id, thread_cpu_time_clock_id;

			namespace {
				void error(int const code = errno) {
					std::cerr << "error: " << code << ": " << ::strerror(code) << std::endl;
				}

				bool supported(int const option) {
					long int result(::sysconf(option));
					if(result < -1) error();
					return result > 0;
				}
			}

			void config() {
				bool static once = false;
				if(once) return;

				monotonic_clock_id = process_cpu_time_clock_id = thread_cpu_time_clock_id = CLOCK_REALTIME;

				#if defined DIVERSALIS__COMPILER__DOXYGEN
					/// define this macro to diagnose potential issues
					#define UNIVERSALIS__OS__CLOCKS__DIAGNOSE
				#endif

				/// TIMERS
				#if !_POSIX_TIMERS
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("will use posix sysconf at runtime to determine whether this OS supports timers: !_POSIX_TIMERS")
					#endif
				#elif _POSIX_TIMERS == -1
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("this OS does not support posix timers: _POSIX_TIMERS == -1")
					#endif
					clock_gettime_supported = clock_getres_supported = false;
				#elif _POSIX_TIMERS > 0
					clock_gettime_supported = clock_getres_supported = true;
				#endif
				#if !defined _SC_TIMERS
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("cannot use posix sysconf at runtime to determine whether this OS supports timers: !defined _SC_TIMERS")
					#endif
					clock_gettime_supported = clock_getres_supported = false;
				#else
					clock_gettime_supported = clock_getres_supported = supported(_SC_TIMERS);
					// beware: cygwin has clock_gettime, but it doesn't have clock_getres.
					#if defined UNIVERSALIS__OS__CLOCKS__DIAGNOSE && defined DIVERSALIS__OS__CYGWIN
						UNIVERSALIS__COMPILER__WARNING("\
							Operating system is Cygwin. Cygwin has _POSIX_TIMERS > 0, \
							but only partially implements this posix option: it supports ::clock_gettime, but not ::clock_getres. \
							This might be the reason ::sysconf(_SC_TIMERS) returns 0, but this condradicts _POSIX_TIMERS > 0. \
							We assume that ::clock_gettime is supported.")
							if(!clock_gettime_supported) clock_gettime_supported = true;
					#endif
				#endif

				// MONOTONIC_CLOCK
				#if !_POSIX_MONOTONIC_CLOCK
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("will use posix sysconf at runtime to determine whether this OS supports monotonic clock: !_POSIX_MONOTONIC_CLOCK")
					#endif
				#elif _POSIX_MONOTONIC_CLOCK == -1
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("this OS does not support posix monotonic clock: _POSIX_MONOTONIC_CLOCK == -1")
					#endif
					monotonic_clock_supported = false;
				#elif _POSIX_MONOTONIC_CLOCK > 0
					monotonic_clock_supported = true;
				#endif
				#if !defined _SC_MONOTONIC_CLOCK
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("cannot use posix sysconf at runtime to determine whether this OS supports monotonic clock: !defined _SC_MONOTONIC_CLOCK")
					#endif
					monotonic_clock_supported = false;
				#else
					monotonic_clock_supported = supported(_SC_MONOTONIC_CLOCK);
					if(monotonic_clock_supported) monotonic_clock_id = CLOCK_MONOTONIC;
				#endif

				// PROCESS_CPUTIME
				#if !_POSIX_CPUTIME
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("will use posix sysconf at runtime to determine whether this OS supports process cpu time: !_POSIX_CPUTIME")
					#endif
				#elif _POSIX_CPUTIME == -1
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("this OS does not support posix process cpu time: _POSIX_CPUTIME == -1")
					#endif
					process_cputime_supported = false;
				#elif _POSIX_CPUTIME > 0
					process_cpu_time_supported = true;
				#endif
				#if !defined _SC_CPUTIME
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("cannot use posix sysconf at runtime to determine whether this OS supports process cpu time: !defined _SC_CPUTIME")
					#endif
					process_cpu_time_supported = false;
				#else
					process_cpu_time_supported = supported(_SC_CPUTIME);
					if(process_cpu_time_supported) process_cpu_time_clock_id = CLOCK_PROCESS_CPUTIME_ID;
				#endif

				// THREAD_CPUTIME
				#if !_POSIX_THREAD_CPUTIME
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("will use posix sysconf at runtime to determine whether this OS supports thread cpu time: !_POSIX_THREAD_CPUTIME")
					#endif
				#elif _POSIX_THREAD_CPUTIME == -1
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("this OS does not support posix thread cpu time: _POSIX_THREAD_CPUTIME == -1")
					#endif
					thread_cpu_time_supported = false;
				#elif _POSIX_THREAD_CPUTIME > 0
					thread_cpu_time_supported = true;
				#endif
				#if !defined _SC_THREAD_CPUTIME
					#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
						UNIVERSALIS__COMPILER__WARNING("cannot use posix sysconf at runtime to determine whether this OS supports thread cpu time: !defined _SC_THREAD_CPUTIME")
					#endif
					thread_cpu_time_supported = false;
				#else
					thread_cpu_time_supported = supported(_SC_THREAD_CPUTIME);
					if(thread_cpu_time_supported) thread_cpu_time_clock_id = CLOCK_THREAD_CPUTIME_ID;
				#endif

				// SMP
				#if _POSIX_CPUTIME > 0 || _SC_CPUTIME
					if(process_cpu_time_supported || thread_cpu_time_supported) {
						::clockid_t clock_id;
						if(clock_getcpuclockid(0, &clock_id) == ENOENT) {
							#ifdef UNIVERSALIS__OS__CLOCKS__DIAGNOSE
								UNIVERSALIS__COMPILER__WARNING("this SMP system makes CLOCK_PROCESS_CPUTIME_ID and CLOCK_THREAD_CPUTIME_ID inconsistent")
							#endif
							process_cpu_time_supported = thread_cpu_time_supported = false;
						}
					}
				#endif

				once = true;
			}
		}
	#endif // defined DIVERSALIS__OS__POSIX

	/******************************************************************************************/

	/// iso std time.
	/// returns the time since the Epoch (00:00:00 UTC, January 1, 1970), measured in seconds.
	/// test result: clock: std::time, min: 1s, avg: 1s, max: 1s
	iso_std_time::time_point iso_std_time::now() {
		const std::time_t t = std::time(0);
		if(t < 0) throw exceptions::iso_or_posix_std(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
		return time_point(seconds(t));
	}

	/// iso std clock.
	/// returns an approximation of processor time used by the program
	/// test result on colinux AMD64: clock: std::clock, min: 0.01s, avg: 0.01511s, max: 0.02s
	iso_std_clock::time_point iso_std_clock::now() {
		const std::clock_t t = std::clock();
		if(t < 0) throw exceptions::iso_or_posix_std(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
		return time_point(nanoseconds(t * nanoseconds::period::den / CLOCKS_PER_SEC));
	}

	/******************************************************************************************/

	#ifdef DIVERSALIS__COMPILER__FEATURE__OPEN_MP
		omp::time_point omp::now() {
			return time_point(nanoseconds(static_cast<nanoseconds::rep>(omp_get_wtime() * nanoseconds::period::den)));
		}
	#endif
	
	/******************************************************************************************/

	#if defined DIVERSALIS__OS__POSIX
		namespace posix {
			namespace {
				nanoseconds get(::clockid_t clock) {
					#if defined _POSIX_TIMERS > 0 || defined _SC_TIMERS
						::timespec t;
						if(::clock_gettime(clock, &t)) throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
						nanoseconds ns(nanoseconds(t.tv_nsec) + seconds(t.tv_sec));
						return ns;
					#else
						return iso_std_clock().time_since_epoch(); // note: this is the process cpu time!
					#endif
				}
			}

			/// posix CLOCK_REALTIME.
			/// System-wide realtime clock.
			/// test result on colinux AMD64: clock: CLOCK_REALTIME, min: 1.3e-05s, avg: 1.6006e-05s, max: 0.001359s
			real_time::time_point real_time::now() { return time_point(get(CLOCK_REALTIME)); }

			/// posix CLOCK_MONOTONIC.
			/// Clock that cannot be set and represents monotonic time since some unspecified starting point.
			/// test result on colinux AMD64: clock: CLOCK_MONOTONIC, min: 1.3e-05s, avg: 1.606e-05s, max: 0.001745s
			monotonic::time_point monotonic::now() {
				return time_point(get(
					#if _POSIX_MONOTONIC_CLOCK > 0 || defined _SC_MONOTONIC_CLOCK
						CLOCK_MONOTONIC
					#else
						CLOCK_REALTIME
					#endif
				));
			}

			/// posix CLOCK_PROCESS_CPUTIME_ID.
			/// High-resolution per-process timer from the CPU.
			/// Realized on many platforms using timers from the CPUs (TSC on i386, AR.ITC on Itanium).
			/// test result on colinux AMD64: clock: CLOCK_PROCESS_CPUTIME_ID, min: 0.01s, avg: 0.01s, max: 0.01s
			process_cpu_time::time_point process_cpu_time::now() {
				return time_point(get(
					#if _POSIX_CPUTIME > 0 || defined _SC_CPUTIME
						CLOCK_PROCESS_CPUTIME_ID
					#else
						CLOCK_REALTIME
					#endif
				));
			}

			/// posix CLOCK_THREAD_CPUTIME_ID.
			/// Thread-specific CPU-time clock.
			/// Realized on many platforms using timers from the CPUs (TSC on i386, AR.ITC on Itanium).
			/// test result on colinux AMD64: clock: CLOCK_THREAD_CPUTIME_ID, min: 0.01s, avg: 0.01s, max: 0.01s
			thread_cpu_time::time_point thread_cpu_time::now() {
				return time_point(get(
					#if _POSIX_THREAD_CPUTIME > 0 || defined _SC_THREAD_CPUTIME
						CLOCK_THREAD_CPUTIME_ID
					#else
						CLOCK_REALTIME
					#endif
				));
			}

			/// posix gettimeofday.
			/// test result on colinux AMD64: clock: gettimeofday, min: 1.3e-05s, avg: 1.5878e-05s, max: 0.001719s
			time_of_day::time_point time_of_day::now() {
				::timeval t;
				if(::gettimeofday(&t, 0)) // second argument passed is 0 for no timezone
					throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
				nanoseconds ns(microseconds(t.tv_usec) + seconds(t.tv_sec));
				return time_point(ns);
			}
		}
	#elif defined DIVERSALIS__OS__MICROSOFT
		namespace microsoft {
			/// The implementation of QueryPerformanceCounter reads, if available, the timer/tick count register of some *unspecified* (!!!) CPU,
			/// i.e. this is realised using the TSC on i386, AR.ITC on Itanium ...
			///
			/// On most CPU architectures, the register is updated at a rate based on the frequency of the cycles,
			/// but often the count value and the tick events are unrelated,
			/// i.e. the value might not be incremented one by one.
			/// So the period corresponding to 1 count unit may be even smaller than the period of a CPU cycle,
			/// but should probably stay in the same order of magnitude.
			/// If the counter is increased by 4,000,000,000 over a second, and is 64-bit long,
			/// it is possible to count an uptime period in the order of a century without wrapping.
			///
			/// The implementation for x86, doesn't work well at all on some of the CPUs whose frequency varies over time.
			/// This will eventually be fixed http://www.x86-secret.com/?option=newsd&nid=845 .
			///
			/// The implementation on MS-Windows is even unpsecified on SMP systems! Thank you again Microsoft.
			///
			/// http://en.wikipedia.org/wiki/Time_Stamp_Counter
			/// http://msdn.microsoft.com/en-us/library/ee417693.aspx
			/// http://stackoverflow.com/questions/644510/cpu-clock-frequency-and-thus-queryperformancecounter-wrong
			///
			/// test result on AMD64: clock resolution: QueryPerformancefrequency: 3579545Hz (3.6MHz)
			/// test result on AMD64: clock: QueryPerformanceCounter, min: 3.073e-006s, avg: 3.524e-006s, max: 0.000375746s
			performance_counter::time_point performance_counter::now() {
				// These are all thread-local variables becauses of (potential?) problems with SMP systems.
				// This becomes useful when we ask the OS to bind each thread to a specific CPU (using SetThreadAffinityMask).
				static thread_local nanoseconds::rep last_counter_time = 0;
				static thread_local nanoseconds::rep last_frequency_time = 0;
				static thread_local ::LONGLONG last_frequency = 0;
				static thread_local ::LONGLONG last_counter = 0;

				// We call QueryPerformanceFrequency the first time and then every time at least a second has elapsed.
				// This is done because Microsoft refuses to specify whether QueryPerformanceFrequency may change or not.
				if(!last_frequency_time || nanoseconds(last_counter_time - last_frequency_time) > seconds(1)) {
					last_frequency_time = last_counter_time;
					::LARGE_INTEGER frequency;
					if(!::QueryPerformanceFrequency(&frequency)) throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
					last_frequency = frequency.QuadPart;
				}
				
				::LARGE_INTEGER counter;
				if(!::QueryPerformanceCounter(&counter)) throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
				
				const nanoseconds::rep counter_diff = counter.QuadPart - last_counter;
				// Note that the counter may go backward for two reasons:
				// 1) After resuming from hibernation:
				//    The computer just booted and hence its counters were reset to zero on boot,
				//    and the operating system restored all data from a persistent medium, including our last_counter value,
				//    which is now greater than the current counter!
				// 2) The operating system, for some reason, moved our thread from one CPU to another,
				//    and the new CPU has its counter behind the one of the previous CPU.
				if(counter_diff > 0) {
					// Note that we may overflow when doing "counter_diff * nanoseconds::period::den", in various cases:
					// 1) The first call to this function, last_counter still has its initial zero value,
					//    and the difference between the current counter and last_counter will be the whole duration of the computer uptime,
					//    *** credits go to JosepMa for spotting this problem ***
					// 2) The counter is increasing fast enough and this function has not been called frequently enough.
					//
					// Examples of overflow cases:
					// - Example 1:
					//   On a virtualbox QueryPerformanceFrequency indicates 2.5GHz,
					//   which means after 1 second, the counter has increased by 2.5e9, and after 10 seconds, has increased by 2.5e10.
					//   If we multiply 2.5e10 with 1e9 (nanoseconds::period::den), we get 2.5e19, which is 2.7 times 2^63.
					// - Example 2:
					//   On a modern motherboard, QueryPerformanceFrequency indicates 3MHz,
					//   which means after 1 second, the counter has increased by 3e6, and after 1 hour, has increased by 1.8e10.
					//   If we multiply 1.8e10 with 1e9 (nanoseconds::period::den), we get 1.8e19, which is 1.95 times 2^63.
					//
					// Note also that the behavior of *signed* integer overflow is unspecified in the C programming language.
					// So, we can't rely on "counter_diff * nanoseconds::period::den" being < 0 due to overflow.
					if(counter_diff < std::numeric_limits<nanoseconds::rep>::max() / nanoseconds::period::den) {
						last_counter_time += counter_diff * nanoseconds::period::den / last_frequency;
					} else {
						last_counter_time += static_cast<nanoseconds::rep>(double(counter_diff) * nanoseconds::period::den / last_frequency);
					}
				}
				last_counter = counter.QuadPart;
				return time_point(nanoseconds(last_counter_time));
			}

			/// ::timeGetTime() is equivalent to ::GetTickCount() but Microsoft very loosely tries to express the idea that
			/// it might be more precise, especially if calling ::timeBeginPeriod and ::timeEndPeriod.
			/// Possibly realised using the PIT/PIC PC hardware.
			/// test result: clock: mmsystem timeGetTime, min: 0.001s, avg: 0.0010413s, max: 0.084s.
			mme_system_time::time_point mme_system_time::now() {
				class set_timer_resolution {
					private: ::UINT milliseconds;
					public:
						set_timer_resolution() {
							// tries to get the best possible resolution, starting with 1ms (which is the minimum supported anyway)
							milliseconds = 1;
							retry:
							if(::timeBeginPeriod(milliseconds) == TIMERR_NOCANDO /* looks like microsoft invented LOLCode! */) {
								if(++milliseconds < 50) goto retry;
								else milliseconds = 0; // give up :-(
							}
						}
						~set_timer_resolution() throw() {
							if(!milliseconds) return; // was not set
							if(::timeEndPeriod(milliseconds) == TIMERR_NOCANDO /* looks like microsoft invented LOLCode! */)
								return; // cannot throw in a destructor
						}
				}; //static once; disabled because this affects to whole OS scheduling frequency, which may have bad side effects!
				return time_point(nanoseconds(::timeGetTime() * nanoseconds::period::den / milliseconds::period::den));
			}

			/// ::GetTickCount() returns the uptime (i.e., time elapsed since computer was booted), in milliseconds.
			/// Microsoft doesn't even specifies wether it's monotonic and as steady as possible, but we can probably assume so.
			/// This function returns a value which is read from a context value which is updated only on context switches,
			/// and hence is very inaccurate: it can lag behind the real clock value as much as 15ms, and sometimes more.
			/// Possibly realised using the PIT/PIC PC hardware.
			/// test result: clock: GetTickCount, min: 0.015s, avg: 0.015719s, max: 0.063s.
			tick_count::time_point tick_count::now() {
				return time_point(nanoseconds(::GetTickCount() * nanoseconds::period::den / milliseconds::period::den));
			}

			/// test result: clock: GetSystemTimeAsFileTime, min: 0.015625s, avg: 0.0161875s, max: 0.09375s.
			system_time_as_file_time_since_epoch::time_point system_time_as_file_time_since_epoch::now() {
				union winapi_is_badly_designed {
					::FILETIME file_time;
					::LARGE_INTEGER large_integer;
				} u;
				::GetSystemTimeAsFileTime(&u.file_time);
				u.large_integer.QuadPart -= 116444736000000000LL; // microsoft disregards the unix/posix epoch time, so we need to apply an offset
				return time_point(nanoseconds(u.large_integer.QuadPart * 100));
			}

			/// virtual clock. kernel time not included.
			/// test result: clock: GetProcessTimes, min: 0.015625s, avg: 0.015625s, max: 0.015625s.
			process_time::time_point process_time::now() {
				union winapi_is_badly_designed {
					::FILETIME file_time;
					::LARGE_INTEGER large_integer;
				} creation, exit, kernel, user;
				::GetProcessTimes(::GetCurrentProcess(), &creation.file_time, &exit.file_time, &kernel.file_time, &user.file_time);
				return time_point(nanoseconds(user.large_integer.QuadPart * 100));
			}

			/// virtual clock. kernel time not included.
			/// The implementation of mswindows' ::GetThreadTimes() is completly broken: http://blog.kalmbachnet.de/?postid=28
			/// test result: clock: GetThreadTimes, min: 0.015625s, avg: 0.015625s, max: 0.015625s.
			thread_time::time_point thread_time::now() {
				union winapi_is_badly_designed {
					::FILETIME file_time;
					::LARGE_INTEGER large_integer;
				} creation, exit, kernel, user;
				::GetThreadTimes(::GetCurrentThread(), &creation.file_time, &exit.file_time, &kernel.file_time, &user.file_time);
				return time_point(nanoseconds(user.large_integer.QuadPart * 100));
			}
		}
	#endif // defined DIVERSALIS__OS__MICROSOFT
} // namespace detail

/******************************************************************************************/

utc_since_epoch::time_point utc_since_epoch::now() {
	///\todo UTC timezone, no daylight time saving
	#if defined DIVERSALIS__OS__POSIX
		return time_point(detail::posix::real_time::now().time_since_epoch());
	#elif defined DIVERSALIS__OS__MICROSOFT
		return time_point(detail::microsoft::system_time_as_file_time_since_epoch::now().time_since_epoch());
	#else
		return time_point(detail::iso_std_time::now().time_since_epoch());
	#endif
}

#if __cplusplus < 201103L
	steady::time_point steady::now() {
		#if defined DIVERSALIS__OS__POSIX
			detail::posix::config();
			if(detail::posix::monotonic_clock_supported) return time_point(detail::posix::monotonic::now().time_since_epoch());
			else return time_point(detail::posix::real_time::now().time_since_epoch());
		#elif defined DIVERSALIS__OS__MICROSOFT
			return time_point(detail::microsoft::performance_counter::now().time_since_epoch());
		#else
			return time_point(detail::iso_std_time::now().time_since_epoch());
		#endif
	}
#endif

process::time_point process::now() {
	#if defined DIVERSALIS__OS__POSIX
		detail::posix::config();
		if(detail::posix::process_cpu_time_supported) return time_point(detail::posix::process_cpu_time::now().time_since_epoch());
		else return time_point(steady::now().time_since_epoch());
	#elif defined DIVERSALIS__OS__MICROSOFT
		return time_point(detail::microsoft::process_time::now().time_since_epoch());
	#else
		return time_point(detail::iso_std_clock::now().time_since_epoch());
	#endif
}

thread::time_point thread::now() {
	#if defined DIVERSALIS__OS__POSIX
		detail::posix::config();
		if(detail::posix::thread_cpu_time_supported) return time_point(detail::posix::thread_cpu_time::now().time_since_epoch());
		else return time_point(process::now().time_since_epoch());
	#elif defined DIVERSALIS__OS__MICROSOFT
		return time_point(detail::microsoft::thread_time::now().time_since_epoch());
	#else
		return time_point(detail::iso_std_clock::now().time_since_epoch());
	#endif
}

hires_thread_or_fallback::time_point hires_thread_or_fallback::now() {
	#if defined DIVERSALIS__OS__POSIX
		detail::posix::config();
		if(detail::posix::thread_cpu_time_supported) return time_point(detail::posix::thread_cpu_time::now().time_since_epoch());
		else return time_point(process::now().time_since_epoch());
	#elif defined DIVERSALIS__OS__MICROSOFT
		// The implementation of mswindows' ::GetThreadTimes() is completly broken: http://blog.kalmbachnet.de/?postid=28
		// It's also a very low resolution: min: 0.015625s, avg: 0.015625s, max: 0.015625s.
		// So we use the performance counter instead.
		return time_point(detail::microsoft::performance_counter::now().time_since_epoch());
	#else
		return time_point(detail::iso_std_clock::now().time_since_epoch());
	#endif
}

}}}
