// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "patternevent.h"

#include <sstream>

namespace psycle { namespace core {
		
PatternEvent::PatternEvent() :
	note_(notetypes::empty),
	inst_(255),
	mach_(255),
	cmd_(0),
	param_(0),
	volume_(255),
	offset_(0)
{
	for(int i = 0; i < 10; i++) paraCmdList_.push_back(PcmType());
}

std::string PatternEvent::toXml( int track ) const
{
	std::ostringstream xml;
	xml
		<< "<patevent track='" << track
		<< std::hex << "' note='" << (int) note_
		<< std::hex << "' mac='" << (int) mach_
		<< std::hex << "' inst='" << (int) inst_
		<< std::hex << "' cmd='" << (int) cmd_
		<< std::hex << "' param='" << (int) param_
		<<"' />\n";
	return xml.str();
}

}}
