/*****************************************************************************/
/* EffectWnd.cpp : implementation file                                       */
/*****************************************************************************/

/*****************************************************************************/
/* Work Derived from the LGPL host "vsthost (1.16m)".                        */
/* (http://www.hermannseib.com/english/vsthost.htm)"                         */
/* vsthost has the following lincense:                                       *

Copyright (C) 2006  Hermann Seib

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#include <psycle/core/detail/project.private.hpp>
#include "EffectWnd.hpp"

namespace seib { namespace vst {

VkeysT CEffectWnd::VKeys[] = {
	{ VK_BACK,      VKEY_BACK      },
	{ VK_TAB,       VKEY_TAB       },
	{ VK_CLEAR,     VKEY_CLEAR     },
	{ VK_RETURN,    VKEY_RETURN    },
	{ VK_PAUSE,     VKEY_PAUSE     },
	{ VK_ESCAPE,    VKEY_ESCAPE    },
	{ VK_SPACE,     VKEY_SPACE     },
	#if 0
	{ VK_NEXT,      VKEY_NEXT      },
	#endif
	{ VK_END,       VKEY_END       },
	{ VK_HOME,      VKEY_HOME      },
	{ VK_LEFT,      VKEY_LEFT      },
	{ VK_UP,        VKEY_UP        },
	{ VK_RIGHT,     VKEY_RIGHT     },
	{ VK_DOWN,      VKEY_DOWN      },
	{ VK_PRIOR,     VKEY_PAGEUP    },
	{ VK_NEXT,      VKEY_PAGEDOWN  },
	{ VK_SELECT,    VKEY_SELECT    },
	{ VK_PRINT,     VKEY_PRINT     },
	{ VK_EXECUTE,   VKEY_ENTER     },
	{ VK_SNAPSHOT,  VKEY_SNAPSHOT  },
	{ VK_INSERT,    VKEY_INSERT    },
	{ VK_DELETE,    VKEY_DELETE    },
	{ VK_HELP,      VKEY_HELP      },
	{ VK_NUMPAD0,   VKEY_NUMPAD0   },
	{ VK_NUMPAD1,   VKEY_NUMPAD1   },
	{ VK_NUMPAD2,   VKEY_NUMPAD2   },
	{ VK_NUMPAD3,   VKEY_NUMPAD3   },
	{ VK_NUMPAD4,   VKEY_NUMPAD4   },
	{ VK_NUMPAD5,   VKEY_NUMPAD5   },
	{ VK_NUMPAD6,   VKEY_NUMPAD6   },
	{ VK_NUMPAD7,   VKEY_NUMPAD7   },
	{ VK_NUMPAD8,   VKEY_NUMPAD8   },
	{ VK_NUMPAD9,   VKEY_NUMPAD9   },
	{ VK_MULTIPLY,  VKEY_MULTIPLY  },
	{ VK_ADD,       VKEY_ADD,      },
	{ VK_SEPARATOR, VKEY_SEPARATOR },
	{ VK_SUBTRACT,  VKEY_SUBTRACT  },
	{ VK_DECIMAL,   VKEY_DECIMAL   },
	{ VK_DIVIDE,    VKEY_DIVIDE    },
	{ VK_F1,        VKEY_F1        },
	{ VK_F2,        VKEY_F2        },
	{ VK_F3,        VKEY_F3        },
	{ VK_F4,        VKEY_F4        },
	{ VK_F5,        VKEY_F5        },
	{ VK_F6,        VKEY_F6        },
	{ VK_F7,        VKEY_F7        },
	{ VK_F8,        VKEY_F8        },
	{ VK_F9,        VKEY_F9        },
	{ VK_F10,       VKEY_F10       },
	{ VK_F11,       VKEY_F11       },
	{ VK_F12,       VKEY_F12       },
	{ VK_NUMLOCK,   VKEY_NUMLOCK   },
	{ VK_SCROLL,    VKEY_SCROLL    },
	{ VK_SHIFT,     VKEY_SHIFT     },
	{ VK_CONTROL,   VKEY_CONTROL   },
	{ VK_MENU,      VKEY_ALT       },
	#if 0
	{ VK_EQUALS,    VKEY_EQUALS    },
	#endif
};


/*****************************************************************************/
/* GetWindowSize : calculates the effect window's size                       */
/*****************************************************************************/

bool CEffectGui::GetViewSize(ERect &rcClient, ERect *pRect) {
	if(!pRect) {
		//The return of this method is not reliable (example: oatmeal). That's why i just check the rect.
		pEffect->EditGetRect(&pRect);
		if(!pRect) return false;
	}

	rcClient.left = rcClient.top = 0;
	rcClient.right = pRect->right - pRect->left;
	rcClient.bottom = pRect->bottom - pRect->top;
	return true;
}

/*===========================================================================*/
/* CEffectWnd class members                                                  */
/*===========================================================================*/

CEffectWnd::CEffectWnd(CEffect* effect)
: pEffect(effect)
{
}
CEffectWnd::~CEffectWnd()
{
	///?
}

/*****************************************************************************/
/* MakeVstKeyCode : converts from Windows to VST                             */
/*****************************************************************************/

void CEffectWnd::ConvertToVstKeyCode(UINT nChar, UINT nRepCnt, UINT nFlags, VstKeyCode &keyCode) {
	if (nChar >= 'A' && nChar <= 'Z')
		keyCode.character = nChar + ('a' - 'A');
	else
		keyCode.character = nChar;
	keyCode.virt = 0;
	for (int i = 0; i < sizeof(VKeys)/sizeof(*VKeys); ++i) {
		if (nChar == VKeys[i].vkWin) {
			keyCode.virt = VKeys[i].vstVirt;
			break;
		}
		keyCode.modifier = 0;
		if (GetKeyState(VK_SHIFT) & 0x8000)
			keyCode.modifier |= MODIFIER_SHIFT;
		if (GetKeyState(VK_CONTROL) & 0x8000)
			keyCode.modifier |= MODIFIER_CONTROL;
		if (GetKeyState(VK_MENU) & 0x8000)
			keyCode.modifier |= MODIFIER_ALTERNATE;
	}
}

/*****************************************************************************/
/* SaveBank saves bank to file                                               */
/*****************************************************************************/

bool CEffectWnd::SaveBank(std::string const & sName) {
	pEffect->SetChunkFile(sName.c_str());
	CFxBank b = pEffect->SaveBank();
	if(b.Initialized())
		return b.Save(sName.c_str());
	else
		return false;
}

bool CEffectWnd::SaveProgram(std::string const & sName) {
	pEffect->SetChunkFile(sName.c_str());
	CFxProgram p = pEffect->SaveProgram();
	if(p.Initialized())
		return p.Save(sName.c_str());
	else
		return false;
}

}}
