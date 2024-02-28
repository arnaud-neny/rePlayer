#pragma once

// Core
#include <Containers/Array.h>

// stl
#include <filesystem>
#include <optional>

namespace rePlayer
{
    using namespace core;

    class FileSelector
    {
        using fsPath = std::filesystem::path;
        using fsDirEntry = std::filesystem::directory_entry;
        using ConstCharPtr = const char*;
    public:
        static void Open(const std::string& defaultDirectory);
        static std::string Close();
        static bool IsOpened();

        static std::optional<Array<fsPath>> Display();

    private:
        struct Root
        {
            fsPath path;
            std::string name;
            bool isValid;
        };
        struct Entry
        {
            std::u8string name;
            std::u8string ext;
            bool isDirectory;
            bool isValid;
            bool isSelected;
        };

    private:
        FileSelector(const std::string& defaultDirectory);
        ~FileSelector();

        std::optional<Array<fsPath>> SelectFiles();

        bool ScanDirectory(const fsPath& directory);
        void ScanRoots();
        void ScanFolders(uint32_t pathIndex);

        void Refresh(const fsPath& directory);

        void BuildPaths(const std::u8string& path);
        void Filter();

    private:
        Array<Entry> m_entries;
        Array<uint32_t> m_filteredEntries;
        Array<Root> m_roots;

        Array<fsPath> m_history;

        Array<std::u8string> m_paths;
        Array<std::u8string> m_folders;

        fsPath m_directory;
        std::string m_editedDirectory;

        uint32_t m_lastPathIndex = 0xffFFffFF;
        uint32_t m_currentPathIndex = 0xffFFffFF;
        uint32_t m_fileFilterIndex = 0;
        uint32_t m_historyIndex = 0;
        int32_t m_lastSelected = -1;

        bool m_isDirty = true;
        int8_t m_isEditing = 0;
        static FileSelector* ms_instance;
    };
}
// namespace rePlayer