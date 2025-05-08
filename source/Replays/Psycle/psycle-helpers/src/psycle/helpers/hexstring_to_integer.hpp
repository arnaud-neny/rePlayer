///\interface psycle::helpers
#pragma once

#include <string>

namespace psycle { namespace helpers {

	/// parses an hexadecimal string to convert it to an integer
	unsigned long long int hexstring_to_integer(std::string const &);

	/// parses an hexadecimal string to convert it to an integer
	template<typename X>
	void hexstring_to_integer(std::string const & s, X & x) { x = static_cast<X>(hexstring_to_integer(s)); }

}}
