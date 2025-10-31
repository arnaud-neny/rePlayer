// Core
#include <ImGui.h>
#include <IO/File.h>

// rePlayer
#include <Library/LibraryDatabase.h>
#include <Library/LibraryDatabaseUI.inl.h>
#include <RePlayer/Core.h>
#include <UI/BusySpinner.h>

#include "LibraryArtistsUI.h"

// stl
#include <algorithm>
#include <ctime>

namespace rePlayer
{
    Library::ArtistsUI::ArtistsUI(Window& owner)
        : Library::DatabaseUI<DatabaseArtistsUI>(owner)
    {}

    Library::ArtistsUI::~ArtistsUI()
    {}

    Library& Library::ArtistsUI::GetLibrary()
    {
        return reinterpret_cast<Library&>(m_owner);
    }

    void Library::ArtistsUI::SourcesUI(Artist* selectedArtist)
    {
        ImGui::SeparatorText("Source");

        Artist* moveToArtist = nullptr;
        uint32_t moveToArtistSourceIndex = 0;

        char txt[256];
        for (auto& source : selectedArtist->Sources())
        {
            ImGui::PushID(source.id.value);
            if (source.timeFetch)
            {
                tm t;
                localtime_s(&t, &source.timeFetch);
                sprintf_s(txt, "%4d/%02d/%02d %02d:%02d %06X %s", 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, source.id.internalId, source.GetName());
            }
            else
                sprintf_s(txt, "----/--/-- --:-- %06X %s", source.id.internalId, source.GetName());
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##source", txt, sizeof(txt), ImGuiInputTextFlags_ReadOnly);
            if (ImGui::GetIO().KeyShift && ImGui::IsItemHovered())
            {
                auto stub = GetLibrary().m_sources[source.id.sourceId]->GetArtistStub(source.id);
                if (!stub.empty())
                    ImGui::Tooltip(stub.c_str(), stub.c_str() + stub.size());
            }
            if (ImGui::BeginPopupContextItem("Source popup"))
            {
                ImGui::SetNextWindowSizeConstraints(ImVec2(128.0f, ImGui::GetTextLineHeightWithSpacing()), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 16));
                if (ImGui::BeginMenu("Move to artist"))
                {
                    ArtistID pickedArtistId = PickArtist(selectedArtist->GetId());
                    if (pickedArtistId != ArtistID::Invalid)
                    {
                        moveToArtist = m_db[pickedArtistId];
                        moveToArtistSourceIndex = uint32_t(&source - selectedArtist->Sources().begin());
                    }
                    ImGui::EndMenu();
                }
                else
                    m_artistPicker.isOpened = false;
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (moveToArtist != nullptr)
        {
            moveToArtist->Edit()->sources.Add(selectedArtist->GetSource(moveToArtistSourceIndex));
            selectedArtist->Edit()->sources.RemoveAt(moveToArtistSourceIndex);
        }
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(5.0f / 7.0f, 0.6f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(5.0f / 7.0f, 0.7f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(5.0f / 7.0f, 0.8f, 1.0f));
        if (ImGui::Button("Import", ImVec2(-FLT_MIN, 0)))
        {
            auto& library = GetLibrary();
            library.m_imports = {};
            library.m_busySpinner.New(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
            library.m_busySpinner->Info("Importing artist");
            library.m_importArtists.isOpened = library.m_importArtists.isExternal = true;
            Core::AddJob([&library, artist = SmartPtr<ArtistSheet>(selectedArtist->Edit())]()
            {
                library.m_busySpinner->Indent(1);

                SourceResults sourceResults;
                for (auto& source : artist->sources)
                    library.ImportArtist(source.id, sourceResults);

                library.m_busySpinner->Indent(-1);
                Core::FromJob([&library, sourceResults = std::move(sourceResults)]()
                {
                    library.m_imports.isOpened = &library.m_importArtists.isOpened;
                    library.m_imports.sourceResults = std::move(sourceResults);
                    library.m_busySpinner.Reset();
                });
            });
        }
        ImGui::PopStyleColor(3);
    }

    void Library::ArtistsUI::OnSavedChanges(Artist* selectedArtist)
    {
        auto& db = reinterpret_cast<LibraryDatabase&>(m_db);
        Array<std::pair<Song*, std::string>> toRename;
        std::string oldDirectory = db.GetDirectory(selectedArtist);
        if (strcmp(selectedArtist->GetHandle(), m_selectedArtistCopy.handles[0].Items()))
        {
            auto selectedArtistId = selectedArtist->GetId();
            for (auto* song : m_db.Songs())
            {
                if (song->GetFileSize() == 0)
                    continue;
                for (auto artistId : song->ArtistIds())
                {
                    if (artistId == selectedArtistId)
                    {
                        toRename.Push();
                        toRename.Last().first = song;
                        toRename.Last().second = db.GetFullpath(song);
                        break;
                    }
                }
            }
        }

        auto selectedArtistEdit = selectedArtist->Edit();
        selectedArtistEdit->realName = m_selectedArtistCopy.realName;
        selectedArtistEdit->handles = m_selectedArtistCopy.handles;
        selectedArtistEdit->countries = m_selectedArtistCopy.countries;
        selectedArtistEdit->groups = m_selectedArtistCopy.groups;

        if (toRename.IsNotEmpty())
        {
            for (auto& entry : toRename)
            {
                auto filename = db.GetFullpath(entry.first);
                io::File::Move(entry.second.c_str(), filename.c_str());
            }
            //call move (rename) if it's only a case change
            io::File::Move(oldDirectory.c_str(), db.GetDirectory(selectedArtist).c_str());
        }

        m_db.Raise(Database::Flag::kSaveArtists);
    }
}
// namespace rePlayer