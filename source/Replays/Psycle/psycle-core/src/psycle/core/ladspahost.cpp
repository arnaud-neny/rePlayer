// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "ladspahost.hpp"

#include "pluginfinder.h"
#include "ladspamachine.h"
#include "playertimeinfo.h"
#include <boost/filesystem.hpp>
#include <iostream>

#if defined DIVERSALIS__OS__POSIX
	#include <dlfcn.h>
#elif defined DIVERSALIS__OS__MICROSOFT
	#include <universalis/os/include_windows_without_crap.hpp>
#else
	#error "unsupported platform"
#endif

namespace psycle { namespace core {

LadspaHost::LadspaHost(MachineCallbacks*calls)
:MachineHost(calls) {}

LadspaHost::~LadspaHost() {}

LadspaHost& LadspaHost::getInstance(MachineCallbacks* callb) {
	static LadspaHost instance(callb);
	return instance;
}

Machine* LadspaHost::CreateMachine(PluginFinder& finder, const MachineKey& key,Machine::id_type id) {
	///\todo This is a good place where to use exceptions. (task for a later date)
	std::string fullPath = finder.lookupDllName(key);
	if (fullPath.empty()) return 0;
	void* hInstance = LoadDll(fullPath);
	if (!hInstance) return 0;
	
	LADSPA_Descriptor_Function pfDescriptorFunction = LoadDescriptorFunction(hInstance);
	if (pfDescriptorFunction) {
		/*Step three: Get the descriptor of the selected plugin (a shared library can have
		several plugins*/
		const LADSPA_Descriptor* psDescriptor = pfDescriptorFunction(key.index());
		if (psDescriptor) {
			LADSPA_Handle handle = Instantiate(psDescriptor);
			if(handle) {
				LADSPAMachine * p = new LADSPAMachine(mcallback_, key, id, hInstance, psDescriptor, handle );
				p->Init();
				return p;
			}
		}
	}
	UnloadDll(hInstance);
	return 0;
}

void LadspaHost::FillPluginInfo(const std::string& fullName, const std::string& fileName,PluginFinder& finder) {
	#if defined __unix__ || defined __APPLE__
	#else
		if(fileName.find(".dll") == std::string::npos) return;
	#endif

	void* hInstance = LoadDll(fullName);
	if (!hInstance) return;
	
	LADSPA_Descriptor_Function pfDescriptorFunction;
	pfDescriptorFunction = LoadDescriptorFunction(hInstance);
	if (pfDescriptorFunction) {
		unsigned int index = 0;
		const LADSPA_Descriptor* psDescriptor = pfDescriptorFunction(index);
		while(psDescriptor) {
			PluginInfo pinfo;
			pinfo.setName( psDescriptor->Name );
			pinfo.setRole( MachineRole::EFFECT );
			pinfo.setLibName( fullName );
			pinfo.setFileTime(boost::filesystem::last_write_time(boost::filesystem::path(fullName)));
			//pinfo.setApiVersion( version );
			//pinfo.setVersion( version );
			pinfo.setAuthor( psDescriptor->Maker );
			pinfo.setAllow( true );
			MachineKey key( hostCode() , fileName, index );
			finder.AddInfo(key, pinfo);
			psDescriptor = pfDescriptorFunction(++index);
		}
	}
	UnloadDll(hInstance);
}

void LadspaHost::setPluginPath(std::string path) {
	///\todo this just uses the first path in getenv. Do this for each path.
	// The best way would be pre-process and store a vector of strings.
	#if defined __unix__ || defined __APPLE__
		std::string::size_type dotpos = path.find(':');
		if(dotpos != path.npos ) plugin_path_ = path.substr( 0, dotpos );
		else plugin_path_ = path;
	#else
		plugin_path_ = path;
	#endif
}


void* LadspaHost::LoadDll( const std::string & fileName ) const
{
	void * libHandle_ = 0;
	// Step one: Open the shared library.
	#if defined __unix__ || defined __APPLE__
		libHandle_ = dlopen( fileName.c_str() , RTLD_NOW);
	#else
		// Set error mode to disable system error pop-ups (for LoadLibrary)
		UINT uOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
		libHandle_ = LoadLibraryA( fileName.c_str() );
		// Restore previous error mode
		SetErrorMode( uOldErrorMode );
	#endif
	return libHandle_;
}

LADSPA_Descriptor_Function LadspaHost::LoadDescriptorFunction( void* libHandle_ ) const {
	// Step two: Get the entry function.
	LADSPA_Descriptor_Function pfDescriptorFunction = (LADSPA_Descriptor_Function)
		#if defined __unix__ || defined __APPLE__
			dlsym(libHandle_, "ladspa_descriptor");
		#else
			GetProcAddress(static_cast<HINSTANCE>( libHandle_ ), "ladspa_descriptor");
	#endif
	return pfDescriptorFunction;
}

LADSPA_Handle LadspaHost::Instantiate(const LADSPA_Descriptor* psDescriptor) const {
	if(LADSPA_IS_INPLACE_BROKEN(psDescriptor->Properties)) return 0;
	// Step four: Create (instantiate) the plugin, so that we can use it.
	return psDescriptor->instantiate(psDescriptor,mcallback_->timeInfo().sampleRate());
}

void LadspaHost::UnloadDll( void* hInstance ) const {
	assert(hInstance);
	#if defined __unix__ || defined __APPLE__
		::dlclose(hInstance);
	#else
		::FreeLibrary(static_cast<HINSTANCE>(hInstance));
	#endif
}

/// This function provides a wrapping of dlopen(). When the filename is
/// not an absolute path (i.e. does not begin with / character), this
/// routine will search the LADSPA_PATH for the file.
void * LadspaHost::dlopenLADSPA(const char * pcFilename, int iFlag) const {
	std::string filename_(pcFilename);
			
	#if 0
		char *pcBuffer;
		const char * pcStart, * pcEnd ,* pcLADSPAPath;
		bool endsInSO, needsSlash;
		size_t iFilenameLength;
	#endif
			
	const char * pcLADSPAPath;
	void * pvResult(0);

	#if defined __unix__ || defined __APPLE__
		if(filename_.compare(filename_.length() - 3, 3, ".so")) filename_.append(".so");
	#else
		if(filename_.compare(filename_.length()-3, 3, ".dll")) filename_.append(".dll");
	#endif
			
	#if defined __unix__ || defined __APPLE__
		if(filename_.c_str()[0] == '/') {
	#else
		if(filename_.c_str()[0] == '\\') {
	#endif
			
		// The filename is absolute. Assume the user knows what he/she is doing and simply dlopen() it.
		#if defined __unix__ || defined __APPLE__
			pvResult = dlopen(filename_.c_str(), iFlag);
		#else
			// Set error mode to disable system error pop-ups (for LoadLibrary)
			UINT uOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
			pvResult = LoadLibraryA( filename_.c_str() );
			// Restore previous error mode
			SetErrorMode( uOldErrorMode );
		#endif
		if (pvResult) return pvResult;
	} else {
		/* If the filename is not absolute then we wish to check along the
		LADSPA_PATH path to see if we can find the file there. We do
		NOT call dlopen() directly as this would find plugins on the
		LD_LIBRARY_PATH, whereas the LADSPA_PATH is the correct place
		to search. */
		
		pcLADSPAPath = std::getenv("LADSPA_PATH");
		if(!pcLADSPAPath)
			pcLADSPAPath = 
			#if defined __unix__ || defined __APPLE__
				"/usr/lib/ladspa/"
			#else
				"C:\\Programme\\Audacity\\Plug-Ins\\"
			#endif
			;
		std::string directory(pcLADSPAPath);
		std::string::size_type dotindex(0),dotpos(0),prevdotpos(0);
		
		dotpos = directory.find(':', dotindex++);
		do {
			std::string fullname = directory.substr(prevdotpos,dotpos);
			#if defined __unix__ || defined __APPLE__
				if(fullname.c_str()[fullname.length()-1] != '/') fullname.append("/");
			#else
				if(fullname.c_str()[fullname.length()-1] != '\\') fullname.append("\\");
			#endif
			fullname.append(filename_);
		
			#if defined __unix__ || defined __APPLE__
				pvResult = dlopen(fullname.c_str(), iFlag);
			#else
				// Set error mode to disable system error pop-ups (for LoadLibrary)
				UINT uOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
				pvResult = LoadLibraryA( fullname.c_str() );
				// Restore previous error mode
				SetErrorMode( uOldErrorMode );
			#endif

			if(pvResult) return pvResult;
			prevdotpos = dotpos;
			dotpos = directory.find(':',dotindex++);
		} while(dotpos != directory.npos);
	}
	
	/* If nothing has worked, then at least we can make sure we set the
	correct error message - and this should correspond to a call to
	dlopen() with the actual filename requested. */

	#if defined __unix__ || defined __APPLE__
		pvResult = dlopen( pcFilename, iFlag);
	#else
		// Set error mode to disable system error pop-ups (for LoadLibrary)
		UINT uOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
		pvResult = LoadLibraryA( pcFilename );
		// Restore previous error mode
		SetErrorMode( uOldErrorMode );
	#endif
	return pvResult;
}
	
}}
