// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__FILTER_HELPER__INCLUDED
#define PSYCLE__CORE__FILTER_HELPER__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>
#include <string>

namespace psycle { namespace core {

///\todo: Have a way to allow to save program-specific data to the song filters.
/// This will allow extended Songs (for example, save some windows specific options that the linux version doesn't need to know about, etc..)
//  Idea: LoaderHelpers:
//  The extended class from CoreSong (passed as class T), could provide a FilterHelper class, which will be called in each load/save state.
//  psycle-core will provide two FilterHelpers: Psy2FilterHelper and Psy3FilterHelper, in order to ease loading the data that these formats
//  already store in a non-packed way.

class FilterHelper {
	public:
		virtual ~FilterHelper() {}
		///\todo: Add helper classes
};

}}

#endif
