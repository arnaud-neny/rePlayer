#include "About.h"

#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>

#include <Imgui.h>
#include <ImGui/ImGuiFileDialog.h>
#include <Xml/tinyxml2.h>

// curl
#include <curl/curl.h>

// libarchive
#include <libarchive/archive.h>

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
        ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_None);

        ImGui::Text("rePlayer %u.%u.%u", Core::GetVersion() >> 28, (Core::GetVersion() >> 14) & ((1 << 14) - 1), Core::GetVersion() & ((1 << 14) - 1));
        ImGui::TextUnformatted(reinterpret_cast<const char*>(u8"Copyright (c) 2021-2023 Arnaud Nény (aka replay/Razor1911)"));

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
        ImGui::TextUnformatted("TagLib 1.13");
        ImGui::Bullet();
        ImGui::TextUnformatted("dllloader\n"
            "Copyright (c) 2012-2022 Scott Chacon and others");

        ImGui::TextUnformatted("\nReplays 3rd parties:");
        Core::GetReplays().DisplayAbout();

        ImGui::EndChild();
    }

}
// namespace rePlayer