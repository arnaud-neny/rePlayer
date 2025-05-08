// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "nativehost.hpp"

#include "internalkeys.hpp"
#include "pluginfinder.h"
#include "plugin.h"

#include <universalis/os/dyn_link.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <sstream>

#if defined DIVERSALIS__OS__POSIX
	#include <dlfcn.h>
#elif defined DIVERSALIS__OS__MICROSOFT
	#include <universalis/os/include_windows_without_crap.hpp>
#else
	#error "unsupported platform"
#endif

namespace psycle { namespace core {

namespace loggers = universalis::os::loggers;
using namespace psycle::plugin_interface;

NativeHost::NativeHost(MachineCallbacks * calls) : MachineHost(calls){}

NativeHost & NativeHost::getInstance(MachineCallbacks* callb) {
	static NativeHost instance(callb);
	return instance;
}

Machine * NativeHost::CreateMachine(PluginFinder & finder, const MachineKey & key,Machine::id_type id)  {
	if(key == InternalKeys::failednative)  {
		Plugin *p = new Plugin(mcallback_, key, id, 0, 0, 0);
		return p;
	}

	//FIXME: This is a good place where to use exceptions. (task for a later date)
	std::string fullPath = finder.lookupDllName(key);
	if(fullPath.empty()) return 0;

	void* hInstance = LoadDll(fullPath);
	if(!hInstance) return 0;
	
	CMachineInfo* info = LoadDescriptor(hInstance);
	if(!info) {
		UnloadDll(hInstance);
		return 0;
	}

	CMachineInterface* maciface = Instantiate(hInstance);
	if(!maciface) {
		UnloadDll(hInstance);
		return 0;
	}

	Plugin * p = new Plugin(mcallback_, key, id, hInstance, info, maciface );
	p->Init();
	return p;
}


void NativeHost::FillPluginInfo(const std::string& fullName, const std::string& fileName, PluginFinder& finder) {
	#if defined DIVERSALIS__OS__POSIX
		if(fileName.find( "libpsycle-plugin-") == std::string::npos) return;
	#else
		if(fileName.find(".dll") == std::string::npos) return;
	#endif
	
	void* hInstance = LoadDll(fullName);
	if(!hInstance) return;
	
	CMachineInfo* minfo = LoadDescriptor(hInstance);
	if(minfo) {
		PluginInfo pinfo;
		///\todo !!!
		//pinfo.setFileTime();
		pinfo.setName(minfo->Name);
		bool isSynth = minfo->Flags == 3;
		pinfo.setRole(isSynth ? MachineRole::GENERATOR : MachineRole::EFFECT);
		pinfo.setLibName(fullName);
		pinfo.setFileTime(boost::filesystem::last_write_time(boost::filesystem::path(fullName)));
		{
			std::ostringstream o;
			o << std::hex << (minfo->APIVersion&0x7FFF);
			if(minfo->APIVersion&0x8000) {
				o << " (64 bits)";
			} else { o << " (32 bits)"; }
			pinfo.setApiVersion(o.str());
		}
		{
			std::ostringstream o;
			std::string version;
			o << std::hex << minfo->PlugVersion;
			pinfo.setPlugVersion(o.str());
		}
		pinfo.setAuthor(minfo->Author);
		pinfo.setAllow(true);
		MachineKey key(hostCode(), fileName, 0);
		finder.AddInfo(key, pinfo);
	}
	///\todo: Implement the bad cases, so that the plugin can be added to 
	/// the finder as bad.
	UnloadDll(hInstance);
}

void* NativeHost::LoadDll(std::string const & file_name) {
	void* hInstance;

	using universalis::os::dyn_link::path_list_type;
	using universalis::os::dyn_link::lib_path;

	path_list_type const old_path(lib_path());
	path_list_type new_path(old_path);
	new_path.push_back(path_list_type::value_type(file_name).branch_path());
	lib_path(new_path);

	#if defined DIVERSALIS__OS__POSIX
		hInstance = ::dlopen(file_name.c_str(), RTLD_LAZY /*RTLD_NOW*/);
		if (!hInstance && loggers::exception()) {
			std::ostringstream s;
			s << "psycle: core: nativehost: LoadDll:" << std::endl
				<< "could not load library: " << file_name << std::endl
				<<  dlerror();
			loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}
	#else
		// Set error mode to disable system error pop-ups (for LoadLibrary)
		UINT uOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
		hInstance = LoadLibraryA( file_name.c_str() );
		// Restore previous error mode
		SetErrorMode( uOldErrorMode );
		if (!hInstance && loggers::exception()) {
			std::ostringstream s;
			s << "psycle: core: nativehost: LoadDll:" << std::endl
				<< "could not load library: " << file_name
				<< universalis::os::exceptions::desc();
			loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}
	#endif

	lib_path(old_path);
	
	return hInstance;
}


CMachineInfo* NativeHost::LoadDescriptor(void* hInstance) {
	try {
		#if defined DIVERSALIS__OS__POSIX
			GETINFO GetInfo = (GETINFO) dlsym( hInstance, "GetInfo");
			if(!GetInfo && loggers::exception()) {
				std::ostringstream s;
				s << "psycle: core: nativehost: LoadDescriptor:" << std::endl
					<< "Cannot load symbols: " << dlerror();
				loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
				return 0;
			}
		#else
			GETINFO GetInfo = (GETINFO) GetProcAddress( static_cast<HINSTANCE>( hInstance ), "GetInfo" );
			if(!GetInfo) {
				///\todo readd the original code here!
				if(!GetInfo && loggers::exception()) {
					std::ostringstream s;
					s << "psycle: core: nativehost: LoadDescriptor:" << std::endl
						<< "could not load symbols: " 
						<< universalis::os::exceptions::desc();
					loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					return 0;
				}
			}
		#endif
		CMachineInfo* info_ = GetInfo();
		// version 10 and 11 didn't use HEX representation.
		// Also, verify for 32 or 64bits.
		if(!(info_->APIVersion == 11 && (MI_VERSION&0xFFF0) == 0x0010)
			&& !((info_->APIVersion&0xFFF0) == (MI_VERSION&0xFFF0)) && loggers::exception()) {

			std::ostringstream s;
			s << "plugin version not supported" << info_->APIVersion;
			loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
			info_ = 0;
		}
		return info_;
	} catch (...) {
		if(loggers::exception()) {
			std::ostringstream s;
			s << "exception while getting plugin info handler: " << universalis::compiler::exceptions::ellipsis_desc();
			loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}
		return 0;
	}
}

CMachineInterface* NativeHost::Instantiate(void * hInstance) {      
	try {
		#if defined DIVERSALIS__OS__POSIX
			CREATEMACHINE GetInterface =  (CREATEMACHINE) dlsym(hInstance, "CreateMachine");
		#else
			CREATEMACHINE GetInterface = (CREATEMACHINE) GetProcAddress( (HINSTANCE)hInstance, "CreateMachine" );
		#endif
		if(!GetInterface) {
			#if defined DIVERSALIS__OS__POSIX
				if(loggers::exception()) {
					std::ostringstream s;
					s << "Cannot load symbol: " << dlerror();
					loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
				}
			#else
				if(loggers::exception()) {
					std::ostringstream s;
					s << "could not load symbol: " << universalis::os::exceptions::desc();
					loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
				}
			#endif
			return 0;
		} else {
			return GetInterface();
		}
	} catch (...) {
		if(loggers::exception()) {
			std::ostringstream s;
			s << "exception while creating interface instance: " << universalis::compiler::exceptions::ellipsis_desc();
			loggers::exception()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}
		return 0;
	}
}

void NativeHost::UnloadDll(void* hInstance) {
	assert(hInstance);
	#if defined DIVERSALIS__OS__POSIX
		dlclose(hInstance);
	#else
		::FreeLibrary((HINSTANCE)hInstance);
	#endif
}

}}
