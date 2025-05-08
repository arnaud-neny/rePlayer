// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2007 johan boule <bohan@jabber.org>
// copyright 2004-2007 psycledelics http://psycle.pastnotecut.org

/// \file
/// \brief Haas stereo time delay spatial localization
#include "plugin.hpp"
#include <cassert>
#include <vector>
namespace psycle { namespace plugin { namespace Haas {
	
/// Haas stereo time delay spatial localization.
class Haas : public Plugin
{
public:
	/// <bohan> use 64-bit floating point numbers or else accuracy is not sufficient
	typedef double Real;

	/*override*/ void help(std::ostream & out)  throw()
	{
		out << "Haas stereo time delay spatial localization" << std::endl;
		out << std::endl;
		out << "What we're doing here is essentially creating two Haas effects:" << std::endl;
		out << "\t The first gives your ears direct spacialization between the sound source and the listener," << std::endl;
		out << "\t and the second gives your brain indirect spacialization between the listener and the source via some reference point on which the sound reflected." << std::endl;
		out << std::endl;
		out << "A third delay can be used for late reflection, giving your brain a hint about the openess of the environement." << std::endl;
		out << std::endl;
		out << "Note: The stereo input signal left and right channels are first summed to form a mono input signal." << std::endl;
	}

	enum Parameters
	{
		overall_gain,
		overall_dry_wet,

		separator_direct,

		direct_gain,
		direct_pan,
		direct_delay_stereo_delta,

		separator_early_reflection,

		early_reflection_gain,
		early_reflection_pan,
		early_reflection_delay,
		early_reflection_delay_stereo_delta,

		separator_late_reflection,

		late_reflection_gain,
		late_reflection_pan,
		late_reflection_delay,

		separator,

		channel_mix
	};

	enum Channel_Mix { normal, swapped, mono };

	static const Information & information() throw()
	{
		static const Information::Parameter parameters [] =
		{
			/* overall_gain                        = */ Information::Parameter::Parameter("gain", std::pow(10., -60. / 20), 1, std::pow(10., +24. / 20), Information::Parameter::exponential::on),
			/* overall_dry_wet                     = */ Information::Parameter::Parameter("dry / wet", 0, 1, 1, Information::Parameter::linear::on),

			/* separator_direct                    = */ Information::Parameter("direct"),

			/* direct_gain                         = */ Information::Parameter::Parameter("gain", std::pow(10., -60. / 20), .5, std::pow(10., +24. / 20), Information::Parameter::exponential::on),
			/* direct_pan                          = */ Information::Parameter::Parameter("pan", -1, 0, 1, Information::Parameter::linear::on),
			/* direct_delay_stereo_delta           = */ Information::Parameter::Parameter("delay stereo delta", -.006, 0, +.006, Information::Parameter::linear::on),

			/* separator_early_reflection          = */ Information::Parameter("early reflection"),

			/* early_reflection_gain               = */ Information::Parameter::Parameter("gain", std::pow(10., -60. / 20), 0, std::pow(10., +24. / 20), Information::Parameter::exponential::on),
			/* early_reflection_pan                = */ Information::Parameter::Parameter("pan", -1, 0, 1, Information::Parameter::linear::on),
			/* early_reflection_delay              = */ Information::Parameter::Parameter("delay", .0005, .01, .045, Information::Parameter::exponential::on),
			/* early_reflection_delay_stereo_delta = */ Information::Parameter::Parameter("delay stereo delta", -.006, 0, +.006, Information::Parameter::linear::on),

			/* separator_late_reflection           = */ Information::Parameter("late reflection"),

			/* late_reflection_gain                = */ Information::Parameter::Parameter("gain", std::pow(10., -60. / 20), 0, std::pow(10., +24. / 20), Information::Parameter::exponential::on),
			/* late_reflection_pan                 = */ Information::Parameter::Parameter("pan", -1, 0, 1, Information::Parameter::linear::on),
			/* late_reflection_delay               = */ Information::Parameter::Parameter("delay", .015, .04, .100, Information::Parameter::exponential::on),

			/* separator                           = */ Information::Parameter(),
			/* channel_mix                         = */ Information::Parameter::Parameter("channel mix", normal, mono, Information::Parameter::discrete::on)
		};
		static const Information information(0x0100, Information::Types::effect, "Haas stereo time delay spatial localization", "Haas", "bohan/dilvie collaboration", 1, parameters, sizeof parameters / sizeof *parameters);
		return information;
	}

	/*override*/ void describe(std::ostream & out, const int & parameter) const
	{
		switch(parameter)
		{
			case direct_delay_stereo_delta:
			case early_reflection_delay_stereo_delta:
				if(std::fabs((*this)(parameter)) < 1e-6)
				{
					out << 0;
					break;
				}
			case early_reflection_delay:
			case late_reflection_delay:
				out << (*this)(parameter) * 1000.f << " ms";
				break;
			case overall_dry_wet:
				out
					<< std::setprecision(3) << std::setw(6) << (*this)(parameter)
					<< " ("
					<< std::setw(6) << 20.f * std::log10(1 - (*this)(parameter)) << ", "
					<< std::setw(6) << 20.f * std::log10(    (*this)(parameter))
					<< " dB)";
				break;
			case overall_gain:
				out
					<< std::setprecision(3) << std::setw(6) << (*this)(parameter)
					<< " (" << std::setw(6) << 20.f * std::log10((*this)(parameter)) << " dB)";
				break;
			case direct_gain:
			case early_reflection_gain:
			case late_reflection_gain:
				//The internal gains are compensated because input is added, so it's like multiplying by two.
				out
					<< std::setprecision(3) << std::setw(6) << (*this)(parameter)
					<< " (" << std::setw(6) << 20.f * std::log10((*this)(parameter)*2.f) << " dB)";
				break;
			case channel_mix:
				switch((*this)[parameter])
				{
					case normal:
						out << "normal";
						break;
					case swapped:
						out << "swapped";
						break;
					case mono:
						out << "mono";
						break;
					default:
						out << "???";
				}
				break;
			case direct_pan:
			case early_reflection_pan:
			case late_reflection_pan:
				if(std::fabs((*this)(parameter)) < 2e-5)
				{
					out << 0;
					break;
				}
			default:
				Plugin::describe(out, parameter);
		}
	}

	Haas()
	:
		Plugin(information()),
		direct_left(direct_first),
		direct_right(direct_first),
		early_reflection_left(early_reflection_first),
		early_reflection_right(early_reflection_first),
		direct_delay_stereo_delta_abs(0),
		early_reflection_delay_stereo_delta_abs(0),
		overall_gain_dry(0),
		overall_gain_wet(1),
		direct_gain_left(1),
		direct_gain_right(1),
		early_reflection_gain_left(1),
		early_reflection_gain_right(1),
		late_reflection_gain_left(1),
		late_reflection_gain_right(1)
	{}
	/*override*/ void init();
	/*override*/ void Work(Sample l [], Sample r [], int samples, int);
	/*override*/ void parameter(const int &);

protected:

	/*override*/ void samples_per_second_changed() { parameter(late_reflection_delay); }

	enum Channels { left, right, channels };
	enum Stages { direct_first, direct_last, early_reflection_first, early_reflection_last, late_reflection, stages };
	int direct_left, direct_right, early_reflection_left, early_reflection_right;
	std::vector<Real> buffer_;
	std::vector<Real>::iterator buffer_iterators_ [stages];
	Real direct_delay_stereo_delta_abs, early_reflection_delay_stereo_delta_abs;
	Real overall_gain_dry, overall_gain_wet;
	Real direct_gain_left, direct_gain_right;
	Real early_reflection_gain_left, early_reflection_gain_right;
	Real late_reflection_gain_left, late_reflection_gain_right;

	inline void Work(Sample & left, Sample & right);
	void resize();
};

PSYCLE__PLUGIN__INSTANTIATOR("haas", Haas)

void Haas::init()
{
	resize();
}

void Haas::parameter(const int & parameter)
{
	switch(parameter)
	{
		case direct_delay_stereo_delta:
			direct_delay_stereo_delta_abs = std::fabs((*this)(parameter));
			if((*this)(parameter) > 0) {
				direct_left = direct_last;
				direct_right = direct_first;
			}
			else if((*this)(parameter) < 0) {
				direct_left = direct_first;
				direct_right = direct_last;
			}
			else {
				direct_left = direct_first;
				direct_right = direct_first;
			}
			resize();
			break;
		case early_reflection_delay_stereo_delta:
			early_reflection_delay_stereo_delta_abs = std::fabs((*this)(parameter));
			if((*this)(parameter) > 0) {
				early_reflection_left = early_reflection_last;
				early_reflection_right = early_reflection_first;
			}
			else if((*this)(parameter) < 0) {
				early_reflection_left = early_reflection_first;
				early_reflection_right = early_reflection_last;
			}
			else {
				early_reflection_left = early_reflection_first;
				early_reflection_right = early_reflection_first;
			}
			resize();
			break;
		case early_reflection_delay:
		case late_reflection_delay:
			resize();
			break;
		case overall_gain:
		case overall_dry_wet:
			overall_gain_dry = (*this)(overall_gain) * (1 - (*this)(overall_dry_wet));
			overall_gain_wet = (*this)(overall_gain) * (*this)(overall_dry_wet);
			break;
		case direct_gain:
		case direct_pan:
			direct_gain_left  = (*this)(direct_gain) * std::min(Real(1), (1 - (*this)(direct_pan)));
			direct_gain_right = (*this)(direct_gain) * std::min(Real(1), (1 + (*this)(direct_pan)));
			break;
		case early_reflection_gain:
		case early_reflection_pan:
			early_reflection_gain_left  = (*this)(early_reflection_gain) * std::min(Real(1), (1 - (*this)(early_reflection_pan)));
			early_reflection_gain_right = (*this)(early_reflection_gain) * std::min(Real(1), (1 + (*this)(early_reflection_pan)));
			break;
		case late_reflection_gain:
		case late_reflection_pan:
			late_reflection_gain_left  = (*this)(late_reflection_gain) * std::min(Real(1), (1 - (*this)(late_reflection_pan)));
			late_reflection_gain_right = (*this)(late_reflection_gain) * std::min(Real(1), (1 + (*this)(late_reflection_pan)));
	}
}

void Haas::resize()
{
	Real const static max_delta(information().parameter(direct_delay_stereo_delta).scale->output_maximum());
	Real const max_delay(std::max((*this)(early_reflection_delay) + max_delta, (*this)(late_reflection_delay)));

	// resizes the buffer at least to 1, the smallest length possible for the algorithm to work
	std::vector<Real>::size_type const new_size(1 + static_cast<int>(max_delay * samples_per_second()));

	std::vector<Real>::difference_type write_pos;

	if(buffer_.size() == 0) {
		buffer_.resize(new_size, 0);
		write_pos = new_size - 1;
	} else {
		write_pos = buffer_iterators_[direct_first] - buffer_.begin();
		if(new_size > buffer_.size())
			buffer_.insert(buffer_.end() - 1, new_size - buffer_.size(), 0);
	}

	buffer_iterators_[direct_first] = buffer_.begin() + write_pos;

	int offset  = write_pos - static_cast<std::vector<Real>::difference_type>(
		direct_delay_stereo_delta_abs * samples_per_second());
	if (offset < 0 ) offset += buffer_.size();
	buffer_iterators_[direct_last] = buffer_.begin() + offset;

	offset = write_pos - static_cast<std::vector<Real>::difference_type>(
		(*this)(early_reflection_delay) * samples_per_second());
	if(offset < 0) offset += buffer_.size();
	buffer_iterators_[early_reflection_first] = buffer_.begin() + offset;

	offset = write_pos - static_cast<std::vector<Real>::difference_type>(
		((*this)(early_reflection_delay) + early_reflection_delay_stereo_delta_abs)
		* samples_per_second());
	if(offset < 0) offset += buffer_.size();
	buffer_iterators_[early_reflection_last] = buffer_.begin() + offset;

	offset = write_pos - static_cast<std::vector<Real>::difference_type>(
		(*this)(late_reflection_delay) * samples_per_second());
	if(offset < 0) offset += buffer_.size();
	buffer_iterators_[late_reflection] = buffer_.begin() + offset;
}

void Haas::Work(Sample l [], Sample r [], int samples, int)
{
	switch((*this)[channel_mix])
	{
		case normal:
			for(int sample(0) ; sample < samples ; ++sample)
				Work(l[sample], r[sample]);
			break;
		case swapped:
			for(int sample(0) ; sample < samples ; ++sample)
				Work(r[sample], l[sample]);
			break;
		case mono:
			for(int sample(0) ; sample < samples ; ++sample)
			{
				Work(l[sample], r[sample]);
				l[sample] = r[sample] = l[sample] + r[sample];
			}
			break;
		default:
			throw Exception("bad setting for the channel mix parameter");
	}
}

inline void Haas::Work(Sample & left, Sample & right)
{
	const Real mono_input(left + right);
	*buffer_iterators_[direct_first] = mono_input;
	left = static_cast<Sample>
	(
		overall_gain_dry * left +
		overall_gain_wet *
		(
			direct_gain_left           * *buffer_iterators_[direct_left] +
			early_reflection_gain_left * *buffer_iterators_[early_reflection_left] +
			late_reflection_gain_left  * *buffer_iterators_[late_reflection]
		)
	);
	right = static_cast<Sample>
	(
		overall_gain_dry * right +
		overall_gain_wet *
		(
			direct_gain_right           * *buffer_iterators_[direct_right] +
			early_reflection_gain_right * *buffer_iterators_[early_reflection_right] +
			late_reflection_gain_right  * *buffer_iterators_[late_reflection]
		)
	);
	for(int stage(0) ; stage < stages ; ++stage)
		if(++buffer_iterators_[stage] == buffer_.end())
			buffer_iterators_[stage] = buffer_.begin();
}

}}}
