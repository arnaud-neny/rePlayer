/*
    net.cpp - networking
    This file is part of the SunDog engine.
    Copyright (C) 2017 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "net.h"

#ifdef SUNDOG_NET

#ifdef OS_ANDROID
    #include "main/android/sundog_bridge.h"
#endif

#if defined(OS_UNIX)
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <ifaddrs.h>
#endif

int snet_global_init()
{
    return 0;
}

int snet_global_deinit()
{
    return 0;
}

int snet_get_host_info( sundog_engine* s, char** host_addr, char** addr_list )
{
    int rv = -1;

#if defined(OS_ANDROID)
#ifndef NOMAIN
    //Delete it when the lower app limit will be >= API level 24 (Android 7.0)
    char* a = android_sundog_get_host_ips( s, 0 );
    char* alist = android_sundog_get_host_ips( s, 1 );
    if( host_addr ) *host_addr = SMEM_STRDUP( a );
    if( addr_list ) *addr_list = SMEM_STRDUP( alist );
    free( a );
    free( alist );
    rv = 0;
#endif
#endif

#if defined(OS_UNIX) && !defined(OS_ANDROID)
    struct ifaddrs* myaddrs = NULL;
    struct ifaddrs* ifa = NULL;
    int status = 0;
    char buf[ 256 ];
    buf[ 0 ] = 0;
    char* list = SMEM_ALLOC2( char, 1 );
    list[ 0 ] = 0;

    while( 1 )
    {
        status = getifaddrs( &myaddrs ); //Android minimum: API level 24 (Android 7.0)
        if( status != 0 )
        {
            slog( "getifaddrs() error %d", status );
            rv = -2;
            break;
        }

        for( ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next )
	{
    	    if( ifa->ifa_addr == NULL ) continue;
    	    if( ( ifa->ifa_flags & IFF_UP ) == 0 ) continue;
    	    if( strcmp( ifa->ifa_name, "lo" ) == 0 ) continue;

    	    if( ifa->ifa_addr->sa_family == AF_INET )
    	    {
        	struct sockaddr_in* s4 = (struct sockaddr_in *)(ifa->ifa_addr);
        	if( inet_ntop( ifa->ifa_addr->sa_family, (void*)&(s4->sin_addr), buf, sizeof( buf ) ) == NULL )
        	{
            	    slog( "%s: inet_ntop failed!\n", ifa->ifa_name );
        	}
        	else
        	{
            	    slog( "%s: %s\n", ifa->ifa_name, buf );
            	    if( addr_list && !smem_strstr( buf, "127.0.0.1" ) )
            	    {
            		SMEM_STRCAT_D( list, buf );
            		SMEM_STRCAT_D( list, " " );
            	    }
		    if( strcmp( ifa->ifa_name, "en0" ) == 0 )
		    {
			if( host_addr && *host_addr == NULL ) *host_addr = SMEM_STRDUP( buf );
		    }
            	    uint8_t* a = (uint8_t*)&(s4->sin_addr);
            	    if( a[ 0 ] == 192 && a[ 1 ] == 168 )
            	    {
			if( host_addr && *host_addr == NULL ) *host_addr = SMEM_STRDUP( buf );
            	    }
        	}
        	continue;
    	    }

    	    if( ifa->ifa_addr->sa_family == AF_INET6 )
    	    {
        	struct sockaddr_in6* s6 = (struct sockaddr_in6 *)(ifa->ifa_addr);
        	if( inet_ntop( ifa->ifa_addr->sa_family, (void *)&(s6->sin6_addr), buf, sizeof( buf ) ) == NULL )
        	{
            	    slog( "%s: inet_ntop failed!\n", ifa->ifa_name );
        	}
        	else
        	{
            	    slog( "%s: %s\n", ifa->ifa_name, buf );
            	    if( addr_list )
            	    {
            	        SMEM_STRCAT_D( list, buf );
            		SMEM_STRCAT_D( list, " " );
            	    }
        	}
        	continue;
    	    }
	}

	if( addr_list )
	{
	    *addr_list = list;
	    size_t len = strlen( list );
	    if( len > 1 ) list[ len - 1 ] = 0;
	    list = NULL;
	}

        rv = 0;
        break;
    }

    if( myaddrs ) freeifaddrs( myaddrs );
    if( list ) smem_free( list );
#endif

    return rv;
}

#endif
