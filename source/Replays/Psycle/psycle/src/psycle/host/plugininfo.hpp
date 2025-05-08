// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#pragma once

#include <psycle/host/detail/project.hpp>
#include <psycle/host/Global.hpp>

#include <string>

namespace psycle { namespace host {

		class PluginInfo
		{
		public:
			PluginInfo();
			virtual ~PluginInfo() throw() { }
			/*
			PluginInfo& operator=(const PluginInfo& newinfo);
			friend bool operator!=(PluginInfo& info1,PluginInfo& info2);
			*/
			std::string dllname;
			int32_t identifier;
			std::string error;
			MachineMode mode;
			MachineType type;
			std::string name;
			std::string vendor;
			std::string desc;
			std::string version;
			uint32_t APIversion;
			FILETIME FileTime;
			bool allow;
			uint32_t flags;
		};
}}

