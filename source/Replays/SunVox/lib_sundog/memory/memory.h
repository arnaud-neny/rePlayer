#pragma once

#if HEAPSIZE <= 16 && !defined(SMEM_USE_NAMES)
    #define SMEM_FAST_MODE
#endif
#if defined(SMEM_FAST_MODE) && defined(SMEM_USE_NAMES)
    #error SMEM_FAST_MODE cant be used with SMEM_USE_NAMES
#endif
#if !defined(SIZE_MAX)
    #error SIZE_MAX is not defined!
#endif
#ifdef SMEM_USE_NAMES
    #define SMEM_NAME_PARS	, const char* name, int line
    #define SMEM_NAME_ARGS 	, name, line
    #define SMEM_CUR_FN_NAME 	, __PRETTY_FUNCTION__, __LINE__
#else
    #define SMEM_NAME_PARS	/**/
    #define SMEM_NAME_ARGS 	/**/
    #define SMEM_CUR_FN_NAME 	/**/
#endif

struct smem_block
{
    size_t size;
#if SIZE_MAX == 0xFFFFFFFF
    size_t tmp; //to make sure the user data alignment is 8 bytes
#endif
#ifdef SMEM_USE_NAMES
    const char* name; //function name
    size_t line; //line number
#endif
#ifndef SMEM_FAST_MODE
    smem_block* next;
    smem_block* prev;
#endif
};

extern size_t g_smem_error;

//
// Base functions, macros and classes
// (only for blocks allocated by smem_alloc())
//

/*
fn() - base function;
zfn() - fn() with zero padding/filling;
fn_d() - dynamic version of fn(): destination object size can be changed;
FN() - fn() wrapper - adds the name of the parent function as the last argument;
ZFN() - zfn() wrapper;
FN2() - fn() wrapper with type casting and with size/offset specified in elements instead of bytes;
FN3() - similar to FN2() but with automatic type detection (C++11 required)
*/

int smem_global_init();
int smem_global_deinit();
size_t smem_get_usage();
void smem_print_usage();
inline size_t smem_get_size( const void* ptr )
{
    if( !ptr ) return 0;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    return m->size;
}
#define SMEM_GET_SIZE2( PTR ) ( smem_get_size( PTR ) / sizeof( PTR[0] ) ) //Get number of elements
inline const char* smem_get_name( const void* ptr )
{
#ifdef SMEM_USE_NAMES
    if( !ptr ) return NULL;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    return m->name;
#endif
    return NULL;
}
inline int smem_get_line( const void* ptr )
{
#ifdef SMEM_USE_NAMES
    if( !ptr ) return 0;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    return m->line;
#endif
    return 0;
}
void* smem_alloc( size_t size  SMEM_NAME_PARS );
inline void* smem_zalloc( size_t size  SMEM_NAME_PARS ) //Create a block filled with zeros
{
    void* p = smem_alloc( size  SMEM_NAME_ARGS );
    if( p ) memset( p, 0, smem_get_size( p ) );
    return p;
}
#define SMEM_ALLOC( SIZE ) smem_alloc( SIZE  SMEM_CUR_FN_NAME )
#define SMEM_ALLOC2( ELEMENT_TYPE, NUM_ELEMENTS ) (ELEMENT_TYPE*)smem_alloc( (NUM_ELEMENTS)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
#define SMEM_ZALLOC( SIZE ) smem_zalloc( SIZE  SMEM_CUR_FN_NAME )
#define SMEM_ZALLOC2( ELEMENT_TYPE, NUM_ELEMENTS ) (ELEMENT_TYPE*)smem_zalloc( (NUM_ELEMENTS)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
void smem_free( void* ptr );
void* smem_get_stdc_ptr( void* ptr, size_t* data_offset ); //Remove ptr from the SunDog memory manager and convert it to stdc (malloc) pointer
inline void smem_zero( void* ptr ) { if( !ptr ) return; memset( ptr, 0, smem_get_size( ptr ) ); }
void* smem_resize( void* ptr, size_t size  SMEM_NAME_PARS );
void* smem_zresize( void* ptr, size_t size  SMEM_NAME_PARS ); //With zero padding
void* smem_smart_zresize( void* ptr, size_t size, size_t resize_add  SMEM_NAME_PARS ); //If cur_size < new_size : resize(new_size+resize_add) ; with zero padding
#define SMEM_RESIZE( PTR, SIZE ) smem_resize( PTR, SIZE  SMEM_CUR_FN_NAME )
#define SMEM_RESIZE2( PTR, ELEMENT_TYPE, NUM_ELEMENTS ) (ELEMENT_TYPE*)smem_resize( PTR, (NUM_ELEMENTS)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
#define SMEM_EXPAND( PTR, SIZE ) smem_resize( PTR, smem_get_size(PTR)+(SIZE)  SMEM_CUR_FN_NAME )
#define SMEM_EXPAND2( PTR, ELEMENT_TYPE, NUM_ELEMENTS ) (ELEMENT_TYPE*)smem_resize( PTR, smem_get_size(PTR)+(NUM_ELEMENTS)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
#define SMEM_ZRESIZE( PTR, SIZE ) smem_zresize( PTR, SIZE  SMEM_CUR_FN_NAME )
#define SMEM_ZRESIZE2( PTR, ELEMENT_TYPE, NUM_ELEMENTS ) (ELEMENT_TYPE*)smem_zresize( PTR, (NUM_ELEMENTS)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
#define SMEM_ZEXPAND( PTR, SIZE ) smem_zresize( PTR, smem_get_size(PTR)+(SIZE)  SMEM_CUR_FN_NAME )
#define SMEM_ZEXPAND2( PTR, ELEMENT_TYPE, NUM_ELEMENTS ) (ELEMENT_TYPE*)smem_zresize( PTR, smem_get_size(PTR)+(NUM_ELEMENTS)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
#define SMEM_SMART_ZRESIZE( PTR, SIZE, ADD ) smem_smart_zresize( PTR, SIZE, ADD  SMEM_CUR_FN_NAME )
#define SMEM_SMART_ZRESIZE2( PTR, ELEMENT_TYPE, NUM_ELEMENTS, ADD ) (ELEMENT_TYPE*)smem_smart_zresize( PTR, (NUM_ELEMENTS)*sizeof(ELEMENT_TYPE), (ADD)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
void* smem_clone( const void* src  SMEM_NAME_PARS );
void* smem_clone( const void* src, size_t src_size  SMEM_NAME_PARS ); //src = any pointer (not only from smem_alloc()); //if( src_size == -1 ) { src_size = smem_get_size( src ); }
#define SMEM_CLONE( SRC ) smem_clone( SRC  SMEM_CUR_FN_NAME )
#define SMEM_CLONE2( SRC, ELEMENT_TYPE, NUM_ELEMENTS ) (ELEMENT_TYPE*)smem_clone( SRC, (NUM_ELEMENTS)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
#define SMEM_CLONE3( SRC, NUM_ELEMENTS ) (std::remove_const_t<decltype(SRC)>)smem_clone( SRC, (NUM_ELEMENTS)*sizeof(SRC[0])  SMEM_CUR_FN_NAME )
void* smem_copy_d( void* dest, size_t dest_offset, size_t resize_add, const void* src, size_t src_size  SMEM_NAME_PARS ); //Dynamic version of smem_copy() with autoresize and zero padding
#define SMEM_COPY_D2( DEST, ELEMENT_TYPE, OFFSET, ADD, SRC, SRC_SIZE ) (ELEMENT_TYPE*)smem_copy_d( DEST, (OFFSET)*sizeof(ELEMENT_TYPE), (ADD)*sizeof(ELEMENT_TYPE), SRC, (SRC_SIZE)*sizeof(ELEMENT_TYPE)  SMEM_CUR_FN_NAME )
int smem_objarray_write_d( void*** objs, void* obj, bool clone_cstring, size_t n  SMEM_NAME_PARS ); //clone_cstring = clone a string (smem_strdup) instead of just copying a pointer;
#define SMEM_OBJARRAY_WRITE_D( OBJS, OBJ, CLONE_CSTR, N ) smem_objarray_write_d( OBJS, OBJ, CLONE_CSTR, N  SMEM_CUR_FN_NAME )
int smem_intarray_write_d( int** ints, int v, size_t n  SMEM_NAME_PARS );
#define SMEM_INTARRAY_WRITE_D( INTS, V, N ) smem_intarray_write_d( INTS, V, N  SMEM_CUR_FN_NAME )

//
// Base C++ wrappers and macros
//

#ifdef SMEM_USE_NAMES
    #define SMEM_NEW new( std::nothrow  SMEM_CUR_FN_NAME )
#else
    #define SMEM_NEW new( std::nothrow )
#endif
#define SMEM_DELETE( OBJPTR ) { delete OBJPTR; OBJPTR = nullptr; }

//C++ new/delete implementations based on smem_alloc()/smem_free():
//no zero initialization; no exceptions; new(std::nothrow) can return NULL;
class smem_base
{
public:
    void* operator new( size_t size ) CPP_NOEXCEPT { void* p = SMEM_ALLOC( size ); return p; }
    void* operator new( size_t size, const std::nothrow_t& ) CPP_NOEXCEPT { void* p = SMEM_ALLOC( size ); return p; }
    void* operator new( size_t size, void* p ) CPP_NOEXCEPT { return p; }
#ifdef SMEM_USE_NAMES
    void* operator new( size_t size  SMEM_NAME_PARS ) CPP_NOEXCEPT { void* p = smem_alloc( size  SMEM_NAME_ARGS ); return p; }
    void* operator new( size_t size, const std::nothrow_t&  SMEM_NAME_PARS ) CPP_NOEXCEPT { void* p = smem_alloc( size  SMEM_NAME_ARGS ); return p; }
#endif
    void operator delete( void* p ) CPP_NOEXCEPT { smem_free( p ); }
};

//
// Additional functions and macros
// (for any pointers)
//

inline void smem_clear( void* ptr, size_t size )
{
    if( !ptr ) return;
    memset( ptr, 0, size );
}
#define SMEM_CLEAR_STRUCT( s ) smem_clear( &( s ), sizeof( s ) ) //in SunDog2 just use struct={} instead
inline void smem_copy( void* dest, const void* src, size_t size )
{
    if( !dest || !src ) return;
    memmove( dest, src, size ); //Overlapping allowed
}
inline int smem_cmp( const void* p1, const void* p2, size_t size )
{
    if( !p1 || !p2 ) return 0;
    return memcmp( p1, p2, size );
}
void* smem_memmem( void* haystack, size_t haystacklen, const void* needle, size_t needlelen ); //Find the needle in the haystack
void smem_erase( void* ptr, size_t bytes_filled, size_t erase_begin, size_t bytes_to_erase ); //Erase with shift
inline void smem_objarray_erase( void** objs, size_t filled, size_t erase_begin, size_t erase_size )
{
    smem_erase( objs, filled * sizeof(void*), erase_begin * sizeof(void*), erase_size * sizeof(void*) );
}

//
// C string manipulation
//

int smem_strcat( char* dest, size_t dest_size, const char* src );
char* smem_strcat_d( char* dest, const char* src  SMEM_NAME_PARS ); //Dynamic version of smem_strcat(). Use it with the SunDog (smem_alloc) memory blocks only!
#define SMEM_STRCAT_D( DEST, SRC ) DEST = smem_strcat_d( DEST, SRC  SMEM_CUR_FN_NAME )
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
char* smem_strdup( const char* s  SMEM_NAME_PARS );
#define SMEM_STRDUP( S ) smem_strdup( S  SMEM_CUR_FN_NAME )
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

//
// C++ Vector (dynamic array)
//

template <typename T>
class smem_vector : public smem_base
{
    T* d;
    size_t n; //number of elements

public:

    smem_vector()
    {
	d = nullptr;
        n = 0;
    }
    void init_copy( const smem_vector& src )
    {
	d = SMEM_CLONE2( src.data(), T, src.size() );
	n = src.size();
    }
    smem_vector( const smem_vector& src ) //copy constructor
    {
	init_copy( src );
    }
#if 1 // rePlayer __cplusplus >= 201103L
    void init_move( smem_vector& src ) CPP_NOEXCEPT
    {
	d = src.data();
	n = src.size();
	src.d = nullptr;
    }
    smem_vector( smem_vector&& src ) CPP_NOEXCEPT //move constructor
    {
	init_move( src );
    }
#endif
    smem_vector( size_t size ) //zero-initialized
    {
	d = SMEM_ZALLOC2( T, size );
	n = size;
    }
    /*smem_vector( size_t size, const T* data )
    {
	d = SMEM_ALLOC2( T, size );
	memmove( d, data, size * sizeof(T) );
	n = size;
    }*/
    void deinit() { smem_free( d ); }
    ~smem_vector() { deinit(); }

    // Capacity:
    size_t size() const CPP_NOEXCEPT { return n; }
    bool empty() const CPP_NOEXCEPT { return n == 0; }

    // Element access:
    //T& at( size_t i ) { if( i < n ) return data[ i ]; else return 0; }
    //const T& at( size_t i ) const { if( i < n ) return data[ i ]; else return 0; }
    //T& front() { return d[ 0 ]; } //first element
    //const T& front() const { return d[ 0 ]; } //first element
    //T& back() { return d[ n - 1 ]; } //last element
    //const T& back() const { return d[ n - 1 ]; } //last element
    T* data() CPP_NOEXCEPT { return d; }
    const T* data() const CPP_NOEXCEPT { return d; }

    // Modifiers:
    void push_back( const T& v )
    {
	size_t capacity = SMEM_GET_SIZE2( d );
	if( n >= capacity )
	{
	    capacity += capacity / 2 + 1;
	    d = SMEM_RESIZE2( d, T, capacity );
	}
	d[ n++ ] = v;
    }
    void assign( size_t size, const T* data )
    {
	d = SMEM_RESIZE2( d, T, size );
	memmove( d, data, size * sizeof(T) );
	n = size;
    }
    void resize( size_t size )
    {
	d = SMEM_SMART_ZRESIZE2( d, T, size, 0 );
	n = size;
    }

    // Operators:
    T& operator[]( size_t i ) { return d[ i ]; }
    const T& operator[]( size_t i ) const { return d[ i ]; }
    smem_vector<T>& operator=( const smem_vector<T>& src ) //copy assignment
    {
	if( this == &src ) return *this;
	deinit();
	init_copy( src );
	return *this;
    }
#if 1 // rePlayer __cplusplus >= 201103L
    smem_vector<T>& operator=( smem_vector&& src ) CPP_NOEXCEPT //move assignment
    {
	if( this == &src ) return *this;
	deinit();
	init_move( src );
	return *this;
    }
#endif
};
