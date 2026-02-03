// core
#include <Audio/AudioTypes.inl.h>
#include <Core/Log.h>
#include <Core/String.h>
#include <Imgui.h>
#include <IO/Stream.h>
#include <Thread/Thread.h>

// rePlayer
#include <RePlayer/Core.h>
#include <RePlayer/Replays.h>
#include <Replays/Replay.h>
#include <UI/BusySpinner.h>

#include "SongEndEditor.h"

// FFTW
#include <fftw3.h>

// Windows
#include <windows.h>
#include <mmeapi.h>
#include <Mmreg.h>

#include <atomic>
#include <algorithm>
#include <numeric>

namespace rePlayer
{
    struct SongEndEditor::Wave
    {
        HWAVEOUT outHandle = nullptr;
        WAVEHDR header = {};
    };

    SongEndEditor* SongEndEditor::Create(ReplayMetadataContext& context, MusicID musicId)
    {
        musicId.subsongId.index = context.subsongIndex;
        auto currentSong = musicId.GetSong();
        if (auto replay = Core::GetReplays().Load(musicId.GetStream(), currentSong->Edit()->metadata.Container(), currentSong->Edit()->type))
            return new SongEndEditor(musicId, replay, context.loop);
        context.isSongEndEditorEnabled = false;
        return nullptr;
    }

    SongEndEditor::SongEndEditor(MusicID musicId, Replay* replay, LoopInfo loop)
        : m_musicId(musicId)
        , m_replay(replay)
        , m_loop(loop)
        , m_wave(new Wave)
    {
        auto artists = musicId.GetArtists();
        if (!artists.empty())
            m_title = artists + " - " + musicId.GetTitle();
        else
            m_title = musicId.GetTitle();

        replay->SetSubsong(musicId.subsongId.index);
        replay->ApplySettings(musicId.GetSong()->Edit()->metadata.Container());

        static constexpr uint64_t kDefaultSongLength = 60 * 15; // in seconds
        m_numSamples = kDefaultSongLength * replay->GetSampleRate();
        m_samples.Resize(m_numSamples);
        memset(m_samples.Items(), 0, m_samples.Size<size_t>());

        Core::AddJob([this]()
        {
            while (std::atomic_ref(m_isRunning).load())
            {
                Render();
                m_semaphore.Wait();
            }
            std::atomic_ref(m_isJobDone).store(true);
        });

        OpenAudio();

        ImGui::OpenPopup("###End Of Song Editor");
    }

    SongEndEditor::~SongEndEditor()
    {
        while (m_busySpinner.IsValid())
            thread::Sleep(1);

        std::atomic_ref(m_isRunning).store(false);
        m_semaphore.Signal();

        if (m_wave->outHandle)
        {
            waveOutPause(m_wave->outHandle);
            waveOutReset(m_wave->outHandle);
            waveOutClose(m_wave->outHandle);
        }

        while (!std::atomic_ref(m_isJobDone).load())
            thread::Sleep(0);

        delete m_replay;
        delete m_wave;
    }

    bool SongEndEditor::Update(ReplayMetadataContext& context)
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(640.0f, 128.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() * 5), ImVec2(FLT_MAX, FLT_MAX));
        bool isOpened = true;

        char* title = (char*)_alloca(m_title.size() + 128);
        if (IsBusy())
            sprintf(title, m_title.size() + 128, "End Of Song Editor \"%s\" %u%%###End Of Song Editor", m_title.c_str(), (ReadCurrentSample() * 100ull) / m_numSamples);
        else
            sprintf(title, m_title.size() + 128, "End Of Song Editor \"%s\"###End Of Song Editor", m_title.c_str());

        if (ImGui::BeginPopupModal(title, &isOpened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::BeginDisabled(m_busySpinner.IsValid());
            m_busySpinner->Begin();

            auto numMillisecondsPerPixel = m_numMillisecondsPerPixel;
            auto sampleRate = m_replay->GetSampleRate();
            uint32_t numSamples = m_numSamples;
            uint32_t numFrames = uint32_t((1000ull * numSamples) / (sampleRate * numMillisecondsPerPixel));
            if (m_frames.NumItems() != numFrames)
            {
                m_frames.Resize(numFrames);
                for (auto& frame : m_frames)
                    frame = { 127, 127, 0 };
                m_currentFrameSample = 0;
            }
            if (m_currentFrameSample != numSamples)
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
                LoopUI();
                ImGui::TableNextColumn();
                ParamsUI();
                ImGui::TableNextColumn();
                if (ImGui::Button("Ok"))
                {
                    context.subsongIndex = m_musicId.subsongId.index;
                    context.loop = m_loop;
                    isOpened = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                    ImGui::Tooltip("Save end of song\nand close");
                ImGui::EndTable();
            }

            m_busySpinner->End();
            ImGui::EndDisabled();

            ImGui::EndPopup();
        }
        else if (isOpened == true)
        {
            Log::Warning("End Of Song Editor: unexpected close\n");
            isOpened = false;
        }

        m_busySpinner->Update();

        return !isOpened;
    }

    void SongEndEditor::Render()
    {
        uint32_t remainingSamples = m_samples.NumItems() - m_currentSample;
        uint32_t preNumSamples = remainingSamples;
        auto* waveform = m_samples.Items(m_currentSample);
        auto replay = m_replay;
        uint32_t sampleRate = replay->GetSampleRate();
        uint32_t silence = m_silence;
        while (remainingSamples && std::atomic_ref(m_isRunning).load())
        {
            auto numSamples = replay->Render(waveform, Min(32768ul, remainingSamples));
            if (numSamples == 0)
            {
                Core::FromJob([this, current_duration = uint32_t((m_currentSample * 1000ull) / sampleRate)]()
                {
                    m_loops.Add(current_duration);
                });
            }
            for (uint32_t i = 0; i < numSamples; ++i)
            {
                if (fabs(m_lastSample.left - waveform[i].left) > 1.5f / 127.f || fabs(m_lastSample.right - waveform[i].right) > 1.5f / 127.f)
                {
                    silence = m_currentSample + i;
                    m_lastSample = waveform[i];
                    m_silence = silence + sampleRate / 4;
                }
            }
            if ((numSamples | preNumSamples) == 0)
            {
                m_loop = { 0, uint32_t((m_currentSample * 1000ull) / sampleRate) };
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
        m_silence = Min(silence + sampleRate / 4, m_currentSample);
    }

    inline uint32_t SongEndEditor::ReadCurrentSample() const
    {
        return std::atomic_ref(m_currentSample).load();
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

    inline bool SongEndEditor::IsBusy() const
    {
        return ReadCurrentSample() != m_samples.NumItems();
    }

    LoopInfo SongEndEditor::FindLoop()
    {
        /*
            Massive code build only with ChatGPT! (as I had no idea of what I was doing)
            This is finally working (but it's a little bit of a mess)
        */
        // convert to mono
        m_busySpinner->Info("Converting to mono");

        auto downsampleFactor = m_loopDetection.downsampleFactor;
        std::vector<float> mono(m_samples.NumItems() / downsampleFactor);
        for (size_t i = 0, e = mono.size(); i < e; i++)
        {
            float sample = m_samples[i * downsampleFactor].left + m_samples[i * downsampleFactor].right;
            for (uint32_t j = 1; j < downsampleFactor; j++)
                sample += m_samples[i * downsampleFactor + j].left + m_samples[i * downsampleFactor + j].right;
            mono[i] = sample / float(downsampleFactor);
        }

        // cache sample rate
        auto sampleRate = m_replay->GetSampleRate() / downsampleFactor;

        // compute autocorrelation using FFT convolution
        auto autocorrelate = [](const std::vector<float>& x)
        {
            auto N = int(x.size());
            int Nfft = 1;
            while (Nfft < 2 * N) Nfft <<= 1;

            fftwf_complex* X = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (Nfft / 2 + 1));
            fftwf_complex* R = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (Nfft / 2 + 1));
            float* in = (float*)fftwf_malloc(sizeof(float) * Nfft);
            float* out = (float*)fftwf_malloc(sizeof(float) * Nfft);
                                                                             
            std::fill(in + N, in + Nfft, 0.0f);

            float mean = std::accumulate(x.begin(), x.end(), 0.0f) / x.size();

            for (int i = 0; i < N; ++i) {
                float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265358979323846f * i / (N - 1))); // Hann
                in[i] = (x[i] - mean) * w;
            }

            fftwf_plan fwd = fftwf_plan_dft_r2c_1d(Nfft, in, X, FFTW_ESTIMATE);
            fftwf_plan inv = fftwf_plan_dft_c2r_1d(Nfft, R, out, FFTW_ESTIMATE);

            for (int lag = 1; lag < N; ++lag)
                out[lag] /= float(N - lag);

            fftwf_execute(fwd);

            // power spectrum |X|^2
            for (int i = 0; i < Nfft / 2 + 1; i++) {
                float a = X[i][0];
                float b = X[i][1];
                R[i][0] = a * a + b * b;
                R[i][1] = 0.0;
            }

            fftwf_execute(inv);

            // normalize autocorrelation
            std::vector<float> ac(N);

            float r0 = out[0]; // total energy
            for (int lag = 0; lag < N; ++lag) {
                float denom = r0 - (lag > 0 ? out[lag] : 0.0f);
                ac[lag] = (denom > 1e-6f) ? out[lag] / denom : 0.0f;
            }

            fftwf_destroy_plan(fwd);
            fftwf_destroy_plan(inv);
            fftwf_free(X);
            fftwf_free(R);
            fftwf_free(in);
            fftwf_free(out);

            fftwf_cleanup();

            return ac;
        };

        // find the first significant autocorrelation peak (loop length)
        auto detectLoopLength = [this](const std::vector<float>& ac, int minLag, int maxLag)
        {
            // Global max for relative threshold
            float globalMax = 0.0f;
            for (int i = minLag; i < maxLag; ++i)
                globalMax = std::max(globalMax, ac[i]);

            const float peakThresh = m_loopDetection.peakThreshold * globalMax;// 0.45-0.65
            const float consistencyThresh = m_loopDetection.consistencyThreshold;//0.35-0.55

            // Scan lags from SMALL to LARGE
            for (int L = minLag + 1; L < maxLag - 1; ++L) {
                float v = ac[L];

                // Must be a strong local peak
                if (v < peakThresh)
                    continue;
                if (v <= ac[L - 1] || v <= ac[L + 1])
                    continue;

                // Check that multiples agree
                int good = 0;
                int tested = 0;

                for (int k = 2; k <= 4; ++k) {
                    int m = k * L;
                    if (m >= maxLag)
                        break;

                    tested++;

                    float mv = ac[m];
                    if (m > 0) mv = std::max(mv, ac[m - 1]);
                    if (m + 1 < (int)ac.size()) mv = std::max(mv, ac[m + 1]);

                    if (mv > consistencyThresh * v)
                        good++;
                }

                // If enough multiples confirm, accept THIS (smallest) lag
                if (tested > 0 && good >= tested - 1)
                    return L;
            }

            // Fallback
            int bestLag = minLag;
            float bestVal = -1e30f;
            for (int i = minLag; i < maxLag; ++i) {
                if (ac[i] > bestVal) {
                    bestVal = ac[i];
                    bestLag = i;
                }
            }
            return bestLag;
        };

        // Mel-Frequency Cepstral Coefficients
        class MFCC
        {
        public:
            MFCC(int sampleRate, int frameSize, int melFilters, int coeffs)
                : sr(sampleRate), N(frameSize),
                melFilters(melFilters), coeffs(coeffs)
            {
                fftBins = N / 2 + 1;

                window.resize(N);
                melBank.resize(melFilters * fftBins);
                dct.resize(coeffs * melFilters);
                power.resize(fftBins);
                melE.resize(melFilters);

                fftIn = (float*)fftwf_malloc(sizeof(float) * N);
                fftOut = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftBins);

                plan = fftwf_plan_dft_r2c_1d(N, fftIn, fftOut, FFTW_MEASURE);

                buildWindow();
                buildMelBank();
                buildDCT();
            }

            ~MFCC()
            {
                fftwf_destroy_plan(plan);
                fftwf_free(fftIn);
                fftwf_free(fftOut);

                fftwf_cleanup();
            }

            void compute(const float* input, float* out)
            {
                for (int i = 0; i < N; ++i)
                    fftIn[i] = input[i] * window[i];

                fftwf_execute(plan);

                for (int i = 0; i < fftBins; ++i)
                {
                    float re = fftOut[i][0];
                    float im = fftOut[i][1];
                    power[i] = re * re + im * im;
                }

                std::memset(melE.data(), 0, sizeof(float) * melFilters);

                for (int m = 0; m < melFilters; ++m)
                {
                    float sum = 0.f;
                    const float* f = &melBank[m * fftBins];
                    for (int k = 0; k < fftBins; ++k)
                        sum += power[k] * f[k];

                    melE[m] = std::logf(sum + 1e-10f);
                }

                for (int c = 0; c < coeffs; ++c)
                {
                    float sum = 0.f;
                    const float* d = &dct[c * melFilters];
                    for (int m = 0; m < melFilters; ++m)
                        sum += melE[m] * d[m];

                    out[c] = sum;
                }
            }

            int coeffCount() const { return coeffs; }

        private:
            int sr, N, fftBins, melFilters, coeffs;
            fftwf_plan plan;
            float* fftIn;
            fftwf_complex* fftOut;

            std::vector<float> window, melBank, dct, power, melE;

            static float hz2mel(float hz)
            {
                return 2595.f * std::log10f(1.f + hz / 700.f);
            }

            static float mel2hz(float mel)
            {
                return 700.f * (std::powf(10.f, mel / 2595.f) - 1.f);
            }

            void buildWindow()
            {
                for (int i = 0; i < N; ++i)
                    window[i] = 0.5f - 0.5f *
                    std::cosf(2.f * 3.14159265358979323846f * i / (N - 1));
            }

            void buildMelBank()
            {
                float melMin = hz2mel(0.f);
                float melMax = hz2mel(sr * 0.5f);

                std::vector<float> melPts(melFilters + 2);
                std::vector<int> bins(melFilters + 2);

                for (int i = 0; i < melFilters + 2; ++i)
                {
                    melPts[i] = melMin +
                        (melMax - melMin) * i / (melFilters + 1);
                    bins[i] = int((N + 1) * mel2hz(melPts[i]) / sr);
                }

                std::fill(melBank.begin(), melBank.end(), 0.f);

                for (int m = 1; m <= melFilters; ++m)
                {
                    for (int k = bins[m - 1]; k < bins[m]; ++k)
                        melBank[(m - 1) * fftBins + k] =
                        float(k - bins[m - 1]) /
                        (bins[m] - bins[m - 1]);

                    for (int k = bins[m]; k < bins[m + 1]; ++k)
                        melBank[(m - 1) * fftBins + k] =
                        float(bins[m + 1] - k) /
                        (bins[m + 1] - bins[m]);
                }
            }

            void buildDCT()
            {
                float scale = std::sqrtf(2.f / melFilters);
                for (int c = 0; c < coeffs; ++c)
                    for (int m = 0; m < melFilters; ++m)
                        dct[c * melFilters + m] =
                        scale * std::cosf(
                            3.14159265358979323846f * c * (m + 0.5f) / melFilters);
            }
        };

        auto cosine = [](const float* a, const float* b, int n)
        {
            float dot = 0.f, na = 0.f, nb = 0.f;
            for (int i = 1; i < n; ++i) // skip MFCC[0]
            {
                dot += a[i] * b[i];
                na += a[i] * a[i];
                nb += b[i] * b[i];
            }
            return dot / (std::sqrt(na * nb) + 1e-12f);
        };

        auto findLoopStart = [&](
            const std::vector<std::vector<float>>& mfcc,
            int loopFrames,
            int compareFrames = 200,
            float threshold = 0.80f)
        {
            int total = (int)mfcc.size();

            for (int i = 0; i + loopFrames + compareFrames < total; ++i)
            {
                float score = 0.f;
                for (int k = 0; k < compareFrames; ++k)
                    score += cosine(
                        mfcc[i + k].data(),
                        mfcc[i + k + loopFrames].data(),
                        (int)mfcc[i].size());

                score /= compareFrames;

                if (score > threshold)
                    return i;  // FIRST strong match
            }
            return -1;
        };

        // 1. Autocorrelation
        m_busySpinner->Info("Detecting loop length");
        m_busySpinner->Indent(1);

        m_busySpinner->Info("Autocorrelate");
        auto ac = autocorrelate(mono);

        // 2. Detect loop length (search between 0.5s and 30s for example)
        int loopMin = sampleRate * m_loopDetection.loopMin;
        int loopMax = sampleRate * m_loopDetection.loopMax;

        m_busySpinner->Info("Detection");
        int loopLength = detectLoopLength(ac, loopMin, loopMax);

        // 3. Detect loop start using alignment refinement
        m_busySpinner->Indent(-1);
        m_busySpinner->Info("Detecting loop start");
        m_busySpinner->Indent(1);
        int FRAME = m_loopDetection.frame;// constexpr int FRAME = 2048;
        int HOP = m_loopDetection.hop;// constexpr int HOP = 512;
        int MEL = m_loopDetection.melFilters;//constexpr int MEL = 26;
        int MFCCS = m_loopDetection.mfccs;//constexpr int MFCCS = 13;
        MFCC mfcc(sampleRate, FRAME, MEL, MFCCS);

        std::vector<std::vector<float>> features;

        auto* message = m_busySpinner->Info("MFCC %u%%", 0u);
        for (size_t i = m_loopDetection.loopStart * sampleRate; i + FRAME < mono.size(); i += HOP)
        {
            features.emplace_back(MFCCS);
            mfcc.compute(&mono[i], features.back().data());
            m_busySpinner->UpdateMessageParam(message, uint32_t(((i + HOP) * 100ull) / mono.size()));
        }
        m_busySpinner->UpdateMessageParam(message, 100u);

        // ---- normalize MFCCs ----
        std::vector<float> mean(MFCCS, 0.f), var(MFCCS, 0.f);

        m_busySpinner->Info("Normalize");
        for (auto& f : features)
            for (int i = 0; i < MFCCS; ++i)
                mean[i] += f[i];

        for (float& m : mean)
            m /= features.size();

        for (auto& f : features)
            for (int i = 0; i < MFCCS; ++i)
                var[i] += (f[i] - mean[i]) * (f[i] - mean[i]);

        for (float& v : var)
            v = 1.f / std::sqrt(v / features.size() + 1e-8f);

        for (auto& f : features)
            for (int i = 0; i < MFCCS; ++i)
                f[i] = (f[i] - mean[i]) * var[i];

        m_busySpinner->Info("Detection");
        int loopFrames = loopLength / HOP;
        int loopStart = findLoopStart(features, loopFrames);
        if (loopStart < 0)
            loopStart = 0;
        m_busySpinner->Indent(-1);

        return { m_loopDetection.loopStart * 1000u + uint32_t((loopStart * HOP * 1000ull) / sampleRate), uint32_t((loopLength * 1000ull) / sampleRate) };
    }

    void SongEndEditor::BuildLoops()
    {
        auto loop = m_loop.GetFixed();
        if (loop.IsValid())
        {
            uint32_t currentEnd = loop.GetDuration();
            uint32_t end = uint32_t((m_numSamples * 1000ull) / m_replay->GetSampleRate());
            m_loops.Clear();
            for (; currentEnd < end; currentEnd += loop.GetFixed().length)
                m_loops.Add(currentEnd);
        }
    }

    uint32_t SongEndEditor::WaveformUI()
    {
        auto pos = ImGui::GetCursorScreenPos();
        uint32_t width = 1;
        if (ImGui::BeginChild("WaveForm", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_None, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetScrollX(0);
            else if (m_isWaveScrolling)
            {
                ImGui::SetScrollX(m_waveScroll);
                m_isWaveScrolling = false;
            }
            auto numMillisecondsPerPixel = m_numMillisecondsPerPixel;
            const auto size = ImGui::GetContentRegionAvail();
            auto offset = uint32_t(ImGui::GetScrollX());
            auto maxFrames = Min(uint64_t(size.x), m_frames.NumItems<uint64_t>());
            if ((offset + maxFrames) > m_frames.NumItems<uint64_t>()) // prevent offset overflow
                offset = uint32_t(m_frames.NumItems<uint64_t>() - maxFrames);
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

                auto framePos = Clamp(offset + uint32_t(ImGui::GetMousePos().x - pos.x), 0u, m_frames.NumItems() - 1u);
                auto cursorFrame = m_frames[framePos];
                auto time = uint64_t(framePos) * numMillisecondsPerPixel;
                ImGui::Tooltip("pos: %u:%02u:%03u\nmin: %d\nmax: %d\nrms: %u", uint32_t(time / 60000), uint32_t(time / 1000) % 60, uint32_t(time % 1000)
                    , cursorFrame.min - 127, cursorFrame.max - 127, cursorFrame.rms);
            }
            if (ImGui::IsItemActive())
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                {
                    ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle).x);
                    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
                {
                    // here we edit the loop
                    if (m_mouseMode == EditLoop::None)
                    {
                        m_mousePosWithLoop = ImGui::GetMousePos().x;
                        m_mouseLoop = m_loop;

                        auto curPos = uint32_t(Clamp(offset + int64_t(m_mousePosWithLoop - pos.x), 0ll, m_frames.NumItems() - 1ll) * numMillisecondsPerPixel);
                        if (curPos >= m_loop.start && curPos <= m_loop.GetDuration()) // start inside the loop
                        {
                            constexpr uint32_t kResizeWindow = 5;
                            m_mouseMode = EditLoop::Move; // move the loop by default
                            if (m_loop.length / numMillisecondsPerPixel >= kResizeWindow * 2)
                            {
                                float delta_start = m_mousePosWithLoop - (float(m_loop.start / numMillisecondsPerPixel) - offset + pos.x);
                                float delta_end = (float(m_loop.GetDuration() / numMillisecondsPerPixel) - offset + pos.x) - m_mousePosWithLoop;
                                if (delta_start <= kResizeWindow)
                                    m_mouseMode = EditLoop::ResizeLeft; // resize the loop from the left
                                else if (delta_end <= kResizeWindow)
                                    m_mouseMode = EditLoop::ResizeRight; // resize the loop from the right
                            }
                        }
                        else // start outside the current loop, then create it
                            m_mouseMode = EditLoop::Create;
                    }

                    float mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).x;
                    auto oldPos = uint32_t(Clamp(offset + int64_t(m_mousePosWithLoop - pos.x), 0ll, m_frames.NumItems() - 1ll) * numMillisecondsPerPixel);
                    auto newPos = uint32_t(Clamp(offset + int64_t(m_mousePosWithLoop + mouseDelta - pos.x), 0ll, m_frames.NumItems() - 1ll) * numMillisecondsPerPixel);
                    if (m_mouseMode == EditLoop::Move)
                    {
                        m_loop.start = uint32_t(Clamp(m_mouseLoop.start + mouseDelta * numMillisecondsPerPixel, 0ll, m_numSamples - 1ll));
                    }
                    else if (m_mouseMode == EditLoop::Create)
                    {
                        if (mouseDelta < 0.f)
                            std::swap(oldPos, newPos);
                        m_loop.start = oldPos;
                        m_loop.length = newPos - oldPos;
                    }
                    else if (m_mouseMode == EditLoop::ResizeLeft)
                    {
                        auto newStart = uint32_t(Clamp(int64_t(m_mouseLoop.start + mouseDelta * numMillisecondsPerPixel), 0, int64_t(m_numSamples) - 1));
                        auto oldEnd = m_mouseLoop.GetDuration();
                        if (newStart <= oldEnd)
                        {
                            m_loop.start = newStart;
                            m_loop.length = oldEnd - m_loop.start;
                        }
                        else
                        {
                            m_loop.start = oldEnd;
                            m_loop.length = newStart - oldEnd;
                        }
                    }
                    else if (m_mouseMode == EditLoop::ResizeRight)
                    {
                        auto newEnd = uint32_t(Max(0, m_mouseLoop.GetDuration() + mouseDelta * numMillisecondsPerPixel));
                        if (newEnd < m_mouseLoop.start)
                        {
                            m_loop.start = newEnd;
                            m_loop.length = m_mouseLoop.start - newEnd;
                        }
                        else
                        {
                            m_loop.start = m_mouseLoop.start;
                            m_loop.length = newEnd - m_mouseLoop.start;
                        }
                    }
                }
            }
            else
                m_mouseMode = EditLoop::None;

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            auto color0 = ImGui::GetColorU32(ImGuiCol_Button);
            auto color1 = ImGui::GetColorU32(ImGuiCol_ButtonActive);
            auto* frames = m_frames.Items();
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
            for (auto loopPos : m_loops)
                drawList->AddLine(ImVec2(pos.x - offset + loopPos / numMillisecondsPerPixel, pos.y), ImVec2(pos.x - offset + loopPos / numMillisecondsPerPixel, pos.y + size.y), 0x8000ffff);
            if (m_silence < m_numSamples)
            {
                auto silence = uint32_t((m_silence * 1000ull) / m_replay->GetSampleRate());
                drawList->AddLine(ImVec2(pos.x - offset + silence / numMillisecondsPerPixel, pos.y), ImVec2(pos.x - offset + silence / numMillisecondsPerPixel, pos.y + size.y), 0x800000ff);
            }
            drawList->AddRectFilled(ImVec2(pos.x - offset + m_loop.start / numMillisecondsPerPixel, pos.y), ImVec2(pos.x - offset + m_loop.GetDuration() / numMillisecondsPerPixel, pos.y + size.y), 0x3fFFffFF);
            drawList->AddLine(ImVec2(pos.x - offset + m_loop.start / numMillisecondsPerPixel, pos.y), ImVec2(pos.x - offset + m_loop.start / numMillisecondsPerPixel, pos.y + size.y), 0x3fFFffFF);
            drawList->AddLine(ImVec2(pos.x - offset + m_loop.GetDuration() / numMillisecondsPerPixel, pos.y), ImVec2(pos.x - offset + m_loop.GetDuration() / numMillisecondsPerPixel, pos.y + size.y), 0x3fFFffFF);
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
        if (ImGui::Button(ImGuiIconMediaStop))
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
        if (ImGui::Button(m_isPlaying ? ImGuiIconMediaPause : ImGuiIconMediaPlay))
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

    void SongEndEditor::LoopUI()
    {
        auto width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 7;
        ImGui::SetNextItemWidth(0.33f * width);
        ImGui::DragUint("##StartLoop", &m_loop.start, 1000.0f, 0, 0, "Loop Start", ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
        int32_t milliseconds = m_loop.start % 1000;
        int32_t seconds = (m_loop.start / 1000) % 60;
        int32_t minutes = m_loop.start / 60000;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width * 0.55f / 6.0f);
        ImGui::DragInt("##StartMinutes", &minutes, 0.1f, 0, 65535, "%dm", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width * 0.55f / 6.0f);
        ImGui::DragInt("##StartSeconds", &seconds, 0.1f, 0, 59, "%ds", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width * 0.55f / 6.0f);
        ImGui::DragInt("##StartMilliseconds", &milliseconds, 1.0f, 0, 999, "%dms", ImGuiSliderFlags_AlwaysClamp);
        m_loop.start = uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds);
        ImGui::SameLine();

        // loop length
        ImGui::SetNextItemWidth(0.12f * width);
        ImGui::DragUint("##LengthLoop", &m_loop.length, 1000.0f, 0, 0, "Length", ImGuiSliderFlags_NoInput, ImVec2(0.0f, 0.5f));
        milliseconds = m_loop.length % 1000;
        seconds = (m_loop.length / 1000) % 60;
        minutes = m_loop.length / 60000;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width * 0.55f / 6.0f);
        ImGui::DragInt("##LengthMinutes", &minutes, 0.1f, 0, 65535, "%dm", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width * 0.55f / 6.0f);
        ImGui::DragInt("##LengthSeconds", &seconds, 0.1f, 0, 59, "%ds", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width * 0.55f / 6.0f);
        ImGui::DragInt("##LengthMilliseconds", &milliseconds, 1.0f, 0, 999, "%dms", ImGuiSliderFlags_AlwaysClamp);
        m_loop.length = uint32_t(minutes) * 60000 + uint32_t(seconds) * 1000 + uint32_t(milliseconds);
    }

    void SongEndEditor::ParamsUI()
    {
        const uint32_t numSamplesToAdd = m_replay->GetSampleRate() * 300;
        uint32_t numAllocatedSamples = m_samples.NumItems();
        uint32_t numSamples = m_numSamples;
        ImGui::BeginDisabled((numSamples + numSamplesToAdd) * m_samples.ItemSize<uint64_t>() > 0xffFFffFFull);
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
        ImGui::SameLine();
        if (ImGui::Button("A"))
            ImGui::OpenPopup("AutoLoop");
        if (ImGui::BeginPopup("AutoLoop"))
        {
            ImGui::SeparatorText("Loops");
            ImGui::BeginDisabled(!m_loop.IsValid());
            if (ImGui::Button("Plot end songs", ImVec2(-FLT_MIN, 0.0f)))
            {
                BuildLoops();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(m_silence >= m_numSamples);
            ImGui::SeparatorText("Silence/End of Song");
            char txt[64] = "Not found###Silence";
            if (m_silence < m_numSamples)
            {
                uint32_t silence = uint32_t((m_silence * 1000ull) / m_replay->GetSampleRate());
                sprintf(txt, "Start at %u:%02u:%03u###Silence", silence / 60000, (silence / 1000) % 60, silence % 1000);
            }
            if (ImGui::Button(txt, ImVec2(-FLT_MIN, 0.0f)))
            {
                m_loop = { 0, uint32_t((m_silence * 1000ull) / m_replay->GetSampleRate()) };
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SeparatorText("Loop Length Params");
            ImGui::DragUint("##downsampleFactor", &m_loopDetection.downsampleFactor, 0.05f, kMinDownsampleFactor, 16, "x%u downsample", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragUint("##LoopMin", &m_loopDetection.loopMin, 0.05f, 1, m_loopDetection.loopMax, "%us loop min", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragUint("##LoopMax", &m_loopDetection.loopMax, 0.05f, m_loopDetection.loopMin, m_numSamples / (5 * m_replay->GetSampleRate() / 2), "%us loop max", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("##peakThreshold", &m_loopDetection.peakThreshold, 0.001f, 0.45f, 0.65f, "%.2f peak threshold", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("##consistencyThreshold", &m_loopDetection.consistencyThreshold, 0.001f, 0.35f, 0.55f, "%.2f consistency threshold", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SeparatorText("Loop Start Params");
            ImGui::DragUint("##LoopStart", &m_loopDetection.loopStart, 0.05f, 0, m_numSamples / m_replay->GetSampleRate(), "%us loop start", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragInt("##FRAME", &m_loopDetection.frame, 10.0f, 256, 32768, "%u Frame size", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragInt("##HOP", &m_loopDetection.hop, 1.0f, 64, 4096, "%u HOP length", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragInt("##MEL", &m_loopDetection.melFilters, 0.05f, 4, 64, "%u Mel Filters", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragInt("##MFCCS", &m_loopDetection.mfccs, 0.05f, 4, 32, "%u MFCC coefs", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SeparatorText("Loop Detection");
            ImGui::BeginDisabled(IsBusy());
            if (ImGui::Button("Find loop", ImVec2(-FLT_MIN, 0.0f)))
            {
                m_busySpinner.New(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                Core::AddJob([this]()
                {
                    auto loop = FindLoop();
                    Core::FromJob([this, loop]()
                    {
                        m_loop = loop;
                        BuildLoops();
                        m_busySpinner.Reset();
                    });
                });
                
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
        else if (ImGui::IsItemHovered())
            ImGui::Tooltip("Detect Loop\nOr Silence");
    }
}
// namespace rePlayer