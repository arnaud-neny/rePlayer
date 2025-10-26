/*
    file.cpp - file management (thread-safe open/close)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_UNIX
    #include <errno.h>
    #include <stdlib.h>
    #include <unistd.h> //for current dir
    #include <sys/stat.h> //mkdir
#ifndef OS_ANDROID
    #include <ftw.h>
#endif
#endif

#ifdef OS_ANDROID
    #include "main/android/sundog_bridge.h"
#endif

#if defined(OS_WIN) || defined(OS_WINCE)
#define WIN_MAKE_LONG_PATH( TYPE, NAME, LEN ) \
    if( LEN >= MAX_PATH ) \
    { \
        char* name_ = SMEM_ALLOC2( char, LEN + 4 ); \
        name_[ 0 ] = '\\'; \
        name_[ 1 ] = '\\'; \
        name_[ 2 ] = '?'; \
        name_[ 3 ] = '\\'; \
        smem_copy( name_ + 4, (void*)NAME, LEN ); \
        smem_free( (void*)NAME ); \
        NAME = (TYPE)name_; \
        LEN += 4; \
    }
#endif

#if HEAPSIZE <= 16
    #define VIRT_FILE_DATA_SIZE_STEP (8*1024)
#else
    #define VIRT_FILE_DATA_SIZE_STEP (32*1024)
#endif

sfs_fd_struct* g_sfs_fd[ SFS_MAX_DESCRIPTORS ] = { 0 };
smutex g_sfs_mutex;

//
// Local system disks and working directories:
//

bool g_sfs_cant_write_disk1 = false;
uint g_disk_count = 0; //Number of local disks
char disk_names[ DISKNAME_SIZE * MAX_DISKS ]; //Disk names (NULL-terminated strings: "C:/", "H:/", etc.)

uint sfs_get_disk_count() 
{
    return g_disk_count; 
}

const char* sfs_get_disk_name( uint n ) 
{ 
    if( n < sfs_get_disk_count() )
        return disk_names + ( DISKNAME_SIZE * n ); 
    else
	return "";
}

int sfs_get_disk_num( const char* path )
{
    int rv = -1;
    for( uint i = 0; i < g_disk_count; i++ )
    {
	const char* disk_name = sfs_get_disk_name( i );
	if( disk_name )
	{
	    int cc = 0;
	    while( 1 )
	    {
		char c1 = path[ cc ];
		char c2 = disk_name[ cc ];
		if( c1 > 0x60 && c1 < 0x7B ) c1 -= 0x20; //Make it capital
		if( c1 != c2 ) break;
		if( c1 == 0 || c2 == 0 ) break;
		cc++;
	    }
	    if( disk_name[ cc ] == 0 )
	    {
		rv = i;
		break;
	    }
	}
    }
    return rv;
}

#ifdef OS_WIN

void sfs_refresh_disks()
{
#if 0 // rePlayer begin
    char temp[ 512 ];
    int len, p, dp, tmp;
    g_disk_count = 0;
    len = GetLogicalDriveStrings( 512, temp );
    for( dp = 0, p = 0; p < len; p++ )
    {
	tmp = ( g_disk_count * DISKNAME_SIZE ) + dp;
	char c = temp[ p ];
	if( c == 92 ) c = '/';
	if( c > 0x60 && c < 0x7B )
	    c -= 0x20; //Make it capital
	disk_names[ tmp ] = c;
	if( temp[ p ] == 0 ) { g_disk_count++; dp = 0; continue; }
	dp++;
    }
#endif // rePlayer end
}

uint sfs_get_current_disk()
{
    int rv = 0;
#if 0 // rePlayer begin
    char cur_dir[ MAX_DIR_LEN ];
    GetCurrentDirectory( MAX_DIR_LEN, cur_dir );
    if( cur_dir[ 0 ] > 0x60 && cur_dir[ 0 ] < 0x7B )
	cur_dir[ 0 ] -= 0x20; //Make it capital
    int d = sfs_get_disk_num( cur_dir );
    if( d >= 0 ) rv = d;
#endif // rePlayer end
    return rv;
}

char g_current_path[ MAX_DIR_LEN ] = { 0 };
const char* sfs_get_work_path()
{
#if 0 // rePlayer begin
    if( g_sfs_cant_write_disk1 ) return sfs_get_conf_path();
    if( g_current_path[ 0 ] == 0 )
    {
	{
	    uint16_t* ts_utf16 = (uint16_t*)malloc( MAX_DIR_LEN * sizeof( uint16_t ) );
	    GetCurrentDirectoryW( MAX_DIR_LEN - 2, (wchar_t*)ts_utf16 );
	    utf16_to_utf8( g_current_path, MAX_DIR_LEN - 2, (const uint16_t*)ts_utf16 );
	    free( ts_utf16 );
	}
	size_t len = smem_strlen( g_current_path );
	for( size_t i = 0; i < len; i++ ) //Make "/mydir" from "\mydir"
    	    if( g_current_path[ i ] == 92 ) g_current_path[ i ] = '/';
	g_current_path[ len ] = '/';
	g_current_path[ len + 1 ] = 0;
    }
#endif // rePlayer end
    return g_current_path;
}

char g_user_path[ MAX_DIR_LEN ] = { 0 };
const char* sfs_get_conf_path()
{
#if 0 // rePlayer begin
    if( g_user_path[ 0 ] == 0 ) 
    {
	{
	    uint16_t* ts_utf16 = (uint16_t*)malloc( MAX_DIR_LEN * 2 * sizeof( uint16_t ) );
	    SHGetFolderPathW( NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, (wchar_t*)ts_utf16 );
	    utf16_to_utf8( g_user_path, MAX_DIR_LEN - 2, (const uint16_t*)ts_utf16 );
	    free( ts_utf16 );
	}
	size_t len = smem_strlen( g_user_path );
	for( size_t i = 0; i < len; i++ ) //Make "/mydir" from "\mydir"
	    if( g_user_path[ i ] == 92 ) g_user_path[ i ] = '/';
	g_user_path[ len ] = '/';
	g_user_path[ len + 1 ] = 0;
    }
#endif // rePlayer end
    return g_user_path;
}

char g_temp_path[ MAX_DIR_LEN ] = { 0 };
const char* sfs_get_temp_path()
{
#if 0 // rePlayer begin
    if( g_temp_path[ 0 ] == 0 )
    {
	{
	    uint16_t* ts_utf16 = (uint16_t*)malloc( MAX_DIR_LEN * sizeof( uint16_t ) );
	    GetTempPathW( MAX_DIR_LEN - 2, (wchar_t*)ts_utf16 );
	    utf16_to_utf8( g_temp_path, MAX_DIR_LEN - 2, (const uint16_t*)ts_utf16 );
	    free( ts_utf16 );
	}
	size_t len = smem_strlen( g_temp_path );
	for( size_t i = 0; i < len; i++ ) //Make "/mydir" from "\mydir"
	    if( g_temp_path[ i ] == 92 ) g_temp_path[ i ] = '/';
	g_temp_path[ len ] = '/';
	g_temp_path[ len + 1 ] = 0;
    }
#endif // rePlayer end
    return g_temp_path;
}

#endif //WIN

#ifdef OS_WINCE

void sfs_refresh_disks()
{
    g_disk_count = 1;
    disk_names[ 0 ] = '/';
    disk_names[ 1 ] = 0;
}

uint sfs_get_current_disk()
{
    return 0;
}

const char* sfs_get_work_path()
{
    return (char*)"";
}

char g_user_path[ MAX_DIR_LEN ] = { 0 };
const char* sfs_get_conf_path()
{
    if( g_user_path[ 0 ] == 0 )
    {
	{
	    uint16_t* ts_utf16 = (uint16_t*)malloc( MAX_DIR_LEN * 2 * sizeof( uint16_t ) );
	    SHGetSpecialFolderPath( NULL, (wchar_t*)ts_utf16, CSIDL_APPDATA, 1 );
	    utf16_to_utf8( g_user_path, MAX_DIR_LEN - 2, (const uint16_t*)ts_utf16 );
	    free( ts_utf16 );
	}
	size_t len = smem_strlen( g_user_path );
	for( size_t i = 0; i < len; i++ ) //Make "/mydir" from "\mydir"
	    if( g_user_path[ i ] == 92 ) g_user_path[ i ] = '/';
	g_user_path[ len ] = '/';
	g_user_path[ len + 1 ] = 0;
    }
    return g_user_path;
}

char g_temp_path[ MAX_DIR_LEN ] = { 0 };
const char* sfs_get_temp_path()
{
    if( g_temp_path[ 0 ] == 0 )
    {
	{
    	    uint16_t* ts_utf16 = (uint16_t*)malloc( MAX_DIR_LEN * sizeof( uint16_t ) );
	    GetTempPathW( MAX_DIR_LEN - 2, (wchar_t*)ts_utf16 );
	    utf16_to_utf8( g_temp_path, MAX_DIR_LEN - 2, (const uint16_t*)ts_utf16 );
	    free( ts_utf16 );
	}
	size_t len = smem_strlen( g_temp_path );
	for( size_t i = 0; i < len; i++ ) //Make "/mydir" from "\mydir"
    	    if( g_temp_path[ i ] == 92 ) g_temp_path[ i ] = '/';
	g_temp_path[ len ] = '/';
	g_temp_path[ len + 1 ] = 0;
    }
    return g_temp_path;
}

#endif //WINCE

#ifdef OS_UNIX

void sfs_refresh_disks()
{
    g_disk_count = 1;
    disk_names[ 0 ] = '/';
    disk_names[ 1 ] = 0;
}

uint sfs_get_current_disk()
{
    return 0;
}

#if !defined(OS_APPLE)

#ifdef OS_ANDROID

const char* sfs_get_work_path() { if( !g_android_files_ext_path ) return ""; else return g_android_files_ext_path; }
const char* sfs_get_conf_path() { if( !g_android_files_int_path ) return ""; else return g_android_files_int_path; }
const char* sfs_get_temp_path() { if( !g_android_cache_int_path ) return ""; else return g_android_cache_int_path; }

#else //Other *nix systems:

char g_current_path[ MAX_DIR_LEN ] = { 0 };
const char* sfs_get_work_path()
{
    if( g_sfs_cant_write_disk1 ) return sfs_get_conf_path();
    if( g_current_path[ 0 ] == 0 )
    {
	getcwd( g_current_path, MAX_DIR_LEN - 2 );
	size_t s = smem_strlen( g_current_path );
	g_current_path[ s ] = '/';
	g_current_path[ s + 1 ] = 0;
    }
    return g_current_path;
}

char g_user_path[ MAX_DIR_LEN ] = { 0 };
const char* sfs_get_conf_path()
{
    if( g_user_path[ 0 ] == 0 )
    {
	char* user_p = getenv( "HOME" );
	char* cfg_p = getenv( "XDG_CONFIG_HOME" );
	g_user_path[ 0 ] = 0;
	if( cfg_p )
	{
	    smem_strcat( g_user_path, sizeof( g_user_path ), (const char*)cfg_p );
	    smem_strcat( g_user_path, sizeof( g_user_path ), "/" );
	}
	else
	{
	    smem_strcat( g_user_path, sizeof( g_user_path ), (const char*)user_p );
	    smem_strcat( g_user_path, sizeof( g_user_path ), "/.config/" );
	}
	smem_strcat( g_user_path, sizeof( g_user_path ), g_app_name_short );
	int err = mkdir( g_user_path, S_IRWXU | S_IRWXG | S_IRWXO );
	if( err != 0 )
	{
	    if( errno == EEXIST ) err = 0;
	}
	if( err != 0 )
	{
	    //Can't create the new config folder for our app; use default HOME directory:
	    g_user_path[ 0 ] = 0;
	    smem_strcat( g_user_path, sizeof( g_user_path ), (const char*)user_p );
	    smem_strcat( g_user_path, sizeof( g_user_path ), "/" );
	}
	else
	{
	    smem_strcat( g_user_path, sizeof( g_user_path ), "/" );
	}
    }
    return g_user_path;
}

const char* sfs_get_temp_path()
{
    return "/tmp/";
}

#endif //!defined(OS_ANDROID)
#endif //!defined(OS_APPLE)
#endif //UNIX

//
// Main functions:
//

int sfs_global_init()
{
#if defined(OS_APPLE)
    apple_sfs_global_init();
#endif

    g_disk_count = 0;
    g_sfs_fd[ 0 ] = 0;

    smutex_init( &g_sfs_mutex, 0 );
    sfs_refresh_disks();
    smem_clear( g_sfs_fd, sizeof( void* ) * SFS_MAX_DESCRIPTORS );

#if 0// rePlayer: defined(OS_LINUX) || defined(OS_MACOS) || defined(OS_WIN)
    {
	g_sfs_cant_write_disk1 = 0;
	sfs_file f = sfs_open( "1:/file_write_test", "wb" );
	if( f )
	{
	    sfs_close( f );
	}
	else
	{
	    //1:/ is write protected
	    g_sfs_cant_write_disk1 = 1;
	}
	sfs_remove_file( "1:/file_write_test" );
    }
#endif

    sfs_fd_struct* fd = SMEM_ALLOC2( sfs_fd_struct, 1 );
    fd->filename = 0;
    fd->f = (void*)stdin;
    fd->type = SFS_FILE_NORMAL;
    g_sfs_fd[ SFS_STDIN - 1 ] = fd;
    fd = SMEM_ALLOC2( sfs_fd_struct, 1 );
    fd->filename = 0;
    fd->f = (void*)stdout;
    fd->type = SFS_FILE_NORMAL;
    g_sfs_fd[ SFS_STDOUT - 1 ] = fd;
    fd = SMEM_ALLOC2( sfs_fd_struct, 1 );
    fd->filename = 0;
    fd->f = (void*)stderr;
    fd->type = SFS_FILE_NORMAL;
    g_sfs_fd[ SFS_STDERR - 1 ] = fd;

    sfs_get_work_path();
    sfs_get_conf_path();
    sfs_get_temp_path();

    return 0;
}

int sfs_global_deinit()
{
    smutex_destroy( &g_sfs_mutex );

    smem_free( g_sfs_fd[ SFS_STDIN - 1 ] );
    smem_free( g_sfs_fd[ SFS_STDOUT - 1 ] );
    smem_free( g_sfs_fd[ SFS_STDERR - 1 ] );

#if defined(OS_APPLE)
    apple_sfs_global_deinit();
#endif

    return 0;
}

char* sfs_make_filename( sundog_engine* sd, const char* filename, bool expand )
{
    char* rv = NULL;
    if( filename == NULL ) return NULL;
    char* disk_path = NULL;
    int disk_path_alloc = 0; //1 - malloc/free; 2 - SMEM_ALLOC/smem_free;
    if( expand )
    {
	//1:/file -> virtual_disk_path/file
	if( smem_strlen( filename ) >= 3 && 
	    filename[ 0 ] > '0' && 
	    filename[ 0 ] <= '9' &&
	    filename[ 1 ] == ':' && 
	    filename[ 2 ] == '/' )
	{
	    //Add specific path to the name:
	    int n = filename[ 0 ] - '0';
	    switch( n )
	    {
		case 1: disk_path = (char*)sfs_get_work_path(); break;
		case 2: disk_path = (char*)sfs_get_conf_path(); break;
		case 3: disk_path = (char*)sfs_get_temp_path(); break;
		default:
#ifdef OS_ANDROID
#ifndef NOMAIN
		    if( n >= ADD_VIRT_DISK )
		    {
			disk_path = android_sundog_get_dir( sd, "external_files", n - ADD_VIRT_DISK + 1 );
			disk_path_alloc = 1;
		    }
#endif
#endif
		    break;
	    }
	    if( disk_path )
	    {
		rv = SMEM_ALLOC2( char, smem_strlen( disk_path ) + ( smem_strlen( filename ) - 3 ) + 1 );
		if( rv == NULL ) return NULL;
		rv[ 0 ] = 0;
		SMEM_STRCAT_D( rv, disk_path );
		SMEM_STRCAT_D( rv, filename + 3 );
		if( disk_path_alloc == 1 ) { free( disk_path ); disk_path = NULL; disk_path_alloc = 0; }
	    }
	}
    }
    else
    {
	//virtual_disk_path/file -> 1:/file
	const char* disk_path2 = NULL;
	char ts[ 8 ];
	for( int i = 0; i < 9; i++ )
	{
	    disk_path = NULL;
	    switch( i )
	    {
		case 0: disk_path = (char*)sfs_get_work_path(); disk_path2 = "1:/"; break;
		case 1: disk_path = (char*)sfs_get_conf_path(); disk_path2 = "2:/"; break;
		case 2: disk_path = (char*)sfs_get_temp_path(); disk_path2 = "3:/"; break;
		default:
#ifdef OS_ANDROID
#ifndef NOMAIN
		    disk_path = android_sundog_get_dir( sd, "external_files", i - (ADD_VIRT_DISK-1) + 1 );
		    if( disk_path )
		    {
			disk_path_alloc = 1;
			sprintf( ts, "%d:/", i + 1 );
			disk_path2 = (const char*)ts;
		    }
#endif
#endif
		    break;
	    }
	    if( !disk_path ) break;
	    if( smem_strstr( filename, disk_path ) == filename )
	    {
		size_t disk_path_len = smem_strlen( disk_path );
		rv = SMEM_ALLOC2( char, smem_strlen( disk_path2 ) + ( smem_strlen( filename ) - disk_path_len ) + 1 );
		if( rv == NULL ) return NULL;
		rv[ 0 ] = 0;
		SMEM_STRCAT_D( rv, disk_path2 );
		SMEM_STRCAT_D( rv, filename + disk_path_len );
		i = 128;
	    }
	    if( disk_path_alloc == 1 ) { free( disk_path ); disk_path = NULL; disk_path_alloc = 0; }
	}
    }
    if( rv == NULL ) rv = SMEM_STRDUP( filename );
    return rv;
}

#if defined(OS_WIN) || defined(OS_WINCE)
uint16_t* sfs_convert_filename_to_win_utf16( char* src )
{
    int len = smem_strlen( src ) + 1;
    char* tmp = (char*)malloc( len + 4 );
    tmp[ 0 ] = '\\';
    tmp[ 1 ] = '\\';
    tmp[ 2 ] = '?';
    tmp[ 3 ] = '\\';
    smem_copy( tmp + 4, src, len );
    int len2 = len + 128;
    uint16_t* rv = (uint16_t*)malloc( sizeof( uint16_t ) * len2 );
    utf8_to_utf16( rv, len2, tmp );
    utf16_unix_slash_to_windows( rv );
    free( tmp );
    return rv;
}
#endif

char* sfs_get_filename_path( const char* filename )
{
    char* rv = SMEM_STRDUP( filename );
    if( !rv ) return NULL;
    for( int i = (int)smem_strlen( rv ) - 1; i >= 0; i-- )
    {
        if( rv[ i ] == '/' )
        {
            rv[ i + 1 ] = 0;
            break;
        }
    }
    return rv;
}

const char* sfs_get_filename_without_dir( const char* filename )
{
    int p = (int)smem_strlen( filename ) - 1;
    while( p >= 0 )
    {
	char c = filename[ p ];
	if( c == '/' || c == '\\' ) break;
	p--;
    }
    p++;
    return filename + p;
}

const char* sfs_get_filename_extension( const char* filename )
{
    int p = (int)smem_strlen( filename ) - 1;
    while( p >= 0 )
    {
	char c = filename[ p ];
	if( c == '.' ) break;
	p--;
    }
    p++;
    return filename + p;
}

sfs_fd_type sfs_get_type( sfs_file f )
{
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	return g_sfs_fd[ f ]->type;
    }
    else 
    {
	return SFS_FILE_NORMAL;
    }
}

void* sfs_get_data( sfs_file f )
{
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	return g_sfs_fd[ f ]->virt_file_data;
    }
    else 
    {
	return nullptr;
    }
}

size_t sfs_get_data_size( sfs_file f )
{
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->type == SFS_FILE_IN_MEMORY )
	    return g_sfs_fd[ f ]->virt_file_size;
	else
	    return sfs_get_file_size( g_sfs_fd[ f ]->filename );
    }
    else 
    {
	return 0;
    }
}

void sfs_set_user_data( sfs_file f, size_t user_data )
{
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	g_sfs_fd[ f ]->user_data = user_data;
    }
}

size_t sfs_get_user_data( sfs_file f )
{
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	return g_sfs_fd[ f ]->user_data;
    }
    else 
    {
	return 0;
    }
}

sfs_file sfs_open_in_memory( sundog_engine* sd, void* data, size_t size )
{
    int a;

    smutex_lock( &g_sfs_mutex );

    for( a = 0; a < SFS_MAX_DESCRIPTORS; a++ ) if( g_sfs_fd[ a ] == 0 ) break;
    if( a == SFS_MAX_DESCRIPTORS ) 
    {
	smutex_unlock( &g_sfs_mutex );
	return 0; //No free descriptors
    }
    //Create new descriptor:
    g_sfs_fd[ a ] = SMEM_ALLOC2( sfs_fd_struct, 1 );

    smutex_unlock( &g_sfs_mutex );

    smem_clear( g_sfs_fd[ a ], sizeof( sfs_fd_struct ) );

    g_sfs_fd[ a ]->sd = sd;
    g_sfs_fd[ a ]->type = SFS_FILE_IN_MEMORY;
    g_sfs_fd[ a ]->virt_file_data = (int8_t*)data;
    if( data && size == 0 ) size = smem_get_size( data );
    g_sfs_fd[ a ]->virt_file_size = size;

    return a + 1;
}

static bool sfs_is_vfs( const char* filename )
{
    int p = 0;
    if( !filename ) return false;
    if( filename[ p ] && filename[ p ] == 'v' )
    {
	p++;
	if( filename[ p ] && filename[ p ] == 'f' )
	{
	    p++;
	    if( filename[ p ] && filename[ p ] == 's' )
	    {
		while( 1 )
		{
		    p++;
		    char c = filename[ p ];
		    if( !c ) break;
		    if( c == ':' )
		    {
			if( filename[ p + 1 ] && filename[ p + 1 ] == '/' )
			{
			    return true;
			}
		    }
		    if( c < '0' || c > '9' ) break;
		}
	    }
	}
    }
    return false;
}

//get packed filesystem descriptor; example: vfs4:/FILE -> 4
//filename2 = file path without "vfsX:/"
static int sfs_get_vfs( const char* filename, const char** filename2 ) 
{
    if( sfs_is_vfs( filename ) == false ) return 0;
    const char* dptr = smem_strstr( filename + 3, ":/" );
    if( dptr )
    {
	char num[ 16 ];
	size_t i = 0;
	for( i = 3; i < 15; i++ )
	{
	    char c = filename[ i ];
	    num[ i - 3 ] = c;
	    if( c == ':' ) break;
	}
	num[ i - 3 ] = 0;
	if( filename2 ) *filename2 = dptr + 2;
	return string_to_int( num );
    }
    return 0;
}

sfs_file sfs_open( sundog_engine* sd, const char* filename, const char* filemode )
{
    if( !filename ) return 0;

    int a;

    smutex_lock( &g_sfs_mutex );

    for( a = 0; a < SFS_MAX_DESCRIPTORS; a++ ) if( g_sfs_fd[ a ] == 0 ) break;
    if( a == SFS_MAX_DESCRIPTORS ) 
    {
	smutex_unlock( &g_sfs_mutex );
        return 0; //No free descriptors
    }
    //Create new descriptor:
    g_sfs_fd[ a ] = SMEM_ALLOC2( sfs_fd_struct, 1 );
    sfs_fd_struct* fd = g_sfs_fd[ a ];

    smutex_unlock( &g_sfs_mutex );

    smem_clear( fd, sizeof( sfs_fd_struct ) );

    fd->sd = sd;

    fd->filename = sfs_make_filename( sd, filename, true );
    if( !fd->filename ) 
    {
	fd->f = 0;
    }
    else
    {
	sfs_file packed_fs = 0;
	const char* filename2 = nullptr; //without disk name
	int vfs = sfs_get_vfs( fd->filename, &filename2 );
	if( vfs > 0 ) packed_fs = vfs;
	if( packed_fs )
	{
	    if( 1 )
	    {
		//File inside the TAR archive:
		char next_file[ 100 ];
		char temp[ 8 * 3 ];
		sfs_rewind( packed_fs );
		while( 1 )
		{
		    if( sfs_read( next_file, 1, 100, packed_fs ) != 100 ) break;
		    sfs_read( temp, 1, 8 * 3, packed_fs );
		    sfs_read( temp, 1, 12, packed_fs );
		    temp[ 12 ] = 0;
		    size_t filelen = 0;
		    for( int i = 0; i < 12; i++ )
		    {
			//Convert from OCT string to integer:
			if( temp[ i ] >= '0' && temp[ i ] <= '9' ) 
			{
			    filelen *= 8;
			    filelen += temp[ i ] - '0';
			}
		    }
		    sfs_seek( packed_fs, 376, 1 );
		    if( smem_strcmp( next_file, filename2 ) == 0 )
		    {
			//File found:
			fd->virt_file_data = SMEM_ALLOC2( int8_t, filelen );
			fd->virt_file_data_autofree = true;
			sfs_read( fd->virt_file_data, 1, filelen, packed_fs );
			fd->virt_file_size = filelen;
			fd->type = SFS_FILE_IN_MEMORY;
			break;
		    }
		    else
		    {
			if( filelen & 511 )
			    filelen += 512 - ( filelen & 511 );
			sfs_seek( packed_fs, filelen, 1 );
		    }
		}
	    }
	}
	if( fd->type == SFS_FILE_NORMAL )
	{
	    //Not the archive. Normal file:
#if defined(OS_UNIX)
	    fd->f = (void*)fopen( fd->filename, filemode );
#ifdef OS_UNIX
	    //if( fd->f == 0 )
		//slog( "Can't open file %s: %s\n", filename, strerror( errno ) );
#endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	    size_t len = smem_strlen( fd->filename ) + 1;
	    WIN_MAKE_LONG_PATH( char*, fd->filename, len );
	    uint16_t* ts = SMEM_ALLOC2( uint16_t, len );
	    uint16_t ts2[ 16 ];
	    utf8_to_utf16( ts, len, fd->filename );
	    utf8_to_utf16( ts2, 16, filemode );
	    utf16_unix_slash_to_windows( ts );
	    fd->f = (void*)_wfopen( (const wchar_t*)ts, (const wchar_t*)ts2 );
	    //if( fd->f == 0 )
		//slog( "Can't open file %s: %s\n", filename, strerror( errno ) );
	    smem_free( ts );
#endif
	}
    }
    if( fd->type == SFS_FILE_NORMAL && fd->f == 0 )
    {
	//File not found:
	sfs_close( a + 1 );
	a = -1;
    }

    return a + 1;
}

int sfs_close( sfs_file f )
{
    int retval = 0;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->filename ) smem_free( g_sfs_fd[ f ]->filename );
	if( g_sfs_fd[ f ]->f )
	{
	    //Close standard file:
	    retval = (int)fclose( (FILE*)g_sfs_fd[ f ]->f );
	}
	if( g_sfs_fd[ f ]->virt_file_data_autofree ) 
	    smem_free( g_sfs_fd[ f ]->virt_file_data );
	smem_free( g_sfs_fd[ f ] );
	g_sfs_fd[ f ] = 0;
    }
    return retval;
}

void sfs_rewind( sfs_file f )
{
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
	    fseek( (FILE*)g_sfs_fd[ f ]->f, 0, 0 );
	}
	else
	{
	    g_sfs_fd[ f ]->virt_file_ptr = 0;
	}
    }
}

int sfs_getc( sfs_file f )
{
    int retval = 0;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
	    retval = getc( (FILE*)g_sfs_fd[ f ]->f );
	}
	else
	{
	    if( g_sfs_fd[ f ]->virt_file_ptr < g_sfs_fd[ f ]->virt_file_size )
	    {
		retval = (int)( (uint8_t)g_sfs_fd[ f ]->virt_file_data[ g_sfs_fd[ f ]->virt_file_ptr ] );
		g_sfs_fd[ f ]->virt_file_ptr++;
	    }
	    else
	    {
		retval = -1;
	    }
	}
    }
    return retval;
}

int64_t sfs_tell( sfs_file f )
{
    int64_t retval = 0;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
#if defined(OS_WIN) && !defined(OS_WINCE)
	    retval = _ftelli64( (FILE*)g_sfs_fd[ f ]->f );
#else
	    retval = ftell( (FILE*)g_sfs_fd[ f ]->f );
#endif
	}
	else
	{
	    retval = g_sfs_fd[ f ]->virt_file_ptr;
	}
    }
    return retval;
}

int sfs_seek( sfs_file f, int64_t offset, int access )
{
    int retval = 0;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
#if defined(OS_WIN) && !defined(OS_WINCE)
	    retval = _fseeki64( (FILE*)g_sfs_fd[ f ]->f, offset, access );
#else
	    retval = fseek( (FILE*)g_sfs_fd[ f ]->f, offset, access );
#endif
	}
	else
	{
	    if( access == 0 ) g_sfs_fd[ f ]->virt_file_ptr = offset;
	    if( access == 1 ) g_sfs_fd[ f ]->virt_file_ptr += offset;
	    if( access == 2 ) g_sfs_fd[ f ]->virt_file_ptr = g_sfs_fd[ f ]->virt_file_size + offset;
	}
    }
    return retval;
}

int sfs_eof( sfs_file f )
{
    int retval = 0;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
	    retval = feof( (FILE*)g_sfs_fd[ f ]->f );
	}
	else
	{
	    if( g_sfs_fd[ f ]->virt_file_ptr >= g_sfs_fd[ f ]->virt_file_size ) retval = 1;
	}
    }
    return retval;
}

int sfs_flush( sfs_file f )
{
    int retval = 0;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
	    retval = fflush( (FILE*)g_sfs_fd[ f ]->f );
	}
    }
    return retval;
}

size_t sfs_read( void* ptr, size_t el_size, size_t elements, sfs_file f )
{
    size_t retval = 0;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
	    retval = fread( ptr, el_size, elements, (FILE*)g_sfs_fd[ f ]->f );
	}
	else
	{
	    if( g_sfs_fd[ f ]->virt_file_data )
	    {
		size_t size = el_size * elements;
		if( g_sfs_fd[ f ]->virt_file_ptr + size > g_sfs_fd[ f ]->virt_file_size )
		    size = g_sfs_fd[ f ]->virt_file_size - g_sfs_fd[ f ]->virt_file_ptr;
		if( (signed)size < 0 ) size = 0;
		if( (signed)size > 0 )
		    smem_copy( ptr, g_sfs_fd[ f ]->virt_file_data + g_sfs_fd[ f ]->virt_file_ptr, size );
		g_sfs_fd[ f ]->virt_file_ptr += size;
		retval = size / el_size;
	    }
	}
    }
    return retval;
}

size_t sfs_write( const void* ptr, size_t el_size, size_t elements, sfs_file f )
{
    size_t retval = 0;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
	    retval = fwrite( ptr, el_size, elements, (FILE*)g_sfs_fd[ f ]->f );
#ifndef OS_WINCE
	    if( retval != elements )
	    {
		slog( "fwrite( %d, %d, %d ) error: %s\n", (int)el_size, (int)elements, (int)f, strerror( errno ) );
	    }
#endif
	}
	else
	{
	    if( g_sfs_fd[ f ]->virt_file_data )
	    {
		size_t size = el_size * elements;
		size_t new_size = g_sfs_fd[ f ]->virt_file_ptr + size;
		if( new_size > g_sfs_fd[ f ]->virt_file_size )
		{
		    if( g_sfs_fd[ f ]->type == SFS_FILE_IN_MEMORY )
		    {
			size_t real_size = smem_get_size( g_sfs_fd[ f ]->virt_file_data );
			if( new_size > real_size ) g_sfs_fd[ f ]->virt_file_data = SMEM_RESIZE2( g_sfs_fd[ f ]->virt_file_data, int8_t, new_size + VIRT_FILE_DATA_SIZE_STEP );
			if( !g_sfs_fd[ f ]->virt_file_data )
			    size = 0;
			g_sfs_fd[ f ]->virt_file_size = new_size;
		    }
		    else 
		    {
			size = g_sfs_fd[ f ]->virt_file_size - g_sfs_fd[ f ]->virt_file_ptr;
		    }
		}
		if( (signed)size > 0 )
		    smem_copy( g_sfs_fd[ f ]->virt_file_data + g_sfs_fd[ f ]->virt_file_ptr, ptr, size );
		g_sfs_fd[ f ]->virt_file_ptr += size;
		retval = size / el_size;
	    }
	}
    }
    return retval;
}

int sfs_write_varlen_uint32( uint32_t v, sfs_file f )
{
    uint8_t buf[ 5 ];
    int buf_len = 0;
    buf_len = write_varlen_uint32( buf, v );
    int written = sfs_write( buf, 1, buf_len, f );
    if( written != buf_len ) return -written;
    return written;
}

uint32_t sfs_read_varlen_uint32( int* len, sfs_file f )
{
    uint32_t rv = 0;
    int offset = 0;
    int l = 0;
    while( 1 )
    {
        int v = sfs_getc( f );
        if( v < 0 ) { l = 0; break; }
        l++;
        rv |= (uint32_t)( v & 127 ) << offset;
        if( v <= 127 ) break;
        offset += 7;
    }
    if( len ) *len = l;
    return rv;
}

int sfs_putc( int val, sfs_file f )
{
    int rv = -1;
    f--;
    if( (unsigned)f < SFS_MAX_DESCRIPTORS && g_sfs_fd[ f ] )
    {
	if( g_sfs_fd[ f ]->f && g_sfs_fd[ f ]->type == SFS_FILE_NORMAL )
	{
	    //Standard file:
	    rv = fputc( val, (FILE*)g_sfs_fd[ f ]->f );
	}
	else
	{
	    if( g_sfs_fd[ f ]->virt_file_data )
	    {
		if( g_sfs_fd[ f ]->virt_file_ptr < g_sfs_fd[ f ]->virt_file_size )
		{
		    g_sfs_fd[ f ]->virt_file_data[ g_sfs_fd[ f ]->virt_file_ptr ] = (int8_t)val;
		    g_sfs_fd[ f ]->virt_file_ptr++;
		    rv = val;
		}
		else 
		{
		    if( g_sfs_fd[ f ]->type == SFS_FILE_IN_MEMORY )
		    {
			size_t new_size = g_sfs_fd[ f ]->virt_file_ptr + 1;
			size_t real_size = smem_get_size( g_sfs_fd[ f ]->virt_file_data );
			if( new_size > real_size ) g_sfs_fd[ f ]->virt_file_data = SMEM_RESIZE2( g_sfs_fd[ f ]->virt_file_data, int8_t, new_size + VIRT_FILE_DATA_SIZE_STEP );
			if( g_sfs_fd[ f ]->virt_file_data )
			{
			    g_sfs_fd[ f ]->virt_file_data[ g_sfs_fd[ f ]->virt_file_ptr ] = (int8_t)val;
			    g_sfs_fd[ f ]->virt_file_ptr++;
			    g_sfs_fd[ f ]->virt_file_size = new_size;
			    rv = val;
			}
		    }
		}
	    }
	}
    }
    return rv;
}

int sfs_remove( const char* filename ) //remove a file or directory
{
    {
	//Safe mode...
	int l = (int)strlen( filename );
	switch( l )
	{
	    case 0: return -1;
	    case 1:
		if( filename[ 0 ] == '.' ) return -1;
		if( filename[ 0 ] == '/' ) return -1;
		break;
	    case 2:
		if( filename[ 0 ] == '.' && filename[ 1 ] == '/' ) return -1;
	    case 3:
		if( filename[ 1 ] == ':' ) return -1;
		break;
	    default: break;
	}
    }
    const char* name = sfs_make_filename( NULL, filename, true );
    if( !name ) return -1;
    int retval = 0;
#if defined(OS_WIN) || defined(OS_WINCE)
    size_t len = smem_strlen( name ) + 1;
    WIN_MAKE_LONG_PATH( const char*, name, len );
    uint16_t* ts = SMEM_ALLOC2( uint16_t, len + 1 );
    utf8_to_utf16( ts, len, name );
    utf16_unix_slash_to_windows( ts );
    uint attr = GetFileAttributesW( (const WCHAR*)ts );
    if( attr == INVALID_FILE_ATTRIBUTES )
    {
	retval = 1;
    }
    else
    {
	if( attr & FILE_ATTRIBUTE_DIRECTORY )
	{
	    ts[ smem_strlen_utf16( ts ) + 1 ] = 0; //Double null terminated
	    SHFILEOPSTRUCTW file_op = {
		0,
		FO_DELETE,
		(const WCHAR*)ts,
		0,
		FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
    		0,
    		0,
    		0
	    };
	    retval = SHFileOperationW( &file_op );
	}
	else
	{
	    if( DeleteFileW( (const WCHAR*)ts ) == 0 )
	    {
		retval = 1;
	    }
	}
    }
    smem_free( ts );
#endif
#if defined(OS_UNIX)
    retval = remove( name );
    if( retval )
    {
	//Directory with files?
	sfs_find_struct fs;
	SMEM_CLEAR_STRUCT( fs );
	bool first;
	char* dir = SMEM_ALLOC2( char, MAX_DIR_LEN );
	fs.start_dir = filename;
	fs.mask = 0;
	first = true;
	while( 1 )
	{
    	    int v;
    	    if( first )
    	    {
        	v = sfs_find_first( &fs );
        	first = false;
    	    }
    	    else
    	    {
        	v = sfs_find_next( &fs );
    	    }
    	    if( v )
    	    {
        	if( strcmp( fs.name, "." ) && strcmp( fs.name, ".." ) )
        	{
                    sprintf( dir, "%s/%s", fs.start_dir, fs.name );
                    sfs_remove( dir );
                }
    	    }
    	    else break;
	}
	sfs_find_close( &fs );
	smem_free( dir );
	//... and try it again:
	retval = remove( name );
    }
#endif
    smem_free( (void*)name );
    return retval;
}

int sfs_remove_file( const char* filename )
{
    const char* name = sfs_make_filename( NULL, filename, true );
    if( !name ) return -1;
    int retval = 0;
#if defined(OS_WIN) || defined(OS_WINCE)
    size_t len = smem_strlen( name ) + 1;
    WIN_MAKE_LONG_PATH( const char*, name, len );
    uint16_t* ts = SMEM_ALLOC2( uint16_t, len + 1 );
    utf8_to_utf16( ts, len, name );
    utf16_unix_slash_to_windows( ts );
    if( DeleteFileW( (const WCHAR*)ts ) == 0 )
    {
	retval = 1;
    }
    smem_free( ts );
#endif
#if defined(OS_UNIX)
    retval = remove( name );
#endif
    smem_free( (void*)name );
    return retval;
}

int sfs_rename( const char* old_name, const char* new_name )
{
    int retval = 0;

    const char* name1 = sfs_make_filename( NULL, old_name, true );
    const char* name2 = sfs_make_filename( NULL, new_name, true );
    if( !name1 ) return -1;
    if( !name2 ) return -1;

#if defined(OS_UNIX)
    retval = rename( name1, name2 );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    size_t len1 = smem_strlen( name1 ) + 1;
    size_t len2 = smem_strlen( name2 ) + 1;
    WIN_MAKE_LONG_PATH( const char*, name1, len1 );
    WIN_MAKE_LONG_PATH( const char*, name2, len2 );
    uint16_t* ts1 = SMEM_ALLOC2( uint16_t, len1 );
    uint16_t* ts2 = SMEM_ALLOC2( uint16_t, len2 );
    if( ts1 && ts2 )
    {
        utf8_to_utf16( ts1, len1, name1 );
        utf8_to_utf16( ts2, len2, name2 );
        utf16_unix_slash_to_windows( ts1 );
        utf16_unix_slash_to_windows( ts2 );
        if( MoveFileW( (const WCHAR*)ts1, (const WCHAR*)ts2 ) != 0 )
            retval = 0; //No errors
        else
            retval = -1;
    }
    else retval = -1;
    smem_free( ts1 );
    smem_free( ts2 );
#endif
    
    smem_free( (void*)name1 );
    smem_free( (void*)name2 );
    
    return retval;
}

int sfs_mkdir( const char* pathname, uint mode )
{
    int retval = -1;
    const char* name = sfs_make_filename( NULL, pathname, true );
    if( !name ) return -1;
#ifdef OS_UNIX
    retval = mkdir( name, mode );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    size_t len = smem_strlen( name ) + 1;
    WIN_MAKE_LONG_PATH( const char*, name, len );
    uint16_t* ts = SMEM_ALLOC2( uint16_t, len );
    if( ts )
    {
        utf8_to_utf16( ts, len, name );
        utf16_unix_slash_to_windows( ts );
        if( CreateDirectoryW( (const WCHAR*)ts, 0 ) != 0 )
        {
            retval = 0; //No errors
        }
        else
        {
    	    slog( "Failed to create directory %s. Error code %d\n", name, GetLastError() );
            retval = -1;
        }
        smem_free( ts );
    }
    else retval = -1;
#endif
    smem_free( (void*)name );
    return retval;
}

#ifdef OS_UNIX
static int unix_get_file_info( const char* fname, struct stat* info )
{
    int rv = 0;
#ifdef OS_ANDROID
    uint8_t* raw_stat = (uint8_t*)info;
    raw_stat[ sizeof( struct stat ) - 1 ] = 98;
    raw_stat[ sizeof( struct stat ) - 2 ] = 76;
    raw_stat[ sizeof( struct stat ) - 3 ] = 54;
#endif
    rv = stat( fname, info );
#ifdef OS_ANDROID
    if( rv == 0 )
    {
        if( raw_stat[ sizeof( struct stat ) - 1 ] == 98 &&
    	    raw_stat[ sizeof( struct stat ) - 2 ] == 76 &&
	    raw_stat[ sizeof( struct stat ) - 3 ] == 54 )
	{
	    //Wrong stat size. Long Long (64bit) fields are not 8 bytes aligned.
	    uint* raw_stat32 = (uint*)info;
	    info->st_size = (size_t)raw_stat32[ 11 ]; //st_size from the packed (without alignment) structure
	}
    }
#endif
    return rv;
}
#endif

int64_t sfs_get_file_size( const char* filename, int* err )
{
    int64_t rv = 0;
    *err = 0;

    if( !filename ) { *err = SD_RES_ERR_FOPEN; return 0; }

    if( sfs_is_vfs( filename ) == false )
    {

#ifdef OS_UNIX
	char* name = sfs_make_filename( NULL, filename, true );
	if( !name ) return 0;
	struct stat file_info;
	if( unix_get_file_info( name, &file_info ) == 0 )
	{
    	    rv = file_info.st_size;
	}
	else
	{
    	    *err = SD_RES_ERR_FOPEN;
	}
	smem_free( name );
	return rv;
#endif

#if defined(OS_WIN) || defined(OS_WINCE)
	char* name = sfs_make_filename( NULL, filename, true );
	if( !name ) return 0;
	size_t len = smem_strlen( name ) + 1;
	WIN_MAKE_LONG_PATH( char*, name, len );
	uint16_t* ts = SMEM_ALLOC2( uint16_t, len );
	if( ts )
	{
	    utf8_to_utf16( ts, len, name );
    	    utf16_unix_slash_to_windows( ts );
    	    WIN32_FILE_ATTRIBUTE_DATA file_info;
	    if( GetFileAttributesExW( (LPCWSTR)ts, GetFileExInfoStandard, &file_info ) != 0 )
    	    {
		rv = file_info.nFileSizeLow | ( (int64_t)file_info.nFileSizeHigh << 32 );
    	    }
    	    else
    	    {
    		*err = SD_RES_ERR_FOPEN;
    	    }
	    smem_free( ts );
	}
	smem_free( name );
	return rv;
#endif

    }

    sfs_file f = sfs_open( filename, "rb" );
    if( f )
    {
        sfs_seek( f, 0, 2 );
        rv = sfs_tell( f );
        sfs_close( f );
    }
    else
    {
        *err = SD_RES_ERR_FOPEN;
    }

    return rv;
}

int sfs_copy_file( const char* dest, const char* src )
{
    int rv = -1;
    sfs_file f1 = sfs_open( src, "rb" );
    if( f1 )
    {
	sfs_file f2 = sfs_open( dest, "wb" );
        if( f2 )
        {
    	    int buf_size = 64 * 1024;
    	    void* buf = SMEM_ALLOC( buf_size );
            if( buf )
            {
                while( 1 )
                {
                    size_t r = sfs_read( buf, 1, buf_size, f1 );
                    if( r == 0 )
                        break;
                    else
                        sfs_write( buf, 1, r, f2 );
                }
                rv = 0;
                smem_free( buf );
            }
            sfs_close( f2 );
        }
        sfs_close( f1 );
    }
    return rv;
}

int sfs_copy_files( const char* dest, const char* src, const char* mask, const char* with_str_in_filename, bool move )
{
    int rv = 0;
    
    size_t dest_strlen = smem_strlen( dest );
    size_t src_strlen = smem_strlen( src );
    
    sfs_find_struct fs;
    SMEM_CLEAR_STRUCT( fs );

    fs.start_dir = src;
    fs.mask = mask;
    bool first = true;
    while( 1 )
    {
        int v;
        if( first )
        {
            v = sfs_find_first( &fs );
            first = false;
        }
        else
        {
            v = sfs_find_next( &fs );
        }
        if( v )
        {
            if( fs.type == 0 )
            {
		bool ignore = false;
		if( with_str_in_filename )
		{
		    if( smem_strstr( fs.name, with_str_in_filename ) == 0 ) ignore = true;
		}
		if( !ignore )
		{
		    rv++;
		    char* ts1 = SMEM_ALLOC2( char, src_strlen + smem_strlen( fs.name ) + 4 );
		    char* ts2 = SMEM_ALLOC2( char, dest_strlen + smem_strlen( fs.name ) + 4 );
		    if( ts1 && ts2 )
		    {
			sprintf( ts1, "%s%s", src, fs.name );
			sprintf( ts2, "%s%s", dest, fs.name );
			printf( "Copying %s to %s\n", ts1, ts2 );
			sfs_copy_file( ts2, ts1 );
			if( move ) sfs_remove_file( ts1 );
		    }
		    smem_free( ts1 );
		    smem_free( ts2 );
		}
            }
        }
        else break;
    }
    sfs_find_close( &fs );
    
    return rv;
}

void sfs_remove_support_files( const char* prefix )
{
    sfs_find_struct fs;
    SMEM_CLEAR_STRUCT( fs );
    bool first;
    int prefix_len = (int)strlen( prefix );
    char* dir = SMEM_ALLOC2( char, 8192 );
    char* ts = SMEM_ALLOC2( char, prefix_len + 8 );

    fs.start_dir = "2:/";
    fs.mask = 0;
    first = 1;
    while( 1 )
    {
        int v;
        if( first )
        {
            v = sfs_find_first( &fs );
            first = 0;
        }
        else
        {
            v = sfs_find_next( &fs );
        }
        if( v )
        {
            if( fs.type == 0 )
            {
        	int len = (int)strlen( fs.name );
        	int i = 0; for( i = 0; i < prefix_len && i < len; i++ ) ts[ i ] = fs.name[ i ];
        	ts[ i ] = 0;
                if( ( prefix_len && smem_strcmp( ts, prefix ) == 0 ) || smem_strcmp( ts, ".sundog_" ) == 0 )
                {
                    sprintf( dir, "%s%s", fs.start_dir, fs.name );
                    slog( "Removing %s\n", dir );
                    sfs_remove_file( dir );
                }
            }
        }
        else break;
    }
    sfs_find_close( &fs );

    smem_free( dir );
    smem_free( ts );
}

//
// Searching files:
//

static int check_file( char* name, sfs_find_struct* fs )
{
    int p;
    int mp; //mask pointer
    int equal = 0;
    int str_len;

    if( fs->mask ) mp = strlen( fs->mask ) - 1; else return 1;
    str_len = (int)strlen( name );
    if( str_len > 0 )
	p = str_len - 1;
    else
	return 0;

    for( ; p >= 0 ; p--, mp-- )
    {
	if( name[ p ] == '.' ) 
	{
	    if( equal ) return 1; //Yes, the file extension matches one of the types in mask[]
	    else 
	    {
		for(;;) 
		{ //Are there other file types (in mask[]) ?:
		    if( fs->mask[ mp ] == '/' ) break; //There is other type
		    mp--;
		    if( mp < 0 ) return 0; //no... it was the last type in mask[]
		}
	    }
	}
	if( mp < 0 ) return 0;
	if( fs->mask[ mp ] == '/' ) { mp--; p = str_len - 1; }
	char c = name[ p ];
	if( c >= 65 && c <= 90 ) c += 0x20; //Make small leters
	if( c != fs->mask[ mp ] ) 
	{
	    for(;;) 
	    { //Are there other file types (in mask[]) ?:
	        if( fs->mask[ mp ] == '/' ) { p = str_len; mp++; break; } //other type found
	        mp--;
	        if( mp < 0 ) return 0; //no... it was the last type in mask[]
	    }
	}
	else equal = 1;
    }

    return 0;
}

#if defined(OS_WIN) || defined(OS_WINCE)
//Windows:

int sfs_find_first( sfs_find_struct* fs )
{
    int wp = 0, p = 0;
    fs->win_mask[ 0 ] = 0;
    fs->win_start_dir = 0;

    fs->start_dir = sfs_make_filename( NULL, fs->start_dir, true );
    if( fs->start_dir == 0 ) return 0;

    size_t len = smem_strlen( fs->start_dir ) + 1;
    WIN_MAKE_LONG_PATH( const char*, fs->start_dir, len );

    //convert start dir from "dir/dir/" to "dir\dir\*.*"
    fs->win_start_dir = SMEM_ALLOC2( char, len + 16 ); 
    smem_copy( fs->win_start_dir, fs->start_dir, len );
    if( fs->win_start_dir[ len - 2 ] != '/' )
	strcat( fs->win_start_dir, "/" );
    strcat( fs->win_start_dir, "*.*" );
    for( wp = 0; ; wp++ ) 
    {
	if( fs->win_start_dir[ wp ] == 0 ) break;
	if( fs->win_start_dir[ wp ] == '/' ) fs->win_start_dir[ wp ] = 92;
    }
    len = smem_strlen( fs->win_start_dir ) + 1;

    //do it:
    uint16_t* ts = SMEM_ALLOC2( uint16_t, len );
    utf8_to_utf16( ts, len, fs->win_start_dir );
#ifdef OS_WINCE
    fs->find_handle = FindFirstFile( (const WCHAR*)ts, &fs->find_data );
#endif
#ifdef OS_WIN
    fs->find_handle = FindFirstFileW( (const WCHAR*)ts, &fs->find_data );
#endif
    smem_free( ts );
    if( fs->find_handle == INVALID_HANDLE_VALUE ) return 0; //no files found :(

    //save filename:
    fs->name[ 0 ] = 0;
    strcat( fs->name, utf16_to_utf8( fs->temp_name, MAX_DIR_LEN, (const uint16_t*)fs->find_data.cFileName ) );
    if( fs->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) 
    {
	//Dir:
	fs->type = SFS_DIR;
    }
    else
    {
	//File:
	fs->type = SFS_FILE;
	if( fs->opt & SFS_FIND_OPT_FILESIZE )
	    fs->size = ( (size_t)fs->find_data.nFileSizeHigh * (MAXDWORD+1) ) + fs->find_data.nFileSizeLow;
	if( !check_file( fs->name, fs ) ) return sfs_find_next( fs );
    }

    return 1;
}

int sfs_find_next( sfs_find_struct* fs )
{
    for(;;)
    {
	if( !FindNextFileW( fs->find_handle, &fs->find_data ) ) return 0; //files not found

	//save filename:
        fs->name[ 0 ] = 0;
	strcat( fs->name, utf16_to_utf8( fs->temp_name, MAX_DIR_LEN, (const uint16_t*)fs->find_data.cFileName ) );
	if( fs->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) 
	{
	    //Dir:
	    fs->type = SFS_DIR;
	    return 1;
	}
	else
	{
	    //File found.
	    fs->type = SFS_FILE;
	    //Is it our file?
	    if( check_file( fs->name, fs ) )
	    {
		if( fs->opt & SFS_FIND_OPT_FILESIZE )
		    fs->size = ( (size_t)fs->find_data.nFileSizeHigh * (MAXDWORD+1) ) + fs->find_data.nFileSizeLow;
		return 1;
	    }
	}
    }

    return 1;
}

void sfs_find_close( sfs_find_struct* fs )
{
    FindClose( fs->find_handle );

    smem_free( (void*)fs->start_dir );
    smem_free( fs->win_start_dir );
    fs->start_dir = 0;
    fs->win_start_dir = 0;
}

#endif

#ifdef OS_UNIX
//UNIX:

int sfs_find_first( sfs_find_struct* fs )
{
    DIR* test;
    char test_dir[ MAX_DIR_LEN ];

    fs->start_dir = sfs_make_filename( NULL, fs->start_dir, true );
    if( !fs->start_dir ) return 0;

    //convert start dir to unix standard:
    fs->new_start_dir[ 0 ] = 0;
    if( fs->start_dir[ 0 ] == 0 ) 
	strcat( fs->new_start_dir, "./" ); 
    else 
	strcat( fs->new_start_dir, fs->start_dir );

    //open dir and read first entry:
    fs->dir = opendir( fs->new_start_dir );
    if( fs->dir == 0 ) return 0; //no such dir :(
    fs->current_file = readdir( fs->dir );
    if( !fs->current_file ) return 0; //no files

    //copy file name:
    fs->name[ 0 ] = 0;
    strcpy( fs->name, fs->current_file->d_name );

    //is it a dir?
    test_dir[ 0 ] = 0;
    strcat( test_dir, fs->new_start_dir );
    strcat( test_dir, fs->current_file->d_name );
    test = opendir( test_dir );
    if( test )
    {
	fs->type = SFS_DIR; 
	closedir( test );
    }
    else
    {
	fs->type = SFS_FILE;
    }
    if( strcmp( fs->current_file->d_name, "." ) == 0 ) fs->type = SFS_DIR;
    if( strcmp( fs->current_file->d_name, ".." ) == 0 ) fs->type = SFS_DIR;

    if( fs->type == SFS_FILE )
    {
	if( fs->opt & SFS_FIND_OPT_FILESIZE )
	{
    	    struct stat file_info;
	    if( unix_get_file_info( test_dir, &file_info ) == 0 )
    		fs->size = file_info.st_size;
    	}
	if( !check_file( fs->name, fs ) ) return sfs_find_next( fs );
    }

    return 1;
}

int sfs_find_next( sfs_find_struct* fs )
{
    DIR* test;
    char test_dir[ MAX_DIR_LEN ];

    for(;;) 
    {
	//read next entry:
	if( fs->dir == 0 ) return 0; //directory not exist
	fs->current_file = readdir( fs->dir );
	if( !fs->current_file ) return 0; //no files

	//copy file name:
	fs->name[ 0 ] = 0;
	strcpy( fs->name, fs->current_file->d_name );

	//is it dir?
	test_dir[ 0 ] = 0;
	strcat( test_dir, fs->new_start_dir );
	strcat( test_dir, fs->current_file->d_name );
	test = opendir( test_dir );
	if( test ) 
	{
	    fs->type = SFS_DIR;
	    closedir( test );
	}
	else
	{
	    fs->type = SFS_FILE;
	}
	if( strcmp( fs->current_file->d_name, "." ) == 0 ) fs->type = SFS_DIR;
	if( strcmp( fs->current_file->d_name, ".." ) == 0 ) fs->type = SFS_DIR;

	if( fs->type == SFS_FILE )
	{
	    if( check_file( fs->name, fs ) )
	    {
		if( fs->opt & SFS_FIND_OPT_FILESIZE )
		{
    		    struct stat file_info;
		    if( unix_get_file_info( test_dir, &file_info ) == 0 )
    			fs->size = file_info.st_size;
    		}
		return 1;
	    }
	}
	else return 1; //Dir found
    }
}

void sfs_find_close( sfs_find_struct* fs )
{
    if( fs->dir )
	closedir( fs->dir );

    smem_free( (void*)fs->start_dir );
    fs->start_dir = 0;
}

#endif
