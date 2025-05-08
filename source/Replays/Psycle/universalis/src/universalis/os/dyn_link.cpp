// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\implementation universalis::os::dyn_link::resolver

#include <universalis/detail/project.private.hpp>
#include "dyn_link.hpp"
#include "exception.hpp"
#include "loggers.hpp"
#include "fs.hpp"
#include <sstream>
#include <cassert>
#include <iostream>
#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
	#include <dlfcn.h> // dlopen, dlsym, dlclose, dlerror
	#include <csignal>
#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
	#include "include_windows_without_crap.hpp"
#else
	#include <glibmm/module.h> // Note: it seems glibmm is not honouring $ORIGIN on ELF.
#endif

namespace universalis { namespace os { namespace dyn_link {

namespace {
	char const lib_path_env_var_name[] = {
		#if defined DIVERSALIS__OS__MICROSOFT
			"PATH"
		#elif defined DIVERSALIS__OS__HPUX
			"SHLIB_PATH"
		#elif defined DIVERSALIS__OS__AIX
			"LIBPATH"
		#elif defined DIVERSALIS__OS__POSIX
			"LD_LIBRARY_PATH" // used at least on linux/solaris/macosx/cygwin
			// note: macosx also has: DYLD_LIBRARY_PATH and DYLD_FALLBACK_LIBRARY_PATH.
			// note: cygwin's dlopen uses LD_LIBRARY_PATH to locate the absolute dll path, and then calls LoadLibrary with it.
		#else
			#error "unknown dynamic linker"
		#endif
	};

	const path_list_type::value_type::value_type path_list_sep =
		#if defined DIVERSALIS__OS__POSIX
			':';
		#elif defined DIVERSALIS__OS__MICROSOFT
			';';
		#else
			#error "unknown path list separator"
		#endif
}

/****************************************************************/
// lib_path

path_list_type lib_path() {
	path_list_type nvr;

	#if !defined DIVERSALIS__OS__MICROSOFT
		char const * const env(std::getenv(lib_path_env_var_name));
	#else
		// On mswindows, the CRT maintains its own copy of the environment, separate from the Win32API copy.
		// CRT getenv() retrieves from this copy. CRT putenv() updates this copy,
		// and then calls SetEnvironmentVariableA() to update the Win32API copy.
		// Since we bypasses the CRT in the lib_path() setter by calling SetEnvironmentVariableA(),
		// we need to use GetEnvironmentVariableA() here instead of simply std::getenv().
		char * env;
		{
			::DWORD size = ::GetEnvironmentVariableA(lib_path_env_var_name, 0, 0);
			if(!size) {
				if(::GetLastError() == ERROR_ENVVAR_NOT_FOUND) env = 0;
				else throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
			} else {
				env = new char[size + 1];
				size = ::GetEnvironmentVariableA(lib_path_env_var_name, env, size + 1);
				if(!size) {
					delete [] env;
					throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
				}
			}
		}
	#endif

	if(env) {
		std::string const s(env);
		#if defined DIVERSALIS__OS__MICROSOFT
			delete [] env;
		#endif
		std::string::const_iterator b(s.begin()), e(s.end());
		for(std::string::const_iterator i(b); i != e; ++i) {
			if(*i == path_list_sep) {
				nvr.push_back(path_list_type::value_type(b, i));
				b = i + 1;
			}
		}
		if(b != e) nvr.push_back(path_list_type::value_type(b, e));
	}
	return nvr;
}

void lib_path(path_list_type const & new_path) {
	std::ostringstream s;
	for(path_list_type::const_iterator i(new_path.begin()), e(new_path.end()); i != e;) {
		#if BOOST_FILESYSTEM_VERSION >= 3
			s << i->string();
		#else
			s << i->directory_string();
		#endif
		if(++i != e) s << path_list_sep;
	}

	#if !defined DIVERSALIS__OS__MICROSOFT
		// setenv is better than putenv because putenv is not well-standardized
		// and has different memory ownership policies on different systems. -- Magnus
		if(::setenv(lib_path_env_var_name, s.str().c_str(), 1 /* overwrite */))
			throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
	#else
		// On mswindows, only putenv exists (with same memory ownership issue), and
		// the CRT maintains its own copy of the environment, separate from the Win32API copy.
		// CRT getenv() retrieves from this copy. CRT putenv() updates this copy,
		// and then calls SetEnvironmentVariableA() to update the Win32API copy.
		#if 0 // It's simpler to just use GetEnvironmentVariableA() in the lib_path() getter function.
			std::string static * old_env = 0;
			std::string & new_env = *new std::string(lib_path_env_var_name + '=' + s.str());
			putenv(new_env.c_str());
			delete old_env;
			old_env = &new_env;
		#else
			if(!::SetEnvironmentVariableA(lib_path_env_var_name, s.str().c_str()))
				throw exception(UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
		#endif
	#endif
}

/****************************************************************/
// resolver

void resolver::open_error(boost::filesystem::path const & path, std::string const & message) {
	throw exceptions::runtime_error("could not open library " + path.string() + ": " + message, UNIVERSALIS__COMPILER__LOCATION__NO_CLASS /* __NO_CLASS because it is a static member function */);
}

void resolver::close_error(std::string const & message) const {
	throw exceptions::runtime_error("could not close library " + path().string() + ": " + message, UNIVERSALIS__COMPILER__LOCATION);
}

void resolver::resolve_symbol_error(std::string const & name, std::string const & message) const {
	throw exceptions::runtime_error("could not resolve symbol " + name + " (" + decorated_symbol(name) + ") in library " + path().string() + ": " + message, UNIVERSALIS__COMPILER__LOCATION);
}

boost::filesystem::path resolver::decorated_filename(boost::filesystem::path const & path, unsigned int const & version) throw() {
	std::ostringstream version_string; version_string << version;
	return
		#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__BIN_FMT
			#if defined DIVERSALIS__OS__CYGWIN
				//"cyg" +
			#else
				//"lib" +
			#endif
			path.branch_path() / (
				#if BOOST_FILESYSTEM_VERSION >= 3
					path.string() +
				#else
					path.leaf() +
				#endif
				#if defined DIVERSALIS__OS__BIN_FMT__ELF
					".so" // "." + version_string.str() // libfoo.so.0
				#elif defined DIVERSALIS__OS__BIN_FMT__MAC_O
					".dylib" // libfoo.dylib or libfoo.bundle
				#elif defined DIVERSALIS__OS__BIN_FMT__PE
					".dll" // "-" + version_string.str() + ".dll" // libfoo-0.dll or cygfoo-0.dll
					// [bohan] we don't need to append the .dll suffix when the given name doesn't contain a dot,
					// [bohan] otherwise, we have to explicitly add the .dll suffix.
					// [bohan] note: for cygwin's dlopen, we always need to add the .dll suffix.
				#else
					#error "bogus preprocessor conditions"
				#endif
			);
		#else
			boost::filesystem::path(
				Glib::Module::build_path(
					path.branch_path().directory_string(),
					path.leaf()
						#if defined DIVERSALIS__OS__MICROSOFT
							//+ "-" + version_string.str()
						#endif
				)
			);
		#endif
}

resolver::resolver(boost::filesystem::path const & path, unsigned int const & version)
:
	#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
		path_(decorated_filename(path, version)),
	#endif
	underlying_(0)
{
	#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
		#if BOOST_FILESYSTEM_VERSION >= 3
			underlying_ = ::dlopen(path_.string().c_str(), RTLD_LAZY /*RTLD_NOW*/);
		#else
			underlying_ = ::dlopen(path_.file_string().c_str(), RTLD_LAZY /*RTLD_NOW*/);
		#endif
		if(!opened()) open_error(path_, std::string(::dlerror()));
	#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
		// we use \ here instead of / because ::LoadLibraryEx will not use the LOAD_WITH_ALTERED_SEARCH_PATH option if it does not see a \ character in the file path:
		boost::filesystem::path const final_path(decorated_filename(path, version));
		#if BOOST_FILESYSTEM_VERSION >= 3
			underlying_ = ::LoadLibraryExA(final_path.string().c_str(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
		#else
			underlying_ = ::LoadLibraryExA(final_path.file_string().c_str(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
		#endif
		if(!opened()) open_error(final_path, exceptions::desc());
	#else
		assert(Glib::Module::get_supported());
		boost::filesystem::path const final_path(decorated_filename(path, version));
		underlying_ = new Glib::Module(final_path.native_file_string(), Glib::MODULE_BIND_LAZY);
		if(!*underlying_) {
			delete underlying_;
			underlying_ = 0;
			open_error(final_path, Glib::Module::get_last_error());
		}
	#endif
}

bool resolver::opened() const throw() {
	return
		#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
			//
		#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
			//
		#else
			*
		#endif
		underlying_;
}

void resolver::close() {
	assert(opened());
	#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
		if(::dlclose(underlying_)) close_error(std::string(::dlerror()));
	#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
		if(!::FreeLibrary(underlying_)) close_error(exceptions::desc());
	#else
		delete underlying_;
	#endif
	underlying_ = 0;
}

resolver::~resolver() throw() {
	if(!opened()) return;
	try {
		close();
	} catch(std::exception const & e) {
		if(loggers::exception()) {
			std::ostringstream s;
			s << (e.what() ? e.what() : "no message");
			loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}
	}
}

resolver::function_pointer resolver::resolve_symbol_untyped(std::string const & name) const {
	assert(opened());
	union result_union {
		function_pointer typed;
		#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
			::PROC
		#else
			void *
		#endif
			untyped;
	} result;
	#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
		result.untyped = ::dlsym(underlying_, decorated_symbol(name).c_str());
		if(!result.untyped) resolve_symbol_error(name, std::string(::dlerror()));
	#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
		result.untyped = ::GetProcAddress(underlying_, decorated_symbol(name).c_str());
		if(!result.untyped) resolve_symbol_error(name, exceptions::desc());
	#else
		if(!underlying_->get_symbol(decorated_symbol(name), result.untyped)) resolve_symbol_error(name, Glib::Module::get_last_error());
	#endif
	return result.typed;
}

boost::filesystem::path resolver::path() const throw() {
	assert(opened());
	#if !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__POSIX
		return path_;
	#elif !defined UNIVERSALIS__QUAQUAVERSALIS && defined DIVERSALIS__OS__MICROSOFT
		char module_file_name[UNIVERSALIS__OS__MICROSOFT__MAX_PATH];
		::GetModuleFileNameA(underlying_, module_file_name, sizeof module_file_name);
		return boost::filesystem::path(module_file_name);
	#else
		return boost::filesystem::path(underlying_->get_name());
	#endif
}

std::string resolver::decorated_symbol(std::string const & name) throw() {
	// calling convention | modifier keyword | parameters stack push            | parameters stack pop | extern "C" symbol name mangling                   | extern "C++" symbol name mangling
	// register           | fastcall         | 3 registers then pushed on stack | callee               | '@' and arguments' byte size in decimal prepended | no standard
	// pascal             | pascal           | left to right                    | callee               | uppercase                                         | no standard
	// standard call      | stdcall          | right to left                    | callee               | preserved                                         | no standard
	// c declaration      | cdecl            | right to left, variable count    | caller               | '_' prepended                                     | no standard
	
	// note: the register convention is different from one compiler to another (for example left to right for borland, right to left for microsoft).
	// for borland's compatibility with microsoft's register calling convention: #define fastcall __msfastcall.
	// note: on the gnu compiler, one can use the #define __USER_LABEL_PREFIX__ to know what character is prepended to extern "C" symbols.
	// note: on microsoft's compiler, with cdecl, there's is no special decoration for extern "C" declarations, i.e., no '_' prepended.

	#if defined DIVERSALIS__COMPILER__FEATURE__NOT_CONCRETE
		return name;
	#elif defined DIVERSALIS__COMPILER__GNU
		return /*UNIVERSALIS__COMPILER__STRINGIZED(__USER_LABEL_PREFIX__) +*/ name;
	#elif defined DIVERSALIS__COMPILER__BORLAND
		return name;
	#elif defined DIVERSALIS__COMPILER__MICROSOFT
		return name;
	#else
		return name;
		//#error "Unsupported compiler."
	#endif
}

}}}
