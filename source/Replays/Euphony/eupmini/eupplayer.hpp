// $Id: eupplayer.h,v 1.4 2000/04/12 23:16:26 hayasaka Exp $

/*      Artistic Style
 *
 * ./astyle --style=stroustrup --convert-tabs --add-braces eupplayer.hpp
 */

#ifndef TJH__EUP_H
#define TJH__EUP_H

#if defined ( _MSC_VER )
#include <Winsock2.h>
#endif // _MSC_VER
#if defined ( __MINGW32__ )
#include <sys/time.h>
#endif // __MINGW32__
#if defined ( __GNUC__ )
#include <sys/time.h>
#endif // __GNUC__
#include <sys/types.h>
#include <iostream>

class PolyphonicAudioDevice {
    struct timeval _timeStep;
    int _volume;
public:
    PolyphonicAudioDevice()
    {
        _timeStep.tv_sec = _timeStep.tv_usec = 0;
        _volume = 256;
    }
    virtual ~PolyphonicAudioDevice() {}
    virtual struct timeval const &timeStep() const {
        return _timeStep;
    }
    virtual int volume(int val = -1)
    {
        if (val >= 0) {
            _volume = val;
        }
        return _volume;
    }
    virtual void timeStep(struct timeval const &step)
    {
        _timeStep = step;
    }
    virtual void enable(int channel, bool en=true) = 0;
    virtual void nextTick() { }
    virtual void note(int channel, int n,
                      int onVelo, int offVelo, int gateTime) = 0;
    virtual void controlChange(int channel, int control, int value) = 0;
    virtual void programChange(int channel, int num) = 0;
    virtual void pitchBend(int channel, int value) = 0;
};

struct EUPHeader {
    uint8_t dummy0[2048];
};

class EUPPlayer {
    enum { _maxTrackNum = 32 };
    PolyphonicAudioDevice *_outputDev;
    int _track2channel[_maxTrackNum];
    int _isPlaying;
    uint8_t const *_curP;
    int _stepTime;
    int _tempo;
    friend int EUPPlayer_cmd_INVALID(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_NOTSUPPORTED(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_8x(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_9x(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_ax(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_bx(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_cx(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_dx(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_ex(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_fx(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f0(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f1(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f2(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f3(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f4(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f5(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f6(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f7(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f8(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_f9(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_fa(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_fb(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_fc(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_fd(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_fe(int cmd, EUPPlayer *pl);
    friend int EUPPlayer_cmd_ff(int cmd, EUPPlayer *pl);
public:
    EUPPlayer();
    ~EUPPlayer();
    PolyphonicAudioDevice* outputDevice() const { return _outputDev; }
    void outputDevice(PolyphonicAudioDevice *outputDev);
    void mapTrack_toChannel(int track, int channel);
    void startPlaying(uint8_t const *ptr);
    void stopPlaying();
    bool isPlaying() const;
    void nextTick();
    int tempo() const;
    void tempo(int t);
};

#endif
