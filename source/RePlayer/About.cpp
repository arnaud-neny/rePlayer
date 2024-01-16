#include "About.h"

#include <Database/Types/SourceID.h>
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

#include <Core/String.h>
#include <Imgui.h>
#include <ImGui/ImGuiFileDialog.h>
#include <Xml/tinyxml2.h>

// curl
#include <curl/curl.h>

// libarchive
#include <libarchive/archive.h>

// TagLib
#include <toolkit/taglib.h>

// tidy
#include <Tidy/tidy.h>

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
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"Copyright (c) 2021-2023 Arnaud Nény (aka replay/Razor1911)"));

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
            "Copyright (c) 2014-2023 Omar Cornut");
        ImGui::Bullet();
        ImGui::TextUnformatted("stb\n"
            "Copyright (c) 2017 Sean Barrett");
        ImGui::Bullet();
        ImGui::TextUnformatted("ImGuiFileDialog " IMGUIFILEDIALOG_VERSION "\n"
            "Copyright (c) 2018-2022 Stephane Cuillerdier (aka Aiekick)");
        ImGui::Bullet();
        ImGui::TextUnformatted("Curl " LIBCURL_VERSION "\n"
            "Copyright (c) " LIBCURL_COPYRIGHT);
        ImGui::Bullet();
        ImGui::Text("Tidy Html %s\n"
            "Copyright (c) 1998-2016 World Wide Web Consortium & Dave Raggett", tidyLibraryVersion());
        ImGui::Bullet();
        ImGui::Text("TinyXML-2 %u.%u.%u\n"
            "Copyright (c) Lee Thomason", TINYXML2_MAJOR_VERSION, TINYXML2_MINOR_VERSION, TINYXML2_PATCH_VERSION);
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