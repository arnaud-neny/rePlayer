///\file
///\brief interface file for psycle::host::Global.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"

class CDPI;

namespace psycle
{
	namespace host
	{
		class CMidiInput;
		class InputHandler;
		class PsycleConfig;
		class ActionHandler;

		class PsycleGlobal : public Global
		{
		public:
			PsycleGlobal();
			virtual ~PsycleGlobal() throw();

			static inline PsycleConfig & conf()   { return *reinterpret_cast<PsycleConfig*>(pConfig); }
			static inline CMidiInput   & midi()   { return *pMidiInput; }
			static inline InputHandler & inputHandler() { return *pInputHandler; }
			static inline CDPI		   & dpiSetting()   { return dpi; }
			static inline ActionHandler & actionHandler() { return *pActions; }

		protected:
			static CMidiInput * pMidiInput;
			static InputHandler* pInputHandler;
			static ActionHandler* pActions;
			static CDPI dpi;
		};
	}
}
