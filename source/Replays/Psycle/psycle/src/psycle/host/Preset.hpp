///\file
///\brief interface file for psycle::host::Preset.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
namespace psycle { namespace host {

		class CPreset  
		{
		public:
			CPreset();
			CPreset(const CPreset& newpreset);
			virtual ~CPreset();
			bool operator <(const CPreset& b) const;
			CPreset& operator=(const CPreset& newpreset);

			void Init(int num);
			void Init(int num,const char* newname,   int const * parameters,int size, void* newdata);
			void Init(int num,const char* newname, float const * parameters); // for VST .fxb's
			void Clear();
			int GetNumPars() const { return numPars; }
			void GetParsArray(int* destarray) const { if(numPars>0) std::memcpy(destarray, params, numPars * sizeof *params); }
			void GetDataArray(void* destarray) const {if(dataSize>0) std::memcpy(destarray, data, dataSize); }
			void* GetData() const {return data;}
			int32_t GetDataSize() const {return dataSize;}
			void SetName(const char *setname) { std::strcpy(name,setname); }
			void GetName(char *nname) const { std::strcpy(nname,name); }
			int GetParam(const int n) const;
			void SetParam(const int n,int val);
		private:
			int numPars;
			int* params;
			int32_t dataSize;
			unsigned char * data;
			char name[32];
		};

	}   // namespace
}   // namespace
