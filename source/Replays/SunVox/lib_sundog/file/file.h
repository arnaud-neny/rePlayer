#pragma once

/*
Naming Conventions
Filesystem root:
    /FILE		- FILE at the root of the file system (all OS except Windows);
    X:/FILE		- FILE on disk X (Windows only); examples: A:/file; C:/file;
File system packed in a file:
    vfsX:/FILE		- FILE on a packed file system; where the X - filesystem descriptor (supported formats: TAR);
Virtual disks (quick access to standard app directories):
    1:/FILE		- FILE in the current working directory (app documents, backups, last sessions, etc.):
			    * Linux and Windows: current working directory of the calling process;
				this is the folder you are in before launching the app;
				if you launch the app from the desktop menu in Linux, the current working directory is /home/username
			    * iOS: local storage for app docs;
			    * macOS: the same directory where the app bundle is located OR the path to an isolated sandbox;
				read more: https://www.warmplace.ru/forum/viewtopic.php?f=3&t=4399
			    * Android: local storage for app docs - primary shared storage (internal memory);
			    * WinCE: filesystem root (/);
			    * if 1:/ is write protected, then 2:/ will be used instead;
    2:/FILE		- FILE in the app/user directory for configs, templates, caches and some hidden data:
			    * Linux: /home/username/.config/appname;
			    * Windows and WinCE: directory for application-specific data (/Documents and Settings/username/Application Data);
		    	    * iOS: application support files;
		    	    * macOS: application support files (/Users/username/Library/Application Support/appname);
			    * Android: internal files directory;
    3:/FILE		- FILE in the temporary directory;
    4,5,6,7...          - additional virtual disks (system dependent) from ADD_VIRT_DISK to 9;
Reserved characters (don't use it in the file name):
    <>:"/\|?*
*/

//This library assumes that the paths to disks and working folders are the same within a single process

#if defined(OS_WIN) || defined(OS_WINCE)
// rePlayer:    #include <shlobj.h>
#endif

#ifdef OS_UNIX
    #include <dirent.h> //for file find
#endif

//
// Local system disks and working directories
//

#ifdef OS_WIN
    #define MAX_DISKS	128
#else
    #define MAX_DISKS	16
#endif
#define DISKNAME_SIZE	4
#define MAX_DIR_LEN	2048 //Don't use it! Must be deleted in future updates!
#define ADD_VIRT_DISK   4

void sfs_refresh_disks(); //get info about local disks
const char* sfs_get_disk_name( uint n ); //get disk name (for example: "C:/", "H:/", "/")
int sfs_get_disk_num( const char* path );
uint sfs_get_current_disk(); //get number of the current disk
uint sfs_get_disk_count();
const char* sfs_get_work_path(); //get current working directory (example: "C:/mydir/"); virtual disk 1:/
const char* sfs_get_conf_path(); //get config directory; virtual disk 2:/
const char* sfs_get_temp_path(); //virtual disk 3:/

//
// Main functions
//

#define SFS_MAX_DESCRIPTORS	256
#define SFS_FOPEN_MAX		( SFS_MAX_DESCRIPTORS - 3 )

//Std files:
#define SFS_STDIN		( SFS_MAX_DESCRIPTORS - 0 )
#define SFS_STDOUT		( SFS_MAX_DESCRIPTORS - 1 )
#define SFS_STDERR		( SFS_MAX_DESCRIPTORS - 2 )
#define SFS_IS_STD_STREAM( f )	( f >= SFS_STDERR )

//Seek access:
#define SFS_SEEK_SET            0
#define SFS_SEEK_CUR            1
#define SFS_SEEK_END            2

enum sfs_fd_type
{
    SFS_FILE_NORMAL,
    SFS_FILE_IN_MEMORY,
};

//File format ID:
enum sfs_file_fmt
{
    SFS_FILE_FMT_UNKNOWN = 0,
    //Audio:
    SFS_FILE_FMT_WAVE,
    SFS_FILE_FMT_AIFF,
    SFS_FILE_FMT_OGG, 	//currently only Vorbis is supported; for other codecs: use some additional options in the decoder/encoder structures
    SFS_FILE_FMT_MP3,
    SFS_FILE_FMT_FLAC,
    SFS_FILE_FMT_MIDI,
    SFS_FILE_FMT_SUNVOX,
    SFS_FILE_FMT_SUNVOXMODULE,
    SFS_FILE_FMT_XM,
    SFS_FILE_FMT_MOD,
    //Image:
    SFS_FILE_FMT_JPEG,
    SFS_FILE_FMT_PNG,
    SFS_FILE_FMT_GIF,
    //Video:
    SFS_FILE_FMT_AVI,
    SFS_FILE_FMT_MP4,
    //Other:
    SFS_FILE_FMT_ZIP,
    SFS_FILE_FMT_PIXICONTAINER,
    SFS_FILE_FMTS,
};

struct sfs_fd_struct
{
    char*	    	filename;
    void*	    	f;
    sundog_engine*	sd;
    sfs_fd_type    	type;
    int8_t*	    	virt_file_data;
    bool		virt_file_data_autofree;
    size_t	    	virt_file_ptr;
    size_t	    	virt_file_size;
    size_t		user_data; //Some user-defined parameter
};

typedef uint sfs_file;

int sfs_global_init();
int sfs_global_deinit();
//expand/shring virtual disk name (e.g. expand: 1:/file -> /home/user/file; shrink: /home/user/file -> 1:/file):
char* sfs_make_filename( sundog_engine* sd, const char* filename, bool expand ); //smem_free() required!
uint16_t* sfs_convert_filename_to_win_utf16( char* src ); //for Windows only; retval = malloc()
char* sfs_get_filename_path( const char* filename ); //smem_free() required! retval example: 1:/dir1/dir2/
const char* sfs_get_filename_without_dir( const char* filename );
const char* sfs_get_filename_extension( const char* filename );
sfs_fd_type sfs_get_type( sfs_file f );
void* sfs_get_data( sfs_file f );
size_t sfs_get_data_size( sfs_file f );
void sfs_set_user_data( sfs_file f, size_t user_data );
size_t sfs_get_user_data( sfs_file f );
sfs_file sfs_open_in_memory( sundog_engine* sd, void* data, size_t size );
inline sfs_file sfs_open_in_memory( void* data, size_t size ) { return sfs_open_in_memory( nullptr, data, size ); }
sfs_file sfs_open( sundog_engine* sd, const char* filename, const char* filemode );
inline sfs_file sfs_open( const char* filename, const char* filemode ) { return sfs_open( nullptr, filename, filemode ); }
int sfs_close( sfs_file f );
void sfs_rewind( sfs_file f );
int sfs_getc( sfs_file f );
int64_t sfs_tell( sfs_file f );
int sfs_seek( sfs_file f, int64_t offset, int access ); //retval: 0 - success; other values - error;
int sfs_eof( sfs_file f );
int sfs_flush( sfs_file f );
size_t sfs_read( void* ptr, size_t el_size, size_t elements, sfs_file f ); //Return value: total number of elements (NOT bytes!) successfully read
size_t sfs_write( const void* ptr, size_t el_size, size_t elements, sfs_file f ); //Return value: total number of elements (NOT bytes!) successfully written
int sfs_write_varlen_uint32( uint32_t v, sfs_file f ); //Return value: positive length of the written number; in case of error: 0 or negative parial number of bytes written
uint32_t sfs_read_varlen_uint32( int* len, sfs_file f ); //len (optional) - number of bytes successfully read or 0 in case of error
int sfs_putc( int val, sfs_file f );
int sfs_remove( const char* filename ); //remove a file or directory
int sfs_remove_file( const char* filename );
int sfs_rename( const char* old_name, const char* new_name );
int sfs_mkdir( const char* pathname, uint mode );
int64_t sfs_get_file_size( const char* filename, int* err );
inline int64_t sfs_get_file_size( const char* filename ) { int err; return sfs_get_file_size( filename, &err ); }
inline bool sfs_file_exists( const char* filename ) { int err; sfs_get_file_size( filename, &err ); return err == 0 ? 1 : 0; }
int sfs_copy_file( const char* dest, const char* src );
int sfs_copy_files( const char* dest, const char* src, const char* mask, const char* with_str_in_filename, bool move ); //Return value: number of files copied/moved; dest/src: "somedir/"; mask: "xm/mod/it" or NULL
void sfs_remove_support_files( const char* prefix ); //Remove the app support files from the 2:/ ; prefix example: ".sunvox_"

//
// Searching files
//

//type in sfs_find_struct:
enum sfs_find_item_type
{
    SFS_FILE = 0,
    SFS_DIR
};

#define SFS_FIND_OPT_FILESIZE	( 1 << 0 )

struct sfs_find_struct
{
    uint32_t opt; //options
    const char* start_dir; //Example: "c:/mydir/" "d:/"
    const char* mask; //Example: "xm/mod/it" (or NULL for all files)

    char name[ MAX_DIR_LEN ]; //Found file name
    char temp_name[ MAX_DIR_LEN ];
    sfs_find_item_type type; //Found file type
    size_t size;

#if defined(OS_WIN)
    WIN32_FIND_DATAW find_data;
#endif
#if defined(OS_WINCE)
    WIN32_FIND_DATA find_data;
#endif
#if defined(OS_WIN) | defined(OS_WINCE)
    HANDLE find_handle;
    char win_mask[ MAX_DIR_LEN ]; //Example: "*.xm *.mod *.it"
    char* win_start_dir; //Example: "mydir\*.xm"
#endif
#ifdef OS_UNIX
    DIR* dir;
    struct dirent* current_file;
    char new_start_dir[ MAX_DIR_LEN ];
#endif
};

int sfs_find_first( sfs_find_struct* ); //Return values: 0 - no files
int sfs_find_next( sfs_find_struct* ); //Return values: 0 - no files
void sfs_find_close( sfs_find_struct* );

//
// File format
//

sfs_file_fmt sfs_get_file_format( const char* filename, sfs_file f ); //get file format ID
const char* sfs_get_mime_type( sfs_file_fmt fmt );
const char* sfs_get_extension( sfs_file_fmt fmt );
int sfs_get_clipboard_type( sfs_file_fmt fmt );

//
// Helper functions for reading and writing various file formats
//

struct simage_desc; //misc.h

#define LOAD_JPEG_FLAG_AUTOROTATE	1

enum sfs_save_jpeg_subsampling
{
    JE_Y_ONLY, //Y (grayscale) only
    JE_H1V1, //YCbCr, no subsampling (H1V1, YCbCr 1x1x1, 3 blocks per MCU)
    JE_H2V1, //YCbCr, H2V1 subsampling (YCbCr 2x1x1, 4 blocks per MCU)
    JE_H2V2, //YCbCr, H2V2 subsampling (YCbCr 4x1x1, 6 blocks per MCU, very common)
};

class sfs_save_jpeg_pars : public smem_base
{
public:
    int				quality; //1-100, higher is better
    sfs_save_jpeg_subsampling	subsampling;
    bool			two_pass_flag;
    sfs_save_jpeg_pars()
    {
	quality = 85;
	subsampling = JE_H2V2;
	two_pass_flag = 0;
    };
};

enum sfs_sample_format
{
    SFMT_INT8,
    SFMT_INT16, //lsb
    SFMT_INT24, //lsb: v = buf[ i3 + 0 ] | ( buf[ i3 + 1 ] << 8 ) | ( buf[ i3 + 2 ] << 16 ); if( v & 0x800000 ) v |= 0xFF000000;
    SFMT_INT32, //lsb
    SFMT_FLOAT32,
    SFMT_FLOAT64,
    SFMT_MAX,
};
extern const int g_sfs_sample_format_sizes[ SFMT_MAX ];

struct sfs_sound_decoder_data;
typedef int (*sfs_sound_decoder_fmt_init_t)( sfs_sound_decoder_data* ); //retval: 0 = success;
typedef void (*sfs_sound_decoder_fmt_deinit_t)( sfs_sound_decoder_data* );
typedef size_t (*sfs_sound_decoder_fmt_read_t)( sfs_sound_decoder_data*, void* dest_buf, size_t len );
typedef int (*sfs_sound_decoder_fmt_seek_t)( sfs_sound_decoder_data*, size_t frame ); //retval: 0 = success;
typedef int64_t (*sfs_sound_decoder_fmt_tell_t)( sfs_sound_decoder_data* );
#define SFS_SDEC_CONVERT_INT24_TO_FLOAT32		( 1 << 0 )
#define SFS_SDEC_CONVERT_INT32_TO_FLOAT32		( 1 << 1 )
#define SFS_SDEC_CONVERT_FLOAT64_TO_FLOAT32		( 1 << 2 )
struct sfs_sound_decoder_data
{
    uint32_t		flags; //SFS_SDEC_*
    char*		filename; //cloned
    sfs_file 		f; //input stream
    bool		initialized;
    bool		f_close_req;
    sundog_engine*	sd;
    sfs_file_fmt	file_format;
    sfs_sample_format 	sample_format;
    sfs_sample_format 	sample_format2; //converted format (if SFS_SDEC_CONVERT_* flags specified)
    int			sample_size; //in bytes
    int			sample_size2; //converted
    int			frame_size; //in bytes
    int			frame_size2; //converted
    int                 rate; //in Hz
    int                 channels;
    size_t              len; //number of frames (frame = sample_size * channels)
    uint8_t		loop_type; //0 - none; 1 - normal; 2 - bidirectional;
    size_t		loop_start; //in frames
    size_t		loop_len; //in frames; 0 - no loop;
    void*		tmp_buf; //for SFS_SDEC_CONVERT_*
    sfs_sound_decoder_fmt_init_t	init;
    sfs_sound_decoder_fmt_deinit_t	deinit;
    sfs_sound_decoder_fmt_read_t	read;
    sfs_sound_decoder_fmt_seek_t	seek;
    sfs_sound_decoder_fmt_tell_t	tell;
    void*		format_decoder_data;
};

struct sfs_sound_encoder_data;
typedef int (*sfs_sound_encoder_fmt_init_t)( sfs_sound_encoder_data* ); //retval: 0 = success;
typedef void (*sfs_sound_encoder_fmt_deinit_t)( sfs_sound_encoder_data* );
typedef size_t (*sfs_sound_encoder_fmt_write_t)( sfs_sound_encoder_data*, void* src_buf, size_t len );
struct sfs_sound_encoder_data
{
    char*		filename; //cloned
    sfs_file 		f; //input stream
    bool		initialized;
    bool		f_close_req;
    sundog_engine*	sd;
    sfs_file_fmt	file_format; //SFS_FILE_FMT_UNKNOWN = RAW
    sfs_sample_format 	sample_format;
    int			sample_size; //in bytes
    int			frame_size; //in bytes
    int                 rate; //in Hz
    int                 channels;
    size_t              len; //number of frames (frame = sample_size * channels); may be != final length
    uint8_t		loop_type; //0 - none; 1 - normal; 2 - bidirectional;
    size_t		loop_start; //in frames
    size_t		loop_len; //in frames; 0 - no loop;
    uint8_t		compression_level; //0 = AUTO; FLAC: 1..9; OGG (Vorbis): 1..100;
    sfs_sound_encoder_fmt_init_t	init;
    sfs_sound_encoder_fmt_deinit_t	deinit;
    sfs_sound_encoder_fmt_write_t	write;
    void*		format_encoder_data;
};

int sfs_load_jpeg( sundog_engine* sd, const char* filename, sfs_file f, simage_desc* img, uint32_t flags );
int sfs_save_jpeg( sundog_engine* sd, const char* filename, sfs_file f, simage_desc* img, sfs_save_jpeg_pars* pars );

int sfs_load_png( sundog_engine* sd, const char* filename, sfs_file f, simage_desc* img );

int sfs_sound_decoder_init( sundog_engine* sd, const char* filename, sfs_file f, sfs_file_fmt file_format, uint32_t flags, sfs_sound_decoder_data* d ); //flags: SFS_SDEC_*
void sfs_sound_decoder_deinit( sfs_sound_decoder_data* d );
size_t sfs_sound_decoder_read( sfs_sound_decoder_data* d, void* dest_buf, size_t len ); //retval: =0: EOF;  >0: actually read (may be < len);
size_t sfs_sound_decoder_read2( sfs_sound_decoder_data* d, void* dest_buf, size_t len ); //read until the file/buffer ends
int sfs_sound_decoder_seek( sfs_sound_decoder_data* d, size_t frame ); //retval: 0 = success;
int64_t sfs_sound_decoder_tell( sfs_sound_decoder_data* d ); //current position in frames

int sfs_sound_encoder_init(
    sundog_engine* sd, 
    const char* filename, sfs_file f,
    sfs_file_fmt file_format,
    sfs_sample_format sample_format,
    int rate, //Hz
    int channels,
    size_t len, //number of frames; final length may be different;
    uint32_t flags,
    sfs_sound_encoder_data* e );
void sfs_sound_encoder_deinit( sfs_sound_encoder_data* e );
size_t sfs_sound_encoder_write( sfs_sound_encoder_data* e, void* src_buf, size_t len ); //retval: =len if success;

//
// Other
//

#if defined(OS_APPLE)
int apple_sfs_global_init();
int apple_sfs_global_deinit();
#endif
