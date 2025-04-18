#pragma once

#define MAX_XM_INS_SAMPLES	128
#define MAX_XM_INSTRUMENTS	128
#define MAX_XM_PATTERNS		256

struct xm_note
{
    uint8_t   	n;
    uint8_t   	inst;
    uint8_t   	vol;
    uint8_t   	fx;
    uint8_t   	par;
    uint8_t   	n1;
    uint8_t   	n2;
    uint8_t   	n3;
};

struct xm_pattern //not binary compatible with the pattern struct in the XM file
{
    int  	rows;
    int  	data_size;
    int  	channels;
    xm_note*	pattern_data;
};

struct xm_sample
{
    uint32_t    length; //number of bytes
    uint	reppnt;
    uint	replen;
    uint8_t    	volume;
    int8_t	finetune;

    uint8_t	type;
    //  0x01 - loop;
    //  0x02 - ping-pong loop;
    //  0x10 - 16-bit;
    //  0x20 - stereo (non-standard extension; non-interleaved);
    //PsyTexx (obsolete extension):
    //  0x20 - 32-bit;
    //  0x40 - stereo (interleaved LR LR LR...);

    uint8_t	panning;
    int8_t	relative_note;
    uint8_t	reserved2;
    uint8_t	name[ 22 ];
    int16_t* 	data;
};

#define EXT_INST_BYTES	    3
#define INST_FLAG_NOTE_OFF  1	//Note dead = note off

struct xm_instrument
{
    //Offset in XM file is 0:
    uint	instrument_size;
    uint8_t   	name[ 22 ];
    uint8_t   	type;
    uint16_t  	samples_num;

    //Offset in XM file is 29:
    //>>> Standard info block:
    uint	sample_header_size;
    uint8_t   	sample_number[ 96 ];
    uint16_t  	volume_points[ 24 ];
    uint16_t  	panning_points[ 24 ];
    uint8_t   	volume_points_num;
    uint8_t   	panning_points_num;
    uint8_t   	vol_sustain;
    uint8_t   	vol_loop_start;
    uint8_t   	vol_loop_end;
    uint8_t   	pan_sustain;
    uint8_t   	pan_loop_start;
    uint8_t   	pan_loop_end;
    uint8_t   	volume_type;
    uint8_t   	panning_type;
    uint8_t   	vibrato_type;
    uint8_t   	vibrato_sweep;
    uint8_t   	vibrato_depth;
    uint8_t   	vibrato_rate;
    uint16_t  	volume_fadeout;
    //>>> End of standard info block. Size of this block is 212 bytes

    //Offset in XM file is 241:
    //Additional 2 bytes (empty in standard XM file):
    uint8_t     volume;	    	//[for PsyTexx only]
    int8_t	finetune;	//[for PsyTexx only]

    //Offset in XM file is 243:
    //EXT_INST_BYTES starts here:
    uint8_t   	panning;	//[for PsyTexx only]
    int8_t	relative_note; 	//[for PsyTexx only]
    uint8_t	flags;		//[for PsyTexx only]

    xm_sample*	samples[ MAX_XM_INS_SAMPLES ];
};

#define SONG_TYPE_XM	0
#define SONG_TYPE_MOD	1

struct xm_song
{
    uint8_t	id_text[ 17 ];
    uint8_t   	name[ 20 ];
    uint8_t   	reserved1;
    uint8_t   	tracker_name[ 20 ];
    uint16_t  	version;
    uint32_t   	header_size;
    uint16_t  	length;
    uint16_t  	restart_position;
    uint16_t  	channels;
    uint16_t  	patterns_num;
    uint16_t  	instruments_num;
    uint16_t  	freq_table;
    uint16_t  	tempo;
    uint16_t  	bpm;
    uint8_t   	patterntable[ MAX_XM_PATTERNS ];

    xm_pattern* patterns[ MAX_XM_PATTERNS ];
    xm_instrument* instruments[ MAX_XM_INSTRUMENTS ];

    int		type; //SONG_TYPE_*
};

void xm_remove_song( xm_song* song );
xm_song* xm_new_song( void );
xm_song* xm_load_song_from_fd( sfs_file f );
xm_song* xm_load_song( const char* filename );

void xm_new_instrument( uint16_t num, const char* name, uint16_t samples, xm_song* song );
void xm_remove_instrument( uint16_t num, xm_song* song );
void xm_save_instrument( uint16_t num, const char* filename, xm_song* song );

void xm_new_pattern( uint16_t num, uint16_t rows, uint16_t channels, xm_song* song );
void xm_remove_pattern( uint16_t num, xm_song* song );

void xm_new_sample( uint16_t num, uint16_t ins_num, const char* name, int length, int type, xm_song* song );
void xm_remove_sample( uint16_t num, uint16_t ins_num, xm_song* song );
void xm_bytes2frames( xm_sample* smp, xm_song* song ); //Convert sample length from bytes to frames
void xm_frames2bytes( xm_sample* smp, xm_song* song ); //Convert sample length from frames to bytes (before XM saving)
