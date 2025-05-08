// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2009-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface psycle::host::cpu_time_clock

#pragma once
#include <psycle/host/detail/project.hpp>
#include <universalis/os/clocks.hpp>

namespace psycle { namespace host {

/// This clock is meant to have the following characteristics:
/// - virtual thread time: ideally, this counts the time spent by cpu(s) in the current thread, blocked time not counted,
/// - monotonic: does not jump to follow the official time,
/// - high resolution: can measure very short durations.
typedef universalis::os::clocks::hires_thread_or_fallback cpu_time_clock;

/// This clock is meant to have the following characteristics:
/// - real time: counts the real, physical time elapsed, independent of the official time,
/// - monotonic: does not jump to follow the official time,
/// - steady: does not accelerate/deccelerate to adjust to official time,
/// - high resolution: can measure very short durations.
typedef universalis::os::clocks::steady steady_clock;

}}
