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

#ifndef H_ADPLUG_FMKPLAYER
#define H_ADPLUG_FMKPLAYER

#include <stdint.h>
#include "player.h"

class CfmkPlayer: public CPlayer
{
public:
    static CPlayer *factory(Copl *newopl);

    CfmkPlayer(Copl *newopl);
    ~CfmkPlayer();

    bool load(const std::string &filename, const CFileProvider &fp);
    bool update();
    void rewind(int subsong);
    float getrefresh() { return (float)m_base_speed; }

    std::string gettype() { return std::string("FM-Kingtracker"); }
    std::string gettitle() { return std::string(m_song_name); }
    std::string getauthor() { return std::string(m_composer); }
    unsigned int getinstruments() { return m_num_instrs; }
    unsigned int getpatterns() { return m_num_patterns; }
    unsigned int getpattern() { return m_cur_pattern; }
    unsigned int getorders() { return m_order_len; }
    unsigned int getorder() { return m_pos_order; }
    unsigned int getrow() { return m_row; }
    unsigned int getspeed() { return m_speed; }

protected:
    static const unsigned int FMK_MAX_TRACKS = 20;
    static const unsigned int FMK_MAX_INSTRS = 99;
    static const unsigned int FMK_MAX_PATTERNS = 64;
    static const unsigned int FMK_MAX_ROWS = 64;
    static const unsigned int FMK_INST_REGS = 11;
    static const unsigned int FMK_MAX_CH = 18;
    static const unsigned int FMK_REG_BANK = 256;

    struct fmk_instr_t {
        uint8_t regs[FMK_INST_REGS];  // 20h,23h,40h,43h,60h,63h,80h,83h,E0h,E3h,C0h
    };

    struct fmk_cell_t {
        uint8_t note;      // 0 = none, packed octave*16+semitone, 254 = cut
        uint8_t instr;
        uint8_t volume;    // OPL attenuation 0..63
        uint8_t cmd;       // effect command 1..22
        uint8_t infobyte;
        uint8_t flags;     // bit0 note, bit1 instr, bit2 vol, bit3 cmd
    };

    struct fmk_chan_t {
        uint8_t  opl_ch;
        uint8_t  is_4op;
        uint8_t  is_4op_main;
        uint8_t  four_op_pair;
        uint8_t  pan;
        uint8_t  active;

        uint8_t  cur_note;
        uint8_t  cur_instr;   // 1-based, 0 = none
        uint8_t  cur_vol;
        uint8_t  vol_set;
        uint8_t  cur_cmd;
        uint8_t  cur_info;
        uint8_t  note_on;
        uint8_t  cut;

        uint8_t  vol_slide;
        uint8_t  vib_speed, vib_depth, vib_phase;
        uint8_t  trem_speed, trem_depth, trem_phase;
        uint8_t  arp_x, arp_y, arp_on;
        uint8_t  port_speed;
        int16_t  port_target;
        uint8_t  retrig_speed;
        uint8_t  note_slide_dir;
        uint8_t  note_slide_speed;
        uint8_t  note_slide_fine;
        uint16_t note_slide_acc;
        uint8_t  mod_vol, car_vol;
        uint8_t  wave_sel;
        uint8_t  mod_params[8];
        uint8_t  car_params[8];

        uint16_t cur_fnum;
        uint8_t  cur_block;

        uint8_t  inst_regs[FMK_INST_REGS];
    };

private:
    // ----- loaded file image -----
    uint8_t  *m_data;
    size_t    m_size;

    char      m_song_name[29];
    char      m_composer[29];

    uint8_t   m_file_type;
    uint8_t   m_global;       // bit0 stereo,1 opl3,2 rhythm,3 4.8db trem,4 14cent vib
    uint8_t   m_base_speed;   // ticks/second (50)
    uint8_t   m_init_speed;
    uint8_t   m_order_len;
    uint8_t   m_num_instrs;
    uint8_t   m_num_patterns;
    uint8_t   m_num_tracks;

    uint8_t   m_track_pan[FMK_MAX_TRACKS];
    uint8_t   m_track_type[FMK_MAX_TRACKS];
    uint8_t   m_track_opl[FMK_MAX_TRACKS];
    uint8_t   m_order[255];
    uint32_t  m_pat_ptr[FMK_MAX_PATTERNS];
    uint16_t  m_instr_ptr[FMK_MAX_INSTRS];

    fmk_instr_t m_instrs[FMK_MAX_INSTRS];

    // ----- replayer state -----
    uint8_t   m_regs[2 * FMK_REG_BANK];

    uint8_t   m_pos_order;
    uint8_t   m_cur_pattern;
    uint8_t   m_row;
    uint8_t   m_tick;
    uint8_t   m_speed;
    uint8_t   m_opl3;
    uint8_t   m_stereo;
    uint8_t   m_rhythm;
    int       m_song_end;
    int       m_cur_chip;     // last bank selected via setchip(), -1 = unknown

    fmk_chan_t m_ch[FMK_MAX_CH];
    fmk_cell_t m_cells[FMK_MAX_CH];

    uint8_t   m_pat_cache[FMK_MAX_PATTERNS][FMK_MAX_ROWS][FMK_MAX_CH * 6];
    uint8_t   m_pat_loaded[FMK_MAX_PATTERNS];

    // ----- helpers -----
    bool parse();
    void opl_init();
    void oplwrite(uint16_t reg, uint8_t val);

    int  track_to_chan(int track, uint8_t *opl_ch, uint8_t *is4op, uint8_t *is4op_main);
    void write_op_reg_ch(uint8_t ch, uint8_t slot, uint8_t base, uint8_t val);
    uint8_t read_op_reg_ch(uint8_t ch, uint8_t slot, uint8_t base);
    void write_op_reg(uint8_t opl_ch, uint8_t op, uint8_t base, uint8_t val);
    uint8_t read_op_reg(uint8_t opl_ch, uint8_t op, uint8_t base);
    void write_chan_reg(uint8_t opl_ch, uint8_t base, uint8_t val);
    void apply_instrument(int ci, uint8_t inst_idx);
    void write_freq(int ci, uint16_t fnum, uint8_t block, uint8_t keyon);
    void set_freq_key(int ci, uint8_t keyon);
    void apply_volume(int ci);
    void decompress_pattern(uint8_t pat);
    void load_row_cells();
    void handle_row();
    void handle_tick_effects();
    void handle_effect(int ci, uint8_t cmd, uint8_t info, uint8_t rowtick);
    void next_row();
};

#endif
