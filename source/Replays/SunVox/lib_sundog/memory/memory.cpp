/*
    memory.cpp - memory management (thread-safe)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

/*
    Why use this memory manager instead of new/delete or malloc/free?
    1. Memory usage monitoring.
    2. Control of memory leaks.
    3. Ability to get the size of a simple block of memory (in places where С++ classes are not required).
*/

#include "sundog.h"

smem_block* g_smem_start = NULL;
smem_block* g_smem_prev_block = NULL;  //Previous memory block
size_t g_smem_size = 0;
size_t g_smem_max_size = 0;
smutex g_smem_mutex;
size_t g_smem_error = 0;

static void free_all( void )
{
#ifndef SMEM_FAST_MODE
    smem_block* next;
    smem_block* start2 = g_smem_start;
    bool cleanup = false;
    if( start2 )
    {
	slog( "MEMORY CLEANUP: " );
	cleanup = true;
    }
    int mnum = 0;
    int mlimit = 64;
    while( start2 )
    {
#ifdef SMEM_USE_NAMES
	const char* name = (const char*)start2->name;
#endif
	size_t size;
	next = start2->next;
	size = start2->size;
	start2 = next;
	if( mnum >= mlimit )
	{
	    slog( "..." );
	    break;
	}
	if( mnum ) slog( ", " );
#ifdef SMEM_USE_NAMES
	slog( PRINTF_SIZET " %s", PRINTF_SIZET_CONV size, name );
	//slog( " ADDR:" PRINTF_SIZET, PRINTF_SIZET_CONV ( (char*)start2 + sizeof( smem_block ) ) );
#else
	slog( PRINTF_SIZET, PRINTF_SIZET_CONV size );
#endif
	mnum++;
    }
    if( cleanup ) slog( "\n" );
    while( g_smem_start )
    {
	next = g_smem_start->next;
	g_smem_size -= g_smem_start->size + sizeof( smem_block );
	free( g_smem_start );
	g_smem_start = next;
    }
    g_smem_start = NULL;
    g_smem_prev_block = NULL;
#endif
    if( g_smem_size )
    {
	slog( "Leaked memory: " PRINTF_SIZET "\n", PRINTF_SIZET_CONV g_smem_size );
    }
}

int smem_global_init( void )
{
    g_smem_start = NULL;
    g_smem_prev_block = NULL;
    g_smem_size = 0;
    g_smem_max_size = 0;
    g_smem_error = 0;
#ifndef SMEM_FAST_MODE
    smutex_init( &g_smem_mutex, 0 );
#endif
    return 0;
}

int smem_global_deinit( void )
{
#ifndef SMEM_FAST_MODE
    smutex_destroy( &g_smem_mutex );
#endif
    free_all();
    return 0;
}

size_t smem_get_usage( void )
{
    return g_smem_size;
}

void smem_print_usage( void )
{
    slog( "Max memory used: " PRINTF_SIZET "\n", PRINTF_SIZET_CONV g_smem_max_size );
    if( g_smem_size )
    {
	slog( "Not freed: " PRINTF_SIZET "\n", PRINTF_SIZET_CONV g_smem_size );
    }
}

void* smem_new2( size_t size, const char* name )
{
    size_t new_size = size + sizeof( smem_block ); //Add structure with info to our memory block
    smem_block* m = (smem_block*)malloc( new_size );

    //Save info about new memory block:
    if( m )
    {
	m->size = size;
#ifdef SMEM_USE_NAMES
	char* mname = (char*)m->name;
	uint32_t np = 0;
	for( ; np < SMEM_MAX_NAME_SIZE - 1; np++ ) { mname[ np ] = name[ np ]; if( name[ np ] == 0 ) break; }
	mname[ SMEM_MAX_NAME_SIZE - 1 ] = 0;
	if( name[ np ] )
	{
	    while( name[ np + 1 ] ) np++;
	    mname[ SMEM_MAX_NAME_SIZE - 2 ] = name[ np ];
	    mname[ SMEM_MAX_NAME_SIZE - 3 ] = '-';
	}
#endif

#ifndef SMEM_FAST_MODE
	smutex_lock( &g_smem_mutex );

	m->prev = g_smem_prev_block;
	m->next = NULL;
	if( g_smem_prev_block == NULL )
	{
	    //It is the first block. Save address:
	    g_smem_start = m;
	    g_smem_prev_block = m;
	}
	else
	{
	    //It is not the first block:
	    g_smem_prev_block->next = m;
	    g_smem_prev_block = m;
	}

	g_smem_size += new_size; if( g_smem_size > g_smem_max_size ) g_smem_max_size = g_smem_size;

	smutex_unlock( &g_smem_mutex );
#else
	g_smem_size += new_size; if( g_smem_size > g_smem_max_size ) g_smem_max_size = g_smem_size;
#endif
    }
    else
    {
	slog( "MEM ALLOC ERROR " PRINTF_SIZET " %s\n", PRINTF_SIZET_CONV size, name );
	if( g_smem_error == 0 )
	{
	    g_smem_error = size;
	}
	return NULL;
    }

    int8_t* rv = (int8_t*)m;
    return (void*)( rv + sizeof( smem_block ) );
}

void smem_free( void* ptr )
{
    if( ptr == NULL ) return;

    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );

#ifndef SMEM_FAST_MODE
    smutex_lock( &g_smem_mutex );
#endif

    g_smem_size -= m->size + sizeof( smem_block );

#ifndef SMEM_FAST_MODE
    smem_block* prev = m->prev;
    smem_block* next = m->next;
    if( prev && next )
    {
	prev->next = next;
	next->prev = prev;
    }
    if( prev && next == NULL )
    {
	prev->next = NULL;
	g_smem_prev_block = prev;
    }
    if( prev == NULL && next )
    {
	next->prev = NULL;
	g_smem_start = next;
    }
    if( prev == NULL && next == NULL )
    {
	g_smem_prev_block = NULL;
	g_smem_start = NULL;
    }

    smutex_unlock( &g_smem_mutex );
#endif //not SMEM_FAST_MODE

    free( m );
}

//Remove ptr from the SunDog memory manager and convert it to stdc (malloc) pointer
//retval = malloc()
//data_offset = data offset inside the stdc memblock
void* smem_get_stdc_ptr( void* ptr, size_t* data_offset )
{
    if( ptr == NULL ) return NULL;

    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );

#ifndef SMEM_FAST_MODE
    smutex_lock( &g_smem_mutex );
#endif

    g_smem_size -= m->size + sizeof( smem_block );

#ifndef SMEM_FAST_MODE
    smem_block* prev = m->prev;
    smem_block* next = m->next;
    if( prev && next )
    {
	prev->next = next;
	next->prev = prev;
    }
    if( prev && next == NULL )
    {
	prev->next = NULL;
	g_smem_prev_block = prev;
    }
    if( prev == NULL && next )
    {
	next->prev = NULL;
	g_smem_start = next;
    }
    if( prev == NULL && next == NULL )
    {
	g_smem_prev_block = NULL;
	g_smem_start = NULL;
    }

    smutex_unlock( &g_smem_mutex );
#endif //not SMEM_FAST_MODE

    if( data_offset ) *data_offset = sizeof( smem_block );

    return m;
}

void smem_zero( void* ptr )
{
    if( ptr == NULL ) return;
    smem_clear( ptr, smem_get_size( ptr ) );
}

void* smem_resize( void* ptr, size_t new_size )
{
    if( ptr == NULL ) return smem_new( new_size );
    
    size_t old_size = smem_get_size( ptr );
    if( old_size == new_size ) return ptr;
    
    void* new_ptr = NULL;
    
    //realloc():
#ifdef SMEM_FAST_MODE
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    smem_block* new_m = (smem_block*)realloc( m, new_size + sizeof( smem_block ) );
    if( new_m )
    {
	new_ptr = (void*)( (int8_t*)new_m + sizeof( smem_block ) );
	new_m->size = new_size;
	g_smem_size += new_size - old_size; if( g_smem_size > g_smem_max_size ) g_smem_max_size = g_smem_size;
    }
#else
    smutex_lock( &g_smem_mutex );
    bool change_prev_block = false;
    smem_block* m = (smem_block*)( (int8_t*)ptr - sizeof( smem_block ) );
    if( g_smem_prev_block == m ) change_prev_block = true;
    smem_block* new_m = (smem_block*)realloc( m, new_size + sizeof( smem_block ) );
    if( new_m )
    {
	new_ptr = (void*)( (int8_t*)new_m + sizeof( smem_block ) );
	if( change_prev_block ) g_smem_prev_block = new_m;
	new_m->size = new_size;
	smem_block* prev = new_m->prev;
	smem_block* next = new_m->next;
	if( prev == NULL )
	{
	    g_smem_start = new_m;
	}
	if( prev != NULL )
	{
	    prev->next = new_m;
	}
	if( next != NULL )
	{
	    next->prev = new_m;
	}
	g_smem_size += new_size - old_size; if( g_smem_size > g_smem_max_size ) g_smem_max_size = g_smem_size;
    }
    smutex_unlock( &g_smem_mutex );
#endif //not SMEM_FAST_MODE
    
    return new_ptr;
}

void* smem_resize2( void* ptr, size_t new_size ) //With zero padding
{
    if( ptr == NULL )
    {
        ptr = smem_new( new_size );
        smem_zero( ptr );
        return ptr;
    }
    size_t old_size = smem_get_size( ptr );
    ptr = smem_resize( ptr, new_size );
    if( ptr )
    {
        if( old_size < new_size )
        {
            smem_clear( (int8_t*)ptr + old_size, new_size - old_size );
        }
    }
    return ptr;
}

//If cur_size < new_size : resize(new_size+size_inc) ; with zero padding
void* smem_resize_if_smaller( void* ptr, size_t new_size, size_t resize_add )
{
    if( !ptr )
    {
        ptr = smem_znew( new_size );
        return ptr;
    }
    size_t old_size = smem_get_size( ptr );
    if( new_size > old_size ) 
    {
	ptr = smem_resize( ptr, new_size + resize_add );
	if( ptr )
	{
    	    smem_clear( (int8_t*)ptr + old_size, new_size + resize_add - old_size );
    	}
    }
    return ptr;
}

void smem_erase( void* ptr, size_t bytes_filled, size_t erase_begin, size_t bytes_to_erase )
{
    size_t size = bytes_filled - ( erase_begin + bytes_to_erase );
    memmove( (int8_t*)ptr + erase_begin, (int8_t*)ptr + erase_begin + bytes_to_erase, size );
}

//Dynamic version with smem_copy() (with autoresize and zero padding)
//Overlapping allowed
void* smem_copy_d( void* dest, size_t dest_offset, size_t resize_add, const void* src, size_t src_size )
{
    if( !src || src_size == 0 ) return dest;
    dest = smem_resize_if_smaller( dest, dest_offset + src_size, resize_add );
    if( dest ) memmove( (int8_t*)dest + dest_offset, src, src_size );
    return dest;
}

void* smem_clone( void* ptr )
{
    if( ptr == NULL ) return NULL;
    void* ptr2 = smem_new( smem_get_size( ptr ) );
    if( ptr2 == NULL ) return NULL;
    smem_copy( ptr2, ptr, smem_get_size( ptr ) );
    return ptr2;
}

int smem_objarray_write_d( void*** objs, void* obj, bool clone_cstring, size_t n ) //clone_cstring = clone a string (smem_strdup) instead of just copying a pointer;
{
    int err = SD_RES_SUCCESS;
    int resize_add = 64;
    *objs = (void**)smem_copy_d( *objs, n * sizeof(void*), resize_add * sizeof(void*), &obj, sizeof(void*) );
    if( *objs )
        if( clone_cstring )
            (*objs)[ n ] = (void*)smem_strdup( (const char*)obj );
    if( *objs == nullptr ) err = SD_RES_ERR_MALLOC;
    return err;
}

int smem_intarray_write_d( int** ints, int v, size_t n )
{
    int resize_add = 64;
    int err = SD_RES_SUCCESS;
    *ints = (int*)smem_copy_d( *ints, n * sizeof(int), resize_add * sizeof(int), &v, sizeof(int) );
    if( *ints == nullptr ) err = SD_RES_ERR_MALLOC;
    return err;
}

//
// Additional functions
// (for any pointers)
//

void* smem_memmem( void* haystack_, size_t haystacklen, const void* needle_, size_t needlelen )
{
    uint8_t* haystack = (uint8_t*)haystack_;
    uint8_t* needle = (uint8_t*)needle_;
    for( size_t i = 0; i < haystacklen; i++ )
    {
	int cmp = 0;
	for( size_t n = 0; n < needlelen; n++ )
	{
	    if( haystack[ i + n ] != needle[ n ] )
	    {
		cmp = 1;
		break;
	    }
	}
	if( cmp == 0 )
	{
	    return &haystack[ i ];
	}
    }
    return nullptr;
}

//
// C string manipulation
//

int smem_strcat( char* dest, size_t dest_size, const char* src )
{
    if( dest == NULL || src == NULL || dest_size == 0 ) return 1;
    size_t i;
    for( i = 0; i < dest_size; i++ )
    {
	if( dest[ i ] == 0 ) break;
    }
    if( i == dest_size )
    {
	return 1;
    }
    for( ; i < dest_size; i++ )
    {
	dest[ i ] = *src;
	if( *src == 0 ) break;
	src++;
    }
    if( i == dest_size )
    {
	dest[ dest_size - 1 ] = 0;
	return 1;
    }
    return 0;
}

char* smem_strcat_d( char* dest, const char* src )
{
    if( src == NULL ) return dest;
    size_t src_len = smem_strlen( src );
    if( src_len == 0 ) return dest;
    if( dest == NULL ) return( smem_strdup( src ) );
    size_t dest_size = smem_get_size( dest );
    size_t dest_len = smem_strlen( dest );
    if( dest_len + src_len + 1 > dest_size )
    {
	dest = (char*)smem_resize( dest, dest_len + src_len + 64 );
    }
    smem_copy( dest + dest_len, src, src_len + 1 );
    return dest;
}

size_t smem_strlen( const char* s )
{
    if( s == NULL ) return 0;
    size_t a;
    for( a = 0 ; ; a++ ) if( s[ a ] == 0 ) break;
    return a;
}

size_t smem_strlen_utf16( const uint16_t* s )
{
    if( s == NULL ) return 0;
    size_t a;
    for( a = 0 ; ; a++ ) if( s[ a ] == 0 ) break;
    return a;
}

size_t smem_strlen_utf32( const uint32_t* s )
{
    if( s == NULL ) return 0;
    size_t a;
    for( a = 0 ; ; a++ ) if( s[ a ] == 0 ) break;
    return a;
}

char* smem_strdup( const char* s1 )
{
    if( s1 == NULL ) return NULL;
    size_t len = smem_strlen( s1 );
    char* newstr = (char*)smem_new( len + 1 );
    smem_copy( newstr, s1, len + 1 );
    return newstr;
}

size_t smem_replace_str( char* dest, size_t dest_size, const char* src, const char* from, const char* to )
{
    if( dest == NULL || dest_size == 0 ) return 0;
    if( src == NULL ) return 0;
    if( from == NULL ) return 0;
    if( to == NULL ) return 0;
    size_t replace_counter = 0;
    size_t dest_ptr = 0;
    size_t from_len = smem_strlen( from );
    size_t to_len = smem_strlen( to );
    while( 1 )
    {
	char c = src[ 0 ];
	if( c == 0 ) break;
	bool found;
	if( c == from[ 0 ] )
	{
	    found = true;
	    for( size_t from_ptr = 1; from_ptr < from_len; from_ptr++ )
	    {
		char c2 = src[ from_ptr ];
		if( c2 != from[ from_ptr ] )
		{
		    found = false;
		    break;
		}
		if( c2 == 0 ) break;
	    }
	}
	else
	{
	    found = false;
	}
	if( found )
	{
	    if( dest_ptr + to_len >= dest_size ) break;
	    replace_counter++;
	    for( size_t to_ptr = 0; to_ptr < to_len; to_ptr++ )
	    {
	        dest[ dest_ptr ] = to[ to_ptr ];
	        dest_ptr++;
	    }
	    src += from_len;
	}
	else
	{
	    dest[ dest_ptr ] = c;
    	    dest_ptr++; if( dest_ptr >= dest_size - 1 ) break;
	    src++;
	}
    }
    dest[ dest_ptr ] = 0;
    return replace_counter;
}

//Split string by delimiter and return a substring num
const char* smem_split_str( char* dest, size_t dest_size, const char* src, char delim, uint num )
{
    dest[ 0 ] = 0;
    dest[ dest_size - 1 ] = 0;
    uint v = 0;
    while( 1 )
    {
        if( v == num ) break;
        if( *src == delim ) v++;
        if( *src == 0 ) break;
        src++;
    }
    size_t i = 0;
    for( ; i < dest_size - 1; i++ )
    {
        dest[ i ] = src[ i ];
        if( dest[ i ] == delim ) dest[ i ] = 0;
        if( dest[ i ] == 0 ) break;
    }
    if( i == dest_size - 1 )
    {
	//Out of dest bounds:
	while( 1 )
	{
	    char c = src[ i ];
	    if( c == 0 ) break;
	    if( c == delim ) break;
	    i++;
	}
    }
    if( src[ i ] == 0 )
	return NULL;
    else
	return &src[ i + 1 ];
}
