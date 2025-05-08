// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "preset.h"

#include <psycle/helpers/binread.hpp>
#include "machine.h"
#include "plugin.h"

namespace psycle { namespace core {

using namespace helpers;

Preset::Preset(int numpars, int dataSize) :
	params_(numpars),
	data_(dataSize)
{}

Preset::Preset(Preset &rhs):
    name_(rhs.name_),
    params_(rhs.params_),
    data_(rhs.data_)
{}

bool Preset::read(BinRead & prsIn) {
	// read the preset name     
	char cbuf[32];
	prsIn.read(cbuf,32);
	cbuf[31] = '\0';
	name_ = cbuf;
	if ( prsIn.eof() || prsIn.bad() ) return false;
	// load parameter values
	for(std::size_t i(0); i < params_.size(); ++i) params_[i] = prsIn.readInt4LE();
	if ( prsIn.eof() || prsIn.bad() ) return false;
	// load special machine data
	if ( data_.size() ) {
		prsIn.read( &data_[0], static_cast<std::streamsize>(data_.size()) ); 
		if ( prsIn.eof() || prsIn.bad() ) return false;
	}
	return true;
}

const std::string & Preset::name() const {
	return name_;
}

const std::vector<int>& Preset::parameter() const {
	return params_;
}

void Preset::tweakMachine( Machine & /*mac*/ ) {
	//FIXME: Implement with polymorphism
	#if 0
	if( mac.type() == MACH_PLUGIN ) {
		int i = 0;
		for (std::vector<int>::iterator it = params_.begin(); it < params_.end(); it++) {
			try {
				reinterpret_cast<Plugin&>(mac).proxy().ParameterTweak(i, *it);
			}
			catch(const std::exception &) {
			}
			catch(...) {
			}
			i++;
		}

		try {
			if ( data_.size() > 0 ) 
				reinterpret_cast<Plugin&>(mac).proxy().PutData( &data_[0]); // Internal save
		}
		catch(const std::exception &) {
		}
		catch(...) {
		}
	}
	#endif
}

}}
