/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#include "track.h"
#include <QString>
#include "sequenceentry.h"
#include <iostream>
#include "mainwindow.h"


namespace Track {

Track::Track() {
}

/*************************************************************************/

void Track::lock() {
    mutex.lock();
}

/*************************************************************************/

void Track::unlock() {
    mutex.unlock();
}

/*************************************************************************/

void Track::newTrack() {
    name = "new track.ttt";
    instruments = QList<Instrument>{
        {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}
    };
    percussion = QList<Percussion>{
        {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"},
        {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}
    };

    patterns.clear();
    Pattern newPatternLeft("Intro left");
    Note newNote(Note::instrumentType::Hold, 0, 0);
    for (int i = 0; i < 16; ++i) {
        newPatternLeft.notes.push_back(newNote);
    }
    patterns.push_back(newPatternLeft);
    Pattern newPatternRight("Intro right");
    for (int i = 0; i < 16; ++i) {
        newPatternRight.notes.push_back(newNote);
    }
    patterns.push_back(newPatternRight);
    channelSequences = QList<Sequence>{{}, {}};
    SequenceEntry newEntryLeft(0, -1);
    SequenceEntry newEntryRight(1, -1);
    channelSequences[0].sequence.push_back(newEntryLeft);
    channelSequences[1].sequence.push_back(newEntryRight);
    globalSpeed = true;
    evenSpeed = 5;
    oddSpeed = 5;
    rowsPerBeat = 4;
    startPatterns[0] = 0;
    startPatterns[1] = 0;
    playPos[0] = 0;
    playPos[1] = 0;
    tvMode = TiaSound::TvStandard::PAL;
    metaAuthor = "";
    metaName = "";
    metaComment = "";
    guideName = "";
    guideBaseFreq = 0.0;
    guideTvStandard = TiaSound::TvStandard::PAL;

    updateFirstNoteNumbers();
}

/*************************************************************************/

int Track::getNumUsedEnvelopeFrames() {
    int numFrames = 0;
    for (int i = 0; i < numInstruments; ++i) {
        if (!instruments[i].isEmpty()) {
            // +1 for dummy byte between sustain and release
            int envelopeLength = instruments[i].getEnvelopeLength();
            // +1 for dummy byte between sustain and release
            numFrames += envelopeLength + 1;
            // If last volume/frequency frames are not 0, add one (0 is mandatory)
            int iLast = envelopeLength - 1;
            if (instruments[i].volumes[iLast] != 0 || instruments[i].frequencies[iLast] != 0) {
                numFrames++;
            }
        }
    }
    return numFrames;
}

/*************************************************************************/

int Track::getNumUsedPercussionFrames() {
    int numFrames = 0;
    for (int i = 0; i < numPercussion; ++i) {
        if (!percussion[i].isEmpty()) {
            int envelopeLength = percussion[i].getEnvelopeLength();
            numFrames += envelopeLength;
            // If last ctrl/volume frames are not 0, add one (0 is mandatory)
            int iLast = envelopeLength - 1;
            if (percussion[i].volumes[iLast] != 0
                    || percussion[i].waveforms[iLast] != TiaSound::Distortion::SILENT) {
                numFrames++;
            }
        }
    }
    return numFrames;
}

/*************************************************************************/

int Track::getNumUsedInstruments() {
    int usedInstruments = 0;
    for (int i = 0; i < numInstruments; ++i) {
        if (!instruments[i].isEmpty()) {
            usedInstruments++;
            if (instruments[i].baseDistortion == TiaSound::Distortion::PURE_COMBINED) {
                usedInstruments++;
            }
        }
    }
    return usedInstruments;
}

/*************************************************************************/

int Track::getNumUsedPercussion()
{
    int usedPercussion = 0;
    for (int i = 0; i < numPercussion; ++i) {
        if (!percussion[i].isEmpty()) {
            usedPercussion++;
        }
    }
    return usedPercussion;
}

/*************************************************************************/

int Track::getChannelNumRows(int channel) const {
    int lastPattern = channelSequences[channel].sequence.back().patternIndex;
    return channelSequences[channel].sequence.back().firstNoteNumber
            + int(patterns[lastPattern].notes.size());
}

/*************************************************************************/

int Track::getTrackNumRows() const {
    return std::max(getChannelNumRows(0), getChannelNumRows(1));
}

/*************************************************************************/

void Track::updateFirstNoteNumbers() {
    for (int channel = 0; channel < 2; ++channel) {
        int noteNumber = 0;
        for (int entry = 0; entry < channelSequences[channel].sequence.size(); ++entry) {
            channelSequences[channel].sequence[entry].firstNoteNumber = noteNumber;
            int iPattern = channelSequences[channel].sequence[entry].patternIndex;
            noteNumber += int(patterns[iPattern].notes.size());
        }
    }
}

/*************************************************************************/

int Track::getSequenceEntryIndex(int channel, int row) {
    int entryIndex = 0;
    SequenceEntry *curEntry = &(channelSequences[channel].sequence[0]);
    Pattern *curPattern = &(patterns[curEntry->patternIndex]);
    while (row >= curEntry->firstNoteNumber + curPattern->notes.size()) {
        entryIndex++;
        curEntry = &(channelSequences[channel].sequence[entryIndex]);
        curPattern = &(patterns[curEntry->patternIndex]);
    }
    return entryIndex;
}

/*************************************************************************/

int Track::getPatternIndex(int channel, int row) {
    int entryIndex = getSequenceEntryIndex(channel, row);
    return channelSequences[channel].sequence[entryIndex].patternIndex;
}

/*************************************************************************/

int Track::getNoteIndexInPattern(int channel, int row) {
    int entryIndex = getSequenceEntryIndex(channel, row);
    return row - channelSequences[channel].sequence[entryIndex].firstNoteNumber;
}

/*************************************************************************/

bool Track::getNextNote(int channel, int *pEntryIndex, int *pPatternNoteIndex) {
    SequenceEntry *curEntry = &(channelSequences[channel].sequence[*pEntryIndex]);
    Pattern *curPattern = &(patterns[curEntry->patternIndex]);
    (*pPatternNoteIndex)++;
    if (*pPatternNoteIndex == curPattern->notes.size()) {
        *(pEntryIndex) = *(pEntryIndex) + 1;
        if (*pEntryIndex == channelSequences[channel].sequence.size()) {
            // End of track reached: There is no next note
            return false;
        }
        *(pPatternNoteIndex) = 0;
    }
    return true;
}

/*************************************************************************/

bool Track::getNextNoteWithGoto(int channel, int *pEntryIndex, int *pPatternNoteIndex, bool loop) {
    SequenceEntry *curEntry = &(channelSequences[channel].sequence[*pEntryIndex]);
    Pattern *curPattern = &(patterns[curEntry->patternIndex]);
    (*pPatternNoteIndex)++;
    if (*pPatternNoteIndex == curPattern->notes.size()) {
        if (!loop) {
            if (curEntry->gotoTarget == -1) {
                *(pEntryIndex) = *(pEntryIndex) + 1;
            } else {
                *(pEntryIndex) = curEntry->gotoTarget;
            }
            if (*pEntryIndex >= channelSequences[channel].sequence.size()) {
                // End of track or invalid goto: There is no next note
                return false;
            }
        }
        *(pPatternNoteIndex) = 0;
    }
    return true;
}

/*************************************************************************/

int Track::getNextNoteWithGoto(int channel, int row) {
    int entryIndex = getSequenceEntryIndex(channel, row);
    int noteIndex = getNoteIndexInPattern(channel, row);
    if (!getNextNoteWithGoto(channel, &entryIndex, &noteIndex, false)) {
        return -1;
    }
    return channelSequences[channel].sequence[entryIndex].firstNoteNumber + noteIndex;
}

/*************************************************************************/

bool Track::checkSlideValidity(int channel, int row) {
    // Skip slides and holds immediately before
    int prevRow = row;
    Note *n;
    do {
        prevRow = skipInstrumentType(channel, prevRow, Note::instrumentType::Slide, -1);
        if (prevRow == -1) {
            return false;
        }
        n = getNote(channel, prevRow);
    } while(n->type == Note::instrumentType::Hold);

    if (n->type != Note::instrumentType::Instrument) {
        return false;
    }
    return true;
}

/*************************************************************************/

int Track::skipInstrumentType(int channel, int row, Note::instrumentType type, int direction) {
    int channelRows = getChannelNumRows(channel);
    do {
        row += direction;
        if (row == -1 || row == channelRows) {
            return -1;
        }
    } while (getNote(channel, row)->type == type);
    return row;
}

/*************************************************************************/

Note *Track::getNote(int channel, int row) {
    int entryIndex = getSequenceEntryIndex(channel, row);
    int patternIndex = channelSequences[channel].sequence[entryIndex].patternIndex;
    int noteIndex = row - channelSequences[channel].sequence[entryIndex].firstNoteNumber;
    return &(patterns[patternIndex].notes[noteIndex]);
}

/*************************************************************************/

bool Track::usesGoto() {
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < channelSequences[channel].sequence.size(); ++i) {
            if (channelSequences[channel].sequence[i].gotoTarget != -1) {
                return true;
            }
        }
    }
    return false;
}

/*************************************************************************/

bool Track::startsWithHold() {
    return (getNote(0, 0)->type == Note::instrumentType::Hold || getNote(1, 0)->type == Note::instrumentType::Hold);
}

/*************************************************************************/

bool Track::usesSlide() {
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < getChannelNumRows(channel); ++i) {
            if (getNote(channel, i)->type == Note::instrumentType::Slide) {
                return true;
            }
        }
    }
    return false;
}

/*************************************************************************/

bool Track::usesOverlay() {
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < getChannelNumRows(channel); ++i) {
            Note *n = getNote(channel, i);
            if (n->type == Note::instrumentType::Percussion && percussion[n->instrumentNumber].overlay) {
                return true;
            }
        }
    }
    return false;
}

/*************************************************************************/

bool Track::usesFunktempo() {
    if (globalSpeed) {
        return (evenSpeed != oddSpeed);
    }
    for (int i = 0; i < channelSequences[0].sequence.size(); ++i) {
        int patternIndex = channelSequences[0].sequence[i].patternIndex;
        if (patterns[patternIndex].evenSpeed != patterns[patternIndex].oddSpeed) {
            return true;
        }
    }
    return false;
}

/*************************************************************************/

int Track::getNumInstrumentsFromTrack() {
    QList<int> found{0, 0, 0, 0, 0, 0, 0};
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < getChannelNumRows(channel); ++i) {
            Note *n = getNote(channel, i);
            if (n->type == Note::instrumentType::Instrument) {
                if (instruments[n->instrumentNumber].baseDistortion == TiaSound::Distortion::PURE_COMBINED) {
                    found[n->instrumentNumber] = 2;
                } else {
                    found[n->instrumentNumber] = 1;
                }
            }
        }
    }
    int result = 0;
    for (int i = 0; i < numInstruments; ++i) {
        result += found[i];
    }
    return result;
}

/*************************************************************************/

int Track::getNumPercussionFromTrack() {
    QList<int> found{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < getChannelNumRows(channel); ++i) {
            Note *n = getNote(channel, i);
            if (n->type == Note::instrumentType::Percussion) {
                found[n->instrumentNumber] = 1;
            }
        }
    }
    int result = 0;
    for (int i = 0; i < numPercussion; ++i) {
        result += found[i];
    }
    return result;
}

/*************************************************************************/

int Track::calcInstrumentsSize() {
    QList<int> found{0, 0, 0, 0, 0, 0, 0};
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < getChannelNumRows(channel); ++i) {
            Note *n = getNote(channel, i);
            if (n->type == Note::instrumentType::Instrument) {
                if (instruments[n->instrumentNumber].baseDistortion == TiaSound::Distortion::PURE_COMBINED) {
                    found[n->instrumentNumber] = 2;
                } else {
                    found[n->instrumentNumber] = 1;
                }
            }
        }
    }
    int result = 0;
    for (int i = 0; i < numInstruments; ++i) {
        if (found[i] != 0) {
            result += found[i]*Emulation::Player::RomPerInstrument;
            result += instruments[i].calcEffectiveSize();
        }
    }
    return result;
}

/*************************************************************************/

int Track::calcPercussionSize() {
    QList<int> found{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < getChannelNumRows(channel); ++i) {
            Note *n = getNote(channel, i);
            if (n->type == Note::instrumentType::Percussion) {
                found[n->instrumentNumber] = 1;
            }
        }
    }
    int result = 0;
    for (int i = 0; i < numPercussion; ++i) {
        if (found[i] != 0) {
            result += Emulation::Player::RomPerPercussion;
            // -1 because calcEffectiveSize includes end marker that is also in RomPerPercussion
            result += 2*(percussion[i].calcEffectiveSize() - 1);
        }
    }
    return result;
}

/*************************************************************************/

int Track::numPatternsUsed() {
    QMap<int, bool> found{};
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < channelSequences[channel].sequence.size(); ++i) {
            found[channelSequences[channel].sequence[i].patternIndex] = true;
        }
    }
//     QMapIterator<int, bool> i(found);
//     while (i.hasNext()) {
//         i.next();
//     }
    return int(found.size());
}

/*************************************************************************/

int Track::calcPatternSize() {
    QMap<int, bool> found;
    for (int channel = 0; channel < 2; ++channel) {
        for (int i = 0; i < channelSequences[channel].sequence.size(); ++i) {
            found[channelSequences[channel].sequence[i].patternIndex] = true;
        }
    }
    int result = 0;
    for (auto& i : found) {
        result += Emulation::Player::RomPerPattern + int(patterns[i.first].notes.size());
    }
    return result;
}

/*************************************************************************/

int Track::sequencesSize() {
    return int(channelSequences[0].sequence.size()) + int(channelSequences[1].sequence.size())
            + 2*Emulation::Player::RomPerSequence;
}

/*************************************************************************/

/*
void Track::toJson(QJsonObject &json) {
    // General data
    json["version"] = MainWindow::version;
    if (tvMode == TiaSound::TvStandard::PAL) {
        json["tvmode"] = "pal";
    } else {
        json["tvmode"] = "ntsc";
    }
    json["globalspeed"] = globalSpeed;
    json["evenspeed"] = evenSpeed;
    json["oddspeed"] = oddSpeed;
    json["rowsperbeat"] = rowsPerBeat;
    json["startpattern0"] = startPatterns[0];
    json["startpattern1"] = startPatterns[1];
    // Pitch Guide is optional
    if (guideBaseFreq != 0.0) {
        json["pitchGuideName"] = guideName;
        json["pitchGuideBaseFrequency"] = guideBaseFreq;
        json["pitchGuideTvStandard"] = (guideTvStandard == TiaSound::TvStandard::PAL ? "PAL" : "NTSC");
    }
    json["metaAuthor"] = metaAuthor;
    json["metaName"] = metaName;
    json["metaComment"] = metaComment;

    // Instruments
    QJsonArray insArray;
    for (int i = 0; i < numInstruments; ++i) {
        QJsonObject insJson;
        instruments[i].toJson(insJson);
        insArray.append(insJson);
    }
    json["instruments"] = insArray;

    // Percussion
    QJsonArray percArray;
    for (int i = 0; i < numPercussion; ++i) {
        QJsonObject percJson;
        percussion[i].toJson(percJson);
        percArray.append(percJson);
    }
    json["percussion"] = percArray;

    // Patterns
    QJsonArray pattArray;
    for (int i = 0; i < patterns.size(); ++i) {
        QJsonObject pattJson;
        patterns[i].toJson(pattJson);
        pattArray.append(pattJson);
    }
    json["patterns"] = pattArray;

    // Sequences
    QJsonArray chanArray;
    for (int i = 0; i < 2; ++i) {
        QJsonObject chanJson;
        channelSequences[i].toJson(chanJson);
        chanArray.append(chanJson);
    }
    json["channels"] = chanArray;
}
*/

/*************************************************************************/

bool Track::fromJson(const QJsonObject &json) {
    if (!json.contains("version"))
        return false;
    int version = json["version"].get<int>();
    if (version > MainWindow::version) {
        MainWindow::displayMessage("This song is from a later version of TIATracker!");
        return false;
    }
    if (!json.contains("tvmode"))
        return false;
    if (json["tvmode"] == "pal") {
        tvMode = TiaSound::TvStandard::PAL;
    } else if (json["tvmode"] == "ntsc") {
        tvMode = TiaSound::TvStandard::NTSC;
    } else {
        MainWindow::displayMessage("Invalid tv mode!");
        return false;
    }
    if (!json.contains("evenspeed"))
        return false;
    evenSpeed = json["evenspeed"].get<int>();
    if (json.contains("globalspeed")) {
        globalSpeed = json["globalspeed"].get<bool>();
    } else {
        globalSpeed = true;
    }
    oddSpeed = json["oddspeed"].get<int>();
    rowsPerBeat = json["rowsperbeat"].get<int>();
    // Start patterns
    if (json.contains("startpattern0")) {
        startPatterns[0] = json["startpattern0"].get<int>();
    } else {
        startPatterns[0] = 0;
    }
    if (json.contains("startpattern1")) {
        startPatterns[1] = json["startpattern1"].get<int>();
    } else {
        startPatterns[1] = 0;
    }
    // Pitch Guide is optional
    if (json.contains("pitchGuideBaseFrequency")) {
        guideBaseFreq = json["pitchGuideBaseFrequency"].get<double>();
        guideName = json["pitchGuideName"].get<std::string>();
        guideTvStandard = (json["pitchGuideTvStandard"].get<std::string>() == "PAL" ? TiaSound::TvStandard::PAL : TiaSound::TvStandard::NTSC);
    } else {
        guideBaseFreq = 0.0;
    }
    // Author and name are optional
    if (json.contains("metaAuthor")) {
        metaAuthor = json["metaAuthor"].get<std::string>();
        metaName = json["metaName"].get<std::string>();
    } else {
        metaAuthor = "";
        metaName = "";
    }
    // Comment is optional
    if (json.contains("metaComment")) {
        metaComment = json["metaComment"].get<std::string>();
    } else {
        metaComment = "";
    }

    // Instruments
    auto& insArray = json["instruments"];
    if (insArray.size() != numInstruments) {
        MainWindow::displayMessage("Invalid number of instruments!");
        return false;
    }
    instruments.clear();
    for (int i = 0; i < numInstruments; ++i) {
        Instrument newIns{""};
        if (!newIns.import(insArray[i])) {
            return false;
        }
        instruments.push_back(newIns);
    }

    // Percussion
    auto& percArray = json["percussion"];
    if (percArray.size() != numPercussion) {
        MainWindow::displayMessage("Invalid number of percussions!");
        return false;
    }
    percussion.clear();
    for (int i = 0; i < numPercussion; ++i) {
        Percussion newPerc{""};
        if (!newPerc.import(percArray[i])) {
            return false;
        }
        percussion.push_back(newPerc);
    }

    // Patterns
    auto& pattArray = json["patterns"];
    patterns.clear();
    for (int i = 0; i < pattArray.size(); ++i) {
        Pattern newPatt;
        if (!newPatt.fromJson(pattArray[i])) {
            return false;
        }
        patterns.push_back(newPatt);
    }

    // Sequences
    auto& chanArray = json["channels"];
    if (chanArray.size() != 2) {
        MainWindow::displayMessage("There are not exactly 2 sequences!");
        return false;
    }
    channelSequences.clear();
    for (int i = 0; i < 2; ++i) {
        Sequence newSeq;
        if (!newSeq.fromJson(chanArray[i])) {
            return false;
        }
        channelSequences.push_back(newSeq);
    }

    updateFirstNoteNumbers();
    return true;
}

/*************************************************************************/

TiaSound::TvStandard Track::getTvMode() const {
    return tvMode;
}

/*************************************************************************/

void Track::setTvMode(const TiaSound::TvStandard &value) {
    tvMode = value;
}

}
