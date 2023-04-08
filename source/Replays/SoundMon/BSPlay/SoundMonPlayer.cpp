#include "SoundMonMixer.h"
#include "SoundMonModule.h"
#include "SoundMonPlayer.h"

#include <Containers/Array.h>

#include <algorithm>

using namespace core;

namespace SoundMon
{
    int16_t Player::ms_bpper[] = { 6848,6464,6080,5760,5440,5120,4832,4576,4320,4064,3840,3616,
                                 3424,3232,3040,2880,2720,2560,2416,2288,2160,2032,1920,1808,
                                 1712,1616,1520,1440,1360,1280,1208,1144,1080,1016,960,904,
                                 856,808,760,720,680,640,604,572,540,508,480,452,
                                 428,404,380,360,340,320,302,286,270,254,240,226,
                                 214,202,190,180,170,160,151,143,135,127,120,113,
                                 107,101,95,90,85,80,76,72,68,64,60,57 };

    int16_t Player::ms_vibtable[] = { 0,64,128,64,0,-64,-128,-64 };

    Player::Player(Module* modptr, uint32_t samplerate)
        : m_modptr(modptr)
        , m_mixer(new Mixer(modptr))
    {
        m_mixer->Initialize(kVoices, samplerate);
        m_mixer->SetPan(0, true);
        m_mixer->SetPan(1, false);
        m_mixer->SetPan(2, false);
        m_mixer->SetPan(3, true);

        BuildSubsongs();
    }

    Player::~Player()
    {
        delete m_mixer;
        delete[] m_subsongs;
    }

    void Player::Stop()
    {
        m_patptr = 0;
        m_stepptr = m_subsongs[m_subsongptr * 2];
        m_speed = 6;
        m_count = 0;
        m_arpptr = 0;
        m_vibrptr = 0;
        for (auto& bpcurrent : m_bpcurrent)
            bpcurrent = {};
        m_mixer->Stop();
        m_loop = false;
    }

    void Player::SetStep(int step)
    {
        if (m_modptr == nullptr)
            return;
        if (step >= m_modptr->GetNumSteps() || step < 0)
            return;
        m_stepptr = step;
    }

    void Player::IncStep()
    {
        if (m_modptr == nullptr)
            return;
        m_stepptr++;
        if (m_stepptr >= m_modptr->GetNumSteps())
            m_stepptr = 0;
    }

    void Player::DecStep()
    {
        if (m_modptr == nullptr)
            return;
        m_stepptr--;
        if (m_stepptr < 0)
            m_stepptr = m_modptr->GetNumSteps() - 1;
    }

    void Player::SetStereoSeparation(float ratio)
    {
        m_mixer->SetStereoSeparation(ratio);
    }

    void Player::SetSubsong(uint8_t subsong)
    {
        m_subsongptr = subsong;
        Stop();
    }

    uint32_t Player::GetDuration() const
    {
        uint32_t ticks{ 49 };

        int16_t count{ 0 };
        int16_t speed{ 6 };
        int32_t patptr{ 0 };
        int32_t stepptr = m_subsongs[m_subsongptr * 2];
        int32_t nextstep{ -1 };
        while (1)
        {
            ticks++;
            count++;
            if (count >= speed)
            {
                for (int16_t voice = 0; voice < kVoices; voice++)
                {
                    auto pattern = m_modptr->GetPattern(stepptr, voice);
                    auto option = m_modptr->GetFX(pattern, patptr);
                    if (option == 0x2)
                        speed = m_modptr->GetFXByte(pattern, patptr);
                    else if (option == 0x7)
                        nextstep = m_modptr->GetFXByte(pattern, patptr);
                }

                count = 0;
                patptr++;
                if (patptr == kNotesPerPattern || nextstep != -1)
                {
                    patptr = 0;
                    if (stepptr == m_subsongs[m_subsongptr * 2 + 1])
                        break;

                    if (nextstep != -1)
                    {
                        stepptr = nextstep;
                        nextstep = -1;
                    }
                    else
                        stepptr++;
                }
            }
        }
        return static_cast<uint32_t>(1000 * ticks / 50);
    }

    uint32_t Player::Render(float* data, uint32_t numsamples)
    {
        if (m_loop)
        {
            m_loop = false;
            return 0;
        }
        auto numsamplesleft = numsamples;
        while (numsamplesleft)
        {
            auto numfill = m_mixer->Fill(data, numsamplesleft);
            if (numfill == 0)
            {
                if (m_loop)
                    break;
                BpPlay();
                m_mixer->Mix();
            }
            else
            {
                data += numfill * 2;
                numsamplesleft -= numfill;
            }
        }
        return numsamples - numsamplesleft;
    }

    uint32_t Player::Seek(uint32_t timeInMs)
    {
        Stop();

        uint32_t pos = 0;
        uint32_t end = timeInMs * 50 / 1000;
        while (pos < end)
        {
            BpPlay();
            pos++;
        }
        return pos * 1000 / 50;
    }

    void Player::BpPlay()
    {
        BpFx();
        m_count++;
        if (m_count >= m_speed)
        {
            //ShowPos(m_stepptr, m_patptr);
            BpNext();
            BpProcess();
            m_count = 0;
            m_patptr++;
            if (m_patptr == kNotesPerPattern || m_nextstep != -1)
            {
                m_patptr = 0;
                m_loop = m_stepptr == m_subsongs[m_subsongptr * 2 + 1];

                if (m_nextstep != -1)
                {
                    if (m_nextstep >= m_modptr->GetNumSteps())
                        m_nextstep = m_subsongs[m_subsongptr * 2];
                    m_stepptr = m_nextstep;
                    m_nextstep = -1;
                }
                else if (m_loop)
                    m_stepptr = m_subsongs[m_subsongptr * 2];
                else
                    m_stepptr++;
            }
        }
    }

    void Player::BpFx()
    {
        m_arpptr++;
        if (m_arpptr == 3) m_arpptr = 0;
        m_vibrptr = (m_vibrptr + 1) & 7;

        for (int16_t voice = 0; voice < kVoices; voice++)
        {
            if (m_bpcurrent[voice].vibrato != 0)
                BpVibrato(voice);
            if (m_bpcurrent[voice].autoarpeggio != 0)
                BpArpeggio(voice, m_bpcurrent[voice].autoarpeggio);
            else if (m_bpcurrent[voice].option == 0 && m_bpcurrent[voice].optiondata != 0)
                BpArpeggio(voice, m_bpcurrent[voice].optiondata);
            if (m_bpcurrent[voice].autoslide != 0)
                BpAutoSlide(voice, m_bpcurrent[voice].autoslide);
            if (m_modptr->IsSynthInstrument(m_bpcurrent[voice].instrument))
                BpSynthFx(voice);
        }
    }

    void Player::BpNext()
    {
        uint16_t pattern;
        uint8_t note;
        uint8_t option;
        uint8_t optiondata;
        uint8_t instrument;
        uint16_t period;
        uint8_t transpose;
        uint8_t soundtranspose;

        for (int16_t voice = 0; voice < kVoices; voice++)
        {
            pattern = m_modptr->GetPattern(m_stepptr, voice);
            transpose = m_modptr->GetTR(m_stepptr, voice);
            soundtranspose = m_modptr->GetST(m_stepptr, voice);
            note = m_modptr->GetNote(pattern, m_patptr);
            option = m_modptr->GetFX(pattern, m_patptr);
            optiondata = m_modptr->GetFXByte(pattern, m_patptr);
            instrument = m_modptr->GetInstrument(pattern, m_patptr);

            if (transpose != 0 && note != 0 && (option != 0xA || (optiondata & 0xF0) == 0))
                note += transpose;
            if (soundtranspose != 0 && instrument != 0 && (option != 0xA || (optiondata & 0x0F) == 0))
                instrument += soundtranspose;
            if (note != 0)
                period = GetFreq(note);
            m_bpcurrent[voice].restart = false;
            if (note != 0)
            {
                if (instrument == 0)
                    instrument = m_bpcurrent[voice].instrument;
                m_bpcurrent[voice].autoslide = 0;
                m_bpcurrent[voice].autoarpeggio = 0;
                m_bpcurrent[voice].vibrato = 0;
                if (option != 0xF && option != 0xE && option != 0xD)
                {
                    m_bpcurrent[voice].restart = true;
                    m_mixer->Stop(voice);
                    m_bpcurrent[voice].modptr = 0;
                    m_bpcurrent[voice].adsrptr = 0;
                    m_bpcurrent[voice].lfoptr = 0;
                    m_bpcurrent[voice].egptr = 0;
                    m_bpcurrent[voice].adsrcount = 1;
                }
//                if (option==0xE || option==0xD)
//                {
//                    m_bpcurrent[voice].adsrptr=0;
//                    m_bpcurrent[voice].adsrcount=1;
//                }
            }
            m_bpcurrent[voice].newnote = false;
            if (note != 0)
            {
                m_bpcurrent[voice].newnote = true;
                m_bpcurrent[voice].note = note;
                m_bpcurrent[voice].period = period;
                m_bpcurrent[voice].instrument = instrument;
                m_bpcurrent[voice].volume = m_modptr->GetInstrumentVolume(instrument);
            }
            m_bpcurrent[voice].option = option;
            m_bpcurrent[voice].optiondata = optiondata;
            switch (option)
            {
            case 0x1:
                m_bpcurrent[voice].volume = optiondata;
                m_mixer->SetVolume(voice, optiondata);
                break;
            case 0x2:
                m_speed = optiondata;
                break;
            case 0x4:
                m_bpcurrent[voice].period -= optiondata;
                m_mixer->SetPeriod(voice, m_bpcurrent[voice].period);
                break;
            case 0x5:
                m_bpcurrent[voice].period += optiondata;
                m_mixer->SetPeriod(voice, m_bpcurrent[voice].period);
                break;
            case 0x6:
                m_bpcurrent[voice].vibrato = optiondata;
                m_bpcurrent[voice].autoslide = 0;
                m_bpcurrent[voice].autoarpeggio = 0;
                break;
            case 0x9:
                m_bpcurrent[voice].vibrato = 0;
                m_bpcurrent[voice].autoslide = 0;
                m_bpcurrent[voice].autoarpeggio = optiondata;
                break;
            case 0x8:
                m_bpcurrent[voice].autoslide = optiondata;
                m_bpcurrent[voice].autoarpeggio = 0;
                m_bpcurrent[voice].vibrato = 0;
                break;
            case 0x7:
                m_nextstep = optiondata;
                break;
            case 0xB:
                m_bpcurrent[voice].fxcontrol = optiondata;
                break;
            case 0xD:
                m_bpcurrent[voice].fxcontrol = m_bpcurrent[voice].fxcontrol ^ 1;
//                m_bpcurrent[voice].autoarpeggio=optiondata;
//                break;
            case 0xE:
                m_bpcurrent[voice].adsrptr = 0;
                m_bpcurrent[voice].adsrcount = 1;
                if (m_bpcurrent[voice].adsrcontrol == 0)
                    m_bpcurrent[voice].adsrcontrol = 1;
            case 0xF:
                m_bpcurrent[voice].autoarpeggio = optiondata;
                break;
            default:
                break;
            }
//            if (m_bpcurrent[voice].volume>64)
//                m_bpcurrent[voice].volume=64;

        }
    }

    void Player::BpProcess()
    {
        int16_t voice;
        uint16_t volume;
        uint16_t period = 0;
        uint16_t size, adsrsize;
        uint8_t* memptr;
        uint8_t egdelay, lfodelay, fxdelay, moddelay;
        uint8_t adsrtable, adsrspeed;

        for (voice = 0; voice < kVoices; voice++)
        {
            if (m_bpcurrent[voice].restart)
            {
                if (m_modptr->IsSynthInstrument(m_bpcurrent[voice].instrument))
                {
                    if (m_modptr->GetSynthInstrument(m_bpcurrent[voice].instrument, size, memptr))
                    {
                        // Make working copy of synth buffer
                        memcpy(m_synthbuf + voice * kSynthFxSize, memptr, kSynthFxSize);
                        // Update synth sound in mixer
                        m_mixer->Update(voice, m_synthbuf + voice * kSynthFxSize, 0, kSynthFxSize);
                        m_modptr->GetSynthDelays(m_bpcurrent[voice].instrument, egdelay, lfodelay, fxdelay, moddelay);
                        m_modptr->GetSynthControls(m_bpcurrent[voice].instrument,
                            m_bpcurrent[voice].adsrcontrol, m_bpcurrent[voice].lfocontrol,
                            m_bpcurrent[voice].egcontrol, m_bpcurrent[voice].fxcontrol,
                            m_bpcurrent[voice].modcontrol);

                        if (m_bpcurrent[voice].option != 0xE && m_bpcurrent[voice].option != 0xF)
                        {
                            m_bpcurrent[voice].egcount = egdelay + 1;
                            m_bpcurrent[voice].modcount = moddelay + 1;
                            m_bpcurrent[voice].lfocount = lfodelay + 1;
                            m_bpcurrent[voice].fxcount = fxdelay + 1;
                        }
                        m_modptr->GetSynthADSRData(m_bpcurrent[voice].instrument,
                            adsrtable, adsrsize, adsrspeed);
                        if (m_bpcurrent[voice].option != 0xF)
                            m_bpcurrent[voice].adsrcount = adsrspeed;
                        if (m_bpcurrent[voice].adsrcontrol != 0)
                            volume = CalcVolume(m_bpcurrent[voice].volume, m_modptr->GetTableValue(adsrtable, 0));
                        else
                            volume = m_bpcurrent[voice].volume;
                    }
                }
                else
                    volume = m_bpcurrent[voice].volume;
                if (m_bpcurrent[voice].period != 0)
                    period = m_bpcurrent[voice].period;
                m_mixer->Play(voice, m_bpcurrent[voice].instrument, period, volume);
            }
            if (m_bpcurrent[voice].period != 0 && m_bpcurrent[voice].newnote)
            {
                m_mixer->SetPeriod(voice, m_bpcurrent[voice].period);
                m_bpcurrent[voice].newnote = false;
            }
        }
    }

    void Player::BpArpeggio(int16_t voice, uint8_t arpeggio)
    {
        uint8_t	note = 0;

        if (m_arpptr == 1)
            note = (arpeggio & 0x0F);
        else if (m_arpptr == 2)
            note = (arpeggio & 0xF0) >> 4;
        note += m_bpcurrent[voice].note;
        m_bpcurrent[voice].period = GetFreq(note);
        m_mixer->SetPeriod(voice, m_bpcurrent[voice].period);
    }

    void Player::BpAutoSlide(int16_t voice, uint8_t slide)
    {
        m_bpcurrent[voice].period += (int16_t)((char)slide);
        m_mixer->SetPeriod(voice, m_bpcurrent[voice].period);
    }

    void Player::BpVibrato(int16_t voice)
    {
        uint8_t note = m_bpcurrent[voice].note;
        uint16_t period;

        if (note == 0)
            return;
        period = GetFreq(note) + ms_vibtable[m_vibrptr] / m_bpcurrent[voice].vibrato;
        m_mixer->SetPeriod(voice, period);
    }

    void Player::BpSynthFx(int16_t voice)
    {
        bool updatesound = false;
        uint16_t tmpsize;
        uint8_t tmptable, tmpdepth, tmpspeed;
        uint8_t tmp;
        uint16_t period;
        int16_t i;

        // Start with ADSR processing, the easy one.....
        if (m_bpcurrent[voice].adsrcontrol != 0 &&
            m_modptr->GetSynthADSRData(m_bpcurrent[voice].instrument, tmptable, tmpsize, tmpspeed))
        {
            m_bpcurrent[voice].adsrcount--;
            if (m_bpcurrent[voice].adsrcount == 0)
            {
                m_bpcurrent[voice].adsrcount = tmpspeed;
                tmp = m_modptr->GetTableValue(tmptable, m_bpcurrent[voice].adsrptr);
                m_mixer->SetVolume(voice, CalcVolume(m_bpcurrent[voice].volume, tmp));
                m_bpcurrent[voice].adsrptr++;
                if (m_bpcurrent[voice].adsrptr == tmpsize)
                {
                    m_bpcurrent[voice].adsrptr = 0;
                    if (m_bpcurrent[voice].adsrcontrol == 1)
                        m_bpcurrent[voice].adsrcontrol = 0;
                }
            }
        }

        // Second in line... LFO processing
        if (m_bpcurrent[voice].lfocontrol != 0 &&
            m_modptr->GetSynthLFOData(m_bpcurrent[voice].instrument, tmptable, tmpsize, tmpspeed, tmpdepth))
        {
            m_bpcurrent[voice].lfocount--;
            if (m_bpcurrent[voice].lfocount == 0)
            {
                m_bpcurrent[voice].lfocount = tmpspeed;
                if (tmpdepth == 0)
                    tmpdepth = 1;
                period = m_bpcurrent[voice].period + (int16_t(m_modptr->GetTableValue(tmptable, m_bpcurrent[voice].lfoptr)) - 128) / tmpdepth;
                m_mixer->SetPeriod(voice, period);
                m_bpcurrent[voice].lfoptr++;
                if (m_bpcurrent[voice].lfoptr == tmpsize)
                {
                    m_bpcurrent[voice].lfoptr = 0;
                    if (m_bpcurrent[voice].lfocontrol == 1)
                        m_bpcurrent[voice].lfocontrol = 0;
                }
            }
        }

        // EG
        if (m_bpcurrent[voice].egcontrol != 0 &&
            m_modptr->GetSynthEGData(m_bpcurrent[voice].instrument, tmptable, tmpsize, tmpspeed))
        {
            m_bpcurrent[voice].egcount--;
            if (m_bpcurrent[voice].egcount == 0)
            {
                uint8_t* ptr = m_synthbuf + voice * kSynthFxSize;
                uint8_t* ttable;
                uint16_t size;

                m_bpcurrent[voice].egcount = tmpspeed;
                tmp = m_modptr->GetTableValue(tmptable, m_bpcurrent[voice].egptr);
                m_modptr->GetSynthInstrument(m_bpcurrent[voice].instrument, size, ttable);
                memcpy(ptr, ttable, kSynthFxSize); // restore original data
                ttable = ptr;
                for (i = 0; i < (tmp >> 3); i++)
                {
                    (*ptr) = 255 - (*ptr);
                    ptr++;
                }
                updatesound = true;
                m_bpcurrent[voice].egptr++;
                if (m_bpcurrent[voice].egptr == tmpsize)
                {
                    m_bpcurrent[voice].egptr = 0;
                    if (m_bpcurrent[voice].egcontrol == 1)
                        m_bpcurrent[voice].egcontrol = 0;
                }
            }
        }

        // FX
        if (m_bpcurrent[voice].fxcontrol != 0 && m_modptr->GetSynthFXData(m_bpcurrent[voice].instrument, tmptable, tmpspeed))
        {
            m_bpcurrent[voice].fxcount--;
            if (m_bpcurrent[voice].fxcount == 0)
            {
                m_bpcurrent[voice].fxcount = 1;
                updatesound = true;
                switch (m_bpcurrent[voice].fxcontrol)
                {
                case 1:
                    BpAveraging(voice);
                    m_bpcurrent[voice].fxcount = tmpspeed;
                    break;
                case 2:
                    BpTransform(voice, tmptable, tmpspeed, true);
                    break;
                case 3:
                    BpTransform(voice, tmptable, tmpspeed, false);
                    break;
                case 4:
                    BpTransform(voice, tmptable + 1, tmpspeed, false);
                    break;
                case 5:
                    BpTransform(voice, tmptable, tmpspeed, false);
                    break;
                default:
                    break;
                }
            }
        }

        // MOD
        if (m_bpcurrent[voice].modcontrol != 0 &&
            m_modptr->GetSynthMODData(m_bpcurrent[voice].instrument, tmptable, tmpsize, tmpspeed))
        {
            m_bpcurrent[voice].modcount--;
            if (m_bpcurrent[voice].modcount == 0)
            {
                m_bpcurrent[voice].modcount = tmpspeed;
                uint8_t* ptr = m_synthbuf + voice * kSynthFxSize;
                *(ptr + kSynthFxSize - 1) = m_modptr->GetTableValue(tmptable, m_bpcurrent[voice].modptr);
                updatesound = true;
                m_bpcurrent[voice].modptr++;
                if (m_bpcurrent[voice].modptr == tmpsize)
                {
                    m_bpcurrent[voice].modptr = 0;
                    if (m_bpcurrent[voice].modcontrol == 1)
                        m_bpcurrent[voice].modcontrol = 0;
                }
            }
        }
        uint8_t* ptr = m_synthbuf + voice * kSynthFxSize;
        if (updatesound)
            m_mixer->Update(voice, ptr, 0, kSynthFxSize);

    }

    void Player::BpAveraging(int16_t voice)
    {
        uint8_t* ptr = m_synthbuf + voice * kSynthFxSize;
        uint8_t	lastval = *ptr;

        for (int16_t i = 1; i < kSynthFxSize - 1; i++)
        {
            ptr++;
            lastval = (uint16_t(lastval) + uint16_t(*(ptr + 1))) >> 1;
            *(ptr) = lastval;
        }
    }

    void Player::BpTransform(int16_t voice, uint8_t table, uint8_t delta, bool invert)
    {
        uint8_t* ptr = m_synthbuf + voice * kSynthFxSize;
        uint8_t	tmp;

        for (int16_t i = 0; i < kSynthFxSize; i++)
        {
            tmp = m_modptr->GetTableValue(table, i);
            if (invert)
                tmp = 255 - tmp;
            if ((*ptr) >= tmp)
                (*ptr) -= delta;
            else if ((*ptr) <= tmp)
                (*ptr) += delta;
            ptr++;
        }
    }

    void Player::BuildSubsongs()
    {
        int32_t stepptr = 0;
        Array<int32_t> steps;
        Array<uint16_t> subsong;
        Array<uint16_t> subsongs;

        while (stepptr != m_modptr->GetNumSteps())
        {
            if (!!steps.AddOnce(stepptr).second)
            {
                std::sort(steps.begin(), steps.end());
                subsong.Add(uint16_t(stepptr));
                int32_t newstepptr = -1;
                bool isblank = true;
                for (uint32_t patptr = 0; patptr < kNotesPerPattern && newstepptr == -1; ++patptr)
                {
                    for (int16_t voice = 0; voice < kVoices; voice++)
                    {
                        auto pattern = m_modptr->GetPattern(stepptr, voice);
                        auto transpose = m_modptr->GetTR(stepptr, voice);
                        auto soundtranspose = m_modptr->GetST(stepptr, voice);
                        auto note = m_modptr->GetNote(pattern, patptr);
                        auto option = m_modptr->GetFX(pattern, patptr);
                        auto optiondata = m_modptr->GetFXByte(pattern, patptr);
                        auto instrument = m_modptr->GetInstrument(pattern, patptr);
                        isblank &= transpose == 0 && soundtranspose == 0 && note == 0 && option == 0 && optiondata == 0 && instrument == 0;
                        if (option == 0x7)
                            newstepptr = m_modptr->GetFXByte(pattern, patptr);
                    }
                }
                if (isblank && subsong.NumItems() == 1)
                {
                    subsong.Pop();
                    stepptr = 0;
                }
                else if (newstepptr >= 0)
                {
                    if (newstepptr >= m_modptr->GetNumSteps())
                        newstepptr = subsong[0];
                    stepptr = newstepptr;
                }
                else
                    stepptr++;
            }
            else
            {
                if (subsong.IsNotEmpty())
                {
                    subsongs.Add(subsong[0]);
                    subsongs.Add(subsong.Last());
                    subsong.Clear();
                }

                stepptr = 0;
                for (auto step : steps)
                {
                    if (step == stepptr)
                        stepptr++;
                    else
                        break;
                }
            }
        }
        if (subsong.IsNotEmpty())
        {
            subsongs.Add(subsong[0]);
            subsongs.Add(subsong.Last());
        }

        m_numsubsongs = static_cast<uint8_t>(subsongs.NumItems() / 2);
        m_subsongs = new uint16_t[subsongs.NumItems()];
        memcpy(m_subsongs, subsongs.Items(), subsongs.Size());
    }
}
//namespace SoundMon