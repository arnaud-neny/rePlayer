#include "ReplayUADE.h"

#include <Core/String.h>
#include <IO/StreamMemory.h>

#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

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
            "RiJo-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_extension("sng");
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_rjp;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                auto len = strlen(filename);
                if (strnicmp(filename + len - 4, ".ins", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "ADSC-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_adsc;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                auto len = strlen(filename);
                if (strnicmp(filename + len - 3, ".as", 3) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "DIRK-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("tpu." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_tpu;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "DNSY-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("dns." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_dns;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "IFGM-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_dum;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                auto len = strlen(filename);
                if (strnicmp(filename + len - 4, ".ins", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "JSPG-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("jpn." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_jpn;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "KSHD-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_kh;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (stricmp(filename, "songplay") == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "MFPK-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("mfp." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_mfp;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "MCOD-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("mcr." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_mcr;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "mcs.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "PAPK-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_pap;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (stricmp(filename, "smp.set") == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "PKNS-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("pn." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_pn;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                auto len = strlen(filename);
                if (strnicmp(filename + len - 5, ".info", 5) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "QTST-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("qts." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_qts;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "SDPR-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("sjs." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_sjs;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "SHDM-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("sdr." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_sdr;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "SHPK-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_osp;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (stricmp(filename, "smp.set") == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "TSHN-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                filepath.replace_filename("thm." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_thm;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                if (strnicmp(filename, "smp.", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "PLRM-MOD",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)replay;
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_dat;
            },
            [](ReplayUADE* replay, const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                (void)replay;
                auto len = strlen(filename);
                if (strnicmp(filename + len - 4, ".ssd", 4) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize -= offset;
                    data += offset;
                    return true;
                }
                return false;
            }
        },
        {
            "CUST-PKG",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                auto* uadeArchive = archive_read_new();
                archive_read_support_format_7zip(uadeArchive);
                if (archive_read_open_memory(uadeArchive, data + 8, dataSize - 8) == ARCHIVE_OK)
                {
                    archive_entry* entry;
                    while (archive_read_next_header(uadeArchive, &entry) == ARCHIVE_OK)
                    {
                        std::string entryName = archive_entry_pathname(entry);
                        auto name = ToLower(entryName);
                        if (strstr(name.c_str(), "cust.") == name.c_str()
                            || strstr(name.c_str(), "/cust."))
                        {
                            auto fileSize = archive_entry_size(entry);
                            auto buffer = core::Alloc<uint8_t>(fileSize, 8);
                            archive_read_data(uadeArchive, buffer, fileSize);
                            replay->m_tempStream = core::io::StreamMemory::Create(entryName.c_str(), buffer, fileSize, false);
                            filepath = entryName;
                            data = buffer;
                            dataSize = fileSize;
                            break;
                        }
                        archive_read_data_skip(uadeArchive);
                    }
                }
                archive_read_free(uadeArchive);
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
                auto* uadeArchive = archive_read_new();
                archive_read_support_format_7zip(uadeArchive);
                if (archive_read_open_memory(uadeArchive, data + 8, dataSize - 8) == ARCHIVE_OK)
                {
                    archive_entry* entry;
                    while (archive_read_next_header(uadeArchive, &entry) == ARCHIVE_OK)
                    {
                        std::string entryName = archive_entry_pathname(entry);
                        if (ToLower(entryName) == fileStr)
                        {
                            auto fileSize = archive_entry_size(entry);
                            auto buffer = core::Alloc<uint8_t>(fileSize, 8);
                            archive_read_data(uadeArchive, buffer, fileSize);
                            replay->m_tempStream = core::io::StreamMemory::Create(entryName.c_str(), buffer, fileSize, false);
                            data = buffer;
                            dataSize = fileSize;
                            isFound = true;
                            break;
                        }
                        archive_read_data_skip(uadeArchive);
                    }
                }
                archive_read_free(uadeArchive);
                return isFound;
            },
            [](ReplayUADE* replay, const uint8_t*& data, size_t& dataSize)
            {
                (void)replay;
                (void)data;
                (void)dataSize;
                assert(0);
            }
        },
        {
            "SMUS-PKG",
            [](ReplayUADE* replay, const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                replay->m_packagedSubsongNames.Clear();
                auto* uadeArchive = archive_read_new();
                archive_read_support_format_7zip(uadeArchive);
                if (archive_read_open_memory(uadeArchive, data + 8, dataSize - 8) == ARCHIVE_OK)
                {
                    archive_entry* entry;
                    while (archive_read_next_header(uadeArchive, &entry) == ARCHIVE_OK)
                    {
                        std::string entryName = archive_entry_pathname(entry);
                        auto name = ToLower(entryName);
                        if (strstr(name.c_str(), ".smus") == (name.c_str() + name.size() - 5))
                        {
                            if (replay->m_subsongIndex == replay->m_packagedSubsongNames.NumItems())
                            {
                                auto fileSize = archive_entry_size(entry);
                                auto buffer = core::Alloc<uint8_t>(fileSize, 8);
                                archive_read_data(uadeArchive, buffer, fileSize);
                                replay->m_tempStream = core::io::StreamMemory::Create(entryName.c_str(), buffer, fileSize, false);
                                filepath = entryName;
                                data = buffer;
                                dataSize = fileSize;
                            }
                            entryName.resize(entryName.size() - 5);
                            replay->m_packagedSubsongNames.Add(entryName);
                        }
                        archive_read_data_skip(uadeArchive);
                    }
                }
                archive_read_free(uadeArchive);
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
                auto* uadeArchive = archive_read_new();
                archive_read_support_format_7zip(uadeArchive);
                if (archive_read_open_memory(uadeArchive, data + 8, dataSize - 8) == ARCHIVE_OK)
                {
                    archive_entry* entry;
                    while (archive_read_next_header(uadeArchive, &entry) == ARCHIVE_OK)
                    {
                        std::string entryName = archive_entry_pathname(entry);
                        if (ToLower(entryName) == fileStr)
                        {
                            auto fileSize = archive_entry_size(entry);
                            auto buffer = core::Alloc<uint8_t>(fileSize, 8);
                            archive_read_data(uadeArchive, buffer, fileSize);
                            replay->m_tempStream = core::io::StreamMemory::Create(entryName.c_str(), buffer, fileSize, false);
                            data = buffer;
                            dataSize = fileSize;
                            isFound = true;
                            break;
                        }
                        archive_read_data_skip(uadeArchive);
                    }
                }
                archive_read_free(uadeArchive);
                return isFound;
            },
            [](ReplayUADE* replay, const uint8_t*& data, size_t& dataSize)
            {
                (void)replay;
                (void)data;
                (void)dataSize;
                assert(0);
            }
        }
    };
}
// namespace rePlayer