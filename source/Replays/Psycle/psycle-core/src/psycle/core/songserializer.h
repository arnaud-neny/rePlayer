// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__SONG_SERIALIZER__INCLUDED
#define PSYCLE__CORE__SONG_SERIALIZER__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

#include <string>
#include <vector>

namespace psycle { namespace core {

class PsyFilterBase;
class CoreSong;
class MachineFactory;

class PSYCLE__CORE__DECL SongSerializer {
	public:
		SongSerializer();

		bool loadSong(const std::string & fileName, CoreSong& song);
		bool saveSong(const std::string & fileName, const CoreSong& song, int version);

	private:
		std::vector<PsyFilterBase*>  filters;
};

}}
#endif
