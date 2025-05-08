// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface universalis::os::thread_name

#pragma once
#include <universalis/detail/project.hpp>
#include <string>

namespace universalis { namespace os {

/// thread name
class UNIVERSALIS__DECL thread_name {
	public:
		/// create an instance without setting any name yet
		thread_name() : thread_name_() {}

		/// sets a name for the current thread
		///\post the name is copied
		thread_name(std::string const & name) : thread_name_(name) { set_tls(); }

		/// sets a name for the current thread
		///\post the name is copied
		void set(std::string const &);

		/// gets the name of the current thread
		std::string static get();

		/// unsets the name of the thread that set it.
		/// There is no problem if the destructor is invoked from another thread
		/// than the one that set the name.
		~thread_name();
	private:
		std::string thread_name_;
		void set_tls();
};

}}
