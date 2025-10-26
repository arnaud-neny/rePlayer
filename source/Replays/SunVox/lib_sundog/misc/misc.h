#pragma once

struct ssymtab;

int smisc_global_init();
int smisc_global_deinit();

//
// List of strings
//

struct slist_data
{
    char* heap; //Items data
    uint heap_usage; //Number of used bytes in heap
    uint* items; //Item pointers
    uint items_num; //Number of items
    //Public fields (can be changed directly):
    //...actually, the best place for these fields is the slist_handler (WM)...
    int selected_item;
    int start_item;
};

void slist_init( slist_data* data );
void slist_deinit( slist_data* data );
void slist_clear( slist_data* data );
void slist_reset_items( slist_data* data );
void slist_reset_selection( slist_data* data );
uint slist_get_items_num( slist_data* data );
int slist_add_item( const char* item, uint8_t attr, slist_data* data );
void slist_delete_item( uint item_num, slist_data* data );
void slist_move_item_up( uint item_num, slist_data* data );
void slist_move_item_down( uint item_num, slist_data* data );
void slist_move_item( uint from, uint to, slist_data* data );
char* slist_get_item( uint item_num, slist_data* data );
uint8_t slist_get_attr( uint item_num, slist_data* data );
void slist_sort( slist_data* data );
void slist_exchange( uint item1, uint item2, slist_data* data );
int slist_save( const char* fname, slist_data* data );
int slist_load( const char* fname, slist_data* data );

//
// Ring buffer
//

struct sring_buf
{
    smutex m;
    uint32_t flags;
    uint8_t* buf;
    size_t buf_size;
    std::atomic_size_t wp;
    std::atomic_size_t rp;
};

#define SRING_BUF_FLAG_SINGLE_RTHREAD	( 1 << 0 )
#define SRING_BUF_FLAG_SINGLE_WTHREAD	( 1 << 1 )
#define SRING_BUF_FLAG_ATOMIC_SPINLOCK	( 1 << 2 )

sring_buf* sring_buf_new( size_t size, uint32_t flags );
void sring_buf_delete( sring_buf* b );
void sring_buf_read_lock( sring_buf* b );
void sring_buf_read_unlock( sring_buf* b );
void sring_buf_write_lock( sring_buf* b );
void sring_buf_write_unlock( sring_buf* b );
size_t sring_buf_write( sring_buf* b, void* data, size_t size ); //Use with Lock+Unlock
size_t sring_buf_read( sring_buf* b, void* data, size_t size ); //Use with Lock+Unlock
void sring_buf_next( sring_buf* b, size_t size ); //Use with Lock+Unlock
size_t sring_buf_avail( sring_buf* b );

//
// Exchange box for large messages; thread-safe
//

struct smbox_msg
{
    const void* id;
    void* data; //ONLY allocated by smem_alloc(); will be freed by smbox_remove_msg();
    stime_ms_t created_t;
    int lifetime_s; //0 - no limit
};

struct smbox
{
    smutex mutex;
    smbox_msg** msg;
    int capacity; //max number of messages
    int active; //number of active messages
};

smbox* smbox_new();
void smbox_delete( smbox* mb );
smbox_msg* smbox_new_msg();
void smbox_remove_msg( smbox_msg* msg );
int smbox_add( smbox* mb, smbox_msg* msg );
smbox_msg* smbox_get( smbox* mb, const void* id, int timeout_ms );

//
// Configuration
// (based on text INI file with key-value pairs)
//

//default = option is missing (or commented out) in the config file;
#define APP_CFG_LANG		"locale_lang" //language code: en_US, ru_RU, etc.
#define APP_CFG_NO_CLOG		"no_clog" //disable logging to console; default = enable; any value = disable;
#define APP_CFG_NO_FLOG		"no_flog" //disable logging to file; default = enable; any value = disable;

struct sconfig_key
{
    char* key;
    char* value;
    int line_num;
    bool deleted;
};

struct sconfig_data
{
    int file_num;
    char* file_name;
    char* source;
    sconfig_key* keys;
    ssymtab* st;
    int num;
    bool changed;
    srwlock lock;
};

int sconfig_init( sconfig_data* p ); //non thread safe
void sconfig_deinit( sconfig_data* p ); //non thread safe
bool sconfig_remove_key( const char* key, sconfig_data* p );
bool sconfig_set_str_value( const char* key, const char* value, sconfig_data* p );
bool sconfig_set_int_value( const char* key, int value, sconfig_data* p );
bool sconfig_set_int_value2( const char* key, int value, int default_value, sconfig_data* p ); //with auto remove
int sconfig_get_int_value( const char* key, int default_value, sconfig_data* p ); //possible return values: found value; 0 in case of empty value; default_value if not found;
char* sconfig_get_str_value( const char* key, const char* default_value, sconfig_data* p ); //possible return values: found value; "" in case of empty value; default_value if not found;
void sconfig_load( const char* filename, sconfig_data* p ); //non thread safe
void sconfig_load_from_string( const char* config, char delim, sconfig_data* p ); //example: config="buffer=1024|audiodriver=alsa|audiodevice=hw:0,0|nowin"; delim='|';
int sconfig_save( sconfig_data* p );
void sconfig_remove_all_files();

//
// App state
//

#define SUNDOG_STATE_TEMP ( 1 << 0 ) //temporary file, the app is responsible for removing it
#define SUNDOG_STATE_ORIGINAL ( 1 << 1 ) //original file, the app can modify it directly
#define SUNDOG_STATE_DELETE_ON_HOST_SIDE ( 1 << 2 ) //when the host wants to control the lifecycle of this object
struct sundog_state
{
    uint32_t flags;
    char* fname;
    std::atomic_int ready_for_delete; //for SUNDOG_STATE_DELETE_ON_HOST_SIDE
    void* data; //stdc pointer (malloc/free) only!
    size_t data_offset;
    size_t data_size;
    //full data size = data_size + data_offset
};
#if defined(SUNDOG_MODULE) || defined(OS_ANDROID) || defined(OS_IOS)
    #define SUNDOG_STATE
#endif
sundog_state* sundog_state_new( const char* fname, uint32_t flags );
sundog_state* sundog_state_new( void* data, size_t data_offset, size_t data_size, uint32_t flags );
void sundog_state_remove( sundog_state* s );
void sundog_state_delete( sundog_state* s ); //delete even if SUNDOG_STATE_DELETE_ON_HOST_SIDE is set
bool sundog_state_is_ready_for_delete( sundog_state* s );
void sundog_state_set( sundog_engine* sd, int io, sundog_state* state ); //io: 0 - app input; 1 - app output;
sundog_state* sundog_state_get( sundog_engine* sd, int io ); //io: 0 - app input; 1 - app output;

//
// String manipulation
//

//*_to_string() retval = length of generated text in chars (without null terminator)
int int_to_string( int value, char* str );
int int_to_string( int value, char* str, int div ); //minimum number of digits: 1 - div=1; 2 - div=10; 3 - div=100; ...
int int_to_string_hex( uint32_t value, char* str );
int int_to_string_hex( uint32_t value, char* str, int str_size );
int get_int_string_len( int value );
int get_int_string_len_hex( uint32_t value );
int string_to_int( const char* str );
int string_to_int_hex( const char* str );
char int_to_hchar( int value );
void truncate_float_str( char* str ); //1.2200 -> 1.22; 1.0 -> 1;
void float_to_string( float value, char* str, int dec_places ); //fast and rough f2s conversion;
//examples:
//  dec_places 1: 1.0="1"; 1.1235="1.1"
//  dec_places 2: 1.0="1"; 1.1235="1.12"; 1.1005="1.1"

int write_varlen_uint32( uint8_t* dest, uint32_t v ); //uint32 -> variable length integer; retval = number of bytes generated; dest size must be at least 5 bytes!
int varlen_uint32_size( uint32_t v );
uint32_t read_varlen_uint32( const uint8_t* src ); //variable length integer -> uint32

uint16_t* utf8_to_utf16( uint16_t* dest, int dest_size, const char* s ); //dest_size = sizeof( destination buffer ) / sizeof( int16 )
uint32_t* utf8_to_utf32( uint32_t* dest, int dest_size, const char* s ); //dest_size = sizeof( destination buffer ) / sizeof( int32 )
char* utf16_to_utf8( char* dst, int dest_size, const uint16_t* src );
char* utf32_to_utf8( char* dst, int dest_size, const uint32_t* src );

int utf8_to_utf32_char( const char* str, uint32_t* res ); //retval: number of bytes read (utf8 char length)
int utf8_to_utf32_char_safe( char* str, size_t str_size, uint32_t* res );

void utf8_unix_slash_to_windows( char* str );
void utf16_unix_slash_to_windows( uint16_t* str );
void utf32_unix_slash_to_windows( uint32_t* str );

int make_string_lowercase( char* dest, size_t dest_size, char* src );
int make_string_uppercase( char* dest, size_t dest_size, char* src );

void get_color_from_string( char* str, uint8_t* r, uint8_t* g, uint8_t* b ); //#RRGGBB -> r,g,b
void get_string_from_color( char* dest, uint dest_size, int r, int g, int b ); //r,g,b -> #RRGGBB

void time_to_str( char* dest, int dest_size, uint64_t t, uint32_t units_per_sec, bool with_unit_names ); //units per sec: 1, 10, 100, 1000 ...
uint64_t str_to_time( const char* src, uint32_t units_per_sec ); //units per sec: 1, 10, 100, 1000 ...

//https://en.wikipedia.org/wiki/Escape_sequences_in_C
//this function DOES NOT automatically add a null terminator to the destination buffer,
//but if this character is present in the source, it can be copied to the destination buffer if there is free space;
//retval = number of bytes written to the destination buffer;
size_t decode_escape_sequences( char* dest, size_t dest_size, const char* src, size_t size );

void trim_trailing_spaces( char* str ); //"ABC   " -> "ABC"
char* trim_leading_spaces( char* str ); //"   ABC" -> "ABC"

#if defined(OS_WIN) || defined(OS_WINCE)
    #ifdef _WIN64
	#define PRINTF_SIZET "%I64u"
	#define PRINTF_SIZET_CONV (size_t)
    #else
	#define PRINTF_SIZET "%u"
	#define PRINTF_SIZET_CONV (uint32_t)
    #endif
#else
    #define PRINTF_SIZET "%zu"
    #define PRINTF_SIZET_CONV (size_t)
#endif

namespace sundog
{

//Someone defined the word "small" in WinCE...
#ifdef small
#undef small
#endif

class string : public smem_base
{
    char*	str;
    size_t	len;
    size_t	smallstr_capacity() const CPP_NOEXCEPT { return sizeof(str) + sizeof(len) - 1; } //bytes
    size_t	smallstr_len_mask2() const CPP_NOEXCEPT { return ( (size_t)1 << ((sizeof(len)-1)*8) ) - 1; }
    int		smallstr_len_offset() const CPP_NOEXCEPT { return (sizeof(len)-1)*8; }
    bool	small() const CPP_NOEXCEPT { return ( len & ((size_t)1<<(sizeof(len)*8-1)) ) != 0; }
    void	set_smallstr_len( size_t new_len ) { len = ( len & smallstr_len_mask2() ) | ( ( new_len | 128 ) << smallstr_len_offset() ); }

public:

    string()
    {
        str = nullptr;
        len = 0;
        set_smallstr_len( 0 );
    }
    void init_copy( const string& src )
    {
	len = src.len;
	if( src.small() )
	    str = src.str;
	else
    	    str = SMEM_CLONE2( src.str, char, src.len + 1 );
    }
    string( const string& src ) //copy constructor
    {
	init_copy( src );
    }
    string( const char* src ); //from c-string
#if 1 // rePlayer __cplusplus >= 201103L
    void init_move( string& src ) CPP_NOEXCEPT
    {
	str = src.str;
	len = src.len;
	src.str = nullptr;
	src.len = 0;
    }
    string( string&& src ) CPP_NOEXCEPT //move constructor
    {
        init_move( src );
    }
#endif
    void deinit()
    {
	if( !small() )
	    smem_free( str );
    }
    ~string()
    {
	deinit();
    }

    // Capacity:
    size_t length() const CPP_NOEXCEPT
    {
	if( small() )
	    return ( len >> smallstr_len_offset() ) & 127;
	return len;
    }
    bool empty() const CPP_NOEXCEPT { return len == 0; }

    // Modifiers:
    string& append( const char* src, size_t src_len );

    // String operations:
    const char* c_str() const CPP_NOEXCEPT //Can't be nullptr!
    {
	if( small() )
	    return (const char*)&str;
	return str;
    }

    // Operators:
    string& operator+= ( const string& src )
    {
	return append( src.c_str(), src.length() );
    }
    string& operator+= ( const char* src )
    {
	if( !src ) return *this;
	return append( src, strlen( src ) );
    }
    string& operator= ( const string& src ) //copy assignment
    {
        if( this == &src ) return *this;
        deinit();
        init_copy( src );
        return *this;
    }
#if 1 // rePlayer __cplusplus >= 201103L
    string& operator= ( string&& src ) noexcept //move assignment
    {
        if( this == &src ) return *this;
        deinit();
        init_move( src );
        return *this;
    }
#endif
};

} //namespace sundog

//
// Locale
//

int slocale_init();
void slocale_deinit();
const char* slocale_get_lang(); //Return language in POSIX format

//
// UNDO manager (action stack)
//

#define UNDO_HANDLER_PARS bool redo, undo_action* action, undo_data* u
#define UNDO_ACTION_FATAL_ERROR		( 1 << 24 )

//action_handler() retval:
// 0 - ok;
// not 0 - empty action or some non-fatal error:
//   undo_add_action(): action will be ignored;
//   execute_undo/redo(): action stack will be cleared;
// UNDO_ACTION_FATAL_ERROR - fatal error bit; action stack will be cleared;

enum 
{
    UNDO_STATUS_NONE = 0,
    UNDO_STATUS_ADD_ACTION,
    UNDO_STATUS_UNDO,
    UNDO_STATUS_REDO
};

struct undo_action
{
    uint32_t level;
    uint32_t type; //Action type
    uint32_t par[ 5 ]; //User defined parameters...
    void* data;
};

struct undo_data
{
    int status; //check it inside the action_handler()

    size_t data_size;
    size_t data_size_limit;

    size_t actions_num_limit;

    uint32_t level;
    size_t first_action;
    size_t cur_action; //Relative to first_action
    size_t actions_num;
    undo_action* actions;
    int (*action_handler)( UNDO_HANDLER_PARS );

    void* user_data;
};

void undo_init( size_t size_limit, int (*action_handler)( UNDO_HANDLER_PARS ), void* user_data, undo_data* u );
void undo_deinit( undo_data* u );
void undo_reset( undo_data* u );
int undo_add_action( undo_action* action, undo_data* u );
void undo_next_level( undo_data* u );
void execute_undo( undo_data* u );
void execute_redo( undo_data* u );

//
// Symbol table (hash table)
//

union SSYMTAB_VAL
{
    int32_t i;
    int64_t i64;
    float f;
    double f64;
    void* p;
};

struct ssymtab_item //Symbol
{
    char*		name; //allocated with smem_strdup()
    int        		type;
    SSYMTAB_VAL		val;
    ssymtab_item*	next;
};

struct ssymtab //Symbol table
{
    int			size;
    ssymtab_item**	symtab;
};

#define SSYMTAB_TABSIZE_NUM 16
extern int g_ssymtab_tabsize[]; //53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433, 1572869

ssymtab* ssymtab_new( int size_level ); //size_level: 0...15 - level; 15... - real symtab size (prime number)
void ssymtab_delete( ssymtab* st );
int ssymtab_init( ssymtab* st, int size_level );
void ssymtab_deinit( ssymtab* st );
inline bool ssymtab_is_initialized( ssymtab* st ) { return st->symtab != nullptr; }
int ssymtab_hash( const char* name, int size );
//auto hash calculation: -1
ssymtab_item* ssymtab_lookup( const char* name, int hash, bool create, int initial_type, SSYMTAB_VAL initial_val, bool* created, ssymtab* st );
ssymtab_item* ssymtab_get_list( ssymtab* st );
int ssymtab_iset( const char* sym_name, int val, ssymtab* st ); //set int value of the symbol
int ssymtab_iset( uint32_t sym_name, int val, ssymtab* st );
int ssymtab_iget( const char* sym_name, int notfound_val, ssymtab* st ); //get int value of the symbol
int ssymtab_iget( uint32_t sym_name, int notfound_val, ssymtab* st );

struct ssymtab_iterator
{
    ssymtab*		st;
    int			n; //next n
    ssymtab_item*	item;
};

void ssymtab_iterator_init( ssymtab_iterator* si, ssymtab* st );
ssymtab_item* ssymtab_iterator_next( ssymtab_iterator* si );
void ssymtab_iterator_deinit( ssymtab_iterator* si );

//
// System-wide copy/paste buffer
//

#ifndef NOFILEUTILS
    #define CAN_COPYPASTE
    #if defined(OS_IOS)
	#define CAN_COPYPASTE_AV
        //macOS: additional tests required (can't paste WAV from SunDog in other apps)
        //       need to use NSPasteboardTypeSound and NSSound?
    #endif
#endif
enum
{
    sclipboard_type_utf8_text = 0,
    sclipboard_type_image,
    sclipboard_type_audio,
    sclipboard_type_video, //only video
    sclipboard_type_movie, //audio+video
    sclipboard_type_av //audio and/or video
};
int sclipboard_copy( sundog_engine* s, const char* filename, uint32_t flags );
char* sclipboard_paste( sundog_engine* s, int type, uint32_t flags ); //retval: filename

//
// URL
//

void open_url( sundog_engine* s, const char* url );

//
// Export / Import functions provided by the system
//

#ifndef NOFILEUTILS
    #if defined(OS_IOS) || defined(OS_ANDROID)
	#define CAN_SEND_TO_GALLERY
    #endif
    #if defined(OS_IOS)
	#define CAN_SEND_TO_EMAIL
	#define CAN_EXPORT
	#define CAN_EXPORT2
	#define CAN_IMPORT
    #endif
#endif
int send_file_to_gallery( sundog_engine* s, const char* filename );
void send_text_to_email( sundog_engine* s, const char* email, const char* subj, const char* body );
#define EIFILE_MODE_IMPORT		0 /* import file */
#define EIFILE_MODE_EXPORT		1 /* export file */
#define EIFILE_MODE_EXPORT2		2 /* open file in another app */
#define EIFILE_MODE			15
#define EIFILE_FLAG_DELFILE		( 1 << 4 ) /* if possible, delete the file after the operation is completed */
int export_import_file( sundog_engine* s, const char* filename, uint32_t flags ); //non blocking! import through the EVT_LOADSTATE

//
// Random generators
//

/*
Simple and fast linear congruential generator
https://en.wikipedia.org/wiki/Linear_congruential_generator
period = 1<<31;
!!! the sequence for the seed X is equivalent to the sequence for the seed (X|0x80000000);
*/
void set_pseudo_random_seed( uint32_t seed );
uint32_t pseudo_random(); //OUT: 0...32767
uint32_t pseudo_random( uint32_t* seed ); //OUT: 0...32767

/*
splitmix64 by Sebastiano Vigna (vigna@acm.org)
(fast generator passing BigCrush)
https://prng.di.unimi.it
*/
uint64_t splitmix64( uint64_t* state );

/*
xoshiro256** by David Blackman and Sebastiano Vigna (vigna@acm.org)
(improved version of the Xorshift algorithm)
https://prng.di.unimi.it
*/
struct xoshiro256ss_state
{
    uint64_t s[ 4 ];
};
uint64_t xoshiro256ss_next( xoshiro256ss_state* state );
void xoshiro256ss_seed( xoshiro256ss_state* state, uint64_t seed );

//
// 3D transformation matrix operations
//

//Matrix structure:
// | 0  4  8  12 |
// | 1  5  9  13 |
// | 2  6  10 14 |
// | 3  7  11 15 |

/*
Common view space (Eye coordinates):
(right-handed)
     +Y
      |
-X -- 0 -- +X
      |
     -Y
Z: + (near; out of the screen); - (far; into the screen)
Camera position: 0,0,0; looks along the -Z axis (into the screen)

Normalized Device Coordinates (NDC) - after exiting the vertex shader, clipping and perspective division
OpenGL (left-handed):
     +1
      |
-1 -- 0 -- +1
      |
     -1
Z: -1 (near) ... +1 (far);
Vulkan (left-handed with inverted Y axis):
     -1
      |
-1 -- 0 -- +1
      |
     +1
Z: 0 (near) ... +1 (far);
*/

void matrix_4x4_reset( float* m ); //reset to identity matrix
void matrix_4x4_mul( float* res, const float* m1, const float* m2 ); //res != m1 != m2 !
void matrix_4x4_rotate( float angle, float x, float y, float z, float* m );
void matrix_4x4_translate( float x, float y, float z, float* m );
void matrix_4x4_scale( float x, float y, float z, float* m );
/*
Multiply by an orthographic matrix (parallel projection):
remap [left,right, bottom,top, -z_near,-z_far] to [-1,1, -1,1, -1(near),1(far)] (equal to OpenGL NDC, left-handed);
for example, if z_near = 2 and z_far = 4, the following Z should be used: from -2 (near) to -4 (far);
*/
void matrix_4x4_ortho( float left, float right, float bottom, float top, float z_near, float z_far, float* m );
/*
Same but for Vulkan NDC:
remap [left,right, bottom,top, -z_near,-z_far] to [-1,1, 1,-1, 0(near),1(far)]
*/
inline void matrix_4x4_ortho_vk( float left, float right, float bottom, float top, float z_near, float z_far, float* m )
{
    matrix_4x4_ortho( left, right, top, bottom, z_near * 3, z_far, m );
}
/*
Зачем знак минуса в z_near/z_far?
К сожалению, я так и не нашел вразумительного ответа, почему не сделали преобразование изначально из [z_near,z_far] в [-1,1].
Что удалось понять:
1) в компьютерной графике после позиционирования камеры обычно используются координаты VIEW SPACE, в которых Z уменьшается при отдалении от камеры (вглубь экрана);
2) в нормализованных координатах (NDC) Z увеличивается при отдалении;
а т.к. ортографическое преобразование переводит Z из пространства (1) в пространство (2),
то пользователя может сбить с толку тот факт, что в первом пространстве Z уменьшается, а во втором увеличивается.
При том, что именно увеличение в глубину учитывается при выборе режима проверки глубины (depth test).
И чтобы такой путаницы не возникало, в данное преобразование ввели минус, в результате чего z_far больше z_near, Z всегда увеличивается при отдалении, пользователь счастлив (наверное).
*/

//
// Image / color / 2D arrays
//

enum simage_pixel_format
{
    PFMT_GRAYSCALE8_SRGB, //8-bit grayscale (sRGB nonlinear);
    PFMT_R8G8B8A8_SRGB, //RGB (sRGB nonlinear), A (linear); first byte = R;
#ifndef SUNDOG_VER
    PFMT_SUNDOG_COLOR, //default pixel format for SunDog1 window manager
#endif
    PFMT_CNT
};

extern uint8_t g_simage_pixel_format_size[ PFMT_CNT ];

struct simage_desc
{
    void*               data;
    simage_pixel_format	format;
    int                 width;
    int                 height;
};

//Non-linear sRGB <-> linear color space:
//(sRGB is not a simple x^2.2 curve; it is linear at small value, then x^2.4 at larger value, resulting an average of 2.2 gamma)
inline float linear_to_sRGB( float v )
{
    if( v <= 0.0031308f ) return v * 12.92f;
    return powf( v, 1 / 2.4f ) * 1.055f - 0.055f;
}
inline float sRGB_to_linear( float v )
{
    if( v <= 0.04045f ) return v / 12.92f;
    return powf( ( v + 0.055f ) / 1.055f, 2.4f );
}
//Colorimetric (perceptual luminance-preserving) conversion to grayscale:
//https://en.wikipedia.org/wiki/Grayscale#Converting_color_to_grayscale
inline float rgb_to_grayscale_linear( float r, float g, float b ) //inputs and output are linear
{
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}
inline int rgb_to_grayscale_sRGB_8bit( int r, int g, int b ) //inputs and output are sRGB nonlinear
{
    float r2 = sRGB_to_linear( r / 255.0f );
    float g2 = sRGB_to_linear( g / 255.0f );
    float b2 = sRGB_to_linear( b / 255.0f );
    r2 = rgb_to_grayscale_linear( r2, g2, b2 );
    return linear_to_sRGB( r2 ) * 255.0f;
}
//Fast but not accurate conversion:
inline int rgb_to_grayscale_sRGB_8bit_fast( int r, int g, int b ) //inputs and output are sRGB nonlinear; 
{
    return ( (int)(0.2126f*65536.0f) * r + (int)(0.7152f*65536.0f) * g + (int)(0.0722f*65536.0f) * b ) >> 16;
}
//Fastest: (r+g+b)/3 (used by sfs_load_jpeg() and sfs_load_png())

//angle*90 degrees (clockwise (по часовой стрелке))
//if save_to == nullptr: the original buffer may be recreated (*ptr will be changed)
int rotate_2d_array( void** ptr, int& xsize, int& ysize, int bytes_per_pixel, int angle, void* save_to );

void vflip_2d_array( void* ptr, size_t xsize, size_t ysize, int bytes_per_pixel ); //flip vertical
void hflip_2d_array( void* ptr, size_t xsize, size_t ysize, int bytes_per_pixel ); //flip hirizontal

//
// Misc
//

size_t round_to_power_of_two( size_t v );
uint sqrt_newton( uint l );
int scale_check( int v, int max, int new_max ); //scale and check the inverse transformation (result * max / new_max = v)
int div_round( int v1, int v2 ); //divide v1/v2 and round to the nearest

inline void swap_bytes( void* vdata, uint32_t size )
{
    int8_t* data = (int8_t*)vdata;
    for( uint32_t i = 0, i2 = size - 1; i < size / 2; i++, i2-- )
    {
        int8_t b1 = data[ i ];
        int8_t b2 = data[ i2 ];
        data[ i ] = b2;
        data[ i2 ] = b1;
    }
}

#define INT32_SWAP( n ) \
    ( ( (((uint32_t)n) << 24 ) & 0xFF000000 ) | \
      ( (((uint32_t)n) << 8 ) & 0x00FF0000 ) | \
      ( (((uint32_t)n) >> 8 ) & 0x0000FF00 ) | \
      ( (((uint32_t)n) >> 24 ) & 0x000000FF ) )

#define INT16_SWAP( n ) \
    ( ( (((uint16_t)n) << 8 ) & 0xFF00 ) | \
      ( (((uint16_t)n) >> 8 ) & 0x00FF ) )\

#define VAL_EXCHANGE( v1, v2 ) { decltype(v1) tmp = v1; v1 = v2; v2 = tmp; } //(C++11 required)
//inline void val_exchange( int* v1, int* v2 ) { int tmp = *v1; *v1 = *v2; *v2 = tmp; }
//inline void val_exchange( int& v1, int& v2 ) { int tmp = v1; v1 = v2; v2 = tmp; }
//inline void val_exchange( uint32_t* v1, uint32_t* v2 ) { uint32_t tmp = *v1; *v1 = *v2; *v2 = tmp; }

#define LIMIT_NUM( val, bottom, top ) { if( val < bottom ) val = bottom; if( val > top ) val = top; }

#define MAX_NUM( x, y ) ( ( (x) > (y) ) ? (x) : (y) )
#define MIN_NUM( x, y ) ( ( (x) < (y) ) ? (x) : (y) )

//[REG_X...]REG_X+REG_LEN
//<CROP_X...>CROP_X+CROP_LEN
//  [...<...>...]  =>  ....[...]....
//  <..[....>...]  =>  ...[....]....
//  [...<....]..>  =>  ....[....]...
//  [.]..<.>       =>  REG_LEN <= 0; REG_X = CROP_X;
//  .....<.>..[.]  =>  REG_LEN <= 0; REG_X = unchanged;
//REG_X and REG_LEN will be changed;
//REG_LEN <= 0 if the region is not visible;
#define CROP_REGION( REG_X, REG_LEN, CROP_X, CROP_LEN ) \
{ \
    if( (REG_X) < (CROP_X) ) { REG_LEN -= (CROP_X) - (REG_X); REG_X = CROP_X; } \
    if( (REG_X) + (REG_LEN) > (CROP_X) + (CROP_LEN) ) REG_LEN = (CROP_X) + (CROP_LEN) - (REG_X); \
}
