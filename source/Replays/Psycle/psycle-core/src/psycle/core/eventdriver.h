// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__EVENT_DRIVER__INCLUDED
#define PSYCLE__CORE__EVENT_DRIVER__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

namespace psycle { namespace core {

/// base class for an event driver, such as a MIDI input driver.
///\author Josep Ma Antolin
class PSYCLE__CORE__DECL EventDriver {
	public:
		EventDriver();
		~EventDriver();

		/// 
		virtual bool Open();
		/// 
		virtual bool Sync(int sampleoffset, int buffersize);
		/// 
		virtual void ReSync();
		/// 
		virtual bool Close( );
		/// 
		virtual bool Active();
};

}}
#endif
