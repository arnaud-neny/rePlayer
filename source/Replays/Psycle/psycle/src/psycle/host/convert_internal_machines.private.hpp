#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "internal_machines.hpp"
#include <psycle/helpers/scale.hpp>
#include <string>
#include <exception>
#include <map>
namespace psycle
{
	namespace host
	{
		namespace convert_internal_machines
		{
			class Converter
			{
			public:
				enum Type
				{
					master,
					ring_modulator, distortion, 
					sampler, 
					delay, filter_2_poles, gainer, flanger,
					nativeplug,
					vsti, vstfx,
					scope,
					dummy = 255
				};
				//converted
				static const char* convnames[];
				static const char asynth2f[];

				//To convert
				static std::string abass;
				static std::string asynth;
				static std::string asynth2;
				static std::string asynth21;
				static std::string asynth22;

				virtual ~Converter() throw()
				{

				}

				Machine & redirect(const int & index, const std::pair<int,std::string> & type, RiffFile & riff) throw(std::exception)
				{
					Plugin & plugin = * new Plugin(index);
					Machine * pointer_to_machine = &plugin;
					try
					{
						std::string name = plugin_names()(type);

						bool bDeleted=false;
						if(!plugin.LoadDll(name))
						{
							pointer_to_machine = 0; // for delete pointer_to_machine in the catch clause
							if (type.first == nativeplug)
							{
								plugin.SkipLoad(&riff);
								pointer_to_machine = new Dummy(&plugin);
								((Dummy*)pointer_to_machine)->dllName=name;
								pointer_to_machine->_macIndex=index;
								pointer_to_machine->Init();
								delete & plugin;
								return *pointer_to_machine;
							}
							else {
								delete & plugin;
								pointer_to_machine = new Dummy(index);
								((Dummy*)pointer_to_machine)->dllName=name;
								bDeleted=true;
							}
						}
						Machine & machine = *pointer_to_machine;
						machine_converted_from[&machine] = type;
						machine.Init();
						assert(sizeof(int) == 4);
						riff.Read(machine._editName, 16); /* sizeof machine._editName); */ machine._editName[16] = 0;
						if(bDeleted) {
							std::stringstream s;
							s << "X!" << machine.GetEditName();
							machine.SetEditName(s.str());
						}
						if (type.first == nativeplug)
						{
							ReadPlugin(type,machine,riff);
						}
						machine.legacyWires.resize(MAX_CONNECTIONS);
						for(int i = 0; i < MAX_CONNECTIONS; i++) {
							riff.Read(&machine.legacyWires[i]._inputMachine,sizeof(machine.legacyWires[i]._inputMachine));	// Incoming connections Machine number
						}
						for(int i = 0; i < MAX_CONNECTIONS; i++) {
							riff.Read(&machine.legacyWires[i]._outputMachine,sizeof(machine.legacyWires[i]._outputMachine));	// Outgoing connections Machine number
						}
						for(int i = 0; i < MAX_CONNECTIONS; i++) {
							riff.Read(&machine.legacyWires[i]._inputConVol,sizeof(machine.legacyWires[i]._inputConVol));	// Incoming connections Machine vol
							machine.legacyWires[i]._wireMultiplier = 1.0f;
						}
						for(int i = 0; i < MAX_CONNECTIONS; i++) {
							riff.Read(&machine.legacyWires[i]._connection,sizeof(machine.legacyWires[i]._connection));      // Outgoing connections activated
						}
						for(int i = 0; i < MAX_CONNECTIONS; i++) {
							riff.Read(&machine.legacyWires[i]._inputCon,sizeof(machine.legacyWires[i]._inputCon));		// Incoming connections activated
						}
						riff.Read(machine._connectionPoint, sizeof(machine._connectionPoint));
						riff.Skip(2*sizeof(int)); // numInputs and numOutputs

						int panning;
						riff.Read(&panning, sizeof(int));
						machine.SetPan(panning);
						//
						// See details of the following at the end of this file (or in the fileformat doc)
						//
						riff.Skip(40); // skips sampler data.

						switch(type.first)
						{
						case delay:
							{
								int parameters [2]; riff.Read(parameters, sizeof parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters, 5);
							}
							break;
						case flanger:
							{
								int parameters [2]; riff.Read(parameters, sizeof parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters, 7);
							}
							break;
						case gainer:
							riff.Skip(sizeof(int));
							{
								int parameters [1]; riff.Read(parameters, sizeof parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters);
							}
							break;
						default:
							riff.Skip(2 * sizeof(int));
						}
						switch(type.first)
						{
						case distortion:
							int parameters [4]; riff.Read(parameters, sizeof parameters);
							retweak(machine, type, parameters, sizeof parameters / sizeof *parameters);
							break;
						default:
							riff.Skip(4 * sizeof(int));
						}
						switch(type.first)
						{
						case ring_modulator:
							{
								unsigned char parameters [4];
								riff.Read(&parameters[0], 2 * sizeof *parameters);
								riff.Skip(sizeof(char));
								riff.Read(&parameters[2], 2 * sizeof *parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters);
							}
							riff.Skip(40);
							break;
						case delay:
							riff.Skip(5);
							{
								int parameters [4];
								riff.Read(&parameters[0], sizeof *parameters);
								riff.Read(&parameters[2], sizeof *parameters);
								riff.Read(&parameters[1], sizeof *parameters);
								riff.Read(&parameters[3], sizeof *parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters);
							}
							riff.Skip(24);
							break;
						case flanger:
							riff.Skip(4);
							{
								unsigned char parameters [1]; riff.Read(parameters, sizeof parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters, 9);
							}
							{
								int parameters [6];
								riff.Read(&parameters[0], sizeof *parameters);
								riff.Skip(4);
								riff.Read(&parameters[3], sizeof *parameters);
								riff.Read(&parameters[5], sizeof *parameters);
								riff.Skip(8);
								riff.Read(&parameters[2], sizeof *parameters);
								riff.Read(&parameters[1], sizeof *parameters);
								riff.Read(&parameters[4], sizeof *parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters);
							}
							riff.Skip(4);
							break;
						case filter_2_poles:
							riff.Skip(21);
							{
								int parameters [6];
								riff.Read(&parameters[1], sizeof parameters - sizeof *parameters);
								riff.Read(&parameters[0], sizeof *parameters);
								retweak(machine, type, parameters, sizeof parameters / sizeof *parameters);
							}
							break;
						default:
							riff.Skip(45);
						}
						return machine;
					}
					catch(...)
					{
						delete pointer_to_machine;
						throw;
					}
				}

				void retweak(Song & song) const
				{
					/// \todo must each twk repeat the machine number ?
					// int previous_machines [MAX_TRACKS]; for(int i = 0 ; i < MAX_TRACKS ; ++i) previous_machines[i] = 255;
					for(int pattern(0) ; pattern < MAX_PATTERNS ; ++pattern)
					{
						if(!song.ppPatternData[pattern]) continue;
						PatternEntry * const lines(reinterpret_cast<PatternEntry*>(song.ppPatternData[pattern]));
						for(int line = 0 ; line < song.patternLines[pattern] ; ++line)
						{
							PatternEntry * const events(lines + line * MAX_TRACKS);
							for(int track(0); track < song.SONGTRACKS ; ++track)
							{
								PatternEntry & event(events[track]);
								if(event._note == notecommands::tweakeffect)
								{
									event._mach += 0x40;
									event._note = notecommands::tweak;
								}
								if(event._note == notecommands::tweak && event._mach < MAX_MACHINES)
								{
									std::map<Machine * const, std::pair<int, std::string>>::const_iterator i(machine_converted_from.find(song._pMachine[event._mach]));
									if(i != machine_converted_from.end())
									{
										int parameter(event._inst);
										int value((event._cmd << 8) + event._parameter);
										retweak(i->second, parameter, value);
										event._inst = parameter;
										event._cmd = value >> 8; event._parameter = 0xff & value;
									}
								}
								else if (event._cmd == 0x0E  && event._mach < MAX_MACHINES) {
									std::map<Machine * const, std::pair<int, std::string>>::const_iterator i(machine_converted_from.find(song._pMachine[event._mach]));
									if(i != machine_converted_from.end())
									{
										if (i->second.second==asynth22) {
											int param(25);
											int value(event._parameter);
											retweak(i->second,param,value);
											event._cmd = 0x0F;
											event._parameter = value;
										}
									}
								}
							}
						}
					}
				}

			private:
				class Plugin_Names : private std::map<std::pair<int, std::string>, const char *>
				{
				public:
					Plugin_Names()
					{
						(*this)[std::pair<int, std::string>(ring_modulator,"")] = convnames[ring_modulator];
						(*this)[std::pair<int, std::string>(distortion,"")] = convnames[distortion];
						(*this)[std::pair<int, std::string>(delay,"")] = convnames[delay];
						(*this)[std::pair<int, std::string>(filter_2_poles,"")] = convnames[filter_2_poles];
						(*this)[std::pair<int, std::string>(gainer,"")] = convnames[gainer];
						(*this)[std::pair<int, std::string>(flanger,"")] = convnames[flanger];
						(*this)[std::pair<int, std::string>(nativeplug,abass)] = asynth2f;
						(*this)[std::pair<int, std::string>(nativeplug,asynth)] = asynth2f;
						(*this)[std::pair<int, std::string>(nativeplug,asynth2)] = asynth2f;
						(*this)[std::pair<int, std::string>(nativeplug,asynth21)] = asynth2f;
						(*this)[std::pair<int, std::string>(nativeplug,asynth22)] = asynth2f;
					}
					~Plugin_Names()
					{
					}
					bool exists(const std::pair<int,std::string>& type) const throw()
					{
						return find(type) != end();
					}
					bool exists(const int & type) const throw()
					{
						return find(std::pair<int,std::string>(type,"")) != end();
					}
					bool exists(std::string plugname) const throw()
					{
						return find(std::pair<int,std::string>(nativeplug,plugname)) != end();
					}
					const char* operator()(const std::pair<int,std::string>& type) const throw(std::exception)
					{
						const_iterator i = find(type);
						if(i == end()) throw std::exception("internal machine replacement plugin not declared");
						return i->second;
					}
					const char* operator()(const int & type) const throw(std::exception)
					{
						const_iterator i = find(std::pair<int,std::string>(type,""));
						if(i == end()) throw std::exception("internal machine replacement plugin not declared");
						return i->second;
					}
					const char* operator()(std::string plugname) const throw(std::exception)
					{
						const_iterator i = find(std::pair<int,std::string>(nativeplug,plugname));
						if(i == end()) throw std::exception("internal machine replacement plugin not declared");
						return i->second;
					}
				};

			public:
				static const Plugin_Names & plugin_names()
				{
					static const Plugin_Names plugin_names;
					return plugin_names;
				}

			private:
				std::map<Machine * const, std::pair<int, std::string>> machine_converted_from;
				void ReadPlugin(const std::pair<int,std::string> type, Machine& machine, RiffFile& riff)
				{
					int numParameters;
					riff.Read(&numParameters, sizeof(numParameters));
					int *Vals = new int[numParameters];
					riff.Read(Vals, numParameters*sizeof(int));

					if(machine._type == MACH_DUMMY) {
						//do nothing.
					}
					else if(type.second==abass) {
						retweak(machine,type,Vals,15,0);
						machine.SetParameter(19,0);
						retweak(machine,type,Vals+15,1,15);
						if (numParameters>16) {
							retweak(machine,type,Vals+16,2,16);
						}
						else {
							machine.SetParameter(24,0);
							machine.SetParameter(25,0);
						}
					}
					else if(type.second==asynth) {
						retweak(machine,type,Vals,numParameters,0);
						machine.SetParameter(24,0);
						machine.SetParameter(25,0);
						machine.SetParameter(27,1);
					}
					else if(type.second==asynth2) {
						retweak(machine,type,Vals,numParameters,0);
						machine.SetParameter(24,0);
						machine.SetParameter(25,0);
					}
					else if(type.second==asynth21) {
						//I am unsure which was the diference between asynth2 and asynth21 (need to chech sources in the cvs)
						retweak(machine,type,Vals,numParameters,0);
						machine.SetParameter(24,0);
						machine.SetParameter(25,0);
					}
					else if(type.second==asynth22) {
						retweak(machine,type,Vals,numParameters,0);
					}
					delete[] Vals;
				}
				template<typename Parameter> void retweak(Machine & machine, const std::pair<int, std::string> & type, Parameter parameters [], const int & parameter_count, const int & parameter_offset = 1)
				{
					for(int parameter(0) ; parameter < parameter_count ; ++parameter)
					{
						int new_parameter(parameter_offset + parameter);
						int new_value(parameters[parameter]);
						retweak(type, new_parameter, new_value);
						machine.SetParameter(new_parameter, new_value);
					}
				}

				void retweak(const std::pair<int, std::string> & type, int & parameter, int & integral_value) const
				{
					typedef double Real;
					Real value(integral_value);
					const Real maximum(0xffff);
					switch(type.first)
					{
					case gainer:
						{
							enum Parameters { gain };
							static const int parameters [] = { gain };
							parameter = parameters[--parameter];
							switch(parameter)
							{
							case gain:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, exp(-4.), exp(+4.)).apply_inverse(value / 0x100);
								break;
							}
						}
						break;
					case distortion:
						{
							enum Parameters { input_gain, output_gain, positive_threshold, positive_clamp, negative_threshold, negative_clamp, symmetric };
							static const int parameters [] = { positive_threshold, positive_clamp, negative_threshold, negative_clamp };
							parameter = parameters[--parameter];
							switch(parameter)
							{
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
							parameter = parameters[--parameter];
							switch(parameter)
							{
							case left_delay:
							case right_delay:
								value *= Real(2 * 3 * 4 * 5 * 7) / Global::player().SamplesPerRow();
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
							parameter = parameters[--parameter];
							switch(parameter)
							{
							case delay:
								value *= maximum / 0.1 / Global::player().SampleRate();
								break;
							case modulation_amplitude:
							case modulation_stereo_dephase:
								value *= maximum / 0x100;
								break;
							case modulation_radians_per_second:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, 0.0001 * helpers::math::pi * 2, 100 * helpers::math::pi * 2).apply_inverse(value * 3e-9 * Global::player().SampleRate());
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
							parameter = parameters[--parameter];
							switch(parameter)
							{
							case cutoff_frequency:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, 15 * helpers::math::pi, 22050 * helpers::math::pi).apply_inverse(std::asin(value / 0x100) * Global::player().SampleRate());
								break;
							case modulation_sequencer_ticks:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, helpers::math::pi * 2 / 10000, helpers::math::pi * 2 * 2 * 3 * 4 * 5 * 7).apply_inverse(value * 3e-8 * Global::player().SamplesPerRow());
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
							parameter = parameters[--parameter];
							switch(parameter)
							{
							case am_radians_per_second:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, 0.0001 * helpers::math::pi * 2, 22050 * helpers::math::pi * 2).apply_inverse(value * 2.5e-3 * Global::player().SampleRate());
								break;
							case am_glide:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, 0.0001 * helpers::math::pi * 2, 15 * 22050 * helpers::math::pi * 2).apply_inverse(value * 5e-6 * Global::player().SampleRate() * Global::player().SampleRate());
								break;
							case fm_radians_per_second:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, 0.0001 * helpers::math::pi * 2, 100 * helpers::math::pi * 2).apply_inverse(value * 2.5e-5 * Global::player().SampleRate());
								break;
							case fm_bandwidth:
								if ( value < 1.0f) value = 0;
								else value = helpers::scale::Exponential(maximum, 0.0001 * helpers::math::pi * 2, 22050 * helpers::math::pi * 2).apply_inverse(value * 5e-4 * Global::player().SampleRate());
								break;
							}
						}
						break;
					case nativeplug:
						{
							if(type.second == abass) {
								if(parameter > 0 && parameter < 15)  {
									parameter+=4;
								}
								else if (parameter == 15 ) {
									parameter+=5;
								}
								else if (parameter > 15) {
									parameter+=8;
								}
							}
							else if(type.second == asynth) {
								if((parameter == 0 ||parameter == 1)&& integral_value == 4) {
									value = 5;
								}
								if(parameter==17) {
									value+=10.f;
								}
							}
							else if(type.second == asynth2) {
								if((parameter == 0 ||parameter == 1)&& integral_value == 4) {
									value = 5;
								}
							}
							else if(type.second == asynth21) {
								if((parameter == 0 ||parameter == 1)&& integral_value == 4) {
									value = 5;
								}
							}
							else if(type.second == asynth22) {
								if((parameter == 0 ||parameter == 1)&& integral_value == 4) {
									value = 5;
								}
								if (parameter==7 && integral_value == 0) {
									value = 1.f;
								}
								else if (parameter==25) {
									value = 256 - sqrtf(16000.f - value*16000.f/127.f);
									parameter+=1;
								}
								else if (parameter==26) {
									parameter-=1;
								}
							}
						}
						break;
					}
					integral_value = std::floor(value + Real(0.5));
				}
			};
			const char* Converter::convnames[] =
			{
				"",
				"ring_modulator.dll",
				"distortion.dll",
				"",
				"delay.dll",
				"filter_2_poles.dll",
				"gainer.dll",
				"flanger.dll"
			};
			const char Converter::asynth2f[] = "arguru synth 2f.dll";

			//To convert
			std::string Converter::abass = "arguru bass.dll";
			std::string Converter::asynth = "arguru synth.dll";
			std::string Converter::asynth2 = "arguru synth 2.dll";
			std::string Converter::asynth21 = "synth21.dll";
			std::string Converter::asynth22 = "synth22.dll";
		}
	}
}
/*
	pFile->Read(&junkdata[0], 8*sizeof(int)); // SubTrack[]
	pFile->Read(&junkdata[0], sizeof(int)); // numSubtracks
	pFile->Read(&junkdata[0], sizeof(int)); // interpol

	pFile->Read(&junkdata[0], sizeof(int)); // outwet
	pFile->Read(&junkdata[0], sizeof(int)); // outdry

	pFile->Read(&junkdata[0], sizeof(int)); // distPosThreshold
	pFile->Read(&junkdata[0], sizeof(int)); // distPosClamp
	pFile->Read(&junkdata[0], sizeof(int)); // distNegThreshold
	pFile->Read(&junkdata[0], sizeof(int)); // distNegClamp

	pFile->Read(&junkdata[0], sizeof(char)); // sinespeed
	pFile->Read(&junkdata[0], sizeof(char)); // sineglide
	pFile->Read(&junkdata[0], sizeof(char)); // sinevolume
	pFile->Read(&junkdata[0], sizeof(char)); // sinelfospeed
	pFile->Read(&junkdata[0], sizeof(char)); // sinelfoamp

	pFile->Read(&junkdata[0], sizeof(int)); // delayTimeL
	pFile->Read(&junkdata[0], sizeof(int)); // delayTimeR
	pFile->Read(&junkdata[0], sizeof(int)); // delayFeedbackL
	pFile->Read(&junkdata[0], sizeof(int)); // delayFeedbackR

	pFile->Read(&junkdata[0], sizeof(int)); // filterCutoff
	pFile->Read(&junkdata[0], sizeof(int)); // filterResonance
	pFile->Read(&junkdata[0], sizeof(int)); // filterLfospeed
	pFile->Read(&junkdata[0], sizeof(int)); // filterLfoamp
	pFile->Read(&junkdata[0], sizeof(int)); // filterLfophase
	pFile->Read(&junkdata[0], sizeof(int)); // filterMode

*/