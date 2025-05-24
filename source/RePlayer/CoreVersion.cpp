#include "Core.h"
#include "Version.h"

#include <Containers/Array.h>
#include <Core/String.h>
#include <IO/File.h>
#include <Thread/Thread.h>

// curl
#include <curl/curl.h>

// libarchive
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

// stl
#include <filesystem>

namespace rePlayer
{
    static constexpr uint32_t kMajorVersion = REPLAYER_VERSION_MAJOR;
    static constexpr uint32_t kMinorVersion = REPLAYER_VERSION_MINOR;
    static constexpr uint32_t kPatchVersion = REPLAYER_VERSION_PATCH;
    static constexpr uint32_t kVersion = (kMajorVersion << 28) | (kMinorVersion << 14) | kPatchVersion;

    uint32_t Core::GetVersion()
    {
        return kVersion;
    }

    const char* const Core::GetLabel()
    {
        #define REPLAYER_HELPER_STRINGIZE(x) #x
        #define REPLAYER_STRINGIZE(x) REPLAYER_HELPER_STRINGIZE(x)
        return "rePlayer " REPLAYER_STRINGIZE(REPLAYER_VERSION_MAJOR) "." REPLAYER_STRINGIZE(REPLAYER_VERSION_MINOR) "." REPLAYER_STRINGIZE(REPLAYER_VERSION_PATCH);
        #undef REPLAYER_HELPER_STRINGIZE
        #undef REPLAYER_STRINGIZE
    }

    Status Core::CheckForNewVersion()
    {
        if (IsDebuggerPresent())
            return Status::kOk;

        // Allow only one instance
        for (int numRetries = 4;;)
        {
            ::CreateMutex(0, false, "Local\\$rePlayer$");
            if (GetLastError() == ERROR_ALREADY_EXISTS)
            {
                thread::Sleep(500);
                if (--numRetries == 0)
                {
                    MessageBox(nullptr, "Already running", "rePlayer", MB_OK);
                    return Status::kFail;
                }
            }
            else
                break;
        }

        auto mainPath = std::filesystem::current_path();

        // clean old files
        std::filesystem::remove_all(mainPath / "replays" REPLAYER_OS_STUB ".old/");
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
                if ((sscanf_s(url, "https://github.com/arnaud-neny/rePlayer/releases/tag/v%u.%u.%u", &majorVersion, &minorVersion, &patchVersion) >= 2
                    && majorVersion < 16 && minorVersion < 16384 && patchVersion < 16384
                    && ((majorVersion << 28) + (minorVersion << 14) + patchVersion) > kVersion)
                    || !std::filesystem::exists(mainPath / "replays" REPLAYER_OS_STUB / "Psycle")) // extra check to re-update if the previous version was older than the 0.19.0)
                {
                    // download
                    struct Buffer : public Array<uint8_t>
                    {
                        static size_t Writer(const uint8_t* data, size_t size, size_t nmemb, Buffer* buffer)
                        {
                            auto oldSize = buffer->NumItems();
                            buffer->Resize(uint32_t(oldSize + size * nmemb));

                            memcpy(&(*buffer)[oldSize], data, size * nmemb);

                            return size * nmemb;
                        }
                    } downloadBuffer;

                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Buffer::Writer);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloadBuffer);
                    char downloadUrl[256];
                    sprintf(downloadUrl, "https://github.com/arnaud-neny/rePlayer/releases/download/v%u.%u.%u/rePlayer" REPLAYER_OS_STUB ".zip", majorVersion, minorVersion, patchVersion);
                    curl_easy_setopt(curl, CURLOPT_URL, downloadUrl);
                    if (curl_easy_perform(curl) == CURLE_OK)
                    {
                        // rename
                        std::filesystem::rename(std::filesystem::path(mainPath) / "rePlayer" REPLAYER_OS_STUB ".exe", std::filesystem::path(mainPath) / "rePlayer" REPLAYER_OS_STUB ".old");
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
                        std::string errors;
                        auto archive = archive_read_new();
                        archive_read_support_format_all(archive);
                        archive_read_open_memory(archive, downloadBuffer.Items(), downloadBuffer.Size<size_t>());
                        archive_entry* entry;
                        while (archive_read_next_header(archive, &entry) == ARCHIVE_OK)
                        {
                            // rebuild the path (remove the root path from the archive files)
                            auto entryPath = std::filesystem::path(archive_entry_pathname(entry));
                            auto path = mainPath;
                            for (auto it = ++entryPath.begin(), end = entryPath.end(); it != end; it++)
                                path /= *it;

                            // extract
                            if ((archive_entry_mode(entry) & S_IFMT) == S_IFDIR)
                                std::filesystem::create_directory(path);
                            else
                            {
                                auto fileSize = archive_entry_size(entry);

                                Array<uint8_t> unpackedData;
                                unpackedData.Resize(uint32_t(fileSize));
                                archive_read_data(archive, unpackedData.Items(), size_t(fileSize));

                                auto file = io::File::OpenForWrite(path.c_str());
                                if (file.IsValid())
                                    file.Write(unpackedData.Items(), fileSize);
                                else
                                {
                                    if (errors.empty())
                                        errors = "Can't write:";
                                    char txt[256];
                                    sprintf(txt, "\n- \"%s\"", archive_entry_pathname(entry));
                                    errors += txt;
                                }
                            }
                        }
                        archive_read_free(archive);

                        if (!errors.empty())
                            MessageBox(nullptr, errors.c_str(), "rePlayer", MB_OK);
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

            if (CreateProcess((mainPath / "rePlayer" REPLAYER_OS_STUB ".exe").generic_string().c_str(), nullptr,
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