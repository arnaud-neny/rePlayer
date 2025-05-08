// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__TIME_SIGNATURE__INCLUDED
#define PSYCLE__CORE__TIME_SIGNATURE__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

namespace psycle { namespace core {

class PSYCLE__CORE__DECL TimeSignature {
	public:
		TimeSignature();
		TimeSignature(int numerator, int denominator);
		TimeSignature(float ownerDefinedBeats);

		int numerator() const { return ownerDefined_ ? 4 : numerator_; }
		void setNumerator(int value) { numerator_ = value; }

		int denominator() const { return ownerDefined_ ? 4 : denominator_; }
		void setDenominator(int value) { if(value != 0) denominator_ = value; }

		int count() const { return count_; }
		void incCount() { ++count_; }
		void setCount(int count) { count_ = count; }

		bool ownerDefined() const { return ownerDefined_; }

		float beats() const { return ownerDefined_ ? ownerDefinedBeats_ : static_cast<float>(numerator_ * count_); }
		void set_beats(float beats) { ownerDefined_ = true; ownerDefinedBeats_ = beats; }

	private:
        int numerator_ = 4;
        int denominator_ = 4;
        int count_ = 1;

        bool ownerDefined_ = false;
        float ownerDefinedBeats_ = 0;
};

}}
#endif
