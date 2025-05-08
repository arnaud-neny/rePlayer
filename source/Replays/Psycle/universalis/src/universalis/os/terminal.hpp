// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface universalis::os::terminal

#pragma once
#include <universalis/detail/project.hpp>
#include "exception.hpp"
#include <universalis/stdlib/mutex.hpp>
#include <string>

namespace universalis { namespace os {

/// terminal.
class UNIVERSALIS__DECL terminal {
	public:
		terminal();
		virtual ~terminal() throw();
		void output(int const & logger_level, std::string const & string);
	private:
		typedef stdlib::unique_lock<stdlib::mutex> scoped_lock;
		stdlib::mutex mutable mutex_;
		#if defined DIVERSALIS__OS__MICROSOFT
			bool allocated_;
		#endif
};

}}
