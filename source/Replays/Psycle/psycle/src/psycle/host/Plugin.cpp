///\file
///\brief implementation file for psycle::host::Plugin

#include <psycle/host/detail/project.private.hpp>
#include "Plugin.hpp"
#include "FileIO.hpp"
#include "Song.hpp"
#include "machineloader.hpp"

#include "Zap.hpp"
#include <diversalis/os.hpp>
#include <universalis.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <cstdlib> // for environment variables functions
#include <string>
#include <sstream>
//#include <algorithm>
#include <cctype>

namespace psycle::plugin_interface
{
    bool PluginFactory::Register(const char* dllName, CMachineInfo const* const (*getInfo)(), CMachineInterface* (*createMachine)())
	{
		PluginEntry plug;
		plug.m_dllName = dllName;
		plug.m_getInfo = getInfo;
		plug.m_createMachine = createMachine;
		Get().push_back(std::move(plug));
		return true;
    };
}

namespace psycle
{
	namespace host
	{
		using namespace psycle::plugin_interface;
		namespace loggers = universalis::os::loggers;
		typedef CMachineInfo * (* GETINFO) ();
		typedef CMachineInterface * (* CREATEMACHINE) ();
		const char* MIDI_CHAN_NAMES[16] = {
			"MIDI Channel 01", "MIDI Channel 02","MIDI Channel 03","MIDI Channel 04",
			"MIDI Channel 05","MIDI Channel 06","MIDI Channel 07","MIDI Channel 08",
			"MIDI Channel 09","MIDI Channel 10","MIDI Channel 11","MIDI Channel 12",
			"MIDI Channel 13","MIDI Channel 14","MIDI Channel 15","MIDI Channel 16"
		};

		PluginFxCallback Plugin::_callback;

		int PluginFxCallback::CallbackFunc(int cbkID, int /*par1*/, int /*par2*/, void* par3) {
			if (cbkID == CBID_GET_WINDOW) {
				void** outWnd = (void**)par3;
				*outWnd = hWnd;
				return true;
			}
			return false;
		}

		bool PluginFxCallback::FileBox(bool openMode, char* filter, char inoutName[]) { 
/* rePlayer begin
			int modes=OFN_HIDEREADONLY|OFN_EXPLORER|OFN_ENABLESIZING;
			if (openMode) {	modes |= OFN_FILEMUSTEXIST|OFN_DONTADDTORECENT; }
			else { modes |= OFN_NOREADONLYRETURN|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST; }
			std::string ext = filter;
			int pos = ext.find("|*.");
			if (pos >= 0) {
				ext = ext.substr(pos+3,ext.find("|",pos+3)-(pos+3));
			}
			CFileDialog fileDlg (openMode, ext.c_str(), NULL, modes, filter);
			std::string dir;
			if (inoutName[0] != '\0') {
				std::string dir2 = inoutName;
				dir = dir2.substr(0,dir2.rfind('\\')+1);
			}
			if (dir.length() == 0 ) {
				dir =  Global::configuration().GetAbsolutePluginDir();
			}
			fileDlg.m_ofn.lpstrInitialDir = dir.c_str();
			if(fileDlg.DoModal () == IDOK) {
				strncpy(inoutName,(LPCSTR)fileDlg.GetPathName(),1024);
				inoutName[1023]='\0';
				return true;
			}
*/
			assert(0); // rePlayer end
			return false;
		}

		#pragma warning(push)
			#pragma warning(disable:4355) // 'this' : used in base member initializer list
			Plugin::Plugin(int index)
				: _dll(0)
				, proxy_(*this)
				, _psAuthor("")
				, _psDllName("")
				, _psName("")
			{
				_macIndex = index;
				_type = MACH_PLUGIN;
				_mode = MACHMODE_FX;
				std::sprintf(_editName, "native plugin");
				InitializeSamplesVector();
			}
		#pragma warning(pop)

		Plugin::~Plugin() throw()
		{
			try {
				Free();
			} catch(...){}
		}

		#if 1 /* <bohan> i'm really not sure about the origin of the problem so i prefer to add the work around unconditionally */ 
			
			// dos/win9x needs a work around for boost::filesystem::equivalent
			// Or could that actually simply be due to FAT filesystems?
			// If that's FAT that's not working, we may encounter problems even on NT (or unices?)
			// since people still use FAT partitions on USB keys etc.
			
			namespace boost { namespace filesystem {
					using namespace ::boost::filesystem;
					bool equivalent(path const & path1, path const & path2) {
						class unique {
							public:
								path operator()(path const & input) {
									std::string s((input.is_complete() ? input : current_path() / input).string());
									std::transform(s.begin(), s.end(), s.begin(), [](auto a) { return std::tolower(a); }); // rePlayer
									return path(s);
								}
						} unique;
						return unique(path1) == unique(path2);
					}
			}}
		#endif

		void Plugin::Instance(std::string file_name) throw(psycle::host::exception)
		{
#if 0
			char const static path_env_var_name[] =
			{
				#if defined DIVERSALIS__OS__LINUX
					"LD_LIBRARY_PATH"
				#elif defined DIVERSALIS__OS__MICROSOFT
					"PATH"
				#else
					#error unknown dynamic linker
				#endif
			};
			//_dll = ::LoadLibraryEx(file_name.c_str(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
			// Or better, add all intermediate dirs from the root plugin dir down to this plugin's dir to the search path
			// ::SetDllDirectory only exists since XP SP1 and doesn't let us add more than one path.. it's hence as poor as LOAD_WITH_ALTERED_SEARCH_PATH.
			// So, we simply locally alter the PATH environment variable, it's more sane than wapi.
			{
				boost::filesystem::path path(file_name, boost::filesystem::native);
				// the base name, i.e. just the file name compound of the path
				std::string base_name(path.leaf().string());
				// save the original path env var
				std::string old_path;
				{
					char const * const env(std::getenv(path_env_var_name));
					if(env) old_path = env;
				}
				std::ostringstream new_path;
				// grows the search path with intermediate dirs between the configured root dir for plugins and the dir of this plugin
				{
					// configured root dir for plugins
					boost::filesystem::path root_path(Global::configuration().GetAbsolutePluginDir(), boost::filesystem::native);
					// grow the search path with each compound of the sub dir until we reached the root dir
					// first, normalize the path so we don't have
					// dirs that are not intermediate because of things like foo/../bar
					// or useless repetitions because of things like foo/./bar
					path.normalize();
					// then, loop
					do
					{
						// go to the parent dir (in the first iteration of the loop, it removes the file leaf)
						path = path.branch_path();
						loggers::trace()("path: " + path.string());
						// the following test is necessary in case the user has changed the configured root dir but not rescanned the plugins.
						// the loop would never exit because boost::filesystem::equivalent returns false if any of the directory doesn't exist.
						if(path.empty()) throw exceptions::library_errors::loading_error("Directory does not exits.");
						// appends the intermediate dir to the list of paths
						new_path << path.string() << ";";
					}
					while(!boost::filesystem::equivalent(path, root_path));
					// append the old path value, at the end so it's searched last
					new_path << old_path;
				}
				// set the new path env var
				#if defined DIVERSALIS__OS__MICROSOFT
					if(!::SetEnvironmentVariable(path_env_var_name, new_path.str().c_str())) {
						//int const e(::GetLastError());
						throw exceptions::library_errors::loading_error("Could not alter PATH env var.");
					}
				#else
					///\todo use ::setenv (and not ::putenv!)
					if(::setenv(path_env_var_name, new_path.str().c_str())) {
						int const e(errno);
						throw ...
					}
				#endif
				loggers::trace()(path_env_var_name + (" env var: " + new_path.str()));
				// load the library passing just the base file name and relying on the search path env var
				_dll = ::LoadLibrary(base_name.c_str());
				// set the path env var back to its original value
				#if defined DIVERSALIS__OS__MICROSOFT
					if(!::SetEnvironmentVariable(path_env_var_name, old_path.c_str())) {
						//int const e(::GetLastError());
						throw exceptions::library_errors::loading_error("Could not set PATH env var back to its original value.");
					}
				#else
					///\todo use ::setenv (and not ::putenv!)
					if(::setenv(path_env_var_name, old_path.c_str())) {
						int const e(errno);
						throw ...
					}
				#endif		
			}
			if(!_dll)
			{
				std::ostringstream s; s
					<< "could not load library: " << file_name << std::endl
					<< universalis::os::exceptions::desc();
				throw exceptions::library_errors::loading_error(s.str());
			}
			GETINFO GetInfo = (GETINFO) GetProcAddress(_dll, "GetInfo");
			if(!GetInfo)
			{
				std::ostringstream s; s
					<< "library is not a psycle native plugin:" << std::endl
					<< "could not resolve symbol 'GetInfo' in library: " << file_name << std::endl
					<< universalis::os::exceptions::desc();
				throw exceptions::library_errors::symbol_resolving_error(s.str());
			}
			try
			{
				_pInfo = GetInfo();
			}
			CATCH_WRAP_AND_RETHROW(*this)

			// checks that version is 11 (0x0B) while MI_VERSION is still 0x1N
			// or version is 0xNM being compatible with the current MI_VERSION.
			if(!(_pInfo->APIVersion == 11 && (MI_VERSION&0xFFF0) == 0x0010)
				&& !((_pInfo->APIVersion&0xFFF0) == (MI_VERSION&0xFFF0))) {

				std::ostringstream s;
				s << "plugin version not supported" << _pInfo->APIVersion;
				throw std::runtime_error(s.str());
			}

			_isSynth = _pInfo->Flags == 3;
			if(_isSynth) _mode = MACHMODE_GENERATOR;
			strncpy(_psShortName,_pInfo->ShortName,15);
			_psShortName[15]='\0';
			strncpy(_editName, _pInfo->ShortName,sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';
			_psAuthor = _pInfo->Author;
			_psName = _pInfo->Name;
			_psDllName = file_name;
			CREATEMACHINE GetInterface = (CREATEMACHINE) GetProcAddress(_dll, "CreateMachine");
			if(!GetInterface)
			{
				std::ostringstream s; s
					<< "could not resolve symbol 'CreateMachine' in library: " << file_name << std::endl
					<< universalis::os::exceptions::desc();
				throw exceptions::library_errors::symbol_resolving_error(s.str());
			}
			try
			{
				proxy()(GetInterface());
			}
			CATCH_WRAP_AND_RETHROW(*this)
#else
			auto it = std::find_if(psycle::plugin_interface::PluginFactory::Get().cbegin(), psycle::plugin_interface::PluginFactory::Get().cend(), [&](auto& p)
			{
				return _stricmp(p.m_dllName.c_str(), file_name.c_str()) == 0;
			});
			if (it != psycle::plugin_interface::PluginFactory::Get().cend())
			{
                _pInfo = (psycle::plugin_interface::CMachineInfo*)it->m_getInfo();

                _isSynth = _pInfo->Flags == 3;
                if (_isSynth) _mode = MACHMODE_GENERATOR;
                strncpy(_psShortName, _pInfo->ShortName, 15);
                _psShortName[15] = '\0';
                strncpy(_editName, _pInfo->ShortName, sizeof(_editName) - 1);
                _editName[sizeof(_editName) - 1] = '\0';
                _psAuthor = _pInfo->Author;
                _psName = _pInfo->Name;
                _psDllName = file_name;
                proxy()(it->m_createMachine());
			}
#endif
		}

		void Plugin::Init()
		{
			Machine::Init();
			if(proxy()()) 
			{
				try
				{
					proxy().Init();
					needsAux_ = proxy().HostEvent(plugin_interface::HE_NEEDS_AUX_COLUMN,0,0.0f);

				}
				catch(const std::exception &e)
				{
					e;
#ifndef NDEBUG 
					throw e;
#endif
				}
				for(int gbp(0) ; gbp < GetInfo()->numParameters ; ++gbp)
				{
					try
					{
						proxy().ParameterTweak(gbp, _pInfo->Parameters[gbp]->DefValue);
					}
					catch(const std::exception &e)
					{
						e;
#ifndef NDEBUG 
						throw e;
#endif
					}
				}
			}
		}

		bool Plugin::LoadDll(std::string psFileName)
		{
			std::transform(psFileName.begin(), psFileName.end(), psFileName.begin(), [](auto a) { return std::tolower(a); }); // rePlayer
			std::string sPath2;
			std::string sPath;
			int shellIdx=0;

			if(!Global::machineload().lookupDllName(psFileName,sPath,MACH_PLUGIN,shellIdx)) 
			{
				// Check Compatibility Table.
				// Probably could be done with the dllNames lookup.
				//GetCompatible(psFileName,sPath2) // If no one found, it will return a null string.
				if (psFileName == "blitz.dll")
				{
					if(Global::configuration().LoadsNewBlitz()) {
						psFileName = "blitzn.dll";
					}
					else {
						psFileName = "blitz12.dll";
					}
				}
				else if (psFileName == "gamefx.dll" )
				{
					if(Global::configuration().LoadsNewBlitz()) {
						psFileName = "gamefxn.dll";
					}
					else {
						psFileName = "gamefx13.dll";
					}
				}
				if(!Global::machineload().lookupDllName(psFileName,sPath,MACH_PLUGIN,shellIdx)) {
#if !defined WINAMP_PLUGIN
				std::ostringstream s; s
					<< "The plugin could not be found :" << psFileName << std::endl
					<< "Replacing with dummy.";
				Global().pLogCallback(s.str().c_str(), "Loading Error");			
#endif
					return false;
				}
			}

			if(!Global::machineload().TestFilename(sPath,shellIdx) ) 
			{
#if !defined WINAMP_PLUGIN
				std::ostringstream s; s
					<< "This plugin is Disabled :" << sPath << std::endl
					<< "Replacing with dummy.";
				Global().pLogCallback(s.str().c_str(), "Loading Error");
#endif
				return false;
			}
			try
			{
				Instance(sPath.c_str());
			}
			catch(const std::exception & e)
			{
				e;
#ifndef NDEBUG 
					throw e;
#elif !defined WINAMP_PLUGIN

				std::ostringstream s; s
					<< "Exception while instanciating: " << sPath << std::endl
					<< "Replacing with dummy." << std::endl
					<< typeid(e).name() << std::endl
					<< e.what();
				Global().pLogCallback(s.str().c_str(), "Loading Error");
#endif
				return false;
			}
			catch(...)
			{
#ifndef NDEBUG 
					throw;
#elif !defined WINAMP_PLUGIN
				std::ostringstream s; s
					<< "Exception while instanciating: " << sPath2 << std::endl
					<< "Replacing with dummy." << std::endl
					<< "Unkown type of exception";
				Global().pLogCallback(s.str().c_str(), "Loading Error");
#endif
					return false;
			}
			return true;
		}

		void Plugin::Free() throw(...)
		{
			const std::exception * exception(0);
			try
			{
				proxy()(0);
			}
			catch(const std::exception & e)
			{
				if(!exception) exception = &e;
			}
			if(_dll)
			{
				::FreeLibrary(_dll);
				_dll = 0;
			}
			if(exception) throw *exception;
		}

		bool Plugin::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			UINT size;
			pFile->Read(&size,sizeof(size)); // size of whole structure
			if(size)
			{
				UINT count;
				pFile->Read(&count,sizeof(count));  // size of vars
				for (UINT i = 0; i < count; i++)
				{
					int temp;
					pFile->Read(&temp,sizeof(temp));
					SetParameter(i,temp);
				}
				size -= sizeof(count) + sizeof(int)*count;
				if(size)
				{
					UINT size2=0;
					try	{ size2 = proxy().GetDataSize();} catch(...) {}
					//This way we guarantee that the plugin will have enough bytes, even if it does not fit what it reads.
					byte* pData = new byte[std::max(size,size2)];
					pFile->Read(pData, size); // Read internal data.
					try
					{
						proxy().PutData(pData); // Internal load
					}
					catch(const std::exception &e)
					{
#ifndef NDEBUG 
						delete[] pData;
						throw e;
						return false;
#else
						e;
						delete[] pData;
						return false;
#endif
					}
					delete[] pData;
					return true;
				}
			}
			return true;
		}

		void Plugin::SaveSpecificChunk(RiffFile* pFile)
		{
			UINT count = GetNumParams();
			UINT size2(0);
			try
			{
				size2 = proxy().GetDataSize();
			}
			catch(const std::exception &e)
			{
				e;
#ifndef NDEBUG 
					throw e;
#endif
				// data won't be saved
			}
			UINT size = size2 + sizeof(count) + sizeof(int)*count;
			pFile->Write(&size,sizeof(size));
			pFile->Write(&count,sizeof(count));
			for (UINT i = 0; i < count; i++)
			{
				int temp = GetParamValue(i);
				pFile->Write(&temp,sizeof temp);
			}
			if(size2)
			{
				byte * pData = new byte[size2];
				try
				{
					proxy().GetData(pData); // Internal save
				}
				catch(const std::exception &e)
				{
					e;
					// this sucks because we already wrote the size,
					// so now we have to write the data, even if they are corrupted.
#ifndef NDEBUG 
					throw e;
#endif

				}
				pFile->Write(pData, size2); //data chunk
				zapArray(pData);
			}
		}

		int Plugin::GenerateAudioInTicks( int /*startSample*/, int numSamples )
		{
			if(_mode == MACHMODE_GENERATOR) {
				Standby(false);
			}
			if (!_mute) 
			{
				if ((_mode == MACHMODE_GENERATOR) || (!Bypass() && !Standby()))
				{
					int ns = numSamples;
					int us = 0;
					while (ns)
					{
						int nextevent = (TWSActive)?TWSSamples:ns+1;
						for (int i=0; i < Global::song().SONGTRACKS; i++)
						{
							if (TriggerDelay[i]._cmd)
							{
								if (TriggerDelayCounter[i] < nextevent)
								{
									nextevent = TriggerDelayCounter[i];
								}
							}
						}
						if (nextevent > ns)
						{
							if (TWSActive)
							{
								TWSSamples -= ns;
							}
							for (int i=0; i < Global::song().SONGTRACKS; i++)
							{
								// come back to this
								if (TriggerDelay[i]._cmd)
								{
									TriggerDelayCounter[i] -= ns;
								}
							}
							try
							{
								//todo: this should change if we implement multi-io for native plugins (complicated right now. needs new API calls)
								proxy().Work(samplesV[0]+us, samplesV[1]+us, ns, Global::song().SONGTRACKS);
							}
							catch(const std::exception & e)
							{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
							}
							ns = 0;
						}
						else
						{
							if(nextevent)
							{
								ns -= nextevent;
								try
								{
									//todo: this should change if we implement multi-io for native plugins (complicated right now. needs new API calls)
									proxy().Work(samplesV[0]+us, samplesV[1]+us, nextevent, Global::song().SONGTRACKS);
								}
								catch(const std::exception &e)
								{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
								}
								us += nextevent;
							}
							if (TWSActive)
							{
								if (TWSSamples == nextevent)
								{
									int activecount = 0;
									TWSSamples = TWEAK_SLIDE_SAMPLES;
									for (int i = 0; i < MAX_TWS; i++)
									{
										if (TWSDelta[i] != 0)
										{
											TWSCurrent[i] += TWSDelta[i];

											if (((TWSDelta[i] > 0) && (TWSCurrent[i] >= TWSDestination[i]))
												|| ((TWSDelta[i] < 0) && (TWSCurrent[i] <= TWSDestination[i])))
											{
												TWSCurrent[i] = TWSDestination[i];
												TWSDelta[i] = 0;
											}
											else
											{
												activecount++;
											}
											try
											{
												proxy().ParameterTweak(TWSInst[i], int(TWSCurrent[i]));
											}
											catch(const std::exception &e)
											{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
											}
										}
									}
									if(!activecount) TWSActive = false;
								}
							}
							for (int i=0; i < Global::song().SONGTRACKS; i++)
							{
								// come back to this
								if (TriggerDelayCounter[i] == nextevent)
								{
									if (TriggerDelay[i]._cmd == PatternCmd::NOTE_DELAY)
									{
										// do event
										try
										{
											proxy().SeqTick(i ,TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
										}
										catch(const std::exception &e)
										{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
										}
										TriggerDelay[i]._cmd = 0;
									}
									else if (TriggerDelay[i]._cmd == PatternCmd::RETRIGGER)
									{
										// do event
										try
										{
											proxy().SeqTick(i, TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
										}
										catch(const std::exception &e)
										{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
										}
										TriggerDelayCounter[i] = (RetriggerRate[i]*Global::player().SamplesPerRow())/256;
									}
									else if (TriggerDelay[i]._cmd == PatternCmd::RETR_CONT)
									{
										// do event
										try
										{
											proxy().SeqTick(i ,TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
										}
										catch(const std::exception &e)
										{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
										}
										TriggerDelayCounter[i] = (RetriggerRate[i]*Global::player().SamplesPerRow())/256;
										int parameter = TriggerDelay[i]._parameter&0x0f;
										if (parameter < 9)
										{
											RetriggerRate[i]+= 4*parameter;
										}
										else
										{
											RetriggerRate[i]-= 2*(16-parameter);
											if (RetriggerRate[i] < 16)
											{
												RetriggerRate[i] = 16;
											}
										}
									}
									else if (TriggerDelay[i]._cmd == PatternCmd::ARPEGGIO)
									{
										PatternEntry entry =TriggerDelay[i];
										switch(ArpeggioCount[i])
										{
										case 0: 
											try
											{
												proxy().SeqTick(i ,TriggerDelay[i]._note, TriggerDelay[i]._inst, 0, 0);
											}
											catch(const std::exception &e)
											{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
											}
											ArpeggioCount[i]++;
											break;
										case 1:
											entry._note+=((TriggerDelay[i]._parameter&0xF0)>>4);
											try
											{
												proxy().SeqTick(i ,entry._note, entry._inst, 0, 0);
											}
											catch(const std::exception &e)
											{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
											}
											ArpeggioCount[i]++;
											break;
										case 2:
											entry._note+=(TriggerDelay[i]._parameter&0x0F);
											try
											{
												proxy().SeqTick(i ,entry._note, entry._inst, 0, 0);
											}
											catch(const std::exception &e)
											{
#ifndef NDEBUG 
					throw e;
					return 0;
#else
					e;
					return 0;
#endif
											}
											ArpeggioCount[i]=0;
											break;
										}
										TriggerDelayCounter[i] = Global::player().SamplesPerTick();
									}
								}
								else if (TriggerDelay[i]._cmd != 0)
								{
									TriggerDelayCounter[i] -= nextevent;
								}
							}
						}
					}
					UpdateVuAndStanbyFlag(numSamples);
				}
			}
			else Standby(true);

			return numSamples;
		}

		bool Plugin::SetParameter(int numparam,int value)
		{
			if(numparam < _pInfo->numParameters && value >= _pInfo->Parameters[numparam]->MinValue
				&& value <= _pInfo->Parameters[numparam]->MaxValue)
			{
				try
				{
					proxy().ParameterTweak(numparam,value);
				}
				catch(const std::exception &e)
				{
#ifndef NDEBUG 
					throw e;
					return false;
#else
					e;
					return false;
#endif
				}
				return true;
			}
			else return false;
		}

		int Plugin::GetParamType(int numparam)
		{
			if( numparam < _pInfo->numParameters ) return _pInfo->Parameters[numparam]->Flags;
			else return 0;
		}
		void Plugin::GetParamName(int numparam, char * name)
		{
			if( numparam < _pInfo->numParameters ) std::strcpy(name,_pInfo->Parameters[numparam]->Name);
			else std::strcpy(name, "Out of Range");

		}
		void Plugin::GetParamRange(int numparam,int &minval,int &maxval)
		{
			if( numparam < _pInfo->numParameters )
			{
				if(GetInfo()->Parameters[numparam]->Flags & MPF_STATE)
				{
					minval = GetInfo()->Parameters[numparam]->MinValue;
					maxval = GetInfo()->Parameters[numparam]->MaxValue;
				}
				else
				{
					minval = maxval = 0;
				}
			}
			else
			{
				minval = maxval = 0;
			}
		}
		int Plugin::GetParamValue(int numparam)
		{
			if(numparam < _pInfo->numParameters)
			{
				try
				{
					return proxy().Val(numparam);
				}
				catch(const std::exception &e)
				{
#ifndef NDEBUG 
					throw e;
					return -1;
#else
					e;
					return -1; // hmm
#endif
				}
			}
			else return -1; // hmm
		}

		void Plugin::GetParamValue(int numparam, char * parval)
		{
			if(numparam < _pInfo->numParameters)
			{
				try
				{
					if(!proxy().DescribeValue(parval, numparam, proxy().Val(numparam)))
						std::sprintf(parval, "%i", proxy().Val(numparam));
				}
				catch(const std::exception &e)
				{
#ifndef NDEBUG 
					throw e;
					return;
#else
					e;
					return;
#endif
				}
			}
			else std::strcpy(parval,"Out of Range");
		}

		void Plugin::Stop()
		{
			try
			{
				proxy().Stop();
			}
			catch(const std::exception &e)
			{
#ifndef NDEBUG 
				throw e;
				return;
#else
				e;
				return;
#endif
			}
		}
		void Plugin::SetSampleRate(int sr)
		{
			Machine::SetSampleRate(sr);
			NewLine();
		}

		void Plugin::NewLine()
		{
			try
			{
				proxy().SequencerTick();
			}
			catch(const std::exception &e)
			{
#ifndef NDEBUG 
				throw e;
				return;
#else
				e;
				return;
#endif
			}
		}

		void Plugin::Tick(int channel, PatternEntry * pData)
		{
			if(pData->_note == notecommands::tweak || pData->_note == notecommands::tweakeffect)
			{
				int param = translate_param(pData->_inst);
				if(param < _pInfo->numParameters)
				{
					int nv = (pData->_cmd<<8)+pData->_parameter;
					int const min = _pInfo->Parameters[param]->MinValue;
					int const max = _pInfo->Parameters[param]->MaxValue;
					nv += min;
					if(nv > max) nv = max;
					try
					{
						proxy().ParameterTweak(param, nv);
					}
					catch(const std::exception &e)
					{
#ifndef NDEBUG 
					throw e;
					return;
#else
					e;
					return;
#endif
					}
				}
			}
			else if(pData->_note == notecommands::tweakslide)
			{
				int param = translate_param(pData->_inst);
				if(param < _pInfo->numParameters)
				{
					int i;
					if(TWSActive)
					{
						// see if a tweak slide for this parameter is already happening
						for(i = 0; i < MAX_TWS; i++)
						{
							if((TWSInst[i] == param) && (TWSDelta[i] != 0))
							{
								// yes
								break;
							}
						}
						if(i == MAX_TWS)
						{
							// nope, find an empty slot
							for (i = 0; i < MAX_TWS; i++)
							{
								if (TWSDelta[i] == 0)
								{
									break;
								}
							}
						}
					}
					else
					{
						// wipe our array for safety
						for (i = MAX_TWS-1; i > 0; i--)
						{
							TWSDelta[i] = 0;
						}
					}
					if (i < MAX_TWS)
					{
						TWSDestination[i] = float(pData->_cmd<<8)+pData->_parameter;
						float min = float(_pInfo->Parameters[param]->MinValue);
						float max = float(_pInfo->Parameters[param]->MaxValue);
						TWSDestination[i] += min;
						if (TWSDestination[i] > max)
						{
							TWSDestination[i] = max;
						}
						TWSInst[i] = param;
						try
						{
							TWSCurrent[i] = float(proxy().Val(TWSInst[i]));
						}
						catch(const std::exception &e)
						{
#ifndef NDEBUG 
					throw e;
					return;
#else
					e;
					return;
#endif
						}
						TWSDelta[i] = float((TWSDestination[i]-TWSCurrent[i])*TWEAK_SLIDE_SAMPLES)/Global::player().SamplesPerRow();
						TWSSamples = 0;
						TWSActive = TRUE;
					}
					else
					{
						// we have used all our slots, just send a twk
						int nv = (pData->_cmd<<8)+pData->_parameter;
						int const min = _pInfo->Parameters[param]->MinValue;
						int const max = _pInfo->Parameters[param]->MaxValue;
						nv += min;
						if (nv > max) nv = max;
						try
						{
							proxy().ParameterTweak(param, nv);
						}
						catch(const std::exception &e)
						{
#ifndef NDEBUG 
					throw e;
					return;
#else
					e;
					return;
#endif
						}
					}
				}
			}
			else if (pData->_note == notecommands::midicc && pData->_inst >= 0x80 && pData->_inst < 0xFF) {
				try
				{
					proxy().MidiEvent(pData->_inst&0x0F, pData->_inst&0xF0, (pData->_cmd<<8) + pData->_parameter);
				}
				catch(const std::exception &e)
				{
#ifndef NDEBUG 
					throw e;
					return;
#else
					e;
					return;
#endif
				}
			}
			else {
				try
				{
					if(pData->_note==notecommands::midicc) {
						pData->_note=notecommands::empty;
						pData->_inst=0xFF;
					}
					proxy().SeqTick(channel, pData->_note, pData->_inst, pData->_cmd, pData->_parameter);
				}
				catch(const std::exception &e)
				{
#ifndef NDEBUG 
					throw e;
					return;
#else
					e;
					return;
#endif
				}
			}
		}
		void Plugin::GetCurrentPreset(CPreset & preset)
		{
			void* pData = NULL;
			int numParameters = GetNumParams();
			try
			{
				int dataSizeStruct = proxy().GetDataSize();
				if(dataSizeStruct > 0)
				{
					pData = new char[dataSizeStruct];
					proxy().GetData(pData); // Internal Save
					preset.Init(numParameters , "", proxy().Vals(), dataSizeStruct, pData);
					delete[] pData;
				}
				else
				{
					Machine::GetCurrentPreset(preset);
				}
			}
			catch(const std::exception &e)
			{
#ifndef NDEBUG 
				throw e;
				delete[] pData;
				preset.Init(numParameters);
#else
				e;
				delete[] pData;
				preset.Init(numParameters);
#endif
			}
		}
		void Plugin::Tweak(CPreset const & preset)
		{
			Machine::Tweak(preset);
			CExclusiveLock lock(&Global::song().semaphore, 2, true);
			if(preset.GetData())
			{
				try
				{
					proxy().PutData(preset.GetData()); // Internal save
				}
				catch(const std::exception &e)
				{
					e;
#ifndef NDEBUG 
					throw e;
#endif
					// o_O`
				}
				catch(...) // reinterpret_cast sucks
				{
#ifndef NDEBUG 
					throw;
#endif
					// o_O`
				}
			}
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		/// old file format vomit. don't look at it.



		/// old file format vomit. don't look at it.
		bool Plugin::Load(RiffFile* pFile)
		{
			int numParameters;
			pFile->Read(&_editName,16);
			_editName[15] = 0;
			pFile->Read(&numParameters, sizeof(numParameters));
			int *Vals = new int[numParameters];
			pFile->Read(Vals, numParameters*sizeof(int));
			try
			{
				 for (int i=0; i<numParameters; i++)
				{
					proxy().ParameterTweak(i,Vals[i]);
				}
			}
			catch(const std::exception &)
			{
			}
			try
			{
				int size = proxy().GetDataSize();
				//pFile->Read(&size,sizeof(int));	// This SHOULD be the right thing to do
				if(size)
				{
					byte* pData = new byte[size];
					pFile->Read(pData, size); // Number of parameters
					try
					{
						proxy().PutData(pData); // Internal load
					}
					catch(const std::exception &)
					{
					}
					zapArray(pData);
				}
			}
			catch(const std::exception &)
			{
			}
			zapArray(Vals);

			legacyWires.resize(MAX_CONNECTIONS);
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputMachine,sizeof(legacyWires[i]._inputMachine));	// Incoming connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._outputMachine,sizeof(legacyWires[i]._outputMachine));	// Outgoing connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputConVol,sizeof(legacyWires[i]._inputConVol));	// Incoming connections Machine vol
				legacyWires[i]._wireMultiplier = 1.0f;
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._connection,sizeof(legacyWires[i]._connection));      // Outgoing connections activated
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputCon,sizeof(legacyWires[i]._inputCon));		// Incoming connections activated
			}
			pFile->Read(&_connectionPoint[0], sizeof(_connectionPoint));
			pFile->Skip(2*sizeof(int)); // numInputs and numOutputs

			pFile->Read(&_panning, sizeof(_panning));
			Machine::SetPan(_panning);
			pFile->Skip(109);

			return true;
		}

		void Plugin::SkipLoad(RiffFile* pFile)
		{
			int numParameters;
			pFile->Read(&_editName,16);
			_editName[15] = 0;
			pFile->Read(&numParameters, sizeof(numParameters));
			pFile->Skip(numParameters*sizeof(int));
			/* This SHOULD be done, but it breaks the fileformat.
			int size;
			pFile->Read(&size,sizeof(int));
			if (size)
			{
				pFile->Skip(size);
			}*/

			legacyWires.resize(MAX_CONNECTIONS);
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputMachine,sizeof(legacyWires[i]._inputMachine));	// Incoming connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._outputMachine,sizeof(legacyWires[i]._outputMachine));	// Outgoing connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputConVol,sizeof(legacyWires[i]._inputConVol));	// Incoming connections Machine vol
				legacyWires[i]._wireMultiplier = 1.0f;
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._connection,sizeof(legacyWires[i]._connection));      // Outgoing connections activated
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputCon,sizeof(legacyWires[i]._inputCon));		// Incoming connections activated
			}
			pFile->Read(&_connectionPoint[0], sizeof(_connectionPoint));
			pFile->Skip(2*sizeof(int)); // numInputs and numOutputs

			pFile->Read(&_panning, sizeof(_panning));
			Machine::SetPan(_panning);
			pFile->Skip(109);
		}
	}
}
