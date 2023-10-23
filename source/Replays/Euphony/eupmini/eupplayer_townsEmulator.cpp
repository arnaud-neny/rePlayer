// $Id: eupplayer_townsEmulator.cc,v 1.24 2000/04/12 23:14:35 hayasaka Exp $

/*      Artistic Style
 *
 * ./astyle --style=stroustrup --convert-tabs --add-braces eupplayer_townsEmulator.cpp
 */

#include <cstdint>
#include <cstdio>
#include <sys/stat.h>
#include <cassert>
#include <cstring>
#include <cmath>
#if defined ( __MINGW32__ )
#include <_bsd_types.h>
#endif
#if defined ( _MSC_VER )
//#include <SDL.h>
inline void SDL_Delay(size_t) {}
#endif // _MSC_VER
#if defined ( __MINGW32__ )
#include <SDL2/SDL.h>
#endif // __MINGW32__
#if defined ( __GNUC__ ) && !defined ( __MINGW32__ )
#include <SDL2/SDL.h>
#endif // __GNUC__
#include "eupplayer_townsEmulator.hpp"
#include "sintbl.hpp"

#if EUPPLAYER_LITTLE_ENDIAN
static inline uint16_t P2(uint8_t const * const p)
{
    return *(uint16_t const *)p;
}
static inline uint32_t P4(uint8_t const * const p)
{
    return *(uint32_t const *)p;
}
#else
static inline uint16_t P2(uint8_t const * const p)
{
    uint16_t const x0 = *p;
    uint16_t const x1 = *(p + 1);
    return x0 + (x1 << 8);
}
static inline uint32_t P4(uint8_t const * const p)
{
    uint32_t const x0 = P2(p);
    uint32_t const x1 = P2(p + 2);
    return x0 + (x1 << 16);
}
#endif

#define DB(a)

#define CHECK_CHANNEL_NUM(funcname, channel) \
  if (channel < 0 || _maxChannelNum <= channel) { \
    return; \
  } \
  (void)false

/* TownsPcmSound */

class TownsPcmSound {
    char _name[9];
    int _id;
    int _numSamples;
    int _loopStart;
    int _loopLength;
    int _samplingRate;
    int _keyOffset;
    int _adjustedSamplingRate;
    int _keyNote;
    signed char *_samples;
public:
    TownsPcmSound(uint8_t const *p);
    ~TownsPcmSound();
    int id() const
    {
        return _id;
    }
    int numSamples() const
    {
        return _numSamples;
    }
    int loopStart() const
    {
        return _loopStart;
    }
    int loopLength() const
    {
        return _loopLength;
    }
    int samplingRate() const
    {
        return _samplingRate;
    }
    int keyOffset() const
    {
        return _keyOffset;
    }
    int adjustedSamplingRate() const
    {
        return _adjustedSamplingRate;
    }
    int keyNote() const
    {
        return _keyNote;
    }
    signed char const * samples() const
    {
        return _samples;
    }
};

TownsPcmSound::TownsPcmSound(uint8_t const *p)
{
    {
        u_int n = 0;
        for (; n < sizeof(_name)-1; n++) {
            _name[n] = char(p[n]);
        }
        _name[n] = '\0';
    }
    _id = int(P4(p+8));
    _numSamples = int(P4(p+12));
    _loopStart = int(P4(p+16));
    _loopLength = int(P4(p+20));
    _samplingRate = int(P2(p + 24));
    _keyOffset = int16_t(P2(p + 26));
    _adjustedSamplingRate = (_samplingRate + _keyOffset) * (1000 * 0x10000 / 0x62);
    _keyNote = int(*(p+28));
    _samples = new signed char[_numSamples + 1]; // append 1 sample, in order to avoid buffer overflow in liner interpolation process
    for (int i = 0; i < _numSamples; i++) {
        int n = int(p[32+i]);
        _samples[i] = (n >= 0x80) ? uint8_t((n & 0x7f)) : uint8_t(-n);
    }
    _samples[_numSamples] = _samples[_numSamples - 1];		// only for non-looping case where signal ends anyway
    if (_loopStart >= _numSamples) {
//         fprintf(stderr, "TownsPcmSound::TownsPcmSound: too large loopStart.  loopStart zeroed.  loopStart=0x%08x, numSamples=0x%08x\n", _loopStart, _numSamples);
        _loopStart = 0;
    }
    if (_loopLength > _numSamples - _loopStart) {
//         fprintf(stderr, "TownsPcmSound::TownsPcmSound: too large loopLength.  loop disabled.  loopStart=0x%08x, loopLength=0x%08x, numSamples=0x%08x\n", _loopStart, _loopLength, _numSamples);
        _loopLength = 0;
    }
    if (_loopLength != 0 && _loopStart + _loopLength < _numSamples) {
        _numSamples = _loopStart + _loopLength;
    }
    //cerr << this->describe() << '\n';
}

TownsPcmSound::~TownsPcmSound()
{
    if (_samples != nullptr) {
        delete _samples;
    }
}

/* TownsPcmEnvelope */

class TownsPcmEnvelope {
    enum State { _s_ready=0, _s_attacking, _s_decaying, _s_sustaining, _s_releasing };
    State _state;
    State _oldState;
    int _currentLevel;
    int _rate;
    int _tickCount;
    int _totalLevel;
    int _attackRate;
    int _decayRate;
    int _sustainLevel;
    int _sustainRate;
    int _releaseLevel;
    int _releaseRate;
    int _rootKeyOffset;
public:
    TownsPcmEnvelope(TownsPcmEnvelope const *e);
    TownsPcmEnvelope(uint8_t const *p);
    ~TownsPcmEnvelope();
    void start(int rate);
    void release();
    int nextTick();
    int state()
    {
        return (int)(_state);
    }
    int rootKeyOffset()
    {
        return _rootKeyOffset;
    }
};

TownsPcmEnvelope::TownsPcmEnvelope(TownsPcmEnvelope const *e)
{
    memcpy(this, e, sizeof(*this));
}

TownsPcmEnvelope::TownsPcmEnvelope(uint8_t const *p)
{
    _state = _s_ready;
    _oldState = _s_ready;
    _currentLevel = 0;
    _totalLevel = int(*(p+0));
    _attackRate = int(*(p+1)) * 10;
    _decayRate = int(*(p+2)) * 10;
    _sustainLevel = int(*(p+3));
    _sustainRate = int(*(p+4)) * 20;
    _releaseRate = int(*(p+5)) * 10;
    _rootKeyOffset = int(*(p+6));
    //cerr << this->describe() << '\n';
}

TownsPcmEnvelope::~TownsPcmEnvelope()
{
}

void TownsPcmEnvelope::start(int rate)
{
    _state = _s_attacking;
    _currentLevel = 0;
    _rate = rate;
    _tickCount = 0;
}

void TownsPcmEnvelope::release()
{
    if (_state == _s_ready) {
        return;
    }
    _state = _s_releasing;
    _releaseLevel = _currentLevel;
    _tickCount = 0;
}

int TownsPcmEnvelope::nextTick()
{
    if (_oldState != _state) {
//         switch (_oldState) {
//         default:
//             break;
//         };
        _oldState = _state;
    }
    switch (_state) {
    case _s_ready:
        return 0;
        break;
    case _s_attacking:
        if (_attackRate == 0) {
            _currentLevel = _totalLevel;
        }
        else if (_attackRate >= 1270) {
            _currentLevel = 0;
        }
        else {
            _currentLevel = (_totalLevel*(_tickCount++)*(int64_t)1000) / (_attackRate*_rate);
        }
        if (_currentLevel >= _totalLevel) {
            _currentLevel = _totalLevel;
            _state = _s_decaying;
            _tickCount = 0;
        }
        break;
    case _s_decaying:
        if (_decayRate == 0) {
            _currentLevel = _sustainLevel;
        }
        else if (_decayRate >= 1270) {
            _currentLevel = _totalLevel;
        }
        else {
            _currentLevel = _totalLevel;
            _currentLevel -= ((_totalLevel-_sustainLevel)*(_tickCount++)*(int64_t)1000) / (_decayRate*_rate);
        }
        if (_currentLevel <= _sustainLevel) {
            _currentLevel = _sustainLevel;
            _state = _s_sustaining;
            _tickCount = 0;
        }
        break;
    case _s_sustaining:
        if (_sustainRate == 0) {
            _currentLevel = 0;
        }
        else if (_sustainRate >= 2540) {
            _currentLevel = _sustainLevel;
        }
        else {
            _currentLevel = _sustainLevel;
            _currentLevel -= (_sustainLevel*(_tickCount++)*(int64_t)1000) / (_sustainRate*_rate);
        }
        if (_currentLevel <= 0) {
            _currentLevel = 0;
            _state = _s_ready;
            _tickCount = 0;
        }
        break;
    case _s_releasing:
        if (_releaseRate == 0) {
            _currentLevel = 0;
        }
        else if (_releaseRate >= 1270) {
            _currentLevel = _releaseLevel;
        }
        else {
            _currentLevel = _releaseLevel;
            _currentLevel -= (_releaseLevel*(_tickCount++)*(int64_t)1000) / (_releaseRate*_rate);
        }
        if (_currentLevel <= 0) {
            _currentLevel = 0;
            _state = _s_ready;
        }
        break;
    default:
        // ここには来ないはず
        break;
    };

    return _currentLevel;
}

/* TownsPcmInstrument */

class TownsPcmInstrument {
    enum { _maxSplitNum = 8, };
    char _name[9];
    int _split[_maxSplitNum];
    int _soundId[_maxSplitNum];
    TownsPcmSound const *_sound[_maxSplitNum];
    TownsPcmEnvelope *_envelope[_maxSplitNum];
public:
    TownsPcmInstrument(uint8_t const *p);
    ~TownsPcmInstrument();
    void registerSound(TownsPcmSound const *sound);
    TownsPcmSound const *findSound(int note) const;
    TownsPcmEnvelope const *findEnvelope(int note) const;
};

TownsPcmInstrument::TownsPcmInstrument(uint8_t const *p)
{
    {
        u_int n = 0;
        for (; n < sizeof(_name)-1; n++) {
            _name[n] = p[n];
        }
        _name[n] = '\0';
    }
    for (int n = 0; n < _maxSplitNum; n++) {
        _split[n] = int(P2(p+16+2*n));
        _soundId[n] = int(P4(p+32+4*n));
        _sound[n] = nullptr;
        _envelope[n] = new TownsPcmEnvelope(p+64+8*n);
    }
    //cerr << this->describe() << '\n';
}

TownsPcmInstrument::~TownsPcmInstrument()
{
    for (int n = 0; n < _maxSplitNum; n++) {
        delete _envelope[n];
    }
}

void TownsPcmInstrument::registerSound(TownsPcmSound const *sound)
{
    for (int i = 0; i < _maxSplitNum; i++) {
        if (_soundId[i] == sound->id()) {
            _sound[i] = sound;
        }
    }
}

TownsPcmSound const *TownsPcmInstrument::findSound(int note) const
{
    // 少なくともどれかの split を選択できるようにしておこう
    int splitNum;
    for (splitNum = 0; splitNum < _maxSplitNum-1; splitNum++) {
        if (note <= _split[splitNum]) {
            break;
        }
    }
    return _sound[splitNum];
}

TownsPcmEnvelope const *TownsPcmInstrument::findEnvelope(int note) const
{
    // 少なくともどれかの split を選択できるようにしておこう
    int splitNum;
    for (splitNum = 0; splitNum < _maxSplitNum-1; splitNum++) {
        if (note <= _split[splitNum]) {
            break;
        }
    }
    return _envelope[splitNum];
}

/* TownsFmEmulator_Operator */

TownsFmEmulator_Operator::TownsFmEmulator_Operator()
{
    _state = _s_ready;
    _oldState = _s_ready;
    _currentLevel = ((int64_t)0x7f << 31);
    _phase = 0;
    _lastOutput = 0;
    _feedbackLevel = 0;
    _detune = 0;
    _multiple = 1;
    _specifiedTotalLevel = 127;
    _keyScale = 0;
    _specifiedAttackRate = 0;
    _specifiedDecayRate = 0;
    _specifiedSustainRate = 0;
    _specifiedReleaseRate = 15;
    this->velocity(0);
}

TownsFmEmulator_Operator::~TownsFmEmulator_Operator()
{
}

void TownsFmEmulator_Operator::velocity(int velo)
{
    _velocity = velo;
    _totalLevel = (((int64_t)_specifiedTotalLevel << 31) +
                   ((int64_t)(127-_velocity) << 29));
    _sustainLevel = ((int64_t)_specifiedSustainLevel << (31+2));
}

void TownsFmEmulator_Operator::feedbackLevel(int level)
{
    _feedbackLevel = level;
}

void TownsFmEmulator_Operator::setInstrumentParameter(u_char const *instrument)
{
    _detune = (instrument[8] >> 4) & 7;
    _multiple = instrument[8] & 15;
    _specifiedTotalLevel = instrument[12] & 127;
    _keyScale = (instrument[16] >> 6) & 3;
    _specifiedAttackRate = instrument[16] & 31;
    _specifiedDecayRate = instrument[20] & 31;
    _specifiedSustainRate = instrument[24] & 31;
    _specifiedSustainLevel = (instrument[28] >> 4) & 15;
    _specifiedReleaseRate = instrument[28] & 15;
    _state = _s_ready; // 本物ではどうなのかな?
    this->velocity(_velocity);
}

void TownsFmEmulator_Operator::keyOn()
{
    _state = _s_attacking;
    _tickCount = 0;
    _phase = 0; // どうも、実際こうらしい
    _currentLevel = ((int64_t)0x7f << 31); // これも、実際こうらしい
}

void TownsFmEmulator_Operator::keyOff()
{
    if (_state != _s_ready) {
        _state = _s_releasing;
    }
}

void TownsFmEmulator_Operator::frequency(int freq)
{
    int r;

    //DB(("freq=%d=%d[Hz]\n", freq, freq/262205));
    _frequency = freq;

    r = _specifiedAttackRate;
    if (r != 0) {
        r = r * 2 + (keyscaleTable[freq/262205] >> (3-_keyScale));
        if (r >= 64) {
            r = 63; // するべきなんだろうとは思うんだけど (赤p.207)
        }
    }
    {
        r = 63 - r;
        int64_t t;
        if (_specifiedTotalLevel >= 128) {
            t = 0;
        }
        else {
            t = powtbl[(r&3) << 7];
            t <<= (r >> 2);
            t *= 41; // r == 4 のとき、0-96db が 8970.24ms
            t >>= (15 + 5);
            t *= 127 - _specifiedTotalLevel;
            t /= 127;
        }
        _attackTime = int(t); // 1 秒 == (1 << 12)
        //DB(("AR=%d->%d, 0-96[db]=%d[ms]\n", _specifiedAttackRate, r, (int)((t*1000)>>12)));
    }

    r = _specifiedDecayRate;
    if (r != 0) {
        r = r * 2 + (keyscaleTable[freq/262205] >> (3-_keyScale));
        if (r >= 64) {
            r = 63;
        }
    }
    //DB(("DR=%d->%d\n", _specifiedDecayRate, r));
    _decayRate = 0x80;
    _decayRate *= powtbl[(r&3) << 7];
    _decayRate <<= 16 + (r >> 2);
    _decayRate >>= 1;
    _decayRate /= 124; // r == 4 のとき、0-96db が 123985.92ms

    r = _specifiedSustainRate;
    if (r != 0) {
        r = r * 2 + (keyscaleTable[freq/262205] >> (3-_keyScale));
        if (r >= 64) {
            r = 63;
        }
    }
    //DB(("SR=%d->%d\n", _specifiedSustainRate, r));
    _sustainRate = 0x80;
    _sustainRate *= powtbl[(r&3) << 7];
    _sustainRate <<= 16 + (r >> 2);
    _sustainRate >>= 1;
    _sustainRate /= 124;

    r = _specifiedReleaseRate;
    if (r != 0) {
        r = r * 2 + 1; // このタイミングで良いのかわからん
        r = r * 2 + (keyscaleTable[freq/262205] >> (3-_keyScale));
        // KS による補正はあるらしい。赤p.206 では記述されてないけど。
        if (r >= 64) {
            r = 63;
        }
    }
    //DB(("RR=%d->%d\n", _specifiedReleaseRate, r));
    _releaseRate = 0x80;
    _releaseRate *= powtbl[(r&3) << 7];
    _releaseRate <<= 16 + (r >> 2);
    _releaseRate >>= 1;
    _releaseRate /= 124;
}

int TownsFmEmulator_Operator::nextTick(int rate, int phaseShift)
{
    // sampling ひとつ分進める

    if (_oldState != _state) {
        //DB(("state %d -> %d\n", _oldState, _state));
        //DB(("(currentLevel = %08x %08x)\n", (int)(_currentLevel>>32), (int)(_currentLevel)));
//         switch (_oldState) {
//         default:
//             break;
//         };
        _oldState = _state;
    }

    switch (_state) {
    case _s_ready:
        return 0;
        break;
    case _s_attacking:
        ++_tickCount;
        if (_attackTime <= 0) {
            _currentLevel = 0;
            _state = _s_decaying;
        }
        else {
            int i = ((int64_t)_tickCount << (12+10)) / ((int64_t)rate * _attackTime);
            if (i >= 1024) {
                _currentLevel = 0;
                _state = _s_decaying;
            }
            else {
                _currentLevel = attackOut[i];
                _currentLevel <<= 31 - 8;
                //DB(("this=%08x, count=%d, time=%d, i=%d, out=%04x\n", this, _tickCount, _attackTime, i, attackOut[i]));
            }
        }
        break;
    case _s_decaying:
        _currentLevel += _decayRate / rate;
        if (_currentLevel >= _sustainLevel) {
            _currentLevel = _sustainLevel;
            _state = _s_sustaining;
        }
        break;
    case _s_sustaining:
        _currentLevel += _sustainRate / rate;
        if (_currentLevel >= ((int64_t)0x7f << 31)) {
            _currentLevel = ((int64_t)0x7f << 31);
            _state = _s_ready;
        }
        break;
    case _s_releasing:
        _currentLevel += _releaseRate / rate;
        if (_currentLevel >= ((int64_t)0x7f << 31)) {
            _currentLevel = ((int64_t)0x7f << 31);
            _state = _s_ready;
        }
        break;
    default:
        // ここには来ないはず
        break;
    };

    int64_t level = _currentLevel + _totalLevel;
    int64_t output = 0;
    if (level < ((int64_t)0x7f << 31)) {
        int const feedback[] = {
            0,
            0x400 / 16,
            0x400 / 8,
            0x400 / 4,
            0x400 / 2,
            0x400,
            0x400 * 2,
            0x400 * 4,
        };

        _phase &= 0x3ffff;
        phaseShift >>= 2; // 正しい変調量は?  3 じゃ小さすぎで 2 じゃ大きいような。
        phaseShift += (((int64_t)(_lastOutput) * feedback[_feedbackLevel]) >> 16); // 正しい変調量は?  16から17の間のようだけど。
        output = sintbl[((_phase >> 7) + phaseShift) & 0x7ff];
        output >>= (level >> 34); // 正しい減衰量は?
        output *= powtbl[511 - ((level>>25)&511)];
        output >>= 16;
        output >>= 1;

        if (_multiple > 0) {
            _phase += (_frequency * _multiple) / rate;
        }
        else {
            _phase += _frequency / (rate << 1);
        }
    }

    _lastOutput = int(output);
    return int(output);
}

/* TownsFmEmulator */

TownsFmEmulator::TownsFmEmulator()
{
    _chn_volume = 127;
    _expression = 127;
    _offVelocity = 0;
    _gateTime = 0;
    _note = 40;
    _frequency = 440;
    _frequencyOffs = 0x2000;
    _algorithm = 7;
    _opr = new TownsFmEmulator_Operator[_numOfOperators];
    _enableL = _enableR = 1;
    this->velocity(0);
}

TownsFmEmulator::~TownsFmEmulator()
{
    delete [] _opr;
}

void TownsFmEmulator::velocity(int velo)
{
    EUP_TownsEmulator_MonophonicAudioSynthesizer::velocity(velo);
    int v = velo + (_chn_volume - 127) * 4;
    bool iscarrier[8][4] = {
        { false, false, false,  true, }, /*0*/
        { false, false, false,  true, }, /*1*/
        { false, false, false,  true, }, /*2*/
        { false, false, false,  true, }, /*3*/
        { false,  true, false,  true, }, /*4*/
        { false,  true,  true,  true, }, /*5*/
        { false,  true,  true,  true, }, /*6*/
        {  true,  true,  true,  true, }, /*7*/
    };
    for (int opr = 0; opr < 4; opr++)
        if (iscarrier[_algorithm][opr]) {
            _opr[opr].velocity(v);
        }
        else {
            _opr[opr].velocity(127);
        }
}

void TownsFmEmulator::setControlParameter(int control, int value)
{
    switch (control) {
    case 0:
        // Bank Select (for devices with more than 128 programs)
        // base for "program change" commands
        if (value > 0) {
//             fprintf(stderr, "warning: unsupported Bank Select: %d\n", value);
//             fflush(stderr);
        }
        break;

    case 1:
        // Modulation controls a vibrato effect (pitch, loudness, brighness)
        if (value > 0) {
//             fprintf(stderr, "warning: unsupported Modulation: %d\n", value);
//             fflush(stderr);
        }
        break;

    case 7:
        _chn_volume = value;
        this->velocity(this->velocity());
        break;

    // panpot
    case 10:
        // Pan (coarse) 0-127
    case 42:
        // Pan (fine) 0-127
        if (value < 0x20) {
            _enableL = 1;
            _enableR = 0;
        }
        else  if (value < 0x60) {
            _enableL = 1;
            _enableR = 1;
        }
        else {
            _enableL = 0;
            _enableR = 1;
        }
        break;

    case 11:
        // Expression (coarse) controllers are for dynamics (i.e., volume). Thus, they work similarly to controllers for volume (e.g., 0x07).
    case 43:
        // Expression (fine) controllers are for dynamics (i.e., volume). Thus, they work similarly to controllers for volume (e.g., 0x07).
        _expression = value;
        this->velocity(this->velocity());

        if (value != 127) {
//             fprintf(stderr, "warning: song uses unimplemented Expression control\n");
//             fflush(stderr);
        }
        break;

    case 64:
        // Hold (damper, sustain) pedal 1 (on/off) < 63 is off, >= 64 is on
        if (value > 64) {
//             fprintf(stderr, "warning: use of unimplemented Sustain Pedal: %d\n", value);
//             fflush(stderr);
        }
        break;

    default:
//         fprintf(stderr, "TownsFmEmulator::setControlParameter: unknown control %d, val=%d\n", control, value);
//         fflush(stderr);
        break;
    };
}

void TownsFmEmulator::setInstrumentParameter(u_char const *fmInst,
        u_char const *pcmInst)
{
    u_char const *instrument = fmInst;

    if (instrument == nullptr) {
//         fprintf(stderr, "%s@%p: can not set null instrument\n",
//                 "TownsFmEmulator::setInstrumentParameter", this);
//         fflush(stderr);
        return;
    }

    _algorithm = instrument[32] & 7;
    _opr[0].feedbackLevel((instrument[32] >> 3) & 7);
    _opr[1].feedbackLevel(0);
    _opr[2].feedbackLevel(0);
    _opr[3].feedbackLevel(0);
    _opr[0].setInstrumentParameter(instrument + 0);
    _opr[1].setInstrumentParameter(instrument + 2);
    _opr[2].setInstrumentParameter(instrument + 1);
    _opr[3].setInstrumentParameter(instrument + 3);
}

void TownsFmEmulator::nextTick(int *outbuf, int buflen)
{
    // steptime ひとつ分進める

    if (_gateTime > 0) {
        if (--_gateTime <= 0) {
            this->velocity(_offVelocity);
            for (int i = 0; i < _numOfOperators; i++) {
                _opr[i].keyOff();
            }
        }
    }

    if (this->velocity() == 0) {
        return;
    }

    for (int i = 0; i < buflen; i++) {
        int d = 0;
        int d1, d2, d3, d4;
        switch (_algorithm) {
        case 0:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), d1);
            d3 = _opr[2].nextTick(this->rate(), d2);
            d4 = _opr[3].nextTick(this->rate(), d3);
            d = d4;
            break;
        case 1:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), 0);
            d3 = _opr[2].nextTick(this->rate(), d1 + d2);
            d4 = _opr[3].nextTick(this->rate(), d3);
            d = d4;
            break;
        case 2:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), 0);
            d3 = _opr[2].nextTick(this->rate(), d2);
            d4 = _opr[3].nextTick(this->rate(), d1 + d3);
            d = d4;
            break;
        case 3:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), d1);
            d3 = _opr[2].nextTick(this->rate(), 0);
            d4 = _opr[3].nextTick(this->rate(), d2 + d3);
            d = d4;
            break;
        case 4:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), d1);
            d3 = _opr[2].nextTick(this->rate(), 0);
            d4 = _opr[3].nextTick(this->rate(), d3);
            d = d2 + d4;
            break;
        case 5:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), d1);
            d3 = _opr[2].nextTick(this->rate(), d1);
            d4 = _opr[3].nextTick(this->rate(), d1);
            d = d2 + d3 + d4;
            break;
        case 6:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), d1);
            d3 = _opr[2].nextTick(this->rate(), 0);
            d4 = _opr[3].nextTick(this->rate(), 0);
            d = d2 + d3 + d4;
            break;
        case 7:
            d1 = _opr[0].nextTick(this->rate(), 0);
            d2 = _opr[1].nextTick(this->rate(), 0);
            d3 = _opr[2].nextTick(this->rate(), 0);
            d4 = _opr[3].nextTick(this->rate(), 0);
            d = d1 + d2 + d3 + d4;
            break;
        default:
            break;
        };

        if constexpr (2 == streamAudioChannelsNum) {
            int j = 2 * i;
            outbuf[j++] += (_enableL ? d : 0);
            outbuf[j] += (_enableR ? d : 0);
        }
        else /*(1 == streamAudioChannelsNum)*/ {
            outbuf[i] += d;
        }
    }
}

void TownsFmEmulator::note(int n, int onVelo, int offVelo, int gateTime)
{
    _note = n;
    this->velocity(onVelo);
    _offVelocity = offVelo;
    _gateTime = gateTime;
    this->recalculateFrequency();
    for (int i = 0; i < _numOfOperators; i++) {
        _opr[i].keyOn();
    }
}

void TownsFmEmulator::pitchBend(int value)
{
    _frequencyOffs = value;
    this->recalculateFrequency();
}

void TownsFmEmulator::recalculateFrequency()
{
    // MIDI とも違うし....
    // どういう仕様なんだろうか?
    // と思ったら、なんと、これ (↓) が正解らしい。
    int64_t basefreq = frequencyTable[_note];
    double basefreq_double = double(basefreq);
    int lcl_frequencyOffs = _frequencyOffs;
    if (lcl_frequencyOffs > 0x4000) {
        lcl_frequencyOffs = 0x4000;
    }
    if (lcl_frequencyOffs < 0x0000) {
        lcl_frequencyOffs = 0x0000;
    }
    double Offs_double = pow(2.0, (double(lcl_frequencyOffs - 8192/*0x2000*/) / double(8192/*0x2000*/)));
    basefreq_double *= Offs_double;

    int cfreq = frequencyTable[_note - (_note % 12)];
    int oct = _note / 12;
    int fnum = int((basefreq << 13) / int64_t(cfreq)); // OPL の fnum と同じようなもの。
    //fnum += _frequencyOffs - 0x2000;
    //if (fnum < 0x2000) {
    //    fnum += 0x2000;
    //    oct--;
    //}
    //if (fnum >= 0x4000) {
    //    fnum -= 0x2000;
    //    oct++;
    //}

    // _frequency は最終的にバイアス 256*1024 倍
    _frequency = (frequencyTable[oct*12] * (int64_t)fnum) >> (13 - 10);

    for (int i = 0; i < _numOfOperators; i++) {
        _opr[i].frequency(_frequency);
    }
}

/* TownsPcmEmulator */

TownsPcmEmulator::TownsPcmEmulator()
{
    _chn_volume = 100; // power-on reset value (100) is defined in midi spec
    _expression = 127;
    this->velocity(0);
    _gateTime = 0;
    _frequencyOffs = 0x2000;
    _currentInstrument = nullptr;
    _currentEnvelope = nullptr;
    _currentSound = nullptr;
    _volL = _volR = 0x10;	// correctly only 0xf - but let's keep it shift friendly
}

TownsPcmEmulator::~TownsPcmEmulator()
{
    delete _currentEnvelope;
}

void TownsPcmEmulator::setControlParameter(int control, int value)
{
    switch (control) {
    case 0:
        // Bank Select (for devices with more than 128 programs)
        // base for "program change" commands
        if (value > 0) {
//             fprintf(stderr, "warning: unsupported Bank Select: %d\n", value);
//             fflush(stderr);
        }
        break;

    case 1:
        // Modulation controls a vibrato effect (pitch, loudness, brighness)
        if (value > 0) {
//             fprintf(stderr, "warning: unsupported Modulation: %d\n", value);
//             fflush(stderr);
        }
        break;

    case 7:
        _chn_volume = value;
        break;

    case 10:
        // Pan (coarse) 0-127
    case 42:
        // Pan (fine) 0-127
        // panpot - rf5c68 seems to have an 8-bit pan register where low-nibble controls output to left
        // speaker and high-nibble the right..
        value -= 0x40;
        _volL = 0x10 - (0x10*value/0x40);
        _volR = 0x10 + (0x10*value/0x40);
        break;

    case 11:
        // Expression (coarse) controllers are for dynamics (i.e., volume). Thus, they work similarly to controllers for volume (e.g., 0x07).
    case 43:
        // Expression (fine) controllers are for dynamics (i.e., volume). Thus, they work similarly to controllers for volume (e.g., 0x07).
        _expression = value;

        if (value != 127) {
//             fprintf(stderr, "warning: song uses unimplemented Expression control\n");
//             fflush(stderr);
        }
        break;

    case 64:
        // Hold (damper, sustain) pedal 1 (on/off) < 63 is off, >= 64 is on
        if (value > 64) {
//             fprintf(stderr, "warning: use of unimplemented Sustain Pedal: %d\n", value);
//             fflush(stderr);
        }
        break;

    default:
//         fprintf(stderr, "TownsPcmEmulator::setControlParameter: unknown control %d, val=%d\n", control, value);
//         fflush(stderr);
        break;
    };
}

void TownsPcmEmulator::setInstrumentParameter(uint8_t const *fmInst,
        uint8_t const *pcmInst)
{
    uint8_t const *instrument = pcmInst;
    if (instrument == nullptr) {
//         fprintf(stderr, "%s@%p: can not set null instrument\n",
//                 "TownsPcmEmulator::setInstrumentParameter", this);
//         fflush(stderr);
        return;
    }
    _currentInstrument = (TownsPcmInstrument*)instrument;
    //fprintf(stderr, "0x%08x: program change (to 0x%08x)\n", this, instrument);
    //fflush(stderr);
}

void TownsPcmEmulator::nextTick(int *outbuf, int buflen)
{
    // steptime ひとつ分進める

    if (_currentEnvelope == nullptr) {
        return;
    }
    if (_gateTime > 0 && --_gateTime <= 0) {
        this->velocity(_offVelocity);
        _currentEnvelope->release();
    }
    if (_currentEnvelope->state() == 0) {
        this->velocity(0);
    }
    if (this->velocity() == 0) {
        delete _currentEnvelope;
        _currentEnvelope = nullptr;
        return;
    }

    uint32_t phaseStep;
    {
        int64_t ps = frequencyTable[_note];
//        ps *= powtbl[_frequencyOffs>>4];
        ps *= 0x10000;							// neutral pos from old table impl
        double bend = ((double)_frequencyOffs - 0x2000) / 0x2000;	// -1..1
        ps = int64_t(pow(1.0594789723915, 2*bend) * ps);		// factor (halftone step) tuned to above table
        ps /= frequencyTable[_currentSound->keyNote() - _currentEnvelope->rootKeyOffset()];
        ps *= _currentSound->adjustedSamplingRate();
        ps /= this->rate();
        ps >>= 16;
        phaseStep = uint32_t(ps);
    }
    uint32_t loopLength = _currentSound->loopLength() << 16; // あらかじめ計算して
    uint32_t numSamples = _currentSound->numSamples() << 16; // おくのは危険だぞ
    signed char const *soundSamples = _currentSound->samples();
    for (int i = 0; i < buflen; i++) {
        if (loopLength > 0)
            while (_phase >= numSamples) {
                _phase -= loopLength;
            }
        else if (_phase >= numSamples) {
            _gateTime = 0;
            this->velocity(0);
            delete _currentEnvelope;
            _currentEnvelope = nullptr;
            // 上との関係もあるしもっといい方法がありそう
            break;
        }

        // 線型補間する。
        int output;
        {
            uint32_t phase0 = uint32_t(_phase);
            uint32_t phase1 = uint32_t(_phase) + 0x10000;
            if (phase1 >= uint32_t(numSamples)) {
                // it's safe even if loopLength == 0, because soundSamples[] is extended by 1 and filled with 0 (see TownsPcmSound::TownsPcmSound).
                phase1 -= uint32_t(loopLength);
            }
            phase0 >>= 16;
            phase1 >>= 16;

            int weight1 = _phase & 0xffff;
            int weight0 = 0x10000 - weight1;

            output = soundSamples[phase0] * weight0 + soundSamples[phase1] * weight1;
            output >>= 16;
        }

        output *= this->velocity(); // 信じられないかも知れないけど、FM と違うんです。
        output <<= 1;
        output *= _currentEnvelope->nextTick();
        output >>= 7;
        output *= _chn_volume; // 正しい減衰量は?
        output >>= 7;
        // FM との音量バランスを取る。
        output *= 185; // くらい?  半端ですねぇ。
        output >>= 8;
        if constexpr (2 == streamAudioChannelsNum) {
            int j = 2 * i;
            outbuf[j++] += (_volL * output) >> 4;
            outbuf[j] += (_volR * output) >> 4;
        }
        else /*(1 == streamAudioChannelsNum)*/ {
            outbuf[i] += output;
        }

        _phase += phaseStep;
    }
}

void TownsPcmEmulator::note(int n, int onVelo, int offVelo, int gateTime)
{
    _note = n;
    this->velocity(onVelo);
    _offVelocity = offVelo;
    _gateTime = gateTime;
    _phase = 0;
    if (_currentInstrument != nullptr) {
        _currentSound = _currentInstrument->findSound(n);
        if (_currentEnvelope)
            delete _currentEnvelope;
        _currentEnvelope = new TownsPcmEnvelope(_currentInstrument->findEnvelope(n));
        _currentEnvelope->start(this->rate());
    }
    else {
        _currentSound = nullptr;
    }
}

void TownsPcmEmulator::pitchBend(int value)
{
    _frequencyOffs = value;
}

/* EUP_TownsEmulator_Channel */

EUP_TownsEmulator_Channel::EUP_TownsEmulator_Channel()
{
    _dev[0] = nullptr;
    _lastNotedDeviceNum = 0;
}

EUP_TownsEmulator_Channel::~EUP_TownsEmulator_Channel()
{
    for (int n = 0; _dev[n] != nullptr; n++) {
        delete _dev[n];
    }
}

void EUP_TownsEmulator_Channel::add(EUP_TownsEmulator_MonophonicAudioSynthesizer *device)
{
    for (int n = 0; n < _maxDevices; n++)
        if (_dev[n] == nullptr) {
            _dev[n] = device;
            _dev[n+1] = nullptr;
            break;
        }
}

void EUP_TownsEmulator_Channel::note(int note, int onVelo, int offVelo, int gateTime)
{
    int n = _lastNotedDeviceNum;
    if (_dev[n] == nullptr || _dev[n+1] == nullptr) {
        n = 0;
    }
    else {
        n++;
    }

    if (_dev[n] != nullptr) {
        _dev[n]->note(note, onVelo, offVelo, gateTime);
    }

    _lastNotedDeviceNum = n;
    //fprintf(stderr, "ch=%08x, dev=%d, note=%d, on=%d, off=%d, gate=%d\n",
    // this, n, note, onVelo, offVelo, gateTime);
    //fflush(stderr);
}

void EUP_TownsEmulator_Channel::setControlParameter(int control, int value)
{
    // いいのかこれで?
    for (int n = 0; _dev[n] != nullptr; n++) {
        _dev[n]->setControlParameter(control, value);
    }
}

void EUP_TownsEmulator_Channel::setInstrumentParameter(uint8_t const *fmInst,
        uint8_t const *pcmInst)
{
    for (int n = 0; _dev[n] != nullptr; n++) {
        _dev[n]->setInstrumentParameter(fmInst, pcmInst);
    }
}

void EUP_TownsEmulator_Channel::pitchBend(int value)
{
    // いいのかこれで?
    for (int n = 0; _dev[n] != nullptr; n++) {
        _dev[n]->pitchBend(value);
    }
}

void EUP_TownsEmulator_Channel::nextTick(int *outbuf, int buflen)
{
    for (int n = 0; _dev[n] != nullptr; n++) {
        _dev[n]->nextTick(outbuf, buflen);
    }
}

void EUP_TownsEmulator_Channel::rate(int r)
{
    for (int n = 0; _dev[n] != nullptr; n++) {
        _dev[n]->rate(r);
    }
}

/* EUP_TownsEmulator */

EUP_TownsEmulator::EUP_TownsEmulator()
{
    for (int n = 0; n < _maxChannelNum; n++) {
        _channel[n] = new EUP_TownsEmulator_Channel;
        _enabled[n] = true;
    }
    this->outputSampleUnsigned(true);
    this->outputSampleSize(1);
    this->outputSampleChannels(1);
    this->outputSampleLSBFirst(true);
    this->rate(8000);
    std::memset(&_fmInstrumentData[0], 0, sizeof(_fmInstrumentData));
    for (int n = 0; n < _maxFmInstrumentNum; n++) {
        _fmInstrument[n] = _fmInstrumentData + 8 + 48*n;
    }
    for (int n = 0; n < _maxPcmInstrumentNum; n++) {
        _pcmInstrument[n] = nullptr;
    }
    for (int n = 0; n < _maxPcmSoundNum; n++) {
        _pcmSound[n] = nullptr;
    }
}

EUP_TownsEmulator::~EUP_TownsEmulator()
{
    for (int n = 0; n < _maxChannelNum; n++) {
        delete _channel[n];
    }
    for (int n = 0; n < _maxPcmInstrumentNum; n++)
        if (_pcmInstrument[n] != nullptr) {
            delete _pcmInstrument[n];
        }
    for (int n = 0; n < _maxPcmSoundNum; n++)
        if (_pcmSound[n] != nullptr) {
            delete _pcmSound[n];
        }
}

void EUP_TownsEmulator::assignFmDeviceToChannel(int channel)
{
    CHECK_CHANNEL_NUM("EUP_TownsEmulator::assignFmDeviceToChannel", channel);

    EUP_TownsEmulator_MonophonicAudioSynthesizer *dev = new TownsFmEmulator;
    dev->rate(_rate);
    _channel[channel]->add(dev);
}

void EUP_TownsEmulator::assignPcmDeviceToChannel(int channel)
{
    CHECK_CHANNEL_NUM("EUP_TownsEmulator::assignPcmDeviceToChannel", channel);

    if (channel >= _maxChannelNum) return; // original impl created out of bounds write to _channel[]

    EUP_TownsEmulator_MonophonicAudioSynthesizer *dev = new TownsPcmEmulator;
    dev->rate(_rate);
    _channel[channel]->add(dev);
}

void EUP_TownsEmulator::setFmInstrumentParameter(int num, uint8_t const *instrument)
{
    if (num < 0 || num >= _maxFmInstrumentNum) {
//         fprintf(stderr, "%s: FM instrument number %d out of range\n",
//                 "EUP_TownsEmulator::setFmInstrumentParameter",  num);
//         fflush(stderr);
        return;
    }
    memcpy(_fmInstrument[num], instrument, 48);
}

void EUP_TownsEmulator::setPcmInstrumentParameters(uint8_t const *instrument, size_t size)
{
    for (int n = 0; n < _maxPcmInstrumentNum; n++) {
        if (_pcmInstrument[n] != nullptr) {
            delete _pcmInstrument[n];
        }
        _pcmInstrument[n] = new TownsPcmInstrument(instrument+8+128*n);
    }
    uint8_t const *p = instrument + 8 + 128*32;
    for (int m = 0; m < _maxPcmSoundNum && p<(instrument+size); m++) {
        if (_pcmSound[m] != nullptr) {
            delete _pcmSound[m];
        }
        _pcmSound[m] = new TownsPcmSound(p);
        for (int n = 0; n < _maxPcmInstrumentNum; n++) {
            _pcmInstrument[n]->registerSound(_pcmSound[m]);
        }
        p += 32 + P4(p+12);
    }
}

void EUP_TownsEmulator::enable(int ch, bool en)
{
    DB(("enable ch=%d, en=%d\n", ch, en));
    CHECK_CHANNEL_NUM("EUP_TownsEmulator::enable", ch);
    _enabled[ch] = en;
}

void EUP_TownsEmulator::nextTick()
{
    // steptime ひとつ分進める

    struct timeval tv = this->timeStep();
    int64_t buflen = (int64_t)_rate * (int64_t)tv.tv_usec; /* 精度上げなきゃ  */
    //buflen /= 1000 * 1000;
    buflen /= 1000 * 1012; // 1010 から 1015 くらいですね、うちでは。曲によるけど。
    buflen++;
#if defined ( _MSC_VER )
    int *buf0 = new int[buflen * _outputSampleChannels];
    if (nullptr == buf0) {
//         fprintf(stderr, "heap allocation problem.\n");
//         fflush(stderr);
        exit(0);
    }
#endif // _MSC_VER
#if defined ( __MINGW32__ )
    int *buf0 = new int[buflen * _outputSampleChannels];
    if (nullptr == buf0) {
//         fprintf(stderr, "heap allocation problem.\n");
//         fflush(stderr);
        exit(0);
    }
#endif // __MINGW32__
#if defined ( __GNUC__ ) && !defined ( __MINGW32__ )
    int buf0[buflen * _outputSampleChannels];
#endif // __GNUC__

    memset(buf0, 0, sizeof(buf0[0]) * buflen * _outputSampleChannels);

    for (int i = 0; i < _maxChannelNum; i++) {
        if (_enabled[i]) {
            _channel[i]->nextTick(buf0, int(buflen));
        }
    }

    if (1 == _outputSampleSize) {
        {
            while (true) { /* infinite loop waiting for empty space in buffer */
                // there's enough space for buflen samples
                if (((pcm.read_pos < pcm.write_pos) && (buflen <= (streamAudioSamplesBuffer - pcm.write_pos + pcm.read_pos)))
                    ||
                    ((pcm.read_pos >= pcm.write_pos) && (buflen <= (pcm.read_pos - pcm.write_pos)))) {
                    int renderData = pcm.write_pos;

                    if (false == _outputSampleUnsigned) {
                        int8_t* renderBuffer = reinterpret_cast<int8_t*>(pcm.buffer);

                        if (2 == _outputSampleChannels) {
                            for (int i = 0; i < buflen; i++) {
                                if (streamAudioSamplesBuffer <= renderData) {
                                    renderData = 0;
                                }

                                int dl = buf0[2 * i];
                                dl *= this->volume();
                                int8_t ddl = int8_t(dl >> 18); // いいかげんだなぁ
                                renderBuffer[2 * renderData] = (127 < ddl) ? 127 : ((-128 > ddl) ? -128 : ddl);
                                int dr = buf0[(2 * i) + 1];
                                dr *= this->volume();
                                int8_t ddr = int8_t(dr >> 18); // いいかげんだなぁ
                                renderBuffer[(2 * renderData) + 1] = (127 < ddr) ? 127 : ((-128 > ddr) ? -128 : ddr);

                                renderData++;
                            }

                            /* left channel sample first place */
                            /* right channel sample second place */
                        }
                        else /*(1 == _outputSampleChannels)*/ {
                            for (int i = 0; i < buflen; i++) {
                                if (streamAudioSamplesBuffer <= renderData) {
                                    renderData = 0;
                                }

                                int d = buf0[i];
                                d *= this->volume();
                                int8_t dd = int8_t(d >> 18); // いいかげんだなぁ
                                renderBuffer[renderData++] = (127 < dd) ? 127 : ((-128 > dd) ? -128 : dd);
                            }
                        }
                    }
                    else /*(true == _outputSampleUnsigned)*/ {
                        uint8_t* renderBuffer = reinterpret_cast<uint8_t*>(pcm.buffer);

                        if (2 == _outputSampleChannels) {
                            for (int i = 0; i < buflen; i++) {
                                if (streamAudioSamplesBuffer <= renderData) {
                                    renderData = 0;
                                }

                                int dl = buf0[2 * i];
                                dl *= this->volume();
                                dl >>= 10; // いいかげんだなぁ
                                dl ^= 0x8000;
                                renderBuffer[2 * renderData] = ((dl >> 8) & 0xff);
                                int dr = buf0[(2 * i) + 1];
                                dr *= this->volume();
                                dl >>= 10; // いいかげんだなぁ
                                dl ^= 0x8000;
                                renderBuffer[(2 * renderData) + 1] = ((dr >> 8) & 0xff);

                                renderData++;
                            }

                            /* left channel sample first place */
                            /* right channel sample second place */
                        }
                        else /*(1 == _outputSampleChannels)*/ {
                            for (int i = 0; i < buflen; i++) {
                                if (streamAudioSamplesBuffer <= renderData) {
                                    renderData = 0;
                                }

                                int d = buf0[i];
                                d *= this->volume();
                                d >>= 10; // いいかげんだなぁ
                                d ^= 0x8000;
                                renderBuffer[renderData++] = ((d >> 8) & 0xff);
                            }
                        }
                    }
                    pcm.write_pos = renderData;
                    break; /* leave infinite loop waiting for empty space in buffer */
                    // there's not space in buffer, please wait
                }
                else {
                    SDL_Delay(1);
                }
            }
        }
    }
    else /*(2 == _outputSampleSize)*/ {
        {
            while (true) { /* infinite loop waiting for empty space in buffer */
                // there's enough space for buflen samples
                if (((pcm.read_pos < pcm.write_pos) && (buflen <= (streamAudioSamplesBuffer - pcm.write_pos + pcm.read_pos)))
                    ||
                    ((pcm.read_pos >= pcm.write_pos) && (buflen <= (pcm.read_pos - pcm.write_pos)))) {
                    int renderData = pcm.write_pos;

                    if (true == _outputSampleLSBFirst) {
                        if (false == _outputSampleUnsigned) {
                            int16_t * renderBuffer = pcm.buffer;

                            if (2 == _outputSampleChannels) {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int dl = buf0[2 * i];
                                    dl *= this->volume();
                                    int32_t ddl = (dl >> 10); // いいかげんだなぁ
                                    renderBuffer[2 * renderData] = int16_t((32767 < ddl) ? 32767 : ((-32768 > ddl) ? -32768 : ddl));
                                    int dr = buf0[(2 * i) + 1];
                                    dr *= this->volume();
                                    int32_t ddr = (dr >> 10); // いいかげんだなぁ
                                    renderBuffer[(2 * renderData) + 1] = int16_t((32767 < ddr) ? 32767 : ((-32768 > ddr) ? -32768 : ddr));

                                    renderData++;
                                }

                                /* left channel sample first place */
                                /* right channel sample second place */
                            }
                            else /*(1 == _outputSampleChannels)*/ {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int d = buf0[i];
                                    d *= this->volume();
                                    int8_t dd = int8_t(d >> 10); // いいかげんだなぁ
                                    renderBuffer[renderData++] = (32767 < dd) ? 32767 : ((-32768 > dd) ? -32768 : dd);
                                }
                            }
                        }
                        else /*(true == _outputSampleUnsigned)*/ {
                            uint16_t* renderBuffer = reinterpret_cast<uint16_t*>(pcm.buffer);

                            if (2 == _outputSampleChannels) {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int dl = buf0[2 * i];
                                    dl *= this->volume();
                                    dl >>= 10; // いいかげんだなぁ
                                    renderBuffer[2 * renderData] = (dl & 0x7fff);
                                    int dr = buf0[(2 * i) + 1];
                                    dr *= this->volume();
                                    dl >>= 10; // いいかげんだなぁ
                                    renderBuffer[(2 * renderData) + 1] = (dr & 0x7fff);

                                    renderData++;
                                }

                                /* left channel sample first place */
                                /* right channel sample second place */
                            }
                            else /*(1 == _outputSampleChannels)*/ {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int d = buf0[i];
                                    d *= this->volume();
                                    d >>= 10; // いいかげんだなぁ
                                    renderBuffer[renderData++] = (d & 0x7fff);
                                }
                            }
                        }
                    }
                    else /*(false == _outputSampleLSBFirst)*/ {
                        if (false == _outputSampleUnsigned) {
                            int16_t* renderBuffer = pcm.buffer;

                            if (2 == _outputSampleChannels) {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int dl = buf0[2 * i];
                                    dl *= this->volume();
                                    int16_t ddl = int16_t(dl >> 10); // いいかげんだなぁ
                                    int16_t dddl = (32767 < ddl) ? 32767 : ((-32768 > ddl) ? -32768 : ddl);
                                    renderBuffer[2 * renderData] = ((dddl >> 8) & 0xff) + ((dddl << 8) & 0xff00);
                                    int dr = buf0[(2 * i) + 1];
                                    dr *= this->volume();
                                    int16_t ddr = int16_t(dr >> 10); // いいかげんだなぁ
                                    int16_t dddr = (32767 < ddr) ? 32767 : ((-32768 > ddr) ? -32768 : ddr);
                                    renderBuffer[(2 * renderData) + 1] = ((dddr >> 8) & 0xff) + ((dddr << 8) & 0xff00);

                                    renderData++;
                                }

                                /* left channel sample first place */
                                /* right channel sample second place */
                            }
                            else /*(1 == _outputSampleChannels)*/ {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int d = buf0[i];
                                    d *= this->volume();
                                    int16_t dd = int16_t(d >> 10); // いいかげんだなぁ
                                    int16_t ddd = (32767 < dd) ? 32767 : ((-32768 > dd) ? -32768 : dd);
                                    renderBuffer[renderData++] = ((ddd >> 8) & 0xff) + ((ddd << 8) & 0xff00);
                                }
                            }
                        }
                        else /*(true == _outputSampleUnsigned)*/ {
                            uint16_t* renderBuffer = reinterpret_cast<uint16_t*>(pcm.buffer);

                            if (2 == _outputSampleChannels) {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int dl = buf0[2 * i];
                                    dl *= this->volume();
                                    dl >>= 10; // いいかげんだなぁ
                                    dl &= 0x7fff;
                                    renderBuffer[2 * renderData] = ((dl >> 8) & 0xff) + ((dl << 8) & 0xff00);
                                    int dr = buf0[(2 * i) + 1];
                                    dr *= this->volume();
                                    dl >>= 10; // いいかげんだなぁ
                                    dr &= 0x7fff;
                                    renderBuffer[(2 * renderData) + 1] = ((dr >> 8) & 0xff) + ((dr << 8) & 0xff00);

                                    renderData++;
                                }

                                /* left channel sample first place */
                                /* right channel sample second place */
                            }
                            else /*(1 == _outputSampleChannels)*/ {
                                for (int i = 0; i < buflen; i++) {
                                    if (streamAudioSamplesBuffer <= renderData) {
                                        renderData = 0;
                                    }

                                    int d = buf0[i];
                                    d *= this->volume();
                                    d >>= 10; // いいかげんだなぁ
                                    d &= 0x7fff;
                                    renderBuffer[renderData++] = ((d >> 8) & 0xff) + ((d << 8) & 0xff00);
                                }
                            }
                        }
                    }

                    pcm.write_pos = renderData;
                    break; /* leave infinite loop waiting for empty space in buffer */
                // there's not space in buffer, please wait
                }
                else {
                    SDL_Delay(1);
                }
            }
        }
    }

#if defined ( _MSC_VER )
    delete buf0;
#endif // _MSC_VER
#if defined ( __MINGW32__ )
    delete buf0;
#endif // __MINGW32__
}

void EUP_TownsEmulator::note(int channel, int n,
                             int onVelo, int offVelo, int gateTime)
{
    CHECK_CHANNEL_NUM("EUP_TownsEmulator::note", channel);
    _channel[channel]->note(n, onVelo, offVelo, gateTime);
}

void EUP_TownsEmulator::pitchBend(int channel, int value)
{
    CHECK_CHANNEL_NUM("EUP_TownsEmulator::pitchBend", channel);
    _channel[channel]->pitchBend(value);
}

void EUP_TownsEmulator::controlChange(int channel, int control, int value)
{
    CHECK_CHANNEL_NUM("EUP_TownsEmulator::controlChange", channel);
    _channel[channel]->setControlParameter(control, value);
}

void EUP_TownsEmulator::programChange(int channel, int num)
{
    CHECK_CHANNEL_NUM("EUP_TownsEmulator::programChange", channel);

    uint8_t *fminst = nullptr;
    uint8_t *pcminst = nullptr;

    if (0 <= num && num < _maxFmInstrumentNum) {
        fminst = _fmInstrument[num];
    }
    if (0 <= num && num < _maxPcmInstrumentNum) {
        pcminst = reinterpret_cast<uint8_t *>(_pcmInstrument[num]);
    }

    _channel[channel]->setInstrumentParameter(fminst, pcminst);
}

void EUP_TownsEmulator::rate(int r)
{
    _rate = r;
    for (int n = 0; n < _maxChannelNum; n++) {
        _channel[n]->rate(r);
    }
}
