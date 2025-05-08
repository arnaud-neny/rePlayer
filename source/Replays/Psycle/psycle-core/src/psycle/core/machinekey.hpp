// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__MACHINE_KEY__INCLUDED
#define PSYCLE__CORE__MACHINE_KEY__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>
#include <boost/operators.hpp>
#include <string>

namespace psycle { namespace core {

// type Hosts::type
// Allows to differentiate between machines of different hosts.
namespace Hosts {
	enum type
	{
		INTERNAL=0,
		NATIVE,
		VST,
		LADSPA,
		//Keep at last position
		NUM_HOSTS
	};
}

class PSYCLE__CORE__DECL MachineKey
: private
	boost::equality_comparable<MachineKey,
	boost::less_than_comparable<MachineKey
	> >
{
	public:
		MachineKey();
		MachineKey(const Hosts::type host, const std::string & dllName, const uint32_t index = 0, bool load = false);
		MachineKey(const MachineKey& key);
		~MachineKey();

		static const std::string preprocessName(std::string dllName, bool load = false);

		const std::string & dllName() const;
		Hosts::type host() const;
		uint32_t index() const;

		bool operator<( const MachineKey & key) const;
		bool operator==( const MachineKey & rhs ) const;
		MachineKey& operator=( const MachineKey & key );
	private:
		std::string dllName_;
		Hosts::type host_;
		uint32_t index_;

};


}}
#endif
