// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PSY_FILTER_BASE__INCLUDED
#define PSYCLE__CORE__PSY_FILTER_BASE__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

#include <boost/signals2.hpp>
#include <string>

namespace psycle { namespace core {

class CoreSong;

class PsyFilterBase {
	public:
		virtual ~PsyFilterBase() {}

	public:
		virtual int version() const = 0;
		virtual std::string filePostfix() const = 0;
		virtual bool testFormat(const std::string & fileName) = 0;
		virtual bool load(const std::string & fileName, CoreSong& song) = 0;
		virtual bool save(const std::string & fileName, const CoreSong& song) = 0;
};

}}
#endif
