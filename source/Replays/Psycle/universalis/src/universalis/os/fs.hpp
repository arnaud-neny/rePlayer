// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net

///\interface universalis::os::fs

#pragma once
#include <universalis/detail/project.hpp>
#include <boost/filesystem/path.hpp>
#include <string>

namespace universalis { namespace os { namespace fs {
	using namespace boost::filesystem;

	/// the user's home dir
	UNIVERSALIS__DECL path const & home();

	/// the path in the user's home dir where an application should write data that are local to the computer
	UNIVERSALIS__DECL path const home_app_local(std::string const & app_name);

	/// the path in the user's home dir where an application should write data that are not local to the computer
	UNIVERSALIS__DECL path const home_app_roaming(std::string const & app_name);

	// the path in the filesystem where an application should write settings for all users.
	UNIVERSALIS__DECL path const all_users_app_settings(std::string const & app_name);

	#if 0 ///\todo not implemented yet on posix
		/// the path to the image file of the currently executing process
		UNIVERSALIS__DECL path const & process_executable_file_path()
	#endif

	#if defined DIVERSALIS__OS__MICROSOFT
		//#include <windows.h>
		/// microsoft's MAX_PATH has too low value for ntfs
		///\see <cstdio>'s FILENAME_MAX
		#define UNIVERSALIS__OS__MICROSOFT__MAX_PATH (MAX_PATH < (1 << 12) ? (1 << 12) : MAX_PATH)
	#endif
}}}
