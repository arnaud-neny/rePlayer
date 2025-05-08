// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PLUGIN_CACHE
#define PSYCLE__CORE__PLUGIN_CACHE
#pragma once

#include "machine.h"
#include "machinekey.hpp"
#include "pluginfinder.h"

namespace psycle { namespace core {

class PSYCLE__CORE__DECL PluginFinderCache: public PluginFinder {
	public:
		PluginFinderCache(bool delayedScan);
		
		/*override*/ void Initialize(bool clear=false);
		/*override*/ void EnablePlugin(const MachineKey & key, bool enable);
		/*override*/ void PostInitialization();

	protected:
		bool loadCache();
		bool saveCache();
		void deleteCache();
};

}}
#endif
