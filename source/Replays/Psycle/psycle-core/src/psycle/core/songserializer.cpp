// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "songserializer.h"

#include "song.h"
#include "psy2filter.h"
#include "psy3filter.h"
//#include "psy4filter.h"
#include <boost/filesystem.hpp>
#include <iostream>

namespace psycle {  namespace core {

SongSerializer::SongSerializer() {
	filters.push_back(Psy2Filter::getInstance());
	filters.push_back(Psy3Filter::getInstance());
	///\todo Psy4Filter doesn't build currently
	//filters.push_back( Psy4Filter::getInstance() );
}

bool SongSerializer::loadSong(const std::string & fileName, CoreSong& song) {
	if(boost::filesystem::exists(fileName)) {
		for(std::vector<PsyFilterBase*>::iterator it = filters.begin(); it != filters.end(); ++it) {
			PsyFilterBase* filter = *it;
			if(filter->testFormat(fileName)) return filter->load(fileName,song);
		}
	}
	std::cerr << "SongSerializer::loadSong(): Couldn't find appropriate filter for file: " << fileName << std::endl;
	return false;
}

bool SongSerializer::saveSong(const std::string& fileName, const CoreSong& song, int version) {
	for (std::vector<PsyFilterBase*>::iterator it = filters.begin(); it < filters.end(); it++) {
		PsyFilterBase* filter = *it;
		if ( filter->version() == version ) {
			// check postfix
			std::string newFileName = fileName;
			std::string::size_type dotPos = fileName.rfind(".");
			if ( dotPos == std::string::npos ) 
				// append postfix
				newFileName = fileName + "." + filter->filePostfix();
			return filter->save(newFileName,song);
		}
	}
	std::cerr << "SongSerializer::saveSong(): Couldn't find appropriate filter for file format version " << version << std::endl;
	return false;
}

}}
