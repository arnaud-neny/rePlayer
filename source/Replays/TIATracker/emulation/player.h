/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "track/track.h"
#include "track/instrument.h"
#include "tiasound/tiasound.h"
#include "emulation/TIASnd.h"
#include "emulation/SoundSDL2.h"


namespace Emulation {

class Player {
public:
    /* Player ROM usage per feature */
    static const int RomGoto = 8;
    static const int RomSlide = 9;
    static const int RomOverlay = 40;
    static const int RomFunktempoGlobal = 7;
    // +1 per pattern
    static const int RomFunktempoLocal = 13;
    static const int RomLocalNoFunk = 6;
    static const int RomLocalWithFunk = 12;
    static const int RomStartsWithHold = 2;
    // ADIndex + SustainIndex + ReleaseIndex + DummyByte + EndByte
    static const int RomPerInstrument = 5;
    // PercIndex + 2 end bytes
    static const int RomPerPercussion = 3;
    static const int RomPerPattern = 3;
    static const int RomPerSequence = 1;
    static const int RomPlayerCore = 166;

    /* Note constants from player routine */
    static const int NoteHold = 8;
    static const int NotePause = 16;
    static const int NoteFirstPerc = 17;

    bool channelMuted[2]{false, false};

    explicit Player(Track::Track *parentTrack);
    ~Player();

    /* Set framerate to play at */
    void setFrameRate(float rate);

public /*slots*/:
    void startTimer();
    void stopTimer();

    // Stop everything next frame
    void silence();

    /* Start to play a note with a given instrument */
    void playInstrument(Track::Instrument *instrument, int frequency);
    void playInstrumentOnce(Track::Instrument *instrument, int frequency);
    /* Stop playing a note, i.e. send instrument into release */
    void stopInstrument();

    /* Start and stop playing a percussion */
    void playPercussion(Track::Percussion *percussion);
    void stopPercussion();

    void stopTrack();

    /* Start waveform */
    void playWaveform(TiaSound::Distortion waveform, int frequency, int volume);

    /* Play song from given channel note indexes */
    void playTrack(int start1, int start2);

    void selectedChannelChanged(int newChannel);
    void toggleLoop(bool toggle);

    void setTVStandard(int iNewStandard);

// signals:
    void newPlayerPos(int pos1, int pos2);
    void invalidNoteFound(int channel, int entryIndex, int noteIndex, QString reason);

public://private:
    Track::Track *pTrack = nullptr;
    Emulation::TIASound tiaSound;
    Emulation::SoundSDL2 sdlSound{&tiaSound};

    TiaSound::TvStandard replayTvStandard = TiaSound::TvStandard::PAL;
    bool doReplay;

    //QTimer *timer = nullptr;
/*
    // Jitter test statistics
    QElapsedTimer *eTimer;
    long statsJitterSum = 0;
    long statsNumFrames = 0;
    long statsJitterMax = 0;
    QVector<int> deltas;
    QVector<long> elapsedValues;
*/
    // Play mode we are in
    enum class PlayMode {
        None, Instrument, InstrumentOnce, Percussion, Waveform, Track
    };
    PlayMode mode = PlayMode::None;

    /* Current values for instrument play */
    Track::Instrument *currentInstrument = nullptr;
    int currentInstrumentFrequency;
    int currentInstrumentFrame;

    /* Current values for percussion play */
    Track::Percussion *currentPercussion;
    int currentPercussionFrame;

    /* Helper methods for timerFired() */
    void updateSilence();
    void updateInstrument();
    void updatePercussion();
    // Get and process next note in channel
    void sequenceChannel(int channel);
    // Play current note in channel
    void updateChannel(int channel);
    // Do next tick for track
    void updateTrack();

    /* Set values for channel 0 */
    void setChannel0(int distortion, int frequency, int volume);

    /* Set values for a channel */
    void setChannel(int channel, int distortion, int frequency, int volume);

    /* Play track vars */
    // current note index inside pattern
    int trackCurNoteIndex[2];
    // Current entry in sequence
    int trackCurEntryIndex[2];
    int trackCurTick;
    Track::Note trackCurNote[2];
    int trackCurEnvelopeIndex[2];
    // Current mode, to validate track
    Track::Note::instrumentType trackMode[2];
    // Was the current note fetched via overlay?
    bool trackIsOverlay[2];
    bool isFirstNote;

    bool loopPattern = false;
    int channelSelected = 0;

public:
    void timerFired();
};

}

#endif // PLAYER_H
