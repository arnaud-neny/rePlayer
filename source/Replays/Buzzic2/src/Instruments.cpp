#include "Instruments.h"

#include <math.h>

const float PI				= 3.1415926f;
const float PI_2			= 2*PI;

// ==================================================================

float my_fabs ( float v )
{
    return fabsf(v);
}
float my_sin ( float v )
{
    return sinf(v);
}
float pow2 ( float x )
{
    return powf(2.0f, x);
}
float Noise1 ( int x )
{
    x = (x<<13) ^ x;
    return (float)( 1.0f - ( (x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}
float my_sqrt ( float v )
{
    return sqrtf(v);
}
float Sawtooth ( float x )
{
    x /= PI_2;
    return fmodf(x, 1.0f);
}
float LowPassResonantFilter ( float x, float f, float q, float* coeff )
{
    float fb = q + q / ( 1.0f - f );
    coeff[0] = coeff[0] + f * ( x - coeff[0] + fb * ( coeff[0] - coeff[1] ) );
    coeff[1] = coeff[1] + f * ( coeff[0] - coeff[1] );
    return coeff[1];
}

// ==================================================================
// Converts note number to frequency (includes multiplication by 2*pi)
float NoteToFrequency ( char note  )
{
    return PI_2 * 440.0f * powf ( 2.0f, (note-45)/12.0f );
}

// ==================================================================
// Returns given value or reads value from byte stream at current position
char Instruments::GetFixedOrStreamValue ( char val )
{
    if ( val >= 0 )
        return val;
    // Ignore streamed values if not streaming
    if ( g_patternNo < 0 )
        return 0;
    val = -val;
    if ( val-1 >= g_byteSequencesCount )
        return 0;
    int k = g_byteSequences[val-1][g_patternNo];
    if ( k )
        return g_bytePatterns[k-1][g_patternRowNo];
    return 0;
}


// ==================================================================
// Fills buffer for one note
int Instruments::FillNoteBuffer ( float* data, int maxLenSamples, Instrument* ins, int rowPos )
{
//     EnterCriticalSection ( &g_stackCs );
    if ( !ins->programLen ) {
//         LeaveCriticalSection ( &g_stackCs );
        return 0;
    }
    g_patternNo = g_patternRowNo = -1;
    if ( rowPos >= 0 ) {
        g_patternNo = ( rowPos >> 4 );
        g_patternRowNo = ( rowPos & 0xF );
    }
    int noteLenSamples = (int)GetFixedOrStreamValue ( ins->noteLen ) * ROW_SIZE_SAMPLES / 8;
    int soundLen = noteLenSamples;
    float insVolume = GetFixedOrStreamValue ( ins->volume ) * 0.01f;
    // Channels
    for ( int ch = 0; ch < 2; ch++ ) {
        char* ip = ins->stack;
        // Process stack
        char note = GetFixedOrStreamValue ( ins->note );
        if ( !note ) {
//             LeaveCriticalSection ( &g_stackCs );
            return 0;
        }
        float *b = data + ch;

        // Zero filter memory
        float stack[128], tmp1, tmp2;
        float filterData[256];
        for ( int i1 = 0; i1 < sizeof(filterData)/sizeof(float); i1++ ) filterData[i1] = 0;
        float tStart = (float)(rowPos*ROW_SIZE_SAMPLES) / SAMPLE_FREQUENCY;
        // Generate sample
        for ( int s = 0; s < core::Min(noteLenSamples, maxLenSamples); s++, b += 2 ) {
            float tRel = (float) s / noteLenSamples;
            float tGlobal = (float)s / SAMPLE_FREQUENCY;
            float tGlobalSt = tGlobal + tStart;
            float* sp = stack;
            int pp = 0;
            float *curFilt = filterData;
            while ( pp < ins->programLen ) {
                char op = ip[pp];
                if ( OP_PUSH_STREAM == op ) { *(sp++) = GetFixedOrStreamValue ( ip[pp+1] ) * 0.01f; pp += 2; }
                if ( OP_PUSH_CONST == op ) { *(sp++) = *((float*)(ip + pp + 1)); pp += 5; }
                if ( OP_PUSH_NOTE == op ) { *(sp++) = NoteToFrequency ( note + ip[pp+1] ); pp += 2; }
                if ( OP_PUSH_TIME == op ) { *(sp++) = tRel; pp++; }
                if ( OP_EXP == op ) { tmp1 = *(--sp); *(sp++) = pow2 ( tmp1 ); pp++; }
                if ( OP_SIN == op ) { tmp1 = *(--sp); tmp2 = *(--sp); *(sp++) = my_sin ( tmp1 * tGlobal + tmp2 ); pp++; }
                if ( OP_SIN_G == op ) { tmp1 = *(--sp); tmp2 = *(--sp); *(sp++) = my_sin ( tmp1 * tGlobalSt + tmp2 ); pp++; }
                if ( OP_SAW == op ) { tmp1 = *(--sp); tmp2 = *(--sp); *(sp++) = Sawtooth ( tmp1 * tGlobal + tmp2 ); pp++; }
                if ( OP_SAW_G == op ) { tmp1 = *(--sp); tmp2 = *(--sp); *(sp++) = Sawtooth ( tmp1 * tGlobalSt + tmp2 ); pp++; }
                if ( OP_NOISE == op ) { *(sp++) = Noise1 ( s ); pp++; }
                if ( OP_MUL == op ) { tmp1 = *(--sp); tmp2 = *(--sp); *(sp++) = tmp1 * tmp2; pp++; }
                if ( OP_ADD == op ) { tmp1 = *(--sp); tmp2 = *(--sp); *(sp++) = tmp1 + tmp2; pp++; }
                if ( OP_PAN == op ) { tmp1 = *(--sp); tmp2 = *(--sp); *(sp++) = tmp2 * (ch ? tmp1 : my_sqrt(1.0f-tmp1*tmp1)); pp++; }
                if ( OP_PUSH_CURRENT == op ) { *(sp++) = *b; pp++; }
                if ( OP_COMPRESS == op ) {
                    float l1 = ip[pp+1]*0.01f, l2 = ip[pp+2]*0.01f;
                    tmp1 = *(--sp);
                    float v = my_fabs(tmp1) - l1;
                    if ( v > 0.0 ) {
                        v = l2 - (l2-l1) / ( 1.0f + v / (l2-l1) );
                        if ( tmp1 > 0.0 )
                            tmp1 = v;
                        else
                            tmp1 = -v;
                    }
                    *(sp++) = tmp1;
                    pp += 3;
                }
                if ( OP_ADSR == op ) {
                    float t1 = ip[pp+1] * 0.01f;
                    float t2 = ip[pp+2] * 0.01f;
                    float t3 = ip[pp+3] * 0.01f;
                    float l = ip[pp+4] * 0.01f;
                    float adsr;
                    if ( tRel < t1 )
                        adsr = tRel / t1;
                    else if ( tRel < t2 )
                        adsr = 1.0f - ( 1.0f - l ) * ( tRel - t1 ) / ( t2 - t1 );
                    else if ( tRel < t3 )
                        adsr = l;
                    else
                        adsr = l * ( 1.0f - tRel  ) / ( 1.0f - t3 );
                    *(sp++) = *(--sp) * adsr;
                    pp += 5;
                }
                if ( OP_LP_FILTER == op || OP_HP_FILTER == op || OP_BP_FILTER == op ) {
                    tmp1 = *(--sp) / (0.35f*PI*SAMPLE_FREQUENCY);	// Frequency (*0.35 for simple filter only)
                    tmp2 = *(--sp);									// Quality
                    float in = *(--sp);
                    // Moog filter coefficients
                    /*float f, p, q;
                    float t1, t2;
                    q = 1.0f - tmp1;
                    p = tmp1 + 0.8f * tmp1 * q;
                    f = p + p - 1.0f;
                    q = tmp2 * (1.0f + 0.5f * q * (1.0f - q + 5.6f * q * q));
                    // Filter
                    in -= q * curFilt[4];                        // feedback
                    t1 = curFilt[1];	curFilt[1] = (in + curFilt[0]) * p - curFilt[1] * f;
                    t2 = curFilt[2];	curFilt[2] = (curFilt[1] + t1) * p - curFilt[2] * f;
                    t1 = curFilt[3];	curFilt[3] = (curFilt[2] + t2) * p - curFilt[3] * f;
                                        curFilt[4] = (curFilt[3] + t1) * p - curFilt[4] * f;
                    curFilt[4] = curFilt[4] - curFilt[4] * curFilt[4] * curFilt[4] * 0.166667f;    // clipping
                    curFilt[0] = in;
                    float y = curFilt[4];
                    if ( OP_HP_FILTER == op )
                        y = in - y;
                    if ( OP_BP_FILTER == op )
                        y = 3.0f * (curFilt[3] - y);*/
                    // Simple LP/HP filter
                    float y = LowPassResonantFilter ( in, tmp1, tmp2, curFilt );
                    if ( OP_HP_FILTER == op )
                        y = in - y;
                    /*if ( OP_BP_FILTER == op )
                        y = y - LowPassResonantFilter ( y, tmp1, tmp2, curFilt + 2 );*/
                    *(sp++) = y;
                    pp++;
                    //curFilt += 6;
                    curFilt += 2;
                }
                if ( OP_ECHO == op ) {
                    float t1 = ip[pp+1] * 0.01f;
                    float y = (*(--sp))*insVolume;
                    float yold = y;
                    float *bb1 = b;
                    int delay = (ROW_SIZE_SAMPLES/8) * 2 * ip[pp+2];
                    bool pan = false;
                    if ( delay < 0 ) {
                        pan = true;
                        delay *= -1;
                    }
                    for ( int k = 0; k < ip[pp+3]; k++ ) {
                        bb1 += delay;
                        y *= t1;
                        *bb1 += ( pan ? y*((k+0)%2 == ch) : y );
                        if ( (bb1 - data)/2 > soundLen )
                            soundLen = int((bb1 - data)/2);
                    }
                    *(sp++) = yold;
                    pp += 4;
                }
            }
            // Write sample out
            *b += (*(--sp))*insVolume;
            if ( sp != stack ) {
//                 LeaveCriticalSection ( &g_stackCs );
//                 MessageBoxA ( NULL, "STACK CORRUPTED!!", "Filling sound buffer", MB_OK | MB_ICONERROR );
                return 0;
            }
        }
    }
//     LeaveCriticalSection ( &g_stackCs );
    return soundLen;
} // FillNoteBuffer
