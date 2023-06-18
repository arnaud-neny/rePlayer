#ifndef __INSTRUMENTS_H__
#define __INSTRUMENTS_H__

#include <Core.h>

constexpr int MAX_SEQUENCES = 64;
constexpr int MAX_TEMPLATE_MELODY_LEN = 256;
constexpr int MAX_PATTERNS = 64;
constexpr int SAMPLE_FREQUENCY	= 44100;
constexpr int FREQ_ANALYZE_SAMPLES = 2048;
constexpr int MAX_OPERATIONS = 64;
constexpr int MAX_OPER_STACK_SIZE = 1024;

#include "Operations.h"

// Declares all instrument properties
struct Instrument
{
    // Display data
    char name[128];
    char mute;
    // Instrument description
    char note;
    char noteLen;
    char volume;
    // Operations
    uint32_t oper[MAX_OPERATIONS];//Operation* oper[MAX_OPERATIONS];
    int operCount;
    // Connections between operations
    OperConnection operConn[MAX_OPERATIONS*10];
    int operConnCount;
    // Operation stack
    char stack[MAX_OPER_STACK_SIZE];
    int programLen;
    char unused[128];
};

struct Instruments
{
    Instrument g_instruments[128];
    int g_instrumentCount;
    int ROW_SIZE_SAMPLES;

    int FillNoteBuffer(float* data, int maxLenSamples, Instrument* ins, int rowPos);
    char g_buf[1024];
    float g_masterVolume;

    // Sequences
    char g_bytePatterns[MAX_PATTERNS][16];
    short g_bitPatterns[MAX_PATTERNS];
    char g_byteSequences[MAX_SEQUENCES][MAX_TEMPLATE_MELODY_LEN];
    char g_bitSequences[MAX_SEQUENCES][MAX_TEMPLATE_MELODY_LEN];
    int g_bytePatternsCount;
    int g_byteSequencesCount;

    int g_patternNo;
    int g_patternRowNo;

    char GetFixedOrStreamValue(char val);
};

#endif  // __INSTRUMENTS_H__