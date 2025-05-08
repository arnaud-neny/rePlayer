// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "machinefactory.h"

#include "pluginfinder.h"
#include "internalkeys.hpp"
#include "internalhost.hpp"
#include "nativehost.hpp"
#include "vsthost.h"
#include "ladspahost.hpp"

namespace psycle { namespace core {

MachineFactory& MachineFactory::getInstance() {
	static MachineFactory factory;
	return factory;
}

MachineFactory::MachineFactory()
:
	callbacks_(),
	finder_()
{}

void MachineFactory::Initialize(MachineCallbacks* callbacks) {
	callbacks_ = callbacks;
	//\todo: probably we need a destructor for this
	finder_= new PluginFinder(false);
	FillHosts();
}

void MachineFactory::Initialize(MachineCallbacks* callbacks,PluginFinder* finder) {
	callbacks_ = callbacks;
	finder_ = finder;
	FillHosts();
}

void MachineFactory::Finalize(bool deleteFinder) {
	if(deleteFinder) delete finder_;
}

void MachineFactory::FillHosts() {
	finder_->Initialize();
	//Please, keep the same order than with the Hosts::type enum. (machinekey.hpp)
	hosts_.push_back( &InternalHost::getInstance(callbacks_) );
	finder_->addHost(Hosts::INTERNAL);
	// InternalHost doesn't have a path, so we call FillFinderData now.
	InternalHost::getInstance(callbacks_).FillFinderData(*finder_);

	hosts_.push_back( &NativeHost::getInstance(callbacks_) );
	finder_->addHost(Hosts::NATIVE);

	#ifdef DIVERSALIS__OS__MICROSOFT
		hosts_.push_back( &vst::host::getInstance(callbacks_) );
		finder_->addHost(Hosts::VST);
	#else
		hosts_.push_back(&NativeHost::getInstance(callbacks_) );
		finder_->addHost(Hosts::NATIVE);
	#endif

	hosts_.push_back( &LadspaHost::getInstance(callbacks_) );
	finder_->addHost(Hosts::LADSPA);

}

Machine* MachineFactory::CreateMachine(const MachineKey & key, Machine::id_type id) {

	assert(key.host() >= 0 && key.host() < Hosts::NUM_HOSTS);

	// a check for these machines is done, because we don't add it into the finder,
	// to prevent a user to create them manually.
	if ( key != InternalKeys::master && 
		key != InternalKeys::failednative && key != InternalKeys::wrapperVst) {
		if (!finder_->hasKey(key)) {
			if(loggers::warning()()) {
				std::ostringstream s;
				s << "psycle: core: machine factory: create machine: plugin not found: " << key.dllName();
				loggers::warning()(s.str());
			}
			return 0;
		}
		if ( !finder_->info(key).allow() ) {
			if(loggers::warning()()) {
				std::ostringstream s;
				s << "psycle: core: machine factory: create machine: plugin not allowed to run: " << key.dllName();
				loggers::warning()(s.str());
			}
			return hosts_[Hosts::INTERNAL]->CreateMachine(*finder_,InternalKeys::dummy,id);
		}
	}
	if(loggers::information()()) {
		std::ostringstream s;
		s << "psycle: core: machine factory: create machine: loading with host: " << key.host() << ", plugin: " << key.dllName()
			//<< " | " << finder_->info(key).libName()
			//<< " | " << finder_->info(key).name()
			;
		loggers::information()(s.str());
	}
	Machine* mac = hosts_[key.host()]->CreateMachine(*finder_, key, id);
	if (mac && mac->getMachineKey() != InternalKeys::master &&
		mac->getMachineKey() != InternalKeys::failednative &&
		mac->getMachineKey() != InternalKeys::wrapperVst &&
		mac->getMachineKey() != InternalKeys::invalid) {
		//Workaround for some where the first work call initializes some variables.
		///\todo: Should better be done inside the host, since it may be enough to do it
		// for VST and fix whichever native or internal that needed it. 
		if (key != InternalKeys::wrapperVst) 
		{
			mac->PreWork(MAX_BUFFER_LENGTH, true);
			mac->GenerateAudio(MAX_BUFFER_LENGTH);
		}
	}
	return mac;
	#if 0
		for(int i = 0; i < hosts_.size(); ++i) {
			if(hosts_[i]->hostCode() == key.host()) {
				return hosts_[i]->CreateMachine(key,id);
				break;
			}
		}
		return 0;
	#endif
}

Machine* MachineFactory::CloneMachine(Machine& mac) {
	Machine* newmac = CreateMachine(mac.getMachineKey());
	newmac->CloneFrom(mac);
	return newmac;
}

///\FIXME: This only returns the first path, should regenerate the string
std::string const & MachineFactory::getPsyclePath() const { return NativeHost::getInstance(0).getPluginPath(0); }
void MachineFactory::setPsyclePath(std::string path,bool cleardata) {
	NativeHost::getInstance(callbacks_).setPluginPath(path);
	if(!finder_->DelayedScan()) {
		NativeHost::getInstance(callbacks_).FillFinderData(*finder_,cleardata);
	}
}

///\FIXME: This only returns the first path, should regenerate the string
std::string const & MachineFactory::getLadspaPath() const { return LadspaHost::getInstance(0).getPluginPath(0); }
void MachineFactory::setLadspaPath(std::string path,bool cleardata) {
	LadspaHost::getInstance(callbacks_).setPluginPath(path);
	if(!finder_->DelayedScan()) {
		LadspaHost::getInstance(callbacks_).FillFinderData(*finder_,cleardata);
	}
}

#ifdef DIVERSALIS__OS__MICROSOFT
	///\FIXME: This only returns the first path, should regenerate the string
	std::string const & MachineFactory::getVstPath() const { return vst::host::getInstance(0).getPluginPath(0); }
	void MachineFactory::setVstPath(std::string path,bool cleardata) {
		vst::host::getInstance(callbacks_).setPluginPath(path);
		if(!finder_->DelayedScan()) {
			vst::host::getInstance(callbacks_).FillFinderData(*finder_,cleardata);
		}
	}
#endif
	
void MachineFactory::RegenerateFinderData(bool clear)  {
	if(clear) finder_->Initialize(clear);
	for(std::size_t i = 0; i < hosts_.size(); ++i) hosts_[i]->FillFinderData(*finder_, clear);
	finder_->PostInitialization();
}

}}
