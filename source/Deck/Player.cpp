// Core
#include <Core/String.h>
#include <Imgui.h>
#include <Imgui/imgui_internal.h>
#include <Thread/Thread.h>

// rePlayer
#include <Deck/Deck.h>
#include <Graphics/Graphics.h>
#include <RePlayer/Core.h>

#include "Player.h"

// TagLib
#include <fileref.h>
#include <tag.h>
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
    struct Player::Wave
    {
        HWAVEOUT outHandle{ nullptr };
        WAVEHDR header{};
    };

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

    void Player::Play()
    {
        if (m_replay->IsStreaming() && !m_song->subsongs[m_id.subsongId.index].isPlayed)
        {
            m_song->subsongs[m_id.subsongId.index].isPlayed = true;
            m_id.MarkForSave();
        }
        if (m_status == Status::Paused)
        {
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
            m_waveFillPos = m_numCachedSamples;
            m_patternsPos = 0;

            m_numLoops = m_replay->CanLoop() ? Core::GetDeck().IsEndless() ? INT_MAX : GetSubsong().state == SubsongState::Loop ? 1 : 0 : 0;
            m_hasSeeked = m_replay->CanLoop() && Core::GetDeck().IsEndless();
            m_remainingFadeOut = m_replay->GetSampleRate() * 4;
            m_fadeOutSilence = 0;

            m_replay->ResetPlayback();
            m_replay->ApplySettings(m_song->metadata.Container());

            Render(m_numCachedSamples, 0);
            memset(m_waveData + m_numCachedSamples, 0, (m_numSamples - m_numCachedSamples) * sizeof(StereoSample));

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
            m_waveFillPos = m_numCachedSamples;
            m_patternsPos = 0;

            m_numLoops = m_replay->CanLoop() ? Core::GetDeck().IsEndless() ? INT_MAX : GetSubsong().state == SubsongState::Loop ? 1 : 0 : 0;
            m_remainingFadeOut = m_replay->GetSampleRate() * 4;
            m_fadeOutSilence = 0;

            timeInMs = m_replay->Seek(timeInMs);
            Render(m_numCachedSamples, 0);
            memset(m_waveData + m_numCachedSamples, 0, (m_numSamples - m_numCachedSamples) * sizeof(StereoSample));

            if (m_status == Status::Paused)
                waveOutPause(m_wave->outHandle);
            waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
            if (m_status == Status::Playing)
                ResumeThread();

            int64_t seekPos = m_replay->GetSampleRate();
            seekPos *= timeInMs;
            m_songSeek = uint32_t(seekPos / 1000);
        }
    }

    Player::EndingState Player::IsEnding(uint32_t timeRangeInMs) const
    {
        if (m_status == Status::Playing)
        {
            auto songEnd = m_songEnd;
            MMTIME mmt{};
            mmt.wType = TIME_BYTES;
            waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));

            mmt.u.cb /= sizeof(StereoSample);
            if (mmt.u.cb >= songEnd)
                return EndingState::kEnded;
            mmt.u.cb += (uint64_t(timeRangeInMs) * m_replay->GetSampleRate()) / 1000;
            if (mmt.u.cb >= songEnd)
                return EndingState::kEnding;
        }
        return EndingState::kNotEnding;
    }

    uint32_t Player::GetPlaybackTimeInMs() const
    {
        if (m_wave->outHandle)
        {
            MMTIME mmt{};
            mmt.wType = TIME_BYTES;
            waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));

            mmt.u.cb /= sizeof(StereoSample);
            if (mmt.u.cb < m_songEnd)
                return uint32_t((1000ull * (mmt.u.cb + m_songSeek)) / m_replay->GetSampleRate());
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
            info += "Streaming Title: ";
            info += streamingTitle;
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
            MMTIME mmt{};
            mmt.wType = TIME_BYTES;
            waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));

            mmt.u.cb /= sizeof(StereoSample);
            patterns = m_replay->UpdatePatterns(mmt.u.cb - m_patternsPos, numLines, charWidth, spaceWidth, flags);
            m_patternsPos = mmt.u.cb;
        }
        return patterns;
    }

    StereoSample Player::GetVuMeter() const
    {
        if (m_status == Status::Stopped)
            return { 0.0f, 0.0f };

        uint32_t numVuMeterSamples = 2 * m_replay->GetSampleRate() / 60;

        MMTIME mmt{};
        mmt.wType = TIME_BYTES;
        waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));
        mmt.u.cb >>= 3;// / 2 channels / 4 bytes (32bit samples)
        auto wavePlayPos = mmt.u.cb;
        wavePlayPos += m_replay->GetSampleRate() / 30; // we should get our actual frame rate to guess the next 1 or 2 frames; here we are just predicting two frames at 60Hz
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
        return { sqrtf(sum.left / numVuMeterSamples), sqrtf(sum.right / numVuMeterSamples) };
    }

    void Player::DrawVisuals(float xMin, float yMin, float xMax, float yMax) const
    {
        DrawOscilloscope(xMin, yMin, xMax, yMax);
        DrawPatterns(xMin, yMin, xMax, yMax);
    }

    void Player::DrawOscilloscope(float xMin, float yMin, float xMax, float yMax) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        auto color = 0x98D9B27A; // ImGui::GetColorU32(ImGuiCol_FrameBgActive);
        if (m_status == Status::Stopped)
        {
            drawList->PrimReserve(6, 4);
            auto idxBase = drawList->_VtxCurrentIdx;

            float y0 = (yMin + yMax) * 0.5f - 0.5f;
            float y1 = (yMin + yMax) * 0.5f + 0.5f;

            drawList->PrimWriteVtx(ImVec2(xMin, y0), drawList->_Data->TexUvWhitePixel, color);
            drawList->PrimWriteVtx(ImVec2(xMin, y1), drawList->_Data->TexUvWhitePixel, color);
            drawList->PrimWriteVtx(ImVec2(xMax, y0), drawList->_Data->TexUvWhitePixel, color);
            drawList->PrimWriteVtx(ImVec2(xMax, y1), drawList->_Data->TexUvWhitePixel, color);

            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 0));
            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 1));
            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 2));

            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 2));
            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 1));
            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 3));
        }
        else
        {
            MMTIME mmt{};
            mmt.wType = TIME_BYTES;
            waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));
            mmt.u.cb >>= 3;// / 2 channels / 4 bytes (32bit samples)
            auto wavePlayPos = mmt.u.cb;
            wavePlayPos += m_replay->GetSampleRate() / 30; // we should get our actual frame rate to guess the next 1 or 2 frames; here we are just predicting two frames at 60Hz

            uint32_t numOscilloscopeSamples = 2 * m_replay->GetSampleRate() / 60;

            auto numSamplesMask = m_numSamples - 1;
            auto waveData = m_waveData;

            drawList->PrimReserve((numOscilloscopeSamples - 1) * 6, numOscilloscopeSamples * 2);
            auto idxBase = drawList->_VtxCurrentIdx;

            for (uint32_t i = 0; i < numOscilloscopeSamples; i++)
            {
                float x = xMin + i * (xMax - xMin) / (numOscilloscopeSamples - 1.0f);

                auto pos = (wavePlayPos + i) & numSamplesMask;
                auto y0 = waveData[pos].left;
                auto y1 = waveData[pos].right;
                if (y1 < y0)
                    std::swap(y0, y1);
                y0 = yMin + (yMax - yMin) * Saturate((y0 + 1.0f) * 0.5f);
                y1 = yMin + (yMax - yMin) * Saturate((y1 + 1.0f) * 0.5f);
                auto d = y1 - y0;
                if (d < 1.0f)
                {
                    y0 = y0 + d * 0.5f - 0.5f;
                    y1 = y1 - d * 0.5f + 0.5f;
                }

                drawList->PrimWriteVtx(ImVec2(x, y0), drawList->_Data->TexUvWhitePixel, color);
                drawList->PrimWriteVtx(ImVec2(x, y1), drawList->_Data->TexUvWhitePixel, color);

                if (i != 0)
                {
                    drawList->PrimWriteIdx(ImDrawIdx(idxBase + (i - 1) * 2));
                    drawList->PrimWriteIdx(ImDrawIdx(idxBase + (i - 1) * 2 + 1));
                    drawList->PrimWriteIdx(ImDrawIdx(idxBase + i * 2));

                    drawList->PrimWriteIdx(ImDrawIdx(idxBase + i * 2));
                    drawList->PrimWriteIdx(ImDrawIdx(idxBase + (i - 1) * 2 + 1));
                    drawList->PrimWriteIdx(ImDrawIdx(idxBase + i * 2 + 1));
                }
            }
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

    uint32_t Player::GetVolume()
    {
        DWORD value;
        waveOutGetVolume(nullptr, &value);
        return Max(value & 0xffFF, value >> 16);
    }

    void Player::SetVolume(uint32_t volume)
    {
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
        , m_numCachedSamples(m_numSamples / 2)
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
            Render(m_numCachedSamples, 0);
            m_wavePlayPos = 0;
            m_waveFillPos = m_numCachedSamples;
            memset(m_waveData + m_waveFillPos, 0, (numSamples - m_numCachedSamples) * sizeof(StereoSample));

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
                    Core::FromJob([This, extraInfo = std::move(extraInfo)]
                    {
                        This->m_extraInfo = extraInfo;
                    });
                }
            });
        }
        else
            m_isJobDone = true;
        return false;
    }

    void Player::ThreadUpdate()
    {
        auto numSamples = m_numSamples;
        auto numCachedSamples = m_numCachedSamples;
        auto numSamplesMask = numSamples - 1;
        while (std::atomic_ref(m_isRunning).load())
        {
            auto startTime = std::chrono::high_resolution_clock::now();

            MMTIME mmt{};
            mmt.wType = TIME_BYTES;
            waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));
            mmt.u.cb /= sizeof(StereoSample);
            auto wavePlayPos = mmt.u.cb;
            auto waveFillPos = m_waveFillPos;
            if (m_wavePlayPos > wavePlayPos) // loop or something wrong happened; need to detect when waveOut had a failure
            {
                m_songSeek += m_wavePlayPos; // simulate a seek to keep track of current time
                waveFillPos = (waveFillPos & numSamplesMask) | (wavePlayPos & ~numSamplesMask);
                waveFillPos += numCachedSamples;
            }
            m_wavePlayPos = wavePlayPos;

            wavePlayPos = wavePlayPos + numCachedSamples;
            while (waveFillPos < wavePlayPos)
            {
                auto count = Min(numSamples - (waveFillPos & numSamplesMask), wavePlayPos - waveFillPos);
                Render(count, waveFillPos & numSamplesMask);
                waveFillPos += count;
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

    void Player::Render(uint32_t numSamples, uint32_t waveFillPos)
    {
        auto subsongState = m_replay->CanLoop() ? GetSubsong().state : SubsongState::Standard;
        auto waveData = m_waveData;
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
                return;
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
                    for (uint32_t i = 0; i < fadeOutSize; i++)
                    {
                        auto left = waveData[waveFillPos].left * fadeOut;
                        auto right = waveData[waveFillPos].right * fadeOut;

                        waveData[waveFillPos].left = left;
                        waveData[waveFillPos].right = right;

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
}
// namespace rePlayer