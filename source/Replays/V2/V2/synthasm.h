#pragma once

#include "scriptasm.h"

namespace V2
{
    struct SYN;

    class SynthAsm : public ScriptAsm
    {
    public:
        SynthAsm();
        ~SynthAsm();

        void synthInit(const void* patchmap, int samplerate);

        void synthRender(void* buf, int smp, void* buf2, int add);
        void synthProcessMIDI(const void* ptr);

        void synthSetGlobals(const void* ptr);

        void synthGetPoly(void* dest);
        void synthGetPgm(void* dest);
        uint32_t synthGetFrameSize();
        void* synthGetSpeechMem();

    private:
        void fastatan();
        void fastsinrc();
        void fastsin();
        void calcNewSampleRate();
        void calcfreq2();
        void calcfreq();
        void pow2();
        void pow();

        void syOscInit();
        void syOscNoteOn();
        void syOscChgPitch();
        void syOscSet();
        void syOscRender();

        void syEnvInit();
        void syEnvSet();
        void syEnvTick();
        void syEnvTick_state_off();
        void syEnvTick_state_atk();
        void syEnvTick_state_dec();
        void syEnvTick_state_sus();
        void syEnvTick_state_rel();

        void syFltInit();
        void syFltSet();
        void syFltRender();
        void syFltRender_process();
        void syFltRender_mode0();
        void syFltRender_mode1();
        void syFltRender_mode2();
        void syFltRender_mode3();
        void syFltRender_mode4();
        void syFltRender_mode5();
        void syFltRender_processmoog();
        void syFltRender_modemlo();
        void syFltRender_modemhi();

        void syLFOInit();
        void syLFOSet();
        void syLFOKeyOn();
        void syLFOTick();
        void syLFOTick_mode0();
        void syLFOTick_mode1();
        void syLFOTick_mode2();
        void syLFOTick_mode3();
        void syLFOTick_mode4();

        void syDistInit();
        void syDistSet();
        void syDistSet_mode0();
        void syDistSet_mode1();
        void syDistSet_mode2();
        void syDistSet_mode2b();
        void syDistSet_mode3();
        void syDistSet_mode4();
        void syDistSet_modeF();
        void syDistRenderMono();
        void syDistRenderMono_mode0();
        void syDistRenderMono_mode1();
        void syDistRenderMono_mode2();
        void syDistRenderMono_mode3();
        void syDistRenderMono_mode4();
        void syDistRenderMono_modeF();
        void syDistRenderStereo();
        void syDistRenderStereo_mode4();
        void syDistRenderStereo_modeF();

        void syDCFInit();
        void syDCFRenderMono();
        void syDCFRenderStereo();

        void syV2Init();
        void syV2Tick();
        void syV2Render();
        void syV2Set();
        void syV2NoteOn();
        void syV2NoteOff();
        void storeV2Values();

        void syBoostInit();
        void syBoostSet();
        void syBoostProcChan();
        void syBoostRender();

        void syModDelInit();
        void syModDelSet();
        void syModDelProcessSample();
        void syModDelRenderAux2Main();
        void syModDelRenderChan();

        void syCompInit();
        void syCompSet();
        void syCompLDMonoPeak();
        void syCompLDMonoRMS();
        void syCompLDStereoPeak();
        void syCompLDStereoRMS();
        void syCompProcChannel();
        void syCompRender();

        void syReverbInit();
        void syReverbReset();
        void syReverbSet();
        void syReverbProcess();

        void syChanInit();
        void syChanSet();
        void syChanProcess();
        void storeChanValues();

        void syRonanInit();
        void syRonanNoteOn();
        void syRonanNoteOff();
        void syRonanTick();
        void syRonanProcess();

        void RenderBlock();
        void vumeter();

        void ProcessNoteOff();
        void ProcessNoteOn();
        void ProcessChannelPressure();
        void ProcessControlChange();
        void ProcessProgramChange();
        void ProcessPitchBend();
        void ProcessPolyPressure();
        void ProcessRealTime();

    private:
        union
        {
            uint64_t m_this;
            SYN* m_syn;
        };
        uint8_t m_temp[16 * 4];
        uint32_t m_oldfpcw;
        uint32_t m_todo[2];
        uint64_t m_outptr[2];
        uint32_t m_addflg;

        float m_SRfcobasefrq;       // C - 3: 130.81278265029931733892499676165 Hz * 2 ^ 32 / SR
        float m_SRfclinfreq;        // 44100hz / SR
        float m_SRfcBoostSin;       // sin of 150hz / SR
        float m_SRfcBoostCos;       // cos of 150hz / SR
        uint32_t m_SRcFrameSize;
        float m_SRfciframe;
        float m_SRfcdcfilter;

        float m_fci12{ 0.083333333333f };
        float m_fc2{ 2.0f };
        float m_fc3{ 3.0f };
        float m_fc6{ 6.0f };
        float m_fc15{ 15.0f };
        float m_fc32bit{ 2147483647.0f };
        float m_fci127{ 0.007874015748f };
        float m_fci128{ 0.0078125f };
        float m_fci6{ 0.16666666666f };
        float m_fci4{ 0.25f };
        float m_fci2{ 0.5f };
        float m_fcpi_2{ 1.5707963267948966192313216916398f };
        float m_fcpi{ 3.1415926535897932384626433832795f };
        float m_fc1p5pi{ 4.7123889803846898576939650749193f };
        float m_fc2pi{ 6.28318530717958647692528676655901f };
        float m_fc64{ 64.0f };
        float m_fc32{ 32.0f };
        float m_fci16{ 0.0625f };
        float m_fcipi_2{ 0.636619772367581343075535053490057f };
        float m_fc32768{ 32768.0f };
        float m_fc256{ 256.0f };
        float m_fc10{ 10.0f };
        float m_fci32768{ 0.000030517578125f };
        float m_fc1023{ 1023.0f };
        float m_fcm12{ -12.0f };
        float m_fcm16{ -16.0f };
        float m_fci8192{ 0.0110485434560398050687631931578883f };
        float m_fci64{ 0.015625f };
        float m_fc48{ 48.0f };
        float m_fcdcflt{ 126.0f };
        float m_fc0p8{ 0.8f };
        float m_fc5p6{ 5.6f };

        float m_fccfframe{ 11.0f }; // for calcfreq2

        float m_fcOscPitchOffs{ 60.0f };
        float m_fcfmmax{ 2.0f };
        float m_fcattackmul{ -0.09375f }; // -0.0859375
        float m_fcattackadd{ 7.0f };
        float m_fcsusmul{ 0.0019375f };
        float m_fcgain{ 0.6f };
        float m_fcgainh{ 0.6f };
        float m_fcmdlfomul{ 1973915.486621315192743764172335601f };
        float m_fcsamplesperms{ 44.1f };
        float m_fccrelease{ 0.01f };
        float m_fccpdfalloff{ 0.9998f };

        float m_fcvufalloff{ 0.9999f };

        double m_fcsinx3{ -0.16666 };
        double m_fcsinx5{ 0.0083143 };
        double m_fcsinx7{ -0.00018542 };
        // r(x) = (x + 0.43157974 * x ^ 3) / (1 + 0.76443945 * x ^ 2 + 0.05831938 * x ^ 4)
        // second set of coeffs is for r(1 / x)
        double m_fcatanx1[2]{ 1.0, -0.431597974 };
        double m_fcatanx3[2]{ 0.43157974, -1.0 };
        double m_fcatanxm0[2]{ 1.0, 0.05831938 };
        double m_fcatanxm4[2]{ 0.05831938, 1.0 };
        double m_fcatanxm2{ 0.76443945 };
        double m_fcatanadd[2]{ 0.0, 1.5707963267948966192313216916398 };

        float m_fcoscbase{ 261.6255653005986346778499935233f };
        float m_fcsrbase{ 44100.0f };
        float m_fcboostfreq{ 150.0f };
        float m_fcframebase{ 128.0f };

        float m_fcdcoffset{ 0.000003814697265625f }; // 2 ^ -18

        uint32_t m_oscseeds[3]{ 0xdeadbeef, 0xbaadf00d, 0xd3adc0de };

        uint8_t* m_mem{ nullptr };
    };
}
//namespace V2