// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2009 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#ifndef PSYCLE__CORE__CONVERT_INTERNAL_MACHINES__INCLUDED
#define PSYCLE__CORE__CONVERT_INTERNAL_MACHINES__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>
#include <string>
#include <map>

namespace psycle { namespace core {

class Machine;
class RiffFile;
class CoreSong;
class MachineFactory;

namespace convert_internal_machines {
	class PSYCLE__CORE__DECL Converter {
		public:
			enum Type {
				master,
				ring_modulator, distortion, sampler, delay, filter_2_poles, gainer, flanger,
				plugin,
				vsti, vstfx,
				scope,
				abass,
				asynth1,
				asynth2,
				asynth21,
				dummy = 255
			};

			Converter();
			virtual ~Converter() throw();

			Machine & redirect(MachineFactory & factory, int index, int type, RiffFile & riff);

			void retweak(CoreSong & song) const;

		private:
			class Plugin_Names : private std::map<int, std::string const *> {
				public:
					Plugin_Names();
					~Plugin_Names();
					bool exists(int type) const throw();
					std::string const & operator()(int type) const;
			};

		public:
			static const Plugin_Names & plugin_names();

		private:
			std::map<Machine *, int const *> machine_converted_from;

			template<typename Parameter> void retweak(Machine & machine, int type, Parameter parameters [], int parameter_count, int parameter_offset = 1);
			void retweak(int type, int & parameter, int & integral_value) const;
	};
}

}}
#endif
