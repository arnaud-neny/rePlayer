/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#include <iostream>
#include "track/track.h"
#include "track/instrument.h"
#include "track/pattern.h"
#include "player.h"


namespace Emulation {

Player::Player(Track::Track *parentTrack)
{
    pTrack = parentTrack;
    replayTvStandard = parentTrack->getTvMode();

    tiaSound.channels(2, true);
    sdlSound.setFrameRate(50.0);
    sdlSound.open();
    sdlSound.mute(false);
    sdlSound.setEnabled(true);
    sdlSound.setVolume(100);

    sdlSound.set(AUDC0, 0, 10);
    sdlSound.set(AUDV0, 0, 15);
    sdlSound.set(AUDF0, 0, 18);
}

Player::~Player()
{
/*
    delete eTimer;

    for (int i = 0; i < deltas.size(); ++i) {
        std::cout << elapsedValues[i] << "/" << deltas[i] << ", ";
    }
    std::cout << "\nnumFrames: " << statsNumFrames - 120 << ", average delta: " << double(statsJitterSum)/double(statsNumFrames - 120) << ", max: " << statsJitterMax << "\n";
*/
}

/*************************************************************************/

void Player::setFrameRate(float rate) {
    sdlSound.close();
    sdlSound.setFrameRate(rate);
    sdlSound.open();
}

/*************************************************************************/

void Player::startTimer() {
    doReplay = true;
    // Replay loop
/*
    while (doReplay) {
        double frameDuration = replayTvStandard == TiaSound::TvStandard::PAL ? 1000.0/50.0 : 1000.0/60.0;
        // Wait and allow events until shortly before next update
        while (elapsedTimer.elapsed() < lastReplayTime + (frameDuration - 10.0)) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        // Busy-wait until next update
        do {
            timestamp = elapsedTimer.elapsed();
        } while (timestamp < lastReplayTime + frameDuration);
        // Do updates
        while (lastReplayTime < timestamp) {
            timerFired();
            lastReplayTime += frameDuration;
        }
    }
*/

/*
    eTimer = new QElapsedTimer;
    eTimer->start();
*/
}

/*************************************************************************/

void Player::stopTimer() {
    //timer->stop();
    doReplay = false;
}

/*************************************************************************/

void Player::silence() {
    mode = PlayMode::None;
}

/*************************************************************************/

void Player::playInstrument(Track::Instrument *instrument, int frequency) {
    if (mode != PlayMode::Instrument) {
        currentInstrument = instrument;
        currentInstrumentFrequency = frequency;
        currentInstrumentFrame = 0;
        mode = PlayMode::Instrument;
    }
}

/*************************************************************************/

void Player::playInstrumentOnce(Track::Instrument *instrument, int frequency) {
    currentInstrument = instrument;
    currentInstrumentFrequency = frequency;
    currentInstrumentFrame = 0;
    mode = PlayMode::InstrumentOnce;
}

/*************************************************************************/

void Player::stopInstrument() {
    if (currentInstrument != nullptr) {
        currentInstrumentFrame = currentInstrument->getReleaseStart();
    }
}

/*************************************************************************/

void Player::playPercussion(Track::Percussion *percussion) {
    currentPercussion = percussion;
    currentPercussionFrame = 0;
    mode = PlayMode::Percussion;
}

/*************************************************************************/

void Player::stopPercussion() {
    mode = PlayMode::None;
}

/*************************************************************************/

void Player::stopTrack() {
    mode = PlayMode::None;
}

/*************************************************************************/

void Player::setChannel0(int distortion, int frequency, int volume) {
    sdlSound.set(AUDC0, uint8_t(distortion), 10);
    sdlSound.set(AUDV0, uint8_t(volume), 15);
    sdlSound.set(AUDF0, uint8_t(frequency), 18);
}

/*************************************************************************/

void Player::setChannel(int channel, int distortion, int frequency, int volume) {
    uint16_t audC = channel == 0 ? AUDC0 : AUDC1;
    uint16_t audV = channel == 0 ? AUDV0 : AUDV1;
    uint16_t audF = channel == 0 ? AUDF0 : AUDF1;
    sdlSound.set(audC, uint8_t(distortion), 10);
    sdlSound.set(audV, uint8_t(volume), 15);
    sdlSound.set(audF, uint8_t(frequency), 18);
}

/*************************************************************************/

void Player::playWaveform(TiaSound::Distortion waveform, int frequency, int volume) {
    int distortion = TiaSound::getDistortionInt(waveform);
    setChannel0(distortion, frequency, volume);
    mode = PlayMode::Waveform;
}

/*************************************************************************/

void Player::playTrack(int start1, int start2) {
    pTrack->lock();
    trackCurNoteIndex[0] = pTrack->getNoteIndexInPattern(0, start1);
    trackCurNoteIndex[1] = pTrack->getNoteIndexInPattern(1, start2);
    trackCurEntryIndex[0] = pTrack->getSequenceEntryIndex(0, start1);
    trackCurEntryIndex[1] = pTrack->getSequenceEntryIndex(1, start2);
    trackCurTick = 0;
    trackMode[0] = Track::Note::instrumentType::Hold;
    trackMode[1] = Track::Note::instrumentType::Hold;
    // InstrumentNumber -1 means: No note yet
    Track::Note startNote(Track::Note::instrumentType::Instrument, -1, -1);
    trackCurNote[0] = startNote;
    trackCurNote[1] = startNote;
    trackIsOverlay[0] = false;
    trackIsOverlay[1] = false;
    mode = PlayMode::Track;
    isFirstNote = true;
    pTrack->unlock();
}

/*************************************************************************/

void Player::selectedChannelChanged(int newChannel) {
    channelSelected = newChannel;
}

/*************************************************************************/

void Player::toggleLoop(bool toggle) {
    loopPattern = toggle;
}

/*************************************************************************/

void Player::setTVStandard(int iNewStandard) {
    replayTvStandard = static_cast<TiaSound::TvStandard>(iNewStandard);
    if (replayTvStandard == TiaSound::TvStandard::PAL) {
        setFrameRate(50.0);
    } else {
        setFrameRate(60.0);
    }
}

/*************************************************************************/

void Player::updateSilence() {
    setChannel(0, 0, 0, 0);
    setChannel(1, 0, 0, 0);
}

/*************************************************************************/

void Player::updateInstrument() {
    /* Play current frame */
    // Check if instrument has changed and made currentFrame illegal
    if (currentInstrumentFrame >= currentInstrument->getEnvelopeLength()) {
        // Go into silence if currentFrame is illegal
        mode = PlayMode::None;
    } else {
        int CValue = currentInstrument->getAudCValue(currentInstrumentFrequency);
        int realFrequency = currentInstrumentFrequency <= 31 ? currentInstrumentFrequency : currentInstrumentFrequency - 32;
        int FValue = realFrequency + currentInstrument->frequencies[currentInstrumentFrame];
        // Check if envelope has caused an underrun
        if (FValue < 0) {
            FValue = 256 + FValue;
        }
        int VValue = currentInstrument->volumes[currentInstrumentFrame];
        setChannel0(CValue, FValue, VValue);

        /* Advance frame */
        currentInstrumentFrame++;
        // Check for end of sustain or release
        if (mode != PlayMode::InstrumentOnce
                && currentInstrumentFrame == currentInstrument->getReleaseStart()) {
            currentInstrumentFrame = currentInstrument->getSustainStart();
        } else if (currentInstrumentFrame == currentInstrument->getEnvelopeLength()) {
            // End of release: Go into silence
            mode = PlayMode::None;
        }
    }
}

/*************************************************************************/

void Player::updatePercussion() {
    /* Play current frame */
    if (currentPercussionFrame >= currentPercussion->getEnvelopeLength()) {
        // Go into silence if currentFrame is illegal
        mode = PlayMode::None;
    } else {
        TiaSound::Distortion waveform = currentPercussion->waveforms[currentPercussionFrame];
        int CValue = TiaSound::getDistortionInt(waveform);
        int FValue = currentPercussion->frequencies[currentPercussionFrame];
        int VValue = currentPercussion->volumes[currentPercussionFrame];
        setChannel0(CValue, FValue, VValue);

        /* Advance frame. End of waveform is handled next frame */
        currentPercussionFrame++;
    }
}

/*************************************************************************/

void Player::sequenceChannel(int channel) {
    if (mode == PlayMode::None) {
        return;
    }
    // Get next note if not first one and not in overlay mode
    if (!isFirstNote && !trackIsOverlay[channel]
            && !pTrack->getNextNoteWithGoto(channel, &(trackCurEntryIndex[channel]), &(trackCurNoteIndex[channel]),
                                            channel == channelSelected && loopPattern)) {
        mode = PlayMode::None;
        return;
    }
    int patternIndex = pTrack->channelSequences[channel].sequence[trackCurEntryIndex[channel]].patternIndex;
    Track::Note *nextNote = &(pTrack->patterns[patternIndex].notes[trackCurNoteIndex[channel]]);
    // Parse and validate next note
    switch(nextNote->type) {
    case Track::Note::instrumentType::Hold:
        break;
    case Track::Note::instrumentType::Instrument:
        // No need to do anything if it has been pre-fetched by overlay already
        if (!trackIsOverlay[channel]) {
            trackMode[channel] = Track::Note::instrumentType::Instrument;
            trackCurNote[channel] = *nextNote;
            trackCurEnvelopeIndex[channel] = 0;
        }
        break;
    case Track::Note::instrumentType::Pause:
        if (trackMode[channel] != Track::Note::instrumentType::Instrument) {
            mode = PlayMode::None;
            updateSilence();
//             emit invalidNoteFound(channel, trackCurEntryIndex[channel], trackCurNoteIndex[channel],
//                                   "A pause can follow only after a melodic instrument!");
        } else {
            trackMode[channel] = Track::Note::instrumentType::Hold;
            Track::Instrument *curInstrument = &(pTrack->instruments[trackCurNote[channel].instrumentNumber]);
            trackCurEnvelopeIndex[channel] = curInstrument->getReleaseStart();
        }
        break;
    case Track::Note::instrumentType::Percussion:
        trackMode[channel] = Track::Note::instrumentType::Percussion;
        trackCurNote[channel] = *nextNote;
        trackCurEnvelopeIndex[channel] = 0;
        break;
    case Track::Note::instrumentType::Slide:
        if (trackMode[channel] != Track::Note::instrumentType::Instrument) {
            mode = PlayMode::None;
            updateSilence();
//             emit invalidNoteFound(channel, trackCurEntryIndex[channel], trackCurNoteIndex[channel],
//                                   "A slide can follow only after a melodic instrument!");
        } else {
            trackCurNote[channel].value += nextNote->value;
            // Check for over-/underflow
            if (trackCurNote[channel].value < 0
                    || (trackCurNote[channel].value)%32 > 31) {
                mode = PlayMode::None;
                updateSilence();
//                 emit invalidNoteFound(channel, trackCurEntryIndex[channel], trackCurNoteIndex[channel],
//                                       "A slide cannot change a frequency value to below 0 or above 31!");
            }
        }
        break;
    }
    trackIsOverlay[channel] = false;
}

/*************************************************************************/

void Player::updateChannel(int channel) {
    if (mode == PlayMode::None) {
        return;
    }
    // Play current note
    if (trackCurNote[channel].instrumentNumber != -1) {
        switch(trackCurNote[channel].type) {
        case Track::Note::instrumentType::Instrument:
        {
            Track::Instrument *curInstrument = &(pTrack->instruments[trackCurNote[channel].instrumentNumber]);
            int CValue = curInstrument->getAudCValue(trackCurNote[channel].value);
            // If at end of release, play silence; otherwise ADSR envelope
            if (trackCurEnvelopeIndex[channel] >= curInstrument->getEnvelopeLength()) {
                setChannel(channel, CValue, 0, 0);
            } else {
                int curFrequency = trackCurNote[channel].value;
                int realFrequency = curFrequency <= 31 ? curFrequency : curFrequency - 32;
                int FValue = realFrequency + curInstrument->frequencies[trackCurEnvelopeIndex[channel]];
                // Check if envelope has caused an underrun
                if (FValue < 0) {
                    FValue = 256 + FValue;
                }
                int VValue = curInstrument->volumes[trackCurEnvelopeIndex[channel]];
                setChannel(channel, CValue, FValue, VValue);
                // Advance frame
                trackCurEnvelopeIndex[channel]++;
                // Check for end of sustain
                if (trackCurEnvelopeIndex[channel] == curInstrument->getReleaseStart()) {
                    trackCurEnvelopeIndex[channel] = curInstrument->getSustainStart();
                }
            }
            break;
        }
        case Track::Note::instrumentType::Percussion:
        {
            Track::Percussion *curPercussion = &(pTrack->percussion[trackCurNote[channel].instrumentNumber]);
            if (!trackIsOverlay[channel]
                    && trackCurEnvelopeIndex[channel] < curPercussion->getEnvelopeLength()) {
                TiaSound::Distortion waveform = curPercussion->waveforms[trackCurEnvelopeIndex[channel]];
                int CValue = TiaSound::getDistortionInt(waveform);
                int FValue = curPercussion->frequencies[trackCurEnvelopeIndex[channel]];
                int VValue = curPercussion->volumes[trackCurEnvelopeIndex[channel]];
                setChannel(channel, CValue, FValue, VValue);
                // Advance frame
                trackCurEnvelopeIndex[channel]++;
                // Check for overlay
                if (trackCurEnvelopeIndex[channel] == curPercussion->getEnvelopeLength()
                        && curPercussion->overlay) {
                    // Get next note
                    if (!pTrack->getNextNoteWithGoto(channel, &(trackCurEntryIndex[channel]), &(trackCurNoteIndex[channel]))) {
                        mode = PlayMode::None;
                    } else {
                        trackIsOverlay[channel] = true;
                        int patternIndex = pTrack->channelSequences[channel].sequence[trackCurEntryIndex[channel]].patternIndex;
                        Track::Note *nextNote = &(pTrack->patterns[patternIndex].notes[trackCurNoteIndex[channel]]);
                        // Only start if melodic instrument
                        if (nextNote->type == Track::Note::instrumentType::Instrument) {
                            // Start in sustain
                            trackMode[channel] = Track::Note::instrumentType::Instrument;
                            trackCurNote[channel] = *nextNote;
                            trackCurEnvelopeIndex[channel] = pTrack->instruments[nextNote->instrumentNumber].getSustainStart();
                        }
                    }
                }
            } else {
                // Percussion has finished, or overlay is active but no instrument followed: Silence
                // Get last AUDC value, to mimick play routine. Needed?
                TiaSound::Distortion waveform = curPercussion->waveforms.back();
                int CValue = TiaSound::getDistortionInt(waveform);
                setChannel(channel, CValue, 0, 0);
            }
            break;
        }
        default:
            break;
        }
    }
}

/*************************************************************************/

void Player::updateTrack() {
    if (--trackCurTick < 0) {
        sequenceChannel(0);
        sequenceChannel(1);
        if (mode == PlayMode::None) {
            return;
        }
        isFirstNote = false;
        if (pTrack->globalSpeed) {
            trackCurTick = trackCurNoteIndex[0]%2 == 0 ? pTrack->oddSpeed - 1 : pTrack->evenSpeed - 1;
        } else {
            int patternIndex = pTrack->channelSequences[0].sequence[trackCurEntryIndex[0]].patternIndex;
            Track::Pattern *curPattern = &(pTrack->patterns[patternIndex]);
            trackCurTick = trackCurNoteIndex[0]%2 == 0 ? curPattern->oddSpeed - 1 : curPattern->evenSpeed - 1;
        }
//         int pos1 = pTrack->channelSequences[0].sequence[trackCurEntryIndex[0]].firstNoteNumber
//                 + trackCurNoteIndex[0];
//         int pos2 = pTrack->channelSequences[1].sequence[trackCurEntryIndex[1]].firstNoteNumber
//                 + trackCurNoteIndex[1];
//         emit newPlayerPos(pos1, pos2);
    }
    for (int channel = 0; channel < 2; ++channel) {
        if (!channelMuted[channel]) {
            updateChannel(channel);
        } else {
            setChannel(channel, 0, 0, 0);
        }
    }
}

/*************************************************************************/

void Player::timerFired() {
/*
    // Jitter test statistics
    long elapsed = (long)eTimer->elapsed();
    eTimer->restart();
    elapsedValues.append(elapsed);
    deltas.append(int(20 - elapsed));
    statsNumFrames++;
    if (statsNumFrames >= 120) {
        long delta = std::abs(20 - elapsed);
        statsJitterSum += delta;
        if (delta > statsJitterMax) {
            statsJitterMax = delta;
        }
    }
*/

    pTrack->lock();
    switch (mode) {
    case PlayMode::Instrument:
    case PlayMode::InstrumentOnce:
        updateInstrument();
        break;
    case PlayMode::Waveform:
        break;
    case PlayMode::Percussion:
        updatePercussion();
        break;
    case PlayMode::Track:
        updateTrack();
        break;
    default:
        updateSilence();
    }
    pTrack->unlock();
}

}
