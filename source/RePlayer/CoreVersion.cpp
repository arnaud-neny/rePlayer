#include "Core.h"

#include <Containers/Array.h>
#include <Core/String.h>
#include <IO/File.h>

// curl
#include <curl/curl.h>

// libarchive
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

// stl
#include <filesystem>

namespace rePlayer
{
    static constexpr uint32_t kMajorVersion = 0;
    static constexpr uint32_t kMinorVersion = 1;
    static constexpr uint32_t kPatchVersion = 1;
    static constexpr uint32_t kVersion = (kMajorVersion << 28) | (kMinorVersion << 14) | kPatchVersion;

    uint32_t Core::GetVersion()
    {
        return kVersion;
    }

    Status Core::CheckForNewVersion()
    {
        if (IsDebuggerPresent())
            return Status::kOk;

        auto mainPath = std::filesystem::current_path();

        // clean old files
        std::filesystem::remove_all(mainPath / "replays.old/");
        for (const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(mainPath))
        {
            if (dirEntry.is_regular_file() && _stricmp(dirEntry.path().extension().generic_string().c_str(), ".old") == 0)
            {
                std::filesystem::remove(dirEntry.path());
            }
        }

        // get the latest version url
        bool isReloadNeeded = false;
        CURL* curl = curl_easy_init();

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_URL, "https://github.com/arnaud-neny/rePlayer/releases/latest");

        if (curl_easy_perform(curl) == CURLE_OK)
        {
            char* url = nullptr;
            curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &url);
            if (url && strstr(url, "https://github.com/arnaud-neny/rePlayer/releases/tag/v"))
            {
                uint32_t majorVersion = 0;
                uint32_t minorVersion = 0;
                uint32_t patchVersion = 0;
                if (sscanf_s(url, "https://github.com/arnaud-neny/rePlayer/releases/tag/v%u.%u.%u", &majorVersion, &minorVersion, &patchVersion) >= 2
                    && majorVersion < 16 && minorVersion < 16384 && patchVersion < 16384
                    && ((majorVersion << 28) + (minorVersion << 14) + patchVersion) > kVersion)
                {
                    // download
                    struct Buffer : public Array<uint8_t>
                    {
                        static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
                        {
                            auto oldSize = buffer->NumItems();
                            buffer->Resize(oldSize + size * nmemb);

                            memcpy(&(*buffer)[oldSize], data, size * nmemb);

                            return size * nmemb;
                        }
                    } downloadBuffer;

                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloadBuffer);
                    char downloadUrl[256];
                     sprintf(downloadUrl, "https://github.com/arnaud-neny/rePlayer/releases/download/v%u.%u.%u/rePlayer.zip", majorVersion, minorVersion, patchVersion);
                    curl_easy_setopt(curl, CURLOPT_URL, downloadUrl);
                    if (curl_easy_perform(curl) == CURLE_OK)
                    {
                        // rename
                        std::filesystem::rename(std::filesystem::path(mainPath) / "rePlayer.exe", std::filesystem::path(mainPath) / "rePlayer.old");
                        std::filesystem::rename(std::filesystem::path(mainPath) / "replays/", std::filesystem::path(mainPath) / "replays.old/");
                        for (const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(mainPath))
                        {
                            if (dirEntry.is_regular_file() && _stricmp(dirEntry.path().extension().generic_string().c_str(), ".dll") == 0)
                            {
                                auto newpath = dirEntry.path();
                                newpath.replace_extension("old");
                                std::filesystem::rename(dirEntry.path(), newpath);
                            }
                        }

                        // unzip
                        auto archive = archive_read_new();
                        archive_read_support_format_all(archive);
                        archive_read_open_memory(archive, downloadBuffer.Items(), downloadBuffer.Size());
                        archive_entry* entry;
                        while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
                        {
                            auto fileSize = archive_entry_size(entry);
                            if (fileSize > 0)
                            {
                                Array<uint8_t> unpackedData;
                                unpackedData.Resize(fileSize);
                                archive_read_data(archive, unpackedData.Items(), fileSize);

                                // rebuild the path (remove the root path from the archive files)
                                auto entryPath = std::filesystem::path(archive_entry_pathname(entry));
                                auto path = mainPath;
                                for (auto it = ++entryPath.begin(), end = entryPath.end(); it != end; it++)
                                    path /= *it;
                                auto file = io::File::OpenForWrite(path.c_str());
                                file.Write(unpackedData.Items(), fileSize);
                            }
                        }
                        archive_read_free(archive);
                    }

                    isReloadNeeded = true;
                }
            }
        }
        curl_easy_cleanup(curl);

        if (isReloadNeeded)
        {
            PROCESS_INFORMATION processInfo;

            STARTUPINFO startupInfo = {};
            startupInfo.cb = sizeof(startupInfo);

            if (CreateProcess((mainPath / "rePlayer.exe").generic_string().c_str(), nullptr,
                nullptr, nullptr, FALSE, 0, nullptr,
                mainPath.generic_string().c_str(), &startupInfo, &processInfo))
            {
                CloseHandle(processInfo.hThread);
                CloseHandle(processInfo.hProcess);
            }
            else
                isReloadNeeded = false;
        }

        return isReloadNeeded ? Status::kFail : Status::kOk;
    }
}
// namespace rePlayer