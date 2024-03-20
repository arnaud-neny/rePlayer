#pragma once

#include <Replay.inl.h>

#include "eupmini/eupplayer.hpp"
#include "eupmini/eupplayer_townsEmulator.hpp"

namespace rePlayer
{
    class ReplayEuphony : public Replay
    {
    public:
        static Replay* Load(io::Stream* stream, CommandBuffer metadata);

    public:
        ~ReplayEuphony() override;

        uint32_t GetSampleRate() const override;
        bool IsSeekable() const override { return true; }

        uint32_t Render(StereoSample* output, uint32_t numSamples) override;
        uint32_t Seek(uint32_t timeInMs) override;

        void ResetPlayback() override;

        void ApplySettings(const CommandBuffer metadata) override;
        void SetSubsong(uint32_t subsongIndex) override;

        uint32_t GetDurationMs() const override;
        uint32_t GetNumSubsongs() const override;
        std::string GetSubsongTitle() const override;
        std::string GetExtraInfo() const override;
        std::string GetInfo() const override;

    private:
        // EUPﾌｧｲﾙﾍｯﾀﾞ構造体
        struct EUPHEAD
        {
            char    title[32]; // オフセット/offset 000h タイトルは半角32文字，全角で16文字以内の文字列で指定します。The title is specified as a character string of 32 half-width characters or less and 16 full-width characters or less.
            char    artist[8]; // オフセット/offset 020h
            char    dummy[44]; // オフセット/offset 028h
            char    trk_name[32][16]; // オフセット/offset 084h 512 = 32 * 16
            char    short_trk_name[32][8]; // オフセット/offset 254h 256 = 32 * 8
            uint8_t trk_mute[32]; // オフセット/offset 354h
            uint8_t trk_port[32]; // オフセット/offset 374h
            uint8_t trk_midi_ch[32]; // オフセット/offset 394h
            uint8_t trk_key_bias[32]; // オフセット/offset 3B4h
            uint8_t trk_transpose[32]; // オフセット/offset 3D4h
            uint8_t trk_play_filter[32][7]; // オフセット/offset 3F4h 224 = 32 * 7
            // ＦＩＬＴＥＲ：形式１ ベロシティフィルター/Format 1 velocity filter ＦＩＬＴＥＲ：形式２ ボリュームフィルター/FILTER: Format 2 volume filter ＦＩＬＴＥＲ：形式３ パンポットフィルター/FILTER: Format 3 panpot filter have 7 typed parameters 1 byte sized each parameter
            // ＦＩＬＴＥＲ：形式４ ピッチベンドフィルター/Format 4 pitch bend filter has 7 typed parameters some parameters are 2 bytes sized
            char    instruments_name[128][4]; // オフセット/offset 4D4h 512 = 128 * 4
            uint8_t fm_midi_ch[6]; // オフセット/offset 6D4h
            uint8_t pcm_midi_ch[8]; // オフセット/offset 6DAh
            char    fm_file_name[8]; // オフセット/offset 6E2h
            char    pcm_file_name[8]; // オフセット/offset 6EAh
            char    reserved[260]; // オフセット/offset 6F2h 260 = (32 * 8) + 4 ??? ＦＩＬＴＥＲ：形式４ ピッチベンドフィルター/Format 4 pitch bend filter additional space ???
            char    appli_name[8]; // オフセット/offset 7F6h
            char    appli_version[2]; // オフセット/offset 7FEh
            int32_t size; // オフセット/offset 800h
            char    signature; // オフセット/offset 804h
            uint8_t first_tempo; // オフセット/offset 805h
        };

    private:
        ReplayEuphony(io::Stream* stream, Array<uint32_t>&& subsongs);

        EUPPlayer* Load(int32_t subsongIndex);

    private:
        SmartPtr<io::Stream> m_stream;
        EUPPlayer* m_player;
        uint64_t m_currentPosition = 0;

        Array<uint8_t> m_data;
        Array<uint32_t> m_subsongs;
        std::string m_title;
    };
}
// namespace rePlayer