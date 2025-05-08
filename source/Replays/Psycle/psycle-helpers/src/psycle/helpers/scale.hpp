// This program is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// copyright 2004-2009 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\interface scale facilities for machine parameters

#ifndef PSYCLE__HELPERS__SCALE__INCLUDED
#define PSYCLE__HELPERS__SCALE__INCLUDED
#pragma once

#include <cmath>

namespace psycle { namespace helpers {

class Scale {
	public:
		typedef double Real;
		Scale(const Real & input_maximum, const Real & output_minimum, const Real & output_maximum)
		: input_maximum_(input_maximum), output_minimum_(output_minimum), output_maximum_(output_maximum)
		{}
		virtual inline ~Scale() throw() {}
        virtual Real apply(const Real & sample) const = 0;
        virtual Real apply_inverse(const Real & sample) const = 0;
		inline const Real & input_maximum() const throw() { return input_maximum_; }
		inline const Real & output_minimum() const throw() { return output_minimum_; }
		inline const Real & output_maximum() const throw() { return output_maximum_; }
	private:
		const Real input_maximum_, output_minimum_, output_maximum_;
};

namespace scale {

class Discrete : public Scale {
	public:
		Discrete(const Real & input_maximum)
		: Scale(input_maximum, 0, input_maximum)
		{}
        inline virtual Real apply(const Real & sample) const
		{
			return sample;
		}
        inline virtual Real apply_inverse(const Real & sample) const
		{
			return sample;
		}
};

class Linear : public Scale {
	public:
		Linear(const Real & input_maximum, const Real & output_minimum, const Real & output_maximum)
		: Scale(input_maximum, output_minimum, output_maximum)
		{
			ratio_ = (output_maximum - output_minimum) / input_maximum;
			offset_ = output_minimum;
		}
        inline virtual Real apply(const Real & sample) const
		{
			return sample * ratio_ + offset_;
		}
        inline virtual Real apply_inverse(const Real & sample) const
		{
			return (sample - offset_) / ratio_;
		}
	private:
		Real offset_, ratio_;
};

class Exponential : public Scale {
	public:
		Exponential(const Real & input_maximum, const Real & output_minimum, const Real & output_maximum)
		: Scale(input_maximum, static_cast<Real>(std::log(output_minimum)), static_cast<Real>(std::log(output_maximum)))
		{
			ratio_ = (this->output_maximum() - this->output_minimum()) / input_maximum;
			offset_ = this->output_minimum() / ratio_;
        }
        inline virtual Real apply(const Real & sample) const
		{
			return static_cast<Real>(std::exp((sample + offset_) * ratio_));
		}
        inline virtual Real apply_inverse(const Real & sample) const
		{
			return static_cast<Real>(std::log(sample) / ratio_ - offset_);
		}
	private:
		Real offset_, ratio_;
};

class Logarithmic : public Scale {
	public:
		Logarithmic(const Real & input_maximum, const Real & output_minimum, const Real & output_maximum)
		: Scale(input_maximum, static_cast<Real>(std::log(output_minimum)), static_cast<Real>(std::log(output_maximum)))
		{
			ratio_ = (this->output_maximum() - this->output_minimum()) / input_maximum;
			offset_ = this->output_minimum() / ratio_;
			offset_plus_input_maximum_ = offset_ + input_maximum;
			output_minimum_maximum_sum_ = output_minimum + output_maximum;
        }
        inline virtual Real apply(const Real & sample) const
		{
			return static_cast<Real>(output_minimum_maximum_sum_ - std::exp((offset_plus_input_maximum_ - sample) * ratio_));
		}
        inline virtual Real apply_inverse(const Real & sample) const
		{
			return static_cast<Real>(offset_plus_input_maximum_ - std::log(output_minimum_maximum_sum_ - sample) / ratio_);
		}
	private:
		Real offset_, ratio_, output_minimum_maximum_sum_, offset_plus_input_maximum_;
};

}}}
#endif
