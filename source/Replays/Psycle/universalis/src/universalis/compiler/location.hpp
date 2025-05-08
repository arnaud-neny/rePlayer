// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.pastnotecut.org : johan boule <bohan@jabber.org>

#pragma once
#include "stringize.hpp"
#ifndef DIVERSALIS__COMPILER__GNU
	// Only gcc is able to include the name of the current class implicitly with __PRETTY_FUNCTION__.
	// We can use rtti support on other compilers.
	#include "typenameof.hpp"
#endif
#include <boost/current_function.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace universalis { namespace compiler {

/// location of current line of the source, within a class.
#define UNIVERSALIS__COMPILER__LOCATION \
	universalis::compiler::location( \
		UNIVERSALIS__COMPILER__LOCATION__DETAIL(UNIVERSALIS__COMPILER__LOCATION__DETAIL__FUNCTION) \
	)

/// location of current line of the source, outside of any class.
#define UNIVERSALIS__COMPILER__LOCATION__NO_CLASS \
	universalis::compiler::location( \
		UNIVERSALIS__COMPILER__LOCATION__DETAIL(UNIVERSALIS__COMPILER__LOCATION__DETAIL__FUNCTION__NO_CLASS) \
	)

/// location of the source.
class location {
	public:
		location(
			std::string const & module,
			std::string const & function,
			std::string const & file,
			unsigned int const line
		)
		:
			module_(module),
			function_(function),
			file_(file),
			line_(line)
		{}

	public:  std::string const & module() const throw() { return module_; }
	private: std::string const   module_;

	public:  std::string const & function() const throw() { return function_; }
	private: std::string const   function_;

	public:  std::string const & file() const throw() { return file_; }
	private: std::string const   file_;

	public:  unsigned int       line() const throw() { return line_; }
	private: unsigned int const line_;
};
}}



/*********************************************************************************************************/
// implementation details

#if \
	defined UNIVERSALIS__META__MODULE__NAME && \
	defined UNIVERSALIS__META__MODULE__VERSION
	///\internal
	#define UNIVERSALIS__COMPILER__LOCATION__DETAIL__MODULE \
		UNIVERSALIS__META__MODULE__NAME " " \
		UNIVERSALIS__COMPILER__STRINGIZE(UNIVERSALIS__META__MODULE__VERSION)
#elif defined UNIVERSALIS__META__MODULE__NAME
	///\internal
	#define UNIVERSALIS__COMPILER__LOCATION__DETAIL__MODULE UNIVERSALIS__META__MODULE__NAME
#else
	///\internal
	#define UNIVERSALIS__COMPILER__LOCATION__DETAIL__MODULE "(unknown module)"
#endif

///\internal
#define UNIVERSALIS__COMPILER__LOCATION__DETAIL(function) \
	universalis::compiler::location( \
		UNIVERSALIS__COMPILER__LOCATION__DETAIL__MODULE, \
		function, \
		__FILE__, \
		__LINE__ \
	)

#if defined DIVERSALIS__COMPILER__GNU
	///\internal
	/// Gcc is able to include the name of the current class implicitly, so we don't need to use rtti.
	/// The dummy "this" usage is just here to ensure the compiler will fail to compile
	/// if this macro was mistakenly used instead of the "NO_CLASS" variant.
	#define UNIVERSALIS__COMPILER__LOCATION__DETAIL__FUNCTION \
		(std::string(this ? "" : "") + BOOST_CURRENT_FUNCTION)
#else
	///\internal
	/// include the name of the current class explicitly using rtti on the "this" pointer
	#define UNIVERSALIS__COMPILER__LOCATION__DETAIL__FUNCTION \
		(universalis::compiler::typenameof(*this) + " :: " UNIVERSALIS__COMPILER__LOCATION__DETAIL__FUNCTION__NO_CLASS)
#endif

///\internal
#define UNIVERSALIS__COMPILER__LOCATION__DETAIL__FUNCTION__NO_CLASS  \
	BOOST_CURRENT_FUNCTION
