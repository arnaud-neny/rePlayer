#include "ReplayAdLib.h"
#include "adplugdb.h"

#include <Audio/AudioTypes.inl.h>
#include <Core/String.h>
#include <Core/Window.inl.h>
#include <IO/StreamFile.h>
#include <IO/StreamMemory.h>
#include <Imgui.h>
#include <ReplayDll.h>

#include "adplug/adplug.h"
#include "adplug/emuopl.h"
#include "adplug/kemuopl.h"
#include "adplug/nemuopl.h"
#include "adplug/surroundopl.h"
#include "adplug/version.h"
#include "adplug/wemuopl.h"

#include <binio.h>

namespace rePlayer
{
    ReplayPlugin g_replayPlugin = {
        .replayId = eReplay::AdLib,
        .name = "AdPlug",
        .about = "AdPlug " ADPLUG_VERSION "\nCopyright (c) 1999-2017 Simon Peter, et al.",
        .init = ReplayAdLib::Init,
        .release = ReplayAdLib::Release,
        .load = ReplayAdLib::Load,
        .displaySettings = ReplayAdLib::DisplaySettings,
        .getFileFilter = ReplayAdLib::GetFileFilters
    };

    COPLprops* ReplayAdLib::CreateCore(bool isStereo)
    {
        COPLprops* core = new COPLprops;
        core->stereo = isStereo;
        core->use16bit = true;
        switch (ms_opl)
        {
        case Opl::kDOSBoxOPL3Emulator:
            core->opl = new CWemuopl(kSampleRate, true, isStereo);
            break;
        case Opl::kNukedOPL3Emulator:
            core->opl = new CNemuopl(kSampleRate);
            core->stereo = true;
            break;
        case Opl::kKenSilvermanEmulator:
            core->opl = new CKemuopl(kSampleRate, true, isStereo);
            break;
        case Opl::kMameEmulator:
            core->opl = new CEmuopl(kSampleRate, true, isStereo);
            break;
        }
        return core;
    }

    class RawFileProvider : public CFileProvider, public binistream
    {
    public:
        RawFileProvider(io::Stream* stream)
        {
            // check for internal packaged formats
            auto data = stream->Read();
            if (memcmp(data.Items(), "KENS-ADB", 8) == 0)
            {
                auto offset = *data.Items<const uint32_t>(8);
                m_stream = io::StreamMemory::Create(stream->GetName(), data.Items(12), offset - 12, true);
                m_otherProvider = new RawFileProvider(io::StreamMemory::Create("insts.dat", data.Items(offset), data.NumItems() - offset, true));
            }
            else if (memcmp(data.Items(), "ABTR-ADB", 8) == 0)
            {
                auto offset = *data.Items<const uint32_t>(8);
                auto name = stream->GetName();
                m_stream = io::StreamMemory::Create(name, data.Items(12), offset - 12, true);
                auto pos = name.find_last_of('.');
                name[pos + 1] = 'i';
                name[pos + 2] = 'n';
                name[pos + 3] = 's';
                m_otherProvider = new RawFileProvider(io::StreamMemory::Create(name, data.Items(offset), data.NumItems() - offset, true));
            }
            else if (memcmp(data.Items(), "SIRA-ADB", 8) == 0)
            {
                auto offset = *data.Items<const uint32_t>(8);
                m_stream = io::StreamMemory::Create(stream->GetName(), data.Items(12), offset - 12, true);
                m_otherProvider = new RawFileProvider(io::StreamMemory::Create("patch.003", data.Items(offset), data.NumItems() - offset, true));
            }
            else if (memcmp(data.Items(), "VICO-ADB", 8) == 0)
            {
                auto offset = *data.Items<const uint32_t>(8);
                m_stream = io::StreamMemory::Create(stream->GetName(), data.Items(12), offset - 12, true);
                m_otherProvider = new RawFileProvider(io::StreamMemory::Create("standard.bnk", data.Items(offset), data.NumItems() - offset, true));
            }
            else
            {
                stream->Seek(0, io::Stream::kSeekBegin);
                m_stream = stream;
            }
        }

        ~RawFileProvider()
        {
            if (m_otherProvider)
                delete m_otherProvider;
        }

        binistream* open(std::string filename) const override
        {
            if (filename != m_stream->GetName())
            {
                if (!m_otherProvider)
                {
                    auto stream = io::StreamFile::Create(filename);
                    if (stream)
                        m_otherProvider = new RawFileProvider(stream);
                }
                return static_cast<binistream*>(const_cast<RawFileProvider*>(m_otherProvider));
            }
            return static_cast<binistream*>(const_cast<RawFileProvider*>(this));
        }
        void close(binistream* /*stream*/) const override
        {
            m_stream->Seek(0, io::Stream::kSeekBegin);
        }

        void seek(long offset, Offset type) override
        {
            if (m_stream->Seek(offset, type == Set ? io::Stream::kSeekBegin : type == Add ? io::Stream::kSeekCurrent : io::Stream::kSeekEnd) != Status::kOk)
                err |= Fatal;
        }

        long pos()
        {
            return long(m_stream->GetPosition());
        }

        Byte getByte() override
        {
            Byte b;
            if (m_stream->Read(&b, 1) != 1)
                err |= Eof;
            return b;
        };

    private:
        SmartPtr<io::Stream> m_stream;
        mutable RawFileProvider* m_otherProvider = nullptr;
    };

    Replay* ReplayAdLib::Load(io::Stream* stream, CommandBuffer /*metadata*/)
    {
        COPLprops* core;
        if (ms_surround)
        {
            core = new COPLprops;
            core->stereo = true;
            core->use16bit = true;
            core->opl = new CSurroundopl(CreateCore(false), CreateCore(false), true);
        }
        else
            core = CreateCore(true);

        RawFileProvider file(stream);
        auto player = CAdPlug::factory(stream->GetName(), core->opl, CAdPlug::players, file);
        if (!player)
        {
            delete core->opl;
            delete core;
            return nullptr;
        }

        return new ReplayAdLib(stream->GetName(), core, player);
    }

    bool ReplayAdLib::DisplaySettings()
    {
        bool changed = false;
        {
            const char* const opls[] = { "DOSBox OPL3", "Nuked OPL3", "Ken Silverman", "Tatsuyuki Satoh MAME OPL2"};
            changed |= ImGui::Combo("OPL Emulator", reinterpret_cast<int32_t*>(&ms_opl), opls, _countof(opls));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
        }
        {
            const char* const surround[] = { "Stereo", "Surround" };
            changed |= ImGui::Combo("Output", &ms_surround, surround, _countof(surround));
            if (ImGui::IsItemHovered())
                ImGui::Tooltip("Applied only on reload :(");
        }
        return changed;
    }

    std::string ReplayAdLib::GetFileFilters()
    {
        std::string fileFilter;
        for (auto& desc : CAdPlug::players)
        {
            if (!fileFilter.empty())
                fileFilter += ",";
            fileFilter += desc->filetype;
            fileFilter += " (";
            for (uint32_t i = 0; desc->get_extension(i); i++)
            {
                if (i != 0)
                    fileFilter += ";*";
                else
                    fileFilter += "*";
                fileFilter += desc->get_extension(i);
            }
            fileFilter += "){";
            for (uint32_t i = 0; desc->get_extension(i); i++)
            {
                if (i != 0)
                    fileFilter += ",";
                fileFilter += desc->get_extension(i);
            }
            fileFilter += "}";
        }
        return fileFilter;
    }

    int32_t ReplayAdLib::ms_surround{ 0 };
    ReplayAdLib::Opl ReplayAdLib::ms_opl{ Opl::kDOSBoxOPL3Emulator };

    bool ReplayAdLib::Init(SharedContexts* ctx, Window& window)
    {
        ctx->Init();

        static CAdPlugDatabase db;
        auto dbStream = io::StreamMemory::Create("", adplugdb, sizeof(adplugdb), true);
        RawFileProvider fp(dbStream);
        db.load(fp);
        CAdPlug::set_database(&db);

        window.RegisterSerializedData(ms_opl, "ReplayAdLibOPL");
        window.RegisterSerializedData(ms_surround, "ReplayAdLibSurround");

        std::string extensions;
        for (auto& desc : CAdPlug::players)
        {
            for (uint32_t i = 0; desc->get_extension(i); i++)
            {
                if (!extensions.empty())
                    extensions += ';';
                extensions += desc->get_extension(i) + 1;
            }
        }
        auto ext = new char[extensions.size() + 1];
        memcpy(ext, extensions.c_str(), extensions.size() + 1);
        g_replayPlugin.extensions = ext;

        auto settingsLabel = std::string("AdPlug ") + CAdPlug::get_version();
        auto settings = new char[settingsLabel.size() + 1];
        memcpy(settings, settingsLabel.c_str(), settingsLabel.size() + 1);
        g_replayPlugin.settings = settings;

        return false;
    }

    void ReplayAdLib::Release()
    {
        delete[] g_replayPlugin.extensions;
        delete[] g_replayPlugin.settings;
    }

    ReplayAdLib::~ReplayAdLib()
    {
        delete m_player;
        delete m_core->opl;
        delete m_core;
    }

    ReplayAdLib::ReplayAdLib(const std::string& filename, COPLprops* core, CPlayer* player)
        : Replay(GetExtension(filename, player), eReplay::AdLib)
        , m_filename(filename)
        , m_core(core)
        , m_player(player)
    {}

    uint32_t ReplayAdLib::Render(StereoSample* output, uint32_t numSamples)
    {
        auto maxSamples = numSamples;

        if (m_hasEnded)
        {
            m_hasEnded = false;
            return 0;
        }

        auto remainingSamples = m_remainingSamples;
        while (numSamples > 0)
        {
            if (remainingSamples > 0)
            {
                auto samplesToAdd = Min(numSamples, remainingSamples);

                auto buf = reinterpret_cast<int16_t*>(output + samplesToAdd) - samplesToAdd * 2;
                m_core->opl->update(buf, samplesToAdd);

                remainingSamples -= samplesToAdd;
                numSamples -= samplesToAdd;

                output = output->Convert(buf, samplesToAdd);
            }
            else if (m_player->update())
            {
                remainingSamples = static_cast<uint32_t>(kSampleRate / m_player->getrefresh());
                m_isStuck = 0;
            }
            else if (m_isStuck == 1)
            {
                m_player->rewind(m_subsongIndex);
                m_isStuck++;
            }
            else
            {
                m_isStuck++;
                m_hasEnded = m_isStuck > 1 || numSamples != maxSamples;
                break;
            }
        }
        m_remainingSamples = remainingSamples;

        return maxSamples - numSamples;
    }

    uint32_t ReplayAdLib::Seek(uint32_t timeInMs)
    {
        m_player->rewind(m_subsongIndex);

        uint32_t pos = 0;
        while (pos < timeInMs)
        {
            m_player->update();
            pos += uint32_t(1000 / m_player->getrefresh());
        }
        return pos;
    }

    void ReplayAdLib::ResetPlayback()
    {
        m_player->rewind(m_subsongIndex);
        m_remainingSamples = 0;
        m_isStuck = 0;
        m_hasEnded = false;
    }

    void ReplayAdLib::ApplySettings(const CommandBuffer /*metadata*/)
    {
    }

    void ReplayAdLib::SetSubsong(uint16_t subsongIndex)
    {
        m_subsongIndex = subsongIndex;
        m_player->rewind(subsongIndex);
    }

    const char* ReplayAdLib::GetExtension(const std::string& filename, CPlayer* player)
    {
        auto extOffset = filename.find_last_of('.');

        const char* ext = nullptr;
        for (int32_t i = 0; player->player_desc->get_extension(i); i++)
        {
            auto playerExt = player->player_desc->get_extension(i);
            if (!ext)
            {
                ext = playerExt;
                if (extOffset == filename.npos)
                    break;
            }
            if (_stricmp(playerExt, filename.c_str() + extOffset) == 0)
            {
                ext = playerExt;
                break;
            }
        }
        return ext + 1;
    }

    uint32_t ReplayAdLib::GetDurationMs() const
    {
        return m_player->songlength(m_subsongIndex);
    }

    uint32_t ReplayAdLib::GetNumSubsongs() const
    {
        return m_player->getsubsongs();
    }

    std::string ReplayAdLib::GetExtraInfo() const
    {
        std::string info;
        if (!m_player->gettitle().empty())
            info += m_player->gettitle();
        if (!m_player->getauthor().empty())
        {
            if (!info.empty())
                info += "\n";
            info += m_player->getauthor();
        }
        if (!m_player->getdesc().empty())
        {
            if (!info.empty())
                info += "\n";
            info += m_player->getdesc();
        }
        for (uint32_t i = 0, e = m_player->getinstruments(); i < e; i++)
        {
            if (!m_player->getinstrument(i).empty())
            {
                if (!info.empty())
                    info += "\n";
                info += m_player->getinstrument(i);
            }
        }
        return info;
    }

    std::string ReplayAdLib::GetInfo() const
    {
        std::string info = "2 Channels\n";
        info += m_player->gettype();
        info += "\nAdPlug ";
        info += CAdPlug::get_version();
        return info;
    }
}
// namespace rePlayer