// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface universalis::os::exception

#pragma once
#include <universalis/exception.hpp>
#include <string>
#include <cerrno>
#if defined DIVERSALIS__OS__MICROSOFT
	#include "include_windows_without_crap.hpp"
#endif

namespace universalis { namespace os {

/// generic exception thrown by functions of the namespace universalis::os.
class UNIVERSALIS__DECL exception : public universalis::exception {
	public:
		typedef
			#if !defined DIVERSALIS__OS__MICROSOFT
				int
			#else
				DWORD
			#endif
			code_type;

		exception(compiler::location const & location) throw()
			: universalis::exception(location), code_(
					#if !defined DIVERSALIS__OS__MICROSOFT
						errno
					#else
						::GetLastError()
					#endif
				), what_() {}
		exception(code_type code, compiler::location const & location) throw() : universalis::exception(location), code_(code), what_() {}
		~exception() throw() { delete what_; }

	public:
		code_type code() const throw() { return code_; }
	private:
		code_type const code_;

	public:
		char const * what() const throw() /*override*/;
	protected:
		std::string const mutable * what_;
};

namespace exceptions {

	using universalis::exceptions::runtime_error;

	#if defined DIVERSALIS__OS__MICROSOFT
		/// exceptions for which code() is a standard iso or posix errno one and not a winapi GetLastError() one.
		/// The what() function then uses the iso standard lib strerror() function instead of windows ntdll FormatMessage().
		class UNIVERSALIS__DECL iso_or_posix_std : public exception {
			public:
				iso_or_posix_std(compiler::location const & location) throw() : exception(errno, location) {}
				iso_or_posix_std(int code, compiler::location const & location) throw() : exception(code, location) {}
				char const * what() const throw() /*override*/;
		};
	#else
		// posix systems conform to the iso standard, so there is no nothing special to do.
		typedef exception iso_or_posix_std;
	#endif

	class UNIVERSALIS__DECL operation_not_permitted : public iso_or_posix_std {
		public:
			operation_not_permitted(compiler::location const & location) throw() : iso_or_posix_std(EPERM, location) {}
	};

	///\internal
	namespace detail {
		std::string UNIVERSALIS__DECL desc(
			#if !defined DIVERSALIS__OS__MICROSOFT
				int const code = errno
			#else
				::DWORD /* or ::HRESULT in some cases */ const /*= ::GetLastError()*/,
				bool from_processor = false
			#endif
		);
	}

	std::string inline desc(
		#if !defined DIVERSALIS__OS__MICROSOFT
			int const code = errno
		#else
			::DWORD /* or ::HRESULT in some cases */ const code = ::GetLastError()
		#endif
	) { std::string nvr = detail::desc(code); return nvr; }

}

}}
