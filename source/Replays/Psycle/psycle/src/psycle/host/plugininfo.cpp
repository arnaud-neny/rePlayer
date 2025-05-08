// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/host/detail/project.private.hpp>
#include "Global.hpp"
#include "plugininfo.hpp"

namespace psycle { namespace host {
	PluginInfo::PluginInfo()
			: mode(MACHMODE_UNDEFINED)
			, type(MACH_UNDEFINED)
			, allow(true)
			, flags(0)
		{
			std::memset(&FileTime, 0, sizeof FileTime);
		}
		/*
		PluginInfo& operator=(const PluginInfo& newinfo)
		{
			mode=newinfo.mode;
			type=newinfo.type;
			strcpy(version,newinfo.version);
			strcpy(name,newinfo.name);
			strcpy(desc,newinfo.desc);
			zapArray(dllname,new char[sizeof(newinfo.dllname)+1]);
			strcpy(dllname,newinfo.dllname);
			return *this;
		}
		friend bool operator!=(PluginInfo& info1,PluginInfo& info2)
		{
			if ((info1.type != info2.type) ||
				(info1.mode != info2.mode) ||
				(strcmp(info1.version,info2.version) != 0 ) ||
				(strcmp(info1.desc,info2.desc) != 0 ) ||
				(strcmp(info1.name,info2.name) != 0 ) ||
				(strcmp(info1.dllname,info2.dllname) != 0)) return true;
			else return false;
		}
		*/
}}
