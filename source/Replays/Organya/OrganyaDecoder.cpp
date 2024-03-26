//
//  OrganyaDecoder.m
//  Organya
//
//  Created by Christopher Snowhill on 12/4/22.
//  Updated by Arnaud Neny on 23/3/24.
//

#include <IO/Stream.h>

#include "OrganyaDecoder.h"
#include "drums.h" // from orgmaker-3.08
#include "wavetable.h"

#include <map>
#include <vector>

/* SIMPLE CAVE STORY MUSIC PLAYER (Organya) */
/* Written by Joel Yliluoma -- http://iki.fi/bisqwit/ */
/* NX-Engine source code was used as reference.       */
/* Cave Story and its music were written by Pixel ( 天谷 大輔 ) */

namespace Organya
{
    //========= PART 0 : INPUT/OUTPUT AND UTILITY ========//

    int coggetc(core::io::Stream* fp) {
        uint8_t value;
        if(fp->Read(&value, sizeof(value)) != sizeof(value)) {
            return -1;
        }
        return value;
    }
    int coggetw(core::io::Stream* fp) { int a = coggetc(fp); int b = coggetc(fp); return (b<<8) + a; }
    int coggetd(core::io::Stream* fp) { int a = coggetw(fp); int b = coggetw(fp); return (b<<16) + a; }

    //========= PART 1 : SOUND EFFECT PLAYER (PXT) ========//

    static signed char Waveforms[6][256];
    static void GenerateWaveforms(void) {
        /* Six simple waveforms are used as basis for the signal generators in PXT: */
        for(unsigned seed=0, i=0; i<256; ++i) {
            /* These waveforms are bit-exact with PixTone v1.0.3. */
            seed = (seed * 214013) + 2531011; // Linear congruential generator
            Waveforms[0][i] = 0x40 * sin(i * 3.1416 / 0x80); // Sine
            Waveforms[1][i] = ((0x40+i) & 0x80) ? 0x80-i : i;     // Triangle
            Waveforms[2][i] = -0x40 + i/2;                        // Sawtooth up
            Waveforms[3][i] =  0x40 - i/2;                        // Sawtooth down
            Waveforms[4][i] =  0x40 - (i & 0x80);                 // Square
            Waveforms[5][i] = (signed char)(seed >> 16) / 2;      // Pseudorandom
        }
    }

    struct Pxt {
        struct Channel {
            bool enabled;
            int nsamples;

            // Waveform generator
            struct Wave {
                const signed char* wave;
                double pitch;
                int level, offset;
            };
            Wave carrier;   // The main signal to be generated.
            Wave frequency; // Modulator to the main signal.
            Wave amplitude; // Modulator to the main signal.

            // Envelope generator (controls the overall amplitude)
            struct Env {
                int initial;                    // initial value (0-63)
                struct { int time, val; } p[3]; // time offset & value, three of them
                int Evaluate(int i) const { // Linearly interpolate between the key points:
                    int prevval = initial, prevtime=0;
                    int nextval = 0,       nexttime=256;
                    for(int j=2; j>=0; --j) if(i < p[j].time) { nexttime=p[j].time; nextval=p[j].val; }
                    for(int j=0; j<=2; ++j) if(i >=p[j].time) { prevtime=p[j].time; prevval=p[j].val; }
                    if(nexttime <= prevtime) return prevval;
                    return (i-prevtime) * (nextval-prevval) / (nexttime-prevtime) + prevval;
                }
            } envelope;

            // Synthesize the sound effect.
            std::vector<int> Synth() const {
                if(!enabled) return {};
                std::vector<int> result(nsamples);

                auto& c = carrier, &f = frequency, &a = amplitude;
                double mainpos = c.offset, maindelta = 256*c.pitch/nsamples;
                for(size_t i=0; i<result.size(); ++i) {
                    auto s = [=](double p=1) { return 256*p*i/nsamples; };
                    // Take sample from each of the three signal generators:
                    int freqval = f.wave[0xFF & int(f.offset + s(f.pitch))] * f.level;
                    int ampval  = a.wave[0xFF & int(a.offset + s(a.pitch))] * a.level;
                    int mainval = c.wave[0xFF & int(mainpos)              ] * c.level;
                    // Apply amplitude & envelope to the main signal level:
                    result[i] = mainval * (ampval+4096) / 4096 * envelope.Evaluate(s()) / 4096;
                    // Apply frequency modulation to maindelta:
                    mainpos += maindelta * (1 + (freqval / (freqval<0 ? 8192. : 2048.)));
                }
                return result;
            }
        } channels[4]; /* Four parallel FM-AM modulators with envelope generators. */
    };

    static Pxt s_fx96 = {
        .channels = {
            { true, 5000,
                { Waveforms[0],  16.0, 33, 0 },
                { Waveforms[3],   1.0, 32, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 64, 63 }, { 166, 35 }, { 255, 0 } } } },
            { true, 1000,
                { Waveforms[5],   1.0, 16, 0 },
                { Waveforms[0],   1.0, 32, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 64, 63 }, { 91, 28 }, { 255, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } }
        }
    };
    static Pxt s_fx97 = {
        .channels = {
            { true, 5000,
                { Waveforms[0],  20.0, 30, 0 },
                { Waveforms[3],   1.0, 44, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 64, 63 }, { 111, 19 }, { 255, 0 } } } },
            { true, 10000,
                { Waveforms[5],  14.0, 41, 0 },
                { Waveforms[5],   3.0, 32, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 64, 18 }, { 91, 12 }, { 255, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } }
        }
    };
    static Pxt s_fx98 = {
        .channels = {
            { true, 1000,
                { Waveforms[5],  48.0, 30, 0 },
                { Waveforms[5],   1.0, 32, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 64, 63 }, { 166, 27 }, { 255, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } }
        }
    };
    static Pxt s_fx99 = {
        .channels = {
            { true, 10000,
                { Waveforms[5],  48.0, 30, 0 },
                { Waveforms[5],   1.0, 32, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 64, 43 }, { 166, 41 }, { 255, 7 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } }
        }
    };
    static Pxt s_fx9a = {
        .channels = {
            { true, 4000,
                { Waveforms[5],  35.0, 30, 0 },
                { Waveforms[3],  35.0, 32, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 53, 21 }, { 166, 13 }, { 255, 0 } } } },
            { true, 10000,
                { Waveforms[1],  63.0, 32, 0 },
                { Waveforms[3],   1.0, 32, 0 },
                { Waveforms[0],   0.0, 32, 0 },
                { 63, { { 64, 39 }, { 91, 20 }, { 255, 0 } } } },
            { false, 0,
                { Waveforms[0],   0.0,  0, 0 },
                { Waveforms[0],   0.0,  0, 0 },
                { Waveforms[0],   0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],   0.0,  0, 0 },
                { Waveforms[0],   0.0,  0, 0 },
                { Waveforms[0],   0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } }
        }
    };
    static Pxt s_fx9b = {
        .channels = {
            { true, 4000,
                { Waveforms[5],   6.0, 32, 0 },
                { Waveforms[3],   2.0, 32, 0 },
                { Waveforms[3],   2.0, 32, 0 },
                { 63, { { 26, 30 }, { 66, 29 }, { 255, 0 } } } },
            { true, 4000,
                { Waveforms[0], 150.0, 32, 0 },
                { Waveforms[0],   0.0,  0, 0 },
                { Waveforms[3],   2.0, 32, 0 },
                { 63, { { 26, 30 }, { 66, 29 }, { 255, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } },
            { false, 0,
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { Waveforms[0],  0.0,  0, 0 },
                { 0, { { 0, 0 }, { 0, 0 }, { 0, 0 } } } }
        }
    };

    unsigned char* s_drums[42] = {
        nullptr, // "Bass01",
        s_Bass02,
        nullptr, // "Snare01",
        s_Snare02,
        nullptr, // "Tom01",

        nullptr, // "HiClose",
        nullptr, // "HiOpen",
        s_Crash,
        nullptr, // "Per01",
        s_Per02,

        s_Bass03,
        s_Tom02,
        s_Bass04, //�V�K�ǉ�
        s_Bass05,
        s_Snare03,

        s_Snare04,
        s_HiClose2,
        s_HiOpen2,
        s_HiClose03,
        s_HiOpen03,

        s_Crash02,
        s_RevSym01,
        s_Ride01,
        s_Tom03,
        s_Tom04,

        s_OrcDrm01,
        s_Bell,
        s_Cat ,
        s_Bass06,		//����ɒǉ�
        s_Bass07,

        s_Snare05,
        s_Snare06,
        s_Snare07,
        s_Tom05,
        s_HiOpen04,

        s_HiClose04,
        s_Clap01,
        s_Pesi01,
        s_Quick01,
        s_Bass08 ,		//���肸�ɒǉ�	// 2011.10.17

        s_Snare08,
        s_HiClose05,
    };

    //========= PART 2 : SONG PLAYER (ORG) ========//
    /* Note: Requires PXT synthesis for percussion (drums). */

    static std::vector<short> DrumSamples[42];

    void LoadDrums(void) {
        GenerateWaveforms();
        /* List of PXT files containing these percussion instruments: */
        static const Pxt* patch[] = { &s_fx96, nullptr, &s_fx97, nullptr, &s_fx9a, &s_fx98, &s_fx99, nullptr, &s_fx9b, nullptr, nullptr, nullptr };
        for(unsigned drumno=0; drumno<12; ++drumno)
        {
            if(!patch[drumno]) continue; // Leave that non-existed drum file unloaded
            // Load the drum parameters
            auto& d = *patch[drumno];
            // Synthesize and mix the drum's component channels
            auto& sample = DrumSamples[drumno];
            for(auto& c: d.channels)
            {
                auto buf = c.Synth();
                if(buf.size() > sample.size()) sample.resize(buf.size());
                for(size_t a=0; a<buf.size(); ++a)
                    sample[a] += buf[a];
            }
        }
        /* convert all over drums samples from OrgMaker 3 */
        for (unsigned drumno = 0; drumno < 42; ++drumno)
        {
            if (!s_drums[drumno]) continue; // nothing to copy
            auto* riff = s_drums[drumno] + 8;
            while (riff[0] != uint8_t('d') || riff[1] != uint8_t('a') || riff[2] != uint8_t('t') || riff[3] != uint8_t('a'))
                riff++;
            // Rebuild samples
            auto& sample = DrumSamples[drumno];
            sample.resize(reinterpret_cast<uint32_t*>(riff)[1]);
            riff += 8;
            for (auto& s : sample)
            {
                s = *riff++ - 0x80;
            }
        }
    }

    struct Song {
        int ms_per_beat, samples_per_beat, loop_start, loop_end;
        int cur_beat, total_beats;
        int loop_count;
        struct Ins {
            int tuning, wave;
            bool pi; // true=all notes play for exactly 1024 samples.
            size_t n_events;

            struct Event { int note, length, volume, panning; };
            std::map<int/*beat*/, Event> events;

            // Volatile data, used & changed during playback:
            double phaseacc, phaseinc, cur_vol;
            int    cur_pan, cur_length, cur_wavesize;
            const short* cur_wave;
        } ins[16];

        uint32_t duration;
        double sampling_rate;
        int old_loop_count;
        int ver;

        bool Load(core::io::Stream* fp) {
            fp->Seek(0, core::io::Stream::kSeekBegin);
            char Signature[6];
            if(fp->Read(Signature, sizeof(Signature)) != sizeof(Signature))
                return false;
            ver = 0;
            if(memcmp(Signature, "Org-01", 6) == 0)
                ver = 1;
            else if(memcmp(Signature, "Org-02", 6) == 0)
                ver = 2;
            else if(memcmp(Signature, "Org-03", 6) == 0)
                ver = 3;
            if (ver == 0)
                return false;
            // Load song parameters
            ms_per_beat     = coggetw(fp);
            /*steps_per_bar =*/coggetc(fp); // irrelevant
            /*beats_per_step=*/coggetc(fp); // irrelevant
            loop_start      = coggetd(fp);
            loop_end        = coggetd(fp);
            // Load each instrument parameters (and initialize them)
            for(auto& i: ins)
                i = { coggetw(fp), coggetc(fp), coggetc(fp)!=0 && ver!=1, (unsigned)coggetw(fp),
                      {}, 0,0,0,0,0,0,0 };
            // Load events for each instrument
            for(auto& i: ins)
            {
                std::vector<std::pair<int,Ins::Event>> events( i.n_events );
                for(auto& n: events) n.first          = coggetd(fp);
                for(auto& n: events) n.second.note    = coggetc(fp);
                for(auto& n: events) n.second.length  = coggetc(fp);
                for(auto& n: events) n.second.volume  = coggetc(fp);
                for(auto& n: events) n.second.panning = coggetc(fp);
                i.events.insert(events.begin(), events.end());
            }

            return true;
        }

        void Reset(void) {
            cur_beat = 0;
            total_beats = 0;
            loop_count = old_loop_count = 0;
            for (auto& i : ins)
            {
                i.phaseacc = 0;
                i.phaseinc = 0;
                i.cur_vol = 0;
                i.cur_pan = 0;
                i.cur_length = 0;
                i.cur_wavesize = 0;
            }
        }

        std::vector<float> Synth()
        {
            // Determine playback settings:
            double samples_per_millisecond = sampling_rate * 1e-3, master_volume = 4e-6;
            int    samples_per_beat = ms_per_beat * samples_per_millisecond; // rounded.
            // Begin synthesis
            {
                if(cur_beat == loop_end) {
                    if (loop_count == old_loop_count)
                    {
                        old_loop_count++;
                        return {};
                    }
                    cur_beat = loop_start;
                    loop_count++;
                }
                // Synthesize this beat in stereo sound (two channels).
                std::vector<float> result( samples_per_beat * 2, 0.f );
                for(auto &i: ins)
                {
                    // Check if there is an event for this beat
                    auto j = i.events.find(cur_beat);
                    if(j != i.events.end())
                    {
                        auto& event = j->second;
                        if(event.volume  != 255) i.cur_vol = event.volume * master_volume;
                        if(event.panning != 255) i.cur_pan = event.panning;
                        if(event.note    != 255)
                        {
                            // Calculate the note's wave data sampling frequency (equal temperament)
                            double freq = pow(2.0, (event.note + i.tuning/1000.0 + 155.376) / 12);
                            // Note: 155.376 comes from:
                            //         12*log(256*440)/log(2) - (4*12-3-1) So that note 4*12-3 plays at 440 Hz.
                            // Note: Optimizes into
                            //         pow(2, (note+155.376 + tuning/1000.0) / 12.0)
                            //         2^(155.376/12) * exp( (note + tuning/1000.0)*log(2)/12 )
                            // i.e.    7901.988*exp(0.057762265*(note + tuning*1e-3))
                            i.phaseinc     = freq / sampling_rate;
                            i.phaseacc     = 0;
                            // And determine the actual wave data to play
                            i.cur_wave     = &WaveTable[256 * (i.wave % 100)];
                            i.cur_wavesize = 256;
                            i.cur_length   = i.pi ? 1024/i.phaseinc : (event.length * samples_per_beat);

                            if(&i >= &ins[8]) // Percussion is different
                            {
                                const auto& d = DrumSamples[i.wave % 42];
                                i.phaseinc = event.note * (22050/32.5) / sampling_rate; // Linear frequency
                                i.cur_wave     = &d[0];
                                i.cur_wavesize = (int) d.size();
                                i.cur_length   = d.size() / i.phaseinc;
                            }
                            // Ignore missing drum samples
                            if(i.cur_wavesize <= 0) i.cur_length = 0;
                        }
                    }

                    // Generate wave data. Calculate left & right volumes...
                    auto left  = (i.cur_pan > 6 ? 12 - i.cur_pan : 6) * i.cur_vol;
                    auto right = (i.cur_pan < 6 ?      i.cur_pan : 6) * i.cur_vol;
                    int n = samples_per_beat > i.cur_length ? i.cur_length : samples_per_beat;
                    for(int p=0; p<n; ++p)
                    {
                        double pos = i.phaseacc;
                        // Take a sample from the wave data.
                        /* We could do simply this: */
                        //int sample = i.cur_wave[ unsigned(pos) % i.cur_wavesize ];
                        /* But since we have plenty of time, use neat Lanczos filtering. */
                        /* This improves especially the low rumble noises substantially. */
                        enum { radius = 2 };
                        auto lanczos = [](double d) -> double
                        {
                            if(d == 0.) return 1.;
                            if(fabs(d) > radius) return 0.;
                            double dr = (d *= 3.14159265) / radius;
                            return sin(d) * sin(dr) / (d*dr);
                        };
                        double scale = 1/i.phaseinc > 1 ? 1 : 1/i.phaseinc, density = 0, sample = 0;
                        int min = -radius/scale + pos - 0.5;
                        int max =  radius/scale + pos + 0.5;
                        for(int m=min; m<max; ++m) // Collect a weighted average.
                        {
                            double factor = lanczos( (m-pos+0.5) * scale );
                            density += factor;
                            sample += i.cur_wave[m<0 ? 0 : m%i.cur_wavesize] * factor;
                        }
                        if(density > 0.) sample /= density; // Normalize
                        // Save audio in float32 format:
                        result[p*2 + 0] += sample * left;
                        result[p*2 + 1] += sample * right;
                        i.phaseacc += i.phaseinc;
                    }
                    i.cur_length -= n;
                }

                cur_beat++;

                return result;
            }
        }
    };

    // Need this static initializer to create the static global tables that sidplayfp doesn't really lock access to
    void initialize() {
        Organya::LoadDrums();
    }

    void release() {
        for (auto& d : DrumSamples)
            d = {};
    }

    Song* Load(core::io::Stream* stream, uint32_t sampleRate) {
        auto m_song = new Organya::Song;
        if(!m_song->Load(stream)) {
            delete m_song;
            return nullptr;
        }

        long beatsToEnd = m_song->loop_end;
        double lengthOfSong = ((double)m_song->ms_per_beat/* * 1e-3*/) * (double)beatsToEnd;
        m_song->duration = (uint32_t)lengthOfSong;
        m_song->sampling_rate = sampleRate;

        m_song->Reset();

        return m_song;
    }

    void Unload(Song* song) {
        if (song) {
            delete song;
        }
    }

    std::vector<float> Render(Song* song) {
        if (song)
            return song->Synth();
        return {};
    }

    void Reset(Song* song) {
        if (song) {
            song->Reset();
        }
    }

    uint32_t GetDuration(Song* song) {
        if (song)
            return song->duration;
        return 0;
    }

    int32_t GetVersion(Song* song) {
        if (song)
            return song->ver;
        return 0;
    }
}