/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include <QString>
#include <QList>
#include <QMutex>
#include "tiasound/tiasound.h"
#include <QJsonObject>


namespace Track {

/* A TIATracker instrument, based on a base distortion and ADSR envelopes
 * for volume and frequency.
 */
class Instrument
{
public:
    static const int maxEnvelopeLength = 99;

    Instrument(QString name) : name(name) {}

    int getEnvelopeLength();
    void setEnvelopeLength(int newSize);

    // Return AUDC value. Can be different per frequency for PURE COMBINED.
    int getAudCValue(int frequency);

    // Checks if instrument has its empty starting values
    bool isEmpty();

//     void toJson(QJsonObject &json);
    bool import(const QJsonObject &json);

    void deleteInstrument();

    /* Checks if release starts after sustain and if not, changes
     * values accordingly. */
    void validateSustainReleaseValues();
    void setSustainAndRelease(int newSustainStart, int newReleaseStart);

    /* Get minimum volume over the whole envelope */
    int getMinVolume();

    /* Get maximum volume over the whole envelope */
    int getMaxVolume();

    /* Insert a new frame before the specified */
    void insertFrameBefore(int frame);
    void insertFrameAfter(int frame);
    void deleteFrame(int frame);

    int getSustainStart() const;
    int getReleaseStart() const;

    /* Calc ROM usage without superfluous trailing 0 bytes */
    int calcEffectiveSize();

    QString name;
    TiaSound::Distortion baseDistortion{TiaSound::Distortion::PURE_COMBINED};
    QList<int> volumes{0, 0};
    QList<int> frequencies{0, 0};

private:
    /* Helper function for insert before/after */
    void correctSustainReleaseForInsert(int frame);

    int envelopeLength = 2;
    int sustainStart = 0;
    int releaseStart = 1;
};

}

#endif // INSTRUMENT_H
