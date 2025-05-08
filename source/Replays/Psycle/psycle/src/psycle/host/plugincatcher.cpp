
#include <psycle/host/detail/project.private.hpp>
#include "plugincatcher.hpp"
#if !defined WINAMP_PLUGIN
	#include "ProgressDialog.hpp"
#else
	#include "player_plugins/winamp/fake_progressDialog.hpp"
#endif //!defined WINAMP_PLUGIN

#include <psycle/host/Plugin.hpp>
#include <psycle/host/VstHost24.hpp>
#include <psycle/host/LuaPlugin.hpp>
#include <psycle/host/LuaHost.hpp>
#include <psycle/host/LadspaHost.hpp>

#include <string>
#include <sstream>
#include <fstream>
#include <algorithm> // for std::transform
#include <cctype> // for std::tolower
#include <direct.h>
#include <filesystem>

namespace psycle
{
	namespace host
	{
		PluginCatcher::PluginCatcher()
			:_numPlugins(-1)
		{}
		PluginCatcher::~PluginCatcher() {
			DestroyPluginInfo();
		}

		bool PluginCatcher::lookupDllName(const std::string& name, std::string & result, MachineType type, int32_t& shellidx)
		{
			std::string tmp = name;
			std::size_t dotpos = name.find_last_of(".");
			if ( dotpos == std::string::npos ) dotpos = 0;
			std::string extension = name.substr(dotpos,4);
			std::string extlower = extension;
			std::transform(extlower.begin(), extlower.end(), extlower.begin(), [](auto a) { return std::tolower(a); }); // rePlayer
			if ( extlower != 
				#if defined DIVERSALIS__OS__MICROSOFT
					".dll"
				#elif defined DIVERSALIS__OS__APPLE
					".dylib"
				#else
					".so"
				#endif
				&& extlower != ".lua")
			{
				shellidx =  extension[0] + extension[1]*256 + extension[2]*65536 + extension[3]*16777216;
				tmp = name.substr(0,name.length()-4);
			}
			else shellidx = 0;

			tmp = preprocessName(tmp);

			switch(type)
			{
			case MACH_PLUGIN:
				{
					std::map<std::string,std::string>::iterator iterator = NativeNames.find(tmp);
					if(iterator != NativeNames.end())
					{
						result=iterator->second;
						return true;
					}
					break;
				}
			case MACH_VST:
			case MACH_VSTFX:
				{
					std::map<std::string,std::string>::iterator iterator = VstNames.find(tmp);
					if(iterator != VstNames.end())
					{
						result=iterator->second;
						return true;
					}
					break;
				}
			case MACH_LUA:
				{
					std::map<std::string,std::string>::iterator iterator = LuaNames.find(tmp);
					if(iterator != LuaNames.end())
					{
						result=iterator->second;
						return true;
					}
					break;
				}
				break;
			case MACH_LADSPA:
				{
					std::map<std::string,std::string>::iterator iterator = LadspaNames.find(tmp);
					if(iterator != LadspaNames.end())
					{
						result=iterator->second;
						return true;
					}
					break;
				}
				break;
			default:
				break;
			}
			return false;
		}


		PluginInfo* PluginCatcher::info(const std::string& name) {
			std::string result;
			std::map<std::string,std::string>::iterator iterator = VstNames.find(name);
            if(iterator != VstNames.end()) {
			  result=iterator->second;
			} else {
				std::map<std::string,std::string>::iterator iterator = LuaNames.find(name);
				if(iterator != LuaNames.end()) {
				  result=iterator->second;				  
				} else {
					std::map<std::string,std::string>::iterator iterator = NativeNames.find(name);
					if(iterator != NativeNames.end()) {
						result=iterator->second;
					} else {
					  std::map<std::string,std::string>::iterator iterator = LadspaNames.find(name);
					  if(iterator != LadspaNames.end()) {
						result=iterator->second;
					  }				
				   }
				}
			}

			int shellIdx = 0;
			for(int i(0) ; i < _numPlugins ; ++i)
			{
				PluginInfo * pInfo = _pPlugsInfo[i];
				if ((result == pInfo->dllname) &&
					(shellIdx == 0 || shellIdx == pInfo->identifier))
				{
					// bad plugins always have allow = false
					if(pInfo->allow) return pInfo;
					std::ostringstream s; s
						<< "Plugin " << name << " is disabled because:" << std::endl
						<< pInfo->error << std::endl
						<< "Try to load anyway?";
					if (::MessageBox(0,s.str().c_str(), "Plugin Warning!", MB_YESNO | MB_ICONWARNING) == IDYES) {
						return pInfo;
					}
					else {
						return 0;
					}
				}
			}
			return 0;
		}

    PluginInfoList PluginCatcher::GetLuaExtensions() {
      PluginInfoList list;
      std::map<std::string,std::string>::iterator iterator = LuaNames.begin();
		  for (;iterator != LuaNames.end();++iterator) {
        std::string result = iterator->second;
        int shellIdx = 0;
			  for(int i(0) ; i < _numPlugins ; ++i)
			  {
				  PluginInfo * pInfo = _pPlugsInfo[i];
				  if ((result == pInfo->dllname) &&
					  (shellIdx == 0 || shellIdx == pInfo->identifier))
				  {
				   	// bad plugins always have allow = false
            if(pInfo->allow && pInfo->mode == MACHMODE_HOST) list.push_back(pInfo);
					/*std::ostringstream s; s
						<< "Plugin " << name << " is disabled because:" << std::endl
						<< pInfo->error << std::endl
						<< "Try to load anyway?";
					if (::MessageBox(0,s.str().c_str(), "Plugin Warning!", MB_YESNO | MB_ICONWARNING) == IDYES) {
						return pInfo;
					}
					else {
						return 0;
					}*/
				  }
        }
      }
      return list;
    }

		bool PluginCatcher::TestFilename(const std::string & name, const int32_t shellIdx)
		{
			for(int i(0) ; i < _numPlugins ; ++i)
			{
				PluginInfo * pInfo = _pPlugsInfo[i];
				if ((name == pInfo->dllname) &&
					(shellIdx == 0 || shellIdx == pInfo->identifier))
				{
					// bad plugins always have allow = false
					if(pInfo->allow) return true;
					std::ostringstream s; s
						<< "Plugin " << name << " is disabled because:" << std::endl
						<< pInfo->error << std::endl
						<< "Try to load anyway?";
					return ::MessageBox(0,s.str().c_str(), "Plugin Warning!", MB_YESNO | MB_ICONWARNING) == IDYES;
				}
			}
			return false;
		}

		void PluginCatcher::LoadPluginInfo(bool verify)
		{
			if(_numPlugins == -1)
			{
				loggers::information()("Scanning plugins ...");

				//DWORD dwThreadId;
				LoadPluginInfoParams params;
				params.verify = verify;
				params.theCatcher = this;
				//This almost works.. except for the fact that when closing the progress dialog, the focus is lost.
				//CreateThread( NULL, 0, ProcessLoadPlugInfo, &params, 0, &dwThreadId );
				//CSingleLock event(&params.theEvent, TRUE);
				ProcessLoadPlugInfo(&params);

				loggers::information()("Saving scan cache file ...");
				SaveCacheFile();
				loggers::information()("Done.");
			}
		}

		void PluginCatcher::ReScan(bool regenerate)
		{
			DestroyPluginInfo();

			if (regenerate) {
/*
#if defined _WIN64
			DeleteFile((Global::configuration().cacheDir() / "psycle64.plugin-scan.cache").string().c_str());
#elif defined _WIN32
			DeleteFile((Global::configuration().cacheDir() / "psycle.plugin-scan.cache").string().c_str());
#else
#error unexpected platform
#endif
*/
			}
			LoadPluginInfo();
		}

		DWORD PluginCatcher::ProcessLoadPlugInfo(void* pParam ) {
			LoadPluginInfoParams* param = (LoadPluginInfoParams*)pParam;
			int plugsCount(0);
			int badPlugsCount(0);
			param->theCatcher->_numPlugins = 0;
			bool cacheValid = param->theCatcher->LoadCacheFile(plugsCount, badPlugsCount, param->verify);

			class populate_plugin_list
			{
			public:
				populate_plugin_list(std::vector<std::string> & result, std::string directory, bool recursive = true)
				{
                    std::error_code ec;
					if (recursive) for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(directory, ec))
						Add(result, dir_entry.path());
					else for (const std::filesystem::directory_entry& dir_entry : std::filesystem::directory_iterator(directory, ec))
                        Add(result, dir_entry.path());

/*
					::CFileFind finder;
					int loop = finder.FindFile(::CString((directory + "\\*").c_str()));
					while(loop)
					{
						loop = finder.FindNextFile();
						if(recursive && finder.IsDirectory()) {
							if(!finder.IsDots())
							{
								std::string sfilePath = finder.GetFilePath();
								populate_plugin_list(result,sfilePath);
							}
						}
						else
						{
							CString filePath=finder.GetFilePath();
							std::string sfilePath = filePath;
							filePath.MakeLower();

							if(filePath.Right(4) == 
								#if defined DIVERSALIS__OS__MICROSOFT
									".dll"
								#elif defined DIVERSALIS__OS__APPLE
									".dylib"
								#else
									".so"
								#endif
								|| filePath.Right(4) == ".lua")
							{
								result.push_back(sfilePath);
							}
						}
					}
					finder.Close();
*/
				}

				static void Add(std::vector<std::string>& result, const std::filesystem::path& filePath)
				{
					auto extension = filePath.extension().u8string();
					if (_stricmp(reinterpret_cast<const char*>(extension.c_str()), ".dll") == 0 || _stricmp(reinterpret_cast<const char*>(extension.c_str()), ".lua") == 0)
						result.push_back(reinterpret_cast<const char*>(filePath.u8string().c_str()));
				}
			};

			std::vector<std::string> nativePlugs;
			std::vector<std::string> vstPlugs;
			std::vector<std::string> luaPlugs;
			std::vector<std::string> ladspaPlugs;

			CProgressDialog progress;
// 			{
// 				char c[1 << 10];
// 				::GetCurrentDirectory(sizeof c, c);
// 				std::string s(c);
// 				loggers::information()("Scanning plugins ... Current Directory: " + s);
// 			}
// 			loggers::information()("Scanning plugins ... Directory for Natives: " + Global::configuration().GetAbsolutePluginDir());
// 			loggers::information()("Scanning plugins ... Directory for VSTs (32): " + Global::configuration().GetAbsoluteVst32Dir());
// 			loggers::information()("Scanning plugins ... Directory for VSTs (64): " + Global::configuration().GetAbsoluteVst64Dir());
// 			loggers::information()("Scanning plugins ... Directory for Luas : " + Global::configuration().GetAbsoluteLuaDir());
// 			loggers::information()("Scanning plugins ... Directory for Ladspas : " + Global::configuration().GetAbsoluteLadspaDir());
// 			loggers::information()("Scanning plugins ... Listing ...");

			if (param->verify) {
// 				progress.SetWindowText("Scanning plugins ... Listing ...");
// 				progress.ShowWindow(SW_SHOW);

                //populate_plugin_list(nativePlugs, Global::configuration().GetAbsolutePluginDir());
#if	defined _WIN64
                populate_plugin_list(vstPlugs, Global::configuration().GetAbsoluteVst64Dir());
                if (Global::configuration().UsesJBridge() || Global::configuration().UsesPsycleVstBridge())
                {
                    populate_plugin_list(vstPlugs, Global::configuration().GetAbsoluteVst32Dir());
                }
#elif defined _WIN32
                populate_plugin_list(vstPlugs, Global::configuration().GetAbsoluteVst32Dir());
                if (Global::configuration().UsesJBridge() || Global::configuration().UsesPsycleVstBridge())
                {
                    populate_plugin_list(vstPlugs, Global::configuration().GetAbsoluteVst64Dir());
                }
#endif
                populate_plugin_list(ladspaPlugs, Global::configuration().GetAbsoluteLadspaDir(), false);
			}
            //Always scan lua dir since it is important that the psycle-specific script are up-to-date.
            populate_plugin_list(luaPlugs, Global::configuration().GetAbsoluteLuaDir(), false);

			int plugin_count = (int)(/*nativePlugs.size()*/plugin_interface::PluginFactory::Get().size() + vstPlugs.size() + luaPlugs.size() + ladspaPlugs.size());

// 			{
// 				std::ostringstream s; s << "Scanning plugins ... Counted " << plugin_count << " plugins.";
// 				loggers::information()(s.str());
// 				progress.m_Progress.SetStep(16384 / std::max(1,plugin_count));
// 				progress.SetWindowText(s.str().c_str());
// 			}
// 			std::ofstream out;
			{
// 				boost::filesystem::path log_dir(Global::configuration().cacheDir());
// 				// note mkdir is posix, not iso, on msvc, it's defined only #if !__STDC__ (in direct.h)
// 				mkdir(log_dir.string().c_str());
// #if defined _WIN64
// 				out.open((log_dir / "psycle64.plugin-scan.log.txt").string().c_str());
// #elif defined _WIN32
// 				out.open((log_dir / "psycle.plugin-scan.log.txt").string().c_str());
// #else
// #error unexpected platform
// #endif
			}
			if (param->verify) {
// 				out
// 					<< "==========================================" << std::endl
// 					<< "=== Psycle Plugin Scan Enumeration Log ===" << std::endl
// 					<< std::endl
// 					<< "If psycle is crashing on load, chances are it's a bad plugin, "
// 					<< "specifically the last item listed, if it has no comment after the library file name." << std::endl;
// 
// 				std::ostringstream s; s << "Scanning " << plugin_count << " plugins ... Testing Natives ...";
// 				progress.SetWindowText(s.str().c_str());

// 				loggers::information()("Scanning plugins ... Testing Natives ...");
// 				out
// 					<< std::endl
// 					<< "======================" << std::endl
// 					<< "=== Native Plugins ===" << std::endl
// 					<< std::endl;
// 				out.flush();

				param->theCatcher->FindPlugins(plugsCount, badPlugsCount, nativePlugs, MACH_PLUGIN/*, out, &progress*/);

// 				out.flush();
// 				{
// 					std::ostringstream s; s << "Scanning " << plugin_count << " plugins ... Testing VSTs ...";
// 					progress.SetWindowText(s.str().c_str());
// 				}

// 				loggers::information()("Scanning plugins ... Testing VSTs ...");
// 				out
// 					<< std::endl
// 					<< "===================" << std::endl
// 					<< "=== VST Plugins ===" << std::endl
// 					<< std::endl;
// 				out.flush();

                param->theCatcher->FindPlugins(plugsCount, badPlugsCount, vstPlugs, MACH_VST/*, out, &progress*/);
            }
//             out.flush();
//             {
//                 std::ostringstream s; s << "Scanning " << plugin_count << " plugins ... Testing Luas ...";
//                 progress.SetWindowText(s.str().c_str());
//             }

//             loggers::information()("Scanning plugins ... Testing Luas ...");
//             out
//                 << std::endl
//                 << "===================" << std::endl
//                 << "=== Lua Scripts ===" << std::endl
//                 << std::endl;
//             out.flush();

            param->theCatcher->FindPlugins(plugsCount, badPlugsCount, luaPlugs, MACH_LUA/*, out, &progress*/);
//             {
//                 std::ostringstream s; s << "Scanned " << plugin_count << " Files." << plugsCount << " scripts found";
//                 out << std::endl << s.str() << std::endl;
//                 out.flush();
//                 loggers::information()(s.str().c_str());
//                 progress.SetWindowText(s.str().c_str());
//             }
            if (param->verify) {
//                 {
//                     std::ostringstream s; s << "Scanning " << plugin_count << " plugins ... Testing LADSPAs ...";
//                     progress.SetWindowText(s.str().c_str());
//                 }

//                 loggers::information()("Scanning plugins ... Testing LADSPAs ...");
//                 out
//                     << std::endl
//                     << "===================" << std::endl
//                     << "=== LADSPA Plugins ===" << std::endl
//                     << std::endl;
//                 out.flush();

                param->theCatcher->FindPlugins(plugsCount, badPlugsCount, ladspaPlugs, MACH_LADSPA/*, out, &progress*/);
            }

// 			out.close();
			param->theCatcher->_numPlugins = plugsCount;
			progress.SendMessage(WM_CLOSE);
			param->theEvent.SetEvent();
			return 0;
		}

	void PluginCatcher::FindPlugins(int & currentPlugsCount, int & currentBadPlugsCount, std::vector<std::string> const & list, MachineType type, /*std::ostream & out, */CProgressDialog * pProgress)
	{
		auto list_size = type == MACH_PLUGIN ? psycle::plugin_interface::PluginFactory::Get().size() : list.size();
		for(unsigned fileIdx=0;fileIdx<list_size;fileIdx++)
		{
			if(pProgress)
			{
				pProgress->m_Progress.StepIt();
// 				::Sleep(1);
			}
			std::string fileName = type == MACH_PLUGIN ? psycle::plugin_interface::PluginFactory::Get()[fileIdx].m_dllName : list[fileIdx];

// 			out << fileName << " ... ";
// 			out.flush();
            bool exists(false);
            FILETIME time;
            ZeroMemory(&time, sizeof FILETIME);
            if (type != MACH_PLUGIN)
            {
                HANDLE hFile = CreateFile(fileName.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    GetFileTime(hFile, 0, 0, &time);
                    CloseHandle(hFile);
                }
                // Verify if the plugin is already in the cache.
                for (int i(0); i < _numPlugins; ++i)
                {
                    PluginInfo* pInfo = _pPlugsInfo[i];
                    if
                        (
                            pInfo->FileTime.dwHighDateTime == time.dwHighDateTime &&
                            pInfo->FileTime.dwLowDateTime == time.dwLowDateTime
                            )
                    {
                        if (pInfo->dllname == fileName)
                        {
                            exists = true;
                            const std::string error(pInfo->error);
                            std::stringstream s;
                            if (error.empty())
                                s << "found in cache.";
                            else
                            {
                                currentBadPlugsCount++;
                                s << "cache says it has previously been disabled because:" << std::endl << error << std::endl;
                            }
// 							out << s.str();
// 							out.flush();
                            loggers::information()(fileName + '\n' + s.str());
                            break;
                        }
                    }
                }
            }
			if(!exists)
			{
				try
				{
// 					out << "new plugin added to cache ; ";
// 					out.flush();
					loggers::information()(fileName + "\nnew plugin added to cache.");
					PluginInfo* pInfo = _pPlugsInfo[currentPlugsCount]= new PluginInfo();
					pInfo->dllname = fileName;
					pInfo->FileTime = time;
					if(type == MACH_PLUGIN)
					{
						pInfo->type = MACH_PLUGIN;
						Plugin plug(0);
						try
						{
							plug.Instance(fileName);
							plug.Init(); // [bohan] hmm, we should get rid of two-stepped constructions.
						}
						catch(const std::exception & e)
						{
							std::ostringstream s; s << typeid(e).name() << std::endl;
							if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
							pInfo->error = s.str();
						}
						catch(...)
						{
							std::ostringstream s; s
								<< "Type of exception is unknown, cannot display any further information." << std::endl;
							pInfo->error = s.str();
						}
						if(!pInfo->error.empty())
						{
// 							out << "### ERRONEOUS ###" << std::endl;
// 							out.flush();
// 							out << pInfo->error;
// 							out.flush();
							std::stringstream title; title
								<< "Machine crashed: " << fileName;
							loggers::exception()(title.str() + '\n' + pInfo->error);
							pInfo->allow = false;
							pInfo->name = "???";
							pInfo->identifier = 0;
							pInfo->vendor = "???";
							pInfo->desc = "???";
							pInfo->version = "???";
							pInfo->APIversion = 0;
							++currentBadPlugsCount;
						}
						else
						{
							pInfo->allow = true;
							pInfo->name = plug.GetName();
							pInfo->identifier = 0;
							pInfo->vendor = plug.GetAuthor();
							if(plug.IsSynth()) pInfo->mode = MACHMODE_GENERATOR;
							else pInfo->mode = MACHMODE_FX;
							{
								std::ostringstream s; s << (plug.IsSynth() ? "Psycle instrument" : "Psycle effect") << " by " << plug.GetAuthor();
								pInfo->desc = s.str();
							}
							pInfo->APIversion = plug.GetAPIVersion();
							{
								std::ostringstream s; s << std::setfill('0') << std::setw(3) << std::hex << plug.GetPlugVersion();
								pInfo->version = s.str();
							}
// 							out << plug.GetName() << " - successfully instanciated";
// 							out.flush();
						}
						learnDllName(fileName,type);
						// [bohan] plug is a stack object, so its destructor is called
							// [bohan] at the end of its scope (this scope actually).
						// [bohan] The problem with destructors of any object of any class is that
						// [bohan] they are never allowed to throw any exception.
						// [bohan] So, we catch exceptions here by calling plug.Free(); explicitly.
						try
						{
							plug.Free();
						}
						catch(const std::exception & e)
						{
							std::stringstream s; s
								<< "Exception occured while trying to free the temporary instance of the plugin." << std::endl
								<< "This plugin will not be disabled, but you might consider it unstable." << std::endl
								<< typeid(e).name() << std::endl;
							if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
// 							out
// 								<< std::endl
// 								<< "### ERRONEOUS ###" << std::endl
// 								<< s.str().c_str();
// 							out.flush();
							std::stringstream title; title
								<< "Machine crashed: " << fileName;
							loggers::exception()(title.str() + '\n' + s.str());
						}
						catch(...)
						{
							std::stringstream s; s
								<< "Exception occured while trying to free the temporary instance of the plugin." << std::endl
								<< "This plugin will not be disabled, but you might consider it unstable." << std::endl
								<< "Type of exception is unknown, no further information available.";
// 							out
// 								<< std::endl
// 								<< "### ERRONEOUS ###" << std::endl
// 								<< s.str().c_str();
// 							out.flush();
							std::stringstream title; title
								<< "Machine crashed: " << fileName;
							loggers::exception()(title.str() + '\n' + s.str());
						}
					}
					else if(type == MACH_VST)
					{
						pInfo->type = MACH_VST;
						vst::Plugin *vstPlug=0;
						try
						{
							vstPlug = dynamic_cast<vst::Plugin*>(Global::vsthost().LoadPlugin(fileName.c_str()));
						}
						catch(const std::runtime_error & e)
						{
							std::ostringstream s; s << typeid(e).name() << std::endl;
							if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
							pInfo->error = s.str();
						}
						//TODO: Warning! This is not std::exception, but universalis::stdlib::exception
						catch(const std::exception & e)
						{
							std::ostringstream s; s << typeid(e).name() << std::endl;
							if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
							pInfo->error = s.str();
						}
						catch(...)
						{
							std::ostringstream s; s << "Type of exception is unknown, cannot display any further information." << std::endl;
							pInfo->error = s.str();
						}
						if(!pInfo->error.empty())
						{
// 							out << "### ERRONEOUS ###" << std::endl;
// 							out.flush();
// 							out << pInfo->error;
// 							out.flush();
							std::stringstream title; title
								<< "Machine crashed: " << fileName;
							loggers::exception()(title.str() + '\n' + pInfo->error);
							pInfo->allow = false;
							pInfo->identifier = 0;
							pInfo->name = "???";
							pInfo->vendor = "???";
							pInfo->desc = "???";
							pInfo->version = "???";
							pInfo->APIversion = 0;
							++currentBadPlugsCount;
							if (vstPlug) delete vstPlug;
						}
						else
						{
							if (vstPlug->IsShellMaster())
							{
								char tempName[64] = {0}; 
								VstInt32 plugUniqueID = 0;
								bool firstrun = true;
								while ((plugUniqueID = vstPlug->GetNextShellPlugin(tempName)) != 0)
								{ 
									if (tempName[0] != 0)
									{
										PluginInfo* pInfo2 = _pPlugsInfo[currentPlugsCount];
										if ( !firstrun )
										{
											++currentPlugsCount;
											pInfo2 = _pPlugsInfo[currentPlugsCount]= new PluginInfo();
											pInfo2->dllname = fileName;
											pInfo2->FileTime = time;
										}

										pInfo2->allow = true;
										{
											std::ostringstream s;
											s << vstPlug->GetVendorName() << " " << tempName;
											pInfo2->name = s.str();
										}
										pInfo2->identifier = plugUniqueID;
										pInfo2->vendor = vstPlug->GetVendorName();
										if(vstPlug->IsSynth()) pInfo2->mode = MACHMODE_GENERATOR;
										else pInfo2->mode = MACHMODE_FX;
										{
											std::ostringstream s;
											s << (vstPlug->IsSynth() ? "VST Shell instrument" : "VST Shell effect") << " by " << vstPlug->GetVendorName();
											pInfo2->desc = s.str();
										}
										{
											std::ostringstream s;
											s << vstPlug->GetPlugVersion();
											pInfo2->version = s.str();
										}
										pInfo2->APIversion = vstPlug->GetVstVersion();
										firstrun=false;
									}
								}
							}
							else
							{
								pInfo->allow = true;
								pInfo->name = vstPlug->GetName();
								pInfo->identifier = vstPlug->uniqueId();
								pInfo->vendor = vstPlug->GetVendorName();
								if(vstPlug->IsSynth()) pInfo->mode = MACHMODE_GENERATOR;
								else pInfo->mode = MACHMODE_FX;
								{
									std::ostringstream s;
									s << (vstPlug->IsSynth() ? "VST instrument" : "VST effect") << " by " << vstPlug->GetVendorName();
									pInfo->desc = s.str();
								}
								{
									std::ostringstream s;
									s << vstPlug->GetPlugVersion();
									pInfo->version = s.str();
								}
								pInfo->APIversion = vstPlug->GetAPIVersion();
							}
// 							out << vstPlug->GetName() << " - successfully instanciated";
// 							out.flush();

							// [bohan] vstPlug is a stack object, so its destructor is called
							// [bohan] at the end of its scope (this cope actually).
							// [bohan] The problem with destructors of any object of any class is that
							// [bohan] they are never allowed to throw any exception.
							// [bohan] So, we catch exceptions here by calling vstPlug.Free(); explicitly.
							try
							{
								vstPlug->Free();
								delete vstPlug;
								// [bohan] phatmatik crashes here...
								// <magnus> so does PSP Easyverb, in FreeLibrary
							}
							catch(const std::exception & e)
							{
								std::stringstream s; s
									<< "Exception occured while trying to free the temporary instance of the plugin." << std::endl
									<< "This plugin will not be disabled, but you might consider it unstable." << std::endl
									<< typeid(e).name() << std::endl;
								if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
// 								out
// 									<< std::endl
// 									<< "### ERRONEOUS ###" << std::endl
// 									<< s.str().c_str();
// 								out.flush();
								std::stringstream title; title
									<< "Machine crashed: " << fileName;
								loggers::exception()(title.str() + '\n' + s.str());
							}
							catch(...)
							{
								std::stringstream s; s
									<< "Exception occured while trying to free the temporary instance of the plugin." << std::endl
									<< "This plugin will not be disabled, but you might consider it unstable." << std::endl
									<< "Type of exception is unknown, no further information available.";
// 								out
// 									<< std::endl
// 									<< "### ERRONEOUS ###" << std::endl
// 									<< s.str().c_str();
// 								out.flush();
								std::stringstream title; title
									<< "Machine crashed: " << fileName;
								loggers::exception()(title.str() + '\n' + s.str());
							}
						}
						learnDllName(fileName,type);
					}
					else if(type == MACH_LUA)
					{
						PluginInfo info;
						try {
							info = LuaGlobal::LoadInfo(fileName.c_str());
						}
						catch(const std::exception & e) {
							std::ostringstream s; s << typeid(e).name() << std::endl;
							if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
							pInfo->error = s.str();
						}
						catch(...) {
							std::ostringstream s; s
								<< "Type of exception is unknown, cannot display any further information." << std::endl;
							pInfo->error = s.str();
						}
						if(!pInfo->error.empty()) {
// 							out << "### ERRONEOUS ###" << std::endl;
// 							out.flush();
// 							out << pInfo->error;
// 							out.flush();
							std::stringstream title; title
								<< "Machine crashed: " << fileName;
							loggers::exception()(title.str() + '\n' + pInfo->error);
							pInfo->allow = false;
							pInfo->name = "???";
							pInfo->identifier = 0;
							pInfo->vendor = "???";
							pInfo->desc = "???";
							pInfo->version = "???";
							pInfo->APIversion = 0;
							++currentBadPlugsCount;
						} else {
							*pInfo = info;	
							pInfo->dllname = fileName;
							pInfo->FileTime = time;
// 							out << info.name << " - successfully instanciated";
// 							out.flush();
						}
						learnDllName(fileName,type);
					} else if(type == MACH_LADSPA)
					{
						std::vector<PluginInfo> infos;
						try {							
							infos = LadspaHost::LoadInfo(fileName.c_str());	
							if (infos.size() == 0) {
								throw std::exception("empty plugin");
							}
						}
						catch(const std::exception & e) {
							std::ostringstream s; s << typeid(e).name() << std::endl;
							if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
							pInfo->error = s.str();
						}
						catch(...) {
							std::ostringstream s; s
								<< "Type of exception is unknown, cannot display any further information." << std::endl;
							pInfo->error = s.str();
						}
						if(!pInfo->error.empty()) {
// 							out << "### ERRONEOUS ###" << std::endl;
// 							out.flush();
// 							out << pInfo->error;
// 							out.flush();
							std::stringstream title; title
								<< "Machine crashed: " << fileName;
							loggers::exception()(title.str() + '\n' + pInfo->error);
							pInfo->allow = false;
							pInfo->name = "???";
							pInfo->identifier = 0;
							pInfo->vendor = "???";
							pInfo->desc = "???";
							pInfo->version = "???";
							pInfo->APIversion = 0;
							++currentBadPlugsCount;
						} else {											
							std::vector<PluginInfo>::iterator it = infos.begin();
							for (int i = 0; it!=infos.end(); ++it, ++i)  {
								PluginInfo info = *it;
								if (i>0) {
									++currentPlugsCount;
									pInfo = _pPlugsInfo[currentPlugsCount]= new PluginInfo();
								}
								*pInfo = info;	
								pInfo->dllname = fileName;
								pInfo->FileTime = time;
// 								out << info.name << " - successfully instanciated";
// 								out.flush();
							}
						}
						learnDllName(fileName,type);
					}
					++currentPlugsCount;
				}
				catch(const std::exception & e)
				{
					std::stringstream s; s
						<< std::endl
						<< "################ SCANNER CRASHED ; PLEASE REPORT THIS BUG! ################" << std::endl
						<< typeid(e).name() << std::endl;
					if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
// 					out
// 						<< s.str().c_str();
// 					out.flush();
					loggers::crash()(s.str());
				}
				catch(...)
				{
					std::stringstream s; s
						<< std::endl
						<< "################ SCANNER CRASHED ; PLEASE REPORT THIS BUG! ################" << std::endl
						<< "Type of exception is unknown, no further information available.";
// 					out
// 						<< s.str().c_str();
// 					out.flush();
					loggers::crash()(s.str());
				}
			}
// 			out << std::endl;
// 			out.flush();
			}
// 			out.flush();
		}

		void PluginCatcher::DestroyPluginInfo()
		{
			for (int i=0; i<_numPlugins; i++)
			{
				delete _pPlugsInfo[i];
				_pPlugsInfo[i] = 0;
			}
			NativeNames.clear();
			VstNames.clear();
			_numPlugins = -1;
		}

		bool PluginCatcher::LoadCacheFile(int& currentPlugsCount, int& currentBadPlugsCount, bool verify)
		{
			return false;// rePlayer: we always scan on first psy load

#if defined _WIN64
			std::string cache((Global::configuration().cacheDir() / "psycle64.plugin-scan.cache").string());
#elif defined _WIN32
			std::string cache((Global::configuration().cacheDir() / "psycle.plugin-scan.cache").string());
#else
#error unexpected platform
#endif
			RiffFile file;
			CFileFind finder;

			if (!file.Open(cache.c_str()))
			{
#if defined _WIN32
				/// try old location
				std::string cache2((boost::filesystem::path(Global::configuration().appPath()) / "psycle.plugin-scan.cache").string());
				if (!file.Open(cache2.c_str())) return false;
#else
				return false;
#endif
			}
			uint64_t filesize = file.FileSize();
			char Temp[MAX_PATH];
			file.Read(Temp,8);
			Temp[8]=0;
			if (strcmp(Temp,"PSYCACHE")!=0)
			{
				file.Close();
				DeleteFile(cache.c_str());
				return false;
			}

			UINT version;
			file.Read(&version,sizeof(version));
			if (version != CURRENT_CACHE_MAP_VERSION)
			{
				file.Close();
				DeleteFile(cache.c_str());
				return false;
			}

			file.Read(&_numPlugins,sizeof(_numPlugins));
			for (int i = 0; i < _numPlugins; i++)
			{
				PluginInfo p;
				file.ReadString(Temp,sizeof(Temp));
				file.Read(&p.FileTime,sizeof(_pPlugsInfo[currentPlugsCount]->FileTime));
				{
					UINT size;
					file.Read(&size, sizeof size);
					if(size > 0 && size < filesize)
					{
						char *chars(new char[size + 1]);
						file.Read(chars, size);
						chars[size] = '\0';
						p.error = (const char*)chars;
						delete [] chars;
					}
					else if (size != 0) {
						//There has been a problem reading the file. Regenerate it
						_numPlugins = currentPlugsCount;
						DestroyPluginInfo();
						file.Close();
						DeleteFile(cache.c_str());
						return false;
					}
				}
				file.Read(&p.allow,sizeof(p.allow));
				file.Read(&p.mode,sizeof(p.mode));
				file.Read(&p.type,sizeof(p.type));
				file.ReadString(p.name);
				file.Read(&p.identifier,sizeof(p.identifier));
				file.ReadString(p.vendor);
				file.ReadString(p.desc);
				file.ReadString(p.version);
				file.Read(&p.APIversion,sizeof(p.APIversion));

				// Temp here contains the full path to the .dll
				if(finder.FindFile(Temp))
				{
					FILETIME time;
					finder.FindNextFile();
					if (finder.GetLastWriteTime(&time))
					{
						// Only add the information to the cache if the dll hasn't been modified (say, a new version)
						if
							(
							p.FileTime.dwHighDateTime == time.dwHighDateTime &&
							p.FileTime.dwLowDateTime == time.dwLowDateTime
							)
						{
							PluginInfo* pInfo = _pPlugsInfo[currentPlugsCount]= new PluginInfo();

							pInfo->dllname = Temp;
							pInfo->FileTime = p.FileTime;

							///\todo this could be better handled
							if(!pInfo->error.empty())
							{
								pInfo->error = "";
							}
							if(!p.error.empty())
							{
								pInfo->error = p.error;
							}

							pInfo->allow = p.allow;

							pInfo->mode = p.mode;
							pInfo->type = p.type;
							pInfo->name = p.name;
							pInfo->identifier = p.identifier;
							pInfo->vendor = p.vendor;
							pInfo->desc = p.desc;
							pInfo->version = p.version;
							pInfo->APIversion = p.APIversion;

							if(p.error.empty())
							{
								learnDllName(pInfo->dllname,pInfo->type);
							}
							++currentPlugsCount;
						}
					}
				}
			}

			_numPlugins = currentPlugsCount;
			file.Close();
			return true;
		}

		bool PluginCatcher::SaveCacheFile()
		{
			return false; // rePlayer no cache
#if defined _WIN64
			boost::filesystem::path cache(Global::configuration().cacheDir() / "psycle64.plugin-scan.cache");
#elif defined _WIN32
			boost::filesystem::path cache(Global::configuration().cacheDir() / "psycle.plugin-scan.cache");
#else
	#error unexpected platform
#endif

			DeleteFile(cache.string().c_str());
			RiffFile file;
			if (!file.Create(cache.string().c_str(),true)) 
			{
				// note mkdir is posix, not iso, on msvc, it's defined only #if !__STDC__ (in direct.h)
				mkdir(cache.branch_path().string().c_str());
				if (!file.Create(cache.string().c_str(),true)) return false;
			}
			file.Write("PSYCACHE",8);
			UINT version = CURRENT_CACHE_MAP_VERSION;
			file.Write(&version,sizeof(version));
			file.Write(&_numPlugins,sizeof(_numPlugins));
			for (int i=0; i<_numPlugins; i++ )
			{
				PluginInfo* pInfo = _pPlugsInfo[i];
				file.Write(pInfo->dllname.c_str(),pInfo->dllname.length()+1);
				file.Write(&pInfo->FileTime,sizeof(pInfo->FileTime));
				{
					const std::string error(pInfo->error);
					UINT size((UINT)error.size());
					file.Write(&size, sizeof size);
					if(size) file.Write(error.data(), size);
				}
				file.Write(&pInfo->allow,sizeof(pInfo->allow));
				file.Write(&pInfo->mode,sizeof(pInfo->mode));
				file.Write(&pInfo->type,sizeof(pInfo->type));
				file.Write(pInfo->name.c_str(),pInfo->name.length()+1);
				file.Write(&pInfo->identifier,sizeof(pInfo->identifier));
				file.Write(pInfo->vendor.c_str(),pInfo->vendor.length()+1);
				file.Write(pInfo->desc.c_str(),pInfo->desc.length()+1);
				file.Write(pInfo->version.c_str(),pInfo->version.length()+1);
				file.Write(&pInfo->APIversion,sizeof(pInfo->APIversion));
			}
			file.Close();
			return true;
		}

		std::string PluginCatcher::preprocessName(const std::string& dllNameOrig) {
			std::string dllName = dllNameOrig;
			// 1) ensure lower case
			std::transform(dllName.begin(),dllName.end(),dllName.begin(), [](auto a) { return std::tolower(a); }); // rePlayer
			{ // 2) remove extension
			#if defined DIVERSALIS__OS__MICROSOFT
				std::string::size_type const pos(dllName.find(".dll"));
			#elif defined DIVERSALIS__OS__APPLE
				std::string::size_type const pos(dllName.find(".dylib"));
			#else
				std::string::size_type const pos(dllName.find(".so"));
			#endif
				if(pos != std::string::npos) dllName = dllName.substr(0, pos);
				std::string::size_type const pos2(dllName.find(".lua"));
				if(pos2 != std::string::npos) dllName = dllName.substr(0, pos2);
			}

			// 3) replace spaces and underscores with dash.
			std::replace(dllName.begin(),dllName.end(),' ','-');
			std::replace(dllName.begin(),dllName.end(),'_','-');

			return dllName;
		}

		void PluginCatcher::learnDllName(const std::string & fullname,MachineType type)
		{
			std::string str=fullname;
			// strip off path
			std::string::size_type pos=str.rfind('\\');
			if(pos != std::string::npos)
				str=str.substr(pos+1);
			pos=str.rfind('/');
			if(pos != std::string::npos)
				str=str.substr(pos+1);

			str = preprocessName(str);

			switch(type)
			{
			case MACH_PLUGIN: NativeNames[str]=fullname;
				break;
			case MACH_VST:
			case MACH_VSTFX: VstNames[str]=fullname;
				break;
			case MACH_LUA: LuaNames[str]=fullname;
				break;
			case MACH_LADSPA: LadspaNames[str]=fullname;
				break;
			default:
				break;
			}
		}

	}
}
