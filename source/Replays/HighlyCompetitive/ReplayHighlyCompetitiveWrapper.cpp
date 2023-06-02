#include "snes9x/snes9x.h"
#include "snes9x/apu/apu.h"
#include "snes9x/apu/resampler.h"
#include "snes9x/memmap.h"

void S9xMessage(int type, int message_no, const char* str)
{
    (void)type;
    (void)message_no;
    (void)str;
}

bool8 S9xOpenSoundDevice(void)
{
    return TRUE;
}

static bool s_isInit = false;
static bool s_isRunning = false;

void Snes9xInit(const uint8_t* rom, size_t romSize, const uint8_t* sram, size_t sramSize)
{
    // ROM Options
    memset(&Settings, 0, sizeof(Settings));

    // Tracing options
    Settings.TraceDMA = false;
    Settings.TraceHDMA = false;
    Settings.TraceVRAM = false;
    Settings.TraceUnknownRegisters = false;
    Settings.TraceDSP = false;

    // ROM timing options (see also	H_Max above)
    Settings.PAL = false;
    Settings.FrameTimePAL = 20000;
    Settings.FrameTimeNTSC = 16667;
    Settings.FrameTime = 16667;

    // CPU options
    Settings.Paused = false;

    Settings.TakeScreenshot = false;

    // Sound options
    Settings.SoundSync = true;
    Settings.Mute = false;
    Settings.SoundPlaybackRate = 48000;
    Settings.SixteenBitSound = true;
    Settings.Stereo = true;
    Settings.ReverseStereo = false;
    Settings.InterpolationMethod = 2;
    Settings.DisableSurround = false;

    Memory.Init();

    S9xInitAPU();
    S9xInitSound(10);

    s_isInit = true;
    s_isRunning = false;

    if (Memory.LoadROMSNSF(rom, int32_t(romSize), sram, int32_t(sramSize)))
        S9xSetSoundMute(false);
}

void Snes9xRelease()
{
    if (s_isInit)
    {
        S9xReset();
        Memory.Deinit();
        S9xDeinitAPU();

        s_isInit = false;
    }
}

void Snes9xSetInterpolationMethod(int32_t interpostionMethod)
{
    Settings.InterpolationMethod = interpostionMethod;
}

void Snes9xRender(int16_t* buf, uint32_t numSamples)
{
    // if it's the first time we play, there can be some annoying silence at the beginning (boot?)
    // so skip it
    uint32_t numSilences = 0;
    while (!s_isRunning)
    {
        unsigned sampleCount = S9xGetSampleCount() >> 1;
        if (sampleCount == 0)
        {
            // 10 seconds max, just in case there is an issue
            if (numSilences >= 10 * Settings.SoundPlaybackRate)
                s_isRunning = true;
            S9xSyncSound();
            S9xMainLoop();
        }
        else
        {
            S9xMixSamples(reinterpret_cast<uint8_t*>(buf), 2);
            if (*reinterpret_cast<uint32_t*>(buf))
            {
                numSamples--;
                buf++;
                s_isRunning = true;
            }
            else
                numSilences++;
        }
    }

    // decode
    while (numSamples)
    {
        unsigned sampleCount = S9xGetSampleCount() >> 1;
        if (sampleCount == 0)
        {
            S9xSyncSound();
            S9xMainLoop();
        }
        else
        {
            if (sampleCount > numSamples)
                sampleCount = numSamples;

            S9xMixSamples(reinterpret_cast<uint8_t*>(buf), sampleCount << 1);
            buf += sampleCount << 1;

            numSamples -= sampleCount;
        }
    }
}