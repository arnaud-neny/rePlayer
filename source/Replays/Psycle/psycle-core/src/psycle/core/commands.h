// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

// This is outside pattern, since this way, machines just need to know about events.

#ifndef PSYCLE__CORE__COMMANDS__INCLUDED
#define PSYCLE__CORE__COMMANDS__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

namespace psycle { namespace core {

const int volumeempty = 0xFF;

namespace instrumenttypes {
	enum type_t {
		machine = 0,
		sample,
		sequence,
		// maintain these two as the last ones
		invalid,
		empty = 0xFFFF ///< empty is used to indicate an unassigned instrument in a track or in a classicEvent.
	};
}

namespace notetypes {
	enum type_t {
		c0 = 0,
		b9 = 119,
		release = 120,
		tweak,
		tweak_slide,
		tweak_slide_to,
		midi_cc,
		midi_slide,
		midi_slide_to,
		automation,
		microtonal0 = 128,
		microtonallast = 247,
		wire,
		wire_slide,
		wire_slide_to,
		pan,
		pan_slide,
		pan_slide_to,
		// maintain these two as the last ones
		invalid,
		empty = 255
	};
}

namespace commandtypes {
	enum type_t {
		EXTENDED      = 0xFE, // see below
		SET_TEMPO     = 0xFF,
		NOTE_DELAY    = 0xFD,
		RETRIGGER     = 0xFB,
		RETR_CONT     = 0xFA,
		SET_VOLUME    = 0xFC,
		SET_PANNING   = 0xF8,
		BREAK_TO_LINE = 0xF2,
		JUMP_TO_ORDER = 0xF3,
		ARPEGGIO      = 0xF0,
		BPM_CHANGE    = 0xFF
		// LOOP_TO TODO
	};
	namespace extended {
		enum type_t {
			// Extended Commands from 0xFE
			SET_LINESPERBEAT0  = 0x00,
			SET_LINESPERBEAT1  = 0x10, ///< Range from FE00 to FE1F is reserved for changing lines per beat.
			SET_BYPASS         = 0x20,
			SET_MUTE           = 0x30,
			PATTERN_LOOP       = 0xB0, ///< Loops the current pattern x times. 0xFEB0 sets the loop start point.
			PATTERN_DELAY      = 0xD0, ///< causes a "pause" of x rows ( i.e. the current row becomes x rows longer)
			FINE_PATTERN_DELAY = 0xF0  ///< causes a "pause" of x ticks ( i.e. the current row becomes x ticks longer)
		};
	}
}

}}
#endif
