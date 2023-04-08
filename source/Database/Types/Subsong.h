#pragma once

#include <Blob/BlobString.h>
#include <Core.h>

#include <string>

namespace rePlayer
{
    using namespace core;

    enum class SubsongState : uint8_t
    {
        Undefined = 0,  //never set or never built (on first complete playback)
        Standard,       //play the subsong once
        Fadeout,        //play the subsong with a fadeout on first repeat
        Loop            //play the subsong, repeat, and fadeout on the second repeat (cool for short subsongs like loaders)
    };
    static constexpr uint32_t kNumSubsongStates = 4;

    template <Blob::Storage storage = Blob::kIsStatic>
    struct SubsongData
    {
        uint32_t durationCs = 0; // seconds * 100
        union
        {
            uint16_t initializer = 1 << 11; // isDirty
            struct
            {
                uint8_t isDiscarded : 1;    // subsong is not visible
                uint8_t rating : 7;         // percentage 1-101, with 0 as undefined
                SubsongState state : 2;     // playback mode
                uint8_t isPlayed : 1;       // subsong has been fully played (no seek) at least once
                uint8_t isDirty : 1;        // force rebuild of subsongs
                uint8_t isInvalid : 1;      // can't play (no known replay has been found)
                uint8_t deprecated : 1;     //
                uint8_t isUnavailable : 1;  // can't load (download failed)
            };
        };
        BlobString<storage> name;

        SubsongData() = default;
        SubsongData(const SubsongData&) requires (storage == Blob::kIsStatic) = delete;
        SubsongData(const SubsongData<Blob::kIsStatic>& otherSubsongData) requires (storage == Blob::kIsDynamic);
        SubsongData(const SubsongData<Blob::kIsDynamic>& otherSubsongData) requires (storage == Blob::kIsDynamic);

        SubsongData& operator=(const SubsongData&) requires (storage == Blob::kIsStatic) = delete;
        SubsongData& operator=(const SubsongData<Blob::kIsStatic>& otherSubsongData) requires (storage == Blob::kIsDynamic);
        SubsongData& operator=(const SubsongData<Blob::kIsDynamic>& otherSubsongData) requires (storage == Blob::kIsDynamic);
        void Clear();
        void Store(BlobSerializer& s) const requires (storage == Blob::kIsDynamic);
    };

    template <Blob::Storage storage>
    inline SubsongData<storage>::SubsongData(const SubsongData<Blob::kIsStatic>& otherSubsongData) requires (storage == Blob::kIsDynamic)
        : durationCs(otherSubsongData.durationCs)
        , initializer(otherSubsongData.initializer)
        , name(otherSubsongData.name)
    {}

    template <Blob::Storage storage>
    inline SubsongData<storage>::SubsongData(const SubsongData<Blob::kIsDynamic>& otherSubsongData) requires (storage == Blob::kIsDynamic)
        : durationCs(otherSubsongData.durationCs)
        , initializer(otherSubsongData.initializer)
        , name(otherSubsongData.name)
    {}

    template <Blob::Storage storage>
    inline SubsongData<storage>& SubsongData<storage>::operator=(const SubsongData<Blob::kIsStatic>& otherSubsongData) requires (storage == Blob::kIsDynamic)
    {
        durationCs = otherSubsongData.durationCs;
        initializer = otherSubsongData.initializer;
        name = otherSubsongData.name;
        return *this;
    }

    template <Blob::Storage storage>
    inline SubsongData<storage>& SubsongData<storage>::operator=(const SubsongData<Blob::kIsDynamic>& otherSubsongData) requires (storage == Blob::kIsDynamic)
    {
        durationCs = otherSubsongData.durationCs;
        initializer = otherSubsongData.initializer;
        name = otherSubsongData.name;
        return *this;
    }

    template <Blob::Storage storage>
    inline void SubsongData<storage>::Clear()
    {
        *this = SubsongData();
    }

    template <Blob::Storage storage>
    void SubsongData<storage>::Store(BlobSerializer& s) const requires (storage == Blob::kIsDynamic)
    {
        s.Store(durationCs);
        s.Store(initializer);
        s.Store(name);
    }

    typedef SubsongData<Blob::kIsDynamic> SubsongSheet;
}
// namespace rePlayer