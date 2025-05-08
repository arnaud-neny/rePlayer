// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "machinehost.hpp"

#include "pluginfinder.h"

#include <boost/filesystem.hpp>
#include <list>

namespace psycle { namespace core {

MachineHost::MachineHost(MachineCallbacks * calls) : mcallback_(calls) {}

void MachineHost::FillFinderData(PluginFinder& finder, bool clearfirst) {
	class populate_plugin_list {
		public:
			populate_plugin_list(std::list<boost::filesystem::path> & result, boost::filesystem::path const & directory) {
				using namespace boost::filesystem;
				for(directory_iterator i(directory); i != directory_iterator(); ++i) {
					file_status const status(i->status());
					if(is_directory(status) && i->path() != "." && i->path() != "..")
						populate_plugin_list(result, i->path());
					else if(is_regular(status))
						result.push_back(i->path());
				}
			}
	};

	if(clearfirst) finder.ClearMap(hostCode());

	for(int i = 0; i < getNumPluginPaths(); ++i) {
		if(getPluginPath(i) == "") continue;

		std::list<boost::filesystem::path> fileList;
		try {
			populate_plugin_list(fileList, getPluginPath(i));
		} catch(std::exception & e) {
			std::cerr
				<< "psycle: host: warning: Unable to scan your " << hostName() << " plugin directory with path: " << getPluginPath(i)
				<< ".\nPlease make sure the directory exists.\nException: " << e.what();
			return;
		}
		for(std::list<boost::filesystem::path>::const_iterator i = fileList.begin(); i != fileList.end(); ++i) {
		#if BOOST_FILESYSTEM_VERSION >= 3
			std::string const base_name(i->filename().string());
			MachineKey key(hostCode(), base_name, 0);
			if(!finder.hasKey(key)) FillPluginInfo(i->string(), base_name, finder);
		#else
			std::string const base_name(i->leaf());
			MachineKey key(hostCode(), base_name, 0);
			if(!finder.hasKey(key)) FillPluginInfo(i->file_string(), base_name, finder);
		#endif
		}
	}
}

}}
