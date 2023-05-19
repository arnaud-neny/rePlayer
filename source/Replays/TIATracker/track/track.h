/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef TRACK_H
#define TRACK_H

#include "instrument.h"
#include "percussion.h"
#include <QList>
#include <QMutex>
#include "tiasound/tiasound.h"
#include "pattern.h"
#include "sequence.h"
#include <QJsonObject>
#include "tiasound/pitchguide.h"


namespace Track {

/* Represents a TIATracker track with instruments, percussion, patterns
 * and meta-data.
 */
class Track
{
public:
    static const int numInstruments = 7;
    static const int numPercussion = 15;

    Track();

    /* Lock or unlock track for thread-safe operations like modifying
     * envelope length of an instrument */
    void lock();
    void unlock();

    void newTrack();

    /* Counts all envelope frames over all instruments */
    int getNumUsedEnvelopeFrames();
    int getNumUsedPercussionFrames();

    int getNumUsedInstruments();
    int getNumUsedPercussion();

    /* Get number of rows of one channel */
    int getChannelNumRows(int channel) const;

    /* Get max number of rows of both channels */
    int getTrackNumRows() const;

    /* Sets "firstNoteNumber" values of all sequence entries */
    void updateFirstNoteNumbers();

    /* For a given channel, return the index of the SequenceEntry
     * a given (valid) row is in. */
    int getSequenceEntryIndex(int channel, int row);

    int getPatternIndex(int channel, int row);

    int getNoteIndexInPattern(int channel, int row);

    /* Get index of next note (and entry, if new pattern is reached)
     * for a specific channel.
     * Returns false if there is no next note. */
    bool getNextNote(int channel, int *pEntryIndex, int *pPatternNoteIndex);

    /* Get index of next note (and entry, if new pattern is reached)
     * for a specific channel, observing goto.
     * Returns false if there is no next note. */
    bool getNextNoteWithGoto(int channel, int *pEntryIndex, int *pPatternNoteIndex, bool loop = false);
    /* Returns -1 if there is no next note */
    int getNextNoteWithGoto(int channel, int row);

    bool checkSlideValidity(int channel, int row);

    int skipInstrumentType(int channel, int row, Note::instrumentType type, int direction);

    Note *getNote(int channel, int row);

    /* Feature checks */
    bool usesGoto();
    bool startsWithHold();
    bool usesSlide();
    bool usesOverlay();
    bool usesFunktempo();
    int getNumInstrumentsFromTrack();
    int getNumPercussionFromTrack();
    int calcInstrumentsSize();
    int calcPercussionSize();
    int numPatternsUsed();
    int calcPatternSize();
    int sequencesSize();

    void toJson(QJsonObject &json);
    bool fromJson(const QJsonObject &json);

    TiaSound::TvStandard getTvMode() const;
    void setTvMode(const TiaSound::TvStandard &value);

    QString name{"new track.ttt"};
    QList<Instrument> instruments{
        {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}
    };
    QList<Percussion> percussion{
        {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"},
        {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}, {"---"}
    };

    QList<Pattern> patterns{};
    QList<Sequence> channelSequences{{}, {}};
    bool globalSpeed = true;
    int evenSpeed = 5;
    int oddSpeed = 5;
    int rowsPerBeat = 4;
    int startPatterns[2]{};
    int playPos[2]{};

    QString guideName;
    double guideBaseFreq = 0.0;
    TiaSound::TvStandard guideTvStandard = TiaSound::TvStandard::PAL;

    QString metaAuthor;
    QString metaName;
    QString metaComment;

private:
    QMutex mutex;

    TiaSound::TvStandard tvMode = TiaSound::TvStandard::PAL;
};

}

#endif // TRACK_H
