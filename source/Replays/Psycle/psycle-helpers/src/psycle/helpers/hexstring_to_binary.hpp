///\interface psycle::helpers
#pragma once

#include <string>

namespace psycle { namespace helpers {

	/// parses an hexadecimal string to convert it to an binary array
	std::size_t hexstring_to_binary(std::string const & s, void* x, std::size_t& max_size);

	/// converts a binary array to an hexadecimal string
	std::string binary_to_hexstring(void* x, std::size_t bytesize);

}}
