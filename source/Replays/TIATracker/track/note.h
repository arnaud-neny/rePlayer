/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef NOTE_H
#define NOTE_H

#include <QJsonObject>
#include <QList>


namespace Track {

class Note
{
public:
    enum class instrumentType {Instrument, Percussion, Hold, Pause, Slide};

    Note();
    Note(instrumentType type, int instrumentNumber, int value)
        : type(type), instrumentNumber(instrumentNumber), value(value) {}

//     void toJson(QJsonObject &json);
    bool fromJson(const QJsonObject &json);

    instrumentType type;
    // 0-6 for instrument, 0-14 for percussion
    int instrumentNumber;
    // Depending on type: frequency, or slide value
    int value;

private:
    static const QList<instrumentType> insTypes;
};

}

#endif // NOTE_H
