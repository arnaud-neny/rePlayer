///\file
///\brief implementation file for psycle::host::Preset.

#include <psycle/host/detail/project.private.hpp>
#include "Preset.hpp"
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <psycle/helpers/math.hpp>
namespace psycle { namespace host {

		CPreset::CPreset()
		:
			params(),
			data(),
			numPars(-1),
			dataSize()
		{
			std::memset(name, 0, sizeof name * sizeof *name);
		}

		CPreset::CPreset(const CPreset& preset)
		:
			params(),
			data(),
			numPars(-1),
			dataSize()
		{
			std::memset(name, 0, sizeof name * sizeof *name);
			operator=(preset);
		}
		CPreset::~CPreset()
		{
			delete[] params;
			delete[] data;
		}

		void CPreset::Clear()
		{
			delete[] params; params = 0;
			numPars =-1;
			delete[] data; data = 0;
			dataSize = 0;
			std::memset(name, 0, sizeof name * sizeof *name);
		}

		void CPreset::Init(int num)
		{
			if ( num > 0 )
			{
				delete[] params; params = new int[num];
				numPars=num;
			}
			else
			{
				delete[] params; params = 0;
				numPars =-1;
			}
			delete[] data; data = 0;
			dataSize = 0;

			std::memset(name,0, sizeof name * sizeof *name);
		}

		void CPreset::Init(int num,const char* newname, int const * parameters,int size, void* newdata)
		{
			if ( num > 0 )
			{
				delete[] params; params = new int[num];
				numPars=num;
				std::memcpy(params,parameters,numPars * sizeof *params);
			}
			else
			{
				delete[] params; params = 0;
				numPars=-1;
			}

			if ( size > 0 )
			{
				delete[] data; data = new unsigned char[size];
				std::memcpy(data,newdata,size);
				dataSize = size;
			}
			else
			{
				delete[] data; data = 0;
				dataSize=0;
			}
			std::strcpy(name,newname);
		}

		void CPreset::Init(int num,const char* newname, float const * parameters)
		{
			if ( num > 0 )
			{
				delete[] params; params = new int[num];
				numPars=num;
				for(int x=0;x<num;x++) params[x]= helpers::math::round<int,float>(parameters[x]*65535.0f);
			}
			else
			{
				delete[] params; params = 0;
				numPars=-1;
			}

			delete[] data; data = 0;
			dataSize = 0;

			std::strcpy(name,newname);
		}
		bool CPreset::operator <(const CPreset& b) const
		{
			std::string n1(name);
			std::string n2(b.name);
			std::transform(n1.begin(),n1.end(),n1.begin(), ::tolower);
			std::transform(n2.begin(),n2.end(),n2.begin(), ::tolower);
			return n1.compare(n2) < 0;
		}
		CPreset& CPreset::operator=(const CPreset& newpreset)
		{
			if ( newpreset.numPars > 0 )
			{
				numPars=newpreset.numPars;
				delete[] params; params = new int[numPars];
				std::memcpy(params,newpreset.params,numPars * sizeof *params);
			}
			else
			{
				delete[] params; params = 0;
				numPars=-1;
			}

			if ( newpreset.dataSize > 0 )
			{
				dataSize = newpreset.dataSize;
				delete[] data; data = new unsigned char[dataSize];
				std::memcpy(data,newpreset.data,dataSize);
			}
			else
			{
				delete[] data; data = 0;
				dataSize = 0;
			}

			strcpy(name,newpreset.name);
			return *this;
		}

		int CPreset::GetParam(const int n) const
		{
			if (( numPars != -1 ) && ( n < numPars )) return params[n];
			return -1;
		}

		void CPreset::SetParam(const int n,int val)
		{
			if (( numPars != -1 ) && ( n < numPars )) params[n] = val;
		}
	}   // namespace
}   // namespace
