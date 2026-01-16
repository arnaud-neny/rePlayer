#include "Song.h"

#include <Blob/Blob.inl.h>
#include <Database/Database.h>
#include <Database/Types/Proxy.inl.h>
#include <Imgui/imgui.h>
#include <Replays/Replay.h>

#include <stdlib.h>

namespace rePlayer
{
    BlobSerializer SongSheet::Serialize() const
    {
        BlobSerializer s;
        s.Store(id);
        s.Store(fileSize);
        s.Store(fileCrc);
        s.Store(tags);
        s.Store(lastSubsongIndex);
        s.Store(type);
        s.Store(name);
        s.Store(artistIds);
        s.Store(sourceIds);
        s.Store(metadata);
        s.Store(releaseYear);
        s.Store(databaseDay);
        for (uint16_t i = 0; i <= lastSubsongIndex; i++)
            s.Store(subsongs[i]);
        return s;
    }

    void SongSheet::Load(io::File& file)
    {
        file.Read(id);
        file.Read(fileSize);
        file.Read(fileCrc);
        file.Read(tags);
        file.Read(lastSubsongIndex);
        file.Read(type);
        name.Load(file);
        artistIds.Load(file);
        sourceIds.Load(file);
        metadata.Load(file);
        file.Read(releaseYear);
        file.Read(databaseDay);
        subsongs.Resize(lastSubsongIndex + 1);
        for (uint16_t i = 0; i <= lastSubsongIndex; i++)
            subsongs[i].Load(file);
    }

    void SongSheet::Save(io::File& file) const
    {
        file.Write(id);
        file.Write(fileSize);
        file.Write(fileCrc);
        file.Write(tags);
        file.Write(lastSubsongIndex);
        file.Write(type);
        name.Save(file);
        artistIds.Save(file);
        sourceIds.Save(file);
        metadata.Save(file);
        file.Write(releaseYear);
        file.Write(databaseDay);
        for (uint16_t i = 0; i <= lastSubsongIndex; i++)
            subsongs[i].Save(file);
    }

    void SongSheet::AddRef()
    {
        std::atomic_ref(refCount)++;
    }

    void SongSheet::Release()
    {
        if (--std::atomic_ref(refCount) <= 0)
            delete this;
    }

    // instantiate
    template Proxy<Song, SongSheet>;

    const SongID Song::GetId() const
    {
        return id;
    }

    const uint32_t Song::GetFileSize() const
    {
        if (dataSize != 0)
            return fileSize;
        return Dynamic()->fileSize;
    }

    const uint32_t Song::GetFileCrc() const
    {
        if (dataSize != 0)
            return fileCrc;
        return Dynamic()->fileCrc;
    }

    const Tag Song::GetTags() const
    {
        if (dataSize != 0)
            return tags;
        return Dynamic()->tags;
    }

    const uint16_t Song::GetReleaseYear() const
    {
        if (dataSize != 0)
            return releaseYear;
        return Dynamic()->releaseYear;
    }

    const uint16_t Song::GetDatabaseDay() const
    {
        if (dataSize != 0)
            return databaseDay;
        return Dynamic()->databaseDay;
    }

    const MediaType Song::GetType() const
    {
        if (dataSize != 0)
            return type;
        return Dynamic()->type;
    }

    const bool Song::IsInvalid() const
    {
        if (dataSize != 0)
            return subsongs[0].isInvalid;
        return Dynamic()->subsongs[0].isInvalid;
    }

    const bool Song::IsPackage() const
    {
        if (dataSize != 0)
            return subsongs[0].isPackage;
        return Dynamic()->subsongs[0].isPackage;
    }

    const bool Song::IsUnavailable() const
    {
        if (dataSize != 0)
            return subsongs[0].isUnavailable;
        return Dynamic()->subsongs[0].isUnavailable;
    }

    const bool Song::IsArchive() const
    {
        if (dataSize != 0)
            return subsongs[0].isArchive;
        return Dynamic()->subsongs[0].isArchive;
    }

    const char* Song::GetName() const
    {
        if (dataSize != 0)
            return name.Items();
        return Dynamic()->name.Items();
    }

    const Span<ArtistID> Song::ArtistIds() const
    {
        if (dataSize != 0)
            return artistIds;
        auto song = Dynamic();
        return song->artistIds;
    }

    const uint16_t Song::NumArtistIds() const
    {
        if (dataSize != 0)
            return artistIds.NumItems<uint16_t>();
        return Dynamic()->artistIds.NumItems<uint16_t>();
    }

    const ArtistID Song::GetArtistId(uint32_t index) const
    {
        if (dataSize != 0)
            return artistIds[index];
        return Dynamic()->artistIds[index];
    }

    const Span<SourceID> Song::SourceIds() const
    {
        if (dataSize != 0)
            return sourceIds;
        auto song = Dynamic();
        return song->sourceIds;
    }

    const uint16_t Song::NumSourceIds() const
    {
        if (dataSize != 0)
            return sourceIds.NumItems<uint16_t>();
        return Dynamic()->sourceIds.NumItems<uint16_t>();
    }

    const SourceID Song::GetSourceId(uint32_t index) const
    {
        if (dataSize != 0)
            return sourceIds[index];
        return Dynamic()->sourceIds[index];
    }

    const char* const Song::GetSourceName(uint32_t index) const
    {
        SourceID sourceId;
        if (dataSize != 0)
        {
            if (sourceIds.NumItems() == 0)
                return "";
            sourceId = sourceIds[index];
        }
        else
        {
            auto song = Dynamic();
            if (song->sourceIds.NumItems() == 0)
                return "";
            sourceId = song->sourceIds[index];
        }
        return SourceID::sourceNames[sourceId.sourceId];
    }

    const uint16_t Song::GetLastSubsongIndex() const
    {
        if (dataSize != 0)
            return lastSubsongIndex;
        return Dynamic()->lastSubsongIndex;
    }

    const uint32_t Song::GetSubsongDurationCs(uint32_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].durationCs;
        return Dynamic()->subsongs[index].durationCs;
    }

    const uint8_t Song::GetSubsongRating(uint32_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].rating;
        return Dynamic()->subsongs[index].rating;
    }

    const SubsongState Song::GetSubsongState(uint32_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].state;
        return Dynamic()->subsongs[index].state;
    }

    const bool Song::IsSubsongDiscarded(uint32_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].isDiscarded;
        return Dynamic()->subsongs[index].isDiscarded;
    }

    const bool Song::IsSubsongPlayed(uint32_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].isPlayed;
        return Dynamic()->subsongs[index].isPlayed;
    }

    const char* Song::GetSubsongName(uint32_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].name.Items();
        return Dynamic()->subsongs[index].name.Items();
    }

    const Span<CommandBuffer::Command> Song::Metadatas() const
    {
        if (dataSize != 0)
            return metadata;
        return Dynamic()->metadata;
    }

    void Song::CopyTo(SongSheet* song) const
    {
        if (dataSize != 0)
        {
            song->id = id;
            song->fileSize = fileSize;
            song->fileCrc = fileCrc;
            song->tags = tags;
            song->releaseYear = releaseYear;
            song->databaseDay = databaseDay;
            song->type = type;
            song->name = name;
            song->artistIds.Clear();
            song->artistIds.Add(artistIds.Items(), artistIds.NumItems());
            song->sourceIds.Clear();
            song->sourceIds.Add(sourceIds.Items(), sourceIds.NumItems());
            song->metadata.Clear();
            song->metadata.Add(metadata.Items(), metadata.NumItems());
            CopySubsongsTo(song);
        }
        else
            *song = *Dynamic();
    }

    void Song::CopySubsongsTo(SongSheet* song) const
    {
        if (dataSize == 0)
        {
            auto dynamic = Dynamic();
            song->subsongs = dynamic->subsongs;
            song->lastSubsongIndex = dynamic->lastSubsongIndex;
        }
        else
        {
            song->subsongs.Resize(lastSubsongIndex + 1);
            song->lastSubsongIndex = lastSubsongIndex;
            for (uint16_t i = 0; i <= song->lastSubsongIndex; i++)
                song->subsongs[i] = subsongs[i];
        }
    }

    void Song::Patch(uint32_t version)
    {
        // version 1: updated type storage
        // version 2: converted internal packages to archive
        if (version == 0)
        {
            struct MediaType0
            {
                eExtension ext : 9{ eExtension::Unknown };
                eReplay replay : 7{ eReplay::Unknown };
            };
            type = MediaType(reinterpret_cast<MediaType0&>(type).ext, reinterpret_cast<MediaType0&>(type).replay);
        }

        // duration is deprecated and changed to loop info
        static constexpr uint32_t kVersionLoop = (0 << 28) | (21 << 14) | 4;
        if (version <= kVersionLoop && metadata.IsNotEmpty())
        {
            auto find = [](Span<CommandBuffer::Command> commands, eReplay replay)
            {
                for (uint32_t i = 0; i < commands.NumItems(); i += 1 + commands[i].numEntries)
                {
                    if (commands[i].commandId == uint16_t(replay))
                        return &commands[i];
                }
                return (CommandBuffer::Command*)nullptr;
            };

            switch (type.replay)
            {
            case eReplay::Ayumi:
            case eReplay::Quartet:
                if (auto* command = find(Metadatas(), type.replay))
                    Edit()->Patch002104Replays(command, 1, type.replay);
                break;
            case eReplay::GBSPlay:
            case eReplay::GME:
            case eReplay::HighlyAdvanced:
            case eReplay::HighlyCompetitive:
            case eReplay::HighlyExperimental:
            case eReplay::HighlyTheoretical:
            case eReplay::KSS:
            case eReplay::LazyUSF:
            case eReplay::NEZplug:
            case eReplay::SNDHPlayer:
            case eReplay::vio2sf:
            case eReplay::ZXTune:
                if (auto* command = find(Metadatas(), type.replay))
                    Edit()->Patch002104Replays(command, 1, type.replay);
                break;
            case eReplay::SidPlay:
                if (auto* command = find(Metadatas(), type.replay))
                    Edit()->Patch002104Replays(command, 2, type.replay);
                break;
            case eReplay::WonderSwan:
                if (auto* command = find(Metadatas(), type.replay))
                    Edit()->Patch002104Replays(command, 1 + 8, type.replay);
                break;
            case eReplay::UADE:
                if (auto* command = find(Metadatas(), type.replay))
                    Edit()->Patch002104UADE(command);
                break;
            case eReplay::VGM:
                if (auto* command = find(Metadatas(), type.replay))
                    Edit()->Patch002104VGM(command);
                break;
            }
        }
    }

    void SongSheet::Patch002104Replays(const CommandBuffer::Command* command, uint16_t numBaseEntries, eReplay replay)
    {
        if (metadata.Container().NumItems() != command->numEntries + 1u)
        {
            // bug fix
            ((CommandBuffer::Command*)command)->numEntries = uint16_t(metadata.Container().NumItems() - 1);
            metadata.Container()[0].numEntries = command->numEntries;
        }

        uint16_t numLoops = command->numEntries - numBaseEntries;
        struct Settings : public Replay::Command<Settings, eReplay::Unknown>
        {
            uint32_t data[0];
        };
        auto* newSettings = reinterpret_cast<Settings*>(_alloca((numLoops * 2 + 1 + numBaseEntries) * sizeof(CommandBuffer::Command)));
        newSettings->numEntries = numLoops * 2 + numBaseEntries;
        newSettings->commandId = uint16_t(replay);

        memcpy(newSettings->data, reinterpret_cast<const Settings*>(command)->data, numBaseEntries * sizeof(uint32_t));
        auto* durations = reinterpret_cast<const uint32_t*>(reinterpret_cast<const Settings*>(command)->data + numBaseEntries);
        auto* loops = reinterpret_cast<LoopInfo*>(newSettings->data + numBaseEntries);
        for (uint16_t i = 0; i < numLoops; ++i)
            loops[i] = { 0, durations[i] };
        CommandBuffer(metadata.Container()).Add(newSettings);
    }

    void SongSheet::Patch002104UADE(const CommandBuffer::Command* command)
    {
        struct Settings : public Replay::Command<Settings, eReplay::UADE>
        {
            union
            {
                uint32_t value = 0;
                struct
                {
                    uint32_t _deprecated1 : 8;
                    uint32_t overrideStereoSeparation : 1;
                    uint32_t stereoSeparation : 7;
                    uint32_t overrideSurround : 1;
                    uint32_t surround : 1;
                    uint32_t _deprecated2 : 2;
                    uint32_t overrideNtsc : 1;
                    uint32_t ntsc : 1;
                    uint32_t overrideFilter : 1;
                    uint32_t filter : 2;
                    uint32_t overridePlayer : 1;
                };
            };
            uint32_t data[0];
        };

        auto* oldSettings = reinterpret_cast<const Settings*>(command);
        if (metadata.Container().NumItems() != oldSettings->numEntries + 1u)
        {
            // bug fix
            const_cast<Settings*>(oldSettings)->numEntries = uint16_t(metadata.Container().NumItems() - 1);
            metadata.Container()[0].numEntries = oldSettings->numEntries;
        }
        uint32_t numSubsongs = oldSettings->numEntries - 1 - oldSettings->overridePlayer;
        auto* newSettings = reinterpret_cast<Settings*>(_alloca((numSubsongs * 2 + 2 + oldSettings->overridePlayer) * sizeof(CommandBuffer::Command)));
        newSettings->numEntries = uint16_t(numSubsongs * 2 + 1 + oldSettings->overridePlayer);
        newSettings->commandId = uint16_t(eReplay::UADE);

        newSettings->value = oldSettings->value;
        newSettings->_deprecated1 = newSettings->_deprecated2 = 0;
        bool isZero = true;
        for (uint32_t i = 0; i < numSubsongs; ++i)
        {
            reinterpret_cast<LoopInfo*>(newSettings->data + oldSettings->overridePlayer)[i] = { 0, oldSettings->data[i + oldSettings->overridePlayer] };
            isZero &= oldSettings->data[i + oldSettings->overridePlayer] == 0;
        }
        if (isZero)
            newSettings->numEntries -= uint16_t(numSubsongs * 2);

        CommandBuffer(metadata.Container()).Update(newSettings, newSettings->value == 0 && isZero);
    }

    void SongSheet::Patch002104VGM(const CommandBuffer::Command* command)
    {
        struct Settings : public Replay::Command<Settings, eReplay::VGM>
        {
            uint32_t value = 0;
            LoopInfo loop = {};
        } newSettings;
        newSettings.value = ((Settings*)command)->value;
        if (command->numEntries == 2) // check for older format as duration has been added after 0.6.0
        {
            newSettings.loop.start = 0;
            newSettings.loop.length = ((Settings*)command)->loop.start;
        }
        CommandBuffer(metadata.Container()).Update(&newSettings, newSettings.value == 0 && newSettings.loop.length == 0);
    }
}
// namespace rePlayer