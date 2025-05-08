// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "vsthost.h"

#ifndef DIVERSALIS__OS__MICROSOFT
	UNIVERSALIS__COMPILER__WARNING("VST host is currently only implemented on Microsoft Windows operating system.")
#else
	#include <psycle/core/pluginfinder.h>
	#include <psycle/core/vstplugin.h>
	#include <psycle/core/playertimeinfo.h>

	#include <iostream>
	#include <sstream>

	#ifdef DIVERSALIS__OS__MICROSOFT
		#include <universalis/os/include_windows_without_crap.hpp>
	#else
		#include <dlfcn.h>
	#endif

	namespace psycle { namespace core { namespace vst {

	host::host(MachineCallbacks*calls)
	:
		MachineHost(calls)
	{
		master = new AudioMaster(calls);
	}
	
	host::~host() {
		delete master;
	}

	host& host::getInstance(MachineCallbacks* callb) {
		static host instance(callb);
		return instance;
	}
	void host::ChangeSampleRate(int sample_rate) {
		master->SetSampleRate(sample_rate);
	}

	Machine* host::CreateMachine(PluginFinder& finder, const MachineKey& key,Machine::id_type id)  {
		if (key == InternalKeys::wrapperVst ) {
			return static_cast<vst::plugin*>(master->CreateWrapper(0));
		}
		//FIXME: This is a good place where to use exceptions. (task for a later date)
		if (!finder.hasKey(key)) return 0;
		std::string fullPath = finder.lookupDllName(key);
		if (fullPath.empty()) return 0;

		vst::plugin* plug = master->LoadPlugin(fullPath, key, id);
		plug->Init();
		return plug;
	}


	void host::FillPluginInfo(const std::string& fullName, const std::string& fileName, PluginFinder& finder) {
		#if defined DIVERSALIS__OS__MICROSOFT
			if(fileName.find(".dll") == std::string::npos) return;
		#elif defined DIVERSALIS__OS__APPLE
			if(fileName.find(".dylib") == std::string::npos) return;
		#else
			if(fileName.find(".so") == std::string::npos) return;
		#endif

		vst::plugin *vstPlug=0;
		std::ostringstream sIn;
		MachineKey key( hostCode(), fileName);

		try {
			vstPlug = master->LoadPlugin(fullName, key, 0);
		} catch(const std::exception & e) {
			sIn << typeid(e).name() << std::endl;
			if(e.what()) sIn << e.what(); else sIn << "no message"; sIn << std::endl;
		} catch(...) {
			sIn << "Type of exception is unknown, cannot display any further information." << std::endl;
		}
		if(!sIn.str().empty()) {
			PluginInfo pinfo;
			pinfo.setError(sIn.str());

			std::cout << "### ERRONEOUS ###" << std::endl;
			std::cout.flush();
			std::cout << pinfo.error();
			std::cout.flush();
			std::stringstream title;
			title << "Machine crashed: " << fileName;

			pinfo.setAllow(false);
			pinfo.setName("???");
			pinfo.setAuthor("???");
			pinfo.setDesc("???");
			pinfo.setLibName(fullName);
			pinfo.setApiVersion("???");
			pinfo.setPlugVersion("???");
			pinfo.setFileTime(boost::filesystem::last_write_time(boost::filesystem::path(fullName)));
			MachineKey key( hostCode(), fileName, 0);
			finder.AddInfo( key, pinfo);
			if (vstPlug) delete vstPlug;
		} else {
			if(vstPlug->IsShellMaster()) {
				char tempName[64] = {0};
				VstInt32 plugUniqueID = 0;
				while((plugUniqueID = vstPlug->GetNextShellPlugin(tempName)) != 0) {
					if(tempName[0] != 0) {
						PluginInfo pinfo;
						pinfo.setLibName(fullName);
						pinfo.setFileTime(boost::filesystem::last_write_time(boost::filesystem::path(fullName)));

						pinfo.setAllow(true);
						{
							std::ostringstream s;
							s << vstPlug->GetVendorName() << " " << tempName;
							pinfo.setName(s.str());
						}
						pinfo.setAuthor(vstPlug->GetVendorName());
						pinfo.setRole( vstPlug->IsSynth()?MachineRole::GENERATOR : MachineRole::EFFECT );

						{
							std::ostringstream s;
							s << (vstPlug->IsSynth() ? "VST Shell instrument" : "VST Shell effect") << " by " << vstPlug->GetVendorName();
							pinfo.setDesc(s.str());
						}
						{
							std::ostringstream s;
							s << vstPlug->GetVersion() << " or " << std::hex << vstPlug->GetVersion();
							pinfo.setPlugVersion(s.str());
						}
						{
							std::ostringstream s;
							s << vstPlug->GetVstVersion();
							pinfo.setApiVersion(s.str());
						}
						MachineKey keysubPlugin( hostCode(), fileName, plugUniqueID);
						finder.AddInfo( keysubPlugin, pinfo);
					}
				}
			} else {
				PluginInfo pinfo;
				pinfo.setAllow(true);
				pinfo.setLibName(fullName);
				pinfo.setFileTime(boost::filesystem::last_write_time(boost::filesystem::path(fullName)));
				pinfo.setName(vstPlug->GetName());
				pinfo.setAuthor(vstPlug->GetVendorName());
				pinfo.setRole( vstPlug->IsSynth()?MachineRole::GENERATOR : MachineRole::EFFECT );

				{
					std::ostringstream s;
					s << (vstPlug->IsSynth() ? "VST instrument" : "VST effect") << " by " << vstPlug->GetVendorName();
					pinfo.setDesc(s.str());
				}
				{
					std::ostringstream s;
					s << vstPlug->GetVersion() << " or " << std::hex << vstPlug->GetVersion();
					pinfo.setPlugVersion(s.str());
				}
				{
					std::ostringstream s;
					s << vstPlug->GetVstVersion();
					pinfo.setApiVersion(s.str());
				}
				MachineKey key( hostCode(), fileName, 0);
				finder.AddInfo( key, pinfo);
			}
			std::cout << vstPlug->GetName() << " - successfully instanciated";
			std::cout.flush();

			try {
				delete vstPlug;
				// [bohan] phatmatik crashes here...
				// <magnus> so does PSP Easyverb, in FreeLibrary
			} catch(const std::exception & e) {
				std::stringstream s; s
					<< "Exception occured while trying to free the temporary instance of the plugin." << std::endl
					<< "This plugin will not be disabled, but you might consider it unstable." << std::endl
					<< typeid(e).name() << std::endl;
				if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
				std::cout
					<< std::endl
					<< "### ERRONEOUS ###" << std::endl
					<< s.str().c_str();
				std::cout.flush();
				std::stringstream title; title
					<< "Machine crashed: " << fileName;
			} catch(...) {
				std::stringstream s; s
					<< "Exception occured while trying to free the temporary instance of the plugin." << std::endl
					<< "This plugin will not be disabled, but you might consider it unstable." << std::endl
					<< "Type of exception is unknown, no further information available.";
				std::cout
					<< std::endl
					<< "### ERRONEOUS ###" << std::endl
					<< s.str().c_str();
				std::cout.flush();
				std::stringstream title; title
					<< "Machine crashed: " << fileName;
			}
		}
	}
	//==============================================================
	//==============================================================

	vst::plugin* AudioMaster::LoadPlugin(std::string fullName, MachineKey key, Machine::id_type id) {
		currentKey = key;
		currentId = id;
		return dynamic_cast<vst::plugin*>(CVSTHost::LoadPlugin(fullName.c_str(),key.index()));
	}

	CEffect * AudioMaster::CreateEffect(LoadedAEffect &loadstruct) {
		return new plugin(pCallbacks,currentKey,currentId,loadstruct);
	}

	CEffect * AudioMaster::CreateWrapper(AEffect *effect) {
		return new plugin(pCallbacks,currentId,effect);
	}


	void AudioMaster::CalcTimeInfo(long lMask) {
		///\todo: cycleactive and recording to a "Start()" function.
		// automationwriting and automationreading.
		//
		/*
		kVstTransportCycleActive = 1 << 2,
		kVstTransportRecording   = 1 << 3,

		kVstAutomationWriting    = 1 << 6,
		kVstAutomationReading    = 1 << 7,
		*/

		const PlayerTimeInfo &info = pCallbacks->timeInfo();
		vstTimeInfo.samplePos = info.samplePos();
		vstTimeInfo.tempo = info.bpm();
		vstTimeInfo.flags |= kVstTempoValid;
		
		if( pCallbacks->timeInfo().cycleEnabled() ) {
			vstTimeInfo.flags |= kVstTransportCycleActive;
		}

		if (lMask & kVstCyclePosValid && pCallbacks->timeInfo().cycleEnabled()) {
			vstTimeInfo.cycleStartPos = info.cycleStartPos();
			vstTimeInfo.cycleEndPos = info.cycleEndPos();
			vstTimeInfo.flags |= kVstCyclePosValid;
		}

		if(lMask & kVstPpqPosValid)
		{
			vstTimeInfo.flags |= kVstPpqPosValid;
			vstTimeInfo.ppqPos = info.playBeatPos();
			// Disable default handling.
			lMask &= ~kVstPpqPosValid;
		}

		//timeSigNumerator } Via SetTimeSignature() function
		//timeSigDenominator } "" ""
		//smpteFrameRate (See VstSmpteFrameRate in aeffectx.h)

		CVSTHost::CalcTimeInfo(lMask);
	}


	bool AudioMaster::OnCanDo(CEffect &pEffect, const char *ptr) const {
		using namespace seib::vst::HostCanDos;
		bool value =  CVSTHost::OnCanDo(pEffect,ptr);
		if (value) return value;
		else if (
			//|| (!strcmp(ptr, canDoReceiveVstEvents)) // "receiveVstEvents",
			//|| (!strcmp(ptr, canDoReceiveVstMidiEvent )) // "receiveVstMidiEvent",
			//|| (!strcmp(ptr, "receiveVstTimeInfo" )) // DEPRECATED

			(!strcmp(ptr, canDoReportConnectionChanges )) // "reportConnectionChanges",
			//|| (!strcmp(ptr, canDoAcceptIOChanges )) // "acceptIOChanges",
			||(!strcmp(ptr, canDoSizeWindow )) // "sizeWindow",

			//|| (!strcmp(ptr, canDoAsyncProcessing )) // DEPRECATED
			//|| (!strcmp(ptr, canDoOffline )) // "offline",
			//|| (!strcmp(ptr, "supportShell" )) // DEPRECATED
			//|| (!strcmp(ptr, canDoEditFile )) // "editFile",
			//|| (!strcmp(ptr, canDoSendVstMidiEventFlagIsRealtime ))
			)
			return true;
		return false; // by default, no
	}

	long AudioMaster::DECLARE_VST_DEPRECATED(OnTempoAt)(CEffect &pEffect, long pos) const {
		//\todo: return the real tempo in the future, not always the current one
		// pos in Sample frames, return bpm* 10000
		return vstTimeInfo.tempo * 10000;
	}

	long AudioMaster::OnGetOutputLatency(CEffect &pEffect) const {
		return pCallbacks->timeInfo().outputLatency();
	}

	long AudioMaster::OnGetInputLatency(CEffect &pEffect) const {
		return pCallbacks->timeInfo().inputLatency();
	}

	void AudioMaster::Log(std::string message) {
		//todo
	}

	bool AudioMaster::OnWillProcessReplacing(CEffect &pEffect) const {
		return ((plugin*)&pEffect)->WillProcessReplace();
	}

	///\todo: Get information about this function
	long AudioMaster::OnGetAutomationState(CEffect &pEffect) const { return kVstAutomationUnsupported; }

	}}}

#endif
