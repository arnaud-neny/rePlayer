/*
    main.cpp - SunDog engine main()
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "video/video.h"
#include "net/net.h"
#include "dsp.h"

#ifdef VULKAN
    #include "vulkan/sd_vulkan.h"
#endif

#ifndef NOGUI
#if SUNDOG_VER == 2
    #include "gui/sd_gui.h"
#endif
#endif

#ifdef OS_IOS
    #include <sys/xattr.h>
#endif

#ifdef SDL
    #ifdef SDL12
	#include "SDL/SDL.h"
    #else
	#include "SDL2/SDL.h"
    #endif
    volatile bool g_sdl_initialized = false;
#endif

#ifdef DENORMAL_NUMBERS
    uint32_t g_denorm_rand_state = 1;
#endif
#ifdef __SSE__
    #include <xmmintrin.h>
    #include <pmmintrin.h>
    static bool g_prev_flush_zero_mode = false;
    static bool g_prev_denormals_zero_mode = false;
#endif
#ifdef ARM_VFP
    static bool g_prev_flush_zero_mode = false;
#endif
static int g_denormal_numbers = -1; //-1 - auto; 0 - disable; 1 - enable;

static int sundog_denormal_numbers( bool enable )
{
    int rv = -1;
    if( enable )
    {
	//Enable:
#ifdef __SSE__
	_MM_SET_FLUSH_ZERO_MODE( g_prev_flush_zero_mode ? _MM_FLUSH_ZERO_ON : _MM_FLUSH_ZERO_OFF );
	_MM_SET_DENORMALS_ZERO_MODE( g_prev_denormals_zero_mode ? _MM_DENORMALS_ZERO_ON : _MM_DENORMALS_ZERO_OFF );
	rv = 0;
#endif
#ifdef ARM_VFP
	#ifdef ARCH_ARM64
            uint64_t cr = 0;
	    asm volatile( "mrs %[dest], FPCR" : [dest] "=r" (cr) );
	#else
            uint32_t cr = 0;
	    #if __GNUC__ <= 4
		asm volatile( "fmrx %[dest], FPSCR" : [dest] "=r" (cr) );
	    #else
		asm volatile( "vmrs %[dest], FPSCR" : [dest] "=r" (cr) );
	    #endif
	#endif
    	if( g_prev_flush_zero_mode ) 
	    cr |= ( 1 << 24 ); //enable flush-to-zero
	else
	    cr &= ~( 1 << 24 );
	#ifdef ARCH_ARM64
	    asm volatile( "msr FPCR, %[src]" : : [src] "r" (cr) );
	#else
	    #if __GNUC__ <= 4
		asm volatile( "fmxr FPSCR, %[src]" : : [src] "r" (cr) );
	    #else
		asm volatile( "vmsr FPSCR, %[src]" : : [src] "r" (cr) );
	    #endif
	#endif
	rv = 0;
#endif
    }
    else
    {
	//Disable:
#ifdef __SSE__
	g_prev_flush_zero_mode = _MM_GET_FLUSH_ZERO_MODE() == _MM_FLUSH_ZERO_ON;
	g_prev_denormals_zero_mode = _MM_GET_DENORMALS_ZERO_MODE() == _MM_DENORMALS_ZERO_ON;
	//if( g_prev_flush_zero_mode || g_prev_denormals_zero_mode )
	//    slog( "Denormals already disabled: %d %d\n", (int)g_prev_flush_zero_mode, (int)g_prev_denormals_zero_mode );
    	_MM_SET_FLUSH_ZERO_MODE( _MM_FLUSH_ZERO_ON ); //return zero instead of a denormal float for operations which would result in a denormal float
	_MM_SET_DENORMALS_ZERO_MODE( _MM_DENORMALS_ZERO_ON ); //treat denormal input arguments to floating point operations as zero
	rv = 0;
#endif
#ifdef ARM_VFP
	#ifdef ARCH_ARM64
            uint64_t cr = 0;
	    asm volatile( "mrs %[dest], FPCR" : [dest] "=r" (cr) );
	#else
            uint32_t cr = 0;
	    #if __GNUC__ <= 4
		asm volatile( "fmrx %[dest], FPSCR" : [dest] "=r" (cr) );
	    #else
		asm volatile( "vmrs %[dest], FPSCR" : [dest] "=r" (cr) );
	    #endif
	#endif
    	g_prev_flush_zero_mode = ( cr & ( 1 << 24 ) ) != 0;
	//if( g_prev_flush_zero_mode )
	//    slog( "Denormals already disabled\n" );
	cr |= ( 1 << 24 ); //enable flush-to-zero
	#ifdef ARCH_ARM64
	    asm volatile( "msr FPCR, %[src]" : : [src] "r" (cr) );
	#else
	    #if __GNUC__ <= 4
		asm volatile( "fmxr FPSCR, %[src]" : : [src] "r" (cr) );
	    #else
		asm volatile( "vmsr FPSCR, %[src]" : : [src] "r" (cr) );
	    #endif
	#endif
	rv = 0;
#endif
    }
    if( rv )
    {
	if( enable )
	    slog( "Can't enable denormal numbers.\n" );
	else
	    slog( "Can't disable denormal numbers.\n" );
    }
    /*volatile float test_v = 1;
    for( int i = 0; i < 256; i++ )
    {
        test_v *= 0.5;
        if( i >= 125 && i <= 150 ) slog( "%d: %e\n", i, test_v );
        if( test_v == 0 ) break;
    }*/
    //Results when FZ enabled: ... 125: 1.175494e-38, 126: 0.000000e+00.
    //Results when FZ disabled: ... 148: 1.401298e-45, 149: 0.000000e+00.
    return rv;
}

static void sundog_denormal_numbers_init( void )
{
#ifdef DISABLE_DENORMAL_NUMBERS
    g_denormal_numbers = 0;
#endif
    g_denormal_numbers = sconfig_get_int_value( APP_CFG_DENORM_NUMBERS, g_denormal_numbers, 0 );
    if( g_denormal_numbers >= 0 ) sundog_denormal_numbers( g_denormal_numbers == 1 );
}

static void sundog_denormal_numbers_deinit( void )
{
    if( g_denormal_numbers >= 0 ) sundog_denormal_numbers( g_denormal_numbers == 0 );
}

void sundog_denormal_numbers_check( void )
{
    if( g_denormal_numbers >= 0 ) sundog_denormal_numbers( g_denormal_numbers == 1 );
}

#ifdef OS_WIN
int g_timer_period = 0;
#endif
static void sundog_set_timer_resolution( void )
{
#if 0// rePlayer: def OS_WIN
    int res = sconfig_get_int_value( APP_CFG_TIMER_RESOLUTION, 1000, 0 ); //Hz
    TIMECAPS tc;
    if( timeGetDevCaps( &tc, sizeof( tc ) ) == MMSYSERR_NOERROR )
    {
        int period = 1000 / res;
        if( period < 1 ) period = 1;
        if( period < tc.wPeriodMin ) period = tc.wPeriodMin;
        if( period > tc.wPeriodMax ) period = tc.wPeriodMax;
        if( timeBeginPeriod( period ) == TIMERR_NOERROR )
        {
            slog( "System timer period has been changed to %d ms; (min:%d;max:%d)\n", period, (int)tc.wPeriodMin, (int)tc.wPeriodMax );
            g_timer_period = period;
        }
    }
#endif
}

static void sundog_reset_timer_resolution( void )
{
#if 0// rePlayer: def OS_WIN
    if( g_timer_period )
    {
        timeEndPeriod( g_timer_period );
        g_timer_period = 0;
    }
#endif
}

#ifdef SUNDOG_MODULE
static sundog::lazy_init_helper sundog_global;
#endif

int sundog_global_init( void )
{
#ifdef SUNDOG_MODULE
    //Global init once per process
    int rv = sundog_global.init_begin( "sundog_global_init()", 5000, 5 );
    if( rv <= 0 ) return rv;
#endif
    stime_global_init();
    smem_global_init();
    sfs_global_init();
    slog_global_init( g_app_log );
    smisc_global_init();
    sundog_set_timer_resolution();
    sthread_global_init();
#ifdef SUNDOG_NET
    snet_global_init();
#endif
    svideo_global_init();
    sundog_sound_global_init();
    sundog_midi_global_init();
#ifdef VULKAN
    sundog::vulkan_global_init();
#endif
#ifndef NOGUI
#if SUNDOG_VER == 2
    sundog::gui_global_init();
#endif
#endif
    sundog_denormal_numbers_init();
    app_global_init();
#ifdef SUNDOG_MODULE
    COMPILER_MEMORY_BARRIER();
    sundog_global.init_end();
#endif
    return 0;
}

int sundog_global_deinit( void )
{
#ifdef SUNDOG_MODULE
    //Global deinit once per process
    int rv = sundog_global.deinit_begin();
    if( rv <= 0 ) return rv;
#endif
    app_global_deinit();
    sundog_denormal_numbers_deinit();
#ifndef NOGUI
#if SUNDOG_VER == 2
    sundog::gui_global_deinit();
#endif
#endif
#ifdef VULKAN
    sundog::vulkan_global_deinit();
#endif
    sundog_midi_global_deinit();
    sundog_sound_global_deinit();
    svideo_global_deinit();
#ifdef SUNDOG_NET
    snet_global_deinit();
#endif
    sthread_global_deinit();
    smisc_global_deinit();
    sfs_global_deinit();
    smem_print_usage();
    slog_global_deinit();
    smem_global_deinit();
    stime_global_deinit();
    sundog_reset_timer_resolution();
#ifdef SDL
    if( g_sdl_initialized )
    {
	SDL_Quit();
	g_sdl_initialized = false;
    }
#endif
#ifdef SUNDOG_MODULE
    COMPILER_MEMORY_BARRIER();
    sundog_global.deinit_end();
#endif
    return 0;
}

#ifndef NOMAIN

#ifdef SUNDOG_MODULE
//Get unique app/module ID (for independent thread or process):
//(this ID can be used to name the session file to avoid overlapping with other open instances of the app)
static void check_module_id( sundog_engine* sd, bool finish )
{
    char sem_name[ 32 ]; sem_name[ 0 ] = 0; smem_strcat( sem_name, sizeof(sem_name), g_app_name_short ); smem_strcat( sem_name, sizeof(sem_name), "ID" );
    ssemaphore sem; ssemaphore_create( &sem, sem_name, 1, 0 ); ssemaphore_wait( &sem, 500 ); //global lock (inter-process)
    uint32_t initial_id = sd->id;
    //printf( "Initial ID: %d\n", initial_id );
    const int max_slots = 128;
    sfs_file f;
    char fname[ 32 ]; fname[ 0 ] = 0;
    smem_strcat( fname, sizeof(fname), "2:/." ); smem_strcat( fname, sizeof(fname), g_app_name_short ); smem_strcat( fname, sizeof(fname), "_slot00" );
    make_string_lowercase( fname, sizeof( fname ), fname );
    int fname_len = (int)strlen( fname );
    int64_t cur_t = stime_time();
    uint64_t cur_tid = sthread_gettid() ^ ( (uint64_t)sthread_getpid() << 32 );
    uint32_t oldest_slot = initial_id;
    int64_t oldest_d = 0;
    int i = 0;
    if( finish )
    {
        fname[ fname_len - 2 ] = int_to_hchar( ( sd->id >> 4 ) & 15 );
        fname[ fname_len - 1 ] = int_to_hchar( sd->id & 15 );
        f = sfs_open( fname, "wb" );
        if( f )
        {
    	    sfs_putc( 0, f ); //busy = 0
	    sfs_write( &cur_t, sizeof( cur_t ), 1, f );
	    sfs_write( &cur_tid, sizeof( cur_tid ), 1, f );
	    sfs_close( f );
	}
	goto check_module_id_finished;
    }
    for( ; i < max_slots; i++ )
    {
        bool fexists = false;
        bool slot_is_free = false;
        char busy = 0;
        int64_t t = 0;
        uint64_t tid = 0;
        fname[ fname_len - 2 ] = int_to_hchar( ( sd->id >> 4 ) & 15 );
        fname[ fname_len - 1 ] = int_to_hchar( sd->id & 15 );
        //printf( "%s\n", fname );
        f = sfs_open( fname, "rb" );
        if( f )
        {
    	    fexists = true;
    	    busy = sfs_getc( f );
	    sfs_read( &t, sizeof( t ), 1, f );
	    sfs_read( &tid, sizeof( tid ), 1, f );
	    sfs_close( f );
	    //printf( "File busy: %d\n", busy );
	}
	while( 1 )
	{
	    if( !fexists ) { slot_is_free = true; break; }
	    int64_t d = cur_t - t;
            if( d > oldest_d ) { oldest_d = d; oldest_slot = i; }
            //printf( "d = %d\n", (int)d );
	    if( busy == 0 )
	    {
	        //slot is not busy:
	        if( d < 0 || d > 5 )
	        {
	    	    //wrong time OR slot was closed more than 5 seconds ago, so we can use it again:
	    	    slot_is_free = true;
	    	    //printf( "Slot is free\n" );
	        }
	        else
	        {
	    	    //can we reuse this slot?
	    	    if( sd->id == initial_id )
	    	    {
	    		//this is previous slot for the current sundog_engine, so we can reuse it:
	    		slot_is_free = true;
	    		//printf( "Reuse\n" );
	    	    }
	        }
	    }
	    else
	    {
	        //slot is busy:
	        if( d < 0 || d > 12 * 60 * 60 )
	        {
	    	    //wrong time OR slot is busy for more than 12 hours -
                    // - possible crash, or result of closing the app without deinitialization
                    //Remove this slot:
	    	    sfs_remove_file( fname );
	    	    slot_is_free = true;
	    	    fexists = false;
	    	    //printf( "Bad slot\n" );
	        }
	    }
	    break;
	}
	if( slot_is_free )
	{
    	    f = sfs_open( fname, "wb" );
    	    if( f )
    	    {
    		sfs_putc( 1, f ); //busy = 1
		sfs_write( &cur_t, sizeof( cur_t ), 1, f );
		sfs_write( &cur_tid, sizeof( cur_tid ), 1, f );
		sfs_close( f );
	    }
#ifdef OS_IOS
	    if( !fexists )
	    {
		char* nn = sfs_make_filename( sd, fname, true );
        	if( nn )
        	{
            	    //Set "Dont backup" property:
            	    u_int8_t attrValue = 1;
            	    setxattr( nn, "com.apple.MobileBackup", &attrValue, sizeof( attrValue ), 0, 0 );
            	    smem_free( nn );
        	}
    	    }
#endif
    	    char busy2 = -1;
    	    int64_t t2 = 0;
    	    uint64_t tid2 = 0;
    	    f = sfs_open( fname, "rb" );
    	    if( f )
    	    {
    		busy2 = sfs_getc( f );
		sfs_read( &t2, sizeof( t2 ), 1, f );
		sfs_read( &tid2, sizeof( tid2 ), 1, f );
		sfs_close( f );
	    }
	    if( busy2 != 1 || t2 != cur_t || tid2 != cur_tid )
	    {
		//Write failed. We can't use this slot.
	    }
	    else
	    {
		//printf( "Write ok; slot found.\n" );
		break;
	    }
	}
	sd->id++;
	sd->id &= max_slots - 1;
    }
    if( i >= max_slots )
    {
        //All slots are busy:
        sd->id = oldest_slot;
    }
    printf( "ID: %d\n", sd->id );
check_module_id_finished:
    ssemaphore_release( &sem ); ssemaphore_destroy( &sem ); //global unlock (inter-process)
}
#endif

#ifndef SUNDOG_MODULE

#if defined(OS_WIN) || defined(OS_WINCE)
int g_argc = 0;
char* g_argv[ 128 ];
char g_cmd_line_utf8[ 2048 ];
static void make_arguments( char* cmd_line )
{
    //Make standard argc and argv[] from windows lpszCmdLine:
#ifdef OS_WINCE
    g_argv[ 0 ] = (char*)"prog";
    g_argc = 1;
#else
    g_argc = 0;
#endif
    if( cmd_line && cmd_line[ 0 ] != 0 )
    {
	int len = (int)strlen( cmd_line );
	while( 1 )
	{
	    if( len <= 0 ) break;
	    if( cmd_line[ len-1 ] == ' ' )
	    {
		cmd_line[ len-1 ] = 0;
		len--;
	    }
	    else break;
	}
	if( len <= 0 ) return;
        int str_ptr = 0;
        int space = 1;
	int r = 0;
	int w = 0;
	int string = 0;
	while( r <= len )
	{
	    char c = cmd_line[ r ];
	    if( c == '"' ) 
	    {
		string ^= 1;
		r++;
		continue;
	    }
	    if( string && c == ' ' ) c = 1;
	    cmd_line[ w ] = c;
	    r++;
	    w++;
	}
        for( int i = 0; i < len; i++ )
        {
    	    if( cmd_line[ i ] != ' ' )
    	    {
    		if( cmd_line[ i ] == 1 ) cmd_line[ i ] = ' ';
	        if( space == 1 )
	        {
	    	    g_argv[ g_argc ] = &cmd_line[ i ];
		    g_argc++;
		}
		space = 0;
	    }
	    else
	    {
		cmd_line[ i ] = 0;
	        space = 1;
	    }
	}
    }
}
#endif

#ifdef OS_WIN
int APIENTRY WinMain( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow )
{
    sundog_engine sd;
    smem_clear_struct( sd );
    sd.hCurrentInst = hCurrentInst;
    sd.hPreviousInst = hPreviousInst; 
    sd.lpszCmdLine = lpszCmdLine;
    sd.nCmdShow = nCmdShow;
    LPWSTR cmd_line = GetCommandLineW();
    if( cmd_line )
    {
	utf16_to_utf8( g_cmd_line_utf8, sizeof(g_cmd_line_utf8), (const uint16_t*)cmd_line );
	make_arguments( g_cmd_line_utf8 );
    }
    sd.argc = g_argc;
    sd.argv = g_argv;
    if( sundog_main( &sd, true ) == 0 )
	return sd.exit_code;
    else
	return -1;
}
#endif

#ifdef OS_WINCE
//int errno = 0;
//(errno is defined in zutil)
WCHAR g_window_name[ 256 ];
extern WCHAR* className; //defined in window manager (wm_wince.h)
int WINAPI WinMain( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPWSTR lpszCmdLine, int nCmdShow )
{
    sundog_engine sd;
    smem_clear_struct( sd );
    sd.hCurrentInst = hCurrentInst;
    sd.hPreviousInst = hPreviousInst;
    sd.lpszCmdLine = lpszCmdLine;
    sd.nCmdShow = nCmdShow;
    utf16_to_utf8( g_cmd_line_utf8, sizeof(g_cmd_line_utf8), (uint16_t*)lpszCmdLine );
    make_arguments( g_cmd_line_utf8 );
    utf8_to_utf16( (uint16_t*)g_window_name, sizeof(g_window_name), g_app_name );
    HWND wnd = FindWindow( className, g_window_name );
    if( wnd )
    {
        //Already opened:
        SetForegroundWindow( wnd ); //Make it foreground
        return 0;
    }
    sd.argc = g_argc;
    sd.argv = g_argv;
    if( sundog_main( &sd, true ) == 0 )
	return sd.exit_code;
    else
	return -1;
}
#endif

#endif // ... ifndef SUNDOG_MODULE

#if defined(OS_UNIX) && !defined(OS_APPLE) && !defined(OS_ANDROID)
#include <pthread.h>
#include <signal.h>
sundog_engine g_sd;
#if SUNDOG_VER == 2
    #include "main_unix_x11.hpp"
#endif //SunDog2
static void main_process_signal_handler( int sig )
{
    switch( sig )
    {
	case SIGINT:
	case SIGTERM:
#if SUNDOG_VER == 2
	    sundog_main_loop_exit( &g_sd );
#else
	    win_exit_request( &g_sd.wm );
#endif
	    break;
	default:
	    break;
    }
}
int main( int argc, char* argv[] )
{
    signal( SIGINT, main_process_signal_handler ); //Interrupt program
    signal( SIGTERM, main_process_signal_handler ); //Software termination signal
    smem_clear_struct( g_sd );
    g_sd.argc = argc;
    g_sd.argv = argv;
    if( sundog_main( &g_sd, true ) == 0 )
	return g_sd.exit_code;
    else
	return 0;
}
#endif

#ifdef SUNDOG_TEST
int sundog_test( sundog_engine* sd )
{
    int rv = 0;
    while( 1 )
    {
	float f;
	int i;
	slog("\n"); i = stime_test( sd ); if( i ) { slog( "stime_test() ERROR %d\n", i ); rv++; }
	slog("\n"); f = fft_test(); slog( "FFT TEST: %.16f\n", f ); if( f > 0.000001506 ) { slog( "fft_test() ERROR\n" ); rv++; }
	slog("\n"); i = ssemaphore_test( sd ); if( i ) { slog( "ssemaphore_test() ERROR %d\n", i ); rv++; }
	slog("\n"); i = srwlock_test( sd ); if( i ) { slog( "srwlock_test() ERROR %d\n", i ); rv++; }
	slog("\n"); i = smutex_test( sd ); if( i ) { slog( "smutex_test() ERROR %d\n", i ); rv++; }
	slog("\n");
	break;
    }
    return rv;
}
#endif

int sundog_main( sundog_engine* sd, bool global_init )
{
    char* cfg = nullptr;
    if( sd->argc > 1 )
    {
        if( strcmp( sd->argv[ 1 ], "/?" ) == 0 ||
            strcmp( sd->argv[ 1 ], "-?" ) == 0 ||
            strcmp( sd->argv[ 1 ], "--help" ) == 0 )
        {
	    if( g_app_usage )
	    {
		printf( "%s\n", g_app_usage );
		return 0;
	    }
	}
	for( int i = 1; i < sd->argc; i++ )
	{
	    char* v = sd->argv[ i ];
    	    while( 1 )
    	    {
    		if( v[ 0 ] == '-' )
    		{
            	    v++;
            	    if( strcmp( v, "id" ) == 0 )
            	    {
        		i++;
            		if( i < sd->argc ) sd->id = string_to_int( sd->argv[ i ] );
            		break;
            	    }
            	    if( strcmp( v, "cfg" ) == 0 )
            	    {
            		i++;
            		if( i < sd->argc ) cfg = sd->argv[ i ];
            		break;
            	    }
            	    if( strcmp( v, "ncl" ) == 0 )
            	    { //no logging to console
            		g_slog_no_cout = 1;
            		break;
            	    }
            	    if( strcmp( v, "nfl" ) == 0 )
            	    { //no logging to file
            		g_slog_no_fout = 1;
            		break;
            	    }
            	    break;
        	}
    		break;
    	    }
	}
    }

main_begin:

    bool restart_request = false;
    int err_count = 0;

    if( global_init )
    {
	sundog_global_init();
    }

    //Set defaults:
#ifdef OS_MAEMO
    sconfig_set_int_value( APP_CFG_WIN_XSIZE, sconfig_get_int_value( APP_CFG_WIN_XSIZE, 800, 0 ), 0 );
    sconfig_set_int_value( APP_CFG_WIN_YSIZE, sconfig_get_int_value( APP_CFG_WIN_YSIZE, 480, 0 ), 0 );
    sconfig_set_int_value( APP_CFG_SND_BUF, sconfig_get_int_value( APP_CFG_SND_BUF, 2048, 0 ), 0 );
    sconfig_set_str_value( APP_CFG_SND_DRIVER, sconfig_get_str_value( APP_CFG_SND_DRIVER, "sdl", 0 ), 0 );
    sconfig_set_int_value( APP_CFG_FULLSCREEN, sconfig_get_int_value( APP_CFG_FULLSCREEN, 1, 0 ), 0 );
    sconfig_set_int_value( APP_CFG_ZOOM, sconfig_get_int_value( APP_CFG_ZOOM, 2, 0 ), 0 );
    sconfig_set_int_value( APP_CFG_PPI, sconfig_get_int_value( APP_CFG_PPI, 240, 0 ), 0 );
    sconfig_set_int_value( APP_CFG_SCALE, sconfig_get_int_value( APP_CFG_SCALE, 130, 0 ), 0 );
    sconfig_set_int_value( APP_CFG_NOCURSOR, sconfig_get_int_value( APP_CFG_NOCURSOR, 1, 0 ), 0 );
#endif
    if( cfg ) sconfig_load_from_string( cfg, '|', nullptr );

#ifdef SUNDOG_MODULE
    check_module_id( sd, false );
#endif

    slog( "SunDog Engine / %s %s\n", __DATE__, __TIME__ );

#ifdef SUNDOG_TEST
    slog( "SunDog TEST BEGIN...\n" );
    int test_rv = sundog_test( sd );
    slog( "SunDog TEST END\n" );
    if( test_rv ) slog( "SunDog TEST ERROR %d\n", test_rv );
#endif

    smbox* mb = smbox_new();
    sd->mb = mb;

#if SUNDOG_VER == 2
#if defined(OS_UNIX) && !defined(OS_APPLE) && !defined(OS_ANDROID)
    int loop_rv = sundog_main_loop( sd );
    if( loop_rv ) err_count++;
#endif
#endif //SunDog2

#ifndef SUNDOG_VER
    window_manager* wm = &sd->wm;
    if( mb && win_init( g_app_name, g_app_window_xsize, g_app_window_ysize, g_app_window_flags, sd ) == 0 )
    {
        if( wm->exit_request == 0 )
        {
    	    int app_init_res = app_init( wm );
	    if( app_init_res == 0 )
	    {
		while( 1 )
	    	{
	    	    sundog_event evt;
		    EVENT_LOOP_BEGIN( &evt, wm );
		    if( EVENT_LOOP_END( wm ) ) break;
		}
		app_deinit( wm );
		if( wm->restart_request ) restart_request = 1;
	    }
	    else
	    {
		if( app_init_res != -2 )
		    err_count++;
	    }
	}
	win_deinit( wm ); //Close window manager
    }
    else err_count++;
#endif //SunDog1

    sd->mb = nullptr;
    COMPILER_MEMORY_BARRIER();
    smbox_remove( mb );
    mb = nullptr;

#ifdef SUNDOG_MODULE
    check_module_id( sd, true );
#endif

    if( err_count > 0 )
    {
	slog_show_error_report( sd );
    }

    if( global_init )
    {
	sundog_global_deinit();
    }

    if( restart_request ) goto main_begin;

    return err_count;
}

#endif // ... ifndef NOMAIN
