// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2009 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#include <psycle/core/detail/project.private.hpp>
#include "convert_internal_machines.private.hpp"
#include "machinefactory.h"
#include "internalkeys.hpp"
#include "fileio.h"
#include "plugin.h"
#include "player.h"
#include "song.h"
#include <psycle/helpers/scale.hpp>
#include <psycle/helpers/math.hpp>

namespace psycle { namespace core { namespace convert_internal_machines {

namespace math = psycle::helpers::math;
namespace scale = psycle::helpers::scale;
typedef psycle::helpers::Scale::Real Real;

Converter::Converter() {}

Converter::~Converter() throw() {
	for(
		std::map<Machine *, int const *>::const_iterator i = machine_converted_from.begin();
		i != machine_converted_from.end(); ++i
	) delete const_cast<int *>(i->second);
}

Machine & Converter::redirect(MachineFactory & factory, int index, int type, RiffFile & riff) {
	Machine * pointer_to_machine = factory.CreateMachine(MachineKey(Hosts::NATIVE,(plugin_names()(type).c_str()),0),index);
	if(!pointer_to_machine) pointer_to_machine = factory.CreateMachine(InternalKeys::dummy,index);
	try {
		Machine & machine = *pointer_to_machine;
		machine_converted_from[&machine] = new int(type);
		machine.Init();
		//BOOST_STATIC_ASSERT(sizeof(int) == 4);
		{
			char c[16];
			riff.ReadArray(c, 16); c[15] = 0;
			machine.SetEditName(c);
		}
		switch(type) {
			case abass:
			case asynth1:
			case asynth2:
			case asynth21:
			{
				Plugin & plug = *((Plugin*)pointer_to_machine);
				int numParameters = 0;
				riff.Read(numParameters);
				if(plug.proxy()()) {
					int32_t * Vals = new int32_t[numParameters];
					riff.ReadArray(Vals, numParameters);
					try {
						if(type == abass) {
							plug.proxy().ParameterTweak(0,Vals[0]);
							for(int i(1); i < 15; ++i) plug.proxy().ParameterTweak(i+4,Vals[i]);
							plug.proxy().ParameterTweak(19,0);
							plug.proxy().ParameterTweak(20,Vals[15]);
							if(numParameters > 16) {
								plug.proxy().ParameterTweak(24,Vals[16]);
								plug.proxy().ParameterTweak(25,Vals[17]);
							} else {
								plug.proxy().ParameterTweak(24,0);
								plug.proxy().ParameterTweak(25,0);
							}
						} else {
							for(int i(0); i < numParameters; ++i) plug.proxy().ParameterTweak(i,Vals[i]);
							if(type == asynth1) { // Patch to replace Synth1 by Arguru Synth 2f
								plug.proxy().ParameterTweak(17,Vals[17]+10);
								plug.proxy().ParameterTweak(24,0);
								plug.proxy().ParameterTweak(25,0);
							} else if(type == asynth2) {
								plug.proxy().ParameterTweak(24,0);
								plug.proxy().ParameterTweak(25,0);
							}
						}
					} catch(const std::exception &) {
						//loggers::warning(UNIVERSALIS__COMPILER__LOCATION);
					}
					try {
						int size = plug.proxy().GetDataSize();
						//pFile->Read(size); // This would have been the right thing to do
						if(size) {
							char * pData = new char[size];
							riff.ReadArray(pData, size); // Number of parameters
							try {
								plug.proxy().PutData(pData); // Internal load
							} catch(const std::exception &) {
								//loggers::warning(UNIVERSALIS__COMPILER__LOCATION);
							}
							delete[] pData;
						}
					} catch(std::exception const &) {
						// loggers::warning(UNIVERSALIS__COMPILER__LOCATION);
					}
					delete[] Vals;
				} else {
					riff.Skip(4*numParameters);
					// If the machine had custom data (PutData()) the loader will crash.
					// It cannot be fixed.
				}
			}
			default: ;
		}

		riff.ReadArray(machine._inputMachines,MAX_CONNECTIONS);
		riff.ReadArray(machine._outputMachines,MAX_CONNECTIONS);
		riff.ReadArray(machine._inputConVol,MAX_CONNECTIONS);
		riff.ReadArray(machine._connection,MAX_CONNECTIONS);
		riff.ReadArray(machine._inputCon,MAX_CONNECTIONS);
		riff.Skip(96); // ConnectionPoints, 12*8bytes
		riff.Read(machine._connectedInputs);
		riff.Read(machine._connectedOutputs);
		int32_t panning = 0;
		riff.Read(panning);
		machine.SetPan(panning);
		riff.Skip(40); // skips shiatz
		switch(type) {
			case delay:
				{
					const int nparams = 2;
					int32_t parameters [nparams]; riff.ReadArray(parameters,nparams);
					retweak(machine, type, parameters, nparams, 5);
				}
				break;
			case flanger:
				{
					const int nparams = 2;
					int32_t parameters [nparams]; riff.ReadArray(parameters, nparams);
					retweak(machine, type, parameters, nparams, 7);
				}
				break;
			case gainer:
				riff.Skip(4);
				{
					const int nparams = 1;
					int32_t parameters [nparams]; riff.ReadArray(parameters, nparams);
					/*if(type == gainer)*/ retweak(machine, type, parameters, nparams);
				}
				break;
			default:
				riff.Skip(8);
		}
		switch(type) {
			case distortion:
			{
				const int nparams=4;
				int32_t parameters [nparams]; riff.ReadArray(parameters, nparams);
				retweak(machine, type, parameters, nparams);
				break;
			}
			default:
				riff.Skip(16);
		}
		switch(type) {
			case ring_modulator:
				{
					const int nparams=4;
					uint8_t parameters [nparams];
					riff.Read(parameters[0]);
					riff.Read(parameters[1]);
					riff.Skip(1);
					riff.Read(parameters[2]);
					riff.Read(parameters[3]);
					retweak(machine, type, parameters, nparams);
				}
				riff.Skip(40);
				break;
			case delay:
				riff.Skip(5);
				{
					const int nparams=4;
					int32_t parameters [nparams];
					riff.Read(parameters[0]);
					riff.Read(parameters[2]);
					riff.Read(parameters[1]);
					riff.Read(parameters[3]);
					retweak(machine, type, parameters, nparams);
				}
				riff.Skip(24);
				break;
			case flanger:
				riff.Skip(4);
				{
					const int nparams=1;
					unsigned char parameters [nparams]; riff.ReadArray(parameters, nparams);
					retweak(machine, type, parameters, nparams, 9);
				}
				{
					int parameters [6];
					riff.Read(parameters[0]);
					riff.Skip(4);
					riff.Read(parameters[3]);
					riff.Read(parameters[5]);
					riff.Skip(8);
					riff.Read(parameters[2]);
					riff.Read(parameters[1]);
					riff.Read(parameters[4]);
					retweak(machine, type, parameters, sizeof parameters / sizeof *parameters);
				}
				riff.Skip(4);
				break;
			case filter_2_poles:
				riff.Skip(21);
				{
					const int nparams=6;
					int32_t parameters [nparams];
					riff.ReadArray(&parameters[1], nparams-1);
					riff.Read(parameters[0]);
					retweak(machine, type, parameters, nparams);
				}
				break;
			default:
				riff.Skip(45);
		}
		return machine;
	}
	catch(...) {
		delete pointer_to_machine;
		throw;
	}
}

void Converter::retweak(CoreSong & /*song*/) const {
#if 0 ///\todo
	// Get the first category (there's only one with imported psy's) and...
	std::vector<PatternCategory*>::iterator cit  = song.sequence().patternPool()->begin();
	// ... for all the patterns in this category...
	for(std::vector<Pattern*>::iterator pit  = (*cit)->begin(); pit != (*cit)->end(); ++pit) {
		// ... check all lines searching...
		for(std::map<double, PatternLine>::iterator lit = (*pit)->begin(); lit != (*pit)->end() ; ++lit) {
			PatternLine & line = lit->second;
			// ...tweaks to modify.
			for(std::map<int, PatternEvent>::iterator tit = line.tweaks().begin(); tit != line.tweaks().end() ; ++tit) {
				// If this tweak is for a replaced machine, modify the values.
				std::map<Machine * const, const int *>::const_iterator i(machine_converted_from.find(song.machine(tit->second.machine())));
				if(i != machine_converted_from.end()) {
					//Machine & machine(*i->first);
					int const & type(*i->second);
					int parameter(tit->second.instrument());
					int value((tit->second.command() << 8) + tit->second.parameter());
					retweak(type, parameter, value);
					tit->second.setInstrument(parameter);
					tit->second.setCommand(value>>8);
					tit->second.setParameter(value&0xff);
				}
			}
		}
	}
#endif
}

Converter::Plugin_Names::Plugin_Names() {
	(*this)[ring_modulator] = new std::string("ring-modulator");
	(*this)[distortion] = new std::string("distortion");
	(*this)[delay] = new std::string("delay");
	(*this)[filter_2_poles] = new std::string("filter-2-poles");
	(*this)[gainer] = new std::string("gainer");
	(*this)[flanger] = new std::string("flanger");
	(*this)[abass] = new std::string("arguru-synth-2f");
	(*this)[asynth1] = new std::string("arguru-synth-2f");
	(*this)[asynth2] = new std::string("arguru-synth-2f");
	(*this)[asynth21] = new std::string("arguru-synth-2f");
}

Converter::Plugin_Names::~Plugin_Names() {
	delete (*this)[ring_modulator];
	delete (*this)[distortion];
	delete (*this)[delay];
	delete (*this)[filter_2_poles];
	delete (*this)[gainer];
	delete (*this)[flanger];
	delete (*this)[abass];
	delete (*this)[asynth1];
	delete (*this)[asynth2];
	delete (*this)[asynth21];
}

bool Converter::Plugin_Names::exists(int type) const throw() {
	return find(type) != end();
}

std::string const & Converter::Plugin_Names::operator()(int type) const {
	const_iterator i = find(type);
	//if(i == end()) throw std::exception("internal machine replacement plugin not declared");
	if(i == end()) throw;
	return *i->second;
}

const Converter::Plugin_Names & Converter::plugin_names() {
	static const Plugin_Names plugin_names;
	return plugin_names;
}

template<typename Parameter>
void Converter::retweak(Machine & machine, int type, Parameter parameters [], int parameter_count, int parameter_offset) {
	for(int parameter(0); parameter < parameter_count ; ++parameter) {
		int new_parameter(parameter_offset + parameter);
		int new_value(parameters[parameter]);
		retweak(type, new_parameter, new_value);
		machine.SetParameter(new_parameter, new_value);
	}
}

void Converter::retweak(int type, int & parameter, int & integral_value) const {
	Real value(integral_value);
	const Real maximum(0xffff);
	switch(type) {
		case gainer:
			{
				///\todo: This "if" had to be added to avoid a crash when pattern contains erroneous data
				/// (which can happen, since the user can write any value in the pattern)
				/// More checks should be added where having values out of range can cause bad behaviours.
				if(parameter != 1) break;
				enum Parameters { gain };
				static const int parameters [] = { gain };
				parameter = parameters[parameter-1];
				switch(parameter) {
					case gain:
						if( value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, exp(-4.), exp(+4.)).apply_inverse(value / 0x100);
						break;
				}
			}
			break;
		case distortion:
			{
				enum Parameters { input_gain, output_gain, positive_threshold, positive_clamp, negative_threshold, negative_clamp, symmetric };
				static const int parameters [] = { positive_threshold, positive_clamp, negative_threshold, negative_clamp };
				parameter = parameters[parameter-1];
				switch(parameter) {
					case negative_threshold:
					case negative_clamp:
					case positive_threshold:
					case positive_clamp:
						value *= maximum / 0x100;
						break;
				}
			}
			break;
		case delay:
			{
				enum Parameters { dry, wet, left_delay, left_feedback, right_delay, right_feedback };
				static const int parameters [] = { left_delay, left_feedback, right_delay, right_feedback, dry, wet };
				parameter = parameters[parameter-1];
				switch(parameter) {
					case left_delay:
					case right_delay:
						value *= Real(2 * 3 * 4 * 5 * 7) / Player::singleton().timeInfo().samplesPerTick();
						break;
					case left_feedback:
					case right_feedback:
						value = (100 + value) * maximum / 200;
						break;
					case dry:
					case wet:
						value = (0x100 + value) * maximum / 0x200;
						break;
				}
			}
			break;
		case flanger:
			{
				enum Parameters { delay, modulation_amplitude, modulation_radians_per_second, modulation_stereo_dephase, interpolation, dry, wet, left_feedback, right_feedback };
				static const int parameters [] = { delay, modulation_amplitude, modulation_radians_per_second, left_feedback, modulation_stereo_dephase, right_feedback, dry, wet, interpolation };
				parameter = parameters[parameter-1];
				switch(parameter) {
					case delay:
						value *= maximum / 0.1 / Player::singleton().timeInfo().sampleRate();
						break;
					case modulation_amplitude:
					case modulation_stereo_dephase:
						value *= maximum / 0x100;
						break;
					case modulation_radians_per_second:
						if(value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, 0.0001 * math::pi * 2, 100 * math::pi * 2).apply_inverse(value * 3e-9 * Player::singleton().timeInfo().sampleRate());
						break;
					case left_feedback:
					case right_feedback:
						value = (100 + value) * maximum / 200;
						break;
					case dry:
					case wet:
						value = (0x100 + value) * maximum / 0x200;
						break;
					case interpolation:
						value = value != 0;
						break;
				}
			}
			break;
		case filter_2_poles:
			{
				enum Parameters { response, cutoff_frequency, resonance, modulation_sequencer_ticks, modulation_amplitude, modulation_stereo_dephase };
				static const int parameters [] = { response, cutoff_frequency, resonance, modulation_sequencer_ticks, modulation_amplitude, modulation_stereo_dephase };
				parameter = parameters[parameter-1];
				switch(parameter) {
					case cutoff_frequency:
						if(value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, 15 * math::pi, 22050 * math::pi).apply_inverse(std::asin(value / 0x100) * Player::singleton().timeInfo().sampleRate());
						break;
					case modulation_sequencer_ticks:
						if(value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, math::pi * 2 / 10000, math::pi * 2 * 2 * 3 * 4 * 5 * 7).apply_inverse(value * 3e-8 * Player::singleton().timeInfo().samplesPerTick());
						break;
					case resonance:
					case modulation_amplitude:
					case modulation_stereo_dephase:
						value *= maximum / 0x100;
						break;
				}
			}
			break;
		case ring_modulator:
			{
				enum Parameters { am_radians_per_second, am_glide, fm_radians_per_second, fm_bandwidth };
				static const int parameters [] = { am_radians_per_second, am_glide, fm_radians_per_second, fm_bandwidth };
				parameter = parameters[parameter-1];
				switch(parameter) {
					case am_radians_per_second:
						if(value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, 0.0001 * math::pi * 2, 22050 * math::pi * 2).apply_inverse(value * 2.5e-3 * Player::singleton().timeInfo().sampleRate());
						break;
					case am_glide:
						if(value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, 0.0001 * math::pi * 2, 15 * 22050 * math::pi * 2).apply_inverse(value * 5e-6 * Player::singleton().timeInfo().sampleRate()) * Player::singleton().timeInfo().sampleRate();
						break;
					case fm_radians_per_second:
						if(value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, 0.0001 * math::pi * 2, 100 * math::pi * 2).apply_inverse(value * 2.5e-5 * Player::singleton().timeInfo().sampleRate());
						break;
					case fm_bandwidth:
						if(value < 1.0f) value = 0;
						else value = scale::Exponential(maximum, 0.0001 * math::pi * 2, 22050 * math::pi * 2).apply_inverse(value * 5e-4 * Player::singleton().timeInfo().sampleRate());
						break;
				}
			}
			break;
	}
	integral_value = std::floor(value + Real(0.5));
}

}}}
