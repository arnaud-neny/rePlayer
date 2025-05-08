// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2003-2007 johan boule <bohan@jabber.org>
// copyright 2003-2007 psycledelics http://psycle.pastnotecut.org

/// \file
/// \brief just a multiplier
#include "plugin.hpp"
namespace psycle { namespace plugin { namespace Gainer {

class Gainer : public Plugin
{
	public:

		/*override*/ void help(std::ostream & out) throw()
		{
			out << "just a multiplier" << std::endl;
		}

		enum Parameters { gain };

		static const Information & information() throw()
		{
			static bool initialized = false;
			static Information *info = NULL;
			if (!initialized) {
				static const Information::Parameter parameters [] =
				{
					Information::Parameter::Parameter("gain", std::exp(-4.), 1, std::exp(+4.), Information::Parameter::exponential::on)
				};
				static Information information(0x0110, Information::Types::effect, "ayeternal Gainer", "Gainer", "bohan", 1, parameters, sizeof parameters / sizeof *parameters);
				info = &information;
				initialized = true;
			}
			return *info;
		}

		/*override*/ void describe(std::ostream & out, const int & parameter) const
		{
			switch(parameter)
			{
				case gain:
					out << std::setprecision(3) << std::setw(6) << gain_;
					if(gain_) out << " (" << std::setw(6) << 20 * std::log10(gain_) << " dB)";
					break;
				default:
					Plugin::describe(out, parameter);
			}
		}

		Gainer() : Plugin(information()) {}

		/*override*/ void parameter(const int & parameter)
		{
			switch(parameter)
			{
				case gain:
					gain_ = (*this)[gain] ? (*this)(gain) : 0;
					break;
			}
		}

		/*override*/ void Work(Sample l[], Sample r[], int sample, int)
		{
			while(sample--)
			{
				Work(l[sample]);
				Work(r[sample]);
			}
		}

	protected:

		inline void Work(Sample & sample)
		{
			sample *= gain_;
		}

		Real gain_;
};

PSYCLE__PLUGIN__INSTANTIATOR("gainer", Gainer)

}}}
