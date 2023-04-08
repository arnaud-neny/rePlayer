#include "Artist.h"

#include <Blob/Blob.inl.h>
#include <Database/Database.h>
#include <Database/Types/Countries.h>
#include <Database/Types/Proxy.inl.h>
#include <Imgui.h>

#include <ctime>

namespace rePlayer
{
    ArtistSource::ArtistSource(SourceID sourceId)
        : id(sourceId)
    {
        std::time(&timeFetch);
    }

    template <Blob::Storage storage>
    void ArtistData<storage>::Tooltip() const
    {
        ImGui::BeginTooltip();
        ImGui::Text("Songs  : %d", numSongs);
        if (!realName.IsEmpty())
            ImGui::Text("Name   : %s", realName.Items());
        if (handles.NumItems() > 1)
        {
            std::string buf = "AKA    : ";
            for (uint16_t i = 1; i < handles.NumItems(); i++)
            {
                if (i > 1)
                {
                    if (i & 1)
                        buf += "\n         ";
                    else
                        buf += "/";
                }
                buf += handles[i].Items();
            }
            ImGui::TextUnformatted(buf.c_str());
        }
        if (!countries.IsEmpty())
        {
            std::string buf = "Country: ";
            for (uint16_t i = 0; i < countries.NumItems(); i++)
            {
                if (i != 0)
                    buf += "/";
                buf += Countries::GetName(countries[i]);
            }
            ImGui::TextUnformatted(buf.c_str());
        }
        if (!groups.IsEmpty())
        {
            std::string buf = "Group  : ";
            for (uint16_t i = 0; i < groups.NumItems(); i++)
            {
                if (i > 0)
                {
                    if (i & 1)
                        buf += "/";
                    else
                        buf += "\n         ";
                }
                buf += groups[i].Items();
            }
            ImGui::TextUnformatted(buf.c_str());
        }
        ImGui::EndTooltip();
    }

    template void ArtistData<Blob::kIsStatic>::Tooltip() const;
    template void ArtistData<Blob::kIsDynamic>::Tooltip() const;

    BlobSerializer ArtistSheet::Serialize() const
    {
        BlobSerializer s;
        s.Store(id);
        s.Store(numSongs);
        s.Store(realName);
        s.Store(handles);
        s.Store(countries);
        s.Store(groups);
        s.Store(sources);
        return s;
    }

    void ArtistSheet::AddRef()
    {
        std::atomic_ref(refCount)++;
    }

    void ArtistSheet::Release()
    {
        if (--std::atomic_ref(refCount) <= 0)
            delete this;
    }

    // instantiate
    template Proxy<Artist, ArtistSheet>;

    const ArtistID Artist::GetId() const
    {
        return id;
    }

    const uint16_t Artist::NumSongs() const
    {
        if (dataSize != 0)
            return numSongs;
        return Dynamic()->numSongs;
    }

    const char* Artist::GetRealName() const
    {
        if (dataSize != 0)
            return realName.Items();
        return Dynamic()->realName.Items();
    }

    const uint16_t Artist::NumHandles() const
    {
        if (dataSize != 0)
            return handles.NumItems<uint16_t>();
        return Dynamic()->handles.NumItems<uint16_t>();
    }

    const char* Artist::GetHandle(size_t index) const
    {
        if (dataSize != 0)
            return handles[index].Items();
        return Dynamic()->handles[index].Items();
    }

    bool Artist::AreSameHandles(ArtistSheet* artistSheet) const
    {
        if (dataSize != 0)
            return handles == artistSheet->handles;
        return Dynamic()->handles == artistSheet->handles;
    }

    const Span<uint16_t> Artist::Countries() const
    {
        if (dataSize != 0)
            return countries;
        return Dynamic()->countries;
    }

    const uint16_t Artist::NumCountries() const
    {
        if (dataSize != 0)
            return countries.NumItems<uint16_t>();
        return Dynamic()->countries.NumItems<uint16_t>();
    }

    const uint16_t Artist::GetCountry(size_t index) const
    {
        if (dataSize != 0)
            return countries[index];
        return Dynamic()->countries[index];
    }

    const uint16_t Artist::NumGroups() const
    {
        if (dataSize != 0)
            return groups.NumItems<uint16_t>();
        return Dynamic()->groups.NumItems<uint16_t>();
    }

    const char* Artist::GetGroup(size_t index) const
    {
        if (dataSize != 0)
            return groups[index].Items();
        return Dynamic()->groups[index].Items();
    }

    bool Artist::AreSameGroups(ArtistSheet* artistSheet) const
    {
        if (dataSize != 0)
            return groups == artistSheet->groups;
        return Dynamic()->groups == artistSheet->groups;
    }

    const Span<ArtistSource> Artist::Sources() const
    {
        if (dataSize != 0)
            return sources;
        return Dynamic()->sources;
    }

    const uint16_t Artist::NumSources() const
    {
        if (dataSize != 0)
            return sources.NumItems<uint16_t>();
        return Dynamic()->sources.NumItems<uint16_t>();
    }

    const ArtistSource& Artist::GetSource(size_t index) const
    {
        if (dataSize != 0)
            return sources[index];
        return Dynamic()->sources[index];
    }

    void Artist::Tooltip() const
    {
        if (dataSize != 0)
            ArtistData<Blob::kIsStatic>::Tooltip();
        else
            Dynamic()->Tooltip();
    }

    void Artist::CopyTo(ArtistSheet* artist) const
    {
        if (dataSize != 0)
        {
            artist->id = id;
            artist->numSongs = numSongs;

            artist->realName = realName;
            artist->handles.Clear();
            for (auto& handle : handles)
                artist->handles.Add(handle.Items());
            artist->countries.Clear();
            artist->countries.Add(countries.Items(), countries.NumItems());
            artist->groups.Clear();
            for (auto& group : groups)
                artist->groups.Add(group.Items());
            artist->sources.Clear();
            artist->sources.Add(sources.Items(), sources.NumItems());
        }
        else
            *artist = *Dynamic();
    }
}
// namespace rePlayer