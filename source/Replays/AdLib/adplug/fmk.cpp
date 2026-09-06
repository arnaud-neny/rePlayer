/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2007 Simon Peter, <dn.tlp@gmx.net>, et al.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * fmk.cpp - FM-Kingtracker v1.06 (.FMK) player
 *           by Dmitry Smagin <dmitry.s.smagin@gmail.com>
 * 
 * NOTES:
 * The player is based on the documentation provided with FMKING.EXE and
 * developed through ai-assisted reverse engineering
 * 
 * REFERENCES:
 * FM-Kingtracker (freeware) Copyright (c) 1998,1999 Sami Wilenius
 * https://web.archive.org/web/20050905064338/http://www.kolumbus.fi/sami.wilenius/
 *
 */

#include <stdlib.h>
#include <string.h>

#include "fmk.h"
#include "debug.h"

/* OPL note frequency table.  The FMK note byte is packed as octave*16 +
   semitone (NOT a linear index): block = note>>4, semitone = (note&0x0F)-1. */
static const uint16_t g_fnum_tab[12] = {
    0x159, 0x16D, 0x183, 0x19A, 0x1B3, 0x1CC,
    0x1E8, 0x205, 0x223, 0x244, 0x267, 0x28B
};

/* Signed sine LFO table and phase-increment table, reverse-engineered verbatim
   from FMKING.EXE.  Vibrato and tremolo share both.  Each tick the 8-bit phase
   advances by g_speedtab[x] (x = speed nibble); the offset is the signed table
   value divided by a depth-dependent divisor (16-depth vibrato, 17-depth
   tremolo; x86 idiv truncates toward zero). */
static const signed char g_lfo_sine[256] = {
     1,    4,    7,   10,   13,   16,   20,   23,   26,   29,   32,   35,   38,   41,   44,   47,
    49,   52,   55,   58,   61,   63,   66,   69,   71,   74,   76,   79,   81,   84,   86,   88,
    90,   93,   95,   97,   99,  101,  103,  104,  106,  108,  109,  111,  112,  114,  115,  116,
   118,  119,  120,  121,  122,  123,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
   127,  127,  127,  127,  126,  126,  125,  125,  124,  124,  123,  122,  121,  120,  119,  118,
   117,  116,  114,  113,  112,  110,  108,  107,  105,  103,  101,  100,   98,   96,   93,   91,
    89,   87,   85,   82,   80,   77,   75,   72,   70,   67,   65,   62,   59,   56,   53,   51,
    48,   45,   42,   39,   36,   33,   30,   27,   24,   21,   18,   15,   12,    8,    5,    2,
    -1,   -4,   -7,  -10,  -13,  -16,  -20,  -23,  -26,  -29,  -32,  -35,  -38,  -41,  -44,  -47,
   -49,  -52,  -55,  -58,  -61,  -63,  -66,  -69,  -71,  -74,  -76,  -79,  -81,  -84,  -86,  -88,
   -90,  -93,  -95,  -97,  -99, -101, -103, -104, -106, -108, -109, -111, -112, -114, -115, -116,
  -118, -119, -120, -121, -122, -123, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
  -127, -127, -127, -127, -126, -126, -125, -125, -124, -124, -123, -122, -121, -120, -119, -118,
  -117, -116, -114, -113, -112, -110, -108, -107, -105, -103, -101, -100,  -98,  -96,  -93,  -91,
   -89,  -87,  -85,  -82,  -80,  -77,  -75,  -72,  -70,  -67,  -65,  -62,  -59,  -56,  -53,  -51,
   -48,  -45,  -42,  -39,  -36,  -33,  -30,  -27,  -24,  -21,  -18,  -15,  -12,   -8,   -5,   -2,
};

static const uint8_t g_speedtab[17] = {
    0, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
};

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                  ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Copy a fixed-width, space-padded FMK name field: keep internal spaces,
   stop at a NUL, and trim trailing padding spaces. */
static void cpy_name(char *dst, const uint8_t *src, int n)
{
    int i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = (char)src[i];
    while (i > 0 && dst[i - 1] == ' ') i--;
    dst[i] = '\0';
}

static void note_to_fb(uint8_t note, uint16_t *fnum, uint8_t *block)
{
    if (note < 1) { *fnum = 0; *block = 0; return; }
    int semi = (note & 0x0F) - 1;
    if (semi < 0) semi = 0;
    if (semi > 11) semi = 11;
    *fnum = g_fnum_tab[semi];
    *block = (uint8_t)((note >> 4) & 0x07);
}

static void note_add_fb(uint8_t note, int add, uint16_t *fnum, uint8_t *block)
{
    if (note < 1) { *fnum = 0; *block = 0; return; }
    int semi = (note & 0x0F) - 1;
    if (semi < 0) semi = 0;
    int oct = (note >> 4) & 0x07;
    int a = oct * 12 + semi + add;
    if (a < 0) a = 0;
    oct = a / 12;
    semi = a % 12;
    if (oct > 7) oct = 7;
    *fnum = g_fnum_tab[semi];
    *block = (uint8_t)oct;
}

/* signed LFO offset for the current phase (see table comment above). */
static int lfo_value(uint8_t phase, int divisor)
{
    if (divisor < 1) divisor = 1;
    return (int)g_lfo_sine[phase] / divisor;
}

static uint8_t vol_to_att(uint8_t vol)
{
    if (vol > 63) vol = 63;
    return vol;
}

/*** public methods **************************************/

CPlayer *CfmkPlayer::factory(Copl *newopl)
{
    return new CfmkPlayer(newopl);
}

CfmkPlayer::CfmkPlayer(Copl *newopl)
    : CPlayer(newopl), m_data(0), m_size(0)
{
    m_song_name[0] = 0;
    m_composer[0] = 0;

    /* Song header defaults; parse() overwrites these from the file. */
    m_file_type = 0;
    m_global = 0;
    m_base_speed = 50;
    m_init_speed = 0;
    m_order_len = 0;
    m_num_instrs = 0;
    m_num_patterns = 0;
    m_num_tracks = 0;

    memset(m_track_pan, 0, sizeof(m_track_pan));
    memset(m_track_type, 0, sizeof(m_track_type));
    memset(m_track_opl, 0, sizeof(m_track_opl));
    memset(m_order, 0, sizeof(m_order));
    memset(m_pat_ptr, 0, sizeof(m_pat_ptr));
    memset(m_instr_ptr, 0, sizeof(m_instr_ptr));
    memset(m_instrs, 0, sizeof(m_instrs));

    /* Replayer state; rewind() resets all of this again before playback. */
    m_pos_order = 0;
    m_cur_pattern = 0;
    m_row = 0;
    m_tick = 0;
    m_speed = 6;
    m_opl3 = 0;
    m_stereo = 0;
    m_rhythm = 0;
    m_song_end = 0;
    m_cur_chip = -1;

    memset(m_regs, 0, sizeof(m_regs));
    memset(m_ch, 0, sizeof(m_ch));
    memset(m_cells, 0, sizeof(m_cells));
    memset(m_pat_cache, 0, sizeof(m_pat_cache));
    memset(m_pat_loaded, 0, sizeof(m_pat_loaded));
}

CfmkPlayer::~CfmkPlayer()
{
    if (m_data) free(m_data);
}

bool CfmkPlayer::load(const std::string &filename, const CFileProvider &fp)
{
    binistream *f = fp.open(filename);
    if (!f) return false;

    if (!fp.extension(filename, ".fmk")) {
        AdPlug_LogWrite("CfmkPlayer::load(\"%s\"): Not an FMK file!\n", filename.c_str());
        fp.close(f);
        return false;
    }

    unsigned long fsize = fp.filesize(f);
    if (fsize < 64 || fsize > (1UL << 20)) {
        fp.close(f);
        return false;
    }

    if (m_data) { free(m_data); m_data = 0; }
    m_data = (uint8_t *)malloc(fsize);
    if (!m_data) { fp.close(f); return false; }
    for (unsigned long i = 0; i < fsize; i++)
        m_data[i] = (uint8_t)f->readInt(1);
    m_size = (size_t)fsize;
    fp.close(f);

    if (!parse()) {
        free(m_data); m_data = 0; m_size = 0;
        return false;
    }

    rewind(0);
    return true;
}

/* Parse the FMK header, order list, instrument/pattern pointers and
   instruments into the member state.  Returns false on a bad signature. */
bool CfmkPlayer::parse()
{
    const uint8_t *d = m_data;

    if (memcmp(d, "FMK!", 4) != 0) return false;
    if (d[60] != 0xF4) return false;

    /* header text fields, each 28 bytes, space-padded */
    cpy_name(m_song_name, d + 4, 28);
    cpy_name(m_composer, d + 32, 28);

    m_file_type    = d[61];
    m_global       = d[62];
    m_base_speed   = d[63];
    m_init_speed   = d[64];
    m_order_len    = d[74];
    m_num_instrs   = d[75];
    m_num_patterns = d[76];

    m_num_tracks = (m_file_type >= 2) ? 18 : 20;

    /* track pan: 5 bytes hold 4 tracks each */
    size_t p = 77;
    for (int i = 0; i < m_num_tracks; i++) {
        uint8_t b = d[p + i / 4];
        int shift = (i % 4) * 2;
        m_track_pan[i] = (b >> shift) & 0x03;
    }

    /* track settings: one byte per track, starting at offset 82 */
    p = 82;
    for (int i = 0; i < m_num_tracks; i++) {
        uint8_t v = d[p + i];
        m_track_type[i] = v & 0x07;
        m_track_opl[i]  = (v >> 3) & 0x1F;
    }
    p = 82 + m_num_tracks;

    /* order list */
    for (int i = 0; i < m_order_len && i < 255; i++)
        m_order[i] = d[p + i];
    p += m_order_len;

    /* song message pointer (word) - skipped */
    p += 2;

    /* instrument pointers */
    for (int i = 0; i < m_num_instrs && (unsigned)i < FMK_MAX_INSTRS; i++) {
        m_instr_ptr[i] = rd16(d + p);
        p += 2;
    }

    /* pattern pointers (long) */
    for (int i = 0; i < m_num_patterns && (unsigned)i < FMK_MAX_PATTERNS; i++) {
        m_pat_ptr[i] = rd32(d + p);
        p += 4;
    }

    /* load instruments */
    memset(m_instrs, 0, sizeof(m_instrs));
    for (int i = 0; i < m_num_instrs && (unsigned)i < FMK_MAX_INSTRS; i++) {
        uint32_t off = m_instr_ptr[i];
        if (off == 0 || off + 50 > m_size) continue;
        const uint8_t *id = d + off;
        fmk_instr_t *ins = &m_instrs[i];
        for (int r = 0; r < (int)FMK_INST_REGS; r++)
            ins->regs[r] = id[39 + r];  // 39..49: 20h,23h,40h,43h,60h,63h,80h,83h,E0h,E3h,C0h
    }

    return true;
}

/* Route an OPL register write through AdPlug's Copl.  FMK addresses the OPL3
   using bank bit 0x100 for the second register set; AdPlug selects the bank
   with setchip().  We also mirror every write so the effect handlers can read
   back the current register value. */
void CfmkPlayer::oplwrite(uint16_t reg, uint8_t val)
{
    int bank = (reg >> 8) & 1;
    int r = reg & 0xFF;
    if (r < (int)FMK_REG_BANK)
        m_regs[bank * FMK_REG_BANK + r] = val;
    if (bank != m_cur_chip) {
        opl->setchip(bank);
        m_cur_chip = bank;
    }
    opl->write(r, val);
}

int CfmkPlayer::track_to_chan(int track, uint8_t *opl_ch, uint8_t *is4op,
                              uint8_t *is4op_main)
{
    uint8_t o = m_track_opl[track];
    if (o == 21 || o == 0) return 0;
    uint8_t ch = o - 1;             /* 1..18 -> 0..17 */
    uint8_t t = m_track_type[track];
    *is4op = (t == 6) ? 1 : 0;
    *opl_ch = ch;
    if (*is4op)
        *is4op_main = ((ch <= 2) || (ch >= 9 && ch <= 11)) ? 1 : 0;
    else
        *is4op_main = 0;
    return 1;
}

void CfmkPlayer::write_op_reg_ch(uint8_t ch, uint8_t slot, uint8_t base, uint8_t val)
{
    uint8_t bank = (ch >= 9) ? 1 : 0;
    uint8_t c = ch % 9;
    uint8_t off;
    if (c < 3)      off = (uint8_t)(c + slot);
    else if (c < 6) off = (uint8_t)(8 + (c - 3) + slot);
    else            off = (uint8_t)(16 + (c - 6) + slot);
    uint16_t reg = (uint16_t)(base + off);
    if (bank) reg |= 0x100;
    oplwrite(reg, val);
}

uint8_t CfmkPlayer::read_op_reg_ch(uint8_t ch, uint8_t slot, uint8_t base)
{
    uint8_t bank = (ch >= 9) ? 1 : 0;
    uint8_t c = ch % 9;
    uint8_t off;
    if (c < 3)      off = (uint8_t)(c + slot);
    else if (c < 6) off = (uint8_t)(8 + (c - 3) + slot);
    else            off = (uint8_t)(16 + (c - 6) + slot);
    return m_regs[bank * FMK_REG_BANK + (base + off)];
}

/* op: 0->mod, 1->car, 2->mod(partner), 3->car(partner); partner = main+3 */
void CfmkPlayer::write_op_reg(uint8_t opl_ch, uint8_t op, uint8_t base, uint8_t val)
{
    if (op < 2)
        write_op_reg_ch(opl_ch, (op == 1) ? 3 : 0, base, val);
    else
        write_op_reg_ch((uint8_t)(opl_ch + 3), (op == 3) ? 3 : 0, base, val);
}

uint8_t CfmkPlayer::read_op_reg(uint8_t opl_ch, uint8_t op, uint8_t base)
{
    if (op < 2)
        return read_op_reg_ch(opl_ch, (op == 1) ? 3 : 0, base);
    else
        return read_op_reg_ch((uint8_t)(opl_ch + 3), (op == 3) ? 3 : 0, base);
}

void CfmkPlayer::write_chan_reg(uint8_t opl_ch, uint8_t base, uint8_t val)
{
    uint8_t bank = (opl_ch >= 9) ? 1 : 0;
    uint8_t ch = opl_ch % 9;
    uint16_t reg = (uint16_t)(base + ch);
    if (bank) reg |= 0x100;
    oplwrite(reg, val);
}

/* Program an instrument into a channel's OPL operators.  inst_idx is 1-based. */
void CfmkPlayer::apply_instrument(int ci, uint8_t inst_idx)
{
    fmk_chan_t *c = &m_ch[ci];
    if (inst_idx == 0 || inst_idx > m_num_instrs) return;
    const fmk_instr_t *ins = &m_instrs[inst_idx - 1];
    uint8_t opl_ch = c->opl_ch;

    /* Every track (including type-6 4-op tracks) is programmed as a plain 2-op
       voice on its own channel; the two channels of a 4-op pair are combined in
       hardware via reg 0x104. */
    uint8_t r20[2] = { ins->regs[0], ins->regs[1] };
    uint8_t r40[2] = { ins->regs[2], ins->regs[3] };
    uint8_t r60[2] = { ins->regs[4], ins->regs[5] };
    uint8_t r80[2] = { ins->regs[6], ins->regs[7] };
    uint8_t rE0[2] = { ins->regs[8], ins->regs[9] };

    for (int o = 0; o < 2; o++) {
        write_op_reg(opl_ch, o, 0x20, r20[o]);
        write_op_reg(opl_ch, o, 0x40, r40[o]);
        write_op_reg(opl_ch, o, 0x60, r60[o]);
        write_op_reg(opl_ch, o, 0x80, r80[o]);
        write_op_reg(opl_ch, o, 0xE0, rE0[o]);
    }

    /* C0h: feedback (bits0-2) + connection (bit3) */
    uint8_t c0 = ins->regs[10] & 0x0F;
    write_chan_reg(opl_ch, 0xC0, c0);

    /* pan via C0h bits4-5 (OPL3 stereo).  FMK pan is L/R swapped relative to OPL:
       pan=0(left)->0x20, pan=2(right)->0x10, centre->0x30. */
    if (m_opl3 && m_stereo) {
        uint8_t panbits = (c->pan == 0) ? 0x20 : (c->pan == 2 ? 0x10 : 0x30);
        uint8_t cur = m_regs[(opl_ch >= 9 ? 1 : 0) * FMK_REG_BANK + 0xC0 + (opl_ch % 9)];
        write_chan_reg(opl_ch, 0xC0, (uint8_t)((cur & 0x0F) | panbits));
    }

    memcpy(c->inst_regs, ins->regs, FMK_INST_REGS);
}

void CfmkPlayer::write_freq(int ci, uint16_t fnum, uint8_t block, uint8_t keyon)
{
    fmk_chan_t *c = &m_ch[ci];
    uint8_t opl_ch = c->opl_ch;
    uint8_t b0 = (uint8_t)(fnum & 0xFF);
    uint8_t b1 = (uint8_t)(((fnum >> 8) & 0x03) | ((block & 0x07) << 2) | (keyon ? 0x20 : 0x00));
    write_chan_reg(opl_ch, 0xA0, b0);
    write_chan_reg(opl_ch, 0xB0, b1);
}

void CfmkPlayer::set_freq_key(int ci, uint8_t keyon)
{
    fmk_chan_t *c = &m_ch[ci];
    write_freq(ci, c->cur_fnum, c->cur_block, keyon);
}

/* The FMK volume column stores an OPL *attenuation* directly (0=loudest,
   63=silent) and, unlike ST3-derived FM trackers, FMKING ADDS it to BOTH
   operators' instrument base levels (Mle/Cle), clamped to 63. */
void CfmkPlayer::apply_volume(int ci)
{
    fmk_chan_t *c = &m_ch[ci];
    uint8_t opl_ch = c->opl_ch;
    uint8_t vol = vol_to_att(c->cur_vol);
    int m = (c->inst_regs[2] & 0x3F) + vol; if (m > 63) m = 63;
    int cr = (c->inst_regs[3] & 0x3F) + vol; if (cr > 63) cr = 63;
    write_op_reg(opl_ch, 0, 0x40, (uint8_t)((c->inst_regs[2] & 0xC0) | m));
    write_op_reg(opl_ch, 1, 0x40, (uint8_t)((c->inst_regs[3] & 0xC0) | cr));
}

/* Decompress a packed pattern into m_pat_cache[pat]. */
void CfmkPlayer::decompress_pattern(uint8_t pat)
{
    if (pat >= m_num_patterns) {
        memset(m_cells, 0, sizeof(m_cells));
        return;
    }
    uint32_t off = m_pat_ptr[pat];
    if (off == 0 || off + 2 > m_size) {
        memset(m_cells, 0, sizeof(m_cells));
        return;
    }
    const uint8_t *pd = m_data + off;
    uint16_t plen = rd16(pd);
    const uint8_t *end = pd + 2 + plen;
    if (end > m_data + m_size) end = m_data + m_size;
    const uint8_t *p = pd + 2;

    uint8_t row_cells[FMK_MAX_CH * 6];
    int row = 0;
    while (p < end && row < (int)FMK_MAX_ROWS) {
        memset(row_cells, 0, sizeof(row_cells));
        for (;;) {
            if (p >= end) break;
            uint8_t b = *p++;
            if (b == 0) break;              /* row done */
            int track = (b & 0x1F);         /* 1..20 */
            if (track < 1 || track > (int)FMK_MAX_CH) {
                /* still must consume this cell's bytes to stay in sync */
                if (b & 0x20) p += 2;
                if (b & 0x40) p += 1;
                if (b & 0x80) p += 2;
                continue;
            }
            int ti = track - 1;
            uint8_t *cl = &row_cells[ti * 6];
            if (b & 0x20) { cl[0] = *p++; cl[1] = *p++; cl[5] |= 0x01 | 0x02; }
            if (b & 0x40) { cl[2] = *p++; cl[5] |= 0x04; }
            if (b & 0x80) { cl[3] = *p++; cl[4] = *p++; cl[5] |= 0x08; }
        }
        memcpy(m_pat_cache[pat][row], row_cells, sizeof(row_cells));
        row++;
    }
    for (; row < (int)FMK_MAX_ROWS; row++)
        memset(m_pat_cache[pat][row], 0, sizeof(row_cells));
    m_pat_loaded[pat] = 1;
}

void CfmkPlayer::load_row_cells()
{
    if (m_cur_pattern >= FMK_MAX_PATTERNS) {
        memset(m_cells, 0, sizeof(m_cells));
        return;
    }
    const uint8_t *rc = m_pat_cache[m_cur_pattern][m_row];
    for (int t = 0; t < (int)FMK_MAX_CH; t++) {
        const uint8_t *cl = &rc[t * 6];
        fmk_cell_t *c = &m_cells[t];
        c->note = cl[0];
        c->instr = cl[1];
        c->volume = cl[2];
        c->cmd = cl[3];
        c->infobyte = cl[4];
        c->flags = cl[5];
    }
}

void CfmkPlayer::handle_row()
{
    /* Row-scoped effects (arpeggio) are active only on their row, so clear the
       command selection each row.  Vibrato/tremolo are NOT cleared here: FMKING
       keeps the sine LFO running across subsequent rows until a new note (or a
       zero-depth re-arm) stops it. */
    for (int i = 0; i < (int)FMK_MAX_CH; i++) {
        m_ch[i].cur_cmd = 0;
        m_ch[i].arp_on = 0;
    }

    for (int t = 0; t < (int)FMK_MAX_CH; t++) {
        fmk_cell_t *cl = &m_cells[t];
        if (!(cl->flags & 0x0F)) continue;

        uint8_t opl_ch, is4op, is4op_main;
        if (!track_to_chan(t, &opl_ch, &is4op, &is4op_main)) continue;
        int ci = opl_ch;
        fmk_chan_t *c = &m_ch[ci];
        c->opl_ch = opl_ch;
        c->is_4op = is4op;
        c->is_4op_main = is4op_main;
        c->active = 1;

        int instr_applied = 0;
        if (cl->flags & 0x02) {
            c->cur_instr = cl->instr;
            apply_instrument(ci, cl->instr);
            instr_applied = 1;
        }
        if (cl->flags & 0x01) {
            if (cl->note == 254) {
                c->cut = 1;
                c->note_on = 0;
                write_chan_reg(opl_ch, 0xB0, 0x1C);   /* FMKING key-off on a cut note */
            } else if (cl->note >= 1 && cl->note <= 253) {
                /* Track type (settings bits 0-2): 0=normal,1=hihat,2=cymbal,3=tom,
                   4=snare,5=bass,6=4op.  FMKING only sets the 0xB0 key-on bit for
                   types 0 and 6; rhythm types 1-5 get their frequency written but no
                   key bit (they sound only via OPL rhythm mode, reg 0xBD). */
                int rhythm_type = (m_track_type[t] >= 1 && m_track_type[t] <= 5);
                if (!instr_applied && c->cur_instr > 0 && c->cur_instr <= m_num_instrs)
                    apply_instrument(ci, c->cur_instr);
                c->cur_note = cl->note;
                uint16_t fn; uint8_t bl;
                note_to_fb(cl->note, &fn, &bl);
                c->cur_fnum = fn;
                c->cur_block = bl;
                c->cut = 0;
                if (rhythm_type) {
                    c->note_on = 0;
                    set_freq_key(ci, 0);
                } else {
                    c->note_on = 1;
                    /* NOTE: vibrato/tremolo LFO state (depth, speed, phase) is NOT
                       touched by a new note.  Reverse-engineered from FMKING's row
                       processor (0x463D), which copies note/instrument/volume/cmd
                       fields per channel but never writes the LFO phase/depth fields;
                       those are only written from inside the Hxy/Rxy dispatch case.
                       The vibrato sine therefore continues smoothly THROUGH note
                       changes as long as H00 keeps being specified. */
                    /* FMKING keys the channel OFF before every note-on so the envelope
                       generator retriggers from attack (percussive patches otherwise
                       decay to silence). */
                    write_chan_reg(opl_ch, 0xB0, 0x1C);
                    set_freq_key(ci, 1);
                }
                if (c->vol_set)
                    apply_volume(ci);
            }
        }
        if (cl->flags & 0x04) {
            c->cur_vol = cl->volume;
            c->vol_set = 1;
            apply_volume(ci);
        }
        if (cl->flags & 0x08) {
            c->cur_cmd = cl->cmd;
            c->cur_info = cl->infobyte;
            handle_effect(ci, cl->cmd, cl->infobyte, 1);
        }
    }
}

void CfmkPlayer::handle_tick_effects()
{
    for (int ci = 0; ci < (int)FMK_MAX_CH; ci++) {
        fmk_chan_t *c = &m_ch[ci];
        if (!c->active) continue;

        uint8_t cmd = c->cur_cmd;
        if (cmd == 0) continue;

        switch (cmd) {
        case 10: { /* Jxy arpeggio: cycle base / +x / +y semitones per tick */
            if (!c->arp_on) break;
            int sel = m_tick % 3;
            int add = (sel == 0) ? 0 : (sel == 1 ? c->arp_x : c->arp_y);
            uint16_t fn; uint8_t bl;
            note_add_fb(c->cur_note, add, &fn, &bl);
            write_freq(ci, fn, bl, c->note_on);
            break;
        }
        default:
            handle_effect(ci, cmd, c->cur_info, 0);
            break;
        }
    }
}

void CfmkPlayer::next_row()
{
    m_row++;
    if (m_row >= FMK_MAX_ROWS) {
        m_row = 0;
        m_pos_order++;
        if (m_pos_order >= m_order_len || m_order[m_pos_order] == 0) {
            m_song_end = 1;
            return;
        }
        m_cur_pattern = m_order[m_pos_order];
        decompress_pattern(m_cur_pattern);
    }
    load_row_cells();
    handle_row();
}

bool CfmkPlayer::update()
{
    if (m_song_end) return false;

    if (m_tick == 0) {
        load_row_cells();
        handle_row();
    } else {
        handle_tick_effects();
    }
    m_tick++;
    if (m_tick >= m_speed) {
        m_tick = 0;
        next_row();
    }
    return !m_song_end;
}

void CfmkPlayer::rewind(int)
{
    m_opl3   = (m_global & 0x02) ? 1 : 0;
    m_stereo = (m_global & 0x01) ? 1 : 0;
    m_rhythm = (m_global & 0x04) ? 1 : 0;
    m_speed  = m_init_speed ? m_init_speed : 6;
    if (!m_base_speed) m_base_speed = 50;

    m_pos_order = 0;
    m_row = 0;
    m_tick = 0;
    m_song_end = 0;

    memset(m_regs, 0, sizeof(m_regs));
    memset(m_ch, 0, sizeof(m_ch));
    memset(m_pat_loaded, 0, sizeof(m_pat_loaded));
    memset(m_cells, 0, sizeof(m_cells));

    /* channel mapping: pans + 4op partners */
    for (int t = 0; t < (int)FMK_MAX_CH; t++) {
        uint8_t opl_ch, is4op, is4op_main;
        if (track_to_chan(t, &opl_ch, &is4op, &is4op_main)) {
            m_ch[opl_ch].opl_ch = opl_ch;
            m_ch[opl_ch].is_4op = is4op;
            m_ch[opl_ch].is_4op_main = is4op_main;
            m_ch[opl_ch].pan = m_track_pan[t];
            m_ch[opl_ch].cur_vol = 0;
            if (is4op) {
                uint8_t partner = (uint8_t)(opl_ch + 3);
                m_ch[opl_ch].four_op_pair = partner;
                if (partner < FMK_MAX_CH) {
                    m_ch[partner].four_op_pair = opl_ch;
                    m_ch[partner].cur_vol = 0;
                }
            }
        }
    }

    opl->init();
    m_cur_chip = -1;
    opl_init();

    m_cur_pattern = (m_order_len > 0) ? m_order[0] : 0;
    decompress_pattern(m_cur_pattern);
}

/* Program the OPL exactly as FMKING does at song load. */
void CfmkPlayer::opl_init()
{
    if (m_opl3) {
        oplwrite(0x105, 0x01);                 /* OPL3 mode (NEW bit) */
        if (m_rhythm)
            oplwrite(0x105, 0x01 | 0x20);
    }
    oplwrite(0x01, 0x20);                     /* WSE: enable waveform select */
    oplwrite(0x02, 0x00);
    oplwrite(0x03, 0x00);
    oplwrite(0x04, 0x00);
    oplwrite(0x08, 0x40);                     /* NTS: note-select */

    /* 0xBD deep-vibrato/deep-tremolo from the global-flags byte. */
    {
        uint8_t bd = 0x00;
        if (m_global & 0x10) bd |= 0x40;        /* DVB deep vibrato */
        if (m_global & 0x08) bd |= 0x80;        /* DAM deep tremolo */
        oplwrite(0xBD, bd);
    }

    /* FMKING default-patches only the 9 bank-0 channels; bank-1 voices are left
       at power-on zero until a note actually plays on them. */
    for (int ch = 0; ch < 9; ch++) {
        for (int slot = 0; slot <= 3; slot += 3) {
            /* The carrier's TL genuinely differs from the modulator's: FMKING's
               init loop pushes 0x10 for the mod-TL write but 0x00 for the
               car-TL write. */
            uint8_t mod_tl = (slot == 0) ? 0x10 : 0x00;
            write_op_reg_ch((uint8_t)ch, (uint8_t)slot, 0x20, 0x01);
            write_op_reg_ch((uint8_t)ch, (uint8_t)slot, 0x40, mod_tl);
            write_op_reg_ch((uint8_t)ch, (uint8_t)slot, 0x60, 0xF0);
            write_op_reg_ch((uint8_t)ch, (uint8_t)slot, 0x80, 0x77);
            write_op_reg_ch((uint8_t)ch, (uint8_t)slot, 0xE0, 0x01);
        }
        write_chan_reg((uint8_t)ch, 0xC0, 0x01);
    }

    /* enable 4-op pairs in reg 0x104 */
    if (m_opl3) {
        uint8_t nv = 0;
        for (int i = 0; i < (int)FMK_MAX_CH; i++) {
            if (m_ch[i].is_4op) {
                uint8_t c = m_ch[i].opl_ch;
                if (c >= 3 && c <= 5) c = (uint8_t)(c - 3);
                else if (c >= 12 && c <= 14) c = (uint8_t)(c - 3);
                uint8_t bit = (c < 3) ? c : (uint8_t)(c - 6);
                nv |= (uint8_t)(1 << bit);
            }
        }
        oplwrite(0x104, nv);
    }
}

/*** effects *********************************************/

void CfmkPlayer::handle_effect(int ci, uint8_t cmd, uint8_t info, uint8_t rowtick)
{
    fmk_chan_t *c = &m_ch[ci];

    switch (cmd) {
    case 1: /* Axx set speed */
        if (info) m_speed = info;
        break;
    case 2: /* Bxx jump to order */
        if (rowtick && info < m_order_len) {
            m_pos_order = info;
            m_cur_pattern = m_order[info];
            m_row = 0; m_tick = 0;
            decompress_pattern(m_cur_pattern);
            load_row_cells();
            handle_row();
        }
        break;
    case 16: /* Pxx break to row xx */
        if (rowtick) {
            m_row = info & 0x3F;
            m_tick = 0;
            m_pos_order++;
            if (m_pos_order >= m_order_len) { m_song_end = 1; break; }
            m_cur_pattern = m_order[m_pos_order];
            decompress_pattern(m_cur_pattern);
            load_row_cells();
            handle_row();
        }
        break;
    case 4: /* Dxy volume slide (attenuation: up=louder=decrease) */
    {
        c->vol_slide = info;
        int v = (int)c->cur_vol;
        int changed = 0;
        if ((info & 0x0F) == 0x0F) {          /* DxF fine up by x */
            if (rowtick) { v -= (info >> 4); changed = 1; }
        } else if ((info & 0xF0) == 0xF0) {   /* DFy fine down by y */
            if (rowtick) { v += (info & 0x0F); changed = 1; }
        } else if ((info & 0x0F) == 0x00) {   /* Dx0 up by x (every 5th tick) */
            if (m_tick % 5 == 0) { v -= (info >> 4); changed = 1; }
        } else if ((info & 0xF0) == 0x00) {   /* D0y down by y (every 5th tick) */
            if (m_tick % 5 == 0) { v += (info & 0x0F); changed = 1; }
        }
        if (changed) {
            if (v < 0) v = 0;
            if (v > 63) v = 63;
            c->cur_vol = (uint8_t)v;
            c->vol_set = 1;
            apply_volume(ci);
        }
        break;
    }
    case 5: /* Exx note slide down */
        c->note_slide_dir = 0;
        c->note_slide_speed = info;
        if ((info & 0x0F) == 0x0F) {
            if (rowtick) { c->cur_fnum = (uint16_t)(c->cur_fnum - (info >> 4)); set_freq_key(ci, c->note_on); }
        } else if (info) {
            c->note_slide_acc = (uint16_t)(c->note_slide_acc + info);
            if (c->note_slide_acc >= 50) {
                c->cur_fnum = (uint16_t)(c->cur_fnum - (c->note_slide_acc / 50));
                c->note_slide_acc %= 50;
                set_freq_key(ci, c->note_on);
            }
        }
        break;
    case 6: /* Fxx note slide up */
        c->note_slide_dir = 1;
        c->note_slide_speed = info;
        if ((info & 0x0F) == 0x0F) {
            if (rowtick) { c->cur_fnum = (uint16_t)(c->cur_fnum + (info >> 4)); set_freq_key(ci, c->note_on); }
        } else if (info) {
            c->note_slide_acc = (uint16_t)(c->note_slide_acc + info);
            if (c->note_slide_acc >= 50) {
                c->cur_fnum = (uint16_t)(c->cur_fnum + (c->note_slide_acc / 50));
                c->note_slide_acc %= 50;
                set_freq_key(ci, c->note_on);
            }
        }
        break;
    case 7: /* Gxx tone portamento */
        c->port_speed = info;
        if (info && c->port_target) {
            int diff = c->port_target - (int)c->cur_fnum;
            int step = (diff > 0) ? info : -info;
            c->cur_fnum = (uint16_t)(c->cur_fnum + step);
            set_freq_key(ci, c->note_on);
        }
        break;
    case 8: /* Hxy vibrato (x=speed, y=depth); H00 continues unchanged.
               Re-evaluates the LFO on every tick it is dispatched: reads the
               CURRENT phase, writes the modulated fnum, THEN advances the
               phase (advancing first would run a tick ahead of the player). */
        if (info) {
            c->vib_speed = info >> 4;
            c->vib_depth = info & 0x0F;
        }
        if (c->vib_depth) {
            int off = lfo_value(c->vib_phase, 16 - (int)c->vib_depth);
            int f = (int)c->cur_fnum + off;
            if (f < 0) f = 0;
            if (f > 0x3FF) f = 0x3FF;
            write_freq(ci, (uint16_t)f, c->cur_block, c->note_on);
            c->vib_phase = (uint8_t)(c->vib_phase +
                g_speedtab[c->vib_speed <= 16 ? c->vib_speed : 16]);
        }
        break;
    case 9: /* Ixx retrig */
        c->retrig_speed = info;
        if (info && (m_tick % info == 0)) {
            set_freq_key(ci, 0);
            set_freq_key(ci, 1);
        }
        break;
    case 10: /* Jxy arpeggio */
        c->arp_on = 1;
        c->arp_x = info >> 4;
        c->arp_y = info & 0x0F;
        break;
    case 3: /* Cxy carrier param */
    {
        uint8_t x = info >> 4, y = info & 0x0F;
        c->car_params[x & 7] = y;
        if (x >= 1 && x <= 7) {
            uint8_t base = (x==1)?0x20:(x==2)?0x40:(x==3)?0x60:(x==4)?0x60:(x==5)?0x80:(x==6)?0x80:0xE0;
            uint8_t shift = (x==2)?6:(x==3)?4:(x==5)?4:0;
            uint8_t mask = (x==7)?0xF8:0x0F;
            uint8_t cur = read_op_reg(c->opl_ch, 1, base);
            write_op_reg(c->opl_ch, 1, base, (uint8_t)((cur & mask) | (y << shift)));
        } else if (x == 8) {
            uint8_t cur = m_regs[(c->opl_ch>=9?1:0)*FMK_REG_BANK + 0xC0 + (c->opl_ch%9)];
            write_chan_reg(c->opl_ch, 0xC0, (uint8_t)((cur & 0xF8) | (y & 0x07)));
        }
        break;
    }
    case 13: /* Mxy modulator param */
    {
        uint8_t x = info >> 4, y = info & 0x0F;
        c->mod_params[x & 7] = y;
        if (x >= 1 && x <= 7) {
            uint8_t base = (x==1)?0x20:(x==2)?0x40:(x==3)?0x60:(x==4)?0x60:(x==5)?0x80:(x==6)?0x80:0xE0;
            uint8_t shift = (x==2)?6:(x==3)?4:(x==5)?4:0;
            uint8_t mask = (x==7)?0xF8:0x0F;
            uint8_t cur = read_op_reg(c->opl_ch, 0, base);
            write_op_reg(c->opl_ch, 0, base, (uint8_t)((cur & mask) | (y << shift)));
        }
        break;
    }
    case 14: /* N0x set waveform */
        c->wave_sel = info & 0x07;
        {
            uint8_t cur = read_op_reg(c->opl_ch, 1, 0xE0);
            write_op_reg(c->opl_ch, 1, 0xE0, (uint8_t)((cur & 0xF8) | c->wave_sel));
        }
        break;
    case 18: /* Rxy tremolo (x=speed, y=depth); R00 continues unchanged.
                Same current-phase-then-advance stepping as vibrato. */
        if (info) {
            c->trem_speed = info >> 4;
            c->trem_depth = info & 0x0F;
        }
        if (c->trem_depth) {
            int delta = lfo_value(c->trem_phase, 17 - (int)c->trem_depth);
            int att = (int)vol_to_att(c->cur_vol) + delta;
            if (att < 0) att = 0;
            if (att > 63) att = 63;
            uint8_t cur = read_op_reg(c->opl_ch, 1, 0x40);
            write_op_reg(c->opl_ch, 1, 0x40, (uint8_t)((cur & 0xC0) | (att & 0x3F)));
            c->trem_phase = (uint8_t)(c->trem_phase +
                g_speedtab[c->trem_speed <= 16 ? c->trem_speed : 16]);
        }
        break;
    case 19: /* S0x stereo control */
    {
        uint8_t x = info & 0x0F;
        c->pan = (x == 1) ? 0 : (x == 3 ? 2 : 1);
        if (m_opl3 && m_stereo) {
            uint8_t panbits = (c->pan == 0) ? 0x20 : (c->pan == 2 ? 0x10 : 0x30);
            uint8_t cur = m_regs[(c->opl_ch>=9?1:0)*FMK_REG_BANK + 0xC0 + (c->opl_ch%9)];
            write_chan_reg(c->opl_ch, 0xC0, (uint8_t)((cur & 0x0F) | panbits));
        }
        break;
    }
    case 20: /* Txx modulator volume.  Unlike the pattern volume column (already
                an OPL-style attenuation, 0=loud..63=silent), Txx/Uxx take a true
                volume 0..63 (63=loud), so it must be inverted to attenuation.
             */
        c->mod_vol = info;
        {
            uint8_t att = (uint8_t)(63 - vol_to_att(info));
            uint8_t cur = read_op_reg(c->opl_ch, 0, 0x40);
            write_op_reg(c->opl_ch, 0, 0x40, (uint8_t)((cur & 0xC0) | att));
        }
        break;
    case 21: /* Uxx carrier volume (see case 20: inverted volume, not attenuation). */
        c->car_vol = info;
        {
            uint8_t att = (uint8_t)(63 - vol_to_att(info));
            uint8_t cur = read_op_reg(c->opl_ch, 1, 0x40);
            write_op_reg(c->opl_ch, 1, 0x40, (uint8_t)((cur & 0xC0) | att));
        }
        break;
    case 22: /* Vxx: FMKING has no dispatcher case; no-op */
    default:
        break;
    }
}
