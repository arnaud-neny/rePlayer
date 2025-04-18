#pragma once

#if defined(OS_UNIX)
//&& !defined(OS_ANDROID)
    #define SUNDOG_NET
#endif

int snet_global_init( void );
int snet_global_deinit( void );
int snet_get_host_info( sundog_engine* s, char** host_addr, char** addr_list );
/*
    Example:
	host_addr = "192.168.1.40";
	addr_list = "192.168.1.40 192.168.1.39 IPv6addr";
*/
