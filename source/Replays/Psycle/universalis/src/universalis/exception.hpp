// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "compiler/exception.hpp"
#include "compiler/location.hpp"
#include <stdexcept>
#include <string>
#if !defined NDEBUG
	#include <typeinfo>
#endif

namespace universalis {

namespace exceptions {
	/// holds information about the location from where an exception was triggered.
	class UNIVERSALIS__DECL locatable {
		public:
			locatable(compiler::location const & location) throw() : location_(location) {}
			operator compiler::location const & () const throw() { return location_; }
			compiler::location const & location() const throw() { return *this    ; }
		private:
			compiler::location location_;
	};

	/// holds information about nested exceptions
	class UNIVERSALIS__DECL nested {
		public:
			nested(void const * cause = 0) throw() : cause_(cause) {}
			void const * cause() const throw() { return this->cause_; }
		private:
			void const * cause_;
	};
}

class UNIVERSALIS__DECL exception
:
	public std::exception,
	public exceptions::locatable,
	public exceptions::nested
{
	public:
		exception(compiler::location const & location, void const * cause = 0) throw()
			: exceptions::locatable(location), exceptions::nested(cause) {}
		virtual ~exception() throw() {}
};

namespace exceptions {
	class UNIVERSALIS__DECL runtime_error
	:
		public std::runtime_error,
		public locatable,
		public nested
	{
		public:
			runtime_error(std::string const & what, compiler::location const & location, void const * cause = 0) throw();
			virtual ~runtime_error() throw() {}
	};

	#if !defined NDEBUG
		class UNIVERSALIS__DECL bad_cast
		:
			public std::bad_cast,
			public locatable,
			public nested
		{
			public:
				bad_cast(std::type_info const & from, std::type_info const & to, compiler::location const & location, void const * cause = 0) throw()
					: locatable(location), nested(cause), from_(from), to_(to) {}
				virtual ~bad_cast() throw() {}

			public:  std::type_info const & from() const throw() { return this->from_; }
			private: std::type_info const & from_;

			public:  std::type_info const & to() const throw() { return this->to_  ; }
			private: std::type_info const & to_;
		};
	#endif

	template<typename E> std::string inline string                (             E const & e) { std::ostringstream s; s << e; return s.str(); }
	template<          > std::string inline string<std::exception>(std::exception const & e) { return e.what(); }
	template<          > std::string inline string<void const *  >(  void const * const &  ) { return compiler::exceptions::ellipsis_desc(); }

	///\internal
	namespace detail {
		template<typename E>
		void operator_(compiler::location const & location, E const * const e = 0) throw(runtime_error) {
			throw runtime_error(exceptions::string(*e), location, e);
		}
		class rethrow_functor {
			public:
				template<typename E>
				void operator_(compiler::location const & location, E const * const e = 0) const throw(runtime_error) {
					detail::operator_<E>(location, e );
				}
		};
		
	}

	#define UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW \
		UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__DETAIL(universalis::exceptions::detail::rethrow_functor(), UNIVERSALIS__COMPILER__LOCATION)

	#define UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__NO_CLASS \
		UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__DETAIL(universalis::exceptions::detail::rethrow_functor(), UNIVERSALIS__COMPILER__LOCATION__NO_CLASS)

	#define UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__WITH_FUNCTOR(rethrow_functor) \
		UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__DETAIL(rethrow_functor, UNIVERSALIS__COMPILER__LOCATION)

	#define UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__WITH_FUNCTOR__NO_CLASS(rethrow_functor) \
		UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__DETAIL(rethrow_functor, UNIVERSALIS__COMPILER__LOCATION__NO_CLASS)

	///\internal
	#define UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__DETAIL(rethrow_functor, location) \
		catch(          std::exception const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(                 wchar_t const e[]) { rethrow_functor.operator_       (location, &e); } \
		catch(                  char   const e[]) { rethrow_functor.operator_       (location, &e); } \
		catch(  signed          char   const e[]) { rethrow_functor.operator_       (location, &e); } \
		catch(unsigned          char   const e[]) { rethrow_functor.operator_       (location, &e); } \
		catch(                  char   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(  signed          char   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(unsigned          char   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(  signed     short int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(unsigned     short int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(  signed           int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(unsigned           int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(  signed      long int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(unsigned      long int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(  signed long long int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(unsigned long long int   const & e) { rethrow_functor.operator_       (location, &e); } \
		catch(            void const * const   e) { rethrow_functor.operator_       (location, &e); } \
		catch(               ...                ) { rethrow_functor.operator_<void*>(location    ); }
	}
}
