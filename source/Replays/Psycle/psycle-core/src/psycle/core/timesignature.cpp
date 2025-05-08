// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "timesignature.h"

namespace psycle { namespace core {

TimeSignature::TimeSignature()
{}

TimeSignature::TimeSignature(int numerator, int denominator)
:
	numerator_(numerator),
    denominator_(denominator)
{}

TimeSignature::TimeSignature(float ownerDefinedBeats)
:
	ownerDefined_(true),
	ownerDefinedBeats_(ownerDefinedBeats)
{}

}}
