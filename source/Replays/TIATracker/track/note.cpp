/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#include "note.h"
#include <stdexcept>

namespace Track {

const QList<Note::instrumentType> Note::insTypes{
    Note::instrumentType::Hold,
    Note::instrumentType::Instrument,
    Note::instrumentType::Pause,
    Note::instrumentType::Percussion,
    Note::instrumentType::Slide
};

Note::Note()
{

}

/*************************************************************************/

/*
void Note::toJson(QJsonObject &json) {
    json["type"] = insTypes.indexOf(type);
    json["number"] = instrumentNumber;
    json["value"] = value;
}
*/

/*************************************************************************/

bool Note::fromJson(const QJsonObject &json) {
    int typeInt = json["type"].get<int>();
    if (typeInt < 0 || typeInt >= int(insTypes.size())) {
        return false;
    }
    type = insTypes[typeInt];
    instrumentNumber = json["number"].get<int>();
    // FIXME: Magic number
    if (instrumentNumber < 0 || instrumentNumber > 22) {
        return false;
    }
    value = json["value"].get<int>();
    return true;
}

}

