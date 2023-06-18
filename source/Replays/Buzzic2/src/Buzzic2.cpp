#include <Core/String.h>

#include "Buzzic2.h"
#include "Instruments.h"

using namespace core;

// File structures
struct FileHeader
{
    int   magicWord;
    int   version;
    float masterVolume;
    int	  bpm;
    int   startSeq;
    int   endSeq;
    char  looping;
    char  unused[128];
};

struct FileMem
{
    const uint8_t* m_data;
    size_t m_size;
    int64_t m_pos = 0;

    bool read(void* buffer, size_t size)
    {
        auto sizeLeft = m_size - m_pos;
        auto hasFailed = size > sizeLeft;
        if (hasFailed)
            size = sizeLeft;
        memcpy(buffer, m_data + m_pos, size);
        m_pos += size;
        return hasFailed;
    }
    bool seek(size_t size)
    {
        auto sizeLeft = m_size - m_pos;
        auto hasFailed = size > sizeLeft;
        if (hasFailed)
            size = sizeLeft;
        m_pos += size;
        return hasFailed;
    }
};

struct Buzzic2 : public Instruments
{
    float masterVolume;
    int	  bpm;
    int   startSeq;
    int   endSeq;

    std::string byteSequenceNames[MAX_SEQUENCES];
    std::string bytePatternNames[MAX_PATTERNS];

    bool Load(const FileHeader& fh, FileMem f);
    ~Buzzic2();

    void GenerateRow();
    uint32_t Render(StereoSample* data, uint32_t numSamples);

    StereoSample* m_samples = nullptr;
    uint32_t m_numSamples;
    uint32_t m_availableSamples = 0;
    int m_currentSeq;
    bool m_hasLooped = false;
};

Buzzic2* Buzzic2Load(const uint8_t* data, size_t size)
{
    if (size < sizeof(FileHeader))
        return nullptr;

    auto* fh = reinterpret_cast<const FileHeader*>(data);
    if (fh->magicWord != '2zub' || (fh->version & 0xffFF) != 2)
    {
        return nullptr;
    }

    Buzzic2* buz = new Buzzic2;
    buz->masterVolume = fh->masterVolume;
    buz->bpm = fh->bpm;
    buz->startSeq = fh->startSeq;
    buz->endSeq = fh->endSeq;
    buz->m_currentSeq = fh->startSeq * 16;
    if (buz->Load(*fh, { data += sizeof(FileHeader), size - sizeof(FileHeader) }))
        return buz;
    delete buz;
    return nullptr;
}

void Buzzic2Release(Buzzic2* buzzic2)
{
    delete buzzic2;
}

void Buzzic2Reset(Buzzic2* buzzic2)
{
    memset(buzzic2->m_samples, 0, buzzic2->m_numSamples * sizeof(StereoSample));
    buzzic2->m_currentSeq = buzzic2->startSeq * 16;
    buzzic2->m_hasLooped = false;
    buzzic2->m_availableSamples = 0;
}

uint32_t Buzzic2DurationMs(struct Buzzic2* buzzic2)
{
    return uint32_t((buzzic2->endSeq - buzzic2->startSeq + 1) * 16 * 1000.0f * 60.0f / (buzzic2->bpm * 4.0f));
}

uint32_t Buzzic2NumIntruments(struct Buzzic2* buzzic2)
{
    return buzzic2->g_instrumentCount;
}

const char* Buzzic2IntrumentName(struct Buzzic2* buzzic2, uint32_t index)
{
    return buzzic2->g_instruments[index].name;
}

uint32_t Buzzic2Render(Buzzic2* buzzic2, StereoSample* samples, uint32_t numSamples)
{
    return buzzic2->Render(samples, numSamples);
}

bool Buzzic2::Load(const FileHeader& fh, FileMem f)
{
    f.read(&g_bytePatternsCount, sizeof(g_bytePatternsCount));
    f.read(&g_byteSequencesCount, sizeof(g_byteSequencesCount));
    int maxPatterns, maxSequences, maxTemplMelodyLen;
    if (((fh.version >> 16) & 0xffFF) >= 1)
    {
        f.read(&maxPatterns, sizeof(maxPatterns));
        f.read(&maxSequences, sizeof(maxSequences));
        f.read(&maxTemplMelodyLen, sizeof(maxTemplMelodyLen));
    }
    else
    {
        maxPatterns = 64;
        maxSequences = 64;
        maxTemplMelodyLen = 128;
    }
    char* pat = new char[maxPatterns * 16];
    char* seq = new char[maxSequences * maxTemplMelodyLen];
    f.read(pat, maxPatterns * 16);
    f.read(seq, maxSequences * maxTemplMelodyLen);
    memset(g_bytePatterns, 0, sizeof(g_bytePatterns));
    memset(g_byteSequences, 0, sizeof(g_byteSequences));
    memcpy(g_bytePatterns, pat, Min(maxPatterns, MAX_PATTERNS) * 16);
    for (int i = 0; i < Min(maxSequences, MAX_SEQUENCES); i++)
        for (int k = 0; k < Min(maxTemplMelodyLen, MAX_TEMPLATE_MELODY_LEN); k++)
            g_byteSequences[i][k] = *(seq + i * maxTemplMelodyLen + k);
    delete[] pat;
    delete[] seq;
    for (auto& byteSequenceName : byteSequenceNames)
    {
        memset(g_buf, 0, sizeof(g_buf));
        f.read(g_buf, 64);
        byteSequenceName = g_buf;
    }
    for (auto& bytePatternName : bytePatternNames)
    {
        memset(g_buf, 0, sizeof(g_buf));
        f.read(g_buf, 64);
        bytePatternName = g_buf;
    }
    // Load instruments
    f.read(&g_instrumentCount, sizeof(g_instrumentCount));
    for (int i = 0; i < g_instrumentCount; i++)
    {
        Instrument* ins = g_instruments + i;
        f.read(ins, sizeof(Instrument));
        for (int o = 0; o < ins->operCount; o++)
        {
            f.seek(sizeof(Operation));
        }
    }

    ROW_SIZE_SAMPLES = int(SAMPLE_FREQUENCY * 60.0f / (bpm * 4.0f));
    m_numSamples = ROW_SIZE_SAMPLES * 11 + 4 * SAMPLE_FREQUENCY;
    m_samples = new StereoSample[m_numSamples];
    memset(m_samples, 0, m_numSamples * sizeof(StereoSample) );

    return true;
}

Buzzic2::~Buzzic2()
{
    delete[] m_samples;
}

void Buzzic2::GenerateRow()
{
    auto* data = m_samples;

    memmove(data, data + ROW_SIZE_SAMPLES, (m_numSamples - ROW_SIZE_SAMPLES) * sizeof(StereoSample));
    memset(data + m_numSamples - ROW_SIZE_SAMPLES, 0, ROW_SIZE_SAMPLES * sizeof(StereoSample));

    auto r = m_currentSeq;
    for (int i = 0; i < g_instrumentCount; i++)
        if (!g_instruments[i].mute)
            FillNoteBuffer(reinterpret_cast<float*>(data), m_numSamples, g_instruments + i, r);

    m_currentSeq++;
    if (m_currentSeq > (endSeq + 1) * 16)
    {
        m_hasLooped = true;
        m_currentSeq = startSeq * 16;
    }
}

uint32_t Buzzic2::Render(StereoSample* data, uint32_t numSamples)
{
    auto remainingSamples = numSamples;
    while (remainingSamples)
    {
        if (m_availableSamples == 0)
        {
            if (m_hasLooped)
            {
                if (remainingSamples == numSamples)
                    m_hasLooped = false;
                return numSamples - remainingSamples;
            }
            GenerateRow();
            m_availableSamples = ROW_SIZE_SAMPLES;
        }
        auto samplesToCopy = core::Min(remainingSamples, m_availableSamples);
        memcpy(data, m_samples + ROW_SIZE_SAMPLES - m_availableSamples, samplesToCopy * sizeof(StereoSample));
        for (uint32_t i = 0; i < samplesToCopy; i++)
            *data++ = *data * masterVolume;

        m_availableSamples -= samplesToCopy;
        remainingSamples -= samplesToCopy;
    }
    return numSamples;
}
