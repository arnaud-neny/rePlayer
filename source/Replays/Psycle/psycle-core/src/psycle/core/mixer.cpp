// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "mixer.h"

#include "player.h"
#include "song.h"
#include "fileio.h"
#include "cpu_time_clock.hpp"
#include <psycle/helpers/math.hpp>
#include <psycle/helpers/dsp.hpp>

namespace psycle { namespace core {

using namespace helpers;
using namespace helpers::math;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mixer

std::string Mixer::_psName = "Mixer";

Mixer::Mixer(MachineCallbacks* callbacks, Machine::id_type id)
:
	Machine(callbacks, id),
	mixed(true)
{
	defineInputAsStereo(2);
	defineOutputAsStereo();
	SetEditName(_psName);
	_numPars = 255;
	SetAudioRange(32768.0f);
	//DefineStereoInput(24);
	//DefineStereoOutput(1);
}

Mixer::~Mixer() throw()
{
}

void Mixer::Init()
{
	Machine::Init();

	if (inputs_.size() != 0) inputs_.resize(0);
	if (returns_.size() != 0) returns_.resize(0);
	master_.Init();

	solocolumn_=-1;
}

void Mixer::Tick( int /*channel*/, const PatternEvent & pData )
{
///\todo: Tick is never called since mixer doesn't use Machine::GenerateAudio.
	if( pData.note()  == notetypes::tweak)
	{
		SetParameter(pData.instrument(), pData.command());
	}
	else if( pData.note() == notetypes::tweak_slide)
	{
		///\todo: Tweaks and tweak slides should not be a per-machine thing, but rather be player centric.
		// doing simply "tweak" for now..
		SetParameter(pData.instrument(), pData.command());
	}
}

void Mixer::recursive_process(unsigned int frames) {
	if(_mute || Bypass()) {
		recursive_process_deps(frames);
		return;
	}

	// Step One, do the usual work, except mixing all the inputs to a single stream.
	recursive_process_deps(frames, false);
	// Step Two, prepare input signals for the Send Fx, and make them work
	FxSend(frames, true);
	{ // Step Three, Mix the returns of the Send Fx's with the leveled input signal
		cpu_time_clock::time_point const t0(cpu_time_clock::now());
		Mix(frames);
		dsp::Undenormalize(_pSamplesL, _pSamplesR, frames);
		Machine::UpdateVuAndStanbyFlag(frames);
		cpu_time_clock::time_point const t1(cpu_time_clock::now());
		accumulate_processing_time(t1 - t0);
	}

	recursive_processed_ = true;
	return;
}

void Mixer::FxSend(int numSamples, bool recurse)
{
	//const PlayerTimeInfo & timeInfo = callbacks->timeInfo();

    for (unsigned int i=0; i<numsends(); i++)
	{
		if (sends_[i].IsValid())
		{
			Machine* pSendMachine = callbacks->song().machine(sends_[i].machine_);
			assert(pSendMachine);
			if (!pSendMachine->recursive_processed_ && !pSendMachine->recursive_is_processing_)
			{
				bool soundready=false;
				// Mix all the inputs and route them to the send fx.
				{
					//cpu::cycles_type cost(cpu::cycles());
					if ( solocolumn_ >=0 && solocolumn_ < MAX_CONNECTIONS )
					{
						int j = solocolumn_;
						if (_inputCon[j] && !Channel(j).Mute() && !Channel(j).DryOnly() && (_sendvolpl[j][i] != 0.0f || _sendvolpr[j][i] != 0.0f))
						{
							Machine* pInMachine = callbacks->song().machine(_inputMachines[j]);
							assert(pInMachine);
							if(!pInMachine->_mute && !pInMachine->Standby())
							{
								dsp::Add(pInMachine->_pSamplesL, pSendMachine->_pSamplesL, numSamples, pInMachine->lVol()*_sendvolpl[j][i]);
								dsp::Add(pInMachine->_pSamplesR, pSendMachine->_pSamplesR, numSamples, pInMachine->rVol()*_sendvolpr[j][i]);
								soundready=true;
							}
						}
					}
                    else for (unsigned int j=0; j<numinputs(); j++)
					{
						if (_inputCon[j] && !Channel(j).Mute() && !Channel(j).DryOnly() && (_sendvolpl[j][i] != 0.0f || _sendvolpr[j][i] != 0.0f ))
						{
							Machine* pInMachine = callbacks->song().machine(_inputMachines[j]);
							assert(pInMachine);
							if(!pInMachine->_mute && !pInMachine->Standby())
							{
								dsp::Add(pInMachine->_pSamplesL, pSendMachine->_pSamplesL, numSamples, pInMachine->lVol()*_sendvolpl[j][i]);
								dsp::Add(pInMachine->_pSamplesR, pSendMachine->_pSamplesR, numSamples, pInMachine->rVol()*_sendvolpr[j][i]);
								soundready=true;
							}
						}
					}
                    for (unsigned int j=0; j<i; j++)
					{
						if (Return(j).IsValid() && Return(j).Send(i) && !Return(j).Mute() && (mixvolretpl[j][i] != 0.0f || mixvolretpr[j][i] != 0.0f ))
						{
							Machine* pRetMachine = callbacks->song().machine(Return(j).Wire().machine_);
							assert(pRetMachine);
							if(!pRetMachine->_mute && !pRetMachine->Standby())
							{
								dsp::Add(pRetMachine->_pSamplesL, pSendMachine->_pSamplesL, numSamples, pRetMachine->lVol()*mixvolretpl[j][i]);
								dsp::Add(pRetMachine->_pSamplesR, pSendMachine->_pSamplesR, numSamples, pRetMachine->rVol()*mixvolretpr[j][i]);
								soundready=true;
							}
						}
					}
					if (soundready) pSendMachine->Standby(false);
					//cost = cpu::cycles() - cost;
					//work_cpu_cost(work_cpu_cost() + cost);
				}

				// tell the FX to work, now that the input is ready.
				if(recurse) {
					Machine* pRetMachine = callbacks->song().machine(Return(i).Wire().machine_);
					pRetMachine->recursive_process(numSamples);
					/// pInMachines are verified in Machine::WorkNoMix, so we only check the returns.
					if(!pRetMachine->Standby()) Standby(false);
				}
			}
		}
	}
}

void Mixer::Mix(int numSamples)
{
	if ( master_.DryWetMix() > 0.0f)
	{
		if ( solocolumn_ >= MAX_CONNECTIONS)
		{
			int i= solocolumn_-MAX_CONNECTIONS;
			if (ReturnValid(i) && !Return(i).Mute() && Return(i).MasterSend() && (mixvolretpl[i][MAX_CONNECTIONS] != 0.0f || mixvolretpr[i][MAX_CONNECTIONS] != 0.0f ))
			{
				Machine* pRetMachine = callbacks->song().machine(Return(i).Wire().machine_);
				assert(pRetMachine);
				if(!pRetMachine->_mute && !pRetMachine->Standby())
				{
					dsp::Add(pRetMachine->_pSamplesL, _pSamplesL, numSamples, pRetMachine->lVol()*mixvolretpl[i][MAX_CONNECTIONS]);
					dsp::Add(pRetMachine->_pSamplesR, _pSamplesR, numSamples, pRetMachine->rVol()*mixvolretpr[i][MAX_CONNECTIONS]);
				}
			}
		}
        else for (unsigned int i=0; i<numreturns(); i++)
		{
			if (Return(i).IsValid() && !Return(i).Mute() && Return(i).MasterSend() && (mixvolretpl[i][MAX_CONNECTIONS] != 0.0f || mixvolretpr[i][MAX_CONNECTIONS] != 0.0f ))
			{
				Machine* pRetMachine = callbacks->song().machine(Return(i).Wire().machine_);
				assert(pRetMachine);
				if(!pRetMachine->_mute && !pRetMachine->Standby())
				{
					dsp::Add(pRetMachine->_pSamplesL, _pSamplesL, numSamples, pRetMachine->lVol()*mixvolretpl[i][MAX_CONNECTIONS]);
					dsp::Add(pRetMachine->_pSamplesR, _pSamplesR, numSamples, pRetMachine->rVol()*mixvolretpr[i][MAX_CONNECTIONS]);
				}
			}
		}
	}
	if ( master_.DryWetMix() < 1.0f && solocolumn_ < MAX_CONNECTIONS)
	{
		if ( solocolumn_ >= 0)
		{
			int i = solocolumn_;
			if (ChannelValid(i) && !Channel(i).Mute() && !Channel(i).WetOnly() && (mixvolpl[i] != 0.0f || mixvolpr[i] != 0.0f ))
			{
				Machine* pInMachine = callbacks->song().machine(_inputMachines[i]);
				assert(pInMachine);
				if(!pInMachine->_mute && !pInMachine->Standby())
				{
					dsp::Add(pInMachine->_pSamplesL, _pSamplesL, numSamples, pInMachine->lVol()*mixvolpl[i]);
					dsp::Add(pInMachine->_pSamplesR, _pSamplesR, numSamples, pInMachine->rVol()*mixvolpr[i]);
				}
			}
		}
        else for (unsigned int i=0; i<numinputs(); i++)
		{
			if (_inputCon[i] && !Channel(i).Mute() && !Channel(i).WetOnly() && (mixvolpl[i] != 0.0f || mixvolpr[i] != 0.0f ))
			{
				Machine* pInMachine = callbacks->song().machine(_inputMachines[i]);
				assert(pInMachine);
				if(!pInMachine->_mute && !pInMachine->Standby())
				{
					dsp::Add(pInMachine->_pSamplesL, _pSamplesL, numSamples, pInMachine->lVol()*mixvolpl[i]);
					dsp::Add(pInMachine->_pSamplesR, _pSamplesR, numSamples, pInMachine->rVol()*mixvolpr[i]);
				}
			}
		}
	}
}

/// tells the scheduler which machines to process before this one
void Mixer::sched_inputs(sched_deps & result) const {
	if(mixed) {
		// step 1: send signal to fx
		Machine::sched_inputs(result);
	} else {
		// step 2: mix with return fx
		for(unsigned int i = 0; i < numreturns(); ++i) {
			if(Return(i).IsValid()) {
				Machine & returned(*callbacks->song().machine(Return(i).Wire().machine_));
				result.push_back(&returned);
			}
		}
	}
}

/// tells the scheduler which machines may be processed after this one
void Mixer::sched_outputs(sched_deps & result) const {
	if(!mixed) {
		// step 1: send signal to fx
        for (unsigned int i=0; i<numsends(); i++) if (Send(i).IsValid()) {
			Machine & input(*callbacks->song().machine(Send(i).machine_));
			result.push_back(&input);
		}
	} else {
		// step 2: mix with return fx
		Machine::sched_outputs(result);
	}
}

/// called by the scheduler to ask for the actual processing of the machine
bool Mixer::sched_process(unsigned int frames) {
	cpu_time_clock::time_point const t0(cpu_time_clock::now());

	if(mixed && numsends()) {
		mixed = false;
		// step 1: send signal to fx
		FxSend(frames, false);
	} else {
		// step 2: mix with return fx
		Mix(frames);
		dsp::Undenormalize(_pSamplesL, _pSamplesR, frames);
		Machine::UpdateVuAndStanbyFlag(frames);
		mixed = true;
	}

	cpu_time_clock::time_point const t1(cpu_time_clock::now());
	accumulate_processing_time(t1 - t0);
	if(mixed) ++processing_count_;
	
	return mixed;
}

void Mixer::InsertInputWire(Machine& srcMac, Wire::id_type dstWire,InPort::id_type dstType, float initialVol) 
{
	if (dstWire< MAX_CONNECTIONS)
	{
		// If we're replacing an existing connection, keep the sends
		if (ChannelValid(dstWire))
		{
			InputChannel chan = Channel(dstWire);
			InsertChannel(dstWire,&chan);
		} else {
			InsertChannel(dstWire);
		}
		Machine::InsertInputWire(srcMac,dstWire,dstType,initialVol);
		RecalcChannel(dstWire);
        for (unsigned int i(0);i<numsends();++i)
		{
			RecalcSend(dstWire,i);
		}
	}
	else
	{
		dstWire-=MAX_CONNECTIONS;
		srcMac.SetMixerSendFlag();
		MixerWire wire(srcMac.id(),0);
		// If we're replacing an existing connection
		if ( ReturnValid(dstWire))
		{
			ReturnChannel ret = Return(dstWire);
			ret.Wire() = wire;
			InsertReturn(dstWire,&ret);
			InsertSend(dstWire,wire);
		} else {
			InsertReturn(dstWire,wire);
			InsertSend(dstWire,wire);
		}
		Return(dstWire).Wire().volume_ = initialVol;
		Return(dstWire).Wire().normalize_ = srcMac.GetAudioRange()/GetAudioRange();
		sends_[dstWire].volume_ = 1.0f;
		sends_[dstWire].normalize_ = 1.0f/(srcMac.GetAudioRange()/GetAudioRange());
		RecalcReturn(dstWire);
        for(unsigned int c(0) ; c < numinputs() ; ++c)
		{
			RecalcSend(c,dstWire);
		}
	}
}
bool Mixer::MoveWireSourceTo(Machine& srcMac, InPort::id_type dsttype, Wire::id_type dstwire, OutPort::id_type srctype)
{
	if ( dstwire >= MAX_CONNECTIONS )
	{
		dsttype = 1;
		dstwire-=MAX_CONNECTIONS;
	}
	if ( dsttype == 1)
	{
		if (!ReturnValid(dstwire))
			return false;
		//FIXME: is this going to work with chained sends??
		Machine* oldSrc = callbacks->song().machine(Return(dstwire).Wire().machine_);
		if (oldSrc)
		{
			return Machine::MoveWireSourceTo(srcMac, dsttype, dstwire, srctype, *oldSrc);
		} else {
			return false;
		}

	} else {
		return Machine::MoveWireSourceTo(srcMac,dsttype,dstwire,srctype);
	}
}

void Mixer::ExchangeInputWires(Wire::id_type first,Wire::id_type second, InPort::id_type /*firstType*/, InPort::id_type /*secondType*/)
{
	// When correctly implemented, we should make use of the InPort Type,
	if ( first  < MAX_CONNECTIONS && second < MAX_CONNECTIONS)
	{
		ExchangeChans(first,second);
	} else if ( first >= MAX_CONNECTIONS && second >= MAX_CONNECTIONS) {
		ExchangeReturns(first-MAX_CONNECTIONS,second-MAX_CONNECTIONS);
	}
}

Wire::id_type Mixer::FindInputWire(Machine::id_type macIndex) const
{
	int ret=Machine::FindInputWire(macIndex);
	if ( ret == -1)
	{
        for (unsigned int c=0; c<numreturns(); c++)
		{
			if (Return(c).Wire().machine_ == macIndex)
			{
				ret = c+MAX_CONNECTIONS;
				break;
			}
		}
	}
	return ret;
}
Wire::id_type Mixer::GetFreeInputWire(InPort::id_type slotType) const
{
	if ( slotType == 0) return Machine::GetFreeInputWire(0);
	else
	{
		// Get a free sendfx slot
		for(int c(0) ; c < MAX_CONNECTIONS ; ++c)
		{
			if(!ReturnValid(c)) return c+MAX_CONNECTIONS;
		}
		return Wire::id_type(-1);
	}
}

void Mixer::DeleteInputWire(Wire::id_type wireIndex, InPort::id_type dstType)
{
	// When correctly implemented, we should make use of the InPort Type,
	if ( wireIndex < MAX_CONNECTIONS)
	{
		Machine::DeleteInputWire(wireIndex,dstType);
		DiscardChannel(wireIndex);
	}
	else
	{
		wireIndex-=MAX_CONNECTIONS;
		Machine* returnMac = callbacks->song().machine(Return(wireIndex).Wire().machine_);
		//"if" added to prevent crashes on song deletion.
		if (returnMac) returnMac->ClearMixerSendFlag();
		Return(wireIndex).Wire().machine_=-1;
		sends_[wireIndex].machine_ = -1;
		DiscardReturn(wireIndex);
		DiscardSend(wireIndex);
	}
}
void Mixer::NotifyNewSendtoMixer(Machine &caller, Machine& sender)
{
	// Mixer reached, set flags upwards.
	caller.SetMixerSendFlag();
	for (int i(0); i < MAX_CONNECTIONS; i++)
	{
		if ( ReturnValid(i))
		{
			if (Return(i).Wire().machine_ == caller.id())
			{
				sends_[i].machine_ = sender.id();
				sends_[i].normalize_ = GetAudioRange()/sender.GetAudioRange();
                for (unsigned int ch(0);ch<numinputs();ch++)
				{
					RecalcSend(ch,i);
				}
			}
		}
	}
}


void Mixer::DeleteWires(CoreSong &song)
{
	Machine::DeleteWires(song);
	Machine *iMac;
    for(unsigned int w=0; w<numreturns(); w++)
	{
		// Checking send/return Wires
		if(Return(w).IsValid())
		{
			iMac = callbacks->song().machine(Return(w).Wire().machine_);
			if (iMac)
			{
				int wix = iMac->FindOutputWire(id());
				if (wix >=0)
				{
					///\todo: get the correct port type.
					iMac->DeleteOutputWire(wix,0);
				}
			}
			///\todo: get the correct port type.
			DeleteInputWire(w+MAX_CONNECTIONS,0);
		}
	}
}
float Mixer::GetWireVolume(Wire::id_type wireIndex) const
{
	if (wireIndex< MAX_CONNECTIONS)
		return Machine::GetWireVolume(wireIndex);
	else if ( ReturnValid(wireIndex-MAX_CONNECTIONS) )
		return Return(wireIndex-MAX_CONNECTIONS).Wire().volume_;
	return 0;
}
void Mixer::SetWireVolume(Wire::id_type wireIndex,float value)
{
	if (wireIndex < MAX_CONNECTIONS)
	{
		Machine::SetWireVolume(wireIndex,value);
		if (ChannelValid(wireIndex))
		{
			RecalcChannel(wireIndex);
		}
	}
	else if (ReturnValid(wireIndex-MAX_CONNECTIONS))
	{
		Return(wireIndex-MAX_CONNECTIONS).Wire().volume_ = value;
		RecalcReturn(wireIndex-MAX_CONNECTIONS);
	}
}
std::string Mixer::GetAudioInputName(Wire::id_type wire) const
{
	std::string rettxt;
	if (wire < chanmax)
	{
		int i = wire-chan1;
		rettxt = "Input ";
		if ( i < 9 ) rettxt += ('1'+i);
		else { rettxt += '1'; rettxt += ('0'+i-9); }
		return rettxt;
	}
	else if ( wire < returnmax)
	{
		int i = wire-return1;
		rettxt = "Return ";
		if ( i < 9 ) rettxt += ('1'+i);
		else { rettxt += '1'; rettxt += ('0'+i-9); }
		return rettxt;
	}
	rettxt = "-";
	return rettxt;
}
std::string Mixer:: GetPortInputName(InPort::id_type port) const
{
	return (port==0)?"Input Port":"Send/Return Port";
}

int Mixer::GetNumCols() const
{
	return 2+numinputs()+numreturns();
}
void Mixer::GetParamRange(int numparam, int &minval, int &maxval) const
{
	int channel=numparam/16;
	int param=numparam%16;
	if ( channel == 0)
	{
		if (param == 0){ minval=0; maxval=0x1000; }
		else if (param <= 12)  {
			if (!ChannelValid(param-1)) { minval=0; maxval=0; }
			else { minval=0; maxval=0x1000; }
		}
		else if (param == 13) { minval=0; maxval=0x100; }
		else if (param == 14) { minval=0; maxval=0x400; }
		else  { minval=0; maxval=0x100; }
	}
	else if (channel <= 12 )
	{
		if (!ChannelValid(channel-1)) { minval=0; maxval=0; }
		else if (param == 0) { minval=0; maxval=0x100; }
		else if (param <= 12) {
			if(!ReturnValid(param-1)) { minval=0; maxval=0; }
			else { minval=0; maxval=0x100; }
		}
		else if (param == 13) { minval=0; maxval=3; }
		else if (param == 14) { minval=0; maxval=0x400; }
		else  { minval=0; maxval=0x100; }
	}
	else if ( channel == 13)
	{
		if ( param > 12) { minval=0; maxval=0; }
		else if ( param == 0 ) { minval=0; maxval=24; }
		else if (!ReturnValid(param-1)) { minval=0; maxval=0; }
		else { minval=0; maxval=(1<<14)-1; }
	}
	else if ( channel == 14)
	{
		if ( param == 0 || param > 12) { minval=0; maxval=0; }
		else if (!ReturnValid(param-1)) { minval=0; maxval=0; }
		else { minval=0; maxval=0x1000; }
	}
	else if ( channel == 15)
	{
		if ( param == 0 || param > 12) { minval=0; maxval=0; }
		else if (!ReturnValid(param-1)) { minval=0; maxval=0; }
		else { minval=0; maxval=0x100; }
	}
	else { minval=0; maxval=0; }
}
void Mixer::GetParamName(int numparam,char *name) const
{
	int channel=numparam/16;
	int param=numparam%16;
	if ( channel == 0)
	{
		if (param == 0) strcpy(name,"Master - Output");
		else if (param <= 12) sprintf(name,"Channel %d - Volume",param);
		else if (param == 13) strcpy(name,"Master - Mix");
		else if (param == 14) strcpy(name,"Master - Gain");
		else strcpy(name,"Master - Panning");
	}
	else if (channel <= 12 )
	{
		if (param == 0) sprintf(name,"Channel %d - Dry mix",channel);
		else if (param <= 12) sprintf(name,"Chan %d Send %d - Amount",channel,param);
		else if (param == 13) sprintf(name,"Channel %d  - Mix type",channel);
		else if (param == 14) sprintf(name,"Channel %d - Gain",channel);
		else sprintf(name,"Channel %d - Panning",channel);
	}
	else if ( channel == 13)
	{
		if (param > 12) strcpy(name,"");
		else if (param == 0) strcpy(name,"Set Channel Solo");
		else sprintf(name,"Return %d - Route Array",param);
	}
	else if ( channel == 14)
	{
		if ( param == 0 || param > 12) strcpy(name,"");
		else sprintf(name,"Return %d - Volume",param);
	}
	else if ( channel == 15)
	{
		if ( param == 0 || param > 12) strcpy(name,"");
		else sprintf(name,"Return %d - Panning",param);
	}
	else strcpy(name,"");
}

int Mixer::GetParamValue(int numparam) const
{
	int channel=numparam/16;
	int param=numparam%16;
	if ( channel == 0)
	{
		if (param == 0)
		{
			float dbs = dsp::dB(master_.Volume());
			return (dbs+96.0f)*42.67; // *(0x1000 / 96.0f)
		}
		else if (param <= 12)
		{
			if (!ChannelValid(param-1)) return 0;
			else {
				float dbs = dsp::dB(Channel(param-1).Volume());
				return (dbs+96.0f)*42.67; // *(0x1000 / 96.0f)
			}
		}
		else if (param == 13) return master_.DryWetMix()*0x100;
		else if (param == 14) return master_.Gain()*0x100;
		else return _panning*2;
	}
	else if (channel <= 12 )
	{
		if (!ChannelValid(channel-1)) return 0;
		else if (param == 0) return Channel(channel-1).DryMix()*0x100;
		else if (param <= 12)
		{
			if ( !ReturnValid(param-1)) return 0;
			else return Channel(channel-1).Send(param-1)*0x100;
		}
		else if (param == 13)
		{
			if (Channel(channel-1).Mute()) return 3;
			else if (Channel(channel-1).WetOnly()) return 2;
			else if (Channel(channel-1).DryOnly()) return 1;
			return 0;
		}
		else if (param == 14) { float val; GetWireVolume(channel-1,val); return val*0x100; }
		else return Channel(channel-1).Panning()*0x100;
	}
	else if ( channel == 13)
	{
		if ( param > 12) return 0;
		else if (param == 0 ) return solocolumn_+1;
		else if ( !ReturnValid(param-1)) return 0;
		else
		{
			int val(0);
			if (Return(param-1).Mute()) val|=1;
            for (unsigned int i(0);i<numreturns();i++)
			{
				if (Return(param-1).Send(i)) val|=(2<<i);
			}
			if (Return(param-1).MasterSend()) val|=(1<<13);
			return val;
		}
	}
	else if ( channel == 14)
	{
		if ( param == 0 || param > 12) return 0;
		else if ( !ReturnValid(param-1)) return 0;
		else
		{
			float dbs = dsp::dB(Return(param-1).Volume());
			return (dbs+96.0f)*42.67; // *(0x1000 / 96.0f)
		}
	}
	else if ( channel == 15)
	{
		if ( param == 0 || param > 12) return 0;
		else if ( !ReturnValid(param-1)) return 0;
		else return Return(param-1).Panning()*0x100;
	}
	else return 0;
}

void Mixer::GetParamValue(int numparam, char *parVal) const
{
	int channel=numparam/16;
	int param=numparam%16;
	parVal[0]='\0';
	if ( channel == 0)
	{
		if (param == 0)
		{
			if (master_.Volume() < 0.00002f ) strcpy(parVal,"-inf");
			else
			{
				float dbs = dsp::dB(master_.Volume());
				sprintf(parVal,"%.01fdB",dbs);
			}
		}
		else if (param <= 12)
		{
			if (!ChannelValid(param-1)) return;
			else if (Channel(param-1).Volume() < 0.00002f ) strcpy(parVal,"-inf");
			else
			{
				float dbs = dsp::dB(Channel(param-1).Volume());
				sprintf(parVal,"%.01fdB",dbs);
			}
		}
		else if (param == 13)
		{
			if (master_.DryWetMix() == 0.0f) strcpy(parVal,"Dry");
			else if (master_.DryWetMix() == 1.0f) strcpy(parVal,"Wet");
			else sprintf(parVal,"%.0f%%",master_.DryWetMix()*100.0f);
		}
		else if (param == 14)
		{
			float val = master_.Gain();
			float dbs = (((val>0.0f)?dsp::dB(val):-100.0f));
			sprintf(parVal,"%.01fdB",dbs);
		}
		else
		{
			if (_panning == 0) strcpy(parVal,"left");
			else if (_panning == 128) strcpy(parVal,"right");
			else if (_panning == 64) strcpy(parVal,"center");
			else sprintf(parVal,"%.0f%%",_panning*0.78125f);
		}
	}
	else if (channel <= 12 )
	{
		if (!ChannelValid(channel-1)) return;
		else if (param == 0)
		{
			if (Channel(channel-1).DryMix() == 0.0f) strcpy(parVal,"Off");
			else sprintf(parVal,"%.0f%%",Channel(channel-1).DryMix()*100.0f);
		}
		else if (param <= 12)
		{
			if ( !ReturnValid(param-1)) return;
			else if (Channel(channel-1).Send(param-1) == 0.0f) strcpy(parVal,"Off");
			else sprintf(parVal,"%.0f%%",Channel(channel-1).Send(param-1)*100.0f);
		}
		else if (param == 13)
		{
			parVal[0]= (Channel(channel-1).DryOnly())?'D':' ';
			parVal[1]= (Channel(channel-1).WetOnly())?'W':' ';
			parVal[2]= (Channel(channel-1).Mute())?'M':' ';
			parVal[3]='\0';
		}
		else if (param == 14)
		{
			float val;
			GetWireVolume(channel-1,val);
			float dbs = (((val>0.0f)?dsp::dB(val):-100.0f));
			sprintf(parVal,"%.01fdB",dbs);
		}
		else
		{
			if (Channel(channel-1).Panning()== 0.0f) strcpy(parVal,"left");
			else if (Channel(channel-1).Panning()== 1.0f) strcpy(parVal,"right");
			else if (Channel(channel-1).Panning()== 0.5f) strcpy(parVal,"center");
			else sprintf(parVal,"%.0f%%",Channel(channel-1).Panning()*100.0f);
		}
	}
	else if ( channel == 13)
	{
		if ( param > 12) return;
		else if (param == 0 ){ sprintf(parVal,"%d",solocolumn_+1); }
		else if ( !ReturnValid(param-1))  return;
		else
		{
			parVal[0]= (Return(param-1).Mute())?'M':' ';
			parVal[1]= (Return(param-1).Send(0))?'1':' ';
			parVal[2]= (Return(param-1).Send(1))?'2':' ';
			parVal[3]= (Return(param-1).Send(2))?'3':' ';
			parVal[4]= (Return(param-1).Send(3))?'4':' ';
			parVal[5]= (Return(param-1).Send(4))?'5':' ';
			parVal[6]= (Return(param-1).Send(5))?'6':' ';
			parVal[7]= (Return(param-1).Send(6))?'7':' ';
			parVal[8]= (Return(param-1).Send(7))?'8':' ';
			parVal[9]= (Return(param-1).Send(8))?'9':' ';
			parVal[10]= (Return(param-1).Send(9))?'A':' ';
			parVal[11]= (Return(param-1).Send(10))?'B':' ';
			parVal[12]= (Return(param-1).Send(11))?'C':' ';
			parVal[13]= (Return(param-1).MasterSend())?'O':' ';
			parVal[14]= '\0';
		}
	}
	else if ( channel == 14)
	{
		if ( param == 0 || param > 12) return;
		else if ( !ReturnValid(param-1)) return;
		else if (Return(param-1).Volume() < 0.00002f ) strcpy(parVal,"-inf");
		else
		{
			float dbs = dsp::dB(Return(param-1).Volume());
			sprintf(parVal,"%.01fdB",dbs);
		}
	}
	else if ( channel == 15)
	{
		if ( param == 0 || param > 12) return;
		else if ( !ReturnValid(param-1)) return;
		else
		{
			if (Return(param-1).Panning()== 0.0f) strcpy(parVal,"left");
			else if (Return(param-1).Panning()== 1.0f) strcpy(parVal,"right");
			else if (Return(param-1).Panning()== 0.5f) strcpy(parVal,"center");
			else sprintf(parVal,"%.0f%%",Return(param-1).Panning()*100.0f);
		}
	}
}

bool Mixer::SetParameter(int numparam, int value)
{
	int channel=numparam/16;
	int param=numparam%16;
	if ( channel == 0)
	{
		if (param == 0)
		{
			if ( value >= 0x1000) master_.Volume()=1.0f;
			else if ( value == 0) master_.Volume()=0.0f;
			else
			{
				float dbs = (value/42.67f)-96.0f;
				master_.Volume() = dsp::dB2Amp(dbs);
			}
			RecalcMaster();
		}
		else if (param <= 12)
		{
			if (!ChannelValid(param-1)) return false;
			else if ( value >= 0x1000) Channel(param-1).Volume()=1.0f;
			else if ( value == 0) Channel(param-1).Volume()=0.0f;
			else
			{
				float dbs = (value/42.67f)-96.0f;
				Channel(param-1).Volume() = dsp::dB2Amp(dbs);
			}
			RecalcChannel(param-1);
		}
		else if (param == 13) { master_.DryWetMix() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcMaster(); }
		else if (param == 14) { master_.Gain() = (value>=1024)?4.0f:((value&0x3FF)/256.0f); RecalcMaster(); }
		else SetPan(value>>1);
		return true;
	}
	else if (channel <= 12 )
	{
		if (!ChannelValid(channel-1)) return false;
		if (param == 0) { Channel(channel-1).DryMix() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcChannel(channel-1); }
		else if (param <= 12) { Channel(channel-1).Send(param-1) = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcSend(channel-1,param-1); }
		else if (param == 13)
		{
			Channel(channel-1).Mute() = (value == 3)?true:false;
			Channel(channel-1).WetOnly() = (value==2)?true:false;
			Channel(channel-1).DryOnly() = (value==1)?true:false;
		}
		else if (param == 14) { float val=(value>=1024)?4.0f:((value&0x3FF)/256.0f); SetWireVolume(channel-1,val); RecalcChannel(channel-1); }
		else { Channel(channel-1).Panning() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcChannel(channel-1); }
		return true;
	}
	else if ( channel == 13)
	{
		if ( param > 12) return false;
		else if (param == 0) solocolumn_ = (value<24)?value-1:23;
		else if (!ReturnValid(param-1)) return false;
		else
		{
			Return(param-1).Mute() = (value&1)?true:false;
            for (unsigned int i(param);i<numreturns();i++)
			{
				Return(param-1).Send(i,(value&(2<<i))?true:false);
			}
			Return(param-1).MasterSend() = (value&(1<<13))?true:false;
			RecalcReturn(param-1);
		}
		return true;
	}
	else if ( channel == 14)
	{
		if ( param == 0 || param > 12) return false;
		else if (!ReturnValid(param-1)) return false;
		else
		{
			if ( value >= 0x1000) Return(param-1).Volume()=1.0f;
			else if ( value == 0) Return(param-1).Volume()=0.0f;
			else
			{
				float dbs = (value/42.67f)-96.0f;
				Return(param-1).Volume() = dsp::dB2Amp(dbs);
			}
			RecalcReturn(param-1);
		}
		return true;
	}
	else if ( channel == 15)
	{
		if ( param == 0 || param > 12) return false;
		else if (!ReturnValid(param-1)) return false;
		else { Return(param-1).Panning() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcReturn(param-1); }
		return true;
	}
	return false;
}



float Mixer::VuChan(Wire::id_type idx) const
{
	if ( _inputCon[idx] )
	{
		//Note that since volumeDisplay is integer, when using positive gain,
		//the result can visually differ from the calculated one
		float vol;
		GetWireVolume(idx,vol);
		vol*=Channel(idx).Volume();
		int temp(round<int>(50.0f * std::log10(vol)));
		return (callbacks->song().machine(_inputMachines[idx])->_volumeDisplay+temp)/97.0f;
	}
	return 0.0f;
}

float Mixer::VuSend(Wire::id_type idx) const
{
	if ( SendValid(idx) )
	{
		//Note that since volumeDisplay is integer, when using positive gain,
		// the result can visually differ from the calculated one
		float vol;
		GetWireVolume(idx+MAX_CONNECTIONS,vol);
		vol *= Return(idx).Volume();
		int temp(round<int>(50.0f * std::log10(vol)));
		return (callbacks->song().machine(Return(idx).Wire().machine_)->_volumeDisplay+temp)/97.0f;
	}
	return 0.0f;
}
void Mixer::InsertChannel(unsigned int idx,InputChannel*input)
{
	assert(idx<MAX_CONNECTIONS);
	if ( idx >= numinputs())
	{
        for(unsigned int i=numinputs(); i<idx; ++i)
		{
			inputs_.push_back(InputChannel(numsends()));
		}
		if (input) inputs_.push_back(*input);
		else inputs_.push_back(InputChannel(numsends()));
	}
	else if (input) inputs_[idx]=*input;
	else { inputs_[idx].Init(); inputs_[idx].ResizeTo(numsends()); }
}
void Mixer::InsertReturn(unsigned int idx,ReturnChannel* retchan)
{
	assert(idx<MAX_CONNECTIONS);
	if ( idx >= numreturns())
	{
        for(unsigned int i=numreturns(); i<idx; ++i)
		{
			returns_.push_back(ReturnChannel(numsends()));
		}
		if (retchan) returns_.push_back(*retchan);
		else returns_.push_back(ReturnChannel(numsends()));
        for(unsigned int i=0; i<numinputs(); ++i)
		{
			Channel(i).ResizeTo(numsends());
		}
        for(unsigned int i=0; i<numreturns(); ++i)
		{
			Return(i).ResizeTo(numsends());
		}
	}
	else if (retchan) returns_[idx]=*retchan;
	else { returns_[idx].Init(); returns_[idx].ResizeTo(numsends());}
}

void Mixer::InsertSend(unsigned int idx,MixerWire swire)
{
	assert(idx<MAX_CONNECTIONS);
	if ( idx >= numsends())
	{
        for(unsigned int i=numsends(); i<idx; ++i)
		{
			sends_.push_back(MixerWire());
		}
		sends_.push_back(swire);
	}
	else sends_[idx]=swire;
    for(unsigned int i=0; i<numinputs(); ++i)
	{
		Channel(i).ResizeTo(numsends());
	}
    for(unsigned int i=0; i<numreturns(); ++i)
	{
		Return(i).ResizeTo(numsends());
	}
}
void Mixer::DiscardChannel(unsigned int idx)
{
	assert(idx<MAX_CONNECTIONS);
	if (idx!=numinputs()-1) return;
	int i;
	for (i = idx; i >= 0; i--)
	{
		if (_inputCon[i])
			break;
	}
	inputs_.resize(i+1);
}
void Mixer::DiscardReturn(unsigned int idx)
{
	assert(idx<MAX_CONNECTIONS);
	if (idx!=numreturns()-1) return;
    unsigned int i;
    for (i = idx; ; i--)
	{
		if (Return(i).IsValid())
			break;
	}
	returns_.resize(i+1);
}
void Mixer::DiscardSend(unsigned int idx)
{
	assert(idx<MAX_CONNECTIONS);
	if (idx!=numsends()-1) return;
	int i;
	for (i = idx; i >= 0; i--)
	{
		if (sends_[i].machine_ != -1)
			break;
	}
	sends_.resize(i+1);
}


void Mixer::ExchangeChans(int chann1,int chann2)
{
	Machine::ExchangeInputWires(chann1,chann2);
	InputChannel tmp = inputs_[chann1];
	inputs_[chann1] = inputs_[chann2];
	inputs_[chann2] = tmp;
	RecalcChannel(chann1);
	RecalcChannel(chann2);
	//The following line autocleans the left-out channels at the end.
	DiscardChannel(numinputs()-1);
}
void Mixer::ExchangeReturns(int chann1,int chann2)
{
	ReturnChannel tmp = returns_[chann1];
	returns_[chann1] = returns_[chann2];
	returns_[chann2] = tmp;
	RecalcReturn(chann1);
	RecalcReturn(chann2);
	ExchangeSends(chann1,chann2);
	//The following lines autoclean the left-out send/returns at the end.
	DiscardReturn(numreturns()-1);
	DiscardSend(numsends()-1);
}
void Mixer::ExchangeSends(int send1,int send2)
{
	MixerWire tmp = sends_[send1];
	sends_[send1] = sends_[send2];
	sends_[send2] = tmp;
    for (unsigned int i = 0; i < numinputs(); ++i)
	{
		Channel(i).ExchangeSends(send1,send2);
		RecalcSend(i,send1);
		RecalcSend(i,send2);
	}
    for (unsigned int i = 0; i < numreturns(); ++i)
	{
		Return(i).ExchangeSends(send1,send2);
	}
}
void Mixer::ResizeTo(int inputs, int sends)
{
	inputs_.resize(inputs);
	returns_.resize(sends);
	sends_.resize(sends);
    for(unsigned int i=0; i<numinputs(); ++i)
	{
		Channel(i).ResizeTo(numsends());
	}
    for(unsigned int i=0; i<numreturns(); ++i)
	{
		Return(i).ResizeTo(numsends());
	}
}
void Mixer::RecalcMaster()
{
    for (unsigned int i = 0;i<numinputs();i++)
	{
		if (_inputCon[i]) RecalcChannel(i);
	}
    for (unsigned int i = 0;i<numreturns();i++)
	{
		if (Return(i).IsValid()) RecalcReturn(i);
	}
}
void Mixer::RecalcReturn(int idx)
{
	assert(idx<MAX_CONNECTIONS);
	float val;
	GetWireVolume(idx,val);

    for(unsigned int send=0; send < numsends(); ++send)
	{
		if (Return(idx).Send(send))
		{
			mixvolretpl[idx][send] = mixvolretpr[idx][send] = Return(idx).Volume()*( Return(idx).Wire().normalize_/ Send(send).normalize_);
			if (Return(idx).Panning() >= 0.5f )
			{
				mixvolretpl[idx][send] *= (1.0f-Return(idx).Panning())*2.0f;
			}
			else mixvolretpr[idx][send] *= (Return(idx).Panning())*2.0f;
		}
	}
	float wet = master_.Volume()*master_.Gain();
	if (master_.DryWetMix() < 0.5f )
	{
		wet *= (master_.DryWetMix())*2.0f;
	}

	mixvolretpl[idx][MAX_CONNECTIONS] = mixvolretpr[idx][MAX_CONNECTIONS] = Return(idx).Volume()*val*wet/Return(idx).Wire().normalize_;
	if (Return(idx).Panning() >= 0.5f )
	{
		mixvolretpl[idx][MAX_CONNECTIONS] *= (1.0f-Return(idx).Panning())*2.0f;
	}
	else mixvolretpr[idx][MAX_CONNECTIONS] *= (Return(idx).Panning())*2.0f;
}
void Mixer::RecalcChannel(int idx)
{
	assert(idx<MAX_CONNECTIONS);
	float val;
	GetWireVolume(idx,val);

	float dry = master_.Volume()*master_.Gain();
	if (master_.DryWetMix() > 0.5f )
	{
		dry *= (1.0f-master_.DryWetMix())*2.0f;
	}

	mixvolpl[idx] = mixvolpr[idx] = Channel(idx).Volume()*val*Channel(idx).DryMix()*dry/_wireMultiplier[idx];
	if (Channel(idx).Panning() >= 0.5f )
	{
		mixvolpl[idx] *= (1.0f-Channel(idx).Panning())*2.0f;
	}
	else mixvolpr[idx] *= (Channel(idx).Panning())*2.0f;
    for (unsigned int i(0);i<numsends();i++) RecalcSend(idx,i);
}
void Mixer::RecalcSend(int chan,int send)
{
	assert(chan<MAX_CONNECTIONS);
	assert(send<MAX_CONNECTIONS);
	float val;
	GetWireVolume(chan,val);

	_sendvolpl[chan][send] =  _sendvolpr[chan][send] = Channel(chan).Volume()*val*Channel(chan).Send(send)/(sends_[send].normalize_*_wireMultiplier[chan]);
	if (Channel(chan).Panning() >= 0.5f )
	{
		_sendvolpl[chan][send] *= (1.0f-Channel(chan).Panning())*2.0f;
	}
	else _sendvolpr[chan][send] *= (Channel(chan).Panning())*2.0f;
}
bool Mixer::LoadSpecificChunk(RiffFile* pFile, int /*version*/)
{
	uint32_t filesize;
	pFile->Read(filesize);

	pFile->Read(solocolumn_);
	pFile->Read(master_.Volume());
	pFile->Read(master_.Gain());
	pFile->Read(master_.DryWetMix());

	int numins(0),numrets(0);
	pFile->Read(numins);
	pFile->Read(numrets);
	if ( numins >0 ) InsertChannel(numins-1);
	if ( numrets >0 ) InsertReturn(numrets-1);
	if ( numrets >0 ) InsertSend(numrets-1,MixerWire());
    for (unsigned int i(0);i<numinputs();i++)
	{
        for (unsigned int j(0);j<numsends();j++)
		{
			float send(0.0f);
			pFile->Read(send);
			Channel(i).Send(j)=send;
		}
		pFile->Read(Channel(i).Volume());
		pFile->Read(Channel(i).Panning());
		pFile->Read(Channel(i).DryMix());
		pFile->Read(Channel(i).Mute());
		pFile->Read(Channel(i).DryOnly());
		pFile->Read(Channel(i).WetOnly());
	}
    for (unsigned int i(0);i<numreturns();i++)
	{
		pFile->Read(Return(i).Wire().machine_);
		pFile->Read(Return(i).Wire().volume_);
		pFile->Read(Return(i).Wire().normalize_);
		pFile->Read(sends_[i].machine_);
		pFile->Read(sends_[i].volume_);
		pFile->Read(sends_[i].normalize_);
        for (unsigned int j(0);j<numsends();j++)
		{
			bool send(false);
			pFile->Read(send);
			Return(i).Send(j,send);
		}
		pFile->Read(Return(i).MasterSend());
		pFile->Read(Return(i).Volume());
		pFile->Read(Return(i).Panning());
		pFile->Read(Return(i).Mute());
	}
	RecalcMaster();
    for (unsigned int i(0);i<numinputs();i++)
        for(unsigned int j(0);j<numsends();j++)
			RecalcSend(i,j);
	return true;
}

void Mixer::SaveSpecificChunk(RiffFile* pFile) const
{
	uint32_t size(sizeof(solocolumn_)+sizeof(master_)+2*sizeof(int));
	size+=(3*sizeof(float)+3*sizeof(bool)+numsends()*sizeof(float))*numinputs();
	size+=(2*sizeof(float)+2*sizeof(bool)+numsends()*sizeof(bool)+2*sizeof(float)+sizeof(int))*numreturns();
	size+=(2*sizeof(float)+sizeof(int))*numsends();
	pFile->Write(size);

	pFile->Write(solocolumn_);
	pFile->Write(master_.Volume());
	pFile->Write(master_.Gain());
	pFile->Write(master_.DryWetMix());

	const int numins = numinputs();
	const int numrets = numreturns();
	pFile->Write(numins);
	pFile->Write(numrets);
    for (unsigned int i(0);i<numinputs();i++)
	{
        for (unsigned int j(0);j<numsends();j++)
		{
			pFile->Write(Channel(i).Send(j));
		}
		pFile->Write(Channel(i).Volume());
		pFile->Write(Channel(i).Panning());
		pFile->Write(Channel(i).DryMix());
		pFile->Write(Channel(i).Mute());
		pFile->Write(Channel(i).DryOnly());
		pFile->Write(Channel(i).WetOnly());
	}
    for (unsigned int i(0);i<numreturns();i++)
	{
		pFile->Write(Return(i).Wire().machine_);
		pFile->Write(Return(i).Wire().volume_);
		pFile->Write(Return(i).Wire().normalize_);
		pFile->Write(sends_[i].machine_);
		pFile->Write(sends_[i].volume_);
		pFile->Write(sends_[i].normalize_);
        for (unsigned int j(0);j<numsends();j++)
		{
			bool send(Return(i).Send(j));
			pFile->Write(send);
		}
		pFile->Write(Return(i).MasterSend());
		pFile->Write(Return(i).Volume());
		pFile->Write(Return(i).Panning());
		pFile->Write(Return(i).Mute());
	}
}

}}
