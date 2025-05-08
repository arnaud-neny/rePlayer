// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2003-2007 psycledelics http://psycle.pastnotecut.org

///\file
///\brief delay modulated by a sine
#include "plugin.hpp"
#include <psycle/helpers/math.hpp>
#include <cassert>
#include <vector>
namespace psycle { namespace plugin { namespace Flanger {

using namespace helpers::math;

class Flanger : public Plugin {
	public:
		virtual void help(std::ostream & out) throw() {
			out << "delay modulated by a sine" << std::endl;
			out << "compatible with original psycle 1 arguru's flanger" << std::endl;
			out << std::endl;
			out << "commands:" << std::endl;
			out << "0x01 0x00-0xff: delay length modulation phase" << std::endl;
		}

		enum Parameters {
			delay,
			modulation_amplitude, modulation_radians_per_second, modulation_stereo_dephase,
			interpolation,
			dry, wet,
			left_feedback, right_feedback,
		};

		enum Interpolation { no, yes };
		
		static const Information & information() throw() {
			static bool initialized = false;
			static Information *info = NULL;
			if (!initialized) {
				static const Information::Parameter parameters [] = {
					Information::Parameter::Parameter("central delay", 0, 0, 0.1, Information::Parameter::linear::on),
					Information::Parameter::Parameter("mod. amplitude", 0, 0, 1, Information::Parameter::linear::on),
					Information::Parameter::Parameter("mod. frequency", 0.0001 * pi * 2, 0, 100 * pi * 2, Information::Parameter::exponential::on),
					Information::Parameter::Parameter("mod. stereodephase", 0, 0, pi, Information::Parameter::linear::on),
					Information::Parameter::Parameter("interpolation", yes, yes, Information::Parameter::discrete::on),
					Information::Parameter::Parameter("dry", -1, 1, 1, Information::Parameter::linear::on),
					Information::Parameter::Parameter("wet", -1, 0, 1, Information::Parameter::linear::on),
					Information::Parameter::Parameter("feedback left", -1, 0, 1, Information::Parameter::linear::on),
					Information::Parameter::Parameter("feedback right", -1, 0, 1, Information::Parameter::linear::on),
				};
				static Information information(0x0110, Information::Types::effect, "ayeternal Flanger", "Flanger", "jaz, bohan, and the psycledelics community", 2, parameters, sizeof parameters / sizeof *parameters);
				info = &information;
				initialized = true;
			}
			return *info;
		}

		virtual void describe(std::ostream & out, const int & parameter) const {
			switch(parameter) {
				case interpolation:
					switch((*this)[interpolation]) {
						case no:
							out << "no";
							break;
						case yes:
							out << "yes";
							break;
						default:
							out << "yes or no ???";
					}
					break;
				case delay:
					out << (*this)(delay) << " seconds";
					break;
				case modulation_radians_per_second:
					out << (*this)(modulation_radians_per_second) / pi / 2 << " hertz";
					break;
				case modulation_amplitude:
						out << (*this)(delay) * (*this)(modulation_amplitude) << " seconds";
						break;
				case modulation_stereo_dephase:
					if((*this)(modulation_stereo_dephase) == 0) out << 0;
					else if((*this)(modulation_stereo_dephase) == Sample(pi)) out << "pi";
					else out << "pi / " << pi / (*this)(modulation_stereo_dephase);
					break;
				case left_feedback:
				case right_feedback:
				case dry:
				case wet:
					if(std::fabs((*this)(parameter)) < 2e-5) {
						out << 0;
						break;
					}
				default:
					Plugin::describe(out, parameter);
			}
		}

		Flanger() : Plugin(information()), modulation_phase_(0) {
			for(int channel(0) ; channel < channels ; ++channel)
				writes_[channel] = 0;
		}
		virtual inline ~Flanger() throw() {}
		virtual void init();
		virtual void Work(Sample l [], Sample r [], int samples, int);
		virtual void parameter(const int &);

	protected:
		virtual void sequencer_note_event(const int, const int, const int, const int command, const int value);
		virtual void samples_per_second_changed();
		inline void process(sinseq<true> &, std::vector<Real> & buffer, int & write, Sample input [], const int & samples, const Real & feedback) throw();
		inline void resize(const Real & delay);
		enum Channels { left, right, channels };
		std::vector<Real> buffers_[channels];
		int delay_in_samples_, writes_[channels];
		Real modulation_amplitude_in_samples_, modulation_radians_per_sample_, modulation_phase_;
		sinseq<true> sin_sequences_[channels];
};

PSYCLE__PLUGIN__INSTANTIATOR("flanger", Flanger)

void Flanger::init() {
	resize(0); // resizes the buffers not to 0, but to 1, the smallest length possible for the algorithm to work
}

void Flanger::sequencer_note_event(const int, const int, const int, const int command, const int value) {
	switch(command) {
		case 1:
			modulation_phase_ = value * pi * 2 / 0x100;
			sin_sequences_[left](modulation_phase_, modulation_radians_per_sample_);
			sin_sequences_[right](modulation_phase_ + (*this)(modulation_stereo_dephase), modulation_radians_per_sample_);
			break;
		default: ;
	}
}

void Flanger::samples_per_second_changed() {
	parameter(modulation_radians_per_second);
}

void Flanger::parameter(const int & parameter) {
	switch(parameter) {
			case delay:
			case modulation_amplitude:
				resize((*this)(delay));
				break;
			case modulation_stereo_dephase:
			case modulation_radians_per_second:
				modulation_radians_per_sample_ = (*this)(modulation_radians_per_second) * seconds_per_sample();
				sin_sequences_[left](modulation_phase_, modulation_radians_per_sample_);
				sin_sequences_[right](modulation_phase_ + (*this)(modulation_stereo_dephase), modulation_radians_per_sample_);
				break;
			case interpolation:
				switch((*this)[interpolation]) {
					case no:
					case yes:
						break;
					default:
						throw Exception("interpolation must be yes or no");
				}
				break;
			default: ;
		}
}

inline void Flanger::resize(const Real & delay) {
	delay_in_samples_ = static_cast<int>(delay * samples_per_second());
	modulation_amplitude_in_samples_ = (*this)(modulation_amplitude) * delay_in_samples_;
	for(int channel(0) ; channel < channels ; ++channel) {
		buffers_[channel].resize(1 + delay_in_samples_ + static_cast<int>(std::ceil(modulation_amplitude_in_samples_)), 0);
			// resizes the buffer at least to 1, the smallest length possible for the algorithm to work
		writes_[channel] %= buffers_[channel].size();
	}
	assert(0 <= modulation_amplitude_in_samples_);
	assert(modulation_amplitude_in_samples_ <= delay_in_samples_);
}

void Flanger::Work(Sample l[], Sample r[], int samples, int) {
	process(sin_sequences_[left], buffers_[left] , writes_[left] , l, samples, (*this)(left_feedback));
	process(sin_sequences_[right], buffers_[right], writes_[right], r, samples, (*this)(right_feedback));
	modulation_phase_ = std::fmod(modulation_phase_ + modulation_radians_per_sample_ * samples, pi * 2);
}

inline void Flanger::process(
	sinseq<true> & sinseq, std::vector<Real> & buffer,
	int & write, Sample input [], const int & samples, const Real & feedback
) throw() {
	const int size(static_cast<int>(buffer.size()));
	switch((*this)[interpolation]) {
		case yes:
			for(int sample(0) ; sample < samples ; ++sample) {
				const Real sin(sinseq()); // [bohan] this uses 64-bit floating point numbers or else accuracy is not sufficient
				
				#if 1
					Real fraction_part = modulation_amplitude_in_samples_ * sin;
					int integral_part = static_cast<int>(fraction_part);
					fraction_part = fraction_part - integral_part;
					if(fraction_part < 0) { fraction_part += 1; integral_part -= 1; }
				#else				
					Real integral_part(0);
					Real fraction_part = std::modf(modulation_amplitude_in_samples_ * sin,&integral_part);
					if(fraction_part < 0) fraction_part = 1.0 + fraction_part;
				#endif
				
				int read = write - delay_in_samples_ + integral_part;
				int next_value = read + 1;
				if(read < 0) {
					read += size;
					if(next_value < 0) next_value += size;
				}
				else if(next_value >= size) {
					next_value -= size;
					if(read >= size) read -= size;
				}

				const Real buffer_read = buffer[read] * ( 1.0 - fraction_part) + buffer[next_value] * fraction_part;
				
				Sample & input_sample = input[sample];
				
				#if 1
					Sample buffer_write = input_sample + feedback * buffer_read + 1e-9;
					buffer_write-= static_cast<Sample>(1e-9);
				#else
					Sample buffer_write = input_sample + feedback * buffer_read;
					erase_all_nans_infinities_and_denormals(buffer_write);
				#endif
				
				buffer[write] = buffer_write;
				++write %= size;
				input_sample *= (*this)(dry);
				input_sample += (*this)(wet) * buffer_read;
				#if 0
					erase_all_nans_infinities_and_denormals(input_sample);
				#endif
			}
			break;
		case no:
		default:
				for(int sample(0) ; sample < samples ; ++sample) {
					#if 0
						// test without optimized sine sequence...
						Real sin;
						if(&sin_sequence == &sin_sequences_[left])
							sin = std::sin(modulation_phase_);
						else
							sin = std::sin(modulation_phase_ + (*this)(modulation_stereo_dephase));
					#else
						const Real sin(sinseq()); // <bohan> this uses 64-bit floating point numbers or else accuracy is not sufficient
					#endif
					

					assert(-1 <= sin);
					assert(sin <= 1);
					assert(0 <= write);
					assert(write < static_cast<int>(buffer.size()));
					assert(0 <= modulation_amplitude_in_samples_);
					assert(modulation_amplitude_in_samples_ <= delay_in_samples_);

					int read;
					
					#if 0
						if(sin < 0) read = write - delay_in_samples_ + static_cast<int>(modulation_amplitude_in_samples_ * sin);
						else read = write - delay_in_samples_ + static_cast<int>(modulation_amplitude_in_samples_ * sin) - 1;
					#elif 0
						read = write - delay_in_samples_ + std::floor(modulation_amplitude_in_samples_ * sin);
					#else
						int integral_part = static_cast<int>(modulation_amplitude_in_samples_ * sin);
						read = write - delay_in_samples_ + integral_part;
					#endif

					if(read < 0) read += size; else if(read >= size) read -= size;

					assert(0 <= read);
					assert(read < static_cast<int>(buffer.size()));

					const Real buffer_read(buffer[read]);
					Sample & input_sample = input[sample];
					
					#if 1
						Sample buffer_write = input_sample + feedback * buffer_read + 1e-9;
						buffer_write -= static_cast<Sample>(1e-9);
					#else
						Sample buffer_write = input_sample + feedback * buffer_read;
						erase_all_nans_infinities_and_denormals(buffer_write);
					#endif
					
					buffer[write] = buffer_write;
					++write %= size;
					input_sample *= (*this)(dry);
					input_sample += (*this)(wet) * buffer_read;
					#if 0
						erase_all_nans_infinities_and_denormals(input_sample);
					#endif
				}
	}
}

}}}
