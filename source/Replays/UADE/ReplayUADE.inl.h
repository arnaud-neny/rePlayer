#include "ReplayUADE.h"

#include <Core/String.h>

namespace rePlayer
{
    ReplayUADE::ReplayOverride ReplayUADE::ms_replayOverrides[] = {
        {
            "TFMX-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("mdat." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 20;
                data += 20;
                return eExtension::_tfm;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smpl.", 5) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize = *reinterpret_cast<const uint32_t*>(data + 12) - offset;
                    data += offset;
                    return true;
                }
                return false;
            },
            [](ReplayUADE* replay, const uint8_t*& data, size_t& dataSize)
            {
                (void)replay;
                dataSize = *reinterpret_cast<const uint32_t*>(data + 8) - 20;
                data += 20;
            }
        },
        {
            "CUST-PKG",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                for (auto stream = replay->m_stream; stream; stream = stream->Next())
                {
                    std::string entryName = stream->GetName();
                    auto name = ToLower(entryName);
                    if (strstr(name.c_str(), "cust.") == name.c_str()
                        || strstr(name.c_str(), "/cust."))
                    {
                        auto buffer = stream->Read();
                        replay->m_tempStream = stream;
                        filepath = entryName;
                        data = buffer.Items();
                        dataSize = buffer.NumItems();
                        break;
                    }
                }
                return eExtension::_cust;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                bool isFound = false;
                auto fileStr = ToLower(filename);
                if (fileStr.back() == '/')
                {
                    data = nullptr;
                    dataSize = 0;
                    return true;
                }
                replay->m_tempStream = replay->m_stream->Open(fileStr);
                if (replay->m_tempStream.IsValid())
                {
                    auto buffer = replay->m_tempStream->Read();
                    data = buffer.Items();
                    dataSize = buffer.NumItems();
                    isFound = true;
                }
                return isFound;
            },
            [](ReplayUADE* replay, const uint8_t*& data, size_t& dataSize)
            {
                (void)replay;
                (void)data;
                (void)dataSize;
            }
        },
        {
            "SMUS-PKG",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                replay->m_packagedSubsongNames.Clear();
                for (auto stream = replay->m_stream; stream; stream = stream->Next())
                {
                    std::string entryName = stream->GetName();
                    auto name = ToLower(entryName);
                    if (strstr(name.c_str(), ".smus") == (name.c_str() + name.size() - 5))
                    {
                        if (replay->m_subsongIndex == replay->m_packagedSubsongNames.NumItems())
                        {
                            auto buffer = stream->Read();
                            replay->m_tempStream = stream;
                            filepath = entryName;
                            data = buffer.Items();
                            dataSize = buffer.NumItems();
                        }
                        entryName.resize(entryName.size() - 5);
                        replay->m_packagedSubsongNames.Add(entryName);
                    }
                }
                return eExtension::_smus;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                bool isFound = false;
                auto fileStr = ToLower(filename);
                if (fileStr.back() == '/')
                {
                    data = nullptr;
                    dataSize = 0;
                    return true;
                }
                replay->m_tempStream = replay->m_stream->Open(fileStr);
                if (replay->m_tempStream.IsValid())
                {
                    auto buffer = replay->m_tempStream->Read();
                    data = buffer.Items();
                    dataSize = buffer.NumItems();
                    isFound = true;
                }
                return isFound;
            },
            [](ReplayUADE* replay, const uint8_t*& data, size_t& dataSize)
            {
                (void)replay;
                (void)data;
                (void)dataSize;
            }
        }
    };
}
// namespace rePlayer