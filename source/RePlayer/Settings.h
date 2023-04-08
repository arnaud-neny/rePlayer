#pragma once

#include <Core/Window.h>

namespace rePlayer
{
    class Settings : public core::Window
    {
    public:
        Settings();
        ~Settings();

        std::string OnGetWindowTitle() override;
        void OnDisplay() override;
    };
}
// namespace rePlayer