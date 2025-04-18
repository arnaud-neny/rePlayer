#pragma once

#if HEAPSIZE <= 16 && !defined(SMEM_USE_NAMES)
    #define SMEM_FAST_MODE
#endif
#if defined(SMEM_FAST_MODE) && defined(SMEM_USE_NAMES)
    #error SMEM_FAST_MODE cant be used with SMEM_USE_NAMES
#endif

#define SMEM_MAX_NAME_SIZE ( 24 / sizeof(void*) * sizeof(void*) )
struct smem_block
{
    size_t size;
#ifdef SMEM_USE_NAMES
    char name[ SMEM_MAX_NAME_SIZE ];
#endif
#ifndef SMEM_FAST_MODE
    smem_block* next;
    smem_block* prev;
#endif
};

extern size_t g_smem_error;

//
// Base functions, macros and classes
// (only for blocks allocated by smem_new())
//

int smem_global_init( void );
int smem_global_deinit( void );
size_t smem_get_usage( void );
void smem_print_usage( void );
void* smem_new2( size_t size, const char* name ); //Each memory block has its own name
#define smem_new( size ) smem_new2( size, __FUNCTION__ )
#define smem_new_struct( STRUCT ) (STRUCT*)smem_new( sizeof(STRUCT) )
#define smem_new_structs( STRUCT, CNT ) (STRUCT*)smem_new( sizeof(STRUCT) * (CNT) )
#define smem_znew_struct( STRUCT ) (STRUCT*)smem_znew( sizeof(STRUCT) )
#define smem_znew_structs( STRUCT, CNT ) (STRUCT*)smem_znew( sizeof(STRUCT) * (CNT) )
#define smem_structs_size( PTR ) ( smem_get_size( PTR ) / sizeof( PTR[0] ) )
void smem_free( void* ptr );
void* smem_get_stdc_ptr( void* ptr, size_t* data_offset ); //Remove ptr from the SunDog memory manager and convert it to stdc (malloc) pointer
void smem_zero( void* ptr );
inline void* smem_znew( size_t size ) { void* p = smem_new( size ); smem_zero( p ); return p; }
void* smem_resize( void* ptr, size_t size );
void* smem_resize2( void* ptr, size_t size ); //With zero padding
void* smem_resize_if_smaller( void* ptr, size_t size, size_t resize_add ); //If cur_size < new_size : resize(new_size+size_inc) ; with zero padding
void smem_erase( void* ptr, size_t bytes_filled, size_t erase_begin, size_t bytes_to_erase );
void* smem_copy_d( void* dest_array, size_t dest_offset, size_t resize_add, const void* src, size_t src_size ); //Dynamic version with smem_copy() (with autoresize and zero padding)
void* smem_clone( void* ptr );
int smem_objarray_write_d( void*** objs, void* obj, bool clone_cstring, size_t n ); //clone_cstring = clone a string (smem_strdup) instead of just copying a pointer;
inline void smem_objarray_erase( void** objs, size_t filled, size_t erase_begin, size_t erase_size )
{
    smem_erase( objs, filled * sizeof(void*), erase_begin * sizeof(void*), erase_size * sizeof(void*) );
}
inline int smem_strarray_write_d( const char*** objs, const char* obj, bool clone_cstring, size_t n )
{
    return smem_objarray_write_d( (void***)objs, (void*)obj, clone_cstring, n );
}
int smem_intarray_write_d( int** ints, int v, size_t n );
inline size_t smem_get_size( void* ptr )
{
    if( !ptr ) return 0;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    return m->size;
}
inline char* smem_get_name( void* ptr )
{
#ifdef SMEM_USE_NAMES
    if( !ptr ) return NULL;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    return m->name;
#else
    return NULL;
#endif
}

class smem_wrapper //without zero initialization
{
public:
    void* operator new( size_t size ) SUNDOG_NOEXCEPT { void* p = smem_new( size ); return p; } //can return NULL; no exceptions;
    //void* operator new( size_t size, const std::nothrow_t& ) SUNDOG_NOEXCEPT { void* p = smem_new( size ); return p; } //can return NULL; no exceptions;
    void operator delete( void* p ) SUNDOG_NOEXCEPT { smem_free( p ); }
};

#define smem_delete( OBJPTR ) { delete OBJPTR; OBJPTR = nullptr; }

//
// Additional functions and macros
// (for any pointers)
//

inline void smem_clear( void* ptr, size_t size )
{
    if( !ptr ) return;
    memset( ptr, 0, size );
}
#define smem_clear_struct( s ) smem_clear( &( s ), sizeof( s ) )
inline void smem_copy( void* dest, const void* src, size_t size ) //Overlapping allowed
{
    if( !dest || !src ) return;
    memmove( dest, src, size ); //Overlapping allowed
}
inline int smem_cmp( const void* p1, const void* p2, size_t size )
{
    if( !p1 || !p2 ) return 0;
    return memcmp( p1, p2, size );
}
void* smem_memmem( void* haystack, size_t haystacklen, const void* needle, size_t needlelen ); //find the needle in the haystack

//
// C string manipulation
//

int smem_strcat( char* dest, size_t dest_size, const char* src );
char* smem_strcat_d( char* dest, const char* src ); //Dynamic version with smem_resize(). Use it with the SunDog (smem_new) memory blocks only!
#define smem_strcat_resize( dest, src ) dest = smem_strcat_d( dest, src )
inline int smem_strcmp( const char* s1, const char* s2 )
{
    if( !s1 || !s2 )
    {
	if( s1 == s2 ) return 0;
	if( s1 ) return 1;
	return -1;
    }
    return strcmp( s1, s2 );
}
inline const char* smem_strstr( const char* s1, const char* s2 )
{
    if( !s1 || !s2 ) return NULL;
    return strstr( s1, s2 );
}
inline char* smem_strstr( char* s1, const char* s2 )
{
    if( !s1 || !s2 ) return NULL;
    return strstr( s1, s2 );
}
inline const char* smem_strchr( const char* str, char character )
{
    if( !str ) return NULL;
    return strchr( str, character );
}
inline char* smem_strchr( char* str, char character )
{
    if( !str ) return NULL;
    return strchr( str, character );
}
size_t smem_strlen( const char* );
size_t smem_strlen_utf16( const uint16_t* s ); //number of UTF16 code points
size_t smem_strlen_utf32( const uint32_t* s ); //number of UTF32 code points
char* smem_strdup( const char* s1 );
size_t smem_replace_str( char* dest, size_t dest_size, const char* src, const char* from, const char* to );
inline void smem_replace_char( char* str, char from, char to )
{
    while( *str != 0 ) { if( *str == from ) *str = to; str++; }
}
//Split string by delimiter and put the substring number n to the dest buffer; retval = pointer to the next substring:
const char* smem_split_str( char* dest, size_t dest_size, const char* src, char delim, uint n );
/* Example:
    char substr[ 32 ];
    const char* next = src;
    while( 1 )
    {
        substr[ 0 ] = 0;
        next = smem_split_str( substr, sizeof( substr ), next, ',', 0 );
        if( substr[ 0 ] != 0 ) { ... handle substr ... }
        if( !next ) break;
    }
*/
