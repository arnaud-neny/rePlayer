// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PATTERN_EVENT__INCLUDED
#define PSYCLE__CORE__PATTERN_EVENT__INCLUDED
#pragma once

#include "commands.h"

#include <vector>
#include <string>

namespace psycle { namespace core {

class PSYCLE__CORE__DECL PatternEvent {
	public:
		typedef std::pair<uint8_t, uint8_t> PcmType;
		typedef std::vector<PcmType> PcmListType;

		PatternEvent();

		bool empty() const {
			return
				note_ == notetypes::empty &&
				inst_ == 255 && mach_ == 255 &&
				cmd_ == 0 && param_ == 0;
		}

		bool IsGlobal() const {
			return (cmd_ >= 0xF0);
		}

		uint8_t note() const { return note_; }
		void setNote(uint8_t value) { note_ = value; }

		uint8_t instrument() const { return inst_; }
		void setInstrument(uint8_t instrument) { inst_ = instrument; }

		uint8_t machine() const { return mach_; }
		void setMachine(uint8_t machine) { mach_ = machine; }

		uint8_t command() const { return cmd_; }
		void setCommand(uint8_t command) { cmd_ = command; }

		uint8_t parameter() const { return param_; }
		void setParameter(uint8_t parameter) { param_ = parameter; }

		uint8_t volume() const { return volume_; }
		void setVolume(uint8_t volume) { volume_ = volume; }

		PcmListType & paraCmdList() { return paraCmdList_; }

		double time_offset() const { return offset_; }
		void set_time_offset(double offset) { offset_ = offset; }

		std::string toXml(int track) const;
	private:
		uint8_t note_;
		uint8_t inst_;
		uint8_t mach_;
		uint8_t cmd_;
		uint8_t param_;
		uint8_t volume_;
		PcmListType paraCmdList_;
		double offset_;
};

}}
#endif
