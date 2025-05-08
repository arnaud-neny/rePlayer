// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2003-2011 members of the psycle project http://psycle.sourceforge.net ; Arguru, johan boule <bohan@jabber.org>, JosepMa

/// \file
/// \brief filter in the frequency domain using 2 poles
#include "plugin.hpp"
#include <psycle/helpers/math.hpp>
namespace psycle { namespace plugin { namespace Filter_2_Poles {

using namespace helpers::math;

class Filter_2_Poles : public Plugin
{
public:
	/*override*/ void help(std::ostream & out) throw()
	{
		out <<
			"Filter in the frequency domain using 2 poles\n"
			"\n"
			"The cutoff frequency is modulated in an unsusal way since the LFO is not a sine.\n"
			"This behavior of the modulation is the same as in Arguru's original code, did he do it intentionally or not.\n"
			"\n"
			"Commands:\n"
			"01 XX: sets the cutoff frequency modulation phase - XX in the range 00 to ff\n";
	}

	enum Parameters
	{
		response, cutoff_frequency, resonance,
		modulation_sequencer_ticks, modulation_amplitude, modulation_stereo_dephase
	};

	enum Response_Type { low, high };

	static const Information & information() throw()
	{
		static bool initialized = false;
		static Information *info = NULL;
		if (!initialized) {
			static const Information::Parameter parameters [] =
			{
				Information::Parameter::Parameter("response", low, high, Information::Parameter::discrete::on),
				/// note: The actual cutoff frequence really is in the range 15Hz to 22050Hz,
				///       but we premultiply it by pi here to spare us from doing the multiplication when recomputing cutoff_sin_.
				Information::Parameter::Parameter("cutoff frequency", 15 * pi, 22050 * pi, 22050 * pi, Information::Parameter::exponential::on),
				Information::Parameter::Parameter("resonance", 0, 0, 1, Information::Parameter::linear::on),
				Information::Parameter::Parameter("mod. period", pi * 2 / 10000, 0, pi * 2 * 2 * 3 * 4 * 5 * 7, Information::Parameter::exponential::on),
				Information::Parameter::Parameter("mod. amplitude", 0, 0, 1, Information::Parameter::linear::on),
				Information::Parameter::Parameter("mod. stereodephase", 0, 0, pi, Information::Parameter::linear::on)
			};
			static Information information(0x0110, Information::Types::effect, "ayeternal 2-Pole Filter", "2-Pole Filter", "bohan", 2, parameters, sizeof parameters / sizeof *parameters);
			info = &information;
			initialized = true;
		}
		return *info;
	}

	/*override*/ void describe(std::ostream & out, const int & parameter) const
	{
		switch(parameter)
		{
		case response:
			switch((*this)[response])
			{
			case low:
				out << "low";
				break;
			case high:
				out << "high";
				break;
			default:
				throw std::string("unknown response type");
			}
			break;
		case cutoff_frequency:
			out << (*this)(cutoff_frequency) / (2*pi) << " Hertz";
			break;
		case modulation_sequencer_ticks:
			out << pi * 2 / (*this)(modulation_sequencer_ticks) << " ticks (lines)";
			break;
		case modulation_amplitude:
			out << "(+/-) " << (*this)(modulation_amplitude) * 7276 << " Hertz";
			break;
		case modulation_stereo_dephase:
			if((*this)(modulation_stereo_dephase) == 0) out << 0;
			else if((*this)(modulation_stereo_dephase) == Sample(pi)) out << "pi";
			else out << "pi / " << pi / (*this)(modulation_stereo_dephase);
			break;
		default:
			Plugin::describe(out, parameter);
		}
	}

	Filter_2_Poles() : Plugin(information()), modulation_phase_(0), cutoff_sin_(0)
	{
		std::memset(buffers_, 0, sizeof buffers_);
	}

	/*override*/ void Work(Sample l[], Sample r[], int samples, int);
	/*override*/ void parameter(const int &);
protected:
	/*override*/ void SeqTick(int, int, int, int command, int value);
	/*override*/ void samples_per_second_changed() { parameter(cutoff_frequency); }
	/*override*/ void sequencer_ticks_per_second_changed() { parameter(modulation_sequencer_ticks); }
	static const int poles = 2;
	inline void update_coefficients();
	inline void update_coefficients(Real coefficients[poles + 1], const Real & modulation_stereo_dephase = 0);
	enum Channels { left, right, channels };
	inline const Sample Work(const Real & input, Real buffer[channels], const Real coefficients[poles + 1]);
	Real cutoff_sin_, modulation_radians_per_sample_, modulation_phase_, buffers_ [channels][poles], coefficients_ [channels][poles + 1];
};

PSYCLE__PLUGIN__INSTANTIATOR("filter-2-poles", Filter_2_Poles)

void Filter_2_Poles::SeqTick(const int note, const int, const int, const int command, const int value)
{
	switch(command)
	{
	case 1:
		modulation_phase_ = pi * 2 * value / 0x100;
		break;
	}
	if ( note <= psycle::plugin_interface::NOTE_MAX )
	{
		///\todo: set cutoff_frequency by note.
		//parameter(cutoff_frequency,pow(2.0, (float)(note-18)/12.0));
	}
}

void Filter_2_Poles::parameter(const int & parameter)
{
	switch(parameter)
	{
	case cutoff_frequency:
		cutoff_sin_ = static_cast<Sample>(std::sin((*this)(cutoff_frequency) * seconds_per_sample()));
		break;
	case modulation_sequencer_ticks:
		modulation_radians_per_sample_ = (*this)(modulation_sequencer_ticks) / samples_per_sequencer_tick();
	}
	update_coefficients();
}

void Filter_2_Poles::update_coefficients()
{
	update_coefficients(coefficients_[left]);
	update_coefficients(coefficients_[right], (*this)(modulation_stereo_dephase));
}

inline void Filter_2_Poles::update_coefficients(Real coefficients[poles + 1], const Real & modulation_stereo_dephase)
{
	// Notes:
	// The cutoff frequency is modulated in an unsusal way since the LFO is not added directly to the cutoff frequency,
	// but to a computed sine (cutoff_sin_).
	// This behavior of the modulation is the same as in Arguru's original code, did he do it intentionally or not.
	// If the modulation amp is "too big", the LFO will get stuck for a long time at SR/2.
	// This is again due to the sum of the two sines (the base freq + the LFO) giving a range [0, 2] but being then clamped to the ]0, 1] range.

	const Real minimum(Real(1e-2));
	const Real maximum(1 - minimum);
	coefficients[0] = clip(minimum, static_cast<Real>(cutoff_sin_ + (*this)(modulation_amplitude) * std::sin(modulation_phase_ + modulation_stereo_dephase)), maximum);
	coefficients[1] = 1 - coefficients[0];
	coefficients[2] = (*this)(resonance) * (1 + 1 / coefficients[1]);
	erase_all_nans_infinities_and_denormals(coefficients, poles + 1);
}

void Filter_2_Poles::Work(Sample l[], Sample r[], int samples, int)
{
	switch((*this)[response]) {
		case low:
			for(int sample = 0; sample < samples ; ++sample) {
				l[sample] = Work(l[sample], buffers_[left] , coefficients_[left]);
				r[sample] = Work(r[sample], buffers_[right], coefficients_[right]);
			}
			break;
		case high:
			for(int sample = 0; sample < samples ; ++sample) {
				l[sample] -= Work(l[sample], buffers_[left] , coefficients_[left]);
				r[sample] -= Work(r[sample], buffers_[right], coefficients_[right]);
			}
			break;
		default:
			throw Exception("unknown response type");
	}

	erase_all_nans_infinities_and_denormals(buffers_[left], channels);
	erase_all_nans_infinities_and_denormals(buffers_[right], channels);

	if((*this)(modulation_amplitude)) // note: this would be done each sample for perfect quality
	{
		modulation_phase_ = std::fmod(modulation_phase_ + modulation_radians_per_sample_ * samples, pi * 2);
		update_coefficients();
	}
}

inline const Filter_2_Poles::Sample Filter_2_Poles::Work(const Real & input, Real buffer[poles], const Real coefficients[poles + 1])
{
	buffer[0] = coefficients[1] * buffer[0] + coefficients[0] * (input + coefficients[2] * (buffer[0] - buffer[1]));
	buffer[1] = coefficients[1] * buffer[1] + coefficients[0] * buffer[0];
	return static_cast<Sample>(buffer[1]);
}

}}}
