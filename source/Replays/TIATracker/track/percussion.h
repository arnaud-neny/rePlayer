/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef PERCUSSION_H
#define PERCUSSION_H

#include <QString>
#include <QList>
#include <QMutex>
#include "tiasound/tiasound.h"
#include <QJsonObject>


namespace Track {

class Percussion
{
public:
    static const int maxEnvelopeLength = 99;

    Percussion(QString name) : name(name) {}

    int getEnvelopeLength();
    void setEnvelopeLength(int newSize);

    // Checks if instrument has its empty starting values
    bool isEmpty();

//     void toJson(QJsonObject &json);
    bool import(const QJsonObject &json);

    void deletePercussion();

    /* Get minimum volume over the whole envelope */
    int getMinVolume();

    /* Get maximum volume over the whole envelope */
    int getMaxVolume();

    /* Insert a new frame before the specified */
    void insertFrameBefore(int frame);
    void insertFrameAfter(int frame);
    void deleteFrame(int frame);

    /* Calc ROM usage without superfluous trailing 0 bytes */
    int calcEffectiveSize();

    QString name;
    QList<int> volumes{0};
    QList<int> frequencies{0};
    QList<TiaSound::Distortion> waveforms{TiaSound::Distortion::WHITE_NOISE};
    bool overlay = false;

private:
    int envelopeLength = 1;


};

}

#endif // PERCUSSION_H
