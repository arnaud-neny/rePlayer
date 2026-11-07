// SPDX-License-Identifier: MIT
// The ".info" file next to a song. UADE's icon.library does not parse the icon: FindToolType looks the key up as a
// plain substring of the whole file (the first occurrence) and answers with the text that follows the key and one more
// character. The eagleplayer reads the type, the number of subsongs and the audio correction from it this way.

#include "pn_internal.h"

#include "nuade_utils.h"
#include <string.h>

// A number of a list ends at a comma, the end of the string, a line feed or a carriage return.
#define PN_LIST_SEPARATOR ','

PnToolType pn_info_find(const PnInfo* info, const char* key) {
    const size_t key_length = strlen(key);
    PnToolType tool = { 0 };
    for (size_t at = 0; at + key_length <= info->size; at++) {
        if (memcmp(info->data + at, key, key_length) == 0) {
            tool.found = true;
            tool.data = info->data;
            tool.size = info->size;
            tool.offset = at + key_length + 1;
            return tool;
        }
    }
    return tool;
}

// The text behind the key; past the end of the file it is zeros.

static uint8_t tool_byte(const PnToolType* tool, size_t index) {
    const size_t at = tool->offset + index;
    return at < tool->size ? tool->data[at] : 0;
}

// The eagleplayer compares a fixed number of bytes, the terminator included.

bool pn_tool_type_is(const PnToolType* tool, const char* text) {
    if (!tool->found) {
        return false;
    }
    const size_t length = strlen(text) + 1;
    for (size_t i = 0; i < length; i++) {
        if (tool_byte(tool, i) != (uint8_t)text[i]) {
            return false;
        }
    }
    return true;
}

// One hexadecimal number: the digit value of a character is its distance from '0', less 7 from 'A' on and less 0x27
// from 'a' on, whatever the character is, and the values are or'ed in a nibble at a time.

static uint32_t read_hex(const PnToolType* tool, size_t* index, uint8_t* terminator) {
    uint32_t value = 0;
    for (;;) {
        const uint8_t character = tool_byte(tool, (*index)++);
        if (character == PN_LIST_SEPARATOR || character == 0 || character == '\n' || character == '\r') {
            *terminator = character;
            return value;
        }
        uint8_t digit = (uint8_t)(character - '0');
        if (digit >= 0x31) {
            digit = (uint8_t)(digit - 0x27);
        } else if (digit >= 0x11) {
            digit = (uint8_t)(digit - 7);
        }
        value = (value << 4) | digit;
    }
}

// SONGS=<last>[,<default>[,<first>]]: the subsong numbers the eagleplayer reports (the numbers are one less than the
// values; the subtraction does not borrow out of the low byte; absent values are 0). Then the glue and the frontend
// make them consistent: the default 0 means the first subsong, a negative default is 0, and a range that does not
// contain the default grows to contain it.

PnSongRange pn_info_song_range(const PnInfo* info) {
    uint16_t last = 0;
    uint16_t default_song = 0;
    uint16_t first = 0;
    const PnToolType songs = pn_info_find(info, "SONGS");
    if (songs.found) {
        size_t index = 0;
        uint8_t terminator;
        uint16_t* const fields[] = { &last, &default_song, &first };
        for (size_t i = 0; i < nuade_sizeof_array(fields); i++) {
            const uint32_t value = read_hex(&songs, &index, &terminator);
            *fields[i] = (uint16_t)((value & ~0xFFu) | ((value - 1) & 0xFFu));
            if (terminator == 0) {
                break;
            }
        }
    }
    int32_t current = (int16_t)default_song;
    if (current == 0) {
        current = (int16_t)first;
    }
    if (current < 0) {
        current = 0;
    }
    int32_t min = (int16_t)first;
    int32_t max = (int16_t)last;
    if (min < 0 || min > current) {
        min = current;
    }
    if (max < current) {
        max = current;
    }
    return (PnSongRange) { .first = (uint16_t)min, .last = (uint16_t)max, .default_song = (uint16_t)current };
}
