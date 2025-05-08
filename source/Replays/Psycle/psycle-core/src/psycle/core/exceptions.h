// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2003-2009 members of the psycle project http://psycle.pastnotecut.org : johan boule <bohan@jabber.org>

#ifndef PSYCLE__CORE__EXCEPTIONS__INCLUDED
#define PSYCLE__CORE__EXCEPTIONS__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>
#include <universalis/exception.hpp>
#include <universalis/compiler/location.hpp>
#include <stdexcept>

namespace psycle { namespace core {

/// base class for exceptions thrown from plugins
class exception : public std::runtime_error {
	public:
		exception(std::string const & what) : std::runtime_error(what) {}
};

/// classes derived from exception
namespace exceptions {

	/// base class for exceptions caused by errors on library operation
	class library_error : public exception {
		public:
			library_error(std::string const & what) : exception(what) {}
	};

	/// classes derived from library
	namespace library_errors {

		/// exception caused by library loading failure
		class loading_error : public library_error {
			public:
				loading_error(std::string const & what) : library_error(what) {}
		};

		/// exception caused by symbol resolving failure in a library
		class symbol_resolving_error : public library_error {
			public:
				symbol_resolving_error(std::string const & what) : library_error(what) {}
		};
	}

	/// base class for exceptions caused by an error in a library function
	class function_error : public exception {
		public:
			function_error(std::string const & what, std::exception const * const exception = 0) : core::exception(what), exception_(exception) {}
		public:
			std::exception const inline * const exception() const throw() { return exception_; }
		private:
			std::exception const * const        exception_;
	};

	///\relates function_error
	namespace function_errors {

		/// exception caused by a bad returned value from a library function.
		class bad_returned_value : public function_error {
			public:
				bad_returned_value(std::string const & what) : function_error(what) {}
		};

		///\internal
		namespace detail {
		
			template<typename T>
			struct identity { typedef T type; };
			
			/// Crashable type concept requirement: it must have a member function void crashed(std::exception const &) throw();
			template<typename Crashable>
			class rethrow_functor {
				private:
					template<typename E> void rethrow(universalis::compiler::location const & location, E const * const e, std::exception const * const standard) const throw(function_error) {
						std::ostringstream s;
						s
							<< "An exception occured in"
							<< " module: " << location.module()
							<< ", function: " << location.function()
							<< ", file: " << location.file() << ':' << location.line()
							<< '\n';
						if(e) {
							s
								<< "exception type: " << universalis::compiler::typenameof(*e) << '\n'
								<< universalis::exceptions::string(*e);
						} else {
							s << universalis::compiler::exceptions::ellipsis_desc();
						}
						function_error const f_error(s.str(), standard);
						crashable_.crashed(f_error);
						throw f_error;
					}
					template<typename E>
					void operator_(identity<E>, universalis::compiler::location const & location, E const * const e = 0) const throw(function_error){
						rethrow(location, e, 0);
					}
					void operator_(identity<std::exception>,universalis::compiler::location const & location, std::exception const * const e) const throw(function_error) {
						rethrow(location, e, e);
					}
					
					Crashable & crashable_;
				public:
					rethrow_functor(Crashable & crashable) : crashable_(crashable) {}

					template<typename E>
					void operator_(universalis::compiler::location const & location, E const * const e = 0) const throw(function_error) {
						operator_(identity<E>(),location,e);
					}

					
					void operator_(universalis::compiler::location const & location, std::exception const * const e) const throw(function_error) {
						operator_(identity<std::exception>(),location, e);
					}
			};

			template<typename Crashable>
			rethrow_functor<Crashable> make_rethrow_functor(Crashable & crashable) {
				return rethrow_functor<Crashable>(crashable);
			}
		}

		/// This macro is to be used in place of a catch statement
		/// to catch an exception of any type thrown by a machine.
		/// It performs the following operations:
		/// - It catches everything.
		/// - It converts the exception to a std::exception (if needed).
		/// - It marks the machine as crashed by calling the machine's member function void crashed(std::exception const &) throw();
		/// - It throws the converted exception.
		/// The usage is:
		/// - for the proxy between the host and a machine:
		///     try { some_machine.do_something(); } PSYCLE__HOST__CATCH_ALL(some_machine)
		/// - for the host:
		///     try { machine_proxy.do_something(); } catch(std::exception) { /* don't rethrow the exception */ }
		///
		/// Note that the crashable argument can be of any type as long as it has a member function void crashed(std::exception const &) throw();
		#define PSYCLE__HOST__CATCH_ALL(crashable) \
			UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__WITH_FUNCTOR( \
				psycle::core::exceptions::function_errors::detail::make_rethrow_functor(crashable) \
			)

		///\see PSYCLE__HOST__CATCH_ALL
		#define PSYCLE__HOST__CATCH_ALL__NO_CLASS(crashable) \
			UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__WITH_FUNCTOR__NO_CLASS( \
				psycle::core::exceptions::function_errors::detail::make_rethrow_functor(crashable) \
			)
	}
}
}}
#endif