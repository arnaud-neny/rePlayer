// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net
#pragma once

#include <psycle/host/detail/project.hpp>
#include "Global.hpp"

namespace psycle { namespace host {

/*abstact*/ class MachineLoader {
	public:
		MachineLoader(){};
		virtual ~MachineLoader(){};
		virtual bool lookupDllName(const std::string & name, std::string & result, MachineType type, int32_t& shellIdx) = 0;
		virtual bool TestFilename(const std::string & name,const int32_t shellIdx) = 0;

		virtual void LoadPluginInfo(bool verify=true) = 0;
		virtual void ReScan(bool regenerate=true) = 0;
		virtual bool IsLoaded() = 0;
};

}}
