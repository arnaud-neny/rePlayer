// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2003-2007 johan boule <bohan@jabber.org>
// copyright 2003-2007 psycledelics http://psycle.pastnotecut.org

/// \file
/// \brief delay
#include "plugin.hpp"
#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>
#include <cassert>
#include <vector>
namespace psycle { namespace plugin { namespace Delay {
	using namespace psycle::helpers::math;

class Delay : public Plugin
{
public:
	/*override*/ void help(std::ostream & out) throw()
	{
		out << "Delay." << std::endl;
		out << "Compatible with original psycle 1 arguru's dala delay." << std::endl;
		out << std::endl;
		out << "Beware if you tweak the delay length with a factor > 2 that the memory buffer gets resized." << std::endl;
	}
	enum Parameters
	{
		dry, wet,
		left_delay, left_feedback,
		right_delay, right_feedback,
		snap
	};

	static const Information & information() throw()
	{
		static bool initialized = false;
		static Information *info = NULL;
		if (!initialized) {
			const int factors(2 * 3 * 4 * 5 * 7);
			const Real delay_maximum(Information::Parameter::input_maximum_value / factors);
			static const Information::Parameter parameters [] =
			{
				Information::Parameter::Parameter("dry", -1, 1, 1, Information::Parameter::linear::on),
				Information::Parameter::Parameter("wet", -1, 0, 1, Information::Parameter::linear::on),
				Information::Parameter::Parameter("delay left", 0, 0, delay_maximum, Information::Parameter::linear::on),
				Information::Parameter::Parameter("feedback left", -1, 0, 1, Information::Parameter::linear::on),
				Information::Parameter::Parameter("delay right", 0, 0, delay_maximum, Information::Parameter::linear::on),
				Information::Parameter::Parameter("feedback right", -1, 0, 1, Information::Parameter::linear::on),
				Information::Parameter::Parameter("snap to", 3, factors - 1, Information::Parameter::discrete::on)
			};
			static Information information(0x0110, Information::Types::effect, "ayeternal Dalay Delay", "Dalay Delay", "bohan", 4, parameters, sizeof parameters / sizeof *parameters);
			info = &information;
			initialized = true;
		}
		return *info;
	}

	/*override*/ void describe(std::ostream & out, const int & parameter) const
	{
		switch(parameter)
		{
		case left_delay:
		case right_delay:
			out << (*this)(parameter) << " ticks (lines)";
			break;
		case snap:
			if((*this)[parameter] == information().parameter(parameter).MaxValue) out << "off ";
			out << "1 / " << 1 + (*this)[parameter] << " ticks (lines)";
			break;
		case left_feedback:
		case right_feedback:
		case dry:
		case wet:
			if(std::fabs((*this)(parameter)) < 2e-5)
			{
				out << 0;
				break;
			}
		default:
			Plugin::describe(out, parameter);
		}
	}

	Delay() : Plugin(information()) {}
	/*override*/ void init();
	/*override*/ void Work(Sample l [], Sample r [], int samples, int);
	/*override*/ void parameter(const int &);
protected:
	/*override*/ void samples_per_second_changed()
	{
		parameter(left_delay);
		parameter(right_delay);
	}
	/*override*/ void sequencer_ticks_per_second_changed()
	{
		parameter(left_delay);
		parameter(right_delay);
	}
	enum Channels { left, right, channels };
	std::vector<Real> buffers_ [channels];
	std::vector<Real>::iterator buffer_iterators_ [channels];
	inline void Work(std::vector<Real> & buffer, std::vector<Real>::iterator & buffer_iterator, Sample & input, const Sample & feedback);
	inline void resize(const int & channel, const int & parameter);
	inline void resize(const int & channel, const Real & delay);
};

PSYCLE__PLUGIN__INSTANTIATOR("delay", Delay)

void Delay::init()
{
	for(int channel(0) ; channel < channels ; ++channel)
		resize(channel, Real(0)); // resizes the buffers not to 0, but to 1, the smallest length possible for the algorithm to work
}

void Delay::parameter(const int & parameter)
{
	switch(parameter)
	{
	case left_delay:
		resize(left, left_delay);
		break;
	case right_delay:
		resize(right, right_delay);
		break;
	}
}

inline void Delay::resize(const int & channel, const int & parameter)
{
	const int snap1((*this)[snap] + 1);
	const Real snap_delay(static_cast<int>((*this)(parameter) * snap1) / static_cast<Real>(snap1));
	(*this)(parameter) = snap_delay;
	(*this)[parameter] = information().parameter(parameter).scale->apply_inverse(snap_delay) + 1; // Round up.
	resize(channel, snap_delay);
}

inline void Delay::resize(const int & channel, const Real & delay)
{
	buffers_[channel].resize(1 + static_cast<int>(delay * samples_per_sequencer_tick()), 0);
			// resizes the buffer at least to 1, the smallest length possible for the algorithm to work
	buffer_iterators_[channel] = buffers_[channel].begin();
}

void Delay::Work(Sample l [], Sample r [], int samples, int)
{
	for(int sample(0) ; sample < samples ; ++sample)
	{
		Work(buffers_[left] , buffer_iterators_[left] , l[sample], (*this)(left_feedback));
		Work(buffers_[right], buffer_iterators_[right], r[sample], (*this)(right_feedback));
	}
}

inline void Delay::Work(std::vector<Real> & buffer, std::vector<Real>::iterator & buffer_iterator, Sample & input, const Sample & feedback)
{
	const Real read(*buffer_iterator);
	Real newval = input + feedback * read;
	erase_all_nans_infinities_and_denormals(newval);
	*buffer_iterator = newval;
	if(++buffer_iterator == buffer.end()) buffer_iterator = buffer.begin();
	input = static_cast<Sample>((*this)(dry) * input + (*this)(wet) * read);
}

}}}
