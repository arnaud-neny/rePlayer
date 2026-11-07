// SPDX-License-Identifier: MIT
// Sound Images: the proof, made when the module is opened, that every byte of song data the driver can read lies in the
// module and clear of the places the port does not read from the file (the driver's variables, which the port keeps in
// structures, and the words the eagleplayer rewrites).
//
// The driver reads the song data in one way only: the song table gives each voice a list of sequence offsets (the first
// is the voice's first sequence, the rest are read one by one when a sequence ends with the next-pattern command; a 0
// ends the list and runs the module's end command, whose arguments follow the zero); a sequence is a stream of notes
// and commands that is read from its first byte on, command by command, until a command leaves it (the next pattern, a
// jump of the list, a stop). So the bytes that can be read are the bytes of those streams, and a walk that parses the
// same way as the driver finds them all. A list entry is read only when the stream before it ends with the next-pattern
// command, so the walk goes on in a list only then (a list need not have a terminator: sequence data can follow it).
// The result of every command boundary is remembered, so the walk takes time in the size of the module.
//
// What happens after a list ended or a list jumped is reached only if the song gets there, and its data can be anything
// a game never used. A bad byte there does not make the module unfit; the port then ends the song at the read (every
// read of the song data is bounded at play time).

#include "nuade_utils.h"
#include "si_internal.h"
#include <stdlib.h>

// An arpeggio pattern is read with a byte index: at most 256 bytes.
#define ARPEGGIO_PATTERN_MAX 256

typedef enum StreamEnd {
    StreamEnd_Unknown = 0, // not parsed yet (in the memo)
    StreamEnd_Failed,      // a byte the port cannot read
    StreamEnd_NextPattern, // the stream goes on with the voice's list
    StreamEnd_Other,       // a jump of the list, a stop, a voice off
} StreamEnd;

typedef struct Job {
    bool is_list;
    bool end_command; // a stream that starts with the end command in place of its first byte
    int64_t position;
} Job;

typedef struct Walk {
    const SiModule* module;
    uint8_t* memo;  // StreamEnd of the stream from each command boundary
    bool* listed;   // the list entry here was walked
    int64_t* trail; // the boundaries of the stream being parsed
    size_t trail_count;
    size_t trail_capacity;
    Job* jobs; // only soft work: reached after a list ended or jumped
    size_t job_count;
    size_t job_capacity;
} Walk;

static bool push_job(Walk* walk, bool is_list, bool end_command, int64_t position) {
    if (walk->job_count == walk->job_capacity) {
        const size_t capacity = walk->job_capacity == 0 ? 256 : walk->job_capacity * 2;
        Job* jobs = realloc(walk->jobs, capacity * sizeof(*jobs));
        if (jobs == nullptr) {
            return false;
        }
        walk->jobs = jobs;
        walk->job_capacity = capacity;
    }
    walk->jobs[walk->job_count++] = (Job) { is_list, end_command, position };
    return true;
}

static bool push_trail(Walk* walk, int64_t position) {
    if (walk->trail_count == walk->trail_capacity) {
        const size_t capacity = walk->trail_capacity == 0 ? 256 : walk->trail_capacity * 2;
        int64_t* trail = realloc(walk->trail, capacity * sizeof(*trail));
        if (trail == nullptr) {
            return false;
        }
        walk->trail = trail;
        walk->trail_capacity = capacity;
    }
    walk->trail[walk->trail_count++] = position;
    return true;
}

static int64_t song_offset(const SiModule* module, uint16_t word) {
    return (int64_t)module->layout.song_table + (int16_t)word;
}

// The bytes a command takes behind its own byte.

static uint32_t command_arguments(SiCommand command) {
    switch (command) {
        case SiCommand_Release:
            return 2;
        case SiCommand_Transpose:
            return 1;
        case SiCommand_Vibrato:
        case SiCommand_SlideOn:
        case SiCommand_SlideOnRelease:
            return 3;
        case SiCommand_Bend:
            return 5;
        case SiCommand_NextPattern:
        case SiCommand_NoteStepDown:
        case SiCommand_NoteStepUp:
        case SiCommand_JumpList:
        case SiCommand_Stop:
        case SiCommand_EndNote:
        case SiCommand_Silence:
        case SiCommand_BendOff:
        case SiCommand_SlideOnReleaseOff:
        case SiCommand_HoldOn:
        case SiCommand_HoldOff:
        case SiCommand_ArpeggioOff:
        case SiCommand_VolumeDown:
        case SiCommand_VolumeUp:
        case SiCommand_Fade:
        case SiCommand_VoiceOff:
        case SiCommand_VoiceOffOwn:
        case SiCommand_VoiceOffOwnSilent:
            break;
    }
    return 0;
}

// An arpeggio pattern is read byte by byte up to the one with the top bit set.

static bool walk_arpeggio(const SiModule* module, uint32_t index) {
    uint16_t word = 0;
    const int64_t table = (int64_t)module->layout.arpeggio_table;
    if (!si_module_u16(module, table + 2 * (int64_t)index, &word)) {
        return false;
    }
    const int64_t pattern = table + (int16_t)word;
    for (int64_t at = pattern; at < pattern + ARPEGGIO_PATTERN_MAX; at++) {
        uint8_t entry = 0;
        if (!si_module_u8(module, at, &entry)) {
            return false;
        }
        if ((entry & 0x80) != 0) {
            break;
        }
    }
    return true;
}

typedef enum Step {
    Step_Failed,
    Step_Continue,
    Step_Left,
} Step;

// One command (or note) behind the byte just read; the cursor is behind the byte.

static Step walk_command(Walk* walk, uint8_t byte, bool force_command, int64_t* cursor, StreamEnd* end) {
    const SiModule* module = walk->module;
    const SiImage* image = module->image;
    uint8_t ignored = 0;
    if ((!force_command && (byte & 0x80) == 0) || byte >= image->duration_class) {
        return Step_Continue;
    }
    if (byte >= image->sample_class) {
        return (uint32_t)(byte - image->sample_class) < module->layout.sample_count ? Step_Continue : Step_Failed;
    }
    uint32_t arguments = 0;
    if (byte >= image->envelope_class) {
        arguments = 3;
    } else if (byte >= image->volume_class) {
        arguments = 0;
    } else if (byte >= image->arpeggio_class) {
        if (!walk_arpeggio(module, (uint32_t)(byte - image->arpeggio_class))) {
            return Step_Failed;
        }
    } else {
        const uint32_t index = byte & 0x7F;
        if (index >= image->command_count) {
            return Step_Failed;
        }
        const SiCommand command = image->commands[index].command;
        switch (command) {
            case SiCommand_NextPattern:
                *end = StreamEnd_NextPattern;
                return Step_Left;
            case SiCommand_Stop:
            case SiCommand_VoiceOff:
            case SiCommand_VoiceOffOwn:
            case SiCommand_VoiceOffOwnSilent:
                *end = StreamEnd_Other;
                return Step_Left;
            case SiCommand_JumpList: {
                uint16_t word = 0;
                if (!si_module_u16(module, *cursor, &word) || !push_job(walk, true, false, song_offset(module, word))) {
                    return Step_Failed;
                }
                *end = StreamEnd_Other;
                return Step_Left;
            }
            default:
                break;
        }
        arguments = command_arguments(command);
    }
    for (uint32_t i = 0; i < arguments; i++) {
        if (!si_module_u8(module, *cursor, &ignored)) {
            return Step_Failed;
        }
        (*cursor)++;
    }
    return Step_Continue;
}

// One stream, command by command, from its first byte (or from the end command, whose arguments follow the list). The
// result of every boundary it passes is the result of the stream.

static StreamEnd walk_stream(Walk* walk, int64_t position, bool end_command) {
    const SiModule* module = walk->module;
    int64_t cursor = position;
    bool force_command = end_command;
    StreamEnd result = StreamEnd_Failed;
    walk->trail_count = 0;
    for (;;) {
        uint8_t byte = 0;
        if (force_command) {
            byte = module->layout.end_command;
        } else {
            if (cursor < 0 || cursor >= (int64_t)module->size) {
                break;
            }
            if (walk->memo[cursor] != StreamEnd_Unknown) {
                result = (StreamEnd)walk->memo[cursor];
                break;
            }
            if (!si_module_u8(module, cursor, &byte) || !push_trail(walk, cursor)) {
                break;
            }
            cursor++;
        }
        StreamEnd end = StreamEnd_Failed;
        const Step step = walk_command(walk, byte, force_command, &cursor, &end);
        force_command = false;
        if (step == Step_Failed) {
            break;
        }
        if (step == Step_Left) {
            result = end;
            break;
        }
    }
    for (size_t i = 0; i < walk->trail_count; i++) {
        walk->memo[walk->trail[i]] = (uint8_t)result;
    }
    return result;
}

// A list: the entries from here, each only if the stream before it goes on with the list. The zero runs the end command
// with the cursor behind it. Returns false for a stream that failed.

static bool walk_list(Walk* walk, int64_t position) {
    const SiModule* module = walk->module;
    for (;;) {
        uint16_t word = 0;
        if (position < 0 || position >= (int64_t)module->size) {
            return false;
        }
        if (walk->listed[position]) {
            return true;
        }
        if (!si_module_u16(module, position, &word)) {
            return false;
        }
        walk->listed[position] = true;
        if (word == 0) {
            return push_job(walk, false, true, position + 2);
        }
        // A list that goes on into bytes that are no list (the last sequence of a voice need not stop) sends the voice
        // to an offset outside the module: the port ends the song at that read (UADE would read chip memory there; the
        // corpus songs that have such an entry stop by command first), so such an entry is not a reason to refuse.
        const int64_t start = song_offset(module, word);
        if (start < 0 || start >= (int64_t)module->size) {
            return true;
        }
        const StreamEnd end = walk_stream(walk, start, false);
        if (end == StreamEnd_Failed) {
            return false;
        }
        if (end != StreamEnd_NextPattern) {
            return true;
        }
        position += 2;
    }
}

bool si_module_walk(const SiModule* module) {
    Walk walk = { .module = module };
    walk.memo = calloc(module->size, 1);
    walk.listed = calloc(module->size, sizeof(bool));
    bool ok = walk.memo != nullptr && walk.listed != nullptr;
    for (uint32_t song = 0; ok && song < module->layout.song_count; song++) {
        SiSong entry;
        ok = si_module_song(module, song, &entry);
        for (uint32_t voice = 0; ok && voice < SI_VOICES; voice++) {
            if (module->image->voice_flags && entry.list[voice] == 0) {
                // The song start leaves the flag of such a voice alone in the versions that do not clear it: a saved
                // game's record would run the voice from its own pointers, which no list vouches for.
                ok = module->image->song_start_clears_voices || si_module_record(module, voice).active == 0;
                continue;
            }
            // The voice's list starts with the offset of its first sequence.
            const int64_t list = song_offset(module, entry.list[voice]);
            uint16_t first = 0;
            ok = si_module_u16(module, list, &first);
            if (ok) {
                const StreamEnd end = walk_stream(&walk, song_offset(module, first), false);
                ok = end != StreamEnd_Failed && (end != StreamEnd_NextPattern || walk_list(&walk, list + 2));
            }
        }
    }
    // Reached only after a list ended or jumped: a failure here is not a reason to refuse.
    while (ok && walk.job_count > 0) {
        const Job job = walk.jobs[--walk.job_count];
        if (job.is_list) {
            (void)walk_list(&walk, job.position);
        } else {
            (void)walk_stream(&walk, job.position, job.end_command);
        }
    }
    free(walk.jobs);
    free(walk.trail);
    free(walk.memo);
    free(walk.listed);
    return ok;
}
