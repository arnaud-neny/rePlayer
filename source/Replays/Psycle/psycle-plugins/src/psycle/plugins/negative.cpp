// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2003-2007 dan peddle <dazzled@retropaganda.info>
// copyright 2003-2007 johan boule <bohan@jabber.org>
// copyright 2003-2007 psycledelics http://psycle.pastnotecut.org

/// \file
/// \brief just a negative
#include "plugin.hpp"
namespace psycle { namespace plugin { namespace Negative {

class Negative : public Plugin
{
public:
	/*override*/ void help(std::ostream & out) throw()
	{
		out << "just a Negative (out = -in)" << std::endl;
	}
	static const Information & information() throw()
	{
		static bool initialized = false;
		static Information *info = NULL;
		if (!initialized) {
			static Information information(0x0100, Information::Types::effect, "Negative", "Negative", "who cares", 1, 0, 0);
			info = &information;
			initialized = true;
		}
		return *info;
	}
	Negative() : Plugin(information()) {}
	/*override*/ void Work(Sample l[], Sample r[], int samples, int);
protected:
	inline void Work(Sample &);
};

PSYCLE__PLUGIN__INSTANTIATOR("negative", Negative)

void Negative::Work(Sample l[], Sample r[], int sample, int)
{
	while(sample--)
	{
		Work(l[sample]);
		Work(r[sample]);
	}
}

inline void Negative::Work(Sample & sample)
{
	sample = -sample;
}

}}}
