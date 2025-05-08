///\file
///\brief interface file for psycle::host::Global.
#pragma once
#include <psycle/host/detail/project.hpp>
#include <psycle/host/Global.hpp>

namespace psycle
{
	namespace host
	{
		class FoobarConfig;

		class FoobarGlobal : public Global
		{
		public:
			FoobarGlobal();
			virtual ~FoobarGlobal() throw();

			static inline FoobarConfig  & conf(){ return *reinterpret_cast<FoobarConfig*>(pConfig); }
		};
	}
}
