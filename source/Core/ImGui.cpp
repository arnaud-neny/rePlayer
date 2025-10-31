#include "Imgui.h"

namespace ImGui
{
    static auto s_resizeCallback = [](ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            auto& str = *reinterpret_cast<std::string*>(data->UserData);
            str.resize(data->BufTextLen);
            data->Buf = str.data();
        }
        return 0;
    };

    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags)
    {
        return InputText(label, str->data(), str->size() + 1, ImGuiInputTextFlags_CallbackResize | flags, s_resizeCallback, str);
    }

    bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags)
    {
        return InputTextMultiline(label, str->data(), str->size() + 1, size, ImGuiInputTextFlags_CallbackResize | flags, s_resizeCallback, str);
    }
}
// namespace ImGui