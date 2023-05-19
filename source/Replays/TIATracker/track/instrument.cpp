/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#include "instrument.h"

#include <QMutex>

#include <stdexcept>
#include <iostream>
#include <QJsonObject>
#include "mainwindow.h"
#include "instrumentstab.h"


namespace Track {

/* TODO: More sensible empty detection, like a bool flag that gets
 * changed whenever the instrument data gets updated by the GUI...
 */
bool Instrument::isEmpty() {
    bool empty = true;
    if (name != "---"
            || envelopeLength > 2
            || frequencies.size() > 2
            || volumes[0] != 0 || volumes[1] != 0
            || frequencies[0] != 0 || frequencies[1] != 0
            || sustainStart != 0 || releaseStart != 1
            || baseDistortion != TiaSound::Distortion::PURE_COMBINED
            ) {
        empty = false;
    }

    return empty;
}

/*************************************************************************/

/*
void Instrument::toJson(QJsonObject &json) {
    json["version"] = MainWindow::version;
    json["name"] = name;
    json["waveform"] = static_cast<int>(baseDistortion);
    json["envelopeLength"] = envelopeLength;
    json["sustainStart"] = sustainStart;
    json["releaseStart"] = releaseStart;

    QJsonArray freqArray;
    for (int iFreq = 0; iFreq < envelopeLength; ++iFreq) {
        freqArray.append(QJsonValue(frequencies[iFreq]));
    }
    json["frequencies"] = freqArray;

    QJsonArray volArray;
    for (int iVol = 0; iVol < envelopeLength; ++iVol) {
        volArray.append(QJsonValue(volumes[iVol]));
    }
    json["volumes"] = volArray;
}
*/

/*************************************************************************/

bool Instrument::import(const QJsonObject &json) {
    int version = json["version"].get<int>();
    if (version > MainWindow::version) {
        MainWindow::displayMessage("An instrument is from a later version of TIATracker!");
        return false;
    }

    QString newName = json["name"].get<std::string>();
    int newWaveform = json["waveform"].get<int>();
    int newEnvelopeLength = json["envelopeLength"].get<int>();
    int newSustainStart = json["sustainStart"].get<int>();
    int newReleaseStart = json["releaseStart"].get<int>();
    auto& freqArray = json["frequencies"];
    auto& volArray = json["volumes"];

    // Check for data validity
    if (newName.length() > InstrumentsTab::maxInstrumentNameLength) {
        MainWindow::displayMessage("An instrument has an invalid name: " + newName);
        return false;
    }
    if (newWaveform < 0 || newWaveform > 16) {
        MainWindow::displayMessage("An instrument has an invalid waveform: " + newName);
        return false;
    }
    if (newEnvelopeLength < 2 || newEnvelopeLength > 99
            || newSustainStart < 0 || newSustainStart >= newEnvelopeLength - 1
            || newReleaseStart <= newSustainStart || newReleaseStart > newEnvelopeLength - 1) {
        MainWindow::displayMessage("An instrument has an invalid envelope structure: " + newName);
        return false;
    }
    if (newEnvelopeLength != freqArray.size() || newEnvelopeLength != volArray.size()) {
        MainWindow::displayMessage("An instrument has an invalid frequency envelope: " + newName);
        return false;
    }

    // Copy data. Adjust volumes or frequencies if necessary.
    frequencies.clear();
    volumes.clear();
    for (int frame = 0; frame < newEnvelopeLength; ++frame) {
        int newVolume = volArray[frame].get<int>();
        if (newVolume < 0) {
            newVolume = 0;
        }
        if (newVolume > 15) {
            newVolume = 15;
        }
        volumes.push_back(newVolume);
        int newFrequency = freqArray[frame].get<int>();
        if (newFrequency < -8) {
            newFrequency = -8;
        }
        if (newFrequency > 7) {
            newFrequency = 7;
        }
        frequencies.push_back(newFrequency);
    }
    name = newName;
    baseDistortion = TiaSound::distortions[newWaveform];
    envelopeLength = newEnvelopeLength;
    sustainStart = newSustainStart;
    releaseStart = newReleaseStart;

    return true;
}

/*************************************************************************/

void Instrument::setEnvelopeLength(int newSize) {
    if (newSize > volumes.size()) {
        // grow
        int lastVolume = volumes[volumes.size() - 1];
        int lastFrequency = frequencies[volumes.size() - 1];
        while (volumes.size() != newSize) {
            volumes.push_back(lastVolume);
            frequencies.push_back(lastFrequency);
        }
    }
    envelopeLength = newSize;
    validateSustainReleaseValues();
}

/*************************************************************************/

int Instrument::getAudCValue(int frequency) {
    int result;

    if (baseDistortion != TiaSound::Distortion::PURE_COMBINED) {
        result = (static_cast<int>(baseDistortion));
    } else {
        if (frequency < 32) {
            result = (static_cast<int>(TiaSound::Distortion::PURE_HIGH));
        } else {
            result = (static_cast<int>(TiaSound::Distortion::PURE_LOW));
        }
    }

    return result;
}

/*************************************************************************/

void Instrument::validateSustainReleaseValues() {
    if (releaseStart >= envelopeLength) {
        releaseStart = envelopeLength - 1;
    }
    if (sustainStart >= releaseStart) {
        sustainStart = releaseStart - 1;
    }
}

/*************************************************************************/

void Instrument::setSustainAndRelease(int newSustainStart, int newReleaseStart) {
    // TODO: change intro assert
    if (newReleaseStart <= newSustainStart
            || newSustainStart >= envelopeLength - 1
            || newReleaseStart >= envelopeLength) {
        throw std::invalid_argument("Invalid new sustain/release index values!");
    }
    sustainStart = newSustainStart;
    releaseStart = newReleaseStart;
}

/*************************************************************************/

void Instrument::deleteInstrument()
{
    name = "---";
    while (volumes.size() > 2) {
        volumes.pop_back();
        frequencies.pop_back();
    }
    volumes[0] = 0;
    volumes[1] = 0;
    frequencies[0] = 0;
    frequencies[1] = 0;
    envelopeLength = 2;
    sustainStart = 0;
    releaseStart = 1;
    baseDistortion = TiaSound::Distortion::PURE_COMBINED;
}

/*************************************************************************/

int Instrument::getMinVolume() {
    int min = volumes[0];
    for (int i = 1; i < volumes.size(); ++i) {
        min = qMin(min, volumes[i]);
    }
    return min;
}

/*************************************************************************/

int Instrument::getMaxVolume() {
    int max = volumes[0];
    for (int i = 1; i < volumes.size(); ++i) {
        max = qMax(max, volumes[i]);
    }
    return max;
}

/*************************************************************************/

void Instrument::correctSustainReleaseForInsert(int frame) {
    if (frame < sustainStart) {
        // in AD
        sustainStart++;
        releaseStart++;
    } else if (frame < releaseStart) {
        // in Sustain
        releaseStart++;
    }
}

void Instrument::insertFrameBefore(int frame) {
    if (envelopeLength == maxEnvelopeLength) {
        return;
    }
    envelopeLength++;

    correctSustainReleaseForInsert(frame);

    // Interpolate
    int volBefore = 0;
    int freqBefore = 0;
    if (frame != 0) {
        volBefore = volumes[frame - 1];
        freqBefore = frequencies[frame - 1];
    }
    int volAfter = volumes[frame];
    int newVol = int((volAfter + volBefore)/2);
    int freqAfter = frequencies[frame];
    int freqNew = int((freqAfter + freqBefore)/2);

    volumes.insert(volumes.begin() + frame, newVol);
    frequencies.insert(frequencies.begin() + frame, freqNew);
}

/*************************************************************************/

void Instrument::insertFrameAfter(int frame) {
    if (envelopeLength == maxEnvelopeLength) {
        return;
    }
    envelopeLength++;

    correctSustainReleaseForInsert(frame);

    // Interpolate
    int volBefore = volumes[frame];
    int freqBefore = frequencies[frame];
    int volAfter = 0;
    int freqAfter = 0;
    // -1 b/c we increased length already
    if (frame + 1 != envelopeLength - 1) {
        volAfter = volumes[frame + 1];
        freqAfter = frequencies[frame + 1];
    }
    int newVol = int((volAfter + volBefore)/2);
    int newFreq = int((freqAfter + freqBefore)/2);

    volumes.insert(volumes.begin() + frame + 1, newVol);
    frequencies.insert(frequencies.begin() + frame + 1, newFreq);
}

/*************************************************************************/

void Instrument::deleteFrame(int frame) {
    if (envelopeLength == 2) {
        return;
    }
    envelopeLength--;

    // Correct sustain and release
    if (frame < sustainStart) {
        // in AD
        sustainStart--;
        releaseStart--;
    } else if (frame < releaseStart) {
        // in Sustain
        if (releaseStart - sustainStart > 1) {
            releaseStart--;
        } else {
            // Trying to delete single sustain frame.
            if (sustainStart > 0) {
                // Move sustain one back
                sustainStart--;
                releaseStart--;
            } else {
                // keep sustain at this position
            }
        }
    } else {
        // in Release
        if (releaseStart - sustainStart == 1) {
            // Move Release back one frame
            sustainStart--;
            releaseStart--;
        }
    }

    // Delete frames
    volumes.erase(volumes.begin() + frame);
    frequencies.erase(frequencies.begin() + frame);
}

/*************************************************************************/

int Instrument::getSustainStart() const {
    return sustainStart;
}

int Instrument::getReleaseStart() const {
    return releaseStart;
}

/*************************************************************************/

int Instrument::calcEffectiveSize() {
    int realSize = envelopeLength;
    while (realSize > (releaseStart + 1) && frequencies[realSize - 1] == 0 && volumes[realSize - 1] == 0) {
        realSize--;
    }
    return realSize + 1;    // +1 for "end of instrument" 0 byte
}

/*************************************************************************/

int Instrument::getEnvelopeLength() {
    return envelopeLength;
}

}
