#include "Graphics.h"

#ifdef _WIN64
#include "GraphicsDx12.h"
#endif
#include "GraphicsDx11.h"

namespace rePlayer
{
	Graphics* Graphics::ms_instance = nullptr;

    bool Graphics::Init(void* hWnd)
    {
#ifdef _WIN64
        ms_instance = new GraphicsDX12(HWND(hWnd));
        if (ms_instance->OnInit())
            delete ms_instance;
        else
            return false;
#endif
        ms_instance = new GraphicsDX11(HWND(hWnd));
        if (ms_instance->OnInit())
        {
            delete ms_instance;
            return true;
        }
        return false;
    }
}
// namespace rePlayer