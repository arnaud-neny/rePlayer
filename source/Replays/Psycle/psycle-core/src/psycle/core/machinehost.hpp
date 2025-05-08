// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__MACHINE_HOST__INCLUDED
#define PSYCLE__CORE__MACHINE_HOST__INCLUDED
#pragma once

#include "machinekey.hpp"
#include "plugininfo.h"
#include "machine.h"

#include <string>

namespace psycle { namespace core {

class MachineCallbacks;
class Machine;
class PluginFinder;

class PSYCLE__CORE__DECL MachineHost {
protected:
	MachineHost(MachineCallbacks*);
public:
	virtual ~MachineHost() {}
	virtual Machine* CreateMachine(PluginFinder&, const MachineKey &, Machine::id_type) = 0;
	virtual void FillFinderData(PluginFinder&, bool clearfirst=false);

    virtual Hosts::type hostCode() const = 0;
	virtual const/*no const*/ std::string hostName() const = 0;

    virtual std::string const & getPluginPath(int) const { static std::string ret = ""; return ret; };
	virtual int getNumPluginPaths() const { return 0; }
    virtual void setPluginPath(std::string /*const &*/) {};
protected:
	virtual void FillPluginInfo(const std::string&, const std::string& , PluginFinder& ) = 0;

	MachineCallbacks* mcallback_;
};

}}
#endif
