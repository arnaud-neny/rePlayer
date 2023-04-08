#pragma once

#include <Core/Window.h>

namespace rePlayer
{
    class About : public core::Window
    {
    public:
        About();
        ~About();

        std::string OnGetWindowTitle() override;
        void OnDisplay() override;
    };
}
// namespace rePlayer