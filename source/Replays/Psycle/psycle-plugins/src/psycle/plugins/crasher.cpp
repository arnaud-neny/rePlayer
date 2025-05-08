// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2007 johan boule <bohan@jabber.org>
// copyright 2004-2007 psycledelics http://psycle.pastnotecut.org

/// \file
/// \brief a crash on purpose plugin
#include "plugin.hpp"
#include <stdexcept>
namespace psycle { namespace plugin { namespace Crasher {

class Crasher : public Plugin
{
public:
	/*override*/ void help(std::ostream & out) throw()
	{
		out << "This is a crash on purpose plugin for helping devers make the host crash proof." << std::endl;
	}

	static const Information & information() throw()
	{
		static const Information information(0x0100, Information::Types::effect, "crasher", "crasher", "bohan", 1, 0, 0);
		return information;
	}

	Crasher() : Plugin(information()) {}

	/*override*/ void Work(Sample l[], Sample r[], int samples, int);

protected:
	inline void Work(Sample &);

	void crash() {
		// c++ exception:
		//throw "crash on purpose!";
		throw std::runtime_error("crash on purpose!");

		// integer division by 0:
		//volatile int i(0); i = 0 / i; // trick so that the compiler does not remove the code when optimizing

		// floating point division by 0:
		//volatile float i(0); i = 0 / i; // trick so that the compiler does not remove the code when optimizing

		// infinite loop so that we can test interruption signal:
		//while(true);
	}
};

PSYCLE__PLUGIN__INSTANTIATOR("crasher", Crasher)

void Crasher::Work(Sample l[], Sample r[], int sample, int)
{
	while(sample--)
	{
		Work(l[sample]);
		Work(r[sample]);
	}
	crash(); ///////////////////////// <--- crash !!!
}

inline void Crasher::Work(Sample & sample)
{
	sample = -sample;
}

}}}
