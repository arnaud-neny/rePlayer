// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__NATIVE_HOST__INCLUDED
#define PSYCLE__CORE__NATIVE_HOST__INCLUDED
#pragma once

#include "machinehost.hpp"
#include "plugininfo.h"

#include <map>

namespace psycle { namespace plugin_interface {
	class CMachineInfo;
	class CMachineInterface;
}}

namespace psycle { namespace core {

class PSYCLE__CORE__DECL NativeHost : public MachineHost {
	protected:
		NativeHost(MachineCallbacks*);
	public:
		static NativeHost& getInstance(MachineCallbacks*);

		virtual Machine* CreateMachine(PluginFinder&, const MachineKey &, Machine::id_type);

        virtual Hosts::type hostCode() const { return Hosts::NATIVE; }
		virtual const std::string hostName() const { return "Native"; }
		virtual std::string const & getPluginPath(int) const { return plugin_path_; }
		virtual int getNumPluginPaths() const { return 1; }
		virtual void setPluginPath(std::string path) { plugin_path_ = path; }

	protected:
		virtual void FillPluginInfo(const std::string&, const std::string&, PluginFinder&);
		void* LoadDll(const std::string&);
		psycle::plugin_interface::CMachineInfo* LoadDescriptor(void*);
		psycle::plugin_interface::CMachineInterface* Instantiate(void*);
		void UnloadDll(void*);
		std::string plugin_path_;
};

}}
#endif
