#include "Song.h"

#include <Blob/Blob.inl.h>
#include <Database/Database.h>
#include <Database/Types/Proxy.inl.h>
#include <Imgui/imgui.h>

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

    const ArtistID Song::GetArtistId(size_t index) const
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

    const SourceID Song::GetSourceId(size_t index) const
    {
        if (dataSize != 0)
            return sourceIds[index];
        return Dynamic()->sourceIds[index];
    }

    const char* const Song::GetSourceName(size_t index) const
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

    const uint32_t Song::GetSubsongDurationCs(size_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].durationCs;
        return Dynamic()->subsongs[index].durationCs;
    }

    const uint8_t Song::GetSubsongRating(size_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].rating;
        return Dynamic()->subsongs[index].rating;
    }

    const SubsongState Song::GetSubsongState(size_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].state;
        return Dynamic()->subsongs[index].state;
    }

    const bool Song::IsSubsongDiscarded(size_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].isDiscarded;
        return Dynamic()->subsongs[index].isDiscarded;
    }

    const bool Song::IsSubsongPlayed(size_t index) const
    {
        if (dataSize != 0)
            return subsongs[index].isPlayed;
        return Dynamic()->subsongs[index].isPlayed;
    }

    const char* Song::GetSubsongName(size_t index) const
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
        if (version == 0)
        {
            struct MediaType0
            {
                eExtension ext : 9{ eExtension::Unknown };
                eReplay replay : 7{ eReplay::Unknown };
            };
            type = MediaType(reinterpret_cast<MediaType0&>(type).ext, reinterpret_cast<MediaType0&>(type).replay);
        }
    }
}
// namespace rePlayer