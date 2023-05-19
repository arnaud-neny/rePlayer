/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#include "pitchguide.h"
#include <QJsonObject>


namespace TiaSound {

void PitchGuide::toJson(QJsonObject &json) {
    json["name"] = name;
    json["baseFrequency"] = baseFreq;
    json["tvStandard"] = (tvStandard == TiaSound::TvStandard::PAL ? "PAL" : "NTSC");
}

}
