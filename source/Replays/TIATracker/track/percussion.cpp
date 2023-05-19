/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#include "percussion.h"
#include <QJsonObject>
#include "mainwindow.h"
#include "percussiontab.h"


namespace Track {

/* TODO: More sensible empty detection, like a bool flag that gets
 * changed whenever the percussion data gets updated by the GUI...
 */
bool Percussion::isEmpty() {
    bool empty = true;
    if (name != "---"
            || envelopeLength > 1
            || frequencies.size() > 1
            || waveforms.size() > 1
            || volumes[0] != 0
            || frequencies[0] != 0
            || waveforms[0] != TiaSound::Distortion::WHITE_NOISE
            || overlay
            ) {
        empty = false;
    }

    return empty;
}

/*************************************************************************/

int Percussion::getEnvelopeLength() {
    return envelopeLength;
}

/*************************************************************************/

void Percussion::setEnvelopeLength(int newSize) {
    if (newSize > volumes.size()) {
        // grow
        int lastVolume = volumes[volumes.size() - 1];
        int lastFrequency = frequencies[volumes.size() - 1];
        TiaSound::Distortion lastWaveform = waveforms[volumes.size() - 1];
        while (volumes.size() != newSize) {
            volumes.push_back(lastVolume);
            frequencies.push_back(lastFrequency);
            waveforms.push_back(lastWaveform);
        }
    }
    envelopeLength = newSize;
}

/*************************************************************************/

/*
void Percussion::toJson(QJsonObject &json) {
    json["version"] = MainWindow::version;
    json["name"] = name;
    json["envelopeLength"] = envelopeLength;
    json["overlay"] = overlay;

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

    QJsonArray waveformArray;
    for (int iWave = 0; iWave < envelopeLength; ++iWave) {
        int waveformValue = TiaSound::getDistortionInt(waveforms[iWave]);
        waveformArray.append(QJsonValue(waveformValue));
    }
    json["waveforms"] = waveformArray;

}
*/

/*************************************************************************/

bool Percussion::import(const QJsonObject &json) {
    int version = json["version"].get<int>();
    if (version > MainWindow::version) {
        MainWindow::displayMessage("A percussion is from a later version of TIATracker!");
        return false;
    }

    QString newName = json["name"].get<std::string>();
    int newEnvelopeLength = json["envelopeLength"].get<int>();
    bool newOverlay = json["overlay"].get<bool>();
    auto& freqArray = json["frequencies"];
    auto& volArray = json["volumes"];
    auto& waveformArray = json["waveforms"];

    // Check for data validity
    if (newName.length() > PercussionTab::maxPercussionNameLength) {
        MainWindow::displayMessage("A percussion has an invalid name: " + newName);
        return false;
    }
    if (newEnvelopeLength < 1 || newEnvelopeLength > 99) {
        MainWindow::displayMessage("A percussion has an invalid envelope length: " + newName);
        return false;
    }
    if (newEnvelopeLength != freqArray.size() || newEnvelopeLength != volArray.size()
            || newEnvelopeLength != waveformArray.size()) {
        MainWindow::displayMessage("A percussion has an invalid envelope: " + newName);
        return false;
    }

    // Copy data. Adjust volumes, frequencies or waveforms if necessary.
    frequencies.clear();
    volumes.clear();
    waveforms.clear();
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
        if (newFrequency < 0) {
            newFrequency = 0;
        }
        if (newFrequency > 31) {
            newFrequency = 31;
        }
        frequencies.push_back(newFrequency);
        int newWaveform = waveformArray[frame].get<int>();
        if (newWaveform < 0 || newWaveform > 15) {
            newWaveform = 0;
        }
        TiaSound::Distortion newDist = TiaSound::distortions[newWaveform];
        waveforms.push_back(newDist);

    }
    name = newName;
    envelopeLength = newEnvelopeLength;
    overlay = newOverlay;

    return true;
}

/*************************************************************************/

void Percussion::deletePercussion() {
    name = "---";
    while (volumes.size() > 1) {
        volumes.pop_back();
        frequencies.pop_back();
        waveforms.pop_back();
    }
    volumes[0] = 0;
    frequencies[0] = 0;
    waveforms[0] = TiaSound::Distortion::WHITE_NOISE;
    envelopeLength = 1;
    overlay = false;
}

/*************************************************************************/

int Percussion::getMinVolume() {
    int min = volumes[0];
    for (int i = 1; i < volumes.size(); ++i) {
        min = qMin(min, volumes[i]);
    }
    return min;
}

/*************************************************************************/

int Percussion::getMaxVolume() {
    int max = volumes[0];
    for (int i = 1; i < volumes.size(); ++i) {
        max = qMax(max, volumes[i]);
    }
    return max;

}

/*************************************************************************/

void Percussion::insertFrameBefore(int frame) {
    if (envelopeLength == maxEnvelopeLength) {
        return;
    }
    envelopeLength++;

    // Interpolate
    int volBefore = 0;
    int freqBefore = frequencies[frame];
    if (frame != 0) {
        volBefore = volumes[frame - 1];
        freqBefore = frequencies[frame - 1];
    }
    int volAfter = volumes[frame];
    int newVol = int((volAfter + volBefore)/2);
    int freqAfter = frequencies[frame];
    int freqNew = int((freqAfter + freqBefore)/2);
    TiaSound::Distortion distNew = waveforms[frame];

    volumes.insert(volumes.begin() + frame, newVol);
    frequencies.insert(frequencies.begin() + frame, freqNew);
    waveforms.insert(waveforms.begin() + frame, distNew);
}

/*************************************************************************/

void Percussion::insertFrameAfter(int frame) {
    if (envelopeLength == maxEnvelopeLength) {
        return;
    }
    envelopeLength++;

    // Interpolate
    int volBefore = volumes[frame];
    int freqBefore = frequencies[frame];
    int volAfter = 0;
    int freqAfter = frequencies[frame];
    // -1 b/c we increased length already
    if (frame + 1 != envelopeLength - 1) {
        volAfter = volumes[frame + 1];
        freqAfter = frequencies[frame + 1];
    }
    int newVol = int((volAfter + volBefore)/2);
    int newFreq = int((freqAfter + freqBefore)/2);
    TiaSound::Distortion distNew = waveforms[frame];

    volumes.insert(volumes.begin() + frame + 1, newVol);
    frequencies.insert(frequencies.begin() + frame + 1, newFreq);
    waveforms.insert(waveforms.begin() + frame + 1, distNew);
}

/*************************************************************************/

void Percussion::deleteFrame(int frame) {
    if (envelopeLength == 1) {
        return;
    }
    envelopeLength--;

    // Delete frames
    volumes.erase(volumes.begin() + frame);
    frequencies.erase(frequencies.begin() + frame);
    waveforms.erase(waveforms.begin() + frame);
}

/*************************************************************************/

int Percussion::calcEffectiveSize() {
    int realSize = envelopeLength;
    while (realSize > 0 && waveforms[realSize - 1] == TiaSound::Distortion::SILENT && volumes[realSize - 1] == 0) {
        realSize--;
    }
    return realSize + 1;    // +1 for End byte
}

}
