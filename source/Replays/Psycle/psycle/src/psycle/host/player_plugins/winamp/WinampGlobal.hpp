///\file
///\brief interface file for psycle::host::Global.
#pragma once
#include <psycle/host/detail/project.hpp>
#include <psycle/host/Global.hpp>

namespace psycle
{
	namespace host
	{
		class WinampConfig;

		class WinampGlobal : public Global
		{
		public:
			WinampGlobal();
			virtual ~WinampGlobal() throw();

			static inline WinampConfig  & conf(){ return *reinterpret_cast<WinampConfig*>(pConfig); }
		};
	}
}
