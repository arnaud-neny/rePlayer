/*
    log.cpp - log management (thread-safe)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_ANDROID
    #include <android/log.h>
#endif

#if defined(OS_WIN) || defined(OS_WINCE)
    #define USE_UTF16_FILENAME
#endif

static volatile bool g_slog_ready = false;
static smutex g_slog_mutex;
static char* g_slog_file = NULL;
#ifdef USE_UTF16_FILENAME
static uint16_t* g_slog_file_utf16 = NULL;
#endif
static int g_slog_file_size = 0;
static int g_slog_file_size_limit = 1024 * 1024 * 32;
static int g_slog_no_cout_counter = 0; //no logging to console
bool g_slog_no_cout = 0;
static int g_slog_no_fout_counter = 0; //no logging to file
bool g_slog_no_fout = 0;

int slog_global_init( const char* filename )
{
    g_slog_file = NULL;
    g_slog_no_cout_counter = 0;
    g_slog_no_fout_counter = 0;
    smutex_init( &g_slog_mutex, 0 );
    char* name = sfs_make_filename( NULL, filename, true );
    if( name )
    {
        g_slog_file = (char*)strdup( name ); //detach the name from the SunDog memory manager
        smem_free( name );
	sfs_remove_file( g_slog_file );
#ifdef USE_UTF16_FILENAME
	g_slog_file_utf16 = sfs_convert_filename_to_win_utf16( g_slog_file );
#endif
    }
    g_slog_ready = true;
    return 0;
}

int slog_global_deinit( void )
{
    if( g_slog_ready == false ) return 0;
    smutex_destroy( &g_slog_mutex );
    free( g_slog_file );
#ifdef USE_UTF16_FILENAME
    free( g_slog_file_utf16 );
#endif
    g_slog_file = 0;
    g_slog_ready = false;
    return 0;
}

void slog( const char* format, ... )
{
#ifdef NOLOG
    return;
#endif

    while( 1 )
    {
	va_list p;
	va_start( p, format );

	if( g_slog_no_cout_counter == 0 && g_slog_no_cout == 0 )
	{
#ifdef OS_ANDROID
	    __android_log_vprint( ANDROID_LOG_INFO, "native-activity", format, p );
#else
	    vprintf( format, p );
#endif
	}

	if( g_slog_file && g_slog_no_fout_counter == 0 && g_slog_no_fout == 0 )
	{
	    if( smutex_lock( &g_slog_mutex ) == 0 )
	    {
#ifdef USE_UTF16_FILENAME
		FILE* f = _wfopen( (const wchar_t*)g_slog_file_utf16, L"ab" );
#else
		FILE* f = fopen( g_slog_file, "ab" );
#endif
		if( f )
		{
		    va_start( p, format );
		    int w = vfprintf( f, format, p );
		    g_slog_file_size += w;
		    fclose( f );
		    if( g_slog_file_size > g_slog_file_size_limit )
		    {
			sfs_remove_file( g_slog_file );
			g_slog_file_size = 0;
		    }
		}
		smutex_unlock( &g_slog_mutex );
	    }
	}

	va_end( p );

	break;
    }
}

void slog_disable( bool console, bool file )
{
    if( g_slog_ready == false ) return;
    if( smutex_lock( &g_slog_mutex ) ) return;
    if( console ) g_slog_no_cout_counter++;
    if( file ) g_slog_no_fout_counter++;
    smutex_unlock( &g_slog_mutex );
}

void slog_enable( bool console, bool file )
{
    if( g_slog_ready == false ) return;
    if( smutex_lock( &g_slog_mutex ) ) return;
    if( console )
    {
	if( g_slog_no_cout_counter > 0 )
	    g_slog_no_cout_counter--;
    }
    if( file )
    {
	if( g_slog_no_fout_counter > 0 )
	    g_slog_no_fout_counter--;
    }
    smutex_unlock( &g_slog_mutex );
}

const char* slog_get_file( void )
{
    return g_slog_file;
}

char* slog_get_latest( sundog_engine* s, size_t size )
{
    size_t log_size = sfs_get_file_size( (const char*)g_slog_file );
    if( log_size == 0 ) return NULL;
    if( size > log_size ) size = log_size;
    char* rv = (char*)smem_new( size + 1 );
    if( rv == NULL ) return NULL;
    rv[ 0 ] = 0;
    sfs_file f = sfs_open( g_slog_file, "rb" );
    if( f )
    {
	if( log_size >= size )
	{
	    sfs_seek( f, log_size - size, SFS_SEEK_SET );
	    sfs_read( rv, 1, size, f );
	    rv[ size ] = 0;
	}
	else
	{
	    sfs_read( rv, 1, log_size, f );
	    rv[ log_size ] = 0;
	}
	sfs_close( f );
    }
    return rv;
}

void slog_show_error_report( sundog_engine* s )
{
#ifndef NOGUI
#ifdef OS_IOS
    char* log = slog_get_latest( s, 1024 * 1024 );
    if( log )
    {
	char ts[ 256 ];
	sprintf( ts, "%s ERROR REPORT", g_app_name );
	send_text_to_email( s, "nightradio@gmail.com", ts, log );
	smem_free( log );
    }
#endif
#endif
}
