///\implementation psycle::helpers.
#include <vector>
#include <cctype>
#include <string>

namespace psycle { namespace helpers {

namespace {
	void hexstring_to_vector(std::string const & string, std::vector<unsigned char> & vector) {
		vector.reserve(string.length());
		for(std::size_t i(0) ; i < string.length() ; ++i) {
			char c(string[i]);
			unsigned char v;
			if(std::isdigit(c)) v = c - '0';
			else {
				c = std::tolower(c);
				if('a' <= c && c <= 'f') v = 10 + c - 'a';
				else continue;
			}
			vector.push_back(v);
		}
	}
}

unsigned long long int hexstring_to_integer(std::string const & string) {
	unsigned long long int result(0);
	std::vector<unsigned char> v;
	hexstring_to_vector(string, v);
	unsigned int r(1);
	for(std::vector<unsigned char>::reverse_iterator i(v.rbegin()) ; i != v.rend() ; ++i)
	{
		result += *i * r;
		r *= 0x10;
	}
	return result;
}

}}
