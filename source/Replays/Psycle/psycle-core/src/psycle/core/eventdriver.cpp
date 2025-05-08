// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "eventdriver.h"

namespace psycle { namespace core {

EventDriver::EventDriver() {
}

EventDriver::~EventDriver() {
}

bool EventDriver::Open() {
	return false;
}

bool EventDriver::Sync(int /*sampleoffset*/, int /*buffersize*/) {
	return false;
}

void EventDriver::ReSync() {}

bool EventDriver::Close() {
	return false;
}

bool EventDriver::Active() {
	return false;
}

}}
