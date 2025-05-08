// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "pluginfinder.h"

#include <cassert>

namespace psycle { namespace core {

PluginFinder::PluginFinder(bool delayedScan) {
	delayedScan_ = delayedScan;
}

PluginFinder::~PluginFinder() {
}

void PluginFinder::Initialize(bool /*clear*/) {
}

void PluginFinder::addHost(Hosts::type type) {
	if(type >= maps_.size()) maps_.resize(type + 1);
}

bool PluginFinder::hasHost(Hosts::type type) const {
	return (type < maps_.size());
}

PluginFinder::const_iterator PluginFinder::begin(Hosts::type host) const {
	assert(host < maps_.size());
	return maps_[host].begin();
}

PluginFinder::const_iterator PluginFinder::end(Hosts::type host) const {
	assert(host < maps_.size());
	return maps_[host].end();
}

int PluginFinder::size(Hosts::type host) const {
	assert(host < maps_.size());
	return static_cast<int>(maps_[host].size());
}
void PluginFinder::AddInfo(const MachineKey & key, const PluginInfo& info) {
	#if 0
	std::cout << "pluginfinder::addinfo: " << key.dllName() << " host:" << key.host() << std::endl;
	#endif
	assert(key.host() < maps_.size());
	maps_[key.host()].insert(std::pair<MachineKey, PluginInfo>(key,info));
}

const PluginInfo & PluginFinder::info ( const MachineKey & key ) const {
	if(!hasHost(key.host())) return empty_;

	PluginFinder::const_iterator it = maps_[key.host()].find( key );
	if(it != maps_[key.host()].end()) return it->second;
	else return empty_;
}

PluginInfo PluginFinder::info( const MachineKey & key ) {
	if(!hasHost(key.host())) return empty_;

	PluginFinder::const_iterator it = maps_[key.host()].find( key );
	if(it != maps_[key.host()].end()) return it->second;
	else return empty_;
}
void PluginFinder::EnablePlugin(const MachineKey & key, bool enable) {
	if(!hasHost(key.host())) return;

	PluginFinder::iterator it = maps_[key.host()].find( key );
	if(it != maps_[key.host()].end()) {
		it->second.setAllow(enable);
	}
}


std::string PluginFinder::lookupDllName( const MachineKey & key ) const {
	return info(key).libName();
}

bool PluginFinder::hasKey( const MachineKey& key ) const {
	if(!hasHost(key.host())) return false;
	std::map<MachineKey, PluginInfo> const & thisMap = maps_[key.host()];

	std::map< MachineKey, PluginInfo >::const_iterator it = thisMap.find(key);
	if(it != thisMap.end()) return true;
	else return false;
}

void PluginFinder::ClearMap(Hosts::type host) {
	if(hasHost(host)) maps_[host].clear();
}
bool PluginFinder::DelayedScan() {
	return delayedScan_;
}
void PluginFinder::PostInitialization() { }
}}
