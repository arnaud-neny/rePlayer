#pragma once

#include "Tags.h"

namespace rePlayer
{
    inline constexpr Tag::Tag(Type tag)
        : m_value(tag)
    {}

    inline constexpr Tag::Tag(uint64_t tag)
        : m_value(Type(tag))
    {}

    inline constexpr Tag& Tag::Set(Tag tag)
    {
        m_value = tag.m_value;
        return *this;
    }

    inline constexpr Tag& Tag::Raise(Tag tag)
    {
        m_value = Type(m_value | tag.m_value);
        return *this;
    }

    inline constexpr Tag& Tag::Lower(Tag tag)
    {
        m_value = Type(m_value & ~tag.m_value);
        return *this;
    }

    inline constexpr Tag& Tag::Enable(Tag tag, bool isEnabled)
    {
        return isEnabled ? Raise(tag) : Lower(tag);
    }

    inline constexpr bool Tag::IsEnabled(Tag tag) const
    {
        return (m_value & tag.m_value) == tag;
    }

    inline constexpr bool Tag::IsAnyEnabled(Tag tag) const
    {
        return (m_value & tag.m_value) != kNone;
    }

    inline constexpr bool Tag::IsEqual(Tag tag, Tag mask) const
    {
        return (m_value & mask.m_value) == (tag.m_value & mask.m_value);
    }

    inline constexpr bool Tag::operator==(Tag other) const
    {
        return m_value == other.m_value;
    }

    inline constexpr bool Tag::operator!=(Tag other) const
    {
        return m_value != other.m_value;
    }

    template <typename T>
    inline constexpr const char* const Tag::Name(T index)
    {
        return ms_names[int32_t(index)];
    }
}
// namespace rePlayer