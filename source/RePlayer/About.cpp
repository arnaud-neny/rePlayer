#include "About.h"

#include <Database/Types/SourceID.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <RePlayer/Version.h>

#include <Core/String.h>
#include <Imgui.h>

// curl
#include <curl/curl.h>

// libarchive
#include <libarchive/archive.h>

// libxml
#include <libxml/xmlversion.h>

// TagLib
#include <toolkit/taglib.h>

namespace rePlayer
{
    About::About()
        : Window("About", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize)
    {}

    About::~About()
    {}

    std::string About::OnGetWindowTitle()
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(512.0f, 256.0f), ImGuiCond_Always);

        return "About";
    }

    void About::OnDisplay()
    {
        ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);

        ImGui::TextUnformatted(Core::GetLabel());
        ImGui::TextUnformatted(REPLAYER_COPYRIGHT " (aka replay/Razor1911)");

        ImGui::TextUnformatted("\nSupported databases:");
        for (uint32_t i = 0; i < SourceID::NumSourceIDs; i++)
        {
            if (i != SourceID::FileImportID && i != SourceID::URLImportID)
            {
                ImGui::Bullet();
                ImGui::TextUnformatted(SourceID::sourceNames[i]);
            }
        }

        ImGui::TextUnformatted("\nSystem 3rd parties:");
        ImGui::Bullet();
        ImGui::TextUnformatted("Dear ImGui " IMGUI_VERSION "\n"
            "Copyright (c) 2014-2025 Omar Cornut");
        ImGui::Bullet();
        ImGui::TextUnformatted("stb\n"
            "Copyright (c) 2017 Sean Barrett");
        ImGui::Bullet();
        ImGui::TextUnformatted("Curl " LIBCURL_VERSION "\n"
            "Copyright (c) " LIBCURL_COPYRIGHT);
        ImGui::Bullet();
        ImGui::Text("libxml2 " LIBXML_DOTTED_VERSION "\n"
            "Copyright (c) 1998-2012 Daniel Veillard");
        ImGui::Bullet();
        ImGui::TextUnformatted("libarchive " ARCHIVE_VERSION_ONLY_STRING);
        ImGui::Bullet();
        ImGui::TextUnformatted("TagLib " TOSTRING(TAGLIB_MAJOR_VERSION) "." TOSTRING(TAGLIB_MINOR_VERSION) "." TOSTRING(TAGLIB_PATCH_VERSION));
        ImGui::Bullet();
        ImGui::TextUnformatted("dllloader\n"
            "Copyright (c) 2012-2022 Scott Chacon and others");

        ImGui::TextUnformatted("\nReplays 3rd parties:");
        Core::GetReplays().DisplayAbout();

        ImGui::EndChild();
    }

}
// namespace rePlayer