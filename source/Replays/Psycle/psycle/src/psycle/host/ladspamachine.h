// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include <psycle/host/detail/project.hpp>
#include "Machine.hpp"
#include "ladspa.h"
#include <map>

namespace psycle { namespace host {	

		class LadspaParam {
			public:
				LadspaParam() {};
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
	
		class LADSPAMachine: public Machine {
			protected:
				LADSPAMachine(//MachineCallbacks*, MachineKey, Machine::id_type, void*,
						const std::string& dllname, int index, int, void*, const LADSPA_Descriptor* ,LADSPA_Handle); friend class LadspaHost;
			public:
				virtual ~LADSPAMachine() throw();
				virtual void Init();
				virtual int GenerateAudioInTicks( int startSample, int numSamples );				
				virtual void Tick(int track, PatternEntry * pData);
				virtual void Stop(){}
				inline virtual const char * const GetDllName() const throw() { return dllname_.c_str(); }
				virtual const char * const GetName() const { return (char *) psDescriptor ? psDescriptor->Name : ""; }
				virtual void GetParamName(int numparam, char * name);
				virtual void GetParamRange(int numparam,int &minval, int &maxval);
				virtual int GetParamValue(int numparam);
				virtual void GetParamValue(int numparam,char* parval);
				virtual bool SetParameter(int numparam,int value);
				virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
				virtual void SaveSpecificChunk(RiffFile * pFile);
				virtual int GetParamType(int numparam) { return 2; }

				virtual std::string GetInputPinName(int pin) const {
					std::map<int,int>::const_iterator iter = inportmap.find(pin);
					const char* desc = "Unknown";
					if (iter != inportmap.end() ) {
						desc = psDescriptor->PortNames[iter->second];
					}
					return desc;
				}
				virtual int GetNumInputPins() const { return numInputs; }
				virtual std::string GetOutputPinName(int pin) const {
					std::map<int,int>::const_iterator iter = outportmap.find(pin);
					const char* desc = "Unknown";
					if (iter != outportmap.end() ) {
						desc = psDescriptor->PortNames[iter->second];
					}
					return desc;
				}
				virtual int GetNumOutputPins() const { return numOutputs; }

				void SetDefaultsForControls();
			private:
				void prepareStructures(void);
				LADSPA_Data GetMinValue(int lPortIndex, LADSPA_PortRangeHintDescriptor iHintDescriptor);
				LADSPA_Data GetMaxValue(int lPortIndex, LADSPA_PortRangeHintDescriptor iHintDescriptor);
				int key_;
				std::string dllname_;
				void* libHandle_;
				const LADSPA_Descriptor * psDescriptor;
				/*const*/ LADSPA_Handle pluginHandle;
				std::vector<LadspaParam> values_;
				//psycle index, ladspa index
				std::map<int,int> inportmap;
				std::map<int,int> outportmap;
				int numInputs;
				int numOutputs;
				float* pOutSamplesL;
				float* pOutSamplesR;
		};

}}

