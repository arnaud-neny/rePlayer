/*
    file_apple.mm - file functions for iOS and macOS (objc)
    This file is part of the SunDog engine.
    Copyright (C) 2018 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#import <sys/stat.h> //mkdir
#import <Foundation/Foundation.h>

char* g_apple_docs_path = NULL;
char* g_apple_appsupport_path = NULL;
char* g_apple_caches_path = NULL;
char* g_apple_tmp_path = NULL;
char* g_apple_resources_path = NULL;

const char* sfs_get_work_path( void )
{
    return (char*)g_apple_docs_path;
}

const char* sfs_get_conf_path( void )
{
    return (char*)g_apple_appsupport_path;
}

const char* sfs_get_temp_path( void )
{
    return (char*)g_apple_tmp_path;
}

int apple_sfs_global_init( void )
{
    //Don't use SunDog file/log functions here!

    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];

    //Get system paths:

    NSArray* paths;
    size_t len;
    NSBundle* mb = [ NSBundle mainBundle ];

    //Documents:
#ifdef OS_IOS
    paths = NSSearchPathForDirectoriesInDomains( NSDocumentDirectory, NSUserDomainMask, YES );
    const char* docs_path_utf8 = [ [ paths objectAtIndex: 0 ] UTF8String ];
    if( docs_path_utf8 )
    {
    	mkdir( docs_path_utf8, S_IRWXU | S_IRWXG | S_IRWXO );
    	len = strlen( docs_path_utf8 );
    	g_apple_docs_path = (char*)malloc( len + 2 );
    	memcpy( g_apple_docs_path, docs_path_utf8, len + 1 );
    	strcat( g_apple_docs_path, "/" );
    }
#else
    if( mb )
    {
	const char* url_cstr = [ [ mb bundlePath ] UTF8String ];
	if( url_cstr )
	{
	    len = (int)strlen( url_cstr );
	    g_apple_docs_path = (char*)malloc( len + 2 );
	    memcpy( g_apple_docs_path, url_cstr, len + 1 );
	    for( int i = len; i > 0; i-- )
	    {
		if( g_apple_docs_path[ i ] == '/' )
		{
		    g_apple_docs_path[ i + 1 ] = 0;
		    break;
		}
	    }
	}
    }
#endif

    //App support:
    bool appsupport_just_created = false;
    paths = NSSearchPathForDirectoriesInDomains( NSApplicationSupportDirectory, NSUserDomainMask, YES );
    const char* appsupport_path_utf8 = [ [ paths objectAtIndex: 0 ] UTF8String ];
    if( appsupport_path_utf8 )
    {
	mkdir( appsupport_path_utf8, S_IRWXU | S_IRWXG | S_IRWXO );
	len = strlen( appsupport_path_utf8 );
	g_apple_appsupport_path = (char*)malloc( len + 256 );
	g_apple_appsupport_path[ 0 ] = 0;
	sprintf( g_apple_appsupport_path, "%s/%s", appsupport_path_utf8, g_app_name_short );
	if( access( g_apple_appsupport_path, F_OK ) == 0 )
	    appsupport_just_created = false;
	else
	{
	    appsupport_just_created = true;
	    mkdir( g_apple_appsupport_path, S_IRWXU | S_IRWXG | S_IRWXO );
	}
	strcat( g_apple_appsupport_path, "/" );
    }

    //Caches:
    paths = NSSearchPathForDirectoriesInDomains( NSCachesDirectory, NSUserDomainMask, YES );
    const char* caches_path_utf8 = [ [ paths objectAtIndex: 0 ] UTF8String ];
    if( caches_path_utf8 )
    {
	mkdir( caches_path_utf8, S_IRWXU | S_IRWXG | S_IRWXO );
	len = strlen( caches_path_utf8 );
	g_apple_caches_path = (char*)malloc( len + 2 );
	memcpy( g_apple_caches_path, caches_path_utf8, len + 1 );
	strcat( g_apple_caches_path, "/" );
    }

    //Temp:
    NSString* tmp = NSTemporaryDirectory();
    const char* tmp_path_utf8 = [ tmp UTF8String ];
    if( tmp_path_utf8 )
    {
	len = strlen( tmp_path_utf8 );
	g_apple_tmp_path = (char*)malloc( len + 2 );
	memcpy( g_apple_tmp_path, tmp_path_utf8, len + 1 );
	if( g_apple_tmp_path[ len - 1 ] != '/' ) strcat( g_apple_tmp_path, "/" );
    }

    //Resources:
    if( mb )
    {
	const char* res_path_utf8 = [ [ mb resourcePath ] UTF8String ];
	if( res_path_utf8 )
	{
	    len = strlen( res_path_utf8 );
	    g_apple_resources_path = (char*)malloc( len + 2 );
	    memcpy( g_apple_resources_path, res_path_utf8, len + 1 );
	    strcat( g_apple_resources_path, "/" );
	}
    }

#ifndef NOMAIN
    printf( "Docs: %s\n", g_apple_docs_path );
    printf( "Appsupport: %s\n", g_apple_appsupport_path );
    printf( "Caches: %s\n", g_apple_caches_path );
    printf( "Temp: %s\n", g_apple_tmp_path );
    printf( "Resources: %s\n", g_apple_resources_path );
#endif

    [ pool release ];

    return 0;
}

int apple_sfs_global_deinit( void )
{
    //Don't use SunDog file/log functions here!

    free( g_apple_docs_path );
    free( g_apple_appsupport_path );
    free( g_apple_caches_path );
    free( g_apple_tmp_path );
    free( g_apple_resources_path );
    g_apple_docs_path = NULL;
    g_apple_appsupport_path = NULL;
    g_apple_caches_path = NULL;
    g_apple_tmp_path = NULL;
    g_apple_resources_path = NULL;

    return 0;
}
