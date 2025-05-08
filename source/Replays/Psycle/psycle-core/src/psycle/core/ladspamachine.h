// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__LADSPA_MACHINE__INCLUDED
#define PSYCLE__CORE__LADSPA_MACHINE__INCLUDED
#pragma once

#include "machine.h"
#include "ladspa.h"

namespace psycle { namespace core {

		class PSYCLE__CORE__DECL LadspaParam {
			public:
				LadspaParam(LADSPA_PortDescriptor descriptor,LADSPA_PortRangeHint hint, const char *newname);
				LADSPA_PortDescriptor descriptor() { return descriptor_; }
				LADSPA_PortRangeHintDescriptor hint() const { return hint_.HintDescriptor; }
				const char* name() const { return portName_; }
				LADSPA_Data rawvalue() const { return value_; }
				LADSPA_Data* valueaddress() { return &value_; }
				int value() const;
				void setValue(int data);
				void setrawvalue(LADSPA_Data data) { value_ = data; }
				void setDefault();
				LADSPA_Data minval() const { return integer_ ? (minVal_*rangeMultiplier_) : 0; }
				LADSPA_Data maxval() const { return integer_ ? (maxVal_-minVal_) : 65535; }
			private:
				LADSPA_PortDescriptor descriptor_;
				LADSPA_PortRangeHint hint_;
				const char * portName_;
				LADSPA_Data value_;
				LADSPA_Data minVal_;
				LADSPA_Data maxVal_;
				float rangeMultiplier_;
				bool integer_;
				bool logarithmic_;
		};
	
		class PSYCLE__CORE__DECL LADSPAMachine: public Machine {
			protected:
				LADSPAMachine(MachineCallbacks*, MachineKey, Machine::id_type, void*,
						const LADSPA_Descriptor* ,LADSPA_Handle); friend class LadspaHost;
			public:
				virtual ~LADSPAMachine() throw();
				virtual void Init();
				virtual int GenerateAudio(int numSamples );
				virtual void Tick(int channel, const PatternEvent & pEntry );
				virtual void Stop(){}
				inline virtual std::string GetDllName() const throw() { return key_.dllName(); }
				virtual const MachineKey& getMachineKey() const {  return key_; }
				virtual std::string GetName() const { return (char *) psDescriptor ? psDescriptor->Name : ""; }
				virtual void GetParamName(int numparam, char * name) const;
				virtual void GetParamRange(int numparam,int &minval, int &maxval) const;
				virtual int GetParamValue(int numparam) const;
				virtual void GetParamValue(int numparam,char* parval) const;
				virtual bool SetParameter(int numparam,int value);
				virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
				virtual void SaveSpecificChunk(RiffFile * pFile) const;

			private:
				void prepareStructures(void);
				LADSPA_Data GetMinValue(int lPortIndex, LADSPA_PortRangeHintDescriptor iHintDescriptor);
				LADSPA_Data GetMaxValue(int lPortIndex, LADSPA_PortRangeHintDescriptor iHintDescriptor);
				void SetDefaultsForControls();
				MachineKey key_;
				void* libHandle_;
				const LADSPA_Descriptor * psDescriptor;
				/*const*/ LADSPA_Handle pluginHandle;
				std::vector<LadspaParam> values_;
				std::vector<LadspaParam> controls_;
				float* pOutSamplesL;
				float* pOutSamplesR;
		};

}}
#endif
