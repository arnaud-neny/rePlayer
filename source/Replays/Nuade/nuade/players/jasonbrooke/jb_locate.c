// SPDX-License-Identifier: MIT
// Jason C. Brooke player: finds the replayer's tables in a module's code, the way the eagleplayer does (it scans for
// instruction patterns and reads the pc-relative offsets out of them). The scans below name the instruction they look
// for; "xxxx" in a pattern is any word.

#include "jb_internal.h"
#include <stdlib.h>

// Pattern scan

#define MAX_PATTERN_WORDS 16

// "43fa xxxx": words of four hex digits; an "x" digit matches any value
static int parse_pattern(const char* pattern, uint16_t* words, uint16_t* masks) {
    int count = 0;
    while (*pattern != '\0' && count < MAX_PATTERN_WORDS) {
        if (*pattern == ' ') {
            pattern++;
            continue;
        }
        words[count] = 0;
        masks[count] = 0;
        for (int digit = 0; digit < 4 && *pattern != '\0'; digit++, pattern++) {
            words[count] = (uint16_t)(words[count] << 4);
            masks[count] = (uint16_t)(masks[count] << 4);
            if (*pattern != 'x') {
                const char text[2] = { *pattern, '\0' };
                words[count] |= (uint16_t)strtoul(text, nullptr, 16);
                masks[count] |= 0xf;
            }
        }
        count++;
    }
    return count;
}

uint32_t jbm_find(const JbModule* m, uint32_t from, uint32_t to, const char* pattern) {
    uint16_t words[MAX_PATTERN_WORDS];
    uint16_t masks[MAX_PATTERN_WORDS];
    const int count = parse_pattern(pattern, words, masks);
    for (uint32_t at = (from + 1) & ~1u; at + 2u * (uint32_t)count <= m->size && at < to; at += 2) {
        int i = 0;
        while (i < count && (jbm_u16(m, at + 2u * (uint32_t)i) & masks[i]) == words[i]) {
            i++;
        }
        if (i == count) {
            return at;
        }
    }
    return JB_NOT_FOUND;
}

// Effective addresses of the operands of an instruction, from the offset of its extension word

// d16(pc): the effective address of a 16 bit displacement whose extension word is at `extension`
static uint32_t pc16(const JbModule* m, uint32_t extension) {
    return extension + (uint32_t)(int32_t)jbm_s16(m, extension);
}

// d8(pc,Xn): the brief extension word at `extension` carries the displacement in its low byte
static uint32_t pc8(const JbModule* m, uint32_t extension) {
    return extension + (uint32_t)(int32_t)(int8_t)jbm_u8(m, extension + 1);
}

// d8(An,Xn): the table at `base` plus the displacement of the brief extension word at `extension`
static uint32_t an8(const JbModule* m, uint32_t base, uint32_t extension) {
    return base + (uint32_t)(int32_t)(int8_t)jbm_u8(m, extension + 1);
}

// JbVersion A

#define A_SEARCH 0x600

// The command a handler at `handler` is. The handlers are told apart by their first instructions: what they store to
// and which flag bits they set.
static JbACommand classify_a_handler(const JbModule* m, const JbATables* t, bool new_syntax, uint32_t handler) {
    if (handler + 8 > m->size) {
        return JbACommand_Invalid;
    }
    const uint16_t w0 = jbm_u16(m, handler);
    const uint16_t w1 = jbm_u16(m, handler + 2);
    const uint16_t w2 = jbm_u16(m, handler + 4);
    const uint16_t w3 = jbm_u16(m, handler + 6);
    switch (w0) {
        case 0x4a28: // tst.b d(a0)
            return JbACommand_Rest;
        case 0x4210: // clr.b (a0)
            return JbACommand_ClearFlags;
        case 0x1159: { // move.b (a1)+,d(a0)
            const uint16_t transpose = new_syntax ? 0x1a : 0x18;
            const uint16_t volume = new_syntax ? 0x12 : 0x10;
            if (w1 == 0x08) {
                return JbACommand_Slide;
            } else if (w2 == 0x1019) {
                return JbACommand_Vibrato;
            } else if (w2 == 0x93cb) {
                return JbACommand_LoopStart;
            } else if (w1 == transpose) {
                return JbACommand_Transpose;
            } else if (w1 == volume) {
                return JbACommand_Volume;
            } else if (new_syntax && w1 == 0x1c) {
                return JbACommand_ArpProgram;
            }
            return JbACommand_Invalid;
        }
        case 0x08d0: // bset.b #bit,(a0)
            if (w1 == 7 && w2 == 0x08d0 && w3 == 3) {
                return JbACommand_NoteDown;
            } else if (w1 == 3) {
                return JbACommand_NoteUp;
            } else if (w1 == 5) {
                return w2 == 0x08d0 && w3 == 1 ? JbACommand_TieNoEnvelope : JbACommand_Tie;
            } else if (w1 == 1) {
                return JbACommand_NoEnvelope;
            }
            return JbACommand_Invalid;
        case 0x1028: // move.b 1(a0),d0
            return w1 == 1 ? JbACommand_NextTrack : JbACommand_Invalid;
        case 0x7600: // moveq #0,d3
            return JbACommand_Stop;
        case 0x1219: // move.b (a1)+,d1
            return JbACommand_AdkCon;
        case 0x117c: // move.b #2,d(a0)
            return w1 == 2 ? JbACommand_LoopTwice : JbACommand_Invalid;
        case 0x5328: // subq.b #1,d(a0)
            return JbACommand_LoopEnd;
        case 0x1759: // move.b (a1)+,d(a3)
            if (w2 == 0x1759) {
                return JbACommand_PwmSet;
            }
            switch (w1 - t->globals) {
                case JbAGlobalByte_Tempo:
                    return JbACommand_Tempo;
                case JbAGlobalByte_Transpose:
                    return JbACommand_GlobalTranspose;
                default:
                    return JbACommand_SkipByte;
            }
        default:
            return JbACommand_Invalid;
    }
}

// Subsongs: the eagleplayer counts the entries of the song table that look like entries, as far as the next table.

// An entry after the first counts when the byte before it (the last byte of the previous record) is 0 and that byte
// with the entry's tempo does not form the word 1. The table ends where the envelope start table begins; returns 0 if
// it does not have room for a record.
static int count_a_songs(const JbModule* m, const JbATables* t) {
    if (t->envelope_start <= t->songs) {
        return 0;
    }
    const uint32_t room = (t->envelope_start - t->songs + 1) / JB_A_SONG_SIZE;
    uint32_t count = room > 0 ? 1 : 0;
    for (uint32_t k = 1; k < room; k++) {
        const uint32_t before = t->songs + JB_A_SONG_SIZE * k - 1;
        if (jbm_u8(m, before) != 0 || jbm_u16(m, before) == 1) {
            break;
        }
        count++;
    }
    return (int)count;
}

bool jb_a_locate(const JbModule* m, uint32_t entry, JbAConfig* config) {
    JbATables* t = &config->tables;
    const uint32_t end = entry + A_SEARCH;
    uint32_t at;

    // move.w #stride,d4 : the voice records fill the module start, then the globals
    at = jbm_find(m, entry, end, "383c 00xx");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->globals = 4u * jbm_u8(m, at + 3);

    // move.b #limit,d2 : the sequencer's note limit
    at = jbm_find(m, entry, end, "143c 00xx");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    config->note_limit = jbm_u8(m, at + 3);

    // move.b (a1)+,d0 ; cmp.b d2,d0 ; bcs (note bytes below the limit) or blt (the new syntax)
    at = jbm_find(m, entry, end, "1019 b002");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    config->new_syntax = (jbm_u8(m, at + 4) == 0x6d);

    // lea waveform(pc),a0 ; move.b gap(pc),d1 [; add.b d1,d0 : the noise pattern]
    at = jbm_find(m, entry, end, "41fa xxxx");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->pwm = pc16(m, at + 2);
    config->pwm_noise = jbm_find(m, at, at + 0x10, "123a xxxx d001") != JB_NOT_FOUND;

    // lea envelope(pc),a1 ; tst.b timer(a0)
    const uint32_t envelope_site = jbm_find(m, entry, end, "43fa xxxx 4a28");
    if (envelope_site == JB_NOT_FOUND) {
        return false;
    }
    t->envelope = pc16(m, envelope_site + 2);

    // the pitch sequence: move.b d8(a1,d1.w),d0 ; the new driver first looks up the sequence start
    // (move.b d8(a1,d1.w),d1)
    uint32_t sequence_site = jbm_find(m, envelope_site, end, "1031 10xx");
    if (sequence_site == JB_NOT_FOUND) {
        return false;
    }
    const uint32_t start_site = jbm_find(m, envelope_site, sequence_site, "1231 10xx");
    if (start_site != JB_NOT_FOUND) {
        t->sequence_starts = an8(m, t->envelope, start_site + 2);
    }
    t->pitch_sequences = an8(m, t->envelope, sequence_site + 2);
    config->sequence_mask_form = !config->new_syntax && jbm_u16(m, sequence_site + 6) != 0x5200;

    // move.w d8(pc,d0.w),d1 : the period table, after the pitch sum (add.b d(a0),d0 ... [add.b global(pc),d0] ;
    // add.b d0,d0)
    const uint32_t period_site = jbm_find(m, sequence_site, end, "323b 00xx");
    if (period_site == JB_NOT_FOUND) {
        return false;
    }
    t->periods = pc8(m, period_site + 2);
    t->global_transpose = jbm_u16(m, period_site - 6) == 0xd03a ? pc16(m, period_site - 4) : 0;
    config->pitch_uses_sequence = jbm_find(m, sequence_site, period_site, "d028 xxxx d028 xxxx") != JB_NOT_FOUND;

    // mulu.w #10,d1 ; lea samples(pc),a2
    const uint32_t sample_site = jbm_find(m, entry, end, "c2fc 000a 45fa xxxx");
    if (sample_site == JB_NOT_FOUND) {
        return false;
    }
    t->samples = pc16(m, sample_site + 6);

    // lea silence(pc),a5|a1 ; lea silence_length(pc),a2
    uint32_t silence_site = jbm_find(m, sample_site, end, "4bfa xxxx 45fa xxxx");
    if (silence_site == JB_NOT_FOUND) {
        silence_site = jbm_find(m, sample_site, end, "43fa xxxx 45fa xxxx");
    }
    if (silence_site == JB_NOT_FOUND) {
        return false;
    }
    t->silence_address = pc16(m, silence_site + 2);
    t->silence_length = pc16(m, silence_site + 6);

    // lea codes(pc),a2 ; move.b (a2,d0.w),sequence_loop(a0) (the old syntax's 0x90..0x9f bytes)
    at = jbm_find(m, entry, end, "45fa xxxx 1172 0000");
    if (at != JB_NOT_FOUND) {
        t->sequence_codes = pc16(m, at + 2);
    }

    // lea starts(pc),a2 ; move.b (a2,d0.w),d0 ; move.b d8(a2,d0.w),d1 : the envelope start table and the volume
    at = jbm_find(m, entry, end, "45fa xxxx 1032 0000 1232 00xx");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->envelope_start = pc16(m, at + 2);
    t->envelope_volume = an8(m, t->envelope_start, at + 10);

    // lea songs(pc),a1 ; ext.w d0 ; mulu.w #$12,d0
    at = jbm_find(m, entry, end, "43fa xxxx 4880 c0fc 0012");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->songs = pc16(m, at + 2);

    // adda.w d8(pc,d0.w),a2 ; jmp (a2)
    at = jbm_find(m, entry, end, "d4fb 00xx 4ed2");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->commands = pc8(m, at + 2);
    config->command_count = (uint8_t)(config->new_syntax ? 0x1f : config->note_limit - 0x80);
    if (config->command_count > JB_A_MAX_COMMANDS) {
        return false;
    }
    for (uint32_t i = 0; i < config->command_count; i++) {
        config->commands[i] = classify_a_handler(m, t, config->new_syntax, jbm_u16(m, t->commands + 2 * i));
        if (config->commands[i] == JbACommand_PwmSet) {
            // move.w d0,length(a3) : the PWM sample's length word
            const uint32_t handler = jbm_u16(m, t->commands + 2 * i);
            const uint32_t store = jbm_find(m, handler, handler + 0x20, "3740 xxxx");
            if (store == JB_NOT_FOUND) {
                return false;
            }
            t->pwm_length = jbm_u16(m, store + 2);
        }
    }

    config->subsong_count = count_a_songs(m, t);
    return config->subsong_count > 0;
}

// JbVersion B

#define B_SEARCH 0x600

bool jb_b_locate(const JbModule* m, JbBConfig* config) {
    JbBTables* t = &config->tables;

    // lea pitch_sequences(pc),a1 (before the voice loop)
    uint32_t at = jbm_find(m, 0, B_SEARCH, "43fa xxxx");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->pitch_sequences = pc16(m, at + 2);

    // move.b d8(pc,d0.w),d0 : the envelope start table
    const uint32_t start_site = jbm_find(m, 0, B_SEARCH, "103b 00xx");
    if (start_site == JB_NOT_FOUND) {
        return false;
    }
    t->envelope_start = pc8(m, start_site + 2);
    config->vibrato_hold = jbm_find(m, start_site, B_SEARCH, "0810 0004") != JB_NOT_FOUND;
    // The envelope bytes are at an offset from the pitch sequences (move.b d8(a1,d0.w),d(a0)), or at their own address
    // (lea envelope(pc),a2 before the table lookup)
    config->envelope_wraps = jbm_u16(m, start_site - 4) != 0x45fa;
    if (!config->envelope_wraps) {
        t->envelope = pc16(m, start_site - 2);
    } else {
        at = jbm_find(m, start_site, B_SEARCH, "1171 00xx 000f");
        if (at == JB_NOT_FOUND) {
            return false;
        }
        t->envelope = an8(m, t->pitch_sequences, at + 2);
    }

    // volume scale: move.b scale(pc),d1 ; cmp.b #$40,d1
    at = jbm_find(m, start_site, B_SEARCH, "123a xxxx b23c 0040");
    t->master = at != JB_NOT_FOUND ? pc16(m, at + 2) : 0;

    // move.w d8(pc,d0.w),d1 : the period table
    at = jbm_find(m, start_site, B_SEARCH, "323b 00xx");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->periods = pc8(m, at + 2);

    // move.b codes(pc,d0.w),sequence_loop(a0)
    at = jbm_find(m, 0, B_SEARCH, "117b 00xx 0016");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->sequence_codes = pc8(m, at + 2);

    // asl.w #7,d0 ; lea blocks(pc),a2 ; adda.w d0,a2
    at = jbm_find(m, 0, B_SEARCH, "ef40 45fa xxxx d4c0");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->blocks = pc16(m, at + 4);

    // move.b d8(pc,d0.w),tempo(a1) : the song table, whose first tempo byte is the target
    at = jbm_find(m, 0, B_SEARCH, "137b 00xx");
    if (at == JB_NOT_FOUND) {
        return false;
    }
    t->songs = pc8(m, at + 2) - 1;

    // adda.w d8(pc,d0.w),a2 ; jmp (a2) : the command table. The vibrato command of the variants with the vibrato hold
    // doubles the depth byte and takes the carry (add.b d0,d0) as the hold bit; the other one stores the depth as it
    // is.
    const uint32_t commands = jbm_find(m, 0, B_SEARCH, "d4fb 00xx 4ed2");
    if (commands == JB_NOT_FOUND) {
        return false;
    }
    const uint32_t vibrato = jbm_u16(m, pc8(m, commands + 2) + 2 * JbBCommand_Vibrato);
    const bool depth_carry = jbm_find(m, vibrato, vibrato + 0x10, "d000") != JB_NOT_FOUND
                             && jbm_find(m, vibrato, vibrato + 0x10, "1140 0013") == JB_NOT_FOUND;
    if (depth_carry != config->vibrato_hold) {
        return false;
    }

    // The eagleplayer counts the entries that look like song entries
    config->subsong_count = 1;
    for (uint32_t p = t->songs + JB_B_SONG_SIZE; jbm_u16(m, p) != 0 && jbm_u8(m, p) == 0; p += JB_B_SONG_SIZE) {
        config->subsong_count++;
    }
    return true;
}
