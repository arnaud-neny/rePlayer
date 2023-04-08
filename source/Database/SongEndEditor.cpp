#include "SongEndEditor.h"

// core
#include <Imgui.h>
#include <IO/Stream.h>

// rePlayer
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <Replays/Replay.h>

// Windows
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmeapi.h>
#include <Mmreg.h>

#include <atomic>

namespace rePlayer
{
    struct SongEndEditor::Wave
    {
        HWAVEOUT outHandle = nullptr;
        WAVEHDR header = {};
    };

    SongEndEditor* SongEndEditor::Create(ReplayMetadataContext& context, MusicID musicId)
    {
        musicId.subsongId.index = context.songIndex;
        auto currentSong = musicId.GetSong();
        if (auto replay = Core::GetReplays().Load(musicId.GetStream(), currentSong->Edit()->metadata.Container(), currentSong->Edit()->type))
            return new SongEndEditor(musicId, replay, context.duration);
        context.isSongEndEditorEnabled = false;
        return nullptr;
    }

    SongEndEditor::SongEndEditor(MusicID musicId, Replay* replay, uint32_t duration)
        : m_musicId(musicId)
        , m_replay(replay)
        , m_duration(duration)
        , m_wave(new Wave)
    {
        replay->SetSubsong(musicId.subsongId.index);
        replay->ApplySettings(musicId.GetSong()->Edit()->metadata.Container());

        static constexpr uint64_t kDefaultSongLength = 60 * 15; // in seconds
        m_numSamples = kDefaultSongLength * replay->GetSampleRate();
        m_samples.Resize(m_numSamples);
        memset(m_samples.Items(), 0, m_samples.Size());

        m_threadHandle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, this, 0, nullptr);
        if (m_threadHandle == 0)
            Render();

        OpenAudio();

        ImGui::OpenPopup("SongEndEditor");
    }

    SongEndEditor::~SongEndEditor()
    {
        m_isRunning = false;
        m_semaphore.Signal();

        if (m_wave->outHandle)
        {
            waveOutPause(m_wave->outHandle);
            waveOutReset(m_wave->outHandle);
            waveOutClose(m_wave->outHandle);
        }

        if (m_threadHandle)
        {
            WaitForSingleObject(m_threadHandle, INFINITE);
            CloseHandle(m_threadHandle);
        }

        delete m_replay;
        delete m_wave;
    }

    bool SongEndEditor::Update(ReplayMetadataContext& context)
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(640.0f, 128.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() * 5), ImVec2(FLT_MAX, FLT_MAX));
        bool isOpened = true;
        if (ImGui::BeginPopupModal("SongEndEditor", &isOpened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            auto numMillisecondsPerPixel = m_numMillisecondsPerPixel;
            auto sampleRate = m_replay->GetSampleRate();
            uint32_t numSamples = m_numSamples;
            uint64_t numFrames = (1000ull * numSamples) / (sampleRate * numMillisecondsPerPixel);
            if (m_frames.NumItems() != numFrames)
            {
                m_frames.Resize(numFrames);
                for (auto& frame : m_frames)
                    frame = { 127, 127, 0 };
                m_currentFrameSample = 0;
            }
            if (m_currentFrameSample != m_numSamples)
            {
                uint32_t currentSample = ReadCurrentSample();
                uint32_t startFrame = uint32_t((1000ull * m_currentFrameSample) / (sampleRate * numMillisecondsPerPixel));
                uint32_t endFrame = currentSample == numSamples ? m_frames.NumItems() : uint32_t((1000ull * currentSample) / (sampleRate * numMillisecondsPerPixel));
                if (startFrame != endFrame)
                {
                    auto* samples = m_samples.Items();
                    for (uint32_t i = startFrame; i < endFrame; i++)
                    {
                        auto x0 = uint32_t(i * numMillisecondsPerPixel * sampleRate / 1000);
                        auto x1 = uint32_t((i + 1) * numMillisecondsPerPixel * sampleRate / 1000);
                        float s = (samples[x0].left + samples[x0].right) * 0.5f;
                        float rms = s * s;
                        float min = s;
                        float max = s;
                        for (auto x = x0 + 1; x < x1; x++)
                        {
                            s = (samples[x].left + samples[x].right) * 0.5f;
                            rms += s * s;
                            if (s < min)
                                min = s;
                            else if (s > max)
                                max = s;
                        }
                        rms = sqrtf(rms / (x1 - x0));
                        m_frames[i].min = uint8_t(Saturate(min * 0.5f + 0.5f) * 255);
                        m_frames[i].max = uint8_t(Saturate(max * 0.5f + 0.5f) * 255);
                        m_frames[i].rms = uint8_t(Saturate(rms) * 127);
                    }
                    m_currentFrameSample = currentSample;
                }
            }

            auto width = WaveformUI();
            if (ImGui::BeginTable("Toolbar", 5, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 0.0f, 1);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 0.0f, 2);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 0.0f, 3);
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 0.0f, 4);

                ImGui::TableNextColumn();
                PlaybackUI();
                ImGui::TableNextColumn();
                ScaleUI(width);
                ImGui::TableNextColumn();
                DurationUI();
                ImGui::TableNextColumn();
                RenderUI();
                ImGui::TableNextColumn();
                if (ImGui::Button("Ok"))
                {
                    context.songIndex = m_musicId.subsongId.index;
                    context.duration = m_duration;
                    isOpened = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Save end of song\nand close");
                ImGui::EndTable();
            }

            ImGui::EndPopup();
        }

        return !isOpened;
    }

    void SongEndEditor::Render()
    {
        uint32_t remainingSamples = m_samples.NumItems() - m_currentSample;
        uint32_t preNumSamples = remainingSamples;
        auto* waveform = m_samples.Items(m_currentSample);
        auto replay = m_replay;
        while (remainingSamples && m_isRunning)
        {
            auto numSamples = replay->Render(waveform, Min(32768ul, remainingSamples));
            if ((numSamples | preNumSamples) == 0)
            {
                std::atomic_ref(m_currentSample) += remainingSamples;
                remainingSamples = 0;
            }
            else
            {
                waveform += numSamples;
                remainingSamples -= numSamples;
                preNumSamples = numSamples;
                std::atomic_ref(m_currentSample) += numSamples;
            }
        }
    }

    inline uint32_t SongEndEditor::ReadCurrentSample()
    {
        uint32_t currentSample = 0;
        std::atomic_ref(m_currentSample).compare_exchange_strong(currentSample, 0);
        return currentSample;
    }

    void SongEndEditor::OpenAudio()
    {
        WAVEFORMATEX waveFormat{};
        waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveFormat.nChannels = 2;
        waveFormat.wBitsPerSample = 32;
        waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
        waveFormat.nSamplesPerSec = m_replay->GetSampleRate();
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        waveFormat.cbSize = 0;

        if (waveOutOpen(&m_wave->outHandle, WAVE_MAPPER, &waveFormat, 0, 0, 0) != S_OK)
            return;

        m_wave->header.dwFlags = 0;
        m_wave->header.lpData = m_samples.Items<CHAR>();
        m_wave->header.dwBufferLength = m_samples.Size<uint32_t>();
        m_wave->header.dwBytesRecorded = 0;
        m_wave->header.dwUser = 0;
        m_wave->header.dwLoops = 0xffFFffFF;
        waveOutPrepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
        waveOutPause(m_wave->outHandle);
        waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
    }

    uint32_t SongEndEditor::ThreadFunc(uint32_t* lpdwParam)
    {
        auto* This = reinterpret_cast<SongEndEditor*>(lpdwParam);
        while (This->m_isRunning)
        {
            This->Render();
            This->m_semaphore.Wait();
        }
        return 0;
    }

    uint32_t SongEndEditor::WaveformUI()
    {
        auto pos = ImGui::GetCursorScreenPos();
        uint32_t width = 1;
        if (ImGui::BeginChild("WaveForm", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetScrollX(0);
            else if (m_isWaveScrolling)
            {
                ImGui::SetScrollX(m_waveScroll);
                m_isWaveScrolling = false;
            }
            auto numMillisecondsPerPixel = m_numMillisecondsPerPixel;
            auto size = ImGui::GetContentRegionAvail();
            auto offset = uint32_t(ImGui::GetScrollX());
            if (ImGui::InvisibleButton("DrawWave", ImVec2(Max(size.x, m_frames.NumItems<float>()), size.y), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight))
            {
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && m_wave->outHandle && m_isPlaying)
                {
                    m_waveStartPosition = Min(uint32_t((uint64_t(offset + ImGui::GetMousePos().x - pos.x) * numMillisecondsPerPixel * m_replay->GetSampleRate()) / 1000), m_samples.NumItems() - 1);
                    waveOutReset(m_wave->outHandle);
                    waveOutUnprepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
                    m_wave->header.dwFlags = 0;
                    m_wave->header.lpData = m_samples.Items<CHAR>(m_waveStartPosition);
                    m_wave->header.dwBufferLength = (m_samples.NumItems() - m_waveStartPosition) * m_samples.ItemSize<uint32_t>();
                    waveOutPrepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
                    waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
                }
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    m_duration = uint32_t(uint64_t(offset + ImGui::GetMousePos().x - pos.x) * numMillisecondsPerPixel);
            }
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsItemHovered())
            {
                if (ImGui::GetIO().MouseWheel != 0)
                {
                    if (ImGui::GetIO().MouseWheel < 0)
                        m_numMillisecondsPerPixel *= uint64_t(ImGui::GetIO().MouseWheel * -2);
                    else
                        m_numMillisecondsPerPixel /= uint64_t(ImGui::GetIO().MouseWheel * 2);
                    m_numMillisecondsPerPixel = Clamp(m_numMillisecondsPerPixel, 1ull, 1 + (m_numSamples * 1000ull) / (m_replay->GetSampleRate() * uint64_t(size.x)));
                    ImGui::SetScrollX((offset + ImGui::GetMousePos().x - pos.x) * numMillisecondsPerPixel / m_numMillisecondsPerPixel - ImGui::GetMousePos().x + pos.x);
                    m_waveScroll = (offset + ImGui::GetMousePos().x - pos.x) * numMillisecondsPerPixel / m_numMillisecondsPerPixel - ImGui::GetMousePos().x + pos.x;
                    m_isWaveScrolling = true;
                }

                auto cursorFrame = m_frames[offset + uint32_t(ImGui::GetMousePos().x - pos.x)];
                ImGui::Tooltip("min: %d\nmax: %d\nrms: %u", cursorFrame.min - 127, cursorFrame.max - 127, cursorFrame.rms);
            }
            if (ImGui::IsItemActive())
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                {
                    ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle).x);
                    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
                    m_duration = uint32_t(uint64_t(offset + ImGui::GetMousePos().x - pos.x) * numMillisecondsPerPixel);
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            auto color0 = ImGui::GetColorU32(ImGuiCol_Button);
            auto color1 = ImGui::GetColorU32(ImGuiCol_ButtonActive);
            auto* frames = m_frames.Items();
            auto maxFrames = Min(uint64_t(size.x), m_frames.NumItems<uint64_t>());
            for (uint64_t x = 0, e = maxFrames; x < e; x++)
            {
                auto frame = frames[x + offset];
                auto p0 = pos;
                p0.y += size.y * frame.min / 255.0f;
                auto p1 = pos;
                p1.y += size.y * frame.max / 255.0f;
                drawList->AddLine(p0, p1, color0);

                p0 = pos;
                p0.y += size.y * 0.5f - size.y * frame.rms / 255.0f;
                p1 = pos;
                p1.y += size.y * 0.5f + size.y * frame.rms / 255.0f;
                drawList->AddLine(p0, p1, color1);
                pos.x++;
            }
            pos.x -= maxFrames;
            auto drawListFlags = drawList->Flags;
            drawList->Flags &= ~(ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedLinesUseTex | ImDrawListFlags_AntiAliasedFill);
            drawList->AddTriangleFilled(ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel + 0.5f, pos.y + 6.5f)
                , ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel - 4.0f, pos.y)
                , ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel + 5.0f, pos.y)
                , 0x8000ff00);
            drawList->AddTriangleFilled(ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel + 0.5f, pos.y - 6.5f + size.y)
                , ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel - 4.0f, pos.y + size.y)
                , ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel + 5.0f, pos.y + size.y)
                , 0x8000ff00);
            drawList->AddLine(ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel, pos.y + 6.0f), ImVec2(pos.x - offset + m_duration / numMillisecondsPerPixel, pos.y - 6.0f + size.y), 0x8000ff00);
            drawList->Flags = drawListFlags;

            if (m_wave->outHandle && m_isPlaying)
            {
                MMTIME mmt{};
                mmt.wType = TIME_BYTES;
                waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));
                mmt.u.ms = uint32_t((m_waveStartPosition * 1000ull + (mmt.u.cb * 1000ull) / sizeof(StereoSample)) / m_replay->GetSampleRate());
                auto frameIndex = int64_t(mmt.u.ms / numMillisecondsPerPixel);
                frameIndex -= offset;
                if (frameIndex >= 0 && frameIndex < int64_t(maxFrames))
                {
                    pos.x = pos.x + frameIndex;
                    auto p0 = pos;
                    auto p1 = pos;
                    p1.y += size.y;
                    drawList->AddLine(p0, p1, 0x8000ffFF);
                }
            }

            width = uint32_t(size.x);
        }
        ImGui::EndChild();

        return width;
    }

    void SongEndEditor::PlaybackUI()
    {
        if (ImGui::Button("Stop"))
        {
            if ((m_isPlaying || m_waveStartPosition != 0) && m_wave->outHandle)
            {
                m_waveStartPosition = 0;
                waveOutReset(m_wave->outHandle);
                waveOutUnprepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
                m_wave->header.dwFlags = 0;
                m_wave->header.lpData = m_samples.Items<CHAR>();
                m_wave->header.dwBufferLength = m_samples.Size<uint32_t>();
                waveOutPrepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
                waveOutPause(m_wave->outHandle);
                waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
            }
            m_isPlaying = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(m_isPlaying ? "Pause###Playback" : "Play###Playback"))
        {
            m_isPlaying = !m_isPlaying;
            if (m_wave->outHandle)
            {
                if (m_isPlaying)
                    waveOutRestart(m_wave->outHandle);
                else
                    waveOutPause(m_wave->outHandle);
            }
        }
    }

    void SongEndEditor::ScaleUI(uint32_t width)
    {
        auto numMillisecondsPerPixel = uint32_t(m_numMillisecondsPerPixel);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragUint("##Scale", &numMillisecondsPerPixel, 1.0f, 1, 1 + (m_numSamples * 1000ull) / (m_replay->GetSampleRate() * uint64_t(width)), "%u ms per pixel", ImGuiSliderFlags_AlwaysClamp, ImVec2(0.0f, 0.5f));
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Waveform display scale");
        m_numMillisecondsPerPixel = numMillisecondsPerPixel;
    }

    void SongEndEditor::DurationUI()
    {
        auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3;
        ImGui::SetNextItemWidth(2.0f * width / 3.0f);
        ImGui::DragUint("##Duration", &m_duration, 1000.0f, 0, 0, "End of Song", ImGuiSliderFlags_NoInput, ImVec2(1.0f, 0.5f));
        int32_t milliseconds = m_duration % 1000;
        int32_t seconds = (m_duration / 1000) % 60;
        int32_t minutes = m_duration / 60000;
        ImGui::SameLine();
        width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2;
        ImGui::SetNextItemWidth(width / 3.0f);
        ImGui::DragInt("##Minutes", &minutes, 0.1f, 0, 65535, "%d m", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(width / 2.0f);
        ImGui::DragInt("##Seconds", &seconds, 0.1f, 0, 59, "%d s", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        width = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(width);
        ImGui::DragInt("##Milliseconds", &milliseconds, 1.0f, 0, 999, "%d ms", ImGuiSliderFlags_AlwaysClamp);
        m_duration = uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds);
    }

    void SongEndEditor::RenderUI()
    {
        const uint32_t numSamplesToAdd = m_replay->GetSampleRate() * 300;
        uint32_t numAllocatedSamples = m_samples.NumItems();
        uint32_t numSamples = m_numSamples;
        ImGui::BeginDisabled((numSamples + numSamplesToAdd) * m_samples.ItemSize() > 0xffFFffFFull);
        if (ImGui::Button("+"))
        {
            numSamples += numSamplesToAdd;
            m_numSamples = numSamples;
        }
        if (ImGui::IsItemHovered())
            ImGui::Tooltip("Add 5 minutes\nto waveform");
        ImGui::EndDisabled();
        if (numSamples != numAllocatedSamples && ReadCurrentSample() == numAllocatedSamples)
        {
            waveOutPause(m_wave->outHandle);
            MMTIME mmt{};
            mmt.wType = TIME_BYTES;
            waveOutGetPosition(m_wave->outHandle, &mmt, sizeof(MMTIME));
            m_waveStartPosition += mmt.u.cb / m_samples.ItemSize<uint32_t>();

            waveOutReset(m_wave->outHandle);
            waveOutUnprepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));

            m_samples.Resize(numSamples);
            memset(m_samples.Items(numAllocatedSamples), 0, (numSamples - numAllocatedSamples) * m_samples.ItemSize());
            m_semaphore.Signal();

            m_wave->header.dwFlags = 0;
            m_wave->header.lpData = m_samples.Items<CHAR>(m_waveStartPosition);
            m_wave->header.dwBufferLength = m_samples.Size<uint32_t>() - m_waveStartPosition * m_samples.ItemSize<uint32_t>();
            waveOutPrepareHeader(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
            waveOutPause(m_wave->outHandle);
            waveOutWrite(m_wave->outHandle, &m_wave->header, sizeof(m_wave->header));
            if (m_isPlaying)
                waveOutRestart(m_wave->outHandle);
        }
    }
}
// namespace rePlayer