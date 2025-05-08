// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/exception.hpp>
#include <string>
#include <cerrno>

namespace universalis { namespace stdlib {

class UNIVERSALIS__DECL exception : public universalis::exception {
	public:
		exception(compiler::location const & location) throw() : universalis::exception(location), code_(errno), what_() {}
		exception(int code, compiler::location const & location) throw() : universalis::exception(location), code_(code), what_() {}
		~exception() throw() { delete what_; }

	public:
		int code() const throw() { return code_; }
	private:
		int const code_;

	public:
		char const * what() const throw() /*override*/;
	protected:
		std::string const mutable * what_;
};

namespace exceptions {
	/// returns a string describing a standard error code
	UNIVERSALIS__DECL std::string desc(int code = errno) throw();
}

}}
