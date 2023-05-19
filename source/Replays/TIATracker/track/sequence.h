/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <QList>
#include "pattern.h"
#include "sequenceentry.h"
#include <QJsonObject>


namespace Track {

class Sequence
{
public:
    Sequence();

//     void toJson(QJsonObject &json);
    bool fromJson(const QJsonObject &json);

    QList<SequenceEntry> sequence{};
};

}

#endif // SEQUENCE_H
