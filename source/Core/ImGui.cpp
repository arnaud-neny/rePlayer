#include "Imgui.h"

namespace ImGui
{
    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags)
    {
        auto resizeCallback = [](ImGuiInputTextCallbackData* data)
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                auto& str = *reinterpret_cast<std::string*>(data->UserData);
                str.resize(data->BufSize - 1);
                data->Buf = str.data();
            }
            return 0;
        };
        bool ret = ImGui::InputText(label, str->data(), str->size() + 1, ImGuiInputTextFlags_CallbackResize | flags, resizeCallback, str);
        if (ret)
            *str = str->c_str();
        return ret;
    }
}
//namespace ImGui
