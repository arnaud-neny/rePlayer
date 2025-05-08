///\interface psycle::helpers
#pragma once
#include <algorithm>
#include <string>
#include <iostream>

namespace psycle { namespace helpers {

namespace pathname_validate {
	inline bool isForbidden( char c )
	{
		static std::string forbiddenChars( "\\/:?\"<>|*" );

		return std::string::npos != forbiddenChars.find( c );
	}

	inline void validate(std::string& path) {
		std::replace_if( path.begin(), path.end(), isForbidden, ' ' );
	}
}}}
