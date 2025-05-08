// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2003-2007 psycledelics http://psycle.pastnotecut.org ; johan boule <bohan@jabber.org>

/// \file
/// \brief yet another psycle plugin interface api by bohan
/// This one is more object-oriented than the original plugin_interface.hpp one.
/// The benefits of it are:
/// - it can apply a scale to parameter values, transparently, so you only deal with the meaningful real scaled value.
/// - it provides a bugfree initialization of the plugin.
/// - it provides the events samples_per_second_changed and sequencer_ticks_per_second_changed, which were not part of the original interface (i don't know if it's been fixed in the host nowadays).
#pragma once
#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/scale.hpp>
#include <universalis.hpp>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring> // strcpy
namespace psycle { namespace plugin {

using plugin_interface::pi;

/// This class is the one your plugin class should derive from.
class Plugin : protected plugin_interface::CMachineInterface {
	public:
		typedef double Real;
		typedef float Sample;

	///\name description of plugin and its parameters
	///\{
		public:
			/// information describing a plugin
			class Information : public plugin_interface::CMachineInfo {
				public:
					struct Types { enum Type { effect = plugin_interface::EFFECT, generator = plugin_interface::GENERATOR }; };
					class Parameter;
					///\return the Parameter at the index parameter.
					Parameter const inline & parameter(int const & parameter) const throw() {
						return *static_cast<Parameter const * const>(Parameters[parameter]);
					}
					/// creates the information object.
					Information(
						short const & version,
						int const & type,
						char const description[],
						char const name[],
						char const author[],
						int const & columns,
						Parameter const parameters[],
						int const & parameter_count
					) :
						plugin_interface::CMachineInfo(
							plugin_interface::MI_VERSION,
							version,
							type,
							parameter_count,
							//This new cannot be deleted. The interface does not support a destructor.
							/*parameter_count <= kMaxParameters ? SomeParameters : */new plugin_interface::CMachineParameter const * [parameter_count],
							description,
							name,
							author,
							"Help",
							columns
						)
					{
						SomeParameters = std::unique_ptr<plugin_interface::CMachineParameter const*>((plugin_interface::CMachineParameter const**)plugin_interface::CMachineInfo::Parameters);
						assert(parameter_count <= kMaxParameters);
						for(int i(0) ; i < numParameters ; ++i) {
							plugin_interface::CMachineParameter const *& p = const_cast<plugin_interface::CMachineParameter const *&>(Parameters[i]);
							p = &parameters[i];
						}
					}
					static constexpr int kMaxParameters = 64;
					//plugin_interface::CMachineParameter const* SomeParameters[kMaxParameters] = { nullptr };
					std::unique_ptr<plugin_interface::CMachineParameter const*> SomeParameters;

				public:
					/// information describing a parameter
					class Parameter : public plugin_interface::CMachineParameter {
						public:
							struct Types { enum Type { null = plugin_interface::MPF_NULL, label = plugin_interface::MPF_LABEL, state = plugin_interface::MPF_STATE }; };
                            enum class discrete { on }; enum class linear { on }; enum class exponential { on }; enum class logarithmic { on };
							std::unique_ptr<helpers::Scale> scale;
						public:
							/// creates a separator with an optional label.
							Parameter(char const name[] = "")
							:
								scale(new helpers::scale::Discrete(0))
							{
								Name = name;
								Description = name;
								MinValue = 0;
								MaxValue = 0;
								if (name[0] == '\0') {
									Flags = Types::null;
								}
								else {
									Flags = Types::label;
								}
								DefValue = 0;
							}
							Parameter(Parameter&& other)
								: scale(std::move(other.scale))
							{
                                Name = other.Name;
                                Description = other.Description;
                                MinValue = other.MinValue;
                                MaxValue = other.MaxValue;
                                Flags = other.Flags;
                                DefValue = other.DefValue;
							}
						public:
							/// minimum lower bound authorized by the GUI interface. (unsigned 16-bit integer)
							static constexpr int input_minimum_value = 0;
							/// maximum upper bound authorized by the GUI interface. (unsigned 16-bit integer)
							static constexpr int input_maximum_value = 0xffff;
						private:
							/// creates a scaled parameter ; you don't use this directly, but rather the public static creation functions.
							Parameter(char const name[], helpers::Scale* scale, Real const & default_value, int const & input_maximum_value = Parameter::input_maximum_value)
							:
								scale(scale)
							{
								Name = name;
								Description = name;
								MinValue = input_minimum_value;
								MaxValue = input_maximum_value;
								Flags = Types::state;
								DefValue = std::min(
									std::max(
										input_minimum_value,
										static_cast<int>(scale->apply_inverse(default_value))
									),
									input_maximum_value
								);
							}
						public:
							/// creates a discrete scale, i.e. integers.
							Parameter(char const name[], int const & default_value, int const & maximum_value, discrete)
								: Parameter(name, new helpers::scale::Discrete(static_cast<Real>(maximum_value)), static_cast<Real>(default_value), maximum_value)
							{}
							/// creates a linear real scale.
							Parameter(char const name[], double const & minimum_value, double const & default_value, double const & maximum_value, linear)
								: Parameter(name, new helpers::scale::Linear(static_cast<Real>(input_maximum_value), static_cast<Real>(minimum_value), static_cast<Real>(maximum_value)), static_cast<Real>(default_value))
							{}
							/// creates an exponential scale.
							Parameter(char const name[], double const & minimum_value, double const & default_value, double const & maximum_value, exponential)
								: Parameter(name, new helpers::scale::Exponential(static_cast<Real>(input_maximum_value), static_cast<Real>(minimum_value), static_cast<Real>(maximum_value)), static_cast<Real>(default_value))
							{}
							/// creates a logarithmic scale.
							Parameter(char const name[], double const & minimum_value, double const & default_value, double const & maximum_value, logarithmic)
								: Parameter(name, new helpers::scale::Logarithmic(static_cast<Real>(input_maximum_value), static_cast<Real>(minimum_value), static_cast<Real>(maximum_value)), static_cast<Real>(default_value))
							{}
					}; // class Parameter
			}; // class Information
		protected:
			Information const & information;
	///\}

	///\name ctor/dtor
	///\{
		protected:
			/// creates an instance of your plugin.
			/// Note: functions that queries the host should only be called after the init() event.
			Plugin(Information const & information)
			:
				information(information),
				initialized_(false),
				samples_per_second_(0),
				sequencer_ticks_per_sequencer_beat_(0),
				sequencer_beats_per_minute_(0)
			{
				Vals = new int[information.numParameters];
				scaled_parameters_ = new Real[information.numParameters];
				for(int parameter(0) ; parameter < information.numParameters ; ++parameter)
					parameter_internal(parameter, information.parameter(parameter).DefValue);
			}
		public:
			~Plugin() { delete [] Vals; delete [] scaled_parameters_; }
	///\}
		
	///\name parameters
	///\{
		public:
			///\returns the integral internal value (i.e. non-scaled) of a parameter ; use this for discrete scales.
			inline int const & operator[](int const & parameter) const throw() { return Vals[parameter]; }
			///\returns the real scaled value of a parameter.
			inline Real const & operator()(int const & parameter) const throw() { return scaled_parameters_[parameter]; }
		protected:
			///\returns the integral internal value (i.e. non-scaled) of a parameter ; use this for discrete scales.
			inline int & operator[](int const & parameter) throw() { return Vals[parameter]; }
			///\returns the real scaled value of a parameter.
			inline Real & operator()(int const & parameter) throw() { return scaled_parameters_[parameter]; }
		private:
			Real * scaled_parameters_;
			///\internal
			void parameter_internal(int const parameter, int const value) {
				this->Vals[parameter] = value;
				this->scaled_parameters_[parameter] = information.parameter(parameter).scale->apply(static_cast<Real>(value));
			}
			///\internal
			/*override*/ void ParameterTweak(int parameter, int value) {
				parameter_internal(parameter, value);
				this->parameter(parameter);
			}
		protected:
			/// event called by the host when it wants a parameter value to be changed.
			/// override this virtual function in your plugin to take any action in response to this event.
			/// You don't need to update the parameter value itself when this event occurs,
			/// but you might want to update other data dependent on the parameter's value.
			virtual void parameter(int const &) {}
	///\}
	
	///\name parameter value description
	///\{
		protected:
			/// event called by the host went it wants a string description of a parameter's current value.
			/// override this virtual function in your plugin to return a specialized text instead of just the value of the number,
			/// i.e. you can append the unit (Hz, seconds, etc), or show the value in deciBell, etc.
			virtual void describe(std::ostream & out, int const & parameter) const {
				out << (*this)(parameter);
			}
		private:
			///\internal
			/*override*/ bool DescribeValue(char * out, int const parameter, int const) {
				std::stringstream s;
				describe(s, parameter);
				std::strcpy(out, s.str().c_str());
				return out[0];
			}
	///\}
	
	///\name initialization by host event
	///\{
		protected:
			/// event called by the host to ask the plugin to initialize.
			/// You should rather override the virtual init() function.
			///\see init()
			/*override*/ void Init() {
				samples_per_second_ = static_cast<Real>(pCB->GetSamplingRate());
				seconds_per_sample_ = 1 / samples_per_second_;
				sequencer_ticks_per_sequencer_beat_ = pCB->GetTPB();
				sequencer_beats_per_minute_ = pCB->GetBPM();
				sequencer_ticks_per_seconds_ = Real(sequencer_ticks_per_sequencer_beat_ * sequencer_beats_per_minute_) / 60;
				init();
				initialized_ = true;
			}
			/// event called by the host to ask the plugin to initialize.
			/// override this virtual function in your plugin to take any action in response to this event.
			/// Note: functions that queries the host should only be called after this event.
			virtual void init() {}
		private:
			///\internal
			bool initialized_;
	///\}

	///\name info from host
	///\{		
		private:
			///\internal
			Real samples_per_second_, seconds_per_sample_, sequencer_ticks_per_seconds_;
			///\internal
			int sequencer_ticks_per_sequencer_beat_, sequencer_beats_per_minute_;
		protected:
			/// event called by the host every tick.
			/// override this virtual function in your plugin to take any action in response to this event.
			/*override*/ void SequencerTick() {
				if(samples_per_second_ != static_cast<Real>(pCB->GetSamplingRate())) {
					samples_per_second_ = static_cast<Real>(pCB->GetSamplingRate());
					seconds_per_sample_ = 1 / samples_per_second_;
					if(initialized_) samples_per_second_changed();
				}
				if(
					sequencer_ticks_per_sequencer_beat_ != pCB->GetTPB() ||
					sequencer_beats_per_minute_ != pCB->GetBPM()
				) {
					sequencer_ticks_per_sequencer_beat_ = pCB->GetTPB();
					sequencer_beats_per_minute_ = pCB->GetBPM();
					sequencer_ticks_per_seconds_ = Real(sequencer_ticks_per_sequencer_beat_ * sequencer_beats_per_minute_) / 60;
					if(initialized_) sequencer_ticks_per_second_changed();
				}
			}
			/// event called when the sample rate changed.
			/// override this virtual function in your plugin to take any action in response to this event.
			virtual void samples_per_second_changed() {}
			/// event called when the duration of a tick changed.
			/// override this virtual function in your plugin to take any action in response to this event.
			virtual void sequencer_ticks_per_second_changed() {}
			/// asks the host to return this value.
			Real const samples_per_second() const { return samples_per_second_; }
			/// asks the host to return this value.
			Real const seconds_per_sample() const { return seconds_per_sample_; }
			/// asks the host to return this value.
			Real const sequencer_ticks_per_seconds() const { return sequencer_ticks_per_seconds_; }
			/// asks the host to return this value.
			Real const samples_per_sequencer_tick() const { return samples_per_second() / sequencer_ticks_per_seconds(); }
	///\}
		
	///\name dialog
	///\{
		protected:
			/// you can ask the host to display a message.
			void message(std::string const & message) const throw() { this->message("message", message); }
			/// event called by the host when the user queries information about your plugin.
			/// override this virtual function in your plugin to output a help text describing what your plugin does, what tracker commands are available etc.
			virtual void help(std::ostream & out) throw() { out << "no help available"; }
		private:
			///\internal
			/*override*/ void Command() throw() {
				std::ostringstream s;
				help(s);
				this->message(information.Command, s.str());
			}
			///\internal
			void message(std::string const & caption, std::string const & message) const {
				std::stringstream s;
				s << information.Name << " - " << caption;
				pCB->MessBox(message.c_str(), s.str().c_str(), 0);
			}
	///\}
	
	protected:
		class Exception : public std::runtime_error {
			public:
				Exception(const std::string & what) : std::runtime_error(what.c_str()) {}
		}; // class Exception
}; // class Plugin

#undef PSYCLE__PLUGIN__INSTANTIATOR

/// call this from your plugin's source file to export the necessary function from the dynamically linked library.
#define PSYCLE__PLUGIN__INSTANTIATOR(dllname, typename) \
		PSYCLE__PLUGIN__DYN_LINK__EXPORT \
		psycle::plugin_interface::CMachineInfo const * const \
		PSYCLE__PLUGIN__CALLING_CONVENTION \
		GetInfo() { return &typename::information(); } \
		\
		PSYCLE__PLUGIN__DYN_LINK__EXPORT \
		psycle::plugin_interface::CMachineInterface * \
		PSYCLE__PLUGIN__CALLING_CONVENTION \
		CreateMachine() { return (psycle::plugin_interface::CMachineInterface *)(new typename); } \
		\
		namespace { \
		struct ThePluginEntry : psycle::plugin_interface::PluginEntry { static bool ms_isRegistered; }; \
		bool ThePluginEntry::ms_isRegistered = psycle::plugin_interface::PluginFactory::Register(dllname, GetInfo, CreateMachine); }

}}
