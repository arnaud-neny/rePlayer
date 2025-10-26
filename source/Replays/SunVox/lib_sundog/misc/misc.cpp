/*
    misc.cpp - miscellaneous: string list, random generator, etc.
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_UNIX
    #include <sys/stat.h> //mkdir
#endif

#if !defined(NOFILEUTILS)

#ifdef OS_MACOS
    #include "main/macos/sundog_bridge.h"
    #include <CoreFoundation/CFBundle.h>
    #include <ApplicationServices/ApplicationServices.h>
#endif
#ifdef OS_IOS
    #include "main/ios/sundog_bridge.h"
#endif
#ifdef OS_ANDROID
    #include "main/android/sundog_bridge.h"
#endif
#ifdef SDL
    #ifdef SDL12
        #include "SDL/SDL.h"
    #else
        #include "SDL2/SDL.h"
    #endif
#endif

#endif

#ifdef X11
    #include <X11/Xlib.h>
    #include <X11/Xatom.h>
    #include <time.h>
    #include <unistd.h>
    #include <poll.h>
    #include <sys/eventfd.h>
    static void x11_clipboard_thread_deinit();
#endif

int smisc_global_init()
{
    sconfig_init( NULL );
    sconfig_load( NULL, NULL );
    slocale_init();
    int no_clog = sconfig_get_int_value( APP_CFG_NO_CLOG, -1, nullptr );
    int no_flog = sconfig_get_int_value( APP_CFG_NO_FLOG, -1, nullptr );
    if( no_clog != -1 ) no_clog = 1; else no_clog = 0;
    if( no_flog != -1 ) no_flog = 1; else no_flog = 0;
    if( no_clog || no_flog ) slog_disable( no_clog, no_flog );
    return 0;
}

int smisc_global_deinit()
{
#ifdef X11
    x11_clipboard_thread_deinit();
#endif
    slocale_deinit();
    sconfig_deinit( NULL );
    return 0;
}

//
// List of strings
//

#ifndef NOLIST

void slist_init( slist_data* data )
{
    smem_clear( data, sizeof( slist_data ) );
    slist_clear( data );
    slist_reset_selection( data );
}

void slist_deinit( slist_data* data )
{
    smem_free( data->heap );
    smem_free( data->items );
    data->heap = 0;
    data->items = 0;
}

void slist_clear( slist_data* data )
{
    data->heap_usage = 0;
    data->items_num = 0;
    data->start_item = 0;
}

void slist_reset_items( slist_data* data )
{
    data->heap_usage = 0;
    data->items_num = 0;
}

void slist_reset_selection( slist_data* data )
{
    data->selected_item = -1;
}

uint slist_get_items_num( slist_data* data )
{
    if( data->heap == 0 ) return 0;
    if( data->items == 0 ) return 0;
    if( data->heap_usage == 0 ) return 0;
    return data->items_num;
}

int slist_add_item( const char* item, uint8_t attr, slist_data* data )
{
    int rv = -1;
    
    if( !data->heap && !data->items )
    {
	data->heap = SMEM_ALLOC2( char, 16 );
	data->items = SMEM_ALLOC2( uint, 16 );
    }
    if( !data->heap ) return -1;
    if( !data->items ) return -1;

    uint old_items_num = smem_get_size( data->items ) / sizeof( uint );
    if( data->items_num + 1 >= old_items_num )
	data->items = SMEM_RESIZE2( data->items, uint, old_items_num * 2 );
    if( !data->items ) return -1;
    data->items[ data->items_num ] = data->heap_usage;
    rv = data->items_num;
    data->items_num++;
    
    uint item_len = smem_strlen( item );
    uint item_ptr = data->heap_usage;
    uint heap_size = smem_get_size( data->heap );
    uint new_heap_usage = item_ptr + item_len + 2;
    if( new_heap_usage > heap_size )
    {
	data->heap = SMEM_RESIZE2( data->heap, char, round_to_power_of_two( new_heap_usage ) );
	if( data->heap == 0 ) return -1;
    }
    data->heap_usage = new_heap_usage;
    
    uint ptr = item_ptr;
    data->heap[ ptr ] = attr | 128;
    ptr++;
    for( uint i = 0; ; i++, ptr++ )
    {
	data->heap[ ptr ] = item[ i ];
	if( item[ i ] == 0 ) break;
    }
    
    return rv;
}

void slist_delete_item( uint item_num, slist_data* data )
{
    if( data->heap == 0 ) return;
    if( data->items == 0 ) return;
    if( data->items_num == 0 ) return;
    if( data->heap_usage == 0 ) return;
    
    if( item_num < data->items_num )
    {
	uint ptr = data->items[ item_num ]; //Offset of our item (in the heap)
	uint item_len = smem_strlen( &data->heap[ ptr ] ); //Item size (in bytes)
	uint items_size = smem_get_size( data->items ) / sizeof( uint );
	
	for( uint p = ptr; p < data->heap_usage - item_len - 1 ; p++ )
	{
	    data->heap[ p ] = data->heap[ p + item_len + 1 ];
	}
	data->heap_usage -= item_len + 1;
	
	for( uint p = 0; p < data->items_num; p++ )
	{
	    if( data->items[ p ] > ptr ) data->items[ p ] -= item_len + 1;
	}
	for( uint p = item_num; p < data->items_num; p++ )
	{
	    if( p + 1 < items_size ) 
		data->items[ p ] = data->items[ p + 1 ];
	    else
		data->items[ p ] = 0;
	}
	data->items_num--;

	if( data->selected_item >= (signed)data->items_num ) 
	{
	    if( data->items_num == 0 )
		data->selected_item = 0;
	    else
		data->selected_item = (signed)data->items_num - 1;
	}
    }
}

void slist_move_item_up( uint item_num, slist_data* data )
{
    if( data->heap == 0 ) return;
    if( data->items == 0 ) return;
    if( data->items_num == 0 ) return;
    if( data->heap_usage == 0 ) return;
    
    if( item_num < data->items_num )
    {
	if( item_num != 0 )
	{
	    uint temp = data->items[ item_num - 1 ];
	    data->items[ item_num - 1 ] = data->items[ item_num ];
	    data->items[ item_num ] = temp;
	    if( (signed)item_num == data->selected_item ) data->selected_item--;
	}
    }
}

void slist_move_item_down( uint item_num, slist_data* data )
{
    if( data->heap == 0 ) return;
    if( data->items == 0 ) return;
    if( data->items_num == 0 ) return;
    if( data->heap_usage == 0 ) return;
    
    if( item_num < data->items_num )
    {
	if( item_num != data->items_num - 1 )
	{
	    uint temp = data->items[ item_num + 1 ];
	    data->items[ item_num + 1 ] = data->items[ item_num ];
	    data->items[ item_num ] = temp;
	    if( (signed)item_num == data->selected_item ) data->selected_item++;
	}
    }
}

void slist_move_item( uint from, uint to, slist_data* data )
{
    if( data->heap == 0 ) return;
    if( data->items == 0 ) return;
    if( data->items_num == 0 ) return;
    if( data->heap_usage == 0 ) return;

    if( from >= data->items_num ) return;
    if( to >= data->items_num ) return;
    if( from == to ) return;

    uint from_data = data->items[ from ];
    for( uint i = from; i < data->items_num - 1; i++ ) data->items[ i ] = data->items[ i + 1 ]; //remove "from"
    for( int i = data->items_num - 1; i > (signed)to; i-- ) data->items[ i ] = data->items[ i - 1 ]; //insert "to"
    data->items[ to ] = from_data;
}

char* slist_get_item( uint item_num, slist_data* data )
{
    if( data->heap == 0 ) return 0;
    if( data->items == 0 ) return 0;
    if( data->items_num == 0 ) return 0;
    if( data->heap_usage == 0 ) return 0;
    
    if( item_num >= data->items_num ) return 0;
    return data->heap + data->items[ item_num ] + 1;
}

uint8_t slist_get_attr( uint item_num, slist_data* data )
{
    if( data->heap == 0 ) return 0;
    if( data->items == 0 ) return 0;
    if( data->items_num == 0 ) return 0;
    if( data->heap_usage == 0 ) return 0;
    
    if( item_num >= data->items_num ) return 0;
    return data->heap[ data->items[ item_num ] ] & 127;
}

//Return values:
//1 - item1 > item2
//0 - item1 <= item2
static int slist_compare_items( uint item1, uint item2, slist_data* data )
{
    char* i1 = data->heap + data->items[ item1 ];
    char* i2 = data->heap + data->items[ item2 ];
    char a1 = i1[ 0 ] & 127;
    char a2 = i2[ 0 ] & 127;
    i1++;
    i2++;
    
    int retval = 0;
    
    //Compare:
    if( a1 != a2 )
    {
	if( a1 == 1 ) retval = 0;
	if( a2 == 1 ) retval = 1;
    }
    else
    {
	for( int a = 0; ; a++ )
	{
	    if( i1[ a ] == 0 ) break;
	    if( i2[ a ] == 0 ) break;
	    if( i1[ a ] < i2[ a ] ) { break; }             //item1 < item2
	    if( i1[ a ] > i2[ a ] ) { retval = 1; break; } //item1 > item2
	}
    }
    
    return retval;
}

void slist_sort( slist_data* data )
{
    if( data->heap == 0 ) return;
    if( data->items == 0 ) return;
    if( data->items_num == 0 ) return;
    if( data->heap_usage == 0 ) return;
    
    for(;;)
    {
	bool s = false;
	for( uint a = 0; a < data->items_num - 1; a++ )
	{
	    if( slist_compare_items( a, a + 1, data ) )
	    {
		s = true;
		uint temp = data->items[ a + 1 ];
		data->items[ a + 1 ] = data->items[ a ];
		data->items[ a ] = temp;
	    }
	}
	if( s == false ) break;
    }
}

void slist_exchange( uint item1, uint item2, slist_data* data )
{
    if( data->heap == 0 ) return;
    if( data->items == 0 ) return;
    if( data->items_num == 0 ) return;
    if( data->heap_usage == 0 ) return;
    
    if( item1 >= data->items_num ) return;
    if( item2 >= data->items_num ) return;
    if( item1 == item2 ) return;

    uint temp = data->items[ item1 ];
    data->items[ item1 ] = data->items[ item2 ];
    data->items[ item2 ] = temp;
}

int slist_save( const char* fname, slist_data* data )
{
    int rv = -1;
    if( data->heap == 0 ) return -1;
    if( data->items == 0 ) return -1;
    if( data->items_num == 0 ) return -1;
    if( data->heap_usage == 0 ) return -1;
    
    sfs_file f = sfs_open( fname, "wb" );
    if( f )
    {
	uint32_t temp = 0;
	const char* sign = "SNDGLIST";
	sfs_write( sign, 1, 8, f );
	sfs_write( &temp, sizeof( temp ), 1, f ); //future use
	sfs_write( &temp, sizeof( temp ), 1, f ); //future use
	for( uint i = 0; i < data->items_num; i++ )
	{
	    char* item = &data->heap[ data->items[ i ] ];
	    sfs_write( item, 1, smem_strlen( item ) + 1, f );
	}
	sfs_close( f );
	rv = 0;
    }
    
    return rv;
}

int slist_load( const char* fname, slist_data* data )
{
    int rv = -1;

    slist_clear( data );

    size_t fsize = sfs_get_file_size( fname );
    if( fsize )
    {
	sfs_file f = sfs_open( fname, "rb" );
	while( f )
	{
	    uint32_t temp;
	    char sign[ 9 ];
	    sign[ 8 ] = 0;
	    sfs_read( &sign, 1, 8, f );
	    if( smem_strcmp( sign, "SNDGLIST" ) != 0 )
	    {
		slog( "List loading error: unknown signature\n" );
		break;
	    }
	    sfs_read( &temp, sizeof( temp ), 1, f ); //future use
	    sfs_read( &temp, sizeof( temp ), 1, f ); //future use
	    fsize -= 16;
	    
	    if( smem_get_size( data->heap ) < fsize )
	    {
		data->heap = SMEM_RESIZE2( data->heap, char, round_to_power_of_two( fsize ) );
	    }
	    if( data->heap )
	    {
		if( sfs_read( data->heap, 1, fsize, f ) == fsize )
		{
		    data->heap_usage = fsize;
		    for( uint i = 0; i < data->heap_usage; i++ )
		    {
			if( data->heap[ i ] == 0 ) data->items_num++;
		    }
		    if( smem_get_size( data->items ) / sizeof( uint ) < data->items_num )
		    {
			data->items = SMEM_RESIZE2( data->items, uint, round_to_power_of_two( data->items_num ) );
		    }
		    if( data->items )
		    {
			uint item_num = 0;
			uint prev_ptr = 0;
			for( uint i = 0; i < data->heap_usage; i++ )
			{
			    if( data->heap[ i ] == 0 ) 
			    {
				data->items[ item_num ] = prev_ptr;
				item_num++;
				prev_ptr = i + 1;
			    }
			}
			rv = 0;
		    }
		}
	    }
	    
	    break;
	}
	if( f )
	{
	    sfs_close( f );
	}
    }
    
    return rv;
}

#endif

//
// Ring buffer
//

sring_buf* sring_buf_new( size_t size, uint32_t flags )
{
    sring_buf* rv;
    rv = SMEM_ZALLOC2( sring_buf, 1 );
    while( rv )
    {
	atomic_init( &rv->wp, (size_t)0 );
	atomic_init( &rv->rp, (size_t)0 );
	rv->flags = flags;
	rv->buf_size = round_to_power_of_two( size );
        rv->buf = SMEM_ALLOC2( uint8_t, rv->buf_size );
	if( ( flags & (SRING_BUF_FLAG_SINGLE_RTHREAD|SRING_BUF_FLAG_SINGLE_WTHREAD) ) != (SRING_BUF_FLAG_SINGLE_RTHREAD|SRING_BUF_FLAG_SINGLE_WTHREAD) )
	{
	    uint32_t mflags = 0;
	    if( flags & SRING_BUF_FLAG_ATOMIC_SPINLOCK )
		mflags |= SMUTEX_FLAG_ATOMIC_SPINLOCK;
	    smutex_init( &rv->m, mflags );
	}
	break;
    }
    return rv;
}

void sring_buf_delete( sring_buf* b )
{
    if( !b ) return;
    smem_free( b->buf );
    if( ( b->flags & (SRING_BUF_FLAG_SINGLE_RTHREAD|SRING_BUF_FLAG_SINGLE_WTHREAD) ) != (SRING_BUF_FLAG_SINGLE_RTHREAD|SRING_BUF_FLAG_SINGLE_WTHREAD) )
	smutex_destroy( &b->m );
    smem_free( b );
}

void sring_buf_read_lock( sring_buf* b )
{
    if( !b ) return;
    if( ( b->flags & SRING_BUF_FLAG_SINGLE_RTHREAD ) == 0 )
	smutex_lock( &b->m );
}

void sring_buf_read_unlock( sring_buf* b )
{
    if( !b ) return;
    if( ( b->flags & SRING_BUF_FLAG_SINGLE_RTHREAD ) == 0 )
	smutex_unlock( &b->m );
}

void sring_buf_write_lock( sring_buf* b )
{
    if( !b ) return;
    if( ( b->flags & SRING_BUF_FLAG_SINGLE_WTHREAD ) == 0 )
	smutex_lock( &b->m );
}

void sring_buf_write_unlock( sring_buf* b )
{
    if( !b ) return;
    if( ( b->flags & SRING_BUF_FLAG_SINGLE_WTHREAD ) == 0 )
	smutex_unlock( &b->m );
}

size_t sring_buf_write( sring_buf* b, void* data, size_t size )
{
    if( !b ) return 0;
    if( !data ) return 0;
    //RP may be less than WP, even if the read operation has already occurred - we can read some old value of RP.
    //WP is always up-to-date, because we don't change it from other threads.
    size_t wp = atomic_load_explicit( &b->wp, std::memory_order_relaxed );
    size_t rp = atomic_load_explicit( &b->rp, std::memory_order_relaxed ); //order_acquire? (see sring_buf_next())
    size_t size2 = b->buf_size - ( ( wp - rp ) & ( b->buf_size - 1 ) );
    if( size >= size2 ) return 0; // ">=" because at least 1 byte must be free in the buffer
    /*if( !b->buf )
    {
	b->buf = SMEM_ALLOC2( uint8_t, b->buf_size );
	if( !b->buf ) return 0;
    }*/
    size_t rv = size;
    uint8_t* bdata = (uint8_t*)data;
    while( size )
    {
        size_t avail = b->buf_size - wp;
        if( avail > size )
            avail = size;
        smem_copy( b->buf + wp, bdata, avail );
        size -= avail;
        bdata += avail;
        wp = ( wp + avail ) & ( b->buf_size - 1 );
    }
    COMPILER_MEMORY_BARRIER(); //for compatibility with older compilers and devices
    atomic_store_explicit( &b->wp, wp, std::memory_order_release );
    return rv;
}

size_t sring_buf_read( sring_buf* b, void* data, size_t size )
{
    if( !b ) return 0;
    if( !data ) return 0;
    if( size == 0 ) return 0;
    //RP is always up-to-date, because we don't change it from other threads.
    size_t rp = atomic_load_explicit( &b->rp, std::memory_order_relaxed );
    size_t wp = atomic_load_explicit( &b->wp, std::memory_order_acquire );
    if( rp == wp ) return 0;
    size_t size2 = ( wp - rp ) & ( b->buf_size - 1 );
    if( size > size2 ) return 0;
    size_t dest_ptr = 0;
    while( size )
    {
        size_t avail = b->buf_size - rp;
        if( avail > size )
            avail = size;
        smem_copy( (uint8_t*)data + dest_ptr, b->buf + rp, avail );
        rp = ( rp + avail ) & ( b->buf_size - 1 );
        size -= avail;
        dest_ptr += avail;
    }
    return dest_ptr;
}

void sring_buf_next( sring_buf* b, size_t size )
{
    if( !b ) return;
    //RP is always up-to-date, because we don't change it from other threads.
    size_t rp = atomic_load_explicit( &b->rp, std::memory_order_relaxed );
    rp = ( rp + size ) & ( b->buf_size - 1 );
    atomic_store_explicit( &b->rp, rp, std::memory_order_relaxed );
    /*
    On buffer overflow: (wp is very close to rp)
      sring_buf_read() may still not be finished here (on ARMs)? Probably not...
      To make sure sring_buf_read() is finished we should use std::memory_order_released (and std::memory_order_acquire in sring_buf_write())
      Is there any other reason to use std::memory_order_released here?..
    */
}

size_t sring_buf_avail( sring_buf* b )
{
    if( !b ) return 0;
    //RP is always up-to-date, because we don't change it from other threads.
    size_t rp = atomic_load_explicit( &b->rp, std::memory_order_relaxed );
    size_t wp = atomic_load_explicit( &b->wp, std::memory_order_relaxed );
    return ( wp - rp ) & ( b->buf_size - 1 );
}

//
// Exchange box for large messages; thread-safe
//

static void smbox_msg_remove( smbox* mb, int i )
{
    smbox_msg* msg = mb->msg[ i ];
    if( !msg ) return;
    smbox_remove_msg( msg );
    mb->msg[ i ] = NULL;
    mb->active--;
}

static void smbox_msg_set( smbox* mb, int i, smbox_msg* msg )
{
    mb->msg[ i ] = msg;
    mb->active++;
}

static bool smbox_msg_check_lifetime( smbox* mb, smbox_msg* msg, stime_ms_t cur_t ) //true = remove
{
    if( msg->lifetime_s )
    {
	if( (stime_ms_t)( cur_t - msg->created_t ) > (stime_ms_t)( msg->lifetime_s * 1000 ) )
	{
	    return true;
	}
    }
    return false;
}

static void smbox_msgs_check_lifetime( smbox* mb, stime_ms_t cur_t )
{
    if( mb->active == 0 ) return;
    for( int i = 0; i < mb->capacity; i++ )
    {
	if( mb->msg[ i ] )
	{
	    if( smbox_msg_check_lifetime( mb, mb->msg[ i ], cur_t ) )
	    {
		smbox_msg_remove( mb, i );
	    }
	}
    }
}

smbox* smbox_new()
{
    smbox* mb = SMEM_ZALLOC2( smbox, 1 );
    if( !mb ) return NULL;
    smutex_init( &mb->mutex, 0 );
    return mb;
}

void smbox_delete( smbox* mb )
{
    if( !mb ) return;
    if( mb->active )
    {
	for( int i = 0; i < mb->capacity; i++ )
	{
	    smbox_msg_remove( mb, i );
	}
    }
    smem_free( mb->msg );
    smutex_destroy( &mb->mutex );
    smem_free( mb );
}

smbox_msg* smbox_new_msg()
{
    smbox_msg* msg = SMEM_ZALLOC2( smbox_msg, 1 );
    return msg;
}

void smbox_remove_msg( smbox_msg* msg )
{
    if( !msg ) return;
    smem_free( msg->data );
    smem_free( msg );
}

int smbox_add( smbox* mb, smbox_msg* msg )
{
    int rv = -1;
    if( !mb ) return -1;
    if( !msg ) return -1;
    stime_ms_t cur_t = stime_ms();
    msg->created_t = cur_t;
    smutex_lock( &mb->mutex );
    smbox_msgs_check_lifetime( mb, cur_t );
    int i = 0;
    for( i = 0; i < mb->capacity; i++ )
    {
	if( mb->msg[ i ] == NULL ) break;
    }
    if( i >= mb->capacity )
    {
	//No free space. Resize:
	mb->capacity += 8;
	mb->msg = SMEM_ZRESIZE2( mb->msg, smbox_msg*, mb->capacity );
    }
    if( mb->msg && mb->msg[ i ] == NULL )
    {
	smbox_msg_set( mb, i, msg );
	rv = 0;
    }
    smutex_unlock( &mb->mutex );
    return rv;
}

smbox_msg* smbox_get( smbox* mb, const void* id, int timeout_ms )
{
    smbox_msg* msg = NULL;
    if( !mb ) return NULL;
    if( timeout_ms )
    {
	int t = 0;
	while( 1 )
	{
	    msg = smbox_get( mb, id, 0 );
	    if( msg ) break;
            stime_sleep( 10 ); t += 10;
            if( t > timeout_ms ) break;
	}
	return msg;
    }
    if( mb->active )
    {
	stime_ms_t cur_t = stime_ms();
	smutex_lock( &mb->mutex );
	if( mb->active )
	{
	    for( int i = 0; i < mb->capacity; i++ )
	    {
		smbox_msg* m = mb->msg[ i ];
		if( m )
		{
		    if( m->id == id )
		    {
			msg = m;
			mb->msg[ i ] = NULL;
			mb->active--;
			break;
		    }
		}
	    }
	}
	smbox_msgs_check_lifetime( mb, cur_t );
	smutex_unlock( &mb->mutex );
    }
    return msg;
}

//
// Configuration
//

sconfig_data g_config;

#define CONFIG_LOCK_TIMEOUT 1000

int sconfig_init( sconfig_data* p ) //not thread safe
{
    int rv = SD_RES_ERR;
    if( !p ) p = &g_config;

    smem_clear( p, sizeof( sconfig_data ) );
    while( 1 )
    {
	p->file_num = -1;
	p->num = 0;
	p->keys = SMEM_ZALLOC2( sconfig_key, 4 );
	if( !p->keys ) { rv = SD_RES_ERR_MALLOC; break; }
	p->st = ssymtab_new( 5 );
	if( !p->st ) break;
	p->changed = true;

	srwlock_init( &p->lock, 0 );

	rv = 0;
	break;
    }

    return rv;
}

void sconfig_deinit( sconfig_data* p ) //not thread safe
{
    if( !p ) p = &g_config;

    smem_free( p->file_name );
    smem_free( p->source );
    p->source = NULL;
    p->file_name = NULL;

    if( p->num && p->keys )
    {
	for( int i = 0; i < p->num; i++ )
	{
	    if( p->keys[ i ].key ) smem_free( p->keys[ i ].key );
	    if( p->keys[ i ].value ) smem_free( p->keys[ i ].value );
	}
    }
    smem_free( p->keys );
    ssymtab_delete( p->st );
    p->st = NULL;
    p->keys = NULL;
    p->num = 0;
    p->changed = false;

    srwlock_destroy( &p->lock );
}

static int sconfig_resize( int new_num, sconfig_data* p )
{
    int rv = -1;

    if( !p ) p = &g_config;

    while( 1 )
    {
	int old_size = smem_get_size( p->keys ) / sizeof( sconfig_key );
	int new_size = new_num + 4;
	if( new_num > old_size )
	{
	    p->keys = SMEM_ZRESIZE2( p->keys, sconfig_key, new_size );
	    if( !p->keys ) break;
	}
	p->num = new_num;
	rv = 0;
	break;
    }

    return rv;
}

static int sconfig_add_key( const char* key, const char* value, int line_num, sconfig_data* p )
{
    int rv = -1;

    if( !p ) p = &g_config;

    while( key && p->keys )
    {
	for( rv = 0; rv < p->num; rv++ )
	{
	    if( p->keys[ rv ].key == NULL ) break;
	}
	if( rv >= p->num )
	{
	    //No free item
	    if( sconfig_resize( p->num + 1, p ) )
	    {
		rv = -1;
		break;
	    }
	}
	p->keys[ rv ].value = SMEM_STRDUP( value );
	p->keys[ rv ].key = SMEM_STRDUP( key );
	p->keys[ rv ].line_num = line_num;
	ssymtab_iset( key, rv, p->st );
	p->changed = true;
	break;
    }

    return rv;
}

bool sconfig_remove_key( const char* key, sconfig_data* p )
{
    bool changed = false;
    if( !p ) p = &g_config;

    if( srwlock_w_lock( &p->lock, CONFIG_LOCK_TIMEOUT ) == -1 ) return false;

    if( key && p->keys )
    {
	int i = ssymtab_iget( key, -1, p->st );
	if( i >= 0 )
	{
	    smem_free( p->keys[ i ].value );
	    p->keys[ i ].value = NULL;
	    p->keys[ i ].deleted = 1;
	    p->changed = true;
	    changed = true;
	}
    }

    srwlock_w_unlock( &p->lock );

    return changed;
}

bool sconfig_set_str_value( const char* key, const char* value, sconfig_data* p )
{
    bool changed = false;
    if( !p ) p = &g_config;

    if( srwlock_w_lock( &p->lock, CONFIG_LOCK_TIMEOUT ) == -1 ) return false;

    if( key && p->keys )
    {
	int i = ssymtab_iget( key, -1, p->st );
	if( i >= 0 )
	{
	    //Already exists:
	    sconfig_key* k = &p->keys[ i ];
	    if( k->value && k->deleted == false && smem_strcmp( k->value, value ) == 0 )
	    {
		//The same value
	    }
	    else
	    {
		char* value_copy = NULL;
		if( value ) value_copy = SMEM_STRDUP( value );
		smem_free( k->value );
		k->value = value_copy;
		k->deleted = false;
		p->changed = true;
		changed = true;
	    }
	}
	else
	{
	    //Does not exist:
	    sconfig_add_key( key, value, -1, p );
	    changed = true;
	}
    }

    srwlock_w_unlock( &p->lock );

    return changed;
}

bool sconfig_set_int_value( const char* key, int value, sconfig_data* p )
{
    char ts[ 16 ];
    int_to_string( value, ts );
    return sconfig_set_str_value( key, ts, p );
}

bool sconfig_set_int_value2( const char* key, int value, int default_value, sconfig_data* p )
{
    bool changed = false;
    if( value == default_value )
	changed = sconfig_remove_key( key, p );
    else
	changed = sconfig_set_int_value( key, value, p );
    return changed;
}

//Possible return values:
//  found value;
//  0 in case of empty value;
//  default_value if no entry was found for the specified key.
int sconfig_get_int_value( const char* key, int default_value, sconfig_data* p )
{
    int rv = default_value;

    if( !p ) p = &g_config;

    if( srwlock_r_lock( &p->lock, CONFIG_LOCK_TIMEOUT ) == -1 ) return rv;

    if( key && p->keys )
    {
	int i = ssymtab_iget( key, -1, p->st );
	if( i >= 0 )
	{
    	    if( p->keys[ i ].value )
	    {
		rv = string_to_int( p->keys[ i ].value );
	    }
	}
    }

    srwlock_r_unlock( &p->lock );

    return rv;
}

//Possible return values:
//  found value;
//  "" in case of empty value;
//  default_value if no entry was found for the specified key.
char* sconfig_get_str_value( const char* key, const char* default_value, sconfig_data* p )
{
    char* rv = (char*)default_value;

    if( !p ) p = &g_config;

    if( srwlock_r_lock( &p->lock, CONFIG_LOCK_TIMEOUT ) == -1 ) return rv;

    if( key && p->keys )
    {
	int i = ssymtab_iget( key, -1, p->st );
	if( i >= 0 )
	{
	    if( p->keys[ i ].value )
	    {
		rv = p->keys[ i ].value;
	    }
	}
    }

    srwlock_r_unlock( &p->lock );

    return rv;
}

#define CONFIG_KEY_CHAR( cc ) ( !( cc < 0x21 || ptr >= size ) )

void sconfig_load( const char* filename, sconfig_data* p ) //not thread safe
{
    char* str1 = SMEM_ALLOC2( char, 1025 );
    if( !str1 ) return;
    char* str2 = SMEM_ALLOC2( char, 1025 );
    if( !str2 ) return;
    str1[ 1024 ] = 0;
    str2[ 1024 ] = 0;
    str1[ 0 ] = 0;
    str2[ 0 ] = 0;
    int i = 0;

    int size = 0;
    char* f = NULL;
    sfs_file fp = 0;

    int ptr = 0;
    int c = 0;
    char comment_mode = 0;
    char key_mode = 0;
    int line_num = 0;

    if( !p ) p = &g_config;

    int pn = -1;
    if( filename == NULL )
    {
	pn = 0;
	while( g_app_config[ pn ] )
	{
	    sfs_file pf = sfs_open( g_app_config[ pn ], "rb" );
	    if( pf )
	    {
		sfs_close( pf );
		filename = g_app_config[ pn ];
		break;
	    }
	    else 
	    {
		filename = NULL;
	    }
	    pn++;
	}
	if( filename == NULL ) pn = -1;
    }

    sconfig_deinit( p );
    sconfig_init( p );

    p->file_num = pn;
    p->file_name = SMEM_ALLOC2( char, smem_strlen( filename ) + 1 );
    p->file_name[ 0 ] = 0;
    SMEM_STRCAT_D( p->file_name, filename );

    size = sfs_get_file_size( filename );
    if( size < 1 ) goto load_end;
    f = SMEM_ALLOC2( char, size );
    if( !f ) goto load_end;
    fp = sfs_open( filename, "rb" );
    if( fp )
    {
	sfs_read( f, 1, size, fp );
	sfs_close( fp );
    }
    else
    {
	smem_free( f );
	goto load_end;
    }

    if( f[ size - 1 ] >= 0x20 && f[ size - 1 ] < 0x7E )
    {
        //Wrong symbol at the end of the file. Must be 0xA or 0xD
	f = SMEM_RESIZE2( f, char, size + 1 );
	if( !f ) goto load_end;
	f[ size ] = 0xA;
	size++;
    }

    while( ptr < size )
    {
        c = f[ ptr ];
        if( c == 0xD || c == 0xA )
        {
    	    comment_mode = 0; //Reset comment mode at the end of line
	    if( key_mode > 0 )
	    {
		sconfig_add_key( str1, str2, line_num, p );
	    }
	    key_mode = 0;
	    line_num++;
	    if( ptr + 1 < size )
	    {
		if( c == 0xD && f[ ptr + 1 ] == 0xA )
		{
		    ptr++;
		}
		else
		{
		    if( c == 0xA && f[ ptr + 1 ] == 0xD ) ptr++;
		}
	    }
	}
	if( comment_mode == 0 )
	{
	    if( f[ ptr ] == '/' && f[ ptr + 1 ] == '/' )
	    {
	        comment_mode = 1; //Comments
		ptr += 2;
	        continue;
	    }
	    if( CONFIG_KEY_CHAR( c ) )
	    {
	        if( key_mode == 0 )
	        {
	    	    //Get key name:
		    str2[ 0 ] = 0;
		    for( i = 0; i < 1024; i++ )
		    {
			if( !CONFIG_KEY_CHAR( f[ ptr ] ) ) 
			{ 
			    str1[ i ] = 0;
			    ptr--;
			    break; 
			}
		        str1[ i ] = f[ ptr ];
			ptr++;
		    }
		    key_mode = 1;
		}
		else if( key_mode == 1 )
		{
		    //Get value:
		    str2[ 0 ] = 0;
		    if( f[ ptr ] == '"' )
		    {
			ptr++;
			for( i = 0; i < 1024; i++ )
			{
			    if( ptr >= size || f[ ptr ] == '"' ) 
			    { 
			        str2[ i ] = 0;
			        break; 
			    }
			    str2[ i ] = f[ ptr ];
			    ptr++;
			}
		    }
		    else
		    {
			for( i = 0; i < 1024; i++ )
			{
			    if( ptr >= size || f[ ptr ] < 0x21 ) 
			    { 
			        str2[ i ] = 0;
				ptr--;
			        break; 
			    }
			    str2[ i ] = f[ ptr ];
			    ptr++;
			}
		    }
		    key_mode = 2;
		}
	    }
	}
	ptr++;
    }
    if( key_mode > 0 )
    {
	sconfig_add_key( str1, str2, line_num, p );
    }

    p->source = f;

load_end:

    smem_free( str1 );
    smem_free( str2 );
}

void sconfig_load_from_string( const char* config, char delim, sconfig_data* p ) // "OPTION1=VAL|OPTION2=VAL|OPTION3|#OPTION_TO_REMOVE..."
{
    if( config == NULL ) return;
    if( config[ 0 ] == 0 ) return;
    if( delim == 0 ) return;
    char str[ 256 ];
    const char* next = config;
    while( next )
    {
	str[ 0 ] = 0;
        next = smem_split_str( str, sizeof( str ), next, delim, 0 ); //next "KEY=VAL"
        if( str[ 0 ] )
        {
            for( int i = smem_strlen( str ) - 1; i > 0; i-- ) //"KEY=VAL  " -> "KEY=VAL"
            {
                if( str[ i ] == ' ' )
                    str[ i ] = 0;
                else
                    break;
            }
        }
        if( str[ 0 ] )
        {
            char* key = str; while( *key == ' ' ) key++; //"  KEY=VAL" -> "KEY=VAL"
            char* val = NULL;
            char* s = smem_strchr( key, '=' );
            if( s )
            {
                *s = 0; //"KEY=" -> "KEY"
                val = s + 1;
                while( *val == ' ' ) val++; //"  VAL" -> "VAL"
            }
            s = smem_strchr( key, ' ' ); if( s ) *s = 0; //"KEY  " -> "KEY"
            if( !val ) val = (char*)"";
            if( key[ 0 ] == '#' )
        	sconfig_remove_key( key + 1, p );
    	    else
        	sconfig_set_str_value( key, val, p );
        }
    }
}

static void sconfig_save_key( int key, sfs_file f, sconfig_data* p )
{
    if( ( p->keys[ key ].key ) && ( p->keys[ key ].deleted == 0 ) )
    {
	sfs_write( p->keys[ key ].key, 1, smem_strlen( p->keys[ key ].key ), f );
	if( p->keys[ key ].value )
	{
	    int vsize = smem_strlen( p->keys[ key ].value );
	    bool q = 0;
	    for( int i2 = 0; i2 < vsize; i2++ )
	    {
		int cc = p->keys[ key ].value[ i2 ];
		if( cc < 0x21 || cc == '/' ) 
		{
		    q = 1;
		    break;
		}
	    }
	    sfs_putc( ' ', f );
	    if( q ) sfs_putc( '"', f );
	    sfs_write( p->keys[ key ].value, 1, vsize, f );
	    if( q ) sfs_putc( '"', f );
	    sfs_putc( '\n', f );
	}
    }
}

int sconfig_save( sconfig_data* p )
{
    int rv = -1;

    if( !p ) p = &g_config;

    if( srwlock_r_lock( &p->lock, CONFIG_LOCK_TIMEOUT ) == -1 ) return rv;

    while( 1 )
    {
	if( p->changed == false )
	{
	    rv = 0;
	    break;
	}
	p->changed = false;

	//Get file name for writing:
	sfs_file f = 0;
	if( p->file_name )
	    f = sfs_open( p->file_name, "wb" );
	if( f )
	{
	    sfs_close( f );
	}
	else 
	{
	    if( p->file_num < 0 )
	    {
		for( p->file_num = 0; ; p->file_num++ )
		{
		    if( g_app_config[ p->file_num ] == NULL ) break;
		}
		p->file_num--;
	    }
	    else 
	    {
		p->file_num--;
	    }
	    if( p->file_num < 0 ) break;
	    while( 1 )
	    {
		smem_free( p->file_name );
		p->file_name = SMEM_STRDUP( g_app_config[ p->file_num ] );
		f = sfs_open( p->file_name, "wb" );
		if( f )
		{
		    sfs_close( f );
		    break;
		}
		p->file_num--;
		if( p->file_num < 0 ) break;
	    }
	    if( p->file_num < 0 ) break;
	}

	//Save config:
	f = sfs_open( p->file_name, "wb" );
	if( f == 0 ) break;
	int line_num = 0;
	bool new_line = 1;
        int size = smem_get_size( p->source );
        for( int i = 0; i < size; i++ )
	{
    	    int c = p->source[ i ];
	    if( c == 0xA || c == 0xD )
	    {
		//New line:
		line_num++;
		new_line = 1;
		sfs_putc( c, f );
	    }
	    else 
	    {
		int key = -1;
		if( new_line )
		{
		    new_line = 0;
		    for( int k = 0; k < p->num; k++ )
		    {
			if( p->keys[ k ].line_num == line_num )
			{
			    //Key found for this line:
			    key = k;
			    break;
			}
		    }
		}
		if( key >= 0 )
		{
		    //Ignore this line:
		    while( i < size )
		    {
			c = p->source[ i ];
			if( c == 0xA || c == 0xD )
			{
			    if( i + 1 < size )
			    {
				if( c == 0xA && p->source[ i + 1 ] == 0xD ) 
				{
			    	    i++;
				}
				else
				{
				    if( c == 0xD && p->source[ i + 1 ] == 0xA ) i++;
				}
			    }
			    line_num++;
			    new_line = 1;
			    break;
			}
			i++;
		    }
		    //Save value:
		    sconfig_save_key( key, f, p );
		}
		else 
		{
		    sfs_putc( c, f );
		}
	    }
	}
	for( int k = 0; k < p->num; k++ )
	{
	    if( p->keys[ k ].line_num == -1 )
	    {
		sconfig_save_key( k, f, p );
	    }
	}
	sfs_close( f );

	rv = 0;
	break;
    }

    srwlock_r_unlock( &p->lock );

    return rv;
}

void sconfig_remove_all_files()
{
    //Remove all config files:
    for( int p = 0;; p++ )
    {
        if( g_app_config[ p ] == NULL ) break;
        slog( "Removing %s\n", g_app_config[ p ] );
        sfs_remove_file( g_app_config[ p ] );
    }
}

//
// App state
//

sundog_state* sundog_state_new( const char* fname, uint32_t flags )
{
    sundog_state* s = (sundog_state*)malloc( sizeof( sundog_state ) );
    if( !s ) return NULL;
    memset( s, 0, sizeof( sundog_state ) );
    s->flags = flags;
    s->fname = strdup( fname );
    if( flags & SUNDOG_STATE_DELETE_ON_HOST_SIDE )
    {
        atomic_init( &s->ready_for_delete, (int)0 );
    }
    return s;
}

sundog_state* sundog_state_new( void* data, size_t data_offset, size_t data_size, uint32_t flags )
{
    sundog_state* s = (sundog_state*)malloc( sizeof( sundog_state ) );
    if( !s ) return NULL;
    memset( s, 0, sizeof( sundog_state ) );
    s->flags = flags;
    s->data = data;
    s->data_offset = data_offset;
    s->data_size = data_size;
    return s;
}

void sundog_state_remove( sundog_state* s )
{
    if( !s ) return;
    if( s->flags & SUNDOG_STATE_DELETE_ON_HOST_SIDE )
        atomic_store( &s->ready_for_delete, (int)1 );
    else
        sundog_state_delete( s );
}

void sundog_state_delete( sundog_state* s )
{
    if( !s ) return;
    free( s->fname );
    free( s->data );
    free( s );
}

bool sundog_state_is_ready_for_delete( sundog_state* s )
{
    if( !s ) return 0;
    return atomic_load( &s->ready_for_delete );
}

//sundog_state_set() and sundog_state_get() must be defined in system dependent code

//
// String manipulation
//

int int_to_string( int value, char* str )
{
    bool sign = false;
    if( value < 0 ) { value = -value; sign = true; }
    char* p = str;
    do {
	int n = value % 10;
	*p = n + 48;
	p++;
	value /= 10;
    } while( value );
    if( sign ) { *p = '-'; p++; }
    int rv = (size_t)( p - str );
    *p = 0; p--;
    char *r = str;
    while( r < p )
    {
	char t = *r;
	*r = *p;
	*p = t;
	r++;
	p--;
    }
    return rv;
}

//minimum number of digits:
//  1 - div=1;
//  2 - div=10;
//  3 - div=100;
//  ...
int int_to_string( int value, char* str, int div )
{
    char* p = str;
    if( value < 0 )
    {
	*p = '-';
	p++;
	value = -value;
    }
    if( value >= div * 10 )
    {
	int v = value / div;
	value -= v * div;
	div /= 10;
	p += int_to_string( v, p );
    }
    while( div >= 1 )
    {
	int v = value / div;
	value -= v * div;
	div /= 10;
	*p = v + 48;
	p++;
    }
    int rv = (size_t)( p - str );
    *p = 0;
    return rv;
}

int int_to_string_hex( uint32_t value, char* str )
{
    char* p = str;
    do {
	int n = value & 15;
	*p = "0123456789ABCDEF"[n];
	p++;
	value >>= 4;
    } while( value );
    int rv = (size_t)( p - str );
    *p = 0; p--;
    char *r = str;
    while( r < p )
    {
	char t = *r;
	*r = *p;
	*p = t;
	r++;
	p--;
    }
    return rv;
}

int int_to_string_hex( uint32_t value, char* str, int str_size )
{
    char* p = str;
    char* p_end = str + str_size - 1;
    do {
	int n = value & 15;
	*p = "0123456789ABCDEF"[n];
	p++;
	value >>= 4;
	if( p == p_end )
	{
	    if( value )
	    {
		*str = 0;
		return 0;
	    }
	    break;
	}
    } while( value );
    int rv = (size_t)( p - str );
    *p = 0; p--;
    char *r = str;
    while( r < p )
    {
	char t = *r;
	*r = *p;
	*p = t;
	r++;
	p--;
    }
    return rv;
}

int get_int_string_len( int value )
{
    int rv = 1;
    if( value < 0 ) { value = -value; rv++; }
    int v = 10;
    while( value >= v )
    {
	v *= 10;
	rv++;
    }
    return rv;
}

int get_int_string_len_hex( uint32_t value )
{
    int rv = 1;
    uint32_t v = 16;
    while( value >= v )
    {
	v *= 16;
	rv++;
    }
    return rv;
}

int string_to_int( const char* str )
{
    int res = 0;
    int d = 1;
    int minus = 1;
    for( int a = smem_strlen( str ) - 1; a >= 0; a-- )
    {
	int v = str[ a ];
	if( v >= '0' && v <= '9' )
	{
	    v -= '0';
	    res += v * d;
	    d *= 10;
	}
	else
	if( v == '-' ) minus = -1;
    }
    return res * minus;
}

int string_to_int_hex( const char* str )
{
    int res = 0;
    int d = 1;
    int minus = 1;
    for( int a = smem_strlen( str ) - 1; a >= 0; a-- )
    {
	int v = str[ a ];
	if( ( v >= '0' && v <= '9' ) || ( v >= 'A' && v <= 'F' ) || ( v >= 'a' && v <= 'f' ) )
	{
	    if( v >= '0' && v <= '9' ) v -= '0';
	    else
	    if( v >= 'A' && v <= 'F' ) { v -= 'A'; v += 10; }
	    else
	    if( v >= 'a' && v <= 'f' ) { v -= 'a'; v += 10; }
	    res += v * d;
	    d *= 16;
	}
	else
	if( v == '-' ) minus = -1;
    }
    return res * minus;
}

char int_to_hchar( int value )
{
    if( value < 10 ) return value + '0';
	else return ( value - 10 ) + 'A';
}

void truncate_float_str( char* str )
{
    int len = 0;
    int dot_ptr = -1;
    while( 1 )
    {
	char c = str[ len ];
	if( !c ) break;
        if( c == '.' ) dot_ptr = len;
        len++;
    }
    if( dot_ptr == -1 ) return;
    int i = len - 1;
    for( ; i >= dot_ptr; i-- )
    {
        if( str[ i ] != '0' ) break;
        str[ i ] = 0;
    }
    if( i == dot_ptr ) str[ i ] = 0;
}

//performance (1024*1024 int/float->str ops; Linux x86_64):
// sprintf("%f") - 700ms; :(
// sprintf("%.2f") - 544ms;
// sprintf("%d") - 100ms;
// float_to_string(v,s,2) - 50ms;
// int_to_string() - 30ms;

//dec_places - number of decimal places after the decimal point
void float_to_string( float value, char* str, int dec_places )
{
    switch( dec_places )
    {
	case 1: value *= 10; break;
	case 2: value *= 100; break;
	case 3: value *= 1000; break;
	case 4: value *= 10000; break;
	case 5: value *= 100000; break;
	default: break;
    }

    bool sign = 0;
    if( value < 0 ) { sign = 1; value = fabs( value ); }

    int iv = (int)value;
    if( value - iv > 0.5 ) iv++;

    int min_len = dec_places;
    char* p = str;
    do {
	int n = iv % 10;
	*p = n + 48;
	p++;
	iv /= 10;
	min_len--;
	if( min_len == 0 ) { *p = '.'; p++; }
    } while( iv || min_len >= 0 );
    if( sign ) { *p = '-'; p++; }
    *p = 0; p--;
    char* last_char_ptr = p;
    char* r = str;
    while( r < p )
    {
	char t = *r;
	*r = *p;
	*p = t;
	r++;
	p--;
    }

    while( *last_char_ptr == '0' )
    {
	*last_char_ptr = 0;
	last_char_ptr--;
    }

    if( *last_char_ptr == '.' )
    {
	*last_char_ptr = 0;
	last_char_ptr--;
    }
}

//Convert uint32 to a variable length integer:
//retval = number of bytes generated;
//min capacity of the dest buffer = 5 bytes;
int write_varlen_uint32( uint8_t* dest, uint32_t v )
{
    int p = 0;
    while( 1 )
    {
	if( v < 128 ) { dest[ p ] = v; p++; break; }
	dest[ p ] = ( v & 127 ) | 128;
	v >>= 7;
	p++;
    }
    return p;
}

int varlen_uint32_size( uint32_t v )
{
    int p = 0;
    while( 1 )
    {
	if( v < 128 ) { p++; break; }
	v >>= 7;
	p++;
    }
    return p;
}

//Convert a variable length integer to uint32:
uint32_t read_varlen_uint32( const uint8_t* src )
{
    uint32_t rv = 0;
    int offset = 0;
    while( 1 )
    {
	uint8_t v = *src;
	rv |= (uint32_t)( v & 127 ) << offset;
	if( v <= 127 ) break;
	offset += 7;
	src++;
    }
    return rv;
}

uint16_t* utf8_to_utf16( uint16_t* dest, int dest_size, const char* s )
{
    uint8_t* src = (uint8_t*)s;
    if( !dest )
    {
	dest = SMEM_ALLOC2( uint16_t, 1024 );
	dest_size = 1024;
	if( !dest ) return NULL;
    }
    uint16_t* dest_begin = dest;
    uint16_t* dest_end = dest + dest_size;
    while( *src != 0 )
    {
	if( *src < 128 ) 
	{
	    *dest = (uint16_t)(*src);
	    src++;
	    dest++;
	}
	else
	{
	    if( *src & 64 )
	    {
		int res;
		if( ( *src & 32 ) == 0 )
		{
		    //Two bytes (11-bit code):
		    res = ( *src & 31 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (uint16_t)res;
		    dest++;
		}
		else if( ( *src & 16 ) == 0 )
		{
		    //Three bytes (16-bit code):
		    res = ( *src & 15 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (uint16_t)res;
		    dest++;
		}
		else if( ( *src & 8 ) == 0 )
		{
		    //Four bytes (21-bit code):
		    res = ( *src & 7 ) << 18;
		    src++;
		    res |= ( *src & 63 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    //res = Code point from U+010000 to U+10FFFF
		    res -= 0x10000;
		    //res = 0 ... FFFFF (20-bit)
		    *dest = 0xD800 + ( ( res >> 10 ) & 1023 );
		    dest++;
		    if( dest >= dest_end )
		    {
			dest--;
			break;
		    }
		    *dest = 0xDC00 + ( res & 1023 );
		    dest++;
		}
		else
		{
		    //Unknown byte:
		    src++;
		    continue;
		}
	    }
	    else
	    {
		//Unknown byte:
		src++;
		continue;
	    }
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    return dest_begin;
}

uint32_t* utf8_to_utf32( uint32_t* dest, int dest_size, const char* s )
{
    uint8_t* src = (uint8_t*)s;
    if( !dest )
    {
	dest = SMEM_ALLOC2( uint32_t, 1024 );
	dest_size = 1024;
	if( !dest ) return NULL;
    }
    uint32_t* dest_begin = dest;
    uint32_t* dest_end = dest + dest_size;
    while( *src != 0 )
    {
	if( *src < 128 ) 
	{
	    *dest = *src;
	    src++;
	    dest++;
	}
	else
	{
	    if( *src & 64 )
	    {
		int res;
		if( ( *src & 32 ) == 0 )
		{
		    //Two bytes (11-bit code):
		    res = ( *src & 31 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (uint32_t)res;
		    dest++;
		}
		else if( ( *src & 16 ) == 0 )
		{
		    //Three bytes (16-bit code):
		    res = ( *src & 15 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (uint32_t)res;
		    dest++;
		}
		else if( ( *src & 8 ) == 0 )
		{
		    //Four bytes (21-bit code):
		    res = ( *src & 7 ) << 18;
		    src++;
		    res |= ( *src & 63 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (uint32_t)res;
		    dest++;
		}
		else
		{
		    //Unknown byte:
		    src++;
		    continue;
		}
	    }
	    else
	    {
		//Unknown byte:
		src++;
		continue;
	    }
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    return dest_begin;
}

char* utf16_to_utf8( char* dst, int dest_size, const uint16_t* src )
{
    uint8_t* dest = (uint8_t*)dst;
    if( !dest )
    {
	dest = SMEM_ALLOC2( uint8_t, 1024 );
	dest_size = 1024;
	if( !dest ) return NULL;
    }
    uint8_t* dest_begin = dest;
    uint8_t* dest_end = dest + dest_size;
    while( *src != 0 )
    {
	int res;
	if( ( *src & ~1023 ) != 0xD800 )
	{
	    res = *src;
	    src++;
	}
	else
	{
	    res = (int)( *src & 1023 ) << 10;
	    src++;
	    res |= (int)( *src & 1023 );
	    src++;
	    //res = 0 ... FFFFF (20-bit)
	    res += 0x10000;
	    //res = Code point from U+010000 to U+10FFFF
	}
	if( res < 128 )
	{
	    *dest = (uint8_t)res;
	    dest++;
	}
	else if( res < 0x800 )
	{
	    if( dest >= dest_end - 2 ) break;
	    *dest = 0xC0 | ( ( res >> 6 ) & 31 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else if( res < 0x10000 )
	{
	    if( dest >= dest_end - 3 ) break;
	    *dest = 0xE0 | ( ( res >> 12 ) & 15 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else
	{
	    if( dest >= dest_end - 4 ) break;
	    *dest = 0xF0 | ( ( res >> 18 ) & 7 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 12 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    return (char*)dest_begin;
}

char* utf32_to_utf8( char* dst, int dest_size, const uint32_t* src )
{
    uint8_t* dest = (uint8_t*)dst;
    if( dest == 0 )
    {
	dest = SMEM_ALLOC2( uint8_t, 1024 );
	dest_size = 1024;
	if( dest == 0 ) return 0;
    }
    uint8_t* dest_begin = dest;
    uint8_t* dest_end = dest + dest_size;
    while( *src != 0 )
    {
	int res = (int)*src;
	src++;
	if( res < 128 )
	{
	    *dest = (uint8_t)res;
	    dest++;
	}
	else if( res < 0x800 )
	{
	    if( dest >= dest_end - 2 ) break;
	    *dest = 0xC0 | ( ( res >> 6 ) & 31 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else if( res < 0x10000 )
	{
	    if( dest >= dest_end - 3 ) break;
	    *dest = 0xE0 | ( ( res >> 12 ) & 15 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else
	{
	    if( dest >= dest_end - 4 ) break;
	    *dest = 0xF0 | ( ( res >> 18 ) & 7 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 12 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    return (char*)dest_begin;
}

int utf8_to_utf32_char( const char* str, uint32_t* res )
{
    *res = 0;
    uint8_t* src = (uint8_t*)str;
    while( *src != 0 )
    {
	if( *src < 128 ) 
	{
	    *res = (uint32_t)*src;
	    return 1;
	}
	else
	{
	    if( *src & 64 )
	    {
		if( ( *src & 32 ) == 0 )
		{
		    //Two bytes:
		    *res = ( *src & 31 ) << 6;
		    src++;
		    *res |= ( *src & 63 );
		    return 2;
		}
		else if( ( *src & 16 ) == 0 )
		{
		    //Three bytes:
		    *res = ( *src & 15 ) << 12;
		    src++;
		    *res |= ( *src & 63 ) << 6;
		    src++;
		    *res |= ( *src & 63 );
		    return 3;
		}
		else if( ( *src & 8 ) == 0 )
		{
		    //Four bytes:
		    *res = ( *src & 7 ) << 18;
		    src++;
		    *res |= ( *src & 63 ) << 12;
		    src++;
		    *res |= ( *src & 63 ) << 6;
		    src++;
		    *res |= ( *src & 63 );
		    return 4;
		}
		else
		{
		    //Unknown byte:
		    *res = '?';
		    return 1;
		}
	    }
	    else
	    {
		//Unknown byte:
		*res = '?';
		return 1;
	    }
	}
    }
    return 0;
}

int utf8_to_utf32_char_safe( char* str, size_t str_size, uint32_t* res )
{
    *res = 0;
    uint8_t* src = (uint8_t*)str;
    uint8_t* src_end = src + str_size;
    while( *src != 0 && src != src_end )
    {
	if( *src < 128 ) 
	{
	    *res = (uint32_t)*src;
	    return 1;
	}
	else
	{
	    if( *src & 64 )
	    {
		if( ( *src & 32 ) == 0 )
		{
		    //Two bytes:
		    *res = ( *src & 31 ) << 6;
		    if( src == src_end ) return 1;
		    src++;
		    *res |= ( *src & 63 );
		    return 2;
		}
		else if( ( *src & 16 ) == 0 )
		{
		    //Three bytes:
		    *res = ( *src & 15 ) << 12;
		    if( src == src_end ) return 1;
		    src++;
		    *res |= ( *src & 63 ) << 6;
		    if( src == src_end ) return 2;
		    src++;
		    *res |= ( *src & 63 );
		    return 3;
		}
		else if( ( *src & 8 ) == 0 )
		{
		    //Four bytes:
		    *res = ( *src & 7 ) << 18;
		    if( src == src_end ) return 1;
		    src++;
		    *res |= ( *src & 63 ) << 12;
		    if( src == src_end ) return 2;
		    src++;
		    *res |= ( *src & 63 ) << 6;
		    if( src == src_end ) return 3;
		    src++;
		    *res |= ( *src & 63 );
		    return 4;
		}
		else
		{
		    //Unknown byte:
		    *res = '?';
		    return 1;
		}
	    }
	    else
	    {
		//Unknown byte:
		*res = '?';
		return 1;
	    }
	}
    }
    return 0;
}

void utf8_unix_slash_to_windows( char* str )
{
    while( *str != 0 )
    {
	if( *str == 0x2F ) *str = 0x5C;
	str++;
    }
}

void utf16_unix_slash_to_windows( uint16_t* str )
{
    while( *str != 0 )
    {
	if( *str == 0x2F ) *str = 0x5C;
	str++;
    }
}

void utf32_unix_slash_to_windows( uint32_t* str )
{
    while( *str != 0 )
    {
	if( *str == 0x2F ) *str = 0x5C;
	str++;
    }
}

int make_string_lower_upper( char* dest, size_t dest_size, char* src, int low_up )
{
    if( src == 0 ) return -1;
    size_t src_size = strlen( src ) + 1;
    if( src_size <= 1 ) return -1;
    uint32_t* str32 = nullptr;
    uint32_t temp[ 64 ];
    if( src_size <= 64 )
	str32 = temp;
    else
	str32 = SMEM_ALLOC2( uint32_t, src_size );
    if( !str32 ) return -1;
    utf8_to_utf32( str32, src_size, src );
    for( size_t i = 0; i < src_size; i++ )
    {
	uint32_t c = str32[ i ];
	if( c == 0 ) break;
	while( 1 )
	{
	    if( low_up == 0 )
	    {
		//Lowercase:
    		if( c >= 65 && c <= 90 )
    		{
    		    //English ASCII:
    		    c += 32;
    		    break;
    		}
    		if( c >= 0x410 && c <= 0x42F )
    		{
    		    //Russian:
    		    c += 32;
    		}
    	    }
    	    else
    	    {
    		//Uppercase:
    		if( c >= 97 && c <= 122 )
    		{
    		    //English ASCII:
    		    c -= 32;
    	    	    break;
    		}
    		if( c >= 0x430 && c <= 0x44F )
    	        {
    	    	    //Russian:
    		    c -= 32;
    		}
    	    }
    	    break;
    	}
        str32[ i ] = c;
    }
    utf32_to_utf8( dest, dest_size, str32 );
    if( str32 != temp )
	smem_free( str32 );
    return 0;
}

int make_string_lowercase( char* dest, size_t dest_size, char* src )
{
    return make_string_lower_upper( dest, dest_size, src, 0 );
}

int make_string_uppercase( char* dest, size_t dest_size, char* src )
{
    return make_string_lower_upper( dest, dest_size, src, 1 );
}

void get_color_from_string( char* str, uint8_t* r, uint8_t* g, uint8_t* b )
{
    uint c = 0;
    if( smem_strlen( str ) < 7 )
    {
	c = 0xFFFFFF;
    }
    else
    {
	for( int i = 1; i < 7; i++ )
	{
    	    c <<= 4;
    	    if( str[ i ] < 58 ) c += str[ i ] - '0';
    	    else if( str[ i ] > 64 && str[ i ] < 91 ) c += str[ i ] - 'A' + 10;
    	    else c += str[ i ] - 'a' + 10;
    	}
    }
    if( r ) *r = ( c >> 16 ) & 255;
    if( g ) *g = ( c >> 8 ) & 255;
    if( b ) *b = c & 255;
}

void get_string_from_color( char* dest, uint dest_size, int r, int g, int b )
{
    if( dest == 0 ) return;
    if( dest_size == 0 ) return;
    if( dest_size < 8 )
    {
	dest[ 0 ] = 0;
	return;
    }
    LIMIT_NUM( r, 0, 255 );
    LIMIT_NUM( g, 0, 255 );
    LIMIT_NUM( b, 0, 255 );
    sprintf( dest, "#%02x%02x%02x", r, g, b );
}

//units per sec: 1, 10, 100, 1000 ...
void time_to_str( char* dest, int dest_size, uint64_t t, uint32_t units_per_sec, bool with_unit_names )
{
    const char* sec_name = "";
    const char* min_name = "";
    const char* hour_name = "";
    if( with_unit_names )
    {
#if !defined(NOMAIN) && !defined(SUNDOG_VER)
	sec_name = wm_get_string( STR_WM_SEC );
	min_name = wm_get_string( STR_WM_MINUTE );
	hour_name = wm_get_string( STR_WM_HOUR );
#else
	sec_name = "s";
	min_name = "m";
	hour_name = "h";
#endif
    }
    uint64_t min_size = 60 * (uint64_t)units_per_sec;
    if( t < min_size )
    {
	uint32_t s = t / units_per_sec;
	t -= s * units_per_sec;
	if( units_per_sec <= 1 )
	    snprintf( dest, dest_size, "%02d%s", s, sec_name );
	else
	{
	    char ts[ 16 ];
	    int_to_string( t, ts, units_per_sec / 10 );
	    snprintf( dest, dest_size, "%02d.%s%s", s, ts, sec_name );
	}
	return;
    }
    uint64_t hour_size = min_size * 60;
    if( t < hour_size )
    {
        uint32_t m = t / min_size;
        t -= m * min_size;
        uint32_t s = t / units_per_sec;
        t -= s * units_per_sec;
        if( units_per_sec <= 1 )
    	    snprintf( dest, dest_size, "%02d%s:%02d%s", m, min_name, s, sec_name );
    	else
    	{
	    char ts[ 16 ];
	    int_to_string( t, ts, units_per_sec / 10 );
    	    snprintf( dest, dest_size, "%02d%s:%02d.%s%s", m, min_name, s, ts, sec_name );
    	}
        return;
    }
    if( 1 )
    {
        uint32_t h = t / hour_size;
        t -= h * hour_size;
        uint32_t m = t / min_size;
        t -= m * min_size;
        uint32_t s = t / units_per_sec;
        t -= s * units_per_sec;
        if( units_per_sec <= 1 )
	    snprintf( dest, dest_size, "%02d%s:%02d%s:%02d%s", h, hour_name, m, min_name, s, sec_name );
	else
	{
	    char ts[ 16 ];
	    int_to_string( t, ts, units_per_sec / 10 );
    	    snprintf( dest, dest_size, "%02d%s:%02d%s:%02d.%s%s", h, hour_name, m, min_name, s, ts, sec_name );
    	}
        return;
    }
}

//units per sec: 1, 10, 100, 1000 ...
uint64_t str_to_time( const char* src, uint32_t units_per_sec )
{
    int substr_cnt = 0;
    char substr[ 3 ][ 16 ];
    const char* next = src;
    while( 1 )
    {
	if( substr_cnt >= 3 ) break;
	char* ss = substr[ substr_cnt ];
	substr_cnt++;
        ss[ 0 ] = 0;
        next = smem_split_str( ss, 16, next, ':', 0 );
        if( !next ) break;
    }
    uint64_t rv = 0;
    for( int i = substr_cnt - 1, i2 = 0; i >= 0; i--, i2++ )
    {
	char* ss = substr[ i ];
	switch( i2 )
	{
	    case 0: //seconds:
		{
		    float v = atof( ss );
		    rv += v * units_per_sec;
		}
		break;
	    case 1: //minutes:
		{
		    uint64_t v = atoi( ss );
		    rv += v * units_per_sec * 60;
		}
		break;
	    case 2: //hours:
		{
		    uint64_t v = atoi( ss );
		    rv += v * units_per_sec * 60 * 60;
		}
		break;
	}
    }
    return rv;
}

//https://en.wikipedia.org/wiki/Escape_sequences_in_C
//this function DOES NOT automatically add a null terminator to the destination buffer,
//but if this character is present in the source, it can be copied to the destination buffer if there is free space;
//retval = number of bytes written to the destination buffer;
size_t decode_escape_sequences( char* dest, size_t dest_size, const char* src, size_t src_size )
{
    size_t r = 0;
    size_t w = 0;
    for( ; r < src_size && w < dest_size; r++, w++ )
    {
	char c = src[ r ];
	if( c == '\\' && r < src_size - 1 )
	{
	    r++;
	    char next_c = src[ r ];
	    switch( next_c )
	    {
		case 'a': dest[ w ] = 0x07; break; //alert
		case 'b': dest[ w ] = 0x08; break; //backspace
		case 't': dest[ w ] = 0x09; break; //tab
		case 'r': dest[ w ] = 0x0D; break; //carriage-return
		case 'n': dest[ w ] = 0x0A; break; //newline
		case 'v': dest[ w ] = 0x0B; break; //vertical-tab
		case 'f': dest[ w ] = 0x0C; break; //form-feed
		case 'e': dest[ w ] = 0x1B; break; //escape character
		case 'x': //8-bit character described by 8-bit hexadecimal number (2 digits)
		case 'u': //unicode character described by 16-bit hexadecimal number (4 digits)
		case 'U': //unicode character described by 32-bit hexadecimal number (8 digits)
		{
		    int num_len = 2; //in chars
		    if( next_c == 'u' ) num_len = 4;
		    if( next_c == 'U' ) num_len = 8;
		    uint32_t val = 0;
		    int i = 0;
		    r++;
		    for( ; i < num_len && r + i < src_size; i++ )
		    {
		        char cc = src[ r + i ];
			val <<= 4;
			if( cc <= '9' )
			{
			    val |= cc - '0';
			}
			else
			{
			    if( cc >= 'a' )
				val |= cc - 'a' + 10;
			    else
			        val |= cc - 'A' + 10;
			}
		    }
		    r += i - 1;
		    if( num_len <= 2 )
		    {
			//8bit:
			dest[ w ] = val;
		    }
		    else
		    {
			//unicode:
			uint32_t utf32_str[ 2 ];
			char utf8_str[ 16 ];
			utf32_str[0] = val;
			utf32_str[1] = 0;
			utf8_str[0] = 0;
			utf32_to_utf8( utf8_str, sizeof(utf8_str), utf32_str );
			size_t w_avail = dest_size - w;
			size_t utf8_str_len = strlen( utf8_str );
			if( utf8_str_len <= w_avail )
			{
			    memcpy( &dest[ w ], utf8_str, utf8_str_len );
			    w += utf8_str_len - 1;
			}
		    }
		    break;
		}
		default:
		{
		    if( next_c >= '0' && next_c <= '7' )
		    {
			//8-bit character described by octal number (3 digits)
			uint8_t val = 0;
			int i = 0;
			r++;
			for( ; i < 3 && r + i < src_size; i++ )
			{
			    char cc = src[ r + i ];
			    if( cc < '0' || cc > '7' ) break;
			    val <<= 3;
			    val |= cc - '0';
			}
			r += i - 1;
			dest[ w ] = val;
		    }
		    else
		    {
			dest[ w ] = next_c;
		    }
		    break;
		}
	    }
	}
	else
	{
	    dest[ w ] = src[ r ];
	}
    }
    return w;
}

void trim_trailing_spaces( char* str ) //"ABC   " -> "ABC"
{
    int len = (int)smem_strlen( str );
    for( int i = len - 1; i >= 0; i-- )
    {
        if( str[ i ] == ' ' )
        {
    	    str[ i ] = 0;
        }
        else break;
    }
}

char* trim_leading_spaces( char* str ) //"   ABC" -> "ABC"
{
    while( *str == ' ' ) str++;
    return str;
}

sundog::string::string( const char* src ) //copy constructor from c-string
{
    size_t src_len = smem_strlen( src );
    str = nullptr;
    len = 0;
    if( src_len + 1 <= smallstr_capacity() )
    {
        if( src ) strcpy( (char*)&str, src );
        set_smallstr_len( src_len );
    }
    else
    {
        str = SMEM_STRDUP( src );
        len = src_len;
    }
}

sundog::string& sundog::string::append( const char* src, size_t src_len ) //append a copy of the first src_len characters in the array of characters
{
    if( !src || !src_len ) return *this;
    size_t old_len = length();
    size_t new_len = old_len + src_len;
    size_t required_capacity = new_len + 1;
    if( small() )
    {
        if( required_capacity <= smallstr_capacity() )
        {
            memmove( ((char*)&str) + old_len, src, src_len );
            ((char*)&str)[ new_len ] = 0;
            set_smallstr_len( new_len );
        }
        else
        {
            char* new_str = SMEM_ALLOC2( char, required_capacity );
            memmove( new_str, (char*)&str, old_len );
            memmove( new_str + old_len, src, src_len );
            new_str[ new_len ] = 0;
            str = new_str;
            len = new_len;
        }
    }
    else
    {
        size_t current_capacity = smem_get_size( str );
        if( required_capacity > current_capacity )
        {
    	    while( required_capacity > current_capacity )
    	        current_capacity += current_capacity / 2 + 1;
    	    str = SMEM_RESIZE2( str, char, current_capacity );
    	}
        memmove( str + old_len, src, src_len );
        str[ new_len ] = 0;
        len = new_len;
    }
    return *this;
}

//
// Locale
//

static char* g_slocale_lang = NULL;

int slocale_init()
{
    int rv = 0;
    char* l = sconfig_get_str_value( APP_CFG_LANG, 0, 0 );
    if( l )
    {
	g_slocale_lang = SMEM_STRDUP( l );
    }
    else
    {
	while( 1 )
	{
#ifdef NOGUI
	    g_slocale_lang = SMEM_STRDUP( "en_US" );
#else
#ifdef OS_UNIX
#ifdef OS_ANDROID
	    g_slocale_lang = SMEM_STRDUP( g_android_lang );
	    break;
#else
#if ( defined(OS_APPLE) ) && !defined(NOMAIN)
	    if( smem_strstr( g_ui_lang, "ru" ) )
		g_slocale_lang = SMEM_STRDUP( "ru_RU" );
	    break;
#else
	    g_slocale_lang = SMEM_STRDUP( getenv( "LANG" ) );
	    break;
#endif
#endif
#else
#if defined(OS_WIN) || defined(OS_WINCE)
	    LCID lid = GetUserDefaultLCID();
	    const char* lname = 0;
	    switch( lid )
	    {
		case 1033: lname = "en_US"; break;
		case 2057: lname = "en_GB"; break;
		case 3081: lname = "en_AU"; break;
		case 4105: lname = "en_CA"; break;
		case 5129: lname = "en_NZ"; break;
		case 6153: lname = "en_IE"; break;
		case 7177: lname = "en_ZA"; break;
		case 1049: lname = "ru_RU"; break;
		case 1058: lname = "uk_UA"; break;
		case 1059: lname = "be_BY"; break;
	    }
	    if( lname ) g_slocale_lang = SMEM_STRDUP( lname );
	    break;
#endif
#endif
#endif //!NOGUI
	    break;
	}
    }
    if( g_slocale_lang == NULL ) g_slocale_lang = SMEM_STRDUP( "en_US" ); //Default language
    return rv;
}

void slocale_deinit()
{
    smem_free( g_slocale_lang );
    g_slocale_lang = NULL;
}

const char* slocale_get_lang()
{
    return g_slocale_lang;
}

//
// UNDO manager (action stack)
//

#define UNDO_DEBUG

void undo_init( size_t size_limit, int (*action_handler)( UNDO_HANDLER_PARS ), void* user_data, undo_data* u )
{
    smem_clear( u, sizeof( undo_data ) );
    u->actions_num_limit = ( size_limit / 4 ) / sizeof( undo_action );
    if( u->actions_num_limit >= 500000 ) u->actions_num_limit = 500000;
    u->data_size_limit = size_limit - u->actions_num_limit * sizeof( undo_action );
    u->action_handler = action_handler;
    u->user_data = user_data;
    //printf( "Action size: %d; action cnt: %d; data size limit: %d kb\n", (int)sizeof( undo_action ), (int)u->actions_num_limit, (int)u->data_size_limit / 1024 );
}

static void undo_clear_action( size_t n, undo_data* u )
{
    undo_action* a = &u->actions[ ( u->first_action + n ) % u->actions_num_limit ];
    u->data_size -= smem_get_size( a->data );
    //printf( "Clear action %d. size: %d. limit: %d\n", smem_get_size( a->data ), u->data_size, u->data_size_limit );
    smem_free( a->data );
    a->data = NULL;
}

void undo_deinit( undo_data* u )
{
    if( u->actions )
    {
	for( size_t i = 0; i < u->actions_num; i++ )
	{
	    undo_clear_action( i, u );
	}
	smem_free( u->actions );
    }
}

void undo_reset( undo_data* u )
{
#ifdef UNDO_DEBUG
    slog( "undo_reset(). data_size: %d\n", u->data_size );
#endif
    if( u->actions )
    {
	for( size_t i = 0; i < u->actions_num; i++ )
	{
	    undo_clear_action( i, u );
	}
	smem_free( u->actions );
	u->actions = 0;
    }
    u->status = 0;
    u->data_size = 0;
    u->level = 0;
    u->first_action = 0;
    u->cur_action = 0;
    u->actions_num = 0;
}

static void undo_remove_first_action( bool including_all_actions_at_this_level, undo_data* u )
{
    undo_action* a = &u->actions[ u->first_action % u->actions_num_limit ];
    uint32_t level = a->level;
    while( u->actions_num && a->level == level )
    {
        undo_clear_action( 0, u );
        u->first_action++;
        u->first_action %= u->actions_num_limit;
        u->cur_action--;
        u->actions_num--;
        if( including_all_actions_at_this_level == false ) break;
        a = &u->actions[ u->first_action % u->actions_num_limit ];
    }
}

static void undo_check_data_overflow( uint32_t level_bound, bool use_level_bound, undo_data* u )
{
    while( ( u->data_size > u->data_size_limit ) && u->actions_num )
    {
	//Data storage overflow.
	//Remove the first action(s):
	undo_action* a = &u->actions[ u->first_action % u->actions_num_limit ];
	if( use_level_bound )
	    if( a->level == level_bound ) break;
	undo_remove_first_action( true, u );
    }
}

int undo_add_action( undo_action* action, undo_data* u )
{
    //Execute action:
    action->level = u->level;
    u->status = UNDO_STATUS_ADD_ACTION;
    int action_error = u->action_handler( 1, action, u );
    u->status = UNDO_STATUS_NONE;
    if( action_error )
    {
#ifdef UNDO_DEBUG
	slog( "undo_add_action(): action %d error %d\n", action->type, action_error );
#endif
	smem_free( action->data );
	action->data = NULL;
	if( action_error & UNDO_ACTION_FATAL_ERROR )
	    undo_reset( u );
	return action_error;
    }

    if( !u->actions )
    {
	u->actions = SMEM_ALLOC2( undo_action, u->actions_num_limit );
    }

    if( u->cur_action >= u->actions_num_limit )
    {
	//Action table overflow.
	//Remove the first action(s):
	undo_remove_first_action( true, u );
    }

    //Remove previous history:
    //(if we performed UNDO and then decided to add a new action)
    for( size_t i = u->cur_action; i < u->actions_num; i++ )
	undo_clear_action( i, u );
    u->actions_num -= u->actions_num - u->cur_action;

    //Save the new action to the table:
    undo_action* a = &u->actions[ ( u->first_action + u->cur_action ) % u->actions_num_limit ];
    smem_copy( a, action, sizeof( undo_action ) );
    u->actions_num++;
    u->cur_action++;
    u->data_size += smem_get_size( a->data );

    return 0;
}

void undo_next_level( undo_data* u )
{
    undo_check_data_overflow( 0, 0, u );
    //printf( "cur: %d / %d; data: %d\n", (int)u->cur_action, (int)u->actions_num, (int)u->data_size );
    u->level++;
}

void execute_undo( undo_data* u )
{
    bool l = 0;
    uint32_t level = 0;

    bool check_data = false;
    while( u->cur_action > 0 )
    {
	u->cur_action--;
	undo_action* a = &u->actions[ ( u->first_action + u->cur_action ) % u->actions_num_limit ];

	if( l == 0 )
	{
	    level = a->level;
	    l = 1;
	}
	else 
	{
	    if( a->level != level )
	    {
		u->cur_action++;
		break;
	    }
	}

	size_t old_size = smem_get_size( a->data );
        u->status = UNDO_STATUS_UNDO;
	int action_error = u->action_handler( 0, a, u );
	u->status = UNDO_STATUS_NONE;
	if( action_error == 0 )
	{
	    size_t new_size = smem_get_size( a->data );
	    u->data_size -= old_size - new_size;
	    check_data = true;
	}
	else
	{
	    //Error:
#ifdef UNDO_DEBUG
	    slog( "execute_undo(). action %d error. data_size: %d\n", a->type, (int)u->data_size );
#endif
	    undo_reset( u );
	    break;
	}
    }
    if( check_data )
    {
	undo_check_data_overflow( level, 1, u );
	if( u->data_size > u->data_size_limit )
	{
	    //Error:
#ifdef UNDO_DEBUG
	    slog( "execute_undo(). data storage overflow: %d\n", (int)u->data_size );
#endif
	    undo_reset( u );
	}
    }
}

void execute_redo( undo_data* u )
{
    bool l = 0;
    uint32_t level = 0;

    bool check_data = false;
    while( u->cur_action < u->actions_num )
    {
	undo_action* a = &u->actions[ ( u->first_action + u->cur_action ) % u->actions_num_limit ];

	if( l == 0 )
	{
	    level = a->level;
	    l = 1;
	}
	else 
	{
	    if( a->level != level ) break;
	}

	size_t old_size = smem_get_size( a->data );
        u->status = UNDO_STATUS_REDO;
	int action_error = u->action_handler( 1, a, u );
        u->status = UNDO_STATUS_NONE;
	if( action_error == 0 )
	{
	    size_t new_size = smem_get_size( a->data );
	    u->data_size -= old_size - new_size;
	    check_data = true;
	}
	else
	{
	    //Error:
#ifdef UNDO_DEBUG
	    slog( "execute_redo(). action %d error. data_size: %d\n", a->type, (int)u->data_size );
#endif
	    undo_reset( u );
	    break;
	}

	u->cur_action++;
    }
    if( check_data )
    {
	undo_check_data_overflow( level, 1, u );
	if( u->data_size > u->data_size_limit )
	{
	    //Error:
#ifdef UNDO_DEBUG
	    slog( "execute_redo(). data storage overflow: %d\n", (int)u->data_size );
#endif
	    undo_reset( u );
	}
    }
}

//
// Symbol table (hash table)
//

int g_ssymtab_tabsize[ SSYMTAB_TABSIZE_NUM ] = { 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433, 1572869 };

int ssymtab_init( ssymtab* st, int size_level )
{
    int rv = SD_RES_ERR;
    if( !st ) return rv;
    smem_clear( st, sizeof( ssymtab ) );

    while( 1 )
    {
	if( (unsigned)size_level >= (unsigned)SSYMTAB_TABSIZE_NUM )
	    st->size = size_level;
	else
	    st->size = g_ssymtab_tabsize[ size_level ];
	st->symtab = SMEM_ZALLOC2( ssymtab_item*, st->size );
	if( !st->symtab ) { rv = SD_RES_ERR_MALLOC; break; }

	rv = 0;
	break;
    }

    return rv;
}

void ssymtab_deinit( ssymtab* st )
{
    if( !st ) return;
    if( !st->symtab ) return;

    for( int i = 0; i < st->size; i++ )
    {
        ssymtab_item* s = st->symtab[ i ];
        while( s )
        {
            ssymtab_item* next = s->next;
            smem_free( s->name );
            smem_free( s );
            s = next;
        }
    }
    smem_free( st->symtab ); st->symtab = nullptr;
}

ssymtab* ssymtab_new( int size_level )
{
    ssymtab* st = SMEM_ZALLOC2( ssymtab, 1 );
    if( !st ) return nullptr;
    if( ssymtab_init( st, size_level ) )
    {
	smem_free( st );
	return nullptr;
    }
    return st;
}

void ssymtab_delete( ssymtab* st )
{
    ssymtab_deinit( st );
    smem_free( st );
}

int ssymtab_hash( const char* name, int size ) //32bit version!
{
    uint h = 0;
    uint8_t* p = (uint8_t*)name;

    for( ; *p != 0; p++ )
        h = 31 * h + *p;

    return (int)( h % size );
}

ssymtab_item* ssymtab_lookup( const char* name, int hash, bool create, int initial_type, SSYMTAB_VAL initial_val, bool* created, ssymtab* st )
{
    ssymtab_item* s;

    if( !st ) return NULL;
    if( !st->symtab ) return NULL;

    if( created ) *created = false;

    if( hash < 0 ) hash = ssymtab_hash( name, st->size );
    for( s = st->symtab[ hash ]; s != NULL; s = s->next )
        if( smem_strcmp( name, s->name ) == 0 )
            return s;

    if( create )
    {
        //Create new symbol:
        s = SMEM_ALLOC2( ssymtab_item, 1 );
        s->name = SMEM_STRDUP( name );
        s->type = initial_type;
	s->val = initial_val;
        s->next = st->symtab[ hash ];
        st->symtab[ hash ] = s;
        if( created ) *created = true;
    }

    return s;
}

ssymtab_item* ssymtab_get_list( ssymtab* st )
{
    ssymtab_item* rv = NULL;
    size_t size = 0;
    if( !st ) return NULL;
    if( !st->symtab ) return NULL;

    for( int i = 0; i < st->size; i++ )
    {
        ssymtab_item* s = st->symtab[ i ];
        while( s )
        {
            if( size == 0 )
                rv = SMEM_ALLOC2( ssymtab_item, 8 );
            else
            {
                if( size >= smem_get_size( rv ) / sizeof( ssymtab_item ) )
                    rv = SMEM_RESIZE2( rv, ssymtab_item, size + 8 );
            }
            rv[ size ].name = s->name;
            rv[ size ].type = s->type;
            rv[ size ].val = s->val;
            size++;
    	    s = s->next;
        }
    }

    if( size > 0 )
    {
        rv = SMEM_RESIZE2( rv, ssymtab_item, size );
    }

    return rv;
}

int ssymtab_iset( const char* sym_name, int val, ssymtab* st )
{
    int rv = 0;
    if( !st ) return -1;
    if( !st->symtab ) return -1;

    SSYMTAB_VAL v;
    v.i = val;
    ssymtab_item* s = ssymtab_lookup( sym_name, -1, true, 0, v, 0, st );
    if( s )
    {
	s->type = 0;
	s->val.i = val;
    }

    return rv;
}

int ssymtab_iset( uint32_t sym_name, int val, ssymtab* st )
{
    char name[ 16 ];
    int_to_string_hex( sym_name, name );
    return ssymtab_iset( name, val, st );
}

int ssymtab_iget( const char* sym_name, int notfound_val, ssymtab* st )
{
    int rv = notfound_val;
    if( !st ) return rv;
    if( !st->symtab ) return rv;

    SSYMTAB_VAL v;
    v.i = 0;
    ssymtab_item* s = ssymtab_lookup( sym_name, -1, false, 0, v, 0, st );
    if( s )
    {
	rv = s->val.i;
    }

    return rv;
}

int ssymtab_iget( uint32_t sym_name, int notfound_val, ssymtab* st )
{
    char name[ 16 ];
    int_to_string_hex( sym_name, name );
    return ssymtab_iget( name, notfound_val, st );
}

void ssymtab_iterator_init( ssymtab_iterator* si, ssymtab* st )
{
    smem_clear( si, sizeof( ssymtab_iterator ) );
    si->st = st;
}

ssymtab_item* ssymtab_iterator_next( ssymtab_iterator* si )
{
    ssymtab* st = si->st;
    if( si->item )
    {
	si->item = si->item->next;
    }
    if( !si->item )
    {
	for( ; si->n < st->size; si->n++ )
	{
    	    si->item = st->symtab[ si->n ];
    	    if( si->item ) { si->n++; break; }
        }
    }
    return si->item;
}

void ssymtab_iterator_deinit( ssymtab_iterator* si )
{
}

//
// System-wide copy/paste buffer
//

#ifdef X11
sthread g_x11_clipboard_thread;
bool g_x11_clipboard_thread_created = false;
std::atomic_int g_x11_clipboard_ready;
int g_x11_clipboard_efd = -1; //file descriptor to event notification
enum {
    X11_CLIPBOARD_EVT_CLOSE = 1,
    X11_CLIPBOARD_EVT_COPY,
    X11_CLIPBOARD_EVT_PASTE,
};
void* g_x11_clipboard_data;
smutex g_x11_clipboard_mutex;
std::atomic_int g_x11_clipboard_paste_finished;
static void* x11_clipboard_thread( void* user_data )
{
    g_x11_clipboard_efd = eventfd( 0, 0 );

    smutex_init( &g_x11_clipboard_mutex, 0 );

    char* win_name = SMEM_STRDUP( g_app_name );
    SMEM_STRCAT_D( win_name, " (CLIPBOARD)" );

    Display* dpy = nullptr;
    const char* dname;
    if( ( dname = getenv( "DISPLAY" ) ) == nullptr )
        dname = ":0";
    dpy = XOpenDisplay( dname );
    if( !dpy )
    {
        slog( "CLIPBOARD: could not open display %s\n", dname );
        return nullptr;
    }
    Window win = XCreateSimpleWindow( dpy, XDefaultRootWindow( dpy ), 0, 0, 1, 1, 0, 0, 0 );
    XSelectInput( dpy, win, PropertyChangeMask ); //requests that the X server report the events associated with the specified event mask
    //XMapWindow( dpy, win );
    XFlush( dpy );

    Atom sel_atom_clipboard = XInternAtom( dpy, "CLIPBOARD", 0 );
    Atom sel_atom_xsel_data = XInternAtom( dpy, "XSEL_DATA", 0 );
    Atom sel_atom_targets = XInternAtom( dpy, "TARGETS", 0 );
    Atom sel_atom_text = XInternAtom( dpy, "TEXT", 0 );
    Atom sel_atom_utf8_string = XInternAtom( dpy, "UTF8_STRING", 1 );
    if( sel_atom_utf8_string == None ) sel_atom_utf8_string = XA_STRING;

    atomic_store( &g_x11_clipboard_ready, 1 );

    int x11_fd = ConnectionNumber( dpy );
    pollfd poll_fds[ 2 ] = {};
    poll_fds[ 0 ].fd = x11_fd;
    poll_fds[ 0 ].events = POLLIN;
    poll_fds[ 1 ].fd = g_x11_clipboard_efd;
    poll_fds[ 1 ].events = POLLIN;
    XEvent evt;
    bool active = true;
    while( active )
    {
	int ready_fds = poll( poll_fds, 2, 1000 );
	if( ready_fds >= 0 )
	{
	    while( XPending( dpy ) )
	    {
    		XNextEvent( dpy, &evt );
	    	switch( evt.type )
    		{
        	    case DestroyNotify: active = false; break;
        	    case SelectionRequest:
        	    {
        	        //Copy:
        	        //printf("COPY BUF %d\n",g_x11_clipboard_data!=nullptr);
            	        if( evt.xselectionrequest.selection != sel_atom_clipboard ) break;
            	        void* buf = nullptr;
            	        XSelectionRequestEvent* xsr = &evt.xselectionrequest;
            	        XSelectionEvent e = { 0 };
	        	e.type = SelectionNotify;
        		e.display = xsr->display;
	                e.requestor = xsr->requestor;
	                e.selection = xsr->selection;
	                e.time = xsr->time;
	                e.target = xsr->target;
	                e.property = xsr->property;
	                while( 1 )
            		{
                	    if( xsr->target == sel_atom_targets )
                	    {
                    	        XChangeProperty( e.display, e.requestor, e.property, XA_ATOM, 32, PropModeReplace, (unsigned char*)&sel_atom_utf8_string, 1 );
                    	        break;
                	    }
                	    Atom target = xsr->target;
                	    if( target == XA_STRING || target == sel_atom_text || xsr->target == sel_atom_utf8_string )
                	    {
			        smutex_lock( &g_x11_clipboard_mutex );
			        buf = SMEM_CLONE( g_x11_clipboard_data );
			        smutex_unlock( &g_x11_clipboard_mutex );
	                	if( buf )
        		        {
                    		    if( target == sel_atom_text ) target = XA_STRING;
	                            XChangeProperty( e.display, e.requestor, e.property, target, 8, PropModeReplace, (const unsigned char*)buf, smem_get_size( buf ) );
        		            break;
                    		}
                    	    }
                	    e.property = None;
                	    break;
                	}
                	XSendEvent( dpy, e.requestor, 0, 0, (XEvent*)&e );
                	smem_free( buf );
            		break;
	    	    }
        	    case SelectionNotify:
        	    {
        		//Paste:
        	        //printf("PASTE\n");
        		if( evt.xselection.selection != sel_atom_clipboard ) break;
            		if( evt.xselection.property )
            		{
			    smutex_lock( &g_x11_clipboard_mutex );
			    smem_free( g_x11_clipboard_data );
			    g_x11_clipboard_data = nullptr;
			    smutex_unlock( &g_x11_clipboard_mutex );

                	    Atom target = 0;
                	    int format;
                	    unsigned long size = 0, size_remain;
                	    char* data = NULL;
                	    XGetWindowProperty( evt.xselection.display, evt.xselection.requestor, evt.xselection.property, 0, ~0, 0, AnyPropertyType, &target, &format, &size, &size_remain, (unsigned char**)&data );
                	    if( size && data )
                	    {
			        smutex_lock( &g_x11_clipboard_mutex );
			        g_x11_clipboard_data = SMEM_ALLOC( size );
                    		if( g_x11_clipboard_data )
                    		{
                        	    smem_copy( g_x11_clipboard_data, data, size );
                        	    //if( target == wm->sel_atom_utf8_string || target == XA_STRING ) {}
                    		}
			        smutex_unlock( &g_x11_clipboard_mutex );
                	    }
                	    XFree( data );
                	    XDeleteProperty( evt.xselection.display, evt.xselection.requestor, evt.xselection.property );
                	}
                	atomic_store( &g_x11_clipboard_paste_finished, 1 );
        		break;
        	    }
    		} //switch( evt.type )
	    }
	    if( poll_fds[ 1 ].revents & POLLIN )
	    {
		uint64_t v = 0;
		read( g_x11_clipboard_efd, &v, sizeof( v ) );
		//printf( "CLIPBOARD SIGNAL %d\n", (int)v );
		switch( v )
		{
		    case X11_CLIPBOARD_EVT_CLOSE: active = false; break; //close
		    case X11_CLIPBOARD_EVT_COPY:
			XSetSelectionOwner( dpy, sel_atom_clipboard, win, CurrentTime );
			//X output buffer (after some X operations) is automatically flushed by calls to XPending(), XNextEvent(), and XWindowEvent().
			//Otherwise call the XFlush():
			XFlush( dpy );
			break;
		    case X11_CLIPBOARD_EVT_PASTE:
		    	XConvertSelection( dpy, sel_atom_clipboard, sel_atom_utf8_string, sel_atom_xsel_data, win, CurrentTime );
			XFlush( dpy );
			break;
		    default: break;
		}
	    }
	}
    }

    XDestroyWindow( dpy, win );
    XCloseDisplay( dpy );
    smem_free( win_name );
    close( g_x11_clipboard_efd ); g_x11_clipboard_efd = -1;
    smutex_lock( &g_x11_clipboard_mutex );
    smem_free( g_x11_clipboard_data ); g_x11_clipboard_data = nullptr;
    smutex_unlock( &g_x11_clipboard_mutex );
    smutex_destroy( &g_x11_clipboard_mutex );
    atomic_store( &g_x11_clipboard_ready, 0 );
    //printf( "CLIPBOARD THREAD FINISHED\n" );
    return nullptr;
}
static int x11_clipboard_thread_init( sundog_engine* s )
{
    int rv = 0;
    if( g_x11_clipboard_thread_created ) return 0;
    sthread_create( &g_x11_clipboard_thread, s, x11_clipboard_thread, s, 0 );
    g_x11_clipboard_thread_created = true;
    STIME_WAIT_FOR( atomic_load( &g_x11_clipboard_ready ) != 0, 1000, 1, { rv = -1; } );
    if( rv )
    {
        sthread_destroy( &g_x11_clipboard_thread, STHREAD_TIMEOUT_INFINITE );
        g_x11_clipboard_thread_created = false;
    }
    return rv;
}
static void x11_clipboard_thread_deinit()
{
    if( g_x11_clipboard_thread_created )
    {
	uint64_t v = X11_CLIPBOARD_EVT_CLOSE;
	write( g_x11_clipboard_efd, &v, sizeof( v ) );
        sthread_destroy( &g_x11_clipboard_thread, STHREAD_TIMEOUT_INFINITE );
        g_x11_clipboard_thread_created = false;
    }
}
#endif

int sclipboard_copy( sundog_engine* s, const char* filename, uint32_t flags )
{
    int rv = -1;

#ifndef NOFILEUTILS

    size_t fsize = 0;
    if( !filename ) return -1;
    fsize = sfs_get_file_size( filename );
    if( fsize == 0 ) return -1;

#ifdef OS_IOS
    rv = ios_sundog_copy( s, filename, flags );
    return rv;
#endif
#ifdef OS_MACOS
    rv = macos_sundog_copy( s, filename, flags );
    return rv;
#endif

    char* fdata = SMEM_ALLOC2( char, fsize + 1 );
    if( fdata )
    {
        sfs_file f = sfs_open( filename, "rb" );
        if( f )
        {
    	    sfs_read( fdata, 1, fsize, f );
	    sfs_close( f );
	    fdata[ fsize ] = 0;
	}
	else
	{
	    smem_free( fdata );
	    fdata = NULL;
	}
    }
    if( fdata )
    {

#ifdef OS_ANDROID
	android_sundog_clipboard_copy( s, fdata );
#endif

#ifdef OS_WIN
	sfs_file_fmt fmt = sfs_get_file_format( filename, 0 );
	int ctype = sfs_get_clipboard_type( fmt );
        if( ctype < 0 ) ctype = sclipboard_type_utf8_text;
	UINT format = 0;
	HANDLE h = NULL;
	switch( ctype )
	{
	    case sclipboard_type_utf8_text:
	    {
		uint16_t* tmp = SMEM_ALLOC2( uint16_t, fsize + 1 );
		if( !tmp ) break;
		utf8_to_utf16( tmp, fsize + 1, fdata );
		size_t tmp_len = smem_strlen_utf16( tmp );

		format = CF_UNICODETEXT;
		h = GlobalAlloc( GMEM_MOVEABLE, ( tmp_len + 1 ) * sizeof( uint16_t ) );
		if( h )
		{
		    uint16_t* dest = (uint16_t*)GlobalLock( h );
		    if( dest )
		    {
			memcpy( dest, tmp, ( tmp_len + 1 ) * sizeof( uint16_t ) );
			GlobalUnlock( h );
		    }
		    else h = NULL;
		}
		break;
	    }
	    default:
		if( fmt == SFS_FILE_FMT_WAVE )
		{
		    format = CF_WAVE;
		    h = GlobalAlloc( GMEM_MOVEABLE, fsize );
		    if( h )
		    {
			void* dest = (void*)GlobalLock( h );
			if( dest )
			{
			    memcpy( dest, fdata, fsize );
			    GlobalUnlock( h );
			}
			else h = NULL;
		    }
		}
		slog( "Clipboard copy: file format %s is not supported\n", sfs_get_mime_type( fmt ) );
		break;
	}
	if( h && format && OpenClipboard( NULL ) )
	{
	    EmptyClipboard();
	    SetClipboardData( format, h );
	    CloseClipboard();
	    rv = 0;
	}
#endif

#ifdef X11
	sfs_file_fmt fmt = sfs_get_file_format( filename, 0 );
	int ctype = sfs_get_clipboard_type( fmt );
        if( ctype < 0 ) ctype = sclipboard_type_utf8_text;
        if( ctype == sclipboard_type_utf8_text )
        {
    	    int l = strlen( fdata );
    	    fdata = SMEM_RESIZE2( fdata, char, l );
        }
	if( x11_clipboard_thread_init( s ) == 0 )
	{
	    smutex_lock( &g_x11_clipboard_mutex );
	    smem_free( g_x11_clipboard_data );
	    g_x11_clipboard_data = fdata;
	    fdata = nullptr;
	    uint64_t v = X11_CLIPBOARD_EVT_COPY;
	    write( g_x11_clipboard_efd, &v, sizeof( v ) );
	    smutex_unlock( &g_x11_clipboard_mutex );
	    rv = 0;
	}
	/*window_manager* wm = &s->wm;
	if( wm->dpy )
	{
	    sfs_file f = sfs_open( "3:/sundog_clipboard", "wb" );
	    if( f )
	    {
		sfs_write( fdata, 1, fsize, f );
		sfs_close( f );
	    }
	    XSetSelectionOwner( wm->dpy, wm->sel_atom_clipboard, wm->win, CurrentTime );
	    rv = 0;
	}*/
#endif

#if defined(SDL) && !defined(SDL12)
	if( rv != 0 && fdata )
	{
	    SDL_SetClipboardText( (const char*)fdata );
	    rv = 0;
	}
#endif

	smem_free( fdata );
    }

#endif //... !NOFILEUTILS

    return rv;
}

char* sclipboard_paste( sundog_engine* s, int type, uint32_t flags )
{
    char* rv = NULL;

#ifndef NOFILEUTILS

#ifdef OS_APPLE
#ifdef OS_IOS
    rv = ios_sundog_paste( s, type, flags );
#else
    rv = macos_sundog_paste( s, type, flags );
#endif
    if( rv )
    {
	char* name = SMEM_STRDUP( rv );
	free( rv );
	rv = name;
    }
#endif

#ifdef OS_ANDROID
    char* txt = android_sundog_clipboard_paste( s );
    if( txt )
    {
	rv = SMEM_STRDUP( "3:/sundog_clipboard" );
    	sfs_file f = sfs_open( rv, "wb" );
	if( f )
	{
	    sfs_write( txt, 1, smem_strlen( txt ), f );
    	    sfs_close( f );
	}
	free( txt );
    }
#endif

#ifdef OS_WIN
    UINT format = 0;
    if( type == sclipboard_type_audio || type == sclipboard_type_av ) format = CF_WAVE;
    if( type == sclipboard_type_utf8_text ) format = CF_UNICODETEXT;
    if( format && IsClipboardFormatAvailable( format ) )
    {
	if( OpenClipboard( NULL ) )
	{
	    HANDLE h = GetClipboardData( format );
	    if( h )
	    {
		size_t size = GlobalSize( h );
		if( size )
		{
		    void* data = (void*)GlobalLock( h );
		    if( data )
		    {
			rv = SMEM_STRDUP( "3:/sundog_clipboard" );
			if( format == CF_UNICODETEXT )
			{
			    char* str = SMEM_ALLOC2( char, size * 2 );
			    if( str )
			    {
				utf16_to_utf8( str, size*2, (const uint16_t*)data );
				sfs_file f = sfs_open( rv, "wb" );
				if( f )
				{
			    	    sfs_write( str, 1, strlen( str ), f );
				    sfs_close( f );
				}
				smem_free( str );
			    }
			}
			else
			{
			    sfs_file f = sfs_open( rv, "wb" );
			    if( f )
			    {
				sfs_write( data, 1, size, f );
				sfs_close( f );
			    }
			}
			GlobalUnlock( h );
		    }
		}
	    }
	    CloseClipboard();
	}
    }
#endif

#ifdef X11
    if( x11_clipboard_thread_init( s ) == 0 )
    {
        atomic_store( &g_x11_clipboard_paste_finished, 0 );
	uint64_t v = X11_CLIPBOARD_EVT_PASTE;
	write( g_x11_clipboard_efd, &v, sizeof( v ) );
	bool ready = true;
	STIME_WAIT_FOR( atomic_load( &g_x11_clipboard_paste_finished ) != 0, 1000, 1, { ready = false; } );
	if( ready )
	{
	    smutex_lock( &g_x11_clipboard_mutex );
	    rv = SMEM_STRDUP( "3:/sundog_clipboard" );
	    sfs_file f = sfs_open( rv, "wb" );
	    if( f )
	    {
		sfs_write( g_x11_clipboard_data, 1, smem_get_size( g_x11_clipboard_data ), f );
		sfs_close( f );
	    }
	    //smem_free( g_x11_clipboard_data );
	    //g_x11_clipboard_data = nullptr;
	    smutex_unlock( &g_x11_clipboard_mutex );
	}
    }
    /*window_manager* wm = &s->wm;
    if( wm->dpy && wm->paste_lock == 0 )
    {
	wm->paste_lock++;
	XConvertSelection( wm->dpy, wm->sel_atom_clipboard, wm->sel_atom_utf8_string, wm->sel_atom_xsel_data, wm->win, CurrentTime );
	bool main_thread = false;
	if( pthread_equal( pthread_self(), wm->main_loop_thread ) ) main_thread = true;
	smbox_msg* msg = NULL;
	int step = 5; //ms
	int timeout = 1000; //ms
	for( int t = 0; t < timeout; t += step )
	{
	    if( main_thread )
	    {
    		sundog_event evt;
	        EVENT_LOOP_BEGIN( &evt, wm );
	        EVENT_LOOP_END( wm );
	        //if( EVENT_LOOP_END( wm ) ) break;
	    }
	    msg = smbox_get( wm->sd->mb, g_msg_id_x11_paste, step );
	    if( msg )
	    {
		rv = SMEM_STRDUP( "3:/sundog_clipboard" );
		sfs_file f = sfs_open( rv, "wb" );
		if( f )
		{
		    sfs_write( msg->data, 1, smem_get_size( msg->data ), f );
		    sfs_close( f );
		}
		smbox_remove_msg( msg );
		break;
	    }
	}
	wm->paste_lock--;
    }*/
#endif

#if defined(SDL) && !defined(SDL12)
    if( rv == NULL )
    {
	char* txt = SDL_GetClipboardText();
	if( txt )
	{
    	    rv = SMEM_STRDUP( "3:/sundog_clipboard" );
    	    sfs_file f = sfs_open( rv, "wb" );
	    if( f )
	    {
		sfs_write( txt, 1, smem_strlen( txt ), f );
    		sfs_close( f );
	    }
	    SDL_free( txt );
	}
    }
#endif

#endif //... !NOFILEUTILS

    return rv;
}

//
// URL
//

void open_url( sundog_engine* s, const char* url )
{
#ifndef NOFILEUTILS
#ifdef OS_LINUX
    char* ts = SMEM_ALLOC2( char, smem_strlen( url ) + 256 );
    if( ts )
    {
	sprintf( ts, "xdg-open \"%s\" &", url );
	if( system( ts ) != 0 )
	{
	    sprintf( ts, "sensible-browser \"%s\" &", url );
	    system( ts );
	}
	smem_free( ts );
    }
#endif
#ifdef OS_WIN
    ShellExecute( NULL, "open", url, NULL, NULL, SW_SHOWNORMAL );
#endif
#ifdef OS_MACOS
    CFURLRef u = CFURLCreateWithBytes (
	NULL,                   // allocator
        (UInt8*)url,     	// URLBytes
        smem_strlen( url ),      // length
        kCFStringEncodingASCII, // encoding
        NULL                    // baseURL
    );
    LSOpenCFURLRef( u, 0 );
    CFRelease( u );
#endif
#ifdef OS_IOS
    ios_sundog_open_url( s, url );
#endif
#ifdef OS_ANDROID
#ifndef NOMAIN
    android_sundog_open_url( s, url );
#endif
#endif
#endif
}

//
// Export / Import functions provided by the system
//

int send_file_to_gallery( sundog_engine* s, const char* filename )
{
    int rv = -1;
#ifndef NOFILEUTILS
    char* real_filename = sfs_make_filename( s, filename, true );
#ifdef OS_ANDROID
    android_sundog_send_file_to_gallery( s, real_filename );
#endif
#ifdef OS_IOS
    ios_sundog_send_file_to_gallery( s, real_filename );
    rv = 0;
#endif
    smem_free( real_filename );
#endif
    return rv;
}

void send_text_to_email( sundog_engine* s, const char* email, const char* subj, const char* body )
{
#ifdef CAN_SEND_TO_EMAIL
#ifdef OS_IOS
    ios_sundog_send_text_to_email( s, email, subj, body );
#endif
#endif
}

int export_import_file( sundog_engine* s, const char* filename, uint32_t flags ) //non-blocking! import through the EVT_LOADSTATE
{
    int rv = -1;
#if defined(CAN_IMPORT) || defined(CAN_EXPORT) || defined(CAN_EXPORT2)
    char* real_filename = sfs_make_filename( s, filename, true );
#ifdef OS_IOS
    rv = ios_sundog_export_import_file( s, real_filename, flags );
#endif
    smem_free( real_filename );
#endif
    return -1;
}

//
// Random generators
//

//Simple linear congruential generator:
uint32_t g_rand_next = 1;
void set_pseudo_random_seed( uint32_t seed )
{
    g_rand_next = seed;
}
uint32_t pseudo_random()
{
    g_rand_next = g_rand_next * 1103515245 + 12345;
    return ( (uint32_t)( g_rand_next / 65536 ) % 32768 );
}
uint32_t pseudo_random( uint32_t* seed )
{
    *seed = *seed * 1103515245 + 12345;
    return ( (uint32_t)( *seed / 65536 ) % 32768 );
}

//splitmix64:
uint64_t splitmix64( uint64_t* state )
{
    uint64_t res = ( *state += 0x9E3779B97F4A7C15ull );
    res = ( res ^ ( res >> 30 ) ) * 0xBF58476D1CE4E5B9ull;
    res = ( res ^ ( res >> 27 ) ) * 0x94D049BB133111EBull;
    return res ^ ( res >> 31 );
}

//xoshiro256**:
static inline uint64_t rol64( uint64_t x, int k )
{
    return ( x << k ) | ( x >> ( 64 - k ) );
}
uint64_t xoshiro256ss_next( xoshiro256ss_state* state )
{
    uint64_t* s = state->s;
    uint64_t const res = rol64( s[1] * 5, 7 ) * 9;
    uint64_t const t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rol64( s[3], 45 );
    return res;
}
void xoshiro256ss_seed( xoshiro256ss_state* state, uint64_t seed )
{
    uint64_t* s = state->s;
    uint64_t smstate = seed;
    s[0] = splitmix64(&smstate);
    s[1] = splitmix64(&smstate);
    s[2] = splitmix64(&smstate);
    s[3] = splitmix64(&smstate);
}

//
// 3D transformation matrix operations
//

void matrix_4x4_reset( float* m )
{
    memset( m, 0, sizeof( float ) * 4 * 4 );
    m[ 0 ] = 1;
    m[ 4 + 1 ] = 1;
    m[ 8 + 2 ] = 1;
    m[ 12 + 3 ] = 1;
}

void matrix_4x4_mul( float* RESTRICT res, const float* RESTRICT m1, const float* RESTRICT m2 )
{
    int res_ptr = 0;
    for( int x = 0; x < 4; x++ )
    {
        for( int y = 0; y < 4; y++ )
        {
            res[ res_ptr ] = 0;
            for( int k = 0; k < 4; k++ )
            {
                res[ res_ptr ] += m1[ y + k * 4 ] * m2[ x * 4 + k ];
            }
            res_ptr++;
        }
    }
}

void matrix_4x4_rotate( float angle, float x, float y, float z, float* m )
{
    angle /= 180;
    angle *= M_PI;

    //Normalize vector:
    float inv_length = 1.0f / sqrt( x * x + y * y + z * z );
    x *= inv_length;
    y *= inv_length;
    z *= inv_length;

    //Create the matrix:
    float c = cos( angle );
    float s = sin( angle );
    float cc = 1 - c;
    float r[ 4 * 4 ];
    float res[ 4 * 4 ];
    r[ 0 + 0 ] = x * x * cc + c;
    r[ 4 + 0 ] = x * y * cc - z * s;
    r[ 8 + 0 ] = x * z * cc + y * s;
    r[ 12 + 0 ] = 0;
    r[ 0 + 1 ] = y * x * cc + z * s;
    r[ 4 + 1 ] = y * y * cc + c;
    r[ 8 + 1 ] = y * z * cc - x * s;
    r[ 12 + 1 ] = 0;
    r[ 0 + 2 ] = x * z * cc - y * s;
    r[ 4 + 2 ] = y * z * cc + x * s;
    r[ 8 + 2 ] = z * z * cc + c;
    r[ 12 + 2 ] = 0;
    r[ 0 + 3 ] = 0;
    r[ 4 + 3 ] = 0;
    r[ 8 + 3 ] = 0;
    r[ 12 + 3 ] = 1;
    matrix_4x4_mul( res, m, r );
    memcpy( m, res, sizeof( float ) * 4 * 4 );
}

void matrix_4x4_translate( float x, float y, float z, float* m )
{
    float m2[ 4 * 4 ];
    float res[ 4 * 4 ];
    memset( m2, 0, sizeof( float ) * 4 * 4 );
    m2[ 0 ] = 1;
    m2[ 4 + 1 ] = 1;
    m2[ 8 + 2 ] = 1;
    m2[ 12 + 0 ] = x;
    m2[ 12 + 1 ] = y;
    m2[ 12 + 2 ] = z;
    m2[ 12 + 3 ] = 1;

    matrix_4x4_mul( res, m, m2 );
    memcpy( m, res, sizeof( float ) * 4 * 4 );
}

void matrix_4x4_scale( float x, float y, float z, float* m )
{
    float m2[ 4 * 4 ];
    float res[ 4 * 4 ];
    memset( m2, 0, sizeof( float ) * 4 * 4 );
    m2[ 0 ] = x;
    m2[ 4 + 1 ] = y;
    m2[ 8 + 2 ] = z;
    m2[ 12 + 3 ] = 1;

    matrix_4x4_mul( res, m, m2 );
    memcpy( m, res, sizeof( float ) * 4 * 4 );
}

void matrix_4x4_ortho( float left, float right, float bottom, float top, float z_near, float z_far, float* m )
{
    float m2[ 4 * 4 ];
    float res[ 4 * 4 ];
    memset( m2, 0, sizeof( float ) * 4 * 4 );
    m2[ 0 ] = 2 / ( right - left );
    m2[ 4 + 1 ] = 2 / ( top - bottom );
    m2[ 8 + 2 ] = -2 / ( z_far - z_near );
    m2[ 12 + 0 ] = - ( right + left ) / ( right - left );
    m2[ 12 + 1 ] = - ( top + bottom ) / ( top - bottom );
    m2[ 12 + 2 ] = - ( z_far + z_near ) / ( z_far - z_near );
    m2[ 12 + 3 ] = 1;

    matrix_4x4_mul( res, m, m2 );
    memcpy( m, res, sizeof( float ) * 4 * 4 );
}

//
// Image / color / 2D arrays
//

uint8_t g_simage_pixel_format_size[ PFMT_CNT ] = 
{
    1, //PFMT_GRAYSCALE8_SRGB
    4, //PFMT_R8G8B8A8_SRGB
#ifndef SUNDOG_VER
    sizeof( COLOR ) //PFMT_SUNDOG_COLOR
#endif
};

#define ROTATE_2D_ARRAY( type ) \
    { \
	type* data = (type*)ptr[ 0 ]; \
        type* new_data = (type*)new_data_v; \
        size_t p1 = 0; \
        for( int y = 0; y < ysize; y++ ) \
        { \
            size_t p22 = p2; \
            for( int x = 0; x < xsize; x++ ) \
            { \
        	new_data[ p22 ] = data[ p1 ]; \
                p22 += p2_xstep; \
                p1++; \
            } \
            p2 += p2_ystep; \
        } \
    }

#define ROTATE_2D_ARRAY2( type ) \
    { \
	type* data = (type*)ptr[ 0 ]; \
	type* new_data; \
	if( save_to ) \
	    new_data = (type*)save_to; \
	else \
	    new_data = (type*)ptr[ 0 ]; \
        for( size_t p1 = 0, p2 = xsize * ysize - 1; p1 < ( (size_t)xsize * ysize ) / 2; p1++, p2-- ) \
        { \
    	    type temp = data[ p1 ]; new_data[ p1 ] = data[ p2 ]; new_data[ p2 ] = temp; \
    	} \
    }

//angle*90 degrees (clockwise (по часовой стрелке))
//if save_to == nullptr: the original buffer may be recreated (*ptr will be changed)
int rotate_2d_array( void** ptr, int& xsize, int& ysize, int bytes_per_pixel, int angle, void* save_to )
{
    int rv = SD_RES_SUCCESS;
    if( *ptr == nullptr ) return SD_RES_ERR;
    angle &= 3;
    switch( angle )
    {
	case 0:
	    break;
	case 1:
	case 3:
	    {
		size_t p2;
		int p2_xstep;
		int p2_ystep;
		if( angle == 1 )
		{
		    p2 = ysize - 1;
		    p2_xstep = ysize;
		    p2_ystep = -1;
		}
		else
		{
		    p2 = ( xsize * ysize ) - ysize;
		    p2_xstep = -ysize;
		    p2_ystep = 1;
		}
		void* new_data_v;
		if( save_to )
		    new_data_v = save_to;
		else
		    new_data_v = SMEM_ALLOC( xsize * ysize * bytes_per_pixel );
		if( !new_data_v ) return SD_RES_ERR;
		switch( bytes_per_pixel )
            	{
            	    case 1: ROTATE_2D_ARRAY( int8_t ); break;
            	    case 2: ROTATE_2D_ARRAY( int16_t ); break;
            	    case 4: ROTATE_2D_ARRAY( int32_t ); break;
            	    case 8: ROTATE_2D_ARRAY( int64_t ); break;
            	    default:
            		rv = SD_RES_ERR;
            		break;
                }
                if( !save_to )
                {
            	    smem_free( ptr[ 0 ] );
            	    ptr[ 0 ] = new_data_v;
            	}
                int temp_xsize = xsize;
                xsize = ysize;
                ysize = temp_xsize;
	    }
	    break;
	case 2:
	    switch( bytes_per_pixel )
    	    {
            	case 1: ROTATE_2D_ARRAY2( int8_t ); break;
            	case 2: ROTATE_2D_ARRAY2( int16_t ); break;
            	case 4: ROTATE_2D_ARRAY2( int32_t ); break;
            	case 8: ROTATE_2D_ARRAY2( int64_t ); break;
            	default:
            	    rv = SD_RES_ERR;
            	    break;
            }
    }
    return rv;
}

void vflip_2d_array( void* ptr, size_t xsize, size_t ysize, int bytes_per_pixel )
{
    int8_t* src = (int8_t*)ptr;
    size_t line_size = xsize * bytes_per_pixel;
    int8_t* dest = (int8_t*)ptr + (ysize-1)*line_size;
    for( size_t y = 0; y < ysize / 2; y++ )
    {
	for( size_t x = 0; x < line_size; x++ )
	{
	    int8_t tmp = src[ x ];
	    src[ x ] = dest[ x ];
	    dest[ x ] = tmp;
	}
	src += line_size;
	dest -= line_size;
    }
}

#define HFLIP_LINE( type ) \
    { \
	type* data = (type*)p; \
	for( size_t x = 0; x < xsize / 2; x++ ) \
	{ \
	    type tmp = data[ x ]; data[ x ] = data[ xsize - 1 - x ]; data[ xsize - 1 - x ] = tmp; \
	} \
    }

void hflip_2d_array( void* ptr, size_t xsize, size_t ysize, int bytes_per_pixel )
{
    int8_t* p = (int8_t*)ptr;
    size_t line_size = xsize * bytes_per_pixel;
    for( size_t y = 0; y < ysize; y++ )
    {
	switch( bytes_per_pixel )
	{
	    case 1: HFLIP_LINE( int8_t ); break;
	    case 2: HFLIP_LINE( int16_t ); break;
	    case 4: HFLIP_LINE( int32_t ); break;
	    case 8: HFLIP_LINE( int64_t ); break;
	}
	p += line_size;
    }
}

//
// Misc
//

size_t round_to_power_of_two( size_t v )
{
    size_t b = 1;
    while( 1 )
    {
        if( b >= v )
        {
	    break;
	}
	b <<= 1;
    }
    return b;
}

uint sqrt_newton( uint l )
{
    uint temp, div; 
    uint rslt = l; 
    
    if( l <= 0 ) return 0;
    
    if( l & 0xFFFF0000 )
    {
	if( l & 0xFF000000L )
	    div = 0x3FFF; 
	else
	    div = 0x3FF; 
    }
    else 
    {
    	if( l & 0x0FF00 ) 
    	    div = 0x3F; 
	else
	    div = ( l > 4 ) ? 0x7 : l; 
    }
    
    while( l )
    {
	temp = l / div + div; 
	div = temp >> 1; 
	div += temp & 1; 
	if( rslt > div ) 
	{
	    rslt = (unsigned)div; 
	}
	else 
	{
	    if( l / rslt == rslt - 1 && l % rslt == 0 )
		rslt--; 
            return rslt; 
        }
    }
    
    return 0;
}

//Scale and check the inverse transformation (result * max / new_max = v)
int scale_check( int v, int max, int new_max )
{
    int v2 = div_round( v * new_max, max );
    if( new_max > max )
    {
	//new resolution is higher:
        if( v2 * max / new_max < v ) v2++;
    }
    return v2;
}
//scale_check() test:
/*for( int i = 0; i < 50000000; i++ )
{
    int v1, grid1;
    int v2, grid2;
    v1 = pseudo_random(); grid1 = pseudo_random() + 1; grid2 = pseudo_random() + 2;
    if( grid2 < grid1 ) continue;
    v2 = scale_check( v1, grid1, grid2 );
    int new_v1 = v2 * grid1 / grid2;
    if( new_v1 != v1 )
    {
        printf( "!!! v1=%d; grid1=%d; grid2=%d; v2=%d; new_v1=%d;\n", v1, grid1, grid2, v2, new_v1 );
        break;
    }
}*/

//Divide and round to the nearest
//Example: translate the value from grid2 to grid1: grid1_val = ( grid1_size * grid2_val ) / grid2_size;
int div_round( int v1, int v2 )
{
    const uint fixed_point_bits = 8;
    bool neg = false;
    if( v1 < 0 ) { v1 = -v1; neg ^= true; }
    if( v2 < 0 ) { v2 = -v2; neg ^= true; }
    int64_t rv_fp = ( (int64_t)v1 << fixed_point_bits ) / v2;
    int rv = rv_fp >> fixed_point_bits;
    if( ( rv_fp & ( ( 1 << fixed_point_bits ) - 1 ) ) >= ( 1 << ( fixed_point_bits - 1 ) ) ) //rounding to the nearest
    {
	rv++;
    }
    if( neg ) rv = -rv;
    return rv;
}
