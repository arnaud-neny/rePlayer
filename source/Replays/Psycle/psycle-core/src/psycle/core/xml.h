// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

///\todo This file should be moved to the common psycle/helpers

#ifndef PSYCLE__CORE__XML__INCLUDED
#define PSYCLE__CORE__XML__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

#ifdef PSYCLE__CORE__CONFIG__LIBXMLPP_AVAILABLE
	#include <libxml++/parsers/domparser.h>
#endif

	#include <sstream>

namespace psycle { namespace core {

///\todo move str and str_hex to their own helper file

template<typename T>
T str(const std::string &  value) {
	T result;
	std::stringstream str;
	str << value;
	str >> result;
	return result;
}

template<typename T>
T str_hex(const std::string & value) {
	///\todo does this really work?
	T result;
	std::stringstream str;
	str << value;
	str >> std::hex >> result;
	return result;
}

/// replaces with xml entities for xml writing.
// There are 5 predefined entity references in XML:
// &lt;   < less than 
// &gt;   > greater than
// &amp;  & ampersand 
// &apos; ' apostrophe
// &quot; " quotation mark
// Only the characters "<" and "&" are strictly illegal in XML. Apostrophes, quotation marks and greater than signs are legal. strict = true  replaces all.
std::string replaceIllegalXmlChr( const std::string & text, bool strict = true);

#ifdef PSYCLE__CORE__CONFIG__LIBXMLPP_AVAILABLE
	class xml_helper_element_not_found {
	};

	class xml_helper_attribute_not_found {
		public:
			std::string attr;
			xml_helper_attribute_not_found(std::string _attr) : attr(_attr) {}
	};

	xmlpp::Element& get_first_element(xmlpp::Node const& node, std::string tag);

	xmlpp::Attribute& get_attribute(xmlpp::Element const& e, std::string attr);

	template<class T> T get_attr(xmlpp::Element const& e, std::string attr) {
		return str<T>(get_attribute(e,attr).get_value());
	}

	template<class T> T get_attr_hex(xmlpp::Element const& e, std::string attr) {
		return str_hex<T>(get_attribute(e,attr).get_value());
	}

#endif

}}
#endif
