// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__LADSPA_HOST__INCLUDED
#define PSYCLE__CORE__LADSPA_HOST__INCLUDED
#pragma once

#include "machinehost.hpp"
#include "ladspa.h"

#include <string>
#include <map>

namespace psycle { namespace core{

class PSYCLE__CORE__DECL LadspaHost : public MachineHost {
protected:
	LadspaHost(MachineCallbacks*);
public:
	virtual ~LadspaHost();
	static LadspaHost& getInstance(MachineCallbacks*);

	virtual Machine* CreateMachine(PluginFinder&, const MachineKey &, Machine::id_type);

    virtual Hosts::type hostCode() const { return Hosts::LADSPA; }
	virtual const std::string hostName() const { return "Ladspa"; }
	virtual std::string const & getPluginPath(int) const { return plugin_path_; }
	virtual int getNumPluginPaths() const { return 1; }
	virtual void setPluginPath(std::string path);

protected:
	virtual void FillPluginInfo(const std::string&, const std::string&, PluginFinder&);
	void* LoadDll( const std::string &  ) const;
	LADSPA_Descriptor_Function LoadDescriptorFunction(void*) const;
	LADSPA_Handle Instantiate(const LADSPA_Descriptor*) const;
	void UnloadDll( void* ) const;
	void *dlopenLADSPA(const char * pcFilename, int iFlag) const;
	std::string  plugin_path_;
};

}}
#endif
