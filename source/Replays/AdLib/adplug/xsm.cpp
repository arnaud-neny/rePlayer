/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2003 Simon Peter, <dn.tlp@gmx.net>, et al.
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
 * xsm.cpp - eXtra Simple Music Player, by Simon Peter <dn.tlp@gmx.net>
 *
 * eXtra Simple Music Player can be found in the All BASIC Code archive
 * package called FMLIB by Davey W. Taylor (SOUND.ABC, 1997).
 * The ABC archive can be obtained from https://github.com/qb40/abc-archive
 *
 * It is a simple (as the name implies ;) package to play FM music using QBasic.
 * There is no tracker included to produce XSM files, hence it is unlikely that
 * more XSM files other than the four included demo songs exist.
 * Davey W. Taylor later went on to write an FM Tracker in QBasic which uses an
 * incompatible file format.
 */

#include <string.h>

#include "xsm.h"

CxsmPlayer::CxsmPlayer(Copl *newopl)
  : CPlayer(newopl), music(0)
{
}

CxsmPlayer::~CxsmPlayer()
{
  if(music) delete [] music;
}

bool CxsmPlayer::load(const std::string &filename, const CFileProvider &fp)
{
  binistream *f = fp.open(filename); if(!f) return false;
  char			id[6];
  int			i, j;

  // check if header matches
  f->readString(id, 6); songlen = f->readInt(2);
  if(strncmp(id, "ofTAZ!", 6) || songlen > 3200) { fp.close(f); return false; }

  // read and set instruments
  for(i = 0; i < 9; i++) {
	for (j = 0; j < 11; j++) {
	  inst[i].value[j] = f->readInt(1);
	}
    f->ignore(5);
  }

  // read song data
  music = new char [songlen * 9];
  for(i = 0; i < 9; i++)
    for(j = 0; j < songlen; j++)
      music[j * 9 + i] = f->readInt(1);

  // success
  fp.close(f);
  rewind(0);
  return true;
}

bool CxsmPlayer::update()
{
  int c;

  // rePlayer begin: reset the songend as we want to get notified only once per loop
  songend = false;
  // rePlayer end

  if(notenum >= songlen) {
    songend = true;
    notenum = last = 0;
  }

  for(c = 0; c < 9; c++)
    if(music[notenum * 9 + c] != music[last * 9 + c])
      opl->write(0xb0 + c, 0);

  for(c = 0; c < 9; c++) {
    if(music[notenum * 9 + c])
      play_note(c, music[notenum * 9 + c] % 12, music[notenum * 9 + c] / 12);
    else
      play_note(c, 0, 0);
  }

  last = notenum;
  notenum++;
  return !songend;
}

void CxsmPlayer::rewind(int subsong)
{
  int i;
  notenum = last = 0;
  songend = false;
  opl->init(); opl->write(1, 32);
  for (i = 0; i < 9; i++) {
    opl->write(0x20 + op_table[i], inst[i].value[0]);
    opl->write(0x23 + op_table[i], inst[i].value[1]);
	opl->write(0x40 + op_table[i], inst[i].value[2]);
	opl->write(0x43 + op_table[i], inst[i].value[3]);
	opl->write(0x60 + op_table[i], inst[i].value[4]);
	opl->write(0x63 + op_table[i], inst[i].value[5]);
	opl->write(0x80 + op_table[i], inst[i].value[6]);
	opl->write(0x83 + op_table[i], inst[i].value[7]);
	opl->write(0xe0 + op_table[i], inst[i].value[8]);
	opl->write(0xe3 + op_table[i], inst[i].value[9]);
	opl->write(0xc0 + op_table[i], inst[i].value[10]);
  }
}

float CxsmPlayer::getrefresh()
{
  return 5.0f;
}

void CxsmPlayer::play_note(int c, int note, int octv)
{
  int freq = note_table[note];

  if(!note && !octv) freq = 0;
  opl->write(0xa0 + c, freq & 0xff);
  opl->write(0xb0 + c, (freq / 0xff) | 32 | (octv * 4));
}
