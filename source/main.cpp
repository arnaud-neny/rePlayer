#include <Core.h>
#include <Core/Log.h>
#include <Core/SharedContext.h>
#include <Imgui.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_impl_win32.h>

#include <Deck/Deck.h>
#include <Graphics/Graphics.h>
#include <RePlayer/RePlayer.h>

#include <ctime>

#if _DEBUG
//#include <crtdbg.h>
#endif

#include "resource.h"

//Imgui
static HWND                 g_hWnd = NULL;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//
RePlayer* s_rePlayer = nullptr;

// imgui hack... todo: better implementation
void OnRePlayerSysCommand(bool isMinimizing)
{
    s_rePlayer->Enable(!isMinimizing);
}

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (auto ret = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return ret;

/*
    if (core::Log::IsEnabled())
        core::Log::Message("Win32: %04x %08llx %08llx\n", msg, wParam, lParam);
*/

    switch (msg)
    {
    case WM_SYSCOMMAND:
        if (wParam == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_HOTKEY:
        switch (wParam)
        {
        case APPCOMMAND_MEDIA_NEXTTRACK:
            s_rePlayer->Next();
            break;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK:
            s_rePlayer->Previous();
            break;
        case APPCOMMAND_MEDIA_STOP:
            s_rePlayer->Stop();
            break;
        case APPCOMMAND_MEDIA_PLAY_PAUSE:
            s_rePlayer->PlayPause();
            break;
        case APPCOMMAND_VOLUME_UP:
            s_rePlayer->IncreaseVolume();
            break;
        case APPCOMMAND_VOLUME_DOWN:
            s_rePlayer->DecreaseVolume();
            break;
        }
        break;
    case WM_USER:
        if (HIWORD(lParam) == 0x777)
        {
            switch (LOWORD(lParam))
            {
            case WM_MBUTTONUP:
                s_rePlayer->SystrayMouseMiddle(static_cast<int16_t>(wParam & 0xffFF), static_cast<int16_t>((wParam >> 16) & 0xffFF));
                break;
            case WM_LBUTTONUP:
                s_rePlayer->SystrayMouseLeft(static_cast<int16_t>(wParam & 0xffFF), static_cast<int16_t>((wParam >> 16) & 0xffFF));
                break;
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                s_rePlayer->SystrayMouseRight(static_cast<int16_t>(wParam & 0xffFF), static_cast<int16_t>((wParam >> 16) & 0xffFF));
                break;
            case NIN_POPUPOPEN:
                s_rePlayer->SystrayTooltipOpen(static_cast<int16_t>(wParam & 0xffFF), static_cast<int16_t>((wParam >> 16) & 0xffFF));
                break;
            case NIN_POPUPCLOSE:
                s_rePlayer->SystrayTooltipClose(static_cast<int16_t>(wParam & 0xffFF), static_cast<int16_t>((wParam >> 16) & 0xffFF));
                break;
            default:
                break;
            }
        }
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void ImGuiInit()
{
    ImGui::SetAllocatorFunctions([](size_t size, void*) { return core::Alloc(size, 0); }
        , [](void* ptr, void*) { core::Free(ptr); });
    ImGui::CreateContext();

    // Setup backend capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;       // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;        // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)
    io.BackendPlatformName = "rePlayer";
    io.IniFilename = "settings.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigViewportsNoTaskBarIcon = true;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    style.WindowPadding.x = style.WindowPadding.y = 4.0f;
    style.FramePadding.x = 2.0f;  style.FramePadding.y = 1.0f;
    style.CellPadding.x = style.CellPadding.y = 1.0f;
    style.ItemSpacing.x = 4.0f; style.ItemSpacing.y = 3.0f;
    style.WindowBorderSize = 0.f;
    style.FrameBorderSize = 0.f;
    style.PopupBorderSize = 0.f;
    style.PopupRounding = 0.f;

    ImGui_ImplWin32_Init(g_hWnd);
}

void ImGuiRelease()
{
    ImGui_ImplWin32_Shutdown();

    ImGui::DestroyContext();
}

static _CRT_ALLOC_HOOK oldHook;
static int MyHook(int allocType, void* userData, size_t size,
    int blockType, long requestNumber,
    const unsigned char* filename, int lineNumber)
{
    if (size == 44672)
        printf("toto");
    return oldHook(allocType, userData, size, blockType, requestNumber, filename, lineNumber);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int /*nCmdShow*/)
{
#if _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(146143);
    //oldHook =_CrtGetAllocHook();
    //_CrtSetAllocHook(MyHook);
#endif

    ::OleInitialize(NULL);

    if (s_rePlayer = RePlayer::Create())
    {
        // Create application window
        WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, hInstance, LoadIcon(hInstance, MAKEINTRESOURCEA(IDI_MAINICON)), nullptr, nullptr, nullptr, "rePlayer", nullptr };
        ::RegisterClassEx(&wc);
        g_hWnd = ::CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW, wc.lpszClassName, "rePlayer", WS_OVERLAPPED, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);

        ImGuiInit();

        core::SharedContexts::Register();

        //
        if (rePlayer::Graphics::Init(g_hWnd))
            return -1;

#if 0
        // enumerate the monitors
        struct Test
        {
            static BOOL proc(HMONITOR hMonitor, HDC hDC, LPRECT rect, LPARAM lParam)
            {
                if (rect && rect->left > 0)
                {
                    MONITORINFO info = { sizeof(MONITORINFO) };
                    GetMonitorInfo(hMonitor, &info);
                    *reinterpret_cast<LPRECT>(lParam) = info.rcWork;
                }
                return TRUE;
            }

        };

        // maximize on the second monitor
        RECT rc;
        if (::EnumDisplayMonitors(NULL, nullptr, Test::proc, reinterpret_cast<LPARAM>(&rc)))
        {
            ::SetWindowLong(g_hWnd, GWL_STYLE, WS_OVERLAPPED);
            ::SetWindowPos(g_hWnd, HWND_TOP,//HWND_TOPMOST,
                rc.left,
                rc.top,
                rc.right - rc.left,
                rc.bottom - rc.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
#endif

        // Taskbar Icon begin
        {
            NOTIFYICONDATA tnid = {};

            tnid.cbSize = sizeof(NOTIFYICONDATA);
            tnid.hWnd = g_hWnd;
            tnid.uID = 0x777;
            tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            tnid.uCallbackMessage = WM_USER;
            tnid.hIcon = wc.hIcon;
            tnid.szTip[0] = ' ';
            tnid.szTip[1] = 0;

            Shell_NotifyIcon(NIM_ADD, &tnid);
            tnid.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIcon(NIM_SETVERSION, &tnid);
        }

        // Show the window
        auto mainWindowExStyle = ::GetWindowLong(g_hWnd, GWL_EXSTYLE);
        ::SetWindowLong(g_hWnd, GWL_EXSTYLE, mainWindowExStyle | WS_EX_TRANSPARENT | WS_EX_LAYERED);
        ::ShowWindow(g_hWnd, SW_SHOWDEFAULT);
        ::UpdateWindow(g_hWnd);

/*
        LONG cur_style = GetWindowLong(g_hWnd, GWL_EXSTYLE);
        SetWindowLong(g_hWnd, GWL_EXSTYLE, cur_style | WS_EX_TRANSPARENT | WS_EX_LAYERED);//let the hwnd messaged passthrough our app as POC
        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);//and let the hwnd always on top to be displayed :)
*/

        std::srand(static_cast<uint32_t>(std::time(0) & 0xffFFffFF));

        if (s_rePlayer->Launch() == core::Status::kOk)
        {
            // Main loop
            MSG msg = {};
            while (msg.message != WM_QUIT)
            {
                // Poll and handle messages (inputs, window resize, etc.)
                // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
                // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
                // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
                // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
                if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                    continue;
                }

                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                if (s_rePlayer->UpdateFrame() != core::Status::kOk)
                    break;

#if _DEBUG
                static bool show_demo_window = false;
                if (ImGui::IsKeyPressed(ImGuiKey_F11))
                    show_demo_window = !show_demo_window;
                if (show_demo_window)
                    ImGui::ShowDemoWindow(&show_demo_window);
#endif

/*
                if (!ImGui::GetCurrentContext()->HoveredWindow)
                {
                    ::SetWindowLong(g_hWnd, GWL_EXSTYLE, mainWindowExStyle | WS_EX_TRANSPARENT | WS_EX_LAYERED);
                }
                else
                {
                    ::SetWindowLong(g_hWnd, GWL_EXSTYLE, mainWindowExStyle);
                }
*/

                ImGui::Render();

                rePlayer::Graphics::BeginFrame();

                if (rePlayer::Graphics::EndFrame(s_rePlayer->GetBlendingFactor()))
                    break;
            }

            // Taskbar Icon end
            {
                NOTIFYICONDATA tnid = {};

                tnid.cbSize = sizeof(NOTIFYICONDATA);
                tnid.hWnd = g_hWnd;
                tnid.uID = 0x777;

                Shell_NotifyIcon(NIM_DELETE, &tnid);
            }

            ImGuiRelease();

            s_rePlayer->Release();
        }

        rePlayer::Graphics::Destroy();

        core::SharedContexts::Unregister();
    }

    ::OleUninitialize();

    return 0;
}
