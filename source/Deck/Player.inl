#pragma once

#include "Player.h"

namespace rePlayer
{
    inline void Player::MarkSongAsNew(bool isNewSong)
    {
        m_isNewSong = isNewSong;
    }

    inline bool Player::IsNewSong() const
    {
        return m_isNewSong;
    }

    inline void Player::ApplySettings()
    {
        m_replay->ApplySettings(m_song->metadata.Container());
    }

    inline bool Player::IsStopped() const
    {
        return m_status == Status::Stopped;
    }

    inline bool Player::IsPlaying() const
    {
        return m_status == Status::Playing;
    }

    inline bool Player::IsSeekable() const
    {
        return m_replay->IsSeekable();
    }

    inline MusicID Player::GetId() const
    {
        return m_id;
    }

    inline SongSheet* Player::GetSong() const
    {
        return m_song;
    }

    inline const SubsongSheet& Player::GetSubsong() const
    {
        return m_song->subsongs[m_id.subsongId.index];
    }

    inline MediaType Player::GetMediaType() const
    {
        return m_replay->GetMediaType();
    }
}
// namespace rePlayer