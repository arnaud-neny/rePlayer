// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface universalis::os::dyn_link::resolver

#pragma once
#include <universalis/detail/project.hpp>
#include <boost/filesystem/path.hpp>
#include <list>
#include <string>

#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
	// no include needed
#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
	#include "include_windows_without_crap.hpp"
#else
	namespace Glib { class Module; }
#endif

namespace universalis { namespace os {
/// handling of dynamic shared libraries.
namespace dyn_link {

/// a list of paths
typedef std::list<boost::filesystem::path> path_list_type;

/// gets the library path list from the environment variable
UNIVERSALIS__DECL
path_list_type lib_path();

/// sets the library path environment variable to the specified path list
UNIVERSALIS__DECL
void lib_path(path_list_type const & new_path);

/// resolves symbol names in a dynamic shared library.
class resolver {
	public:
		/// creates a new resolver for symbols in the library of given name.
		///\param path the path to the the library file to load.
		/// The library will be loaded/mapped in the process's memory,
		/// and its reference discarded when this object is deleted, or when you call close(),
		/// which means the library will be unloaded/unmapped if there is no other
		/// resolver object (or other mean) holding the same library.
		/// You must make sure you do not need anymore to use any data or code
		/// contained in this library before closing (that is, unloading) it.
		///\throws exception if there is an error trying to open the library file.
		///\post opened()
		UNIVERSALIS__DECL
		resolver(boost::filesystem::path const & path, unsigned int const & version);

		/// wether the underlying library is opened and loaded/mapped.
		///\returns true if the underlying library is opened and loaded/mapped
		UNIVERSALIS__DECL
		bool opened() const throw();

		/// the full path to the file of the underlying library.
		///\pre opened()
		UNIVERSALIS__DECL
		boost::filesystem::path path() const throw();

		/// resolves a symbol name in the library to an address in memory.
		///\name the symbol name in the library to resolve to an address in memory.
		///\returns the address in memory of the code or data represented by the given symbol name.
		///\throws exception if there is an error trying to resolve the symbol.
		///\pre opened()
		template<typename X>
		X const resolve_symbol(std::string const & name) const {
			return reinterpret_cast<X>(resolve_symbol_untyped(name));
		}

		/// closes the underlying library.
		/// The library will be unloaded if there is no other
		/// resolver object (or other mean) holding the same library.
		/// You must make sure you do not need anymore to use any data or code
		/// contained in this library before closing (that is, unloading/unmapping) it.
		///\throws exception if there is an error trying to unload the library or close the library file.
		///\post !opened()
		UNIVERSALIS__DECL
		void close();

		/// will implicitely close() if opened().
		UNIVERSALIS__DECL
		virtual ~resolver() throw();

	private:
		typedef void (*function_pointer)();
		function_pointer resolve_symbol_untyped(std::string const & name) const;
		void static open_error(boost::filesystem::path const & path, std::string const & message);
		void close_error(std::string const & message) const;
		void resolve_symbol_error(std::string const & name, std::string const & message) const;
		/// os-specific
		boost::filesystem::path static decorated_filename(boost::filesystem::path const & path, unsigned int const & version) throw();
		/// abi/compiler-specific
		std::string static decorated_symbol(std::string const & name) throw();

		/// os-specific
		#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
			boost::filesystem::path const path_;
			void *
		#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
			::HINSTANCE
		#else
			Glib::Module *
		#endif
				underlying_;
};

}}}
