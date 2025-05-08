// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\implementation universalis::os::thread_name

#include <universalis/detail/project.private.hpp>
#include "thread_name.hpp"
#include "loggers.hpp"
#include <universalis/compiler/thread_local.hpp>
#include <cassert>

namespace universalis { namespace os {

///\todo use pthread_setname_np() on posix if available.

namespace {
	static thread_local std::string const * tls_thread_name_(0);

	void dump_current_thread_id(std::ostream & out) {
		out <<
			#if defined DIVERSALIS__OS__POSIX
				::pthread_self()
			#elif defined DIVERSALIS__OS__MICROSOFT
				::GetCurrentThreadId()
			#else
				#error "unsupported operating system"
			#endif
		;
	}
}

std::string thread_name::get() {
	std::string nvr;
	if(tls_thread_name_) nvr = *tls_thread_name_;
	else {
		std::ostringstream s;
		s << "thread-id-";
		dump_current_thread_id(s);
		nvr = s.str();
	}
	return nvr;
}

void thread_name::set(std::string const & name) {
	assert(!thread_name_.length() || &thread_name_ == tls_thread_name_);
	thread_name_ = name;
	set_tls();
}

void thread_name::set_tls() {
	if(os::loggers::trace()) {
		std::ostringstream s;
		s << "setting name for thread: id: ";
		dump_current_thread_id(s);
		s << ", name: " << thread_name_;
		os::loggers::trace()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
	}
	tls_thread_name_ = &thread_name_;
}

thread_name::~thread_name() {
	if(&thread_name_ == tls_thread_name_) tls_thread_name_ = 0;
}

}}
