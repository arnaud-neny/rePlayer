// Core
#include <Containers/SmartPtr.h>
#include <Core/String.h>
#include <Imgui.h>
#include <ImGui/imgui_internal.h>

// rePlayer
#include <rePlayer/Core.h>
#include <rePlayer/Replays.h>

#include "FileSelector.h"

// Windows
#include <windows.h>
#include <Shlobj.h>
#include <Shlwapi.h>

namespace rePlayer
{
    FileSelector* FileSelector::ms_instance = nullptr;

    void FileSelector::Open(const std::string& defaultDirectory)
    {
        if (ms_instance == nullptr)
            ms_instance = new FileSelector(defaultDirectory);
    }

    std::string FileSelector::Close()
    {
        std::string directory;
        if (ms_instance)
        {
            directory = ConstCharPtr(ms_instance->m_directory.u8string().c_str());
            delete ms_instance;
            ms_instance = nullptr;
        }
        return directory;
    }

    bool FileSelector::IsOpened()
    {
        return ms_instance != nullptr;
    }

    std::optional<Array<FileSelector::fsPath>> FileSelector::Display()
    {
        if (ms_instance)
            return ms_instance->SelectFiles();
        return {};
    }

    FileSelector::FileSelector(const std::string& defaultDirectory)
    {
        ScanRoots();
        if (!ScanDirectory(defaultDirectory))
            ScanDirectory(L".");
        Filter();
        m_history.Add(m_directory);
    }

    FileSelector::~FileSelector()
    {}

    std::optional<Array<FileSelector::fsPath>> FileSelector::SelectFiles()
    {
        std::optional<Array<FileSelector::fsPath>> ret;

        fsPath newDirectory;
        int32_t historyState = 0;

        // navigation bar
        ImGui::BeginDisabled(m_historyIndex == 0);
        if (ImGui::Button(ImGuiIconPrev))
        {
            newDirectory = m_history[m_historyIndex - 1];
            historyState = -1;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_historyIndex == m_history.NumItems() - 1 );
        if (ImGui::Button(ImGuiIconNext))
        {
            newDirectory = m_history[m_historyIndex + 1];
            historyState = 1;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(ImGuiIconRefresh))
        {
            ScanRoots();
            ScanDirectory(m_directory);
            Filter();
        }
        ImGui::SameLine();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0x60ffffff);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0xa0ffffff);
        if (m_isEditing)
        {
            ImGui::SameLine(0.0f, 0.0f);
            if (m_isEditing < 0)
                ImGui::SetKeyboardFocusHere(0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##EditPath", &m_editedDirectory, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                newDirectory = m_editedDirectory;
                m_isEditing = 0;
            }
            else if (m_isEditing < 0)
                m_isEditing = 1;
            else
                m_isEditing = ImGui::GetCurrentContext()->LastActiveId == ImGui::GetCurrentWindow()->GetID("##EditPath"); // IsItemFocused doesn't work, IsItemEdited doesn't work... and this test is not valid on first call to InputText
        }
        else
        {
            ImVec2 popupFoldersPosition;
            for (uint32_t i = 0; i < m_paths.NumItems(); i++)
            {
                auto txtSize = m_paths[i].size() + 16;
                char* txt = reinterpret_cast<char*>(_alloca(txtSize));
                ImGui::SameLine(0.0f, 0.0f);
                sprintf(txt, txtSize, "%s###PTH%d", reinterpret_cast<const char*>(m_paths[i].c_str()), i);
                if (ImGui::Button(txt))
                {
                    newDirectory = m_paths[0];
                    newDirectory += L"\\"; // filesystem on windows is...
                    for (uint32_t j = 1; j <= i; j++)
                        newDirectory /= m_paths[j];
                    historyState = 0;
                }
                if (i != m_lastPathIndex)
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, 0xffff9933);
                    sprintf(txt, txtSize, "%c###FLD%d", m_currentPathIndex != i ? '>' : 'v', i);
                    if (ImGui::Button(txt))
                    {
                        popupFoldersPosition.x = ImGui::GetItemRectMin().x;
                        popupFoldersPosition.y = ImGui::GetItemRectMax().y;
                        ScanFolders(i);
                        ImGui::OpenPopup("Folders");
                    }
                    ImGui::PopStyleColor();
                }
            }
            ImGui::SetNextWindowPos(popupFoldersPosition, ImGuiCond_Appearing);
            if (ImGui::BeginPopup("Folders", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
            {
                for (uint32_t i = 0; i < m_folders.NumItems(); i++)
                {
                    if (ImGui::Selectable(ConstCharPtr(m_folders[i].c_str())))
                    {
                        newDirectory = m_paths[0];
                        newDirectory += L"\\"; // filesystem on windows is...
                        for (uint32_t j = 1; j <= m_currentPathIndex; j++)
                            newDirectory /= m_paths[j];
                        newDirectory /= m_folders[i];
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }
            else
                m_currentPathIndex = 0xffFFffFF;
        }

        ImGui::PopStyleColor(3);
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
        ImGui::SameLine(0.0f, 0.0f);
        if (ImGui::Button("##Edit", ImVec2(-FLT_MIN, 0.0f)))
        {
            m_editedDirectory = ConstCharPtr(m_directory.u8string().c_str());
            m_isEditing = -1;
        }
        ImGui::PopStyleColor(3);

        // main table
        if (ImGui::BeginTable("Explorer", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit, ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 128.0f, 0);
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);

            ImGui::TableNextColumn();

            if (ImGui::BeginTable("Roots", 1, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY, ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))
            {
                for (uint32_t i = 0; i < m_roots.NumItems(); i++)
                {
                    ImGui::TableNextColumn();
                    ImGui::PushID(i);
                    ImGui::BeginDisabled(!m_roots[i].isValid);
                    if (ImGui::Selectable("##select", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0))
                        newDirectory = m_roots[i].path;
                    ImGui::SameLine(0.0f, 0.0f);//no spacing
                    ImGui::TextUnformatted(m_roots[i].name.c_str(), m_roots[i].name.c_str() + m_roots[i].name.size());
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TableNextColumn();
            if (ImGui::BeginTable("Folder", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti, ImVec2(0, -ImGui::GetFrameHeightWithSpacing())))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
                ImGui::TableSetupColumn("Ext", ImGuiTableColumnFlags_WidthFixed, 0.0f, 1);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                auto* sortsSpecs = ImGui::TableGetSortSpecs();
                if (sortsSpecs && sortsSpecs->SpecsDirty || m_isDirty)
                {
                    if (m_filteredEntries.NumItems() > 2)
                    {
                        // Don't filter the first one as it is ".."
                        std::sort(m_filteredEntries.begin() + 1, m_filteredEntries.end(), [&](auto l, auto r)
                        {
                            auto& lEntry = m_entries[l];
                            auto& rEntry = m_entries[r];
                            // always directories first
                            if (lEntry.isDirectory != rEntry.isDirectory)
                                return lEntry.isDirectory;
                            // now sort names and extensions
                            for (int i = 0; i < sortsSpecs->SpecsCount; i++)
                            {
                                auto& sortSpec = sortsSpecs->Specs[i];
                                int32_t delta = 0;
                                switch (sortSpec.ColumnUserID)
                                {
                                case 0:
                                    delta = CompareStringMixed(ConstCharPtr(lEntry.name.c_str()), ConstCharPtr(rEntry.name.c_str()));
                                    break;
                                case 1:
                                    delta = CompareStringMixed(ConstCharPtr(lEntry.ext.c_str()), ConstCharPtr(rEntry.ext.c_str()));
                                    break;
                                }
                                if (delta)
                                    return (sortSpec.SortDirection == ImGuiSortDirection_Ascending) ? delta < 0 : delta > 0;
                            }
                            return l < r;
                        });
                    }
                    sortsSpecs->SpecsDirty = false;
                    m_isDirty = false;
                }

                ImGuiListClipper clipper;
                clipper.Begin(m_filteredEntries.NumItems<int32_t>());
                while (clipper.Step())
                {
                    for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++)
                    {
                        auto index = m_filteredEntries[rowIdx];
                        auto& entry = m_entries[index];
                        ImGui::TableNextColumn();
                        ImGui::PushID(index);
                        ImGui::BeginDisabled(!entry.isValid);
                        if (ImGui::Selectable("##select", entry.isSelected && index != 0, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            if (entry.isDirectory && ImGui::IsMouseDoubleClicked(0))
                            {
                                newDirectory = m_directory / entry.name;
                            }
                            else
                            {
                                if (ImGui::GetIO().KeyShift)
                                {
                                    if (m_lastSelected < 0)
                                        m_lastSelected = rowIdx;
                                    if (!ImGui::GetIO().KeyCtrl)
                                    {
                                        for (auto si : m_filteredEntries)
                                            m_entries[si].isSelected = false;
                                    }
                                    auto start = rowIdx;
                                    auto end = m_lastSelected;
                                    if (start > end)
                                        std::swap(start, end);
                                    for (; start <= end; start++)
                                        m_entries[m_filteredEntries[start]].isSelected = true;
                                }
                                else
                                {
                                    m_lastSelected = rowIdx;
                                    if (!ImGui::GetIO().KeyCtrl)
                                    {
                                        for (auto si : m_filteredEntries)
                                            m_entries[si].isSelected = false;
                                    }
                                    entry.isSelected = !entry.isSelected;
                                }
                            }
                        }
                        ImGui::SameLine(0.0f, 0.0f);//no spacing
                        std::string name = entry.isDirectory ? ImGuiIconFolder : ImGuiIconFile;
                        name += ConstCharPtr(entry.name.c_str());
                        ImGui::TextUnformatted(name.c_str(), name.c_str() + name.size());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(ConstCharPtr(entry.ext.c_str()), ConstCharPtr(entry.ext.c_str()) + entry.ext.size());
                        ImGui::EndDisabled();
                        ImGui::PopID();
                    }
                }

                ImGui::EndTable();
            }

            ImGui::EndTable();
        }

        // footer bar
        ImGui::BeginDisabled(false);
        if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
        {
            Array<FileSelector::fsPath> selection;
            const std::u8string directory = m_directory.u8string() + u8"\\";
            auto& fileFilters = Core::GetReplays().GetFileFilters();
            auto& fileFilter = fileFilters.Get(m_fileFilterIndex);
            for (auto& entry : m_entries)
            {
                if (entry.isSelected && entry.isValid)
                {
                    if (entry.isDirectory)
                    {
                        auto path = directory + entry.name + u8"\\";
                        std::error_code ec;
                        for (const std::filesystem::directory_entry& dirEntry : std::filesystem::recursive_directory_iterator(path, ec))
                        {
                            if (dirEntry.is_regular_file(ec))
                            {
                                if (m_fileFilterIndex == 0)
                                {
                                    selection.Add(dirEntry);
                                }
                                else for (uint32_t j = 0; j < fileFilter.numExtensions; j++)
                                {
                                    if (dirEntry.path().extension().native().size() == fileFilter.extensions[j].size + 1)
                                    {
                                        auto ext = dirEntry.path().extension().u8string();
                                        if (_strnicmp(ConstCharPtr(ext.c_str() + 1), fileFilter.Get(fileFilters, j), fileFilter.extensions[j].size) == 0)
                                        {
                                            selection.Add(dirEntry);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        auto filename = directory + entry.name;
                        if (!entry.ext.empty())
                        {
                            filename += '.';
                            filename += entry.ext;
                        }
                        selection.Add(std::filesystem::canonical(filename));
                    }
                }
            }
            ret = std::move(selection);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        auto& fileFilters = Core::GetReplays().GetFileFilters();
        if (ImGui::BeginCombo("##Extensions", fileFilters.Label(m_fileFilterIndex), ImGuiComboFlags_PopupAlignLeft))
        {
            auto fileFilterIndex = m_fileFilterIndex;
            for (uint32_t filterIndex = 0; filterIndex < fileFilters.Count(); filterIndex++)
            {
                ImGui::PushID(filterIndex);
                if (ImGui::Selectable("##Select", false, ImGuiSelectableFlags_AllowOverlap) && fileFilterIndex != filterIndex)
                {
                    m_fileFilterIndex = filterIndex;
                    Filter();
                }
                if (fileFilterIndex == filterIndex)
                    ImGui::SetItemDefaultFocus();
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextUnformatted(fileFilters.Label(filterIndex));
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        if (!newDirectory.empty() && ScanDirectory(newDirectory))
        {
            Filter();
            if (historyState == 0)
            {
                m_history.Resize(++m_historyIndex);
                m_history.Add(m_directory);
            }
            else
                m_historyIndex += historyState;
        }

        return ret;
    }

    bool FileSelector::ScanDirectory(const fsPath& directory)
    {
        auto path = std::filesystem::canonical(directory);
        if (m_directory.native().size() == path.native().size() && _wcsicmp(m_directory.native().c_str(), path.native().c_str()) == 0)
            return false;

        auto findPath = path / "*.*";
        WIN32_FIND_DATAW findData = {};
        auto& fileEntry = reinterpret_cast<uint64_t&>(findData.cFileName);
        auto hFind = FindFirstFileW(findPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            BuildPaths(path.u8string());
            bool hasDirectories = false;
            m_entries.Clear();
            m_entries.Add({ u8"..", u8"", true, true, false });
            do
            {
                if (fileEntry != 0x2e && fileEntry != 0x2e002e) // != "." & != ".."
                {
                    auto* entry = m_entries.Push();
                    findPath = findData.cFileName;
                    entry->isDirectory = findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
                    std::error_code ec;
                    entry->isValid = std::filesystem::exists(path / findData.cFileName, ec); // exists will fail if the file can't be accessed
                    entry->isSelected = false;
                    if (entry->isDirectory)
                    {
                        entry->name = findPath.filename().u8string();
                    }
                    else
                    {
                        entry->name = findPath.stem().u8string();
                        if (findPath.has_extension())
                            entry->ext = findPath.extension().u8string().c_str() + 1;
                    }
                    hasDirectories |= entry->isDirectory;
                }
                fileEntry = 0;
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);

            m_lastPathIndex = !hasDirectories ? m_paths.NumItems() - 1 : 0xffFFffFF;
            m_directory = path;

            return true;
        }
        return false;
    }

    void FileSelector::ScanRoots()
    {
        m_roots.Clear();

        // Windows shortcuts
        static const KNOWNFOLDERID folderIds[] = {
            FOLDERID_Desktop,
            FOLDERID_Documents,
            FOLDERID_Downloads,
            FOLDERID_Music,
            FOLDERID_Videos
        };
        static const char* const folderNames[] = {
            "Desktop",
            "Documents",
            "Downloads",
            "Music",
            "Videos"
        };
        static_assert(_countof(folderIds) == _countof(folderNames));

        for (size_t i = 0; i < _countof(folderIds); i++)
        {
            PWSTR path = nullptr;
            HRESULT hr = SHGetKnownFolderPath(folderIds[i], 0, nullptr, &path);
            if (SUCCEEDED(hr))
            {
                auto* root = m_roots.Push();
                root->path = path;
                root->name = ImGuiIconFolder;
                root->name += folderNames[i];

                WIN32_FIND_DATAW findData;
                auto hFind = FindFirstFileW((root->path / L"*.*").c_str(), &findData);
                if (root->isValid = hFind != INVALID_HANDLE_VALUE)
                    FindClose(hFind);
            }
            ::CoTaskMemFree(path);
        }

        // drives
        std::wstring drives;
        drives.resize(::GetLogicalDriveStringsW(0, nullptr));
        auto numChars = ::GetLogicalDriveStringsW(DWORD(drives.size()), drives.data());
        for (size_t i = 0; i < numChars;)
        {
            auto len = 0;
            for (; i < numChars && drives[i] == 0; i++);
            for (; (i + len) < numChars && drives[i + len] != 0; len++);
            if (len)
            {
                auto* root = m_roots.Push();
                root->path = drives.c_str() + i;

                // thx to https://github.com/derceg/explorerplusplus

                PIDLIST_ABSOLUTE pidl;
                if (SUCCEEDED(SHParseDisplayName(drives.c_str() + i, nullptr, &pidl, 0, nullptr)))
                {
                    SmartPtr<IShellFolder> shellFolder;
                    PCITEMID_CHILD pidlChild = nullptr;
                    if (SUCCEEDED(SHBindToParent(pidl, IID_PPV_ARGS(&shellFolder), &pidlChild)))
                    {
                        STRRET str;
                        if (SUCCEEDED(shellFolder->GetDisplayNameOf(pidlChild, SHGDN_NORMAL, &str)))
                        {
                            PSTR cName = nullptr;
                            if (SUCCEEDED(StrRetToStr(&str, pidlChild, &cName)))
                            {
                                root->name = ImGuiIconDrive;
                                root->name += cName;
                            }
                            ::CoTaskMemFree(cName);
                        }
                    }
                }
                if (root->name.empty())
                {
                    root->name = ImGuiIconDrive "??? ";
                    root->name += std::filesystem::path(drives.c_str() + i).string();
                }

                WIN32_FIND_DATAW findData;
                auto hFind = FindFirstFileW((root->path / L"*.*").c_str(), &findData);
                if (root->isValid = hFind != INVALID_HANDLE_VALUE)
                    FindClose(hFind);
            }
            i += len;
        }
    }

    void FileSelector::ScanFolders(uint32_t pathIndex)
    {
        m_folders.Clear();

        fsPath findPath = m_paths[0];
        findPath += L"\\"; // filesystem on windows is...
        for (uint32_t i = 1; i <= pathIndex; i++)
            findPath /= m_paths[i];
        findPath /= "*.*";
        WIN32_FIND_DATAW findData = {};
        auto& fileEntry = reinterpret_cast<uint64_t&>(findData.cFileName);
        auto hFind = FindFirstFileW(findPath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fileEntry != 0x2e && fileEntry != 0x2e002e) // != "." & != ".."
                {
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        findPath = findData.cFileName;
                        m_folders.Add(findPath.stem().u8string());
                    }
                }
                fileEntry = 0;
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);

            std::sort(m_folders.begin(), m_folders.end(), [&](auto& l, auto& r)
            {
                return l < r;
            });
        }
        m_currentPathIndex = pathIndex;
    }

    void FileSelector::Refresh(const fsPath& directory)
    {
        ScanRoots();
        ScanDirectory(directory);
    }

    void FileSelector::BuildPaths(const std::u8string& path)
    {
        m_paths.Clear();
        for (auto* c = path.c_str(); *c;)
        {
            for (; *c == '\\'; c++);
            auto* e = c;
            for (; *e != 0 && *e != '\\'; e++);
            if (c != e)
            {
                auto p = m_paths.Push();
                p->assign(c, e);
            }
            c = e;
        }
    }

    void FileSelector::Filter()
    {
        auto numEntries = m_entries.NumItems();
        if (m_fileFilterIndex == 0)
        {
            m_filteredEntries.Resize(numEntries);
            for (uint32_t i = 0; i < numEntries; i++)
                m_filteredEntries[i] = i;
        }
        else
        {
            m_filteredEntries.Clear();
            auto& fileFilters = Core::GetReplays().GetFileFilters();
            auto& fileFilter = fileFilters.Get(m_fileFilterIndex);
            for (uint32_t i = 0; i < numEntries; i++)
            {
                if (m_entries[i].isDirectory)
                {
                    m_filteredEntries.Add(i);
                }
                else
                {
                    auto isAdded = false;
                    for (uint32_t j = 0; j < fileFilter.numExtensions; j++)
                    {
                        if (m_entries[i].ext.size() == fileFilter.extensions[j].size && _strnicmp(ConstCharPtr(m_entries[i].ext.c_str()), fileFilter.Get(fileFilters, j), fileFilter.extensions[j].size) == 0)
                        {
                            m_filteredEntries.Add(i);
                            isAdded = true;
                            break;
                        }
                    }
                    m_entries[i].isSelected &= isAdded;
                }
            }
        }
        m_isDirty = true;
    }
}
// namespace rePlayer