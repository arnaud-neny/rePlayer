#include "Settings.h"

#include <Deck/Deck.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

#include <Imgui.h>

namespace rePlayer
{
    Settings::Settings()
        : Window("Settings", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)
    {}

    Settings::~Settings()
    {}

    std::string Settings::OnGetWindowTitle()
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        return "Settings";
    }

    void Settings::OnDisplay()
    {
        Core::GetDeck().DisplaySettings();
        if (Core::GetReplays().DisplaySettings())
            Core::GetDeck().ChangedSettings();
    }
}
// namespace rePlayer