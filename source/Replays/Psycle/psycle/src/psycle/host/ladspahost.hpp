// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include <psycle/host/detail/project.hpp>
// #include "machinehost.hpp"
#include "plugininfo.hpp"
#include "ladspa.h"
#include "Machine.hpp"

#include <string>
#include <map>
#include <vector>

namespace psycle { namespace host {	

class LadspaHost {
public:
	// virtual Machine* CreateMachine(PluginFinder&, const MachineKey &, Machine::id_type);
	static Machine* LoadPlugin(const std::string& dllpath, int macIdx, int key);	
	static std::vector<PluginInfo> LoadInfo(const std::string& dllpath);
protected:
	static void* LoadDll(const std::string &);
	static LADSPA_Descriptor_Function LoadDescriptorFunction(void*);
	static LADSPA_Handle Instantiate(const LADSPA_Descriptor*);
	static void UnloadDll(void*);
	static void *dlopenLADSPA(const char * pcFilename, int iFlag);
	/*virtual void FillPluginInfo(const std::string&, const std::string&, PluginFinder&);
	void* LoadDll( const std::string &  ) const;
	LADSPA_Descriptor_Function LoadDescriptorFunction(void*) const;
	LADSPA_Handle Instantiate(const LADSPA_Descriptor*) const;
	void UnloadDll( void* ) const;
	void *dlopenLADSPA(const char * pcFilename, int iFlag) const;
	std::string  plugin_path_;*/
};

}}

