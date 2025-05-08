// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface universalis::cpu::exception

#pragma once
#include <universalis/os/exception.hpp>

namespace universalis { namespace cpu {

/// external cpu/os exception translated into a c++ one, with deferred querying of the human-readable message.
class UNIVERSALIS__DECL exception : public universalis::os::exception {
	public:
		#if defined NBEBUG
			inline
		#endif
			exception(code_type code, compiler::location const & location) throw()
				#if defined NDEBUG
					: universalis::os::exception(code, location) {}
				#else
					;
				#endif
};

namespace exceptions {
	/// This should be called for and from any new thread created to enable cpu/os to c++ exception translation for that thread.
	void UNIVERSALIS__DECL install_handler_in_thread();

	std::string UNIVERSALIS__DECL desc(int const &) throw();
}

}}
