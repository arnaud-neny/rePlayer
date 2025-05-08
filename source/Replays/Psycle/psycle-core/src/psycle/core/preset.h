// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PRESET__INCLUDED
#define PSYCLE__CORE__PRESET__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

#include <vector>
#include <string>

namespace psycle {
	namespace helpers {
		class BinRead;
	}
	
	namespace core {
		class Machine;

		class PSYCLE__CORE__DECL Preset {
			public:
				Preset(int numpars, int dataSize);
                Preset(Preset& rhs);

				bool read(helpers::BinRead& prsIn);

				const std::string& name() const;
				const std::vector<int>& parameter() const;

				void tweakMachine(Machine & mac);

			private:
				std::string name_;

				std::vector<int> params_;
				std::vector<char> data_;
		};

}}
#endif
