// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "ladspamachine.h"

#include <psycle/helpers/dsp.hpp>
#include "player.h"
#include "fileio.h"

#include <iostream> // only for debug output
#include <sstream>

#if defined __unix__ || defined __APPLE__
	#include <dlfcn.h>
#else
	#include <windows.h>
#endif

namespace psycle { namespace core {

using namespace helpers;
	
		///\todo: Improve the case where no min/max limit is given by the plugin (Example: the amp.so doesn't have a max value).
		LadspaParam::LadspaParam(LADSPA_PortDescriptor descriptor,LADSPA_PortRangeHint hint, const char *newname)
		:descriptor_(descriptor)
		,hint_(hint)
		,portName_(newname)
		,rangeMultiplier_(1)
		,integer_(false)
		,logarithmic_(false)
		{
			if (LADSPA_IS_HINT_TOGGLED(hint.HintDescriptor)) {
				minVal_= 0; maxVal_ = 1; integer_= true;
			}
			else 
			{
				if (LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor)) {
					minVal_= hint.LowerBound;
				}
				else minVal_ = 0;
				if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor)) {
					maxVal_ = hint.UpperBound;
				}
				else maxVal_ = 1;
				if (LADSPA_IS_HINT_SAMPLE_RATE(hint.HintDescriptor)) {
					maxVal_*=Player::singleton().timeInfo().sampleRate();
				}

				if ( LADSPA_IS_HINT_LOGARITHMIC(hint.HintDescriptor) ){
					logarithmic_ = true;
					// rangeMultiplier_ =   9 / (maxVal_ - minVal_);
					rangeMultiplier_ =   (exp(1.0)-1) / (maxVal_ - minVal_);
				}
				else if ( LADSPA_IS_HINT_INTEGER(hint.HintDescriptor) ){
					integer_ = true;
				} else {
					rangeMultiplier_ =   65535.0f / (maxVal_ - minVal_);
				}
			}
			setDefault();
			std::cout << "min/max/def" << minVal_ << "/" << maxVal_ << "/" << value_ << std::endl;
		}
		
		void LadspaParam::setDefault()
		{
			LADSPA_Data fDefault=0.0f;
			switch (hint_.HintDescriptor & LADSPA_HINT_DEFAULT_MASK) {
			case LADSPA_HINT_DEFAULT_NONE:
				break;
			case LADSPA_HINT_DEFAULT_MINIMUM:
				fDefault = hint_.LowerBound * (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				break;
			case LADSPA_HINT_DEFAULT_LOW:
				if (LADSPA_IS_HINT_LOGARITHMIC(hint_.HintDescriptor)) {
					fDefault 
					= exp(log(hint_.LowerBound) * 0.75
					+ log(hint_.UpperBound) * 0.25) * (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				}
				else {
					fDefault 
					= (hint_.LowerBound * 0.75
					+ hint_.UpperBound * 0.25)* (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				}
				break;
			case LADSPA_HINT_DEFAULT_MIDDLE:
				if (LADSPA_IS_HINT_LOGARITHMIC(hint_.HintDescriptor)) {
					fDefault 
					= sqrt(hint_.LowerBound
					* hint_.UpperBound) * (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				}
				else {
					fDefault 
					= 0.5 * (hint_.LowerBound
					+ hint_.UpperBound) * (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				}
				break;
			case LADSPA_HINT_DEFAULT_HIGH:
				if (LADSPA_IS_HINT_LOGARITHMIC(hint_.HintDescriptor)) {
					fDefault 
					= exp(log(hint_.LowerBound) * 0.25
					+ log(hint_.UpperBound) * 0.75) * (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				}
				else {
					fDefault 
					= (hint_.LowerBound * 0.25
					+ hint_.UpperBound * 0.75) * (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				}
				break;
			case LADSPA_HINT_DEFAULT_MAXIMUM:
				fDefault = hint_.UpperBound* (float)((LADSPA_IS_HINT_SAMPLE_RATE(hint_.HintDescriptor)) ? (float)Player::singleton().timeInfo().sampleRate() : 1.0f);
				break;
			case LADSPA_HINT_DEFAULT_0:
				fDefault=0.0f;
				break;
			case LADSPA_HINT_DEFAULT_1:
				fDefault=1.0f;
				break;
			case LADSPA_HINT_DEFAULT_100:
				fDefault=100.0f;
				break;
			case LADSPA_HINT_DEFAULT_440:
				fDefault=440.0f;
				break;
			default:
				break;
			}
			value_ = fDefault;
		}
		
		int LadspaParam::value() const
		{ 
			return (integer_)? value_ :
				// (logarithmic_) ? log10(1+((value_-minVal_)*rangeMultiplier_))*65535.0f:
				(logarithmic_) ?  log(1 + ((value_ - minVal_) * rangeMultiplier_)) * 65535.0f:
				(value_- minVal_)*rangeMultiplier_;
		}
		void LadspaParam::setValue(int data)
		{
			value_ = (integer_) ? data :
				// (logarithmic_) ? minVal_ + (pow(10, data/65535.0f)-1)/ rangeMultiplier_ :
				(logarithmic_) ? minVal_ + (exp(data / 65535.0f) - 1) / rangeMultiplier_ :
				minVal_+ (data/rangeMultiplier_);
		}
		
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
		LADSPAMachine::LADSPAMachine(MachineCallbacks* callbacks, MachineKey key, Machine::id_type id,
			void* libHandle, const LADSPA_Descriptor* psDescriptor1,LADSPA_Handle pluginHandle1)
		: Machine(callbacks,id)
		, key_(key)
		, libHandle_(libHandle)
		, psDescriptor(psDescriptor1)
		, pluginHandle(pluginHandle1)
		{
			SetEditName(GetName());
			SetAudioRange(1.0f);
			//pOutSamplesL= new LADSPA_Data[MAX_BUFFER_LENGTH];
			//pOutSamplesR= new LADSPA_Data[MAX_BUFFER_LENGTH];
			// Step five: Prepare the structures to use the plugin with the program.
			std::cout << "step five" << std::endl;
			prepareStructures();
			defineInputAsStereo();
			defineOutputAsStereo();
			// and switch on:
			std::cout << "step six" << std::endl;
		
			if (psDescriptor->activate) psDescriptor->activate(pluginHandle);
		}
		
		LADSPAMachine::~LADSPAMachine() throw()
		{
			if ( pluginHandle )
			{
				if ( psDescriptor->deactivate ) psDescriptor->deactivate(pluginHandle);
				psDescriptor->cleanup(pluginHandle);
				pluginHandle=0;
				psDescriptor=0;
			}
			if ( libHandle_ ) {
				#if defined __unix__ || defined __APPLE__
				dlclose(libHandle_);
				#else
				::FreeLibrary( static_cast<HINSTANCE> ( libHandle_ ) ) ;
				#endif

			}
			//delete[] pOutSamplesL;
			//delete[] pOutSamplesR;
		}
		
		void LADSPAMachine::prepareStructures()
		{
			// Controls
			LADSPA_Data *ppbuffersIn[]={_pSamplesL,_pSamplesR};
			LADSPA_Data *ppbuffersOut[]={_pSamplesL,_pSamplesR};
			
			//we're passing addresses within the vector to the plugin.. let's be sure they don't move around
			values_.reserve(psDescriptor->PortCount);
			_numPars=0;
			int indexinput(0),indexoutput(0);
			for (unsigned int lPortIndex = 0; lPortIndex < psDescriptor->PortCount; lPortIndex++) {
				LADSPA_PortDescriptor iPortDescriptor = psDescriptor->PortDescriptors[lPortIndex];
				if (LADSPA_IS_PORT_CONTROL(iPortDescriptor)) {
					LadspaParam parameter(iPortDescriptor,psDescriptor->PortRangeHints[lPortIndex],psDescriptor->PortNames[lPortIndex]);

					values_[_numPars]=parameter;
					psDescriptor->connect_port(pluginHandle,lPortIndex,
						values_[_numPars].valueaddress());
					_numPars++;
				}
				else if (LADSPA_IS_PORT_AUDIO(iPortDescriptor))
				{
					// Only Stereo for now.
					if (LADSPA_IS_PORT_INPUT(iPortDescriptor)  && indexinput < 2 ) {
						psDescriptor->connect_port(pluginHandle,lPortIndex,
							ppbuffersIn[indexinput++]);
						}
					else if (LADSPA_IS_PORT_OUTPUT(iPortDescriptor)  && indexoutput < 2 ) {
						psDescriptor->connect_port(pluginHandle,lPortIndex,
							ppbuffersOut[indexoutput++]);
						}
				}
			}
			_nCols = (GetNumParams()/12)+1;
		}
		
		void LADSPAMachine::Init()
		{
			Machine::Init();
			// Not sure what should we do here.
			SetDefaultsForControls();
		}
		void LADSPAMachine::Tick(int /*channel*/, const PatternEvent & pEntry )
		{
			if ( pEntry.note() == notetypes::tweak || pEntry.note() == notetypes::tweak_slide)
			{
				SetParameter(pEntry.instrument(),pEntry.command()*0x100+pEntry.parameter());
			}
		}
		
		int LADSPAMachine::GenerateAudio(int numSamples )
		{
			if(!_mute && !_bypass && !_standby)
				psDescriptor->run(pluginHandle,numSamples);
			//std::swap(_pSamplesL,pOutSamplesL);
			//std::swap(_pSamplesR,pOutSamplesR);
			return numSamples;
		}
		void  LADSPAMachine::GetParamName(int numparam, char * name) const
		{
			strcpy(name,values_[numparam].name());
		}
		void  LADSPAMachine::GetParamRange(int numparam,int &minval, int &maxval) const
		{
			minval=values_[numparam].minval();
			maxval=values_[numparam].maxval();
		}
		
		int LADSPAMachine::GetParamValue(int numparam) const { return values_[numparam].value(); } 
		
		void LADSPAMachine::GetParamValue(int numparam,char* parval) const
		{
			LADSPA_PortRangeHintDescriptor iHintDescriptor = values_[numparam].hint();
			float value = values_[numparam].rawvalue();
			if (LADSPA_IS_HINT_TOGGLED(iHintDescriptor))
			{
				std::strcpy(parval, (value>0.0)?"on":"off");
			}
			else if (LADSPA_IS_HINT_INTEGER(iHintDescriptor)) 
			{
				std::sprintf(parval, "%.0f", value);
			}
			else
			{
				std::sprintf(parval, "%.4f", value);
			}
		}
		
		bool  LADSPAMachine::SetParameter(int numparam,int value)
		{
			values_[numparam].setValue(value); return true;
		}
		
		void LADSPAMachine::SetDefaultsForControls()
		{
			for (int lPortIndex = 0; lPortIndex < _numPars; lPortIndex++) {
				values_[lPortIndex].setDefault();
			}
		}

		
		bool LADSPAMachine::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			uint32_t size=0;
			pFile->Read(size); // size of whole structure
			if(size)
			{
				if(version > CURRENT_FILE_VERSION_MACD)
				{
					pFile->Skip(size);
					std::ostringstream s; s
						<< version << " > " << CURRENT_FILE_VERSION_MACD << std::endl
						<< "Data is from a newer format of psycle, it might be unsafe to load." << std::endl;
					//MessageBox(0, s.str().c_str(), "Loading Error", MB_OK | MB_ICONWARNING);
					return false;
				}
				else
				{
					uint32_t count=0;
					pFile->Read(count);  // size of vars
					for(unsigned int i(0) ; i < count ; ++i)
					{
						float temp=0;
						pFile->Read(temp);
						values_[i].setrawvalue(temp);
					}
				}
			}
			return true;
		}
		
		void LADSPAMachine::SaveSpecificChunk(RiffFile* pFile) const
		{
			uint32_t count = GetNumParams();
			uint32_t size = sizeof count  + sizeof(uint32_t) * count;
			pFile->Write(size);
			pFile->Write(count);
			for(unsigned int i(0) ; i < count ; ++i) {
				float temp = values_[i].rawvalue();
				pFile->Write(temp);
			}
		}
	}
}
