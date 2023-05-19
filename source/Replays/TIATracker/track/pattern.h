/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef PATTERN_H
#define PATTERN_H

#include <QString>
#include <QList>
#include "note.h"
#include <QJsonObject>

namespace Track {

class Pattern
{
public:
    const static int minSize = 1;
    const static int maxSize = 127;

    Pattern();

    Pattern(QString name) : name(name) {}

//     void toJson(QJsonObject &json);
    bool fromJson(const QJsonObject &json);

    QString name;
    QList<Note> notes;
    int evenSpeed = 3;
    int oddSpeed = 3;
};

}

#endif // PATTERN_H
