// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\implementation universalis::os::exception

#include <universalis/detail/project.private.hpp>
#include "exception.hpp"
#include <universalis/stdlib/exception.hpp>
#if defined DIVERSALIS__OS__MICROSOFT
	#include <sstream>
#endif

namespace universalis { namespace os {

char const * exception::what() const throw() {
	if(!what_) what_ = new std::string(exceptions::desc(code()));
	return what_->c_str();
}

#if defined DIVERSALIS__OS__MICROSOFT
	namespace exceptions {
		char const * iso_or_posix_std::what() const throw() {
			if(!what_) what_ = new std::string(stdlib::exceptions::desc(code()));
			return what_->c_str();
		}
	}
#endif

namespace exceptions { namespace detail {
	std::string desc(
		#if !defined DIVERSALIS__OS__MICROSOFT
			int const code
		#else
			::DWORD /* or ::HRESULT in some cases */ const code,
			bool from_processor
		#endif
	) {
		#if !defined DIVERSALIS__OS__MICROSOFT
			std::string nvr = stdlib::exceptions::desc(code);
			return nvr;
		#else
			std::ostringstream s; s
			<< (from_processor ? "cpu/os exception" : "microsoft api error") << ": "
			<< code << " 0x" << std::hex << code << ": ";
			::DWORD flags(
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS
			);
			char * error_message_pointer(0);
			::HMODULE module(0);
			if(from_processor) module = ::LoadLibraryExA("ntdll", 0, LOAD_LIBRARY_AS_DATAFILE);
			try {
				if(module) flags |= FORMAT_MESSAGE_FROM_HMODULE;
				if(::FormatMessageA( // ouch! plain old c api style, this is ugly...
						flags, module, code,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
						reinterpret_cast<char*>(&error_message_pointer), // we *must* hard-cast! this seems to be a hack to extend an originally badly designed api... there is no other way to do it
						0, 0
				)) s << error_message_pointer;
				else s << "unknown exception code: " << code << " 0x" << std::hex << code;
				if(error_message_pointer) ::LocalFree(error_message_pointer);
			} catch(...) {
				if(module) ::FreeLibrary(module);
				throw;
			}
			if(module) ::FreeLibrary(module);
			std::string nvr = s.str();
			return nvr;
		#endif
	}
}}

}}
