/*
    time.cpp - time management (thread-safe)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_UNIX
    #include <time.h>
#endif
#if defined(OS_APPLE)
    #include <mach/mach.h>	    //mach_absolute_time() ...
    #include <mach/mach_time.h>	    //mach_absolute_time() ...
    #include <unistd.h>
#endif
#ifdef OS_FREEBSD
    #include <sys/time.h>
#endif
#ifdef OS_LINUX
    #include <sys/select.h>
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    #include <time.h>
#endif

#if defined(OS_APPLE) || defined(OS_WIN) || defined(OS_WINCE)
static uint64_t muldiv128( uint64_t i, uint32_t numer, uint32_t denom ) //rv = i * numer / denom
{
    uint64_t high = ( i >> 32 ) * numer;
    uint64_t low = ( i & 0xffffffffull ) * numer / denom;
    uint64_t high_rem = ( ( high % denom ) << 32 ) / denom;
    high /= denom;
    return ( high << 32 ) + high_rem + low;
}
#endif

#if defined(OS_APPLE)
#ifndef OS_IOS
    #define TIMEHIGHPREC
#endif
static mach_timebase_info_data_t g_timebase_info;
static uint64_t g_time_bias = 0;
double g_time_scale1 = 0; //lowres
double g_time_scale2 = 0; //highres
double g_time_scale3 = 0; //to nsec
stime_ticks_t convert_mach_absolute_time_to_sundog_ticks( uint64_t mt, int tick_type ) //0 - lowres (1000 TPS); 1 - highres;
{
#ifdef TIMEHIGHPREC
    uint64_t t = muldiv128( mt - g_time_bias, g_timebase_info.numer, g_timebase_info.denom );
    if( tick_type == 0 )
    	t /= (1000000000/1000);
    else
	t /= (1000000000/stime_ticks_per_second());
    return (stime_ticks_t)t;
#else
    double t = (double)( mt - g_time_bias );
    if( tick_type == 0 )
	t *= g_time_scale1;
    else
	t *= g_time_scale2;
    return (stime_ticks_t)t;
#endif
}
static uint64_t convert_mach_absolute_time_to_nsec( uint64_t mt )
{
#ifdef TIMEHIGHPREC
    return muldiv128( mt - g_time_bias, g_timebase_info.numer, g_timebase_info.denom );
#else
    double t = (double)( mt - g_time_bias );
    t *= g_time_scale3;
    return (uint64_t)t;
#endif
}
uint64_t convert_sundog_ticks_to_mach_absolute_time( stime_ticks_t t, int tick_type ) //0 - lowres (1000 TPS); 1 - highres;
{
    double mt = t;
    if( tick_type == 0 )
	mt /= g_time_scale1;
    else
	mt /= g_time_scale2;
    //t overflow (which will give incorrect result of this function) protection:
    /*if( t > 0xFFFFFFFF - stime_ticks_per_second() * 60 * 15 ) 
    {
	//FIRST TEST IT!!
	stime_reset(); 
	mt = 0;
    }*/
    return (uint64_t)mt + g_time_bias;
}
#endif

#if defined(OS_WIN) || defined(OS_WINCE)
static uint64_t g_ticks_per_second = 0;
static stime_ticks_t g_ticks_per_second_norm = 0;
static uint32_t g_ticks_div = 1;
static uint32_t g_ticks_mul = 0;
static uint64_t g_time_bias = 0;
#endif

static void stime_reset()
{
#if defined(OS_APPLE)
    mach_timebase_info( &g_timebase_info );
    g_time_bias = mach_absolute_time();
    g_time_scale1 = (double)g_timebase_info.numer / (double)g_timebase_info.denom / (double)(1000000000/1000); //1000 TPS
    g_time_scale2 = (double)g_timebase_info.numer / (double)g_timebase_info.denom / (double)(1000000000/stime_ticks_per_second()); //highres
    g_time_scale3 = (double)g_timebase_info.numer / (double)g_timebase_info.denom; //to nsec
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    QueryPerformanceFrequency( (LARGE_INTEGER*)&g_ticks_per_second );
    if( g_ticks_per_second > 50000 )
    {
	g_ticks_div = g_ticks_per_second / 50000;
	g_ticks_per_second_norm = (stime_ticks_t)( g_ticks_per_second / g_ticks_div );
    }
    else
    {
	g_ticks_mul = 50000 / g_ticks_per_second;
	if( g_ticks_mul * g_ticks_per_second < 50000 ) g_ticks_mul++;
	g_ticks_per_second_norm = (stime_ticks_t)( g_ticks_per_second * g_ticks_mul );
    }
    uint64_t tick;
    QueryPerformanceCounter( (LARGE_INTEGER*)&tick );
    g_time_bias = tick;
    /*
    printf( "TPS: %d\n", (int)g_ticks_per_second );
    printf( "TPSN: %d\n", (int)g_ticks_per_second_norm );
    printf( "DIV: %d\n", (int)g_ticks_div );
    printf( "MUL: %d\n", (int)g_ticks_mul );
    */
#endif
}

int stime_global_init()
{
    stime_reset();
    return 0;
}

int stime_global_deinit()
{
    return 0;
}

int64_t stime_time()
{
#ifdef OS_UNIX
    time_t t; //may be 32bit on 32bit system :(
    time( &t );
    return t; //number of seconds since Jan 1, 1970 (UTC)
#endif
#ifdef OS_WIN
    __time64_t t;
    _time64( &t );
    return t; //number of seconds since Jan 1, 1970 (UTC)
#endif
#ifdef OS_WINCE
    FILETIME ft;
    SYSTEMTIME st;
    GetSystemTime( &st );
    if( SystemTimeToFileTime( &st, &ft ) )
    {
	int64_t rv = (int64_t)ft.dwLowDateTime | ( (int64_t)ft.dwHighDateTime << 32 ); //number of 100-nanosecond intervals since Jan 1, 1601 (UTC).
	rv /= 10000000; //to seconds;
	rv -= 11644473600; //to C standard Jan 1, 1970 (UTC)
	return rv;
    }
    return 0;
#endif
}

uint32_t stime_year()
{
#ifdef OS_UNIX
    time_t t;
    time( &t );
    return localtime( &t )->tm_year + 1900;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wYear;
#endif
}

uint32_t stime_month()
{
#ifdef OS_UNIX
    time_t t;
    time( &t );
    return localtime( &t )->tm_mon + 1;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wMonth;
#endif
}

const char* stime_month_string()
{
    switch( stime_month() )
    {
	case 1: return "jan"; break;
	case 2: return "feb"; break;
	case 3: return "mar"; break;
	case 4: return "apr"; break;
	case 5: return "may"; break;
	case 6: return "jun"; break;
	case 7: return "jul"; break;
	case 8: return "aug"; break;
	case 9: return "sep"; break;
	case 10: return "oct"; break;
	case 11: return "nov"; break;
	case 12: return "dec"; break;
	default: return ""; break;
    }
}

uint32_t stime_day()
{
#ifdef OS_UNIX
    time_t t;
    time( &t );
    return localtime( &t )->tm_mday;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wDay;
#endif
}

uint32_t stime_hours()
{
#ifdef OS_UNIX
    time_t t;
    time( &t );
    return localtime( &t )->tm_hour;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wHour;
#endif
}

uint32_t stime_minutes()
{
#ifdef OS_UNIX
    time_t t;
    time( &t );
    return localtime( &t )->tm_min;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wMinute;
#endif
}

uint32_t stime_seconds()
{
#ifdef OS_UNIX
    time_t t = 0;
    time( &t );
    return localtime( &t )->tm_sec;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wSecond;
#endif
}

stime_ms_t stime_ms()
{
#ifdef OS_UNIX
    #if defined(OS_APPLE)
	return (stime_ms_t)convert_mach_absolute_time_to_sundog_ticks( mach_absolute_time(), 0 );
    #else
	timespec t;
	clock_gettime( CLOCK_REALTIME, &t );
	return (stime_ms_t)( t.tv_nsec / 1000000 ) + t.tv_sec * 1000;
    #endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    return (uint64_t)stime_ticks() * 1000 / stime_ticks_per_second();
    //return (stime_ms_t)GetTickCount();
#endif
}

//#include <x86intrin.h>
//uint64_t t1 = __rdtsc();

#if defined(OS_WIN) || defined(OS_WINCE)
stime_ticks_t stime_ticks_per_second()
{
    return g_ticks_per_second_norm;
}
#endif
#if 0// rePlayer def OS_WIN
stime_ticks_t __attribute__ ((force_align_arg_pointer)) stime_ticks()
#else
stime_ticks_t stime_ticks()
#endif
{
#ifdef OS_UNIX
    #if defined(OS_APPLE)
	return (stime_ticks_t)convert_mach_absolute_time_to_sundog_ticks( mach_absolute_time(), 1 );
    #else
	//Other UNIX systems:
        timespec t;
	clock_gettime( CLOCK_REALTIME, &t );
        return (stime_ticks_t)( t.tv_nsec / ( 1000000000 / stime_ticks_per_second() ) ) + t.tv_sec * stime_ticks_per_second();
    #endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    uint64_t tick;
    QueryPerformanceCounter( (LARGE_INTEGER*)&tick );
    if( g_ticks_mul )
	return (stime_ticks_t)( tick * g_ticks_mul );
    else
	return (stime_ticks_t)( tick / g_ticks_div );
#endif
    return 0;
}

stime_ns_t stime_ns()
{
#ifdef OS_UNIX
    #if defined(OS_APPLE)
	return convert_mach_absolute_time_to_nsec( mach_absolute_time() );
    #else
        timespec t;
	clock_gettime( CLOCK_REALTIME, &t );
        return t.tv_nsec + t.tv_sec * (uint64_t)1000000000;
    #endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    uint64_t tick;
    QueryPerformanceCounter( (LARGE_INTEGER*)&tick );
    return muldiv128( tick - g_time_bias, 1000000000, g_ticks_per_second );
#endif
    return 0;
}

void stime_sleep( int milliseconds )
{
#ifdef OS_UNIX
    #if defined(OS_APPLE)
	while( 1 )
	{
	    int t = milliseconds;
	    if( t > 1000 ) t = 1000;
	    usleep( t * 1000 );
	    milliseconds -= t;
	    if( milliseconds <= 0 ) break;
	}
    #else
	#ifdef OS_EMSCRIPTEN
	    emscripten_sleep( milliseconds );
	#else
	    timeval t;
	    t.tv_sec = milliseconds / 1000;
	    t.tv_usec = ( milliseconds % 1000 ) * 1000;
	    select( 0 + 1, 0, 0, 0, &t );
	#endif
    #endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    Sleep( milliseconds );
#endif
}

#ifdef SUNDOG_TEST
int stime_test( sundog_engine* sd )
{
    slog( "STIME TEST...\n" );
    stime_ns_t t1 = stime_ns();
    stime_sleep( 1000 );
    stime_ns_t t2 = stime_ns();
    slog( "stime_sleep( 1000 ) = %u ns\n", t2 - t1 );
    t1 = stime_ns();
    t2 = stime_ns();
    slog( "stime_ns() quantum = %d ns ; %f ms\n", t2 - t1, ( t2 - t1 ) / 1000000000.0 * 1000.0 );
    slog( "STIME TEST FINISHED\n" );
    return 0;
}
#endif
