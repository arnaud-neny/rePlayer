// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "machinekey.hpp"
#include "internalhost.hpp"

#include <algorithm>

namespace psycle { namespace core {

		struct ToLower {
			char operator() (char c) const  { return std::tolower(c); }
		};


		MachineKey::MachineKey()
		:
			dllName_(),
			host_(Hosts::INTERNAL),
			index_(uint32_t(-1))
		{}
		MachineKey::MachineKey(const MachineKey & key)
		:
			dllName_(key.dllName()),
			host_(key.host()),
			index_(key.index())
		{}

		MachineKey::MachineKey(const Hosts::type host, const std::string & dllName, uint32_t index, bool load)
		:
			host_(host),
			index_(index)
		{
			if(!dllName.empty()) dllName_ = preprocessName(dllName, load);
		}

		MachineKey::~MachineKey() {
		}

		const std::string MachineKey::preprocessName(std::string dllName, bool load) {
			#if 0
			std::cout << "preprocess in: " << dllName << std::endl;
			#endif
			
			{ // 1) remove extension
				std::string::size_type pos(dllName.find(".so"));
				if(pos != std::string::npos) dllName = dllName.substr(0, pos);

				pos = dllName.find(".dll");
				if(pos != std::string::npos) dllName = dllName.substr(0, pos);
			}

			// 2) ensure lower case
			std::transform(dllName.begin(),dllName.end(),dllName.begin(),ToLower());
			
			// 3) replace spaces and underscores with dash.
			std::replace(dllName.begin(),dllName.end(),' ','-');
			std::replace(dllName.begin(),dllName.end(),'_','-');

			{ // 4) remove prefix
				std::string const prefix("libpsycle-plugin-");
				std::string::size_type const pos(dllName.find(prefix));
				if(pos == 0) dllName.erase(pos, prefix.length());
			}

			if(load) {
				if(dllName == "blitz") dllName = "blitz12";
				else if(dllName == "gamefx") dllName = "gamefx13";
			}

			#if 0
			std::cout << "preprocess out: " << dllName << std::endl;
			#endif
			
			return dllName;
		}

		bool MachineKey::operator<(const MachineKey & key) const {
			if ( host() != key.host() ) 
				return host() < key.host();
			else if ( dllName() != key.dllName() )
				return dllName() < key.dllName();
			return index() < key.index();
		}

		bool MachineKey::operator==( const MachineKey & rhs ) const {
			return host() == rhs.host() && dllName() == rhs.dllName() && index() == rhs.index();
		}

		MachineKey& MachineKey::operator=( const MachineKey & key ) {
			host_ = key.host();
			dllName_ = key.dllName();
			index_ = key.index();
			return *this;
		}
		
		const std::string & MachineKey::dllName() const {
			return dllName_;
		}
		
		Hosts::type MachineKey::host() const {
			return host_;
		}
		
		uint32_t MachineKey::index() const {
			return index_;
		}
}}
