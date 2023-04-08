#include "ReplayUADE.h"

namespace rePlayer
{
    ReplayUADE::ReplayOverride ReplayUADE::ms_replayOverrides[] = {
        {
            "TFMX-MOD",
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("mdat." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 20;
                data += 20;
                return eExtension::_tfm;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
                if (strnicmp(filename, "smpl.", 5) == 0)
                {
                    auto offset = *reinterpret_cast<const uint32_t*>(data + 8);
                    dataSize = *reinterpret_cast<const uint32_t*>(data + 12) - offset;
                    data += offset;
                    return true;
                }
                return false;
            }
            , 20
        },
        {
            "RiJo-MOD",
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_extension("sng");
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_rjp;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_adsc;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("tpu." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_tpu;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("dns." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_dns;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_dum;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("jpn." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_jpn;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_kh;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("mfp." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_mfp;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("mcr." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_mcr;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_pap;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("pn." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_pn;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("qts." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_qts;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("sjs." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_sjs;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("sdr." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_sdr;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_osp;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                filepath.replace_filename("thm." + filepath.filename().string());
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_thm;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
            [](const uint8_t*& data, std::filesystem::path& filepath, size_t& dataSize)
            {
                (void)filepath;
                auto smplOfs = *reinterpret_cast<const uint32_t*>(data + 8);
                dataSize = smplOfs - 12;
                data += 12;
                return eExtension::_dat;
            },
            [](const uint8_t*& data, const char* filename, size_t& dataSize)
            {
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
        }
    };
}
// namespace rePlayer