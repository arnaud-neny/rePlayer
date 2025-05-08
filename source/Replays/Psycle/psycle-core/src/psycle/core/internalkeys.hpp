// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__INTERNAL_KEYS__INCLUDED
#define PSYCLE__CORE__INTERNAL_KEYS__INCLUDED
#pragma once

#include <psycle/core/machinekey.hpp>

namespace psycle { namespace core {


// type InternalMacs::type
// Allows to differentiate between internal machines.
// They are not intended to be used from outside. Use InternalKeys for that.
namespace InternalMacs {
	enum type_t {
		MASTER = 0,
		DUMMY,
		SAMPLER,
		XMSAMPLER,
		DUPLICATOR,
		MIXER,
		AUDIOINPUT,
		LFO,
		//Keep at Last position.
		NUM_MACS
	};
}
namespace InternalKeys {
	static const MachineKey invalid(Hosts::INTERNAL,"<invalid>", uint32_t(-1));
	static const MachineKey master(Hosts::INTERNAL,"<master>", InternalMacs::MASTER);
	static const MachineKey dummy(Hosts::INTERNAL,"<dummy>", InternalMacs::DUMMY);
	static const MachineKey sampler(Hosts::INTERNAL,"<sampler>", InternalMacs::SAMPLER );
	static const MachineKey sampulse(Hosts::INTERNAL,"<xm-sampler>", InternalMacs::XMSAMPLER );
	static const MachineKey duplicator(Hosts::INTERNAL,"<duplicator>", InternalMacs::DUPLICATOR);
	static const MachineKey mixer(Hosts::INTERNAL,"<mixer>", InternalMacs::MIXER );
	static const MachineKey audioinput(Hosts::INTERNAL,"<audio-input>", InternalMacs::AUDIOINPUT );
	static const MachineKey lfo(Hosts::INTERNAL,"<lfo>", InternalMacs::LFO );
	static const MachineKey failednative(Hosts::NATIVE,"<failed-native>", 0);
	static const MachineKey wrapperVst(Hosts::VST,"<vst-wrapper>", 0);
}

}}
#endif
