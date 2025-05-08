// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PLUGIN_FINDER
#define PSYCLE__CORE__PLUGIN_FINDER
#pragma once

#include "machinekey.hpp"
#include "plugininfo.h"

#include <vector>
#include <map>

namespace psycle { namespace core {

class PSYCLE__CORE__DECL PluginFinder {
	public:
		typedef std::map<MachineKey, PluginInfo>::const_iterator const_iterator;
		typedef std::map<MachineKey, PluginInfo>::iterator iterator;

		PluginFinder(bool delayedScan);
		virtual ~PluginFinder();

		virtual void addHost(Hosts::type);
		virtual bool hasHost(Hosts::type) const;

		virtual void AddInfo(const MachineKey &, const PluginInfo& );
		PluginInfo info(const MachineKey & key );
		const PluginInfo& info( const MachineKey & key ) const;
		bool hasKey( const MachineKey& key ) const;
		std::string lookupDllName( const MachineKey & key ) const;
		virtual void EnablePlugin(const MachineKey & key, bool enable);
	
		virtual void Initialize(bool clear=false);
		PluginFinder::const_iterator begin(Hosts::type) const;
		PluginFinder::const_iterator end(Hosts::type) const;
		virtual int size(Hosts::type) const;
		virtual void ClearMap(Hosts::type);
		virtual void PostInitialization();
		bool DelayedScan();

	protected:
		bool delayedScan_;
		PluginInfo empty_;
		std::vector<std::map<MachineKey, PluginInfo> > maps_;
};

}}
#endif
