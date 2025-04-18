#pragma once

struct midi_track
{
    uint8_t* data;
};

struct midi_file
{
    uint16_t format;
    uint16_t tracks_num;
    uint16_t stime_div;
    uint16_t ticks_per_quarter_note;
    float fps;
    int ticks_per_frame;
    midi_track** tracks;
};

void midi_file_remove( midi_file* mf );
midi_file* midi_file_new( void );
void midi_track_remove( midi_track* mf );
midi_track* midi_track_new( void );
midi_file* midi_file_load_from_fd( sfs_file f );
midi_file* midi_file_load( const char* filename );
