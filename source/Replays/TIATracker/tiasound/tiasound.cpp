/* TIATracker, (c) 2016 Andre "Kylearan" Wichmann.
 * Website: https://bitbucket.org/kylearan/tiatracker
 * Email: andre.wichmann@gmx.de
 * See the file "license.txt" for information on usage and redistribution
 * of this file.
 */

#include "tiasound.h"

namespace TiaSound {

QString getNoteName(Note note) {
    static const char* const noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    int noteIndexInOctave = (static_cast<int>(note))%12;
    return noteNames[noteIndexInOctave];
}

/*************************************************************************/

QString getNoteNameWithOctave(Note note)
{
    static const QList<QString> noteNames{"C%1", "C#%1", "D%1", "D#%1", "E%1", "F%1", "F#%1", "G%1", "G#%1", "A%1", "A#%1", "B%1"};

    int noteIndexInOctave = (static_cast<int>(note))%12;
    // +1 because first note is C_1, not C_0
    int octave = int((static_cast<int>(note))/12) + 1;

    return noteNames[noteIndexInOctave].arg(octave);
}

/*************************************************************************/

QString getNoteNameWithOctaveFixedWidth(Note note)
{
    static const QList<QString> noteNames{"C-%1", "C#%1", "D-%1", "D#%1", "E-%1", "F-%1", "F#%1", "G-%1", "G#%1", "A-%1", "A#%1", "B-%1"};

    int noteIndexInOctave = (static_cast<int>(note))%12;
    // +1 because first note is C_1, not C_0
    int octave = int((static_cast<int>(note))/12) + 1;

    return noteNames[noteIndexInOctave].arg(octave);
}

/*************************************************************************/

QString getDistortionName(Distortion dist)
{
    static const char* const distNames[] = {
            "Silent (0)",            // 0
            "Buzzy (1)",             // 1
            "Buzzy/Rumble (2)",      // 2
            "Flangy/Wavering (3)",   // 3
            "Pure High (4)",         // 4
            "Pure High (5)",         // 5
            "Pure Buzzy (6)",        // 6
            "Reedy/Rumble (7)",      // 7
            "White Noise (8)",       // 8
            "Reedy/Rumble (9)",      // 9
            "Pure Buzzy (10)",       // 10
            "Silent (11)",           // 11
            "Pure Low (12)",         // 12
            "Pure Low (13)",         // 13
            "Electronic Rumble (14)",// 14
            "Electronic Squeal (15)",// 15
            "Pure Combined (4+12)"   // 16
                                          };

    return distNames[static_cast<int>(dist)];
}

/*************************************************************************/

int getDistortionInt(Distortion dist) {
    return static_cast<int>(dist);
}

/*************************************************************************/

Note getNoteFromInt(int n) {
    return static_cast<Note>(n);
}

/*************************************************************************/

int getIntFromNote(Note n) {
    return static_cast<int>(n);
}

}
