#include <stdint.h>
#include <string.h>

/*
    C implementation of a Fuxoft player taken from https://github.com/Dovgalyuk/ArduinoFXMPlayer

    Author (probably): Pavel Dovgalyuk
    License: unknown

    note: I patched the original impl to just return the soundchip register settings
    so that Ayumi can take care of the rest. I also added "Amadeus" (ZXAYAMAD) file format
    handling (which seem to be FXM with some Amiga specific wrapper).
*/
#define EMSCRIPTEN

#ifdef EMSCRIPTEN

// just some additions to leave the original AVR impl largely untouched

#define PROGMEM
#define false 0
#define true 1
#define bool uint8_t

static uint8_t pgm_read_byte(const uint8_t *buf)
{
    return buf[0];
}
static uint16_t pgm_read_word(const uint16_t *buf)
{
    return buf[0];
}

static int ayregs_out[14];	// 	the 2 "I/O Port x Data Store" registers are irrelevant here

static void  send_data(uint8_t reg, uint8_t value) {
    ayregs_out[reg]= value;
}

int* fxm_get_registers()
{
    return ayregs_out;	// use 16-bit to match existing Ayumi impl
}
#endif


static const uint8_t *ram;
static uint16_t ram_addr;

static uint8_t peek(uint16_t addr)
{
    return pgm_read_byte(ram + addr - ram_addr);
}

static uint16_t peek2(uint16_t addr)
{
  return peek(addr) + (peek(addr + 1) << 8);
}

static uint8_t ayregs[14];

static void z80_write_ay_reg(uint8_t reg, uint8_t val)
{
  ayregs[reg] = val;
}

static const uint16_t FXM_Table[] PROGMEM = {
  0xfbf, 0xedc, 0xe07, 0xd3d, 0xc7f, 0xbcc, 0xb22, 0xa82, 0x9eb, 0x95d, 0x8d6, 0x857, 0x7df, 0x76e, 0x703,
  0x69f, 0x640, 0x5e6, 0x591, 0x541, 0x4f6, 0x4ae, 0x46b, 0x42c, 0x3f0, 0x3b7, 0x382, 0x34f, 0x320, 0x2f3,
  0x2c8, 0x2a1, 0x27b, 0x257, 0x236, 0x216, 0x1f8, 0x1dc, 0x1c1, 0x1a8, 0x190, 0x179, 0x164, 0x150, 0x13d,
  0x12c, 0x11b, 0x10b, 0xfc, 0xee, 0xe0, 0xd4, 0xc8, 0xbd, 0xb2, 0xa8, 0x9f, 0x96, 0x8d, 0x85, 0x7e, 0x77, 0x70,
  0x6a, 0x64, 0x5e, 0x59, 0x54, 0x4f, 0x4b, 0x47, 0x43, 0x3f, 0x3b, 0x38, 0x35, 0x32, 0x2f, 0x2d, 0x2a, 0x28, 0x25,
  0x23, 0x21
};

struct FXMChannel
{
  uint8_t id;
  uint16_t address_in_pattern;
  uint16_t point_in_ornament;
  uint16_t ornament_pointer;
  uint16_t point_in_sample;
  uint16_t sample_pointer;
  uint8_t volume;
  uint8_t sample_tick_counter;
  uint8_t mixer;
  int16_t note_skip_counter;
  uint16_t tone;
  uint8_t note;
  uint8_t transposit;

  bool b0e, b1e, b2e, b3e;

  uint16_t stack[16];
  uint8_t sp;
};

static uint8_t noise;
static struct FXMChannel channels[3];
int isAmad = 0;


#ifdef EMSCRIPTEN
uint8_t read_uint8(uint8_t**in, uint32_t *available) {
    if (!(*available)) return 0;

    (*available) -= 1;

    uint8_t result= (*in)[0];

    (*in) += 1;	// advance as if data was read from a file
    return result;
}

uint16_t read_uint16(uint8_t**in, uint32_t *available) {
    uint32_t result= read_uint8(in, available);
    result = (result<<8)+ read_uint8(in, available);
    return result;
}
int16_t read_int16(uint8_t**in, uint32_t *available) {
    uint16_t u= read_uint16(in, available);
    int16_t s= *((int16_t*)(&u));
    return s;
}

int match(uint8_t**in, uint32_t *available, uint8_t *pattern) {
    for (int i= 0; i<strlen(pattern); i++) {
        if (read_uint8(in, available) != pattern[i]) {
            return 0;
        }
    }
    return 1;
}

static const char* _song_name= 0;
static const char* _song_author= 0;

int fxm_load(const uint8_t *fxm, uint32_t len) {
  uint8_t *pos= (uint8_t *)fxm;	// tmp work position

  uint32_t available;
  if (!match(&pos, &len, (uint8_t *)"FXSM"))
    return 0;

  ram = pos + 2;	// point to "data block" of song
  ram_addr = pgm_read_byte(pos + 0) + (pgm_read_byte(pos + 1) << 8);

  _song_name = _song_author = 0;

  return 1;
}

void fxm_get_songinfo(const char** songName, const char **songAuthor) {
    *songName = _song_name;
    *songAuthor = _song_author;
}

int amad_load(const uint8_t *fxm, uint32_t len) {
  uint8_t *pos= (uint8_t *)fxm;	// tmp work position

  if (!match(&pos, &len, (uint8_t *)"ZXAYAMAD"))
    return 0;

    // 20 byte header start --
        // note: key "feature" is that below "pointers" are signed and refer to the addr that they were loaded from

    uint8_t aym_fileversion= read_uint8(&pos, &len);	// FileVersion
    uint8_t aym_playerversion= read_uint8(&pos, &len);	// PlayerVersion
    uint16_t aym_internal= read_uint16(&pos, &len);		// PSpecialPlayer (can be ignored)

    int16_t aym_creator_ptr= read_int16(&pos, &len);	// PAuthor
    uint8_t *creator = pos + aym_creator_ptr-2;			// relative to before reading the 2 bytes..

    int16_t aym_miscinfo_ptr= read_int16(&pos, &len);	// other info (0 means unused)
    uint8_t *misc_info= "";
    if (aym_miscinfo_ptr) misc_info= pos + aym_miscinfo_ptr-2;

    _song_author= (const char*)creator;

    uint8_t aym_songs= read_uint8(&pos, &len)+1;	// Number of tunes in file decreased by 1 (0 means one song:-)
    uint8_t aym_firstsong= read_uint8(&pos, &len);	//Tune number, which must be played first, decreased by 1 too


    int16_t aym_songtab_ptr= read_int16(&pos, &len);	// Relative pointer to 1st “Song structure” record
    // -- header end

    // "song structure" array with 4 bytes per song (i.e. for multi-track songs
    // there are more entries - except that only playback if 1st song is implemented here .. KNOWN LIMITATION!)
    uint32_t slen= 4;
    uint8_t *song_struct= pos + aym_songtab_ptr-2;

    int16_t aym_songname_ptr= read_int16(&song_struct, &slen);
    uint8_t *song= song_struct + aym_songname_ptr-2;

    _song_name= (const char*)song;

    int16_t aym_songdata_ptr= read_int16(&song_struct, &slen);
        // handle the actual AMAD song data..
    uint8_t *songdata= song_struct + aym_songdata_ptr-2;

    uint32_t data_len;
    if (aym_songs > 1) {
        // presume the following song starts right after this one
        slen= 4;
        int16_t aym_songname2_ptr= read_int16(&song_struct, &slen);
        song= song_struct + aym_songname2_ptr-2;

        int16_t aym_songdata2_ptr= read_int16(&song_struct, &slen);
        uint8_t *songdata2= song_struct + aym_songdata2_ptr-2;

        data_len= songdata2 - songdata;
    } else {
        // presume the remaining file is all songdata
        data_len= len - (songdata-pos);
    }

    // read the record "Song data"
    uint16_t song_allocaddress= read_uint16(&songdata, &data_len);	// Allocation address of data block in Spectrum memory.
    uint8_t song_andsix= read_uint8(&songdata, &data_len);	// The parameter must either 31 or 15.
                                                            // It is used for correction result of
                                                            // addition current noise value and parameter
                                                            // of 8D command (see FXM description). I.e.
                                                            // some players use 5 bits of noise register, and
                                                            // some only 4 ones.

    uint8_t song_loops= read_uint8(&songdata, &data_len);		// Number of loops.
    uint16_t song_looplen= read_uint16(&songdata, &data_len);	// Length of one loop in interrupts (VBI).
    uint16_t song_fadeoffset= read_uint16(&songdata, &data_len);	// Precise fade specification (unused in Ay_Emul)
    uint16_t song_fadelen= read_uint16(&songdata, &data_len);		// How long to fade (unused in Ay_Emul)

    // irrelevant Amiga stuff
    uint8_t song_achan= read_uint8(&songdata, &data_len);	// Amiga's channel number for emulating A AY channel.
    uint8_t song_bchan= read_uint8(&songdata, &data_len);	// Amiga's channel number for emulating B AY channel.
    uint8_t song_cchan= read_uint8(&songdata, &data_len);	// Amiga's channel number for emulating C AY channel.
    uint8_t song_noise= read_uint8(&songdata, &data_len);	// Amiga's channel number for emulating AY noise.

    uint8_t *song_zxdata= 	songdata;
    uint16_t song_zxdatalen= data_len;


    // configure FXM using the retrieved info
    ram = song_zxdata;
    ram_addr = song_allocaddress;	// XXX might need to be flipped?

    isAmad = 1;

    return 1;
}

int fxm_init(const uint8_t *fxm, uint32_t len)
{
  memset((void*)channels, 0, sizeof(channels));

  for (int i = 0 ; i < 3 ; ++i)
  {
    channels[i].note_skip_counter = 1;
    channels[i].mixer = 8;
    channels[i].id = i;
  }

  if (fxm_load(fxm, len) || amad_load(fxm, len)) {/* all is well */}
  else return 0;

  channels[0].address_in_pattern = pgm_read_byte(ram + 0) + (pgm_read_byte(ram + 1) << 8);
  channels[1].address_in_pattern = pgm_read_byte(ram + 2) + (pgm_read_byte(ram + 3) << 8);
  channels[2].address_in_pattern = pgm_read_byte(ram + 4) + (pgm_read_byte(ram + 5) << 8);

/*
  int clock_rate= 1773400;
  int frame_count= 30000;
  double frame_rate= 50.;
  set_song_params(clock_rate, frame_count, frame_rate);
*/
  return 1;
}
#else
void fxm_init(const uint8_t *fxm)
{
  if (pgm_read_byte(fxm) != 'F'
    || pgm_read_byte(fxm + 1) != 'X'
    || pgm_read_byte(fxm + 2) != 'S'
    || pgm_read_byte(fxm + 3) != 'M')
    return;

  ram = fxm + 6;
  ram_addr = pgm_read_byte(fxm + 4) + (pgm_read_byte(fxm + 5) << 8);

  for (int i = 0 ; i < 3 ; ++i)
  {
    channels[i].note_skip_counter = 1;
    channels[i].mixer = 8;
    channels[i].id = i;
  }

  channels[0].address_in_pattern = pgm_read_byte(fxm + 6) + (pgm_read_byte(fxm + 7) << 8);
  channels[1].address_in_pattern = pgm_read_byte(fxm + 8) + (pgm_read_byte(fxm + 9) << 8);
  channels[2].address_in_pattern = pgm_read_byte(fxm + 10) + (pgm_read_byte(fxm + 11) << 8);

  // init hardware YM
  set_ym_clock();
  set_bus_ctl();
}
#endif
static void fxm_play_channel(struct FXMChannel *ch)
{
  if (!--ch->note_skip_counter)
  {
    while (true)
    {
      uint8_t v = peek(ch->address_in_pattern++);
      switch (v)
      {
      case 0x80:
        // jump
        ch->address_in_pattern = peek2(ch->address_in_pattern);
        break;
      case 0x81:
        // call
        ch->stack[ch->sp++] = ch->address_in_pattern + 2;
        ch->address_in_pattern = peek2(ch->address_in_pattern);
        break;
      case 0x82:
        // loop begin
        ch->stack[ch->sp++] = peek(ch->address_in_pattern++);
        ch->stack[ch->sp++] = ch->address_in_pattern;
        break;
      case 0x83:
        // loop
        if (--ch->stack[ch->sp - 2] & 0xff)
          ch->address_in_pattern = ch->stack[ch->sp - 1];
        else
          ch->sp -= 2;
        break;
      case 0x84:
        // set noise
        noise = peek(ch->address_in_pattern++);
        break;
      case 0x85:
        // set mixer
        ch->mixer = peek(ch->address_in_pattern++);
        break;
      case 0x86:
        // set ornament
        ch->ornament_pointer = peek2(ch->address_in_pattern);
        ch->address_in_pattern += 2;
        break;
      case 0x87:
        // set sample
        ch->sample_pointer = peek2(ch->address_in_pattern);
        ch->address_in_pattern += 2;
        break;
      case 0x88:
        // transposition
        ch->transposit = peek(ch->address_in_pattern++);
        break;
      case 0x89:
        // return
        ch->address_in_pattern = ch->stack[--ch->sp];
        break;
      case 0x8a:
        // don't restart sample at new note
        ch->b0e = true;
        ch->b1e = false;
        break;
      case 0x8b:
        // restart sample at new note
        ch->b0e = false;
        ch->b1e = false;
        break;
      case 0x8c:
        // not implemented code call
        ch->address_in_pattern += 2;
        break;
      case 0x8d:
        // add to noise
        noise = (noise + peek(ch->address_in_pattern++)) & 0x1f;
        break;
      case 0x8e:
        // add transposition
        ch->transposit += peek(ch->address_in_pattern++);
        break;
      case 0x8f:
        // push transposition
        ch->stack[ch->sp++] = ch->transposit;
        break;
      case 0x90:
        // pop transposition
        ch->transposit = ch->stack[--ch->sp];
        break;
      default:
        // note
        if (v <= 0x54)
        {
          if (v)
          {
            ch->note = v + ch->transposit;
            ch->tone = pgm_read_word(&FXM_Table[ch->note - 1]);
            ch->b3e = false;
          }
          else
          {
            ch->tone = 0;
          }
          ch->note_skip_counter = peek(ch->address_in_pattern++);
          ch->point_in_ornament = ch->ornament_pointer;
          ch->b2e = true;
          if (!ch->b1e)
          {
            ch->b1e = ch->b0e;
            ch->point_in_sample = ch->sample_pointer;
            ch->volume = peek(ch->point_in_sample++);
            ch->sample_tick_counter = peek(ch->point_in_sample++);
            goto ret;
          }
          else
          {
            goto decode_sample;
          }
          break;
        }
      }
    }
  }

decode_sample:
  if (!--ch->sample_tick_counter)
  {
    while (true)
    {
      uint8_t s = peek(ch->point_in_sample++);
      if (s == 0x80)
      {
        ch->point_in_sample = peek2(ch->point_in_sample);
        continue;
      }
      else if (s < 0x1e)
      {
        ch->volume = s;
        ch->sample_tick_counter = peek(ch->point_in_sample++);
      }
      else
      {
        s -= 0x32;
        ch->volume = s;
        ch->sample_tick_counter = 1;
      }
      break;
    }
  }
  // decode ornament
  if (ch->tone && !ch->b2e)
  {
    while (true)
    {
      uint8_t s = peek(ch->point_in_ornament++);
      switch (s)
      {
      case 0x80:
        ch->point_in_ornament = peek2(ch->point_in_ornament);
        break;
      case 0x82:
        ch->b3e = true;
        break;
      case 0x83:
        ch->b3e = false;
        break;
      case 0x84:
        ch->mixer ^= 9;
        break;
      default:
        if (!ch->b3e)
        {
          ch->tone += (int8_t)s;
        }
        else
        {
          ch->note += s;
          ch->tone = pgm_read_word(&FXM_Table[ch->note - 1]);
        }
        goto ret;
      }
    }
  }
ret:
  z80_write_ay_reg(6, noise);
  ch->b2e = 0;

  z80_write_ay_reg(ch->id + 8, ch->tone ? ch->volume & 0xf : 0);
  z80_write_ay_reg(2 * ch->id, ch->tone);
  z80_write_ay_reg(2 * ch->id + 1, ch->tone >> 8);
}

void fxm_loop()
{
  if (!ram)
    return;

  fxm_play_channel(&channels[0]);
  fxm_play_channel(&channels[1]);
  fxm_play_channel(&channels[2]);

  z80_write_ay_reg(7, (channels[2].mixer << 2) | (channels[1].mixer << 1) | channels[0].mixer);
  for (int i = 13 ; i >= 0 ; --i)
    send_data(i, ayregs[i]);
}

