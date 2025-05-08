// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2008-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\implementation universalis::os::sched

#include <universalis/detail/project.private.hpp>
#include "sched.hpp"

#if defined DIVERSALIS__OS__POSIX
	#include <sys/time.h>
	#include <sys/resource.h>
	#include <cerrno>
#endif

#include <cassert>

namespace universalis { namespace os { namespace sched {

#if defined DIVERSALIS__OS__POSIX
	namespace {
		// returns the min and max priorities for a policy
		void priority_min_max(int policy, int & min, int & max) {
			if((min = sched_get_priority_min(policy)) == -1)
				throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
			if((max = sched_get_priority_max(policy)) == -1)
				throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
		}
	}
#endif

/**********************************************************************/
// process

process::process()
: native_handle_(
	#if defined DIVERSALIS__OS__MICROSOFT
		GetCurrentProcess()
	#endif
)
{}


affinity_mask process::affinity_mask() const {
	#if defined DIVERSALIS__OS__POSIX
		///\todo also try using os.sysconf('SC_NPROCESSORS_ONLN') // SC_NPROCESSORS_CONF
		#if defined DIVERSALIS__OS__CYGWIN
			///\todo sysconf
			class affinity_mask result;
			result(0, true);
			return result;
		#else // actually, linux-specific
			class affinity_mask result;
			if(sched_getaffinity(native_handle_, sizeof result.native_mask_, &result.native_mask_) == -1)
				throw exception(UNIVERSALIS__COMPILER__LOCATION);
			return result;
		#endif
	#elif defined DIVERSALIS__OS__MICROSOFT
		class affinity_mask process, system;
		if(!GetProcessAffinityMask(native_handle_, &process.native_mask_, &system.native_mask_))
			throw exception(UNIVERSALIS__COMPILER__LOCATION);
		return process;
	#else
		#error "unsupported operating system"
	#endif
}

#if defined DIVERSALIS__OS__MICROSOFT
	// Special case for Windows: we use an inline implementation in the header.
	// Why? Fear of doing something horribly wrong with the blackbox that Windows is.
	// The doc says:
	// "Do not call SetProcessAffinityMask in a DLL that may be called by processes other than your own."
	// Of course it might just be their way to say a DLL must be a good citizen and not do weird things that clients don't expect,
	// but, due to lack of specification from microsoft, we are doomed to do stupid and probably useless things like this.
	// Thank you Microsoft, once again.
#else
	void process::affinity_mask(class affinity_mask const & affinity_mask) {
		#if defined DIVERSALIS__OS__POSIX
			#if defined DIVERSALIS__OS__CYGWIN
				///\todo sysconf
			#else // actually, linux-specific
				if(sched_setaffinity(native_handle_, sizeof affinity_mask.native_mask_, &affinity_mask.native_mask_) == -1)
					throw exception(UNIVERSALIS__COMPILER__LOCATION);
			#endif
		#elif defined DIVERSALIS__OS__MICROSOFT
			if(!SetProcessAffinityMask(native_handle_, affinity_mask.native_mask_))
				throw exception(UNIVERSALIS__COMPILER__LOCATION);
		#else
			#error "unsupported operating system"
		#endif
	}
#endif

void process::become_realtime(/* realtime constraints: deadline, period, bugdet, runtime ... */) {
	#if defined DIVERSALIS__OS__POSIX
		// Notes for Linux:
		// When a POSIX scheduler looks for a process to run, it first looks at processes with the highest priority,
		// and only then it uses the policy to break ties amongst the list of processes with the same priority.
		// The SCHED_FIFO policy is the most agressive.
		// That said, Linux actually splits the processes in two categories due to the following constraints it imposes:
		// All processes that are given a priority of 0 must use either SCHED_OTHER, SCHED_BATCH or SCHED_IDLE policy.
		// All processes that use the SCHED_OTHER, SCHED_BATCH or SCHED_IDLE policy must be assigned a priority of 0.
		// All processes that are given a priority > 0 must use either the SCHED_FIFO or SCHED_RR policy.
		// All processes that use the SCHED_FIFO or SCHED_RR policy must be assigned a priority > 0.
		// So, "non-realtime" processes are at priority 0, with a SCHED_OTHER, SCHED_BATCH or SCHED_IDLE policy,
		// and "realtime" processes are at priority > 0, with either a SCHED_FIFO or SCHED_RR policy.
		// The priority we set here is the static priority.
		// For the SCHED_OTHER policy, there is also a dynamic priority (nice value),
		// which is set to an absolute value with the setpriority() function or incremented/decremented with the nice() function.
		// The dynamic priority (nice value) is not used by the SCHED_FIFO and SCHED_RR policies.
	
		int fifo_min, fifo_max;
		priority_min_max(SCHED_FIFO, fifo_min, fifo_max);
		
		int other_min, other_max;
		priority_min_max(SCHED_OTHER, other_min, other_max);

		sched_param param = sched_param();
		
		// set priority just above other_max, contrained to the [fifo_min, fifo_max] range
		param.sched_priority = std::min(std::max(fifo_min, other_max + 1), fifo_max);
		
		if(-1 == sched_setscheduler(native_handle_, SCHED_FIFO, &param)) {
			int const error = errno;
			switch(error) {
				case EPERM: throw exceptions::operation_not_permitted(UNIVERSALIS__COMPILER__LOCATION);
				default: throw exception(error, UNIVERSALIS__COMPILER__LOCATION);
			}
		}
	#elif defined DIVERSALIS__OS__MICROSOFT
		if(!SetPriorityClass(native_handle_, REALTIME_PRIORITY_CLASS))
			throw exception(UNIVERSALIS__COMPILER__LOCATION);
	#else
		#error "unsupported operating system"
	#endif
}

/**********************************************************************/
// thread

thread::thread() : native_handle_(
	#if defined DIVERSALIS__OS__POSIX
		::pthread_self()
	#elif defined DIVERSALIS__OS__MICROSOFT
		::GetCurrentThread()
	#else
		#error "unsupported operating system"
	#endif
) {}

affinity_mask thread::affinity_mask() const {
	#if defined DIVERSALIS__OS__POSIX
		#if defined DIVERSALIS__OS__CYGWIN
			///\todo sysconf
			class affinity_mask result;
			result[0] = true;
			return result;
			#else // beware, in pthread_getaffinity_np, np stands for non-portable
			class affinity_mask result;
			if(int error = pthread_getaffinity_np(native_handle_, sizeof result.native_mask_, &result.native_mask_))
				throw exception(error, UNIVERSALIS__COMPILER__LOCATION);
			return result;
		#endif
	#elif defined DIVERSALIS__OS__MICROSOFT
		// *** Note ***
		// There is no GetThreadAffinityMask function in the winapi!
		// So we have no choice but to return the process' affinity mask.
		process p;
		class affinity_mask result(p.affinity_mask());
		return result;
	#else
		#error "unsupported operating system"
	#endif
}

void thread::affinity_mask(class affinity_mask const & affinity_mask) {
	#if defined DIVERSALIS__OS__POSIX
		#if defined DIVERSALIS__OS__CYGWIN
			///\todo sysconf
		#else // beware, in pthread_getaffinity_np, np stands for non-portable
			if(int error = pthread_setaffinity_np(native_handle_, sizeof affinity_mask.native_mask_, &affinity_mask.native_mask_))
				throw exception(error, UNIVERSALIS__COMPILER__LOCATION);
		#endif
	#elif defined DIVERSALIS__OS__MICROSOFT
		if(!SetThreadAffinityMask(native_handle_, affinity_mask.native_mask_))
			throw exception(UNIVERSALIS__COMPILER__LOCATION);
	#else
		#error "unsupported operating system"
	#endif
}

void thread::become_realtime(/* realtime constraints: deadline, period, bugdet, runtime ... */) {
	#if defined DIVERSALIS__OS__POSIX
		// For details, see process::become_realtime
	
		int fifo_min, fifo_max;
		priority_min_max(SCHED_FIFO, fifo_min, fifo_max);
		
		int other_min, other_max;
		priority_min_max(SCHED_OTHER, other_min, other_max);

		sched_param param = sched_param();
		
		// set priority just above other_max, contrained to the [fifo_min, fifo_max] range
		param.sched_priority = std::min(std::max(fifo_min, other_max + 1), fifo_max);
		
		if(int error = pthread_setschedparam(native_handle_, SCHED_FIFO, &param))
			switch(error) {
				case EPERM: throw exceptions::operation_not_permitted(UNIVERSALIS__COMPILER__LOCATION);
				default: throw exception(error, UNIVERSALIS__COMPILER__LOCATION);
			}
	#elif defined DIVERSALIS__OS__MICROSOFT
		// Note: If the thread (actually, the process) has the REALTIME_PRIORITY_CLASS base class,
		//       then this can also be -7, -6, -5, -4, -3, 3, 4, 5, or 6.
		if(!::SetThreadPriority(native_handle_, THREAD_PRIORITY_TIME_CRITICAL))
			throw exception(UNIVERSALIS__COMPILER__LOCATION);
	#endif
}

/**********************************************************************/
// affinity_mask

affinity_mask::affinity_mask()
	#if defined DIVERSALIS__OS__MICROSOFT
		: native_mask_()
	#endif
{
	#if defined DIVERSALIS__OS__POSIX // actually, linux-specific
		CPU_ZERO(&native_mask_);
	#endif
}

unsigned int affinity_mask::active_count() const {
	unsigned int result(0);
	for(unsigned int i = 0; i < size(); ++i) if((*this)(i)) ++result;
	return result;
}

unsigned int affinity_mask::size() const {
	return
		#if defined DIVERSALIS__OS__POSIX // actually, linux-specific
			CPU_SETSIZE;
		#elif defined DIVERSALIS__OS__MICROSOFT
			sizeof native_mask_ << 3;
		#else
			#error "unsupported operating system"
		#endif
}

bool affinity_mask::operator()(unsigned int cpu_index) const {
	assert(cpu_index < size());
	return
		#if defined DIVERSALIS__OS__POSIX // actually, linux-specific
			CPU_ISSET(cpu_index, &native_mask_);
		#elif defined DIVERSALIS__OS__MICROSOFT
			native_mask_ & native_mask_type(1) << cpu_index;
		#else
			#error "unsupported operating system"
		#endif
}

void affinity_mask::operator()(unsigned int cpu_index, bool active) {
	assert(cpu_index < size());
	#if defined DIVERSALIS__OS__POSIX // actually, linux-specific
		if(active)
			CPU_SET(cpu_index, &native_mask_);
		else
			CPU_CLR(cpu_index, &native_mask_);
	#elif defined DIVERSALIS__OS__MICROSOFT
		native_mask_ |= native_mask_type(1) << cpu_index;
	#else
		#error "unsupported operating system"
	#endif
	assert((*this)(cpu_index) == active);
}

}}}
