///\implementation psycle::helpers.
#include <cctype>
#include <string>
#include <sstream>

namespace psycle { namespace helpers {

	/// parses an hexadecimal string to convert it to an binary array
	std::size_t hexstring_to_binary(std::string const & string, void* x, std::size_t& max_size)
	{
		unsigned char* out = reinterpret_cast<unsigned char*>(x);
		unsigned char accumulate = '\0';
		bool even = false;
		std::size_t bytesize = 0;
		for(std::size_t i(0) ; i < string.length() ; ++i) {
			char c(string[i]);
			unsigned char v;
			if(std::isdigit(c)) v = c - '0';
			else {
				c = std::tolower(c);
				if('a' <= c && c <= 'f') v = 10 + c - 'a';
				else continue;
			}
			even = !even;
			if(even) {
				accumulate = v;
			}
			else {
				accumulate = (accumulate<<4) | v;
				out[bytesize] = accumulate;
				bytesize++;
				if(bytesize >= max_size) break;
			}
		}
		return bytesize;
	}

	/// converts a binary array to an hexadecimal string
	std::string binary_to_hexstring(void* x, std::size_t bytesize)
	{
		std::ostringstream out;
		unsigned char* in = reinterpret_cast<unsigned char*>(x);
		for(std::size_t i(0) ; i < bytesize ; ++i) {
			out << std::hex << (in[i] >> 4) << (in[i]&0xF);
		}
		return out.str();
	}

}}
