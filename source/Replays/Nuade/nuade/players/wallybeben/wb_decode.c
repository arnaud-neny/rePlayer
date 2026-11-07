// SPDX-License-Identifier: MIT
// Wally Beben: decoder for the 68000 subset of the replayers (see wb_decode.h).

#include "wb_decode.h"

#include "nuade_base.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define WB_HARDWARE_HIGH 0x00DFu // absolute addresses 0xDFxxxx are custom chip registers

typedef struct Decoder {
    const uint8_t* mod;
    uint32_t size;
    WbInsn* out;
    uint32_t len; // bytes of the instruction so far
    bool flow_ends;
} Decoder;

// An operand's extension words. `imm_bytes` is the size of an immediate operand (2 for byte and word, 4 for long).
static bool decode_ea(Decoder* d, uint32_t ea, uint32_t imm_bytes) {
    const uint32_t mode = (ea >> 3) & 7;
    const uint32_t reg = ea & 7;
    if (mode <= 4) {
        return true;
    }
    if (mode <= 6) {
        d->len += 2;
        return true;
    }
    switch (reg) {
        case 1:
            if (d->out->slot_count >= 2) {
                return false;
            }
            d->out->slot_at[d->out->slot_count++] = (uint8_t)d->len;
            d->len += 4;
            return true;
        case 0:
        case 2:
        case 3:
            d->len += 2;
            return true;
        case 4:
            d->len += imm_bytes;
            return true;
        default:
            return false;
    }
}

static uint32_t size_to_imm(uint32_t size_field) {
    return size_field == 2 ? 4u : 2u;
}

// The length of the instruction at `at`; sets the branch target and whether the code after it is reachable.
static bool decode_one(Decoder* d, uint32_t at, WbInsn* out) {
    memset(out, 0, sizeof(*out));
    out->at = at;
    d->out = out;
    d->len = 2;
    d->flow_ends = false;
    const uint16_t w = nuade_read_be16(d->mod + at);
    const uint32_t ea = w & 0x3Fu;
    bool ok = false;
    switch (w >> 12) {
        case 0:
            if (w & 0x0100u) {
                ok = (w & 0x0038u) != 0x0008u && decode_ea(d, ea, 2);
            } else {
                const uint32_t kind = (w >> 9) & 7;
                const uint32_t size = (w >> 6) & 3;
                if (kind == 4) {
                    d->len += 2;
                    ok = decode_ea(d, ea, 2);
                } else if ((kind <= 3 || kind == 5 || kind == 6) && size != 3 && (ea & 0x3Fu) != 0x3Cu) {
                    d->len += size_to_imm(size);
                    ok = decode_ea(d, ea, 2);
                }
            }
            break;
        case 1:
        case 2:
        case 3: {
            const uint32_t size = (w >> 12) == 1 ? 0u : ((w >> 12) == 3 ? 1u : 2u);
            const uint32_t dst = ((w >> 6) & 7) << 3 | ((w >> 9) & 7);
            ok = decode_ea(d, ea, size_to_imm(size)) && (dst & 0x3Fu) != 0x3Cu && decode_ea(d, dst, 2);
            break;
        }
        case 4:
            if (w == 0x4E75u || w == 0x4E73u) {
                d->flow_ends = true;
                ok = true;
            } else if (w == 0x4E71u) {
                ok = true;
            } else if ((w & 0xFF80u) == 0x4E80u) {
                ok = ((w & 0x3Fu) == 0x39u) && decode_ea(d, ea, 2); // jsr / jmp abs.l
                if (ok) {
                    out->target_kind = WbTarget_Absolute;
                    out->target_at = 2;
                    out->target = nuade_read_be32(d->mod + at + 2);
                    out->slot_count = 0;
                    d->flow_ends = (w & 0x0040u) != 0;
                }
            } else if ((w & 0xF1C0u) == 0x41C0u) {
                ok = decode_ea(d, ea, 2);
            } else if ((w & 0xFB80u) == 0x4880u && (w & 0x38u) != 0) {
                d->len += 2;
                ok = decode_ea(d, ea, 2);
            } else if (((w & 0xFF00u) == 0x4200u || (w & 0xFF00u) == 0x4A00u || (w & 0xFF00u) == 0x4600u)
                       && ((w >> 6) & 3) != 3) {
                ok = decode_ea(d, ea, 2);
            }
            break;
        case 5:
            if (((w >> 6) & 3) == 3) {
                if ((w & 0x38u) == 0x08u) {
                    d->len += 2;
                    out->target_kind = WbTarget_Relative;
                    out->target_at = 2;
                    out->target = at + 2 + (uint32_t)(int32_t)(int16_t)nuade_read_be16(d->mod + at + 2);
                    ok = true;
                }
            } else {
                ok = decode_ea(d, ea, 2);
            }
            break;
        case 6: {
            const uint32_t disp = w & 0xFFu;
            if (disp == 0xFFu) {
                break;
            }
            out->target_kind = WbTarget_Relative;
            if (disp == 0) {
                d->len += 2;
                out->target_at = 2;
                out->target = at + 2 + (uint32_t)(int32_t)(int16_t)nuade_read_be16(d->mod + at + 2);
            } else {
                out->target_at = 1;
                out->target = at + 2 + (uint32_t)(int32_t)(int8_t)disp;
            }
            d->flow_ends = (w & 0x0F00u) == 0;
            ok = true;
            break;
        }
        case 7:
            ok = (w & 0x0100u) == 0;
            break;
        case 8:
        case 9:
        case 11:
        case 12:
        case 13: {
            // the operand size: the divide / multiply instructions (8, 12) work on words, the address register forms
            // (9, 11, 13) are word at opmode 3 and long at 7
            const uint32_t opmode = (w >> 6) & 7;
            const bool data_ops = (w >> 12) == 8 || (w >> 12) == 12;
            const uint32_t size = (opmode == 3 || (opmode == 7 && data_ops)) ? 1u : (opmode == 7 ? 2u : (opmode & 3));
            ok = decode_ea(d, ea, size_to_imm(size));
            break;
        }
        case 14:
            ok = ((w >> 6) & 3) == 3 ? decode_ea(d, ea, 2) : true;
            break;
        default:
            break;
    }
    if (!ok || at + d->len > d->size) {
        return false;
    }
    out->len = (uint8_t)d->len;
    // custom chip registers are not data operands
    uint32_t kept = 0;
    for (uint32_t k = 0; k < out->slot_count; k++) {
        if ((nuade_read_be32(d->mod + at + out->slot_at[k]) >> 16) != WB_HARDWARE_HIGH) {
            out->slot_at[kept++] = out->slot_at[k];
        }
    }
    out->slot_count = (uint8_t)kept;
    return true;
}

bool wb_code_decode(WbCode* code, const uint8_t* mod, uint32_t size, uint32_t limit, uint32_t origin) {
    memset(code, 0, sizeof(*code));
    if (limit == 0 || limit > size || limit > 0xFFFEu) {
        return false;
    }
    uint8_t* covered = calloc(limit, 1);
    uint16_t* where = calloc(limit, sizeof(uint16_t)); // found index + 1 at each instruction start
    WbInsn* found = malloc(sizeof(WbInsn) * WB_MAX_INSNS);
    uint32_t* work = malloc(sizeof(uint32_t) * WB_MAX_INSNS);
    uint32_t found_count = 0;
    uint32_t work_count = 0;
    bool ok = where != nullptr && covered != nullptr && found != nullptr && work != nullptr;
    Decoder d = { .mod = mod, .size = size };
    if (ok) {
        work[work_count++] = 4;
        work[work_count++] = 0;
    }
    while (ok && work_count > 0) {
        uint32_t at = work[--work_count];
        while (ok) {
            if (at >= limit || (at & 1u) != 0) {
                ok = false;
                break;
            }
            if (where[at] != 0) {
                break;
            }
            if (covered[at] != 0 || found_count >= WB_MAX_INSNS) {
                ok = false;
                break;
            }
            WbInsn* insn = &found[found_count];
            if (!decode_one(&d, at, insn) || at + insn->len > limit) {
                ok = false;
                break;
            }
            for (uint32_t k = 0; k < insn->len && ok; k++) {
                ok = covered[at + k] == 0;
            }
            if (!ok) {
                break;
            }
            memset(covered + at, 1, insn->len);
            found_count++;
            where[at] = (uint16_t)found_count;
            if (insn->target_kind != WbTarget_None) {
                uint32_t target = insn->target;
                if (insn->target_kind == WbTarget_Absolute) {
                    target -= origin;
                    insn->target = target;
                }
                if (work_count >= WB_MAX_INSNS) {
                    ok = false;
                    break;
                }
                work[work_count++] = target;
            }
            if (d.flow_ends) {
                break;
            }
            at += insn->len;
        }
    }
    if (ok) {
        code->insn = malloc(sizeof(WbInsn) * found_count);
        code->index_at = calloc(limit, sizeof(uint16_t));
        ok = code->insn != nullptr && code->index_at != nullptr;
    }
    if (ok) {
        code->limit = limit;
        for (uint32_t at = 0; at < limit; at++) {
            if (where[at] != 0) {
                code->insn[code->count] = found[where[at] - 1u];
                code->count++;
                code->index_at[at] = (uint16_t)code->count;
            }
        }
        ok = code->count == found_count;
    }
    // every branch lands on an instruction
    for (uint32_t k = 0; ok && k < code->count; k++) {
        const WbInsn* insn = &code->insn[k];
        ok = insn->target_kind == WbTarget_None || (insn->target < limit && code->index_at[insn->target] != 0);
    }
    free(where);
    free(covered);
    free(found);
    free(work);
    if (!ok) {
        wb_code_free(code);
    }
    return ok;
}

void wb_code_free(WbCode* code) {
    free(code->insn);
    free(code->index_at);
    memset(code, 0, sizeof(*code));
}

const WbInsn* wb_code_insn_at(const WbCode* code, uint32_t at) {
    if (at >= code->limit || code->index_at[at] == 0) {
        return nullptr;
    }
    return &code->insn[code->index_at[at] - 1u];
}

static uint64_t fnv_byte(uint64_t h, uint8_t b) {
    return (h ^ b) * WB_FNV_PRIME;
}

uint64_t wb_code_hash(const WbCode* code, const uint8_t* mod, uint32_t masked_at) {
    uint64_t h = WB_FNV_OFFSET;
    for (uint32_t i = 0; i < code->count; i++) {
        const WbInsn* insn = &code->insn[i];
        uint8_t token[16];
        uint32_t n = insn->len;
        memcpy(token, mod + insn->at, n);
        for (uint32_t k = 0; k < insn->slot_count; k++) {
            memset(token + insn->slot_at[k], 0, 4);
        }
        if (insn->at == masked_at && n == 6) {
            memset(token + 2, 0, 4);
        }
        if (insn->target_kind != WbTarget_None) {
            const uint32_t target = code->index_at[insn->target] - 1u;
            if (insn->target_kind == WbTarget_Relative) {
                if (insn->target_at == 1) {
                    token[1] = 0;
                } else {
                    token[2] = 0;
                    token[3] = 0;
                }
                n = insn->target_at == 1 ? 2u : 4u;
                token[n++] = (uint8_t)(target >> 24);
                token[n++] = (uint8_t)(target >> 16);
                token[n++] = (uint8_t)(target >> 8);
                token[n++] = (uint8_t)target;
            } else {
                token[2] = (uint8_t)(target >> 24);
                token[3] = (uint8_t)(target >> 16);
                token[4] = (uint8_t)(target >> 8);
                token[5] = (uint8_t)target;
                n = 6;
            }
        }
        h = fnv_byte(h, (uint8_t)n);
        for (uint32_t k = 0; k < n; k++) {
            h = fnv_byte(h, token[k]);
        }
    }
    return h;
}

uint32_t wb_code_slots(const WbCode* code, const uint8_t* mod, uint32_t* out) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < code->count; i++) {
        const WbInsn* insn = &code->insn[i];
        for (uint32_t k = 0; k < insn->slot_count; k++) {
            if (n >= WB_MAX_SLOTS) {
                return n;
            }
            out[n++] = nuade_read_be32(mod + insn->at + insn->slot_at[k]);
        }
    }
    return n;
}
