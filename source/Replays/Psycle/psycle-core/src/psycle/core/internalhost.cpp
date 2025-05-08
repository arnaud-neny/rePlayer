// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "internalhost.hpp"

#include "pluginfinder.h"
#include "internal_machines.h"
#include "sampler.h"
#include "xmsampler.h"
#include "mixer.h"

#include <map>

namespace psycle {namespace core {

namespace InternalMacs {

	static const MachineKey keys[]={
		InternalKeys::master,
		InternalKeys::dummy,
		InternalKeys::sampler,
		InternalKeys::sampulse,
		InternalKeys::duplicator,
		InternalKeys::mixer,
		InternalKeys::audioinput,
		InternalKeys::lfo
	};
	static const PluginInfo infos[]={
		PluginInfo(MachineRole::MASTER,"Master","Psycledelics","Outputs audio to soundcard","","1.1","","Master"),
		PluginInfo(MachineRole::EFFECT,"Dummy Machine","Psycledelics","lets audio pass through","","1.1","","Mixer"),
		PluginInfo(MachineRole::GENERATOR,"Sampler","Psycledelics","Plays .wav audio samples","","1.3","","Sampler"),
		PluginInfo(MachineRole::GENERATOR,"Sampulse","JosepMa [JAZ]","Tracker oriented sampler","","0.9","","Sampler"),
		PluginInfo(MachineRole::CONTROLLER,"Note Duplicator","JosepMa [JAZ]","Forwards events to other machines","","1.0","","Controller"),
		PluginInfo(MachineRole::EFFECT,"Send/Return Mixer","JosepMa [JAZ]","Mixes audio with send/returns","","1.0","","Mixer"),
		PluginInfo(MachineRole::GENERATOR,"AudioInput","JosepMa [JAZ]","Receives audio from the soundcard","","0.7","","Capture"),
		PluginInfo(MachineRole::CONTROLLER,"LFO Machine","dw","Controls parameters of other machines","","0.5","","Controller")
	};
}

InternalHost::InternalHost(MachineCallbacks*calls)
:MachineHost(calls){
}
InternalHost::~InternalHost()
{
}

InternalHost& InternalHost::getInstance(MachineCallbacks* callb) {
	static InternalHost instance(callb);
	return instance;
}

Machine* InternalHost::CreateMachine(PluginFinder& /*finder */, const MachineKey& key, Machine::id_type id)
{
	Machine* mac=0;

	switch (key.index())
	{
	case InternalMacs::MASTER:
		mac = new Master(mcallback_, id);
		break;
	case InternalMacs::DUMMY:
		mac = new Dummy(mcallback_, id);
		break;
	case InternalMacs::SAMPLER:
		mac = new Sampler(mcallback_, id);
		break;
	case InternalMacs::XMSAMPLER:
		mac = new XMSampler(mcallback_, id);
		break;
	case InternalMacs::DUPLICATOR:
		mac = new DuplicatorMac(mcallback_, id);
		break;
	case InternalMacs::MIXER:
		mac = new Mixer(mcallback_, id);
		break;
	case InternalMacs::AUDIOINPUT:
		mac = new AudioRecorder(mcallback_, id);
		break;
	case InternalMacs::LFO:
		mac = new LFO(mcallback_, id);
		break;
	default:
		break;
	}
	mac->Init();
	return mac;
}

void InternalHost::FillFinderData(PluginFinder& finder, bool /*clearfirst*/)
{
	//InternalHost always regenerates its pluginInfo.
	finder.ClearMap(hostCode());

	// Master machine is skipped because it is never created by the user.
	for(int  i=InternalMacs::type_t(1); i < InternalMacs::NUM_MACS; i++) {
		finder.AddInfo(InternalMacs::keys[i], InternalMacs::infos[i]);
	}
}

}}
