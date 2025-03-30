#include "Window.inl.h"

#include <ImGui.h>
#include <ImGui/imgui_internal.h>

#include <windows.h>

namespace core
{
    void Window::Handle::Apply(States other)
    {
        if (hWnd == nullptr)
        {
            if (hWnd = ImGui::GetWindowViewport()->PlatformHandle)
            {
                states.isAlwaysOnTop = other.isAlwaysOnTop;
                states.isPassthrough = other.isPassthrough;
                ApplyPassthrough();
                ApplyAlwaysOnTop();
            }
        }
        else
        {
            if (states.isPassthrough != other.isPassthrough)
            {
                states.isPassthrough = other.isPassthrough;
                ApplyPassthrough();
            }
            if (states.isAlwaysOnTop != other.isAlwaysOnTop)
            {
                states.isAlwaysOnTop = other.isAlwaysOnTop;
                ApplyAlwaysOnTop();
            }
        }
    }

    void Window::Handle::ApplyAlwaysOnTop() const
    {
        ::SetWindowPos(HWND(hWnd), states.isAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | (states.isAlwaysOnTop ? SWP_SHOWWINDOW : 0));
    }

    void Window::Handle::ApplyPassthrough() const
    {
        auto hWndStyle = ::GetWindowLongW(HWND(hWnd), GWL_EXSTYLE);
        if (Window::ms_isPassthroughAvailable == Passthrough::IsAvailable && states.isPassthrough)
            hWndStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
        else
            hWndStyle &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);
        ::SetWindowLongW(HWND(hWnd), GWL_EXSTYLE, hWndStyle);
    }

    Window::Passthrough Window::ms_isPassthroughAvailable;
    Window* Window::ms_windows = nullptr;

    Window::Window(std::string&& name, ImGuiWindowFlags flags, bool isMaster)
        : m_nextWindow(ms_windows)
        , m_name(std::move(name))
        , m_flags(flags)
        , m_isMaster(isMaster)
    {
        // register only once
        if (m_nextWindow == nullptr)
        {
            ImGuiSettingsHandler iniHandler = {};
            iniHandler.TypeName = "rePlayer";
            iniHandler.TypeHash = ImHashStr("rePlayer");
            iniHandler.ReadOpenFn = ImGuiIniReadOpen;
            iniHandler.ReadLineFn = ImGuiIniReadLine;
            iniHandler.ApplyAllFn = ImGuiApplyAll;
            iniHandler.WriteAllFn = ImGuiIniWriteAll;
            iniHandler.UserData = &ms_windows;
            ImGui::GetCurrentContext()->SettingsHandlers.push_back(iniHandler);
        }
        ms_windows = this;
        if (isMaster)
        {
            RegisterSerializedData(m_handle.states.isMinimal, "IsMinimal");
            RegisterSerializedData(m_handle.states.isAlwaysOnTop, "IsAlwaysOnTop");
            RegisterSerializedData(m_handle.states.isPassthrough, "IsPassthrough");
        }
        RegisterSerializedData(m_handle.states.isEnabled, "IsEnabled");
    }

    void Window::Update(States states)
    {
        for (auto window = ms_windows; window; window = window->m_nextWindow)
            window->OnBeginFrame();
        for (auto window = ms_windows; window; window = window->m_nextWindow)
            window->UpdateInternal(states);
        for (auto window = ms_windows; window; window = window->m_nextWindow)
            window->OnEndFrame();
    }

    void Window::UpdateInternal(States states)
    {
        OnBeginUpdate();

        m_handle.states.isMinimal = states.isMinimal;
        if (m_isMaster || (m_handle.states.isEnabled && !states.isMinimal && states.isEnabled))
        {
            ImGui::BeginDisabled(m_handle.states.isPassthrough);
            ImGui::Begin(OnGetWindowTitle().c_str(), &m_handle.states.isEnabled, m_flags);

            m_handle.Apply(states);
            if (m_isMaster)
            {
                WINDOWPLACEMENT windowPlacement = { sizeof(WINDOWPLACEMENT) };
                ::GetWindowPlacement(HWND(m_handle.hWnd), &windowPlacement);
                if (windowPlacement.showCmd == SW_SHOWMINIMIZED && m_handle.states.isEnabled)
                {
                    ::ShowWindow(HWND(m_handle.hWnd), SW_NORMAL);
                    ::SetForegroundWindow(HWND(m_handle.hWnd));
                }
                else if (windowPlacement.showCmd != SW_SHOWMINIMIZED && !m_handle.states.isEnabled)
                {
                    ::ShowWindow(HWND(m_handle.hWnd), SW_MINIMIZE);
                }
            }

            OnDisplay();

            ImGui::End();
            ImGui::EndDisabled();
        }
        else
            m_handle.hWnd = nullptr;

        OnEndUpdate();
    }

    bool Window::LoadSettings(const char* line)
    {
        bool isLoaded = false;
        for (auto& data : m_serializedData)
        {
            std::string name = data.name;
            switch (data.type)
            {
            case SerializedData::kFloat:
                if (sscanf_s(line, (name + "=%f").c_str(), data.addr) == 1)
                    isLoaded = true;
                break;
            case SerializedData::kInt8_t:
                if (sscanf_s(line, (name + "=%hhd").c_str(), data.addr) == 1)
                    isLoaded = true;
                break;
            case SerializedData::kUint8_t:
                if (sscanf_s(line, (name + "=%hhu").c_str(), data.addr) == 1)
                    isLoaded = true;
                break;
            case SerializedData::kInt16_t:
                if (sscanf_s(line, (name + "=%hd").c_str(), data.addr) == 1)
                    isLoaded = true;
                break;
            case SerializedData::kUint16_t:
                if (sscanf_s(line, (name + "=%hu").c_str(), data.addr) == 1)
                    isLoaded = true;
                break;
            case SerializedData::kInt32_t:
                if (sscanf_s(line, (name + "=%d").c_str(), data.addr) == 1)
                    isLoaded = true;
                break;
            case SerializedData::kUint32_t:
                if (sscanf_s(line, (name + "=%u").c_str(), data.addr) == 1)
                    isLoaded = true;
                break;
            case SerializedData::kBool:
            {
                int32_t intToBool;
                if (sscanf_s(line, (name + "=%d").c_str(), &intToBool) == 1)
                {
                    *reinterpret_cast<bool*>(data.addr) = intToBool != 0;
                    isLoaded = true;
                }
                break;
            }
            case SerializedData::kString:
                if (strstr(line, (name + "=").c_str()))
                {
                    *reinterpret_cast<std::string*>(data.addr) = line + name.length() + 1;
                    isLoaded = true;
                }
                break;
            case SerializedData::kChars:
                if (strstr(line, (name + "=").c_str()))
                {
                    strcpy_s(reinterpret_cast<char*>(data.addr), data.size, line + name.length() + 1);
                    isLoaded = true;
                }
                break;
            case SerializedData::kCustom:
                isLoaded = data.onLoadCallback(data.addr, line);
                break;
            }
            if (isLoaded)
            {
                if (data.type != SerializedData::kCustom && data.onLoadedCallback)
                {
                    if (data.isStaticCallback)
                        data.onLoadedStaticCallback(data.userData, data.addr, data.name);
                    else
                        (this->*data.onLoadedCallback)(data.addr, data.name);
                }
                break;
            }
        }
        return isLoaded;
    }

    void Window::SaveSettings(ImGuiTextBuffer* buf)
    {
        for (auto& data : m_serializedData)
        {
            std::string name = data.name;
            switch (data.type)
            {
            case SerializedData::kFloat:
                buf->appendf((name + "=%f\n").c_str(), *reinterpret_cast<float*>(data.addr));
                break;
            case SerializedData::kInt8_t:
                buf->appendf((name + "=%d\n").c_str(), *reinterpret_cast<int8_t*>(data.addr));
                break;
            case SerializedData::kUint8_t:
                buf->appendf((name + "=%u\n").c_str(), *reinterpret_cast<uint8_t*>(data.addr));
                break;
            case SerializedData::kInt16_t:
                buf->appendf((name + "=%d\n").c_str(), *reinterpret_cast<int16_t*>(data.addr));
                break;
            case SerializedData::kUint16_t:
                buf->appendf((name + "=%u\n").c_str(), *reinterpret_cast<uint16_t*>(data.addr));
                break;
            case SerializedData::kInt32_t:
                buf->appendf((name + "=%d\n").c_str(), *reinterpret_cast<int32_t*>(data.addr));
                break;
            case SerializedData::kUint32_t:
                buf->appendf((name + "=%u\n").c_str(), *reinterpret_cast<uint32_t*>(data.addr));
                break;
            case SerializedData::kBool:
                buf->appendf((name + "=%d\n").c_str(), *reinterpret_cast<bool*>(data.addr) ? 1 : 0);
                break;
            case SerializedData::kString:
                buf->appendf((name + "=%s\n").c_str(), reinterpret_cast<std::string*>(data.addr)->c_str());
                break;
            case SerializedData::kChars:
                buf->appendf((name + "=%s\n").c_str(), reinterpret_cast<const char*>(data.addr));
                break;
            case SerializedData::kCustom:
                data.onSaveCallback(data.addr, buf);
                break;
            }
        }
    }

    void* Window::ImGuiIniReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
    {
        for (auto window = ms_windows; window; window = window->m_nextWindow)
        {
            if (window->m_name == name)
                return window;
        }
        return nullptr;
    }

    void Window::ImGuiIniReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* window, const char* line)
    {
        reinterpret_cast<Window*>(window)->LoadSettings(line);
    }

    void Window::ImGuiApplyAll(ImGuiContext*, ImGuiSettingsHandler*)
    {
        for (auto window = ms_windows; window; window = window->m_nextWindow)
            window->OnApplySettings();
    }

    void Window::ImGuiIniWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
    {
        for (auto window = ms_windows; window; window = window->m_nextWindow)
        {
            buf->appendf("[%s][%s]\n", handler->TypeName, window->m_name.c_str());
            window->SaveSettings(buf);
            buf->append("\n");
        }
    }
}
//namespace core