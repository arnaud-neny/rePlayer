///\file
///\brief implementation file for psycle::host::ScrollableDlgBar.
#include <psycle/host/detail/project.private.hpp>
#include "ScrollableDlgBar.hpp"

namespace psycle { namespace host {

	LRESULT CScrollableDlgBar::WindowProc(UINT nMsg, WPARAM wParam, LPARAM lParam)
	{
		ASSERT_VALID(this);

		LRESULT lResult;
/*
		switch (nMsg)
		{
		case WM_HSCROLL:			//<dw> don't give the dialog bar a chance to handle these message itself.. because it thinks it can!
		case WM_VSCROLL:			//instead, just send it straight to the parent window, which is what it should be doing anyways
				lResult = GetOwner()->SendMessage(nMsg, wParam, lParam);
				return lResult;
		case WM_NOTIFY:
		case WM_COMMAND:
		case WM_DRAWITEM:
		case WM_MEASUREITEM:
		case WM_DELETEITEM:
		case WM_COMPAREITEM:
		case WM_VKEYTOITEM:
		case WM_CHARTOITEM:
			// send these messages to the owner if not handled
			if (OnWndMsg(nMsg, wParam, lParam, &lResult))
				return lResult;
			else
			{

				if (m_pInPlaceOwner && nMsg == WM_COMMAND)
					lResult = m_pInPlaceOwner->SendMessage(nMsg, wParam, lParam);
				else
					lResult = GetOwner()->SendMessage(nMsg, wParam, lParam);

				// special case for TTN_NEEDTEXTA and TTN_NEEDTEXTW
				if(nMsg == WM_NOTIFY)
				{
					NMHDR* pNMHDR = (NMHDR*)lParam;
					if (pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW)
					{
						TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
						TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;

						if (pNMHDR->code == TTN_NEEDTEXTA)
						{
							if (pTTTA->hinst == 0 && (!pTTTA->lpszText || !*pTTTA->lpszText))
							{
								// not handled by owner, so let bar itself handle it
								lResult = CWnd::WindowProc(nMsg, wParam, lParam);
							}
						} else if (pNMHDR->code == TTN_NEEDTEXTW)
						{
							if (pTTTW->hinst == 0 && (!pTTTW->lpszText || !*pTTTW->lpszText))
							{
								// not handled by owner, so let bar itself handle it
								lResult = CWnd::WindowProc(nMsg, wParam, lParam);
							}
						}
					}
				}
				return lResult;
			}
		}
*/
	

		// otherwise, just handle in default way
		lResult = CDialogBar::WindowProc(nMsg, wParam, lParam);
		return lResult;
	}

}   // namespace
}   // namespace
