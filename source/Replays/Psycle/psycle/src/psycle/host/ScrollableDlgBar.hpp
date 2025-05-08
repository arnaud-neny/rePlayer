///\file
///\brief interface file for psycle::host::CScrollableDlgBar.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		/// this class is a specialization of CDialogBar with one very small but extremely important modifation:  it handles WM_HSCROLL and WM_VSCROLL
		/// messages properly, to allow the normal use of sliders and scrollbars.
		class CScrollableDlgBar : public CDialogBar
		{
		public:
			CScrollableDlgBar() {}
			virtual ~CScrollableDlgBar() {}

			virtual LRESULT WindowProc(UINT nMsg, WPARAM wParam, LPARAM lParam);
		};

}}
