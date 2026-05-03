// Core
#include <Core/Log.h>
#include <Core/String.h>
#include <Imgui.h>
#include <Imgui/imgui_internal.h>
#include <Thread/Thread.h>

// rePlayer
#include <Deck/Deck.h>
#include <Graphics/Graphics.h>
#include <RePlayer/Core.h>
#include <RePlayer/ReplayGainScanner.h>
#include <Replays/Replay.inl.h>

#include "Player.h"

// TagLib
#include <fileref.h>
#include <tag.h>
#include <toolkit/tpropertymap.h>
#include "TagLibStream.h"

// Windows
#include <windows.h>
#include <mmeapi.h>
#include <Mmreg.h>
#pragma comment(lib, "winmm.lib")

// stl
#include <atomic>
#include <bit>
#include <chrono>

namespace rePlayer
{
    bool Player::ms_isReplayGainEnabled = true;
    bool Player::ms_isReplayGainChecked = false;

    struct Player::Wave
    {
        HWAVEOUT outHandle{ nullptr };
        WAVEHDR header{};
    };

    uint32_t Player::ms_crossFadeLengthInMs = 8000;

    SmartPtr<Player> Player::Create(MusicID id, SongSheet* song, Replay* replay, io::Stream* stream, bool isExport)
    {
        if (replay)
        {
            SmartPtr<Player> player(kAllocate, id, song, replay);
            if (player->Init(stream, isExport))
                return nullptr;
            return player;
        }
        return nullptr;
    }

    void Player::Play(CrossFadeState crossFadeState)
    {
        if (m_replay->IsStreaming() && (!m_song->subsongs[m_id.subsongId.index].isPlayed || m_song->subsongs[m_id.subsongId.index].durationCs != 0))
        {
            m_song->subsongs[m_id.subsongId.index].isPlayed = true;
            m_song->subsongs[m_id.subsongId.index].durationCs = 0;
            m_id.MarkForSave();
        }
        if (m_status == Status::Paused)
        {
            SetupCrossFade(crossFadeState);

            m_numRetries = kNumRetries;
            m_status = Status::Playing;
            waveOutRestart(m_wave->outHandle);
            ResumeThread();
            thread::KeepAwake(true);
        }
        else if (IsStopped())
        {
            m_status = Status::Playing;
            m_songSeek = 0;
            m_songPos = 0;
            m_songEnd = ~0ull;
            m_wavePlayPos = 0;
            m_waveFillPos = m_numSamples;
            m_patternsPos = 0;
            m_numRetries = kNumRetries;
            m_crossFadePosition = 0;

            m_numLoops = m_replay->CanLoop() ? Core::GetDeck().IsEndless() ? INT_MAX : GetSubsong().state == SubsongState::Loop ? 1 : 0 : 0;
            m_hasSeeked = m_replay->CanLoop() && Core::GetDeck().IsEndless();
            m_remainingFadeOut = m_replay->GetSampleRate() * 4;
            m_fadeOutSilence = 0;

            SetupCrossFade(crossFadeState);

            m_replay->ResetPlayback();
            m_replay->ApplySettings(m_song->metadata.Container());

            FillCache();

            waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
            ResumeThread();
            thread::KeepAwake(true);
        }
    }

    void Player::Pause()
    {
        if (m_status == Status::Playing)
        {
            waveOutPause(m_wave->outHandle);
            SuspendThread();
            m_status = Status::Paused;
            thread::KeepAwake(false);
        }
    }

    void Player::Stop()
    {
        if (!IsStopped())
        {
            waveOutPause(m_wave->outHandle);
            if (m_status == Status::Playing)
            {
                SuspendThread();
                thread::KeepAwake(false);
            }
            m_status = Status::Stopped;
            waveOutReset(m_wave->outHandle);
            m_songSeek = 0;
        }
    }

    void Player::Seek(uint32_t timeInMs)
    {
        if (!IsStopped() && m_replay->IsSeekable())
        {
            m_hasSeeked = true;

            waveOutPause(m_wave->outHandle);
            if (m_status == Status::Playing)
                SuspendThread();
            waveOutReset(m_wave->outHandle);

            m_songPos = 0;
            m_songEnd = ~0ull;
            m_wavePlayPos = 0;
            m_waveFillPos = m_numSamples;
            m_patternsPos = 0;

            m_numLoops = m_replay->CanLoop() ? Core::GetDeck().IsEndless() ? INT_MAX : GetSubsong().state == SubsongState::Loop ? 1 : 0 : 0;
            m_remainingFadeOut = m_replay->GetSampleRate() * 4;
            m_fadeOutSilence = 0;

            timeInMs = m_replay->Seek(timeInMs);

            int64_t seekPos = m_replay->GetSampleRate();
            seekPos *= timeInMs;
            m_songSeek = uint32_t(seekPos / 1000);
            m_crossFadePosition = uint32_t(m_songSeek);

            m_crossFadeInLength = 0;

            Render(m_numSamples, 0);

            if (m_status == Status::Paused)
                waveOutPause(m_wave->outHandle);
            waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
            if (m_status == Status::Playing)
                ResumeThread();
        }
    }

    void Player::SetSubsong(uint16_t subsongIndex)
    {
        Stop();
        m_id.subsongId.index = subsongIndex;
        m_replay->SetSubsong(subsongIndex);
        Play(CrossFadeState::kNone);
    }

    Player::EndingState Player::IsEnding() const
    {
        if (m_status == Status::Playing)
        {
            auto songEnd = m_songEnd;

            auto playingPos = GetPlayingPosition();
            if (playingPos >= songEnd)
                return EndingState::kEnded;
            if (m_crossFadeOutLength)
                songEnd = m_crossFadeOut - m_songSeek;
            static constexpr uint64_t kTimeRangeInMs = 75;
            playingPos += (kTimeRangeInMs * m_replay->GetSampleRate()) / 1000;
            if (playingPos >= songEnd)
                return EndingState::kEnding;
        }
        return EndingState::kNotEnding;
    }

    uint32_t Player::GetPlaybackTimeInMs() const
    {
        if (m_wave->outHandle)
        {
            auto playingPos = GetPlayingPosition();
            if (playingPos < m_songEnd)
                return uint32_t((1000ull * (playingPos + m_songSeek)) / m_replay->GetSampleRate());
            return uint32_t((1000ull * (m_songEnd + m_songSeek)) / m_replay->GetSampleRate());
        }
        return 0;
    }

    std::string Player::GetTitle() const
    {
        auto streamingTitle = m_replay->GetStreamingTitle();
        auto defaultTitle = m_id.GetTitle();
        if (streamingTitle.empty())
            return defaultTitle;
        if (defaultTitle.empty())
            return streamingTitle;
        return defaultTitle + " / " + streamingTitle;
    }

    std::string Player::GetArtists() const
    {
        auto streamingArtist = m_replay->GetStreamingArtist();
        auto defaultArtists = m_id.GetArtists();
        if (streamingArtist.empty())
            return defaultArtists;
        if (defaultArtists.empty())
            return streamingArtist;
        return defaultArtists + " / " + streamingArtist;
    }

    std::string Player::GetMetadata() const
    {
        auto info = m_replay->GetExtraInfo();
        if (info.empty())
            info = m_extraInfo;
        auto streamingTitle = m_replay->GetStreamingTitle();
        if (!streamingTitle.empty())
        {
            if (!info.empty())
                info += "\n";
            info += "Streaming Title : ";
            info += streamingTitle;
        }
        auto streamingArtist = m_replay->GetStreamingArtist();
        if (!streamingArtist.empty())
        {
            if (!info.empty())
                info += "\n";
            info += "Streaming Artist: ";
            info += streamingArtist;
        }
        return info;
    }

    std::string Player::GetInfo() const
    {
        return m_replay->GetInfo();
    }

    Replay::Patterns Player::GetPatterns(uint32_t numLines, uint32_t charWidth, uint32_t spaceWidth, Replay::Patterns::Flags flags) const
    {
        Replay::Patterns patterns;
        if (m_status != Status::Stopped)
        {
            auto playingPos = GetPlayingPosition();
            patterns = m_replay->UpdatePatterns(playingPos - m_patternsPos, numLines, charWidth, spaceWidth, flags);
            m_patternsPos = playingPos;
        }
        return patterns;
    }

    StereoSample Player::GetVuMeter() const
    {
        if (m_status == Status::Stopped)
            return { 0.0f, 0.0f };

        uint32_t numVuMeterSamples = 2 * m_replay->GetSampleRate() / 60;

        auto wavePlayPos = GetPlayingPosition();
        wavePlayPos += m_replay->GetSampleRate() / 15; // display a little bit in the future
        wavePlayPos -= numVuMeterSamples / 2;

        StereoSample sum{ 0.0f, 0.0f };
        auto numSamplesMask = m_numSamples - 1;
        auto waveData = m_waveData;

        //Basic fake Vu Meter (todo, fft power)
        for (uint32_t i = 0; i < numVuMeterSamples; i++)
        {
            auto pos = (wavePlayPos + i) & numSamplesMask;
            auto v1 = waveData[pos].left;
            auto v2 = waveData[pos].right;
            sum.left += v1 * v1;
            sum.right += v2 * v2;
        }
        auto rg = m_song->subsongs[m_id.subsongId.index].replayGain;
        float scale = 1.0f;
        if (ms_isReplayGainEnabled && rg.IsValid())
            scale = powf(1.0f / (powf(10.0f, rg.gain / 20.0f) * rg.peak), 2);
        else if (rg.IsValid())
            scale = powf(1.0f / rg.peak, 2);
        scale /= numVuMeterSamples;
        return { sqrtf(sum.left * scale), sqrtf(sum.right * scale) };
    }

    void Player::DrawVisuals(float xMin, float yMin, float xMax, float yMax) const
    {
        DrawOscilloscope(xMin, yMin, xMax, yMax);
        DrawPatterns(xMin, yMin, xMax, yMax);
    }

    void Player::DrawOscilloscope(float xMin, float yMin, float xMax, float yMax) const
    {
        if (m_status != Status::Stopped)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            auto color = 0x98D9B27A; // ImGui::GetColorU32(ImGuiCol_FrameBgActive);

            auto wavePlayPos = GetPlayingPosition();
            wavePlayPos += m_replay->GetSampleRate() / 15; // display a little bit in the future

            uint32_t numOscilloscopeSamples = m_replay->GetSampleRate() / 100; // 10ms oscilloscope

            auto numSamplesMask = m_numSamples - 1;
            auto waveData = m_waveData;

            drawList->PushClipRect({ xMin, yMin }, { xMax, yMax });

            auto rg = m_song->subsongs[m_id.subsongId.index].replayGain;
            float scale = 1.0f;
            if (ms_isReplayGainEnabled && rg.IsValid())
                scale = powf(1.0f / (powf(10.0f, rg.gain / 20.0f) * rg.peak), 2);
            else if (rg.IsValid())
                scale = powf(1.0f / rg.peak, 2);

            auto yCenter = (yMin + yMax) * 0.5f;
            auto yScale = (yMax - yMin) * 0.5f * scale;

            for (uint32_t i = 0; i < numOscilloscopeSamples; i++)
            {
                float x = xMin + i * (xMax - xMin) / (numOscilloscopeSamples - 1.0f);

                auto pos = (wavePlayPos + i) & numSamplesMask;

                auto p = ImVec2(x, yCenter + yScale * waveData[pos].left);
                drawList->PathLineTo(p);
            }
            drawList->PathStroke(color, 0, 1);
            for (uint32_t i = 0; i < numOscilloscopeSamples; i++)
            {
                float x = xMin + i * (xMax - xMin) / (numOscilloscopeSamples - 1.0f);

                auto pos = (wavePlayPos + i) & numSamplesMask;

                auto p = ImVec2(x, yCenter + yScale * waveData[pos].right);
                drawList->PathLineTo(p);
            }
            drawList->PathStroke(color, 0, 1);

            drawList->PopClipRect();
        }
    }

    void Player::DrawPatterns(float xMin, float yMin, float xMax, float yMax) const
    {
        auto height = int32_t(yMax) - int32_t(yMin);
        auto numHalfLines = (((height - kCharHeight) / 2) + kCharHeight - 1) / kCharHeight;
        auto numLines = 1 + 2 * numHalfLines;

        Replay::Patterns patterns = GetPatterns(numLines, kCharWidth, kCharWidth / 2, Replay::Patterns::kDisableAll);

        if (patterns.lines.IsNotEmpty())
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect({ xMin, yMin }, { xMax, yMax });
            ImGuiIO& io = ImGui::GetIO();
            float y = yMin + (height / 2) - (kCharHeight / 2 - 1) - numHalfLines * kCharHeight;
            uint32_t colors[] = { 0xCCC5685B, 0xCCC77B3A }; // { ImGui::GetColorU32(ImGuiCol_Tab), ImGui::GetColorU32(ImGuiCol_TabHovered) };
            if (patterns.currentLine & 1)
                std::swap(colors[0], colors[1]);
            auto baseRect = Graphics::Get3x5BaseRect();
            auto* c = patterns.lines.Items();
            for (auto& size : patterns.sizes)
            {
                if (size > 0)
                {
                    auto color = (&size - patterns.sizes) == numHalfLines ? 0xffff8080 : colors[0];

                    drawList->PrimReserve(int32_t(6 * size), int32_t(4 * size));
                    auto idxBase = drawList->_VtxCurrentIdx;
                    float x;
                    if (int32_t(xMax) - int32_t(xMin) >= patterns.width)
                        x = xMax - patterns.width;
                    else
                        x = xMin +(int32_t(xMax) - int32_t(xMin) - patterns.width) / 2;
                    for (; *c; ++c)
                    {
                        auto cc = *c;
                        if (cc < 0) // ignore the colors
                            continue;
                        if (cc >= 33 && cc <= 126)
                        {
                            ImFontAtlasRect rect;
                            io.Fonts->GetCustomRect(baseRect + cc - 33, &rect);
                            ImVec2 uv0 = rect.uv0, uv1 = rect.uv1;

                            drawList->PrimWriteVtx({ x, y }, uv0, color);
                            drawList->PrimWriteVtx({ x, y + 5.0f }, { uv0.x, uv1.y }, color);
                            drawList->PrimWriteVtx({ x + 3.0f, y }, { uv1.x, uv0.y }, color);
                            drawList->PrimWriteVtx({ x + 3.0f, y + 5.0f }, uv1, color);

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 0));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 1));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 2));

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 2));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 1));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 3));
                        }
                        else
                        {
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                        }

                        idxBase += 4;
                        x += cc == ' ' ? kCharWidth / 2 : kCharWidth;
                    }
                }
                std::swap(colors[0], colors[1]);
                y += kCharHeight;
                c++;
            }
            drawList->PopClipRect();
        }
    }

    uint32_t Player::GetVolume(bool isLogarithmic)
    {
        DWORD value;
        waveOutGetVolume(nullptr, &value);
        value = Max(value & 0xffFF, value >> 16);
        if (isLogarithmic)
        {
            const auto volume = logf(1000.0f * value / float(0xffFF)) / logf(1000.0f);
            value = DWORD(volume * 0xffFF);
        }
        return value;
    }

    void Player::SetVolume(uint32_t volume, bool isLogarithmic)
    {
        if (isLogarithmic)
        {
            // https://www.dr-lex.be/info-stuff/volumecontrols.html
            auto value = Saturate(exp(logf(1000.0f) * volume / float(0xffFF)) / 1000.0f);
            volume = uint32_t(value * 0xffFF);
        }
        waveOutSetVolume(nullptr, volume | (volume << 16));
    }

    void Player::EnableEndless(bool isEnabled)
    {
        m_numLoops = m_replay->CanLoop() ? isEnabled ? INT_MAX : GetSubsong().state == SubsongState::Loop ? 1 : 0 : 0;
        m_hasSeeked |= m_replay->CanLoop() && isEnabled;
    }

    Player::Player(MusicID id, SongSheet* song, Replay* replay)
        : m_id(id)
        , m_song(song)
        , m_replay(replay)
        , m_wave(new Wave())
        , m_numSamples(1 << (32 - std::countl_zero(replay->GetSampleRate())))
    {}

    Player::~Player()
    {
        Stop();
        std::atomic_ref(m_isWaiting).store(false);
        std::atomic_ref(m_isRunning).store(false);
        m_semaphore.Signal();
        while (!std::atomic_ref(m_isJobDone).load())
            thread::Sleep(0);

        if (m_wave->outHandle)
            waveOutClose(m_wave->outHandle);

        delete m_replay;
        delete m_wave;
        delete[] m_waveData;
    }

    bool Player::Init(io::Stream* stream, bool isExport)
    {
        WAVEFORMATEX waveFormat{};
        waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveFormat.nChannels = 2;
        waveFormat.wBitsPerSample = 32;
        waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
        waveFormat.nSamplesPerSec = m_replay->GetSampleRate();
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        waveFormat.cbSize = 0;

        if (!isExport && waveOutOpen(&m_wave->outHandle, WAVE_MAPPER, &waveFormat, 0, 0, 0) != S_OK)
            return true;

        auto numSamples = m_numSamples;
        m_waveData = new StereoSample[numSamples];

        m_wave->header.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
        m_wave->header.lpData = reinterpret_cast<LPSTR>(m_waveData);
        m_wave->header.dwBufferLength = static_cast<uint32_t>(numSamples * sizeof(StereoSample));
        m_wave->header.dwBytesRecorded = 0;
        m_wave->header.dwUser = 0;
        m_wave->header.dwLoops = 0xffFFffFF;
        if (!isExport)
            waveOutPrepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));

        SongSheet* song = m_song;
        m_replay->SetSubsong(m_id.subsongId.index);
        m_numLoops = m_replay->CanLoop() ? Core::GetDeck().IsEndless() ? INT_MAX : song->subsongs[m_id.subsongId.index].state == SubsongState::Loop ? 1 : 0 : 0;
        m_hasSeeked = m_replay->CanLoop() && Core::GetDeck().IsEndless();
        m_remainingFadeOut = m_replay->GetSampleRate() * 4;
        m_fadeOutRatio = 1.0f / m_remainingFadeOut;
        m_replay->ApplySettings(song->metadata.Container());

        if (!isExport)
        {
            FillCache();
            m_wavePlayPos = 0;
            m_waveFillPos = m_numSamples;
            m_crossFadePosition = 0;

            waveOutPause(m_wave->outHandle);
            waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
            m_status = Status::Paused;

            Core::AddJob([this]()
            {
                auto threadHandle = ::GetCurrentThread();
                auto threadPriority = ::GetThreadPriority(threadHandle);
                ::SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL);// THREAD_PRIORITY_HIGHEST);

                ThreadUpdate();

                ::SetThreadPriority(threadHandle, threadPriority);
            });

            if (!m_replay->IsStreaming())
            {
                Core::AddJob([This = SmartPtr<Player>(this), clonedStream = stream->Clone()]()
                {
                    // read stream tags if there is any
                    TagLibStream tagLibStream(clonedStream);
                    TagLib::FileRef f(&tagLibStream);
                    if (auto tag = f.tag())
                    {
                        std::string extraInfo;
                        if (!tag->artist().isEmpty())
                        {
                            extraInfo = "Artist: ";
                            extraInfo += tag->artist().to8Bit();
                        }
                        if (!tag->title().isEmpty())
                        {
                            if (!extraInfo.empty())
                                extraInfo += "\n";
                            extraInfo += "Title : ";
                            extraInfo += tag->title().to8Bit();
                        }
                        if (!tag->album().isEmpty())
                        {
                            if (!extraInfo.empty())
                                extraInfo += "\n";
                            extraInfo += "Album : ";
                            extraInfo += tag->album().to8Bit();
                        }
                        if (!tag->genre().isEmpty())
                        {
                            if (!extraInfo.empty())
                                extraInfo += "\n";
                            extraInfo += "Genre : ";
                            extraInfo += tag->genre().to8Bit();
                        }
                        if (tag->year())
                        {
                            if (!extraInfo.empty())
                                extraInfo += "\n";
                            extraInfo += "Year  : ";
                            char buf[64];
                            core::sprintf(buf, "%d", tag->year());
                            extraInfo += buf;
                        }

                        if (!tag->comment().isEmpty())
                        {
                            if (!extraInfo.empty())
                                extraInfo += "\n\n";
                            extraInfo += tag->comment().to8Bit();
                        }

                        auto pmap = tag->properties();
                        if (!This->m_song->subsongs[This->m_id.subsongId.index].replayGain.IsValid())
                        {
                            auto itGain = pmap.find("REPLAYGAIN_TRACK_GAIN");
                            auto itPeak = pmap.find("REPLAYGAIN_TRACK_PEAK");
                            if (itGain != pmap.end() && itPeak != pmap.end())
                            {
                                double gain, peak;
                                if (sscanf_s(itGain->second.toString().toCString(), "%lf", &gain) == 1)
                                {
                                    if (sscanf_s(itPeak->second.toString().toCString(), "%lf", &peak) == 1)
                                    {
                                        This->m_song->subsongs[This->m_id.subsongId.index].replayGain.gain = float(gain);
                                        This->m_song->subsongs[This->m_id.subsongId.index].replayGain.peak = float(peak);
                                    }
                                }
                            }
                        }

                        Replay::Properties properties;
                        auto* property = properties.Push();
                        property->label = "Tags";
                        property->numColumns = 2;
                        for (auto it = pmap.begin(); it != pmap.end(); ++it)
                        {
                            property->Add(it->first.toCString(), Replay::Property::kIsNotEditable
                                , it->second.toString().toCString(), Replay::Property::kIsEditable);
                        }
                        if (property->numEntries == 0)
                            properties.Clear();
                        Core::FromJob([This, extraInfo = std::move(extraInfo), properties = std::move(properties)]
                        {
                            This->m_extraInfo = std::move(extraInfo);
                            This->m_properties = std::move(properties);
                        });
                    }

                    auto isValid = This->m_song->subsongs[This->m_id.subsongId.index].replayGain.IsValid();
                    if (ms_isReplayGainChecked || (ms_isReplayGainEnabled && !isValid))
                    {
                        ReplayGainScanner rg;
                        rg.Enqueue(This->m_id);
                        rg.Update();
                        if (This->m_status != Status::Playing && !isValid)
                            This->Scale(This->m_numSamples, 0);
                    }
                });
            }
        }
        else
            m_isJobDone = true;
        return false;
    }

    uint32_t Player::GetPlayingPosition() const
    {
        MMTIME mmt{};
        mmt.wType = TIME_BYTES;
        waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));
        mmt.u.cb /= sizeof(StereoSample);
        return mmt.u.cb;
    }

    void Player::ThreadUpdate()
    {
        const auto numSamples = m_numSamples;
        const auto numSamplesMask = numSamples - 1;
        while (std::atomic_ref(m_isRunning).load())
        {
            auto startTime = std::chrono::high_resolution_clock::now();

            auto wavePlayPos = GetPlayingPosition() & ~1u; // align for SSE (or ~3u for AVX)
            auto waveFillPos = m_waveFillPos;
            if (m_wavePlayPos > wavePlayPos) // loop or something wrong happened; need to detect when waveOut had a failure
            {
                m_songSeek += m_wavePlayPos; // simulate a seek to keep track of current time
                waveFillPos = (waveFillPos & numSamplesMask) | (wavePlayPos & ~numSamplesMask);
                waveFillPos += numSamples;
            }
            m_wavePlayPos = wavePlayPos;

            wavePlayPos = wavePlayPos + numSamples;
            while (waveFillPos < wavePlayPos)
            {
                auto count = Min(numSamples - (waveFillPos & numSamplesMask), wavePlayPos - waveFillPos);
                if (Render(count, waveFillPos & numSamplesMask))
                {
                    if (m_numRetries-- > 0)
                    {
                        Log::Warning("Player: render error, retrying %d/%d...\n", kNumRetries - m_numRetries, kNumRetries);
                        m_replay->ResetPlayback();

                        // reinit the fill/playback position to make stream buffering happy
                        wavePlayPos = GetPlayingPosition() & ~1u; // align for SSE (or ~3u for AVX)
                        m_wavePlayPos = wavePlayPos;
                        waveFillPos = wavePlayPos + numSamples;
                    }
                    else
                    {
                        Log::Warning("Player: render error, aborting...\n");
                        m_songEnd = 0;
                    }
                    break;
                }
                m_numRetries = kNumRetries;
                waveFillPos += count;
                m_crossFadePosition += count;
            }

            m_waveFillPos = waveFillPos;

            auto timeSpent = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
            auto waitTime = std::atomic_ref(m_isWaiting).load() ? INFINITE : (timeSpent < 5) ? uint32_t(5 - timeSpent) : 0;
            std::atomic_ref(m_waitTime).store(waitTime);
            m_semaphore.Wait(waitTime);
            if (waitTime == INFINITE)
                std::atomic_ref(m_waitTime).store(0);
        }
        std::atomic_ref(m_isJobDone).store(true);
    }

    void Player::FillCache()
    {
        // don't render when streaming to allow some buffering without waiting for it
        if (m_replay->IsStreaming())
            memset(m_waveData, 0, sizeof(StereoSample) * m_numSamples);
        else
            Render(m_numSamples, 0);
    }

    bool Player::Render(uint32_t numSamples, uint32_t waveFillPos)
    {
        auto remaining = Generate(numSamples, waveFillPos);
        Scale(numSamples - ((remaining + 1) & ~1u), waveFillPos); // align for SSE (or ~3u for AVX)
        Crossfade(numSamples - remaining, waveFillPos);
        return remaining != 0;
    }

    uint32_t Player::Generate(uint32_t numSamples, uint32_t waveFillPos)
    {
        auto waveData = m_waveData;
        if (m_replay->IsStreaming())
        {
            while (numSamples)
            {
                auto count = m_replay->Render(waveData + waveFillPos, numSamples);
                if (count == 0)
                    break;
                numSamples -= count;
                waveFillPos += count;
                m_songPos += count;
            }
            return numSamples;
        }

        auto subsongState = m_replay->CanLoop() ? GetSubsong().state : SubsongState::Standard;
        uint32_t previousCount = 0xffFFffFF;
        while (numSamples)
        {
            if (m_numLoops < 0 && subsongState == SubsongState::Standard)
            {
                //fix duration
                auto duration = static_cast<uint32_t>((m_songPos * 100ull) / m_replay->GetSampleRate());
                bool isDirty = false;
                auto& subsong = m_song->subsongs[m_id.subsongId.index];
                if (!m_hasSeeked && duration != subsong.durationCs)
                {
                    subsong.durationCs = duration;
                    isDirty = true;
                }
                if (!m_hasSeeked && !subsong.isPlayed)
                {
                    subsong.isPlayed = true;
                    isDirty = true;
                }
                if (subsong.state != subsongState)
                {
                    subsong.state = subsongState;
                    isDirty = true;
                }
                if (isDirty)
                    m_id.MarkForSave();

                m_songEnd = m_songPos;
                memset(waveData + waveFillPos, 0, numSamples * sizeof(StereoSample));
                return 0;
            }

            auto count = m_replay->Render(waveData + waveFillPos, numSamples);
            if (count == 0)
            {
                if (m_numLoops >= 0)
                {
                    m_numLoops--;

                    if (subsongState == SubsongState::Undefined)
                    {
                        subsongState = SubsongState::Standard;

                        //detect the loop (last 0.125s skipping the last 2 frames at 50Hz)
                        uint32_t mask = m_numSamples - 1;
                        for (uint32_t i = Min(m_replay->GetSampleRate() / 25, m_numSamples), e = Min(m_replay->GetSampleRate() / 8, m_numSamples); i < e; i++)
                        {
                            auto pos = (waveFillPos - i - 1) & mask;

                            if (fabsf(waveData[pos].left) > 0.01f || fabsf(waveData[pos].right) > 0.01f)
                            {
                                subsongState = SubsongState::Fadeout;
                                break;
                            }
                        }

                        //send message to the database to update the song
                        m_song->subsongs[m_id.subsongId.index].state = subsongState;
                        m_id.MarkForSave();
                    }
                }
                else if (previousCount == 0)//prevent a bug when the player doesn't generate any more samples
                {
                    subsongState = SubsongState::Standard;
                }
            }
            else
            {
                if (m_numLoops < 0)
                {
                    uint32_t fadeOutSize = Min(count, m_remainingFadeOut);
                    float fadeOutRatio = m_fadeOutRatio;
                    float fadeOut = m_remainingFadeOut * m_fadeOutRatio;
                    uint32_t silenceRate = m_replay->GetSampleRate() / 2;
                    auto fadeOutSilence = m_fadeOutSilence;
                    auto isCrossFadeEnabled = m_crossFadeOutLength |= 0;
                    for (uint32_t i = 0; i < fadeOutSize; i++)
                    {
                        auto left = waveData[waveFillPos].left * fadeOut;
                        auto right = waveData[waveFillPos].right * fadeOut;

                        if (!isCrossFadeEnabled)
                        {
                            waveData[waveFillPos].left = left;
                            waveData[waveFillPos].right = right;
                        }

                        waveFillPos++;
                        fadeOut += fadeOutRatio;

                        static constexpr float kEpsilon = 1.0f / 32767.0f;
                        if (fabsf(left) < kEpsilon && fabsf(right) < kEpsilon)
                        {
                            // to help reduce long silences
                            fadeOutSilence++;
                            if (fadeOutSilence >= silenceRate)
                            {
                                fadeOutSize = i + 1;
                                m_remainingFadeOut = fadeOutSize;
                                break;

                            }
                        }
                        else
                            fadeOutSilence = 0;
                    }
                    m_fadeOutSilence = fadeOutSilence;

                    if (fadeOutSize < count)
                    {
                        //fix duration
                        auto duration = static_cast<uint32_t>(((m_songPos + fadeOutSize) * 100ull) / m_replay->GetSampleRate());
                        bool isDirty = false;
                        auto& subsong = m_song->subsongs[m_id.subsongId.index];
                        if (!m_hasSeeked && duration != subsong.durationCs)
                        {
                            subsong.durationCs = duration;
                            isDirty = true;
                        }
                        if (!m_hasSeeked && !subsong.isPlayed)
                        {
                            subsong.isPlayed = true;
                            isDirty = true;
                        }
                        if (isDirty)
                            m_id.MarkForSave();

                        m_songEnd = m_songPos;
                        if (!isCrossFadeEnabled)
                            memset(waveData + waveFillPos, 0, (count - fadeOutSize) * sizeof(StereoSample));
                        waveFillPos += count - fadeOutSize;
                    }

                    m_remainingFadeOut -= fadeOutSize;
                    numSamples -= count;
                    m_songPos += fadeOutSize;
                }
                else
                {
                    numSamples -= count;
                    waveFillPos += count;
                    m_songPos += count;
                }
            }
            previousCount = count;
        }
        return 0;
    }

    void Player::Scale(uint32_t numSamples, uint32_t waveFillPos)
    {
        if (ms_isReplayGainEnabled == false)
            return;

        auto rg = m_song->subsongs[m_id.subsongId.index].replayGain;
        if (!rg.IsValid())
            return;

        auto scale = powf(10.0f, rg.gain / 20.0f);
        auto* waveData = m_waveData + waveFillPos;
        assert(((waveFillPos | numSamples) & 1) == 0);

/*
        // clipping
        if (clipping && rg.peak > 0)
            scale = Min(scale, 1.0 / rg.peak);
*/

        __m128 vScale = _mm_set1_ps(scale);
        for (uint32_t i = 0; i < numSamples; i += 2)
        {
            __m128 vData = _mm_load_ps(pCast<float>(waveData + i));
            __m128 vResult = _mm_mul_ps(vData, vScale);
            _mm_storeu_ps(pCast<float>(waveData + i), vResult);
        }
    }

    void Player::Crossfade(uint32_t numSamples, uint32_t waveFillPos)
    {
        auto time = m_crossFadePosition;
        auto crossFadeLength = m_crossFadeInLength;
        if (time < crossFadeLength)
        {
            numSamples = Min(numSamples, crossFadeLength - time);
            auto* waveData = m_waveData + waveFillPos;
            for (uint32_t i = 0; i < numSamples; ++i)
            {
                float c = sinf(((time + i) * 3.14159265359f * 0.5f) / crossFadeLength);
                waveData[i] = waveData[i] * c;
            }
        }
        else
        {
            crossFadeLength = m_crossFadeOutLength;
            auto crossFadeOut = m_crossFadeOut;
            if (crossFadeLength && time + numSamples > crossFadeOut)
            {
                if (time < crossFadeOut)
                {
                    auto n = crossFadeOut - time;
                    numSamples -= n;
                    time += n;
                    waveFillPos += n;
                }
                time -= crossFadeOut;
                auto* waveData = m_waveData + waveFillPos;
                if (time >= crossFadeLength)
                    memset(waveData, 0, sizeof(waveData[0]) * numSamples);
                else for (uint32_t i = 0; i < numSamples; ++i)
                {
                    float c = cosf((Min(int64_t(time + i), crossFadeLength) * 3.14159265359f * 0.5f) / crossFadeLength);
                    waveData[i] = waveData[i] * c;
                }
            }
        }
    }

    void Player::ResumeThread()
    {
        std::atomic_ref(m_isWaiting).store(false);
        m_semaphore.Signal();
    }

    void Player::SuspendThread()
    {
        std::atomic_ref(m_isWaiting).store(true);
        while (std::atomic_ref(m_waitTime).load() != INFINITE)
            thread::Sleep(0);
    }

    void Player::SetupCrossFade(CrossFadeState crossFadeState)
    {
        bool hasFadeIn = m_crossFadeInLength != 0;
        auto crossFadeLengthInMs = Clamp(ms_crossFadeLengthInMs, kMinCrossFadeLengthInMs, kMaxCrossFadeLengthInMs);

        auto isCrossFadeEnabled = crossFadeLengthInMs > 0 && m_song->subsongs[m_id.subsongId.index].durationCs >= (3 * crossFadeLengthInMs / 10);
        if (isCrossFadeEnabled && int(crossFadeState) & int(CrossFadeState::kCrossFadeIn))
            m_crossFadeInLength = uint32_t((crossFadeLengthInMs * uint64_t(m_replay->GetSampleRate())) / 1000ull);
        else
            m_crossFadeInLength = 0;
        if (isCrossFadeEnabled && int(crossFadeState) & int(CrossFadeState::kCrossFadeOut))
        {
            m_crossFadeOutLength = uint32_t((crossFadeLengthInMs * uint64_t(m_replay->GetSampleRate())) / 1000ull);
            m_crossFadeOut = uint32_t((m_song->subsongs[m_id.subsongId.index].durationCs * uint64_t(m_replay->GetSampleRate())) / 100ull) - m_crossFadeOutLength;
        }
        else
        {
            m_crossFadeOutLength = 0;
            m_crossFadeOut = 0;
        }

        if (m_crossFadePosition == 0)
        {
            if (!hasFadeIn && m_crossFadeInLength != 0)
                Crossfade(m_numSamples, 0);
            m_crossFadePosition = m_numSamples;
        }
    }
}
// namespace rePlayer