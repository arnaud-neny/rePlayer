/*
    sundog_bridge.mm - SunDog<->iOS bridge
    This file is part of the SunDog engine.
    Copyright (C) 2009 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h> //mkdir
#include <atomic>

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>

#import <UIKit/UIKit.h>
#import <UIKit/UIPasteboard.h>
#import <MobileCoreServices/UTCoreTypes.h>
#import <MessageUI/MFMailComposeViewController.h>
#import <CoreAudioKit/CoreAudioKit.h>
#import <AVFoundation/AVFoundation.h>
#import <AVFoundation/AVAudioSession.h>

#include "sundog.h"
#include "sound/sound_ios.hpp"
#include "sundog_bridge.h"

char g_ui_lang[ 64 ];

void sundog_state_set_( ios_sundog_engine* sd, int io, sundog_state* state );

/*
 Application startup sequence:
 didFinishLaunchingWithOptions
   sundog_init()
     view = (MyView*)view_controller.view
       view -> initWithCoder - init the view from the Interface Builder files; creating OpenGL layer and context
       controller -> viewDidLoad - called after the controller's view is loaded into memory
     make crossconnection ios_sundog_engine <--> view and view_controller;
     viewInit: framebuffer creation;
     SunDog engine thread begins here...
   sundog_init() finished
 controller -> prefersStatusBarHidden
 iPhone X: view -> safeAreaInsetsDidChange; controller -> prefersStatusBarHidden;
 view -> didMoveToWindow - tells the view that its window object changed
 view -> layoutSubviews * 2 - place where you can perform more precise layout of the subviews
 applicationDidBecomeActive
*/

#ifndef SUNDOG_MODULE
    #define MAIN_VIEW_CTL MainViewController
#else
    #ifdef AUDIOUNIT_EXTENSION
	#define MAIN_VIEW_CTL AudioUnitViewController
    #endif
#endif

struct ios_sundog_engine
{
    int initialized;
    pthread_t pth;
    volatile int pth_finished;
    volatile int hold; //Hold the initial thread phase (before the sundog_main())
    volatile bool eventloop_stop_request;
    volatile bool eventloop_stop_answer;
    sundog_engine s;

    MAIN_VIEW_CTL* view_controller;
    MyView* view;

    EAGLContext* gl_context;
    pthread_mutex_t gl_mutex;

    //The following part will be cleared only once (default zeroes + sundog_init_state_atomics()):
    bool 			state_atomics_initialized;
    std::atomic<sundog_state*> 	in_state; //Input state (document)
    std::atomic<sundog_state*> 	out_state; //Output state
    volatile int 		state_req; //state requests: "input available" or/and "output requested"
    int 			state_req_r; //state_req != state_req_r --> handle state requests;
    volatile bool 		out_state_req;
    uint32_t			app_id;
};
#ifndef SUNDOG_MODULE
    ios_sundog_engine g_sd;
#endif

//
// Main functions, will be called from the App Delegate or from the Audio Unit Extension
//

void* sundog_thread( void* arg )
{
    ios_sundog_engine* sd = (ios_sundog_engine*)arg;

    while( sd->hold == 1 ) stime_sleep( 20 );
    if( sd->hold == 2 ) goto thread_end;

    [ EAGLContext setCurrentContext: sd->gl_context ];

    sundog_main( &sd->s, true );

    sd->pth_finished = 1;
thread_end:
    pthread_exit( NULL );
    return 0;
}

void sundog_init_state_atomics( ios_sundog_engine* sd )
{
    if( !sd->state_atomics_initialized )
    {
	sd->state_atomics_initialized = true;
	atomic_init( &sd->in_state, (sundog_state*)NULL );
	atomic_init( &sd->out_state, (sundog_state*)NULL );
    }
    else
    {
	sundog_state_set_( sd, 1, NULL ); //previous output (if exists) can't be used anyomore
    }
}

void sundog_init( MAIN_VIEW_CTL* ctl, ios_sundog_engine* sd )
{
    int err;

    slog( "sundog_init() ...\n" );
    size_t zsize = (size_t)( (char*)&sd->state_atomics_initialized - (char*)sd );
    memset( sd, 0, zsize );
    sd->s.device_specific = sd;
    sd->s.id = sd->app_id;
    sundog_init_state_atomics( sd );

    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init( &mutexattr );
    pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
    pthread_mutex_init( &sd->gl_mutex, &mutexattr );
    pthread_mutexattr_destroy( &mutexattr );

    sd->view_controller = ctl;
    sd->view = (MyView*)sd->view_controller.view;
#ifndef SUNDOG_MODULE
    [ sd->view_controller setSunDog: sd ];
#endif
    [ sd->view setSunDog: sd ];

#ifdef AUDIOBUS
    audiobus_init( sd->view );
#endif

#ifdef AUDIOUNIT_EXTENSION
    sd->s.auext_channel_count = [ ctl auGetChannelCount ];
    sd->s.auext_sample_rate = [ ctl auGetSampleRate ];
#endif

    //Init the view:
    if( [ sd->view viewInit ] ) return;

    if( g_ui_lang[ 0 ] == 0 )
    {
    	const char* lang = [ [ [ NSLocale preferredLanguages ] objectAtIndex:0 ] UTF8String ];
	if( lang )
	{
	    size_t len = strlen( lang );
	    if( len )
	    {
	    	if( len > sizeof( g_ui_lang ) - 1 ) len = sizeof( g_ui_lang ) - 1;
	    	memcpy( g_ui_lang, lang, len );
	    	g_ui_lang[ len ] = 0;
	    }
	}
    }

    slog( "sundog_init(): screen %d x %d; lang %s\n", sd->s.screen_xsize, sd->s.screen_ysize, g_ui_lang );

    //Create the main thread:
    sd->hold = 0;
#ifndef SUNDOG_MODULE
    if( [ UIApplication sharedApplication ].applicationState == UIApplicationStateBackground ) sd->hold = 1;
#endif
    err = pthread_create( &sd->pth, 0, &sundog_thread, (void*)sd );
    if( err == 0 )
    {
	//The pthread_detach() function marks the thread identified by thread as
	//detached. When a detached thread terminates, its resources are 
	//automatically released back to the system.
	err = pthread_detach( sd->pth );
	if( err != 0 )
	{
	    slog( "sundog_init(): pthread_detach error %d\n", err );
	    return;
	}
    }
    else
    {
	slog( "sundog_init(): pthread_create error %d\n", err );
	return;
    }

    slog( "sundog_init(): done\n" );
    sd->initialized = 1;
}

void sundog_deinit( ios_sundog_engine* sd )
{
    int err;
    
    slog( "sundog_deinit() ...\n" );
    
    if( sd->initialized == 0 )
    {
	sd->hold = 2;
	return;
    }
    sd->hold = 0;

    //Stop the thread:
    win_exit_request( &sd->s.wm );
    {
	sd->eventloop_stop_request = false;
	STIME_WAIT_FOR( sd->eventloop_stop_answer == false, 4000, 10, slog( "Thread stop timeout!\n" ) );
    }
    int timeout_counter = 1000 * 7; //Timeout in milliseconds
    while( timeout_counter > 0 )
    {
	struct timespec delay;
	delay.tv_sec = 0;
	delay.tv_nsec = 1000000 * 20; //20 milliseconds
	if( sd->pth_finished ) break;
	nanosleep( &delay, NULL ); //Sleep for delay time
	timeout_counter -= 20;
    }
    if( timeout_counter <= 0 )
    {
	slog( "sundog_deinit(): thread timeout\n" );
	err = pthread_cancel( sd->pth );
	if( err != 0 )
	{
	    slog( "sundog_deinit(): pthread_cancel error %d\n", err );
	}
    }
    sd->app_id = sd->s.id;

    [ sd->view viewDeinit ];

#ifdef AUDIOBUS
    audiobus_deinit();
#endif

    pthread_mutex_destroy( &sd->gl_mutex );
    
    sd->initialized = 0;

    slog( "sundog_deinit(): done\n" );
}

void sundog_pause( ios_sundog_engine* sd )
{
    slog( "App pause\n" );
#ifdef IDLE_TIMER_DISABLED
#ifndef SUNDOG_MODULE
    [ UIApplication sharedApplication ].idleTimerDisabled = NO;
#endif
#endif
    sd->eventloop_stop_request = true;
    STIME_WAIT_FOR( sd->eventloop_stop_answer == true, 4000, 10, slog( "App pause timeout!\n" ) );
}

void sundog_resume( ios_sundog_engine* sd )
{
    slog( "App resume\n" );
#ifdef IDLE_TIMER_DISABLED
#ifndef SUNDOG_MODULE
    [ UIApplication sharedApplication ].idleTimerDisabled = YES;
#endif
#endif
    sd->eventloop_stop_request = false;
    STIME_WAIT_FOR( sd->eventloop_stop_answer == false, 4000, 10, slog( "App resume timeout!\n" ) );
}

//
// The following functions will be called from the SunDog (wm_ios.h and others)
//

void sundog_state_set_( ios_sundog_engine* sd, int io, sundog_state* state ) //io: 0 - app input; 1 - app output;
{
    if( !sd->state_atomics_initialized ) sundog_init_state_atomics( sd );
    sundog_state* prev_state = NULL;
    if( io == 0 )
    {
	prev_state = atomic_exchange( &sd->in_state, state );
	COMPILER_MEMORY_BARRIER();
	sd->state_req++;
    }
    else
    {
	prev_state = atomic_exchange( &sd->out_state, state );
    }
    sundog_state_remove( prev_state );
}

void sundog_state_set( sundog_engine* s, int io, sundog_state* state ) //io: 0 - app input; 1 - app output;
{
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    if( sd ) sundog_state_set_( sd, io, state );
}

sundog_state* sundog_state_get( sundog_engine* s, int io ) //io: 0 - app input; 1 - app output;
{
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    sundog_state* state = NULL;
    if( io == 0 )
    {
	state = atomic_exchange( &sd->in_state, (sundog_state*)NULL );
    }
    else
    {
	state = atomic_exchange( &sd->out_state, (sundog_state*)NULL );
    }
    return state;
}

NSString* imgfile_to_jpeg( const char* src_path, bool delete_src_file )
{
    NSString* src_path_ns = [NSString stringWithUTF8String:src_path];
    NSData* src_file_data = [NSData dataWithContentsOfFile:src_path_ns];
    if( !src_file_data ) return nullptr;
    UIImage* image = [UIImage imageWithData:src_file_data];
    if( !image ) return nullptr;
    NSData* jpeg_data = UIImageJPEGRepresentation(image, 0.8);
    if( !jpeg_data ) return nullptr;
    
    NSString* dest_path_ns = [ NSTemporaryDirectory() stringByAppendingString:@"converted_image.jpg" ];
    if( [ jpeg_data writeToFile:dest_path_ns atomically:NO ] )
    {
        if( delete_src_file )
        {
            NSError* error;
            [ [ NSFileManager defaultManager ] removeItemAtPath:src_path_ns error:&error ];
        }
        return dest_path_ns;
    }
    
    return nullptr;
}

int sundog_state_import_from_url( ios_sundog_engine* sd, NSURL* url, uint32_t flags )
{
    const char* path = [ [ url path ] UTF8String ];
    if( !path ) return -1;
    while( 1 )
    {
        FILE* f = fopen( path, "rb" );
        if( f )
        {
            fclose( f );
            break;
        }
        flags |= SUNDOG_STATE_TEMP;
        flags &= ~SUNDOG_STATE_ORIGINAL;
        if( [ url startAccessingSecurityScopedResource ] )
        {
            NSError* error;
            NSString* dest = [ NSTemporaryDirectory() stringByAppendingString:[ url lastPathComponent ] ];
            const char* dest_cstr = [ dest UTF8String ];
            [ [ NSFileManager defaultManager ] removeItemAtPath:dest error:&error ]; //remove previous copy of this file
            if( [ [ NSFileManager defaultManager ] copyItemAtPath:[ url path ] toPath:dest error:&error ] == YES )
            {
                path = dest_cstr;
            }
            else
            {
                printf( "Can't copy from %s to %s : %s\n", path, dest_cstr, [ [ error localizedDescription ] UTF8String ] );
                path = NULL;
            }
            [ url stopAccessingSecurityScopedResource ];
            break;
        }
        printf( "Can't open %s\n", path );
        path = NULL;
        break;
    }
    if( path )
    {
        if( strstr( path, ".heic" ) || strstr( path, ".heif" ) || strstr( path, ".HEIC" ) || strstr( path, ".HEIF" ) )
        {
            printf( "HEIC -> JPEG...\n" );
            NSString* new_file = imgfile_to_jpeg( path, ( flags & SUNDOG_STATE_TEMP ) != 0 );
            if( new_file )
            {
                path = [ new_file UTF8String ];
                flags |= SUNDOG_STATE_TEMP;
                flags &= ~SUNDOG_STATE_ORIGINAL;
            }
        }
    }
    if( path )
    {
	printf( "Opening %s (", path );
	if( flags & SUNDOG_STATE_TEMP ) printf( "T" );
	if( flags & SUNDOG_STATE_ORIGINAL ) printf( "O" );
	printf( ") ...\n" );
	sundog_state_set_( sd, 0, sundog_state_new( path, flags ) );
	return 0;
    }
    return -1;
}

void ios_sundog_gl_lock( sundog_engine* s )
{
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    pthread_mutex_lock( &sd->gl_mutex );
}

void ios_sundog_gl_unlock( sundog_engine* s )
{
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    pthread_mutex_unlock( &sd->gl_mutex );
}

void ios_sundog_screen_redraw( sundog_engine* s )
{
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    [ sd->view redrawAll ];
}

void ios_sundog_event_handler( window_manager* wm )
{
    HANDLE_THREAD_EVENTS;
    ios_sundog_engine* sd = (ios_sundog_engine*)wm->sd->device_specific;
    if( sd->eventloop_stop_request )
    {
	slog( "SunDog event loop stopped\n" );
#ifndef SUNDOG_MODULE
	bool ss_pause = false;
	uint32_t ss_idle_start = wm->sd->ss_idle_frame_counter;
	uint32_t ss_sample_rate = wm->sd->ss_sample_rate;
#endif
	win_suspend( true, wm );
#ifdef NO_BACKGROUND_ACTIVITY
	usleep( 1000000 ); //extra time to save state in background tasks
        /* BUT if there is usleep() in the background thread, iOS may not wake up this thread at all :(
           and it will not be able to save its data anyway... */
        slog( "SunDog event loop stopped [2]\n" );
#endif
	sd->eventloop_stop_answer = true;
	while( sd->eventloop_stop_request )
	{
#ifndef SUNDOG_MODULE
	    if( ss_sample_rate && wm->sd->ss_idle_frame_counter - ss_idle_start > ss_sample_rate * 60 * 4 )
	    {
		if( !ss_pause )
		{
		    slog( "sundog_sound_device_stop...\n" );
		    win_suspend_devices( true, wm );
		    //exit( 0 ); //https://developer.apple.com/library/ios/#qa/qa2008/qa1561.html
		    ss_pause = true;
		    //... and then app will be frozen (because audiosession was closed in sundog_sound_device_stop())
		}
	    }
#endif
	    usleep( 1000 * 200 );
	}
	sd->eventloop_stop_answer = false;
	win_suspend( false, wm );
	win_suspend_devices( false, wm );
	slog( "SunDog event loop started\n" );
    }
    if( sd->state_req_r != sd->state_req )
    {
	sd->state_req_r = sd->state_req;
	slog( "state request...\n" );
	//Input:
	if( atomic_load( &sd->in_state ) != NULL )
	{
	    send_event( wm->root_win, EVT_LOADSTATE, wm );
	}
	//Output:
	if( sd->out_state_req )
	{
	    sd->out_state_req = false;
	    send_event( wm->root_win, EVT_SAVESTATE, wm );
	}
    }
}

// 5MB max per item in iOS clipboard
#define BM_CLIPBOARD_CHUNK_SIZE ( 5 * 1024 * 1024 )

int ios_sundog_copy( sundog_engine* s, const char* filename, uint32_t flags )
{
    int rv = -1;

    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    
    while( 1 )
    {
	NSString* dataType = (NSString*)kUTTypeUTF8PlainText;
	int ctype = sfs_get_clipboard_type( sfs_get_file_format( filename, 0 ) );
	switch( ctype )
	{
	    case sclipboard_type_image: dataType = (NSString*)kUTTypeImage; break;
	    case sclipboard_type_audio: dataType = (NSString*)kUTTypeAudio; break;
	    case sclipboard_type_video: dataType = (NSString*)kUTTypeVideo; break;
	    case sclipboard_type_movie: dataType = (NSString*)kUTTypeMovie; break;
	    case sclipboard_type_av: dataType = (NSString*)kUTTypeAudiovisualContent; break;
	    default: break;
	}

        // More types:
        // kUTTypeAudiovisualContent - An abstract type identifier for audio and/or video content.
        // kUTTypeMovie - An abstract type identifier for a media format which may contain both video and audio. Corresponds to what users would label a "movie"
        // kUTTypeVideo - An abstract type identifier for pure video data (no audio).
	// kUTTypeAudio - An abstract type identifier for pure audio data (no video).
	// kUTTypePlainText - The type identifier for text with no markup and in an unspecified encoding.
	// kUTTypeUTF8PlainText - The type identifier for plain text in a UTF-8 encoding. 

	UIPasteboard* board = [ UIPasteboard generalPasteboard ];
	
	//Open file as binary data
	char* new_filename = sfs_make_filename( s, filename, true );
	NSData* dataFile = [ NSData dataWithContentsOfFile:[ NSString stringWithUTF8String:new_filename ] ];
	smem_free( new_filename );
	if( !dataFile )
	{
	    NSLog( @"ios_sundog_copy: Can't open file" );
	    break;
	}
	
	//Create chunked data and append to clipboard
	NSUInteger sz = [ dataFile length ];
	NSUInteger chunkNumbers = ( sz / BM_CLIPBOARD_CHUNK_SIZE ) + 1;
	NSMutableArray* items = [ NSMutableArray arrayWithCapacity:chunkNumbers ];
	NSRange curRange;
	
	for( NSUInteger i = 0; i < chunkNumbers; i++ ) 
	{
	    curRange.location = i * BM_CLIPBOARD_CHUNK_SIZE;
	    curRange.length = MIN( BM_CLIPBOARD_CHUNK_SIZE, sz - curRange.location );
	    NSData* subData = [ dataFile subdataWithRange:curRange ];
	    NSDictionary* dict = [ NSDictionary dictionaryWithObject:subData forKey:dataType ];
	    [ items addObject:dict ];
	    rv = 0;
	}
	
	board.items = items;
	
	break;
    }
    
    [ pool release ];
    
    return rv;
}

char* ios_sundog_paste( sundog_engine* s, int type, uint32_t flags )
{
    char* rv = NULL;
    
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    
    while( 1 )
    {
	UIPasteboard* board = [ UIPasteboard generalPasteboard ];

	NSString* dataType = (NSString*)kUTTypeUTF8PlainText;
	switch( type )
	{
	    case sclipboard_type_utf8_text: break;
	    case sclipboard_type_image: dataType = (NSString*)kUTTypeImage; break;
	    case sclipboard_type_audio: dataType = (NSString*)kUTTypeAudio; break;
	    case sclipboard_type_video: dataType = (NSString*)kUTTypeVideo; break;
	    case sclipboard_type_movie: dataType = (NSString*)kUTTypeMovie; break;
	    case sclipboard_type_av: dataType = (NSString*)kUTTypeAudiovisualContent; break;
	    default: break;
	}
	
	NSArray* typeArray = [ NSArray arrayWithObject:dataType ];
	NSIndexSet* set = [ board itemSetWithPasteboardTypes:typeArray ];
	if( !set ) 
	{
	    NSLog( @"ios_sundog_paste: Can't get item set" );
	    break;
	}
	
	NSArray* items = [ board dataForPasteboardType:dataType inItemSet:set ];
	if( items ) 
	{
	    size_t cnt = [ items count ];
	    if( !cnt ) 
	    {
		NSLog( @"ios_sundog_paste: Nothing to paste" );
		break;
	    }
	    
	    //Create a file and write each chunks to it:
	    NSString* path = [ NSTemporaryDirectory() stringByAppendingPathComponent:@"temp-pasteboard" ];
	    if( ! [ [ NSFileManager defaultManager ] createFileAtPath:path contents:nil attributes:nil ] ) 
	    {
		NSLog( @"ios_sundog_paste: Can't create file" );
	    }
	    
	    NSFileHandle* handle = [ NSFileHandle fileHandleForWritingAtPath:path ];
	    if( !handle ) 
	    {
		NSLog( @"ios_sundog_paste: Can't open file for writing" );
		break;
	    }
	    
	    //Write each chunk to file:
	    for( UInt32 i = 0; i < cnt; i++ ) 
	    {
		[ handle writeData:[ items objectAtIndex:i ] ];
	    }
	    [ handle closeFile ];

	    rv = strdup( [ path UTF8String ] );
	}
	
	break;
    }

    [ pool release ];
    
    return rv;
}

static UIApplication* get_UIApplication( sundog_engine* s )
{
#ifndef SUNDOG_MODULE
    return [ UIApplication sharedApplication ];
#endif
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    if( !sd ) return nil;
    if( !sd->view_controller ) return nil;
    UIResponder* responder = sd->view_controller;
    while ((responder = [responder nextResponder]) != nil) {
        const char* n = object_getClassName( responder );
        if( strcmp( n, "UIApplication" ) == 0 )
        {
            return (UIApplication*)responder;
        }
    }
    return nil;
}

void ios_sundog_open_url( sundog_engine* s, const char* url_text )
{
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    
    NSURL* url = [ NSURL URLWithString:[ NSString stringWithUTF8String:url_text ] ];
    dispatch_async( dispatch_get_main_queue(), ^{
        UIApplication* app = get_UIApplication( s );
        if( app ) [ app openURL:url options:@{} completionHandler:nil ];
    } );

    [ pool release ];
}

void ios_sundog_send_text_to_email( sundog_engine* s, const char* email_text, const char* subj_text, const char* body_text )
{
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    
    NSString* mailLinkTemplate = @"mailto:%@?subject=%@&body=%@"; // to subject body
    NSString* body = [ NSString stringWithUTF8String:body_text ];
    
    NSString* bodyEncoded = [ body stringByAddingPercentEscapesUsingEncoding: NSUTF8StringEncoding ];
    NSString* themeEncoded = [ [ NSString stringWithUTF8String:subj_text ] stringByAddingPercentEscapesUsingEncoding: NSUTF8StringEncoding ];
    NSString* emailEncoded = [ [ NSString stringWithUTF8String:email_text ] stringByAddingPercentEscapesUsingEncoding: NSUTF8StringEncoding ];
    
    NSString* eMailURL = [ NSString stringWithFormat: mailLinkTemplate, emailEncoded, themeEncoded, bodyEncoded ];
    dispatch_async( dispatch_get_main_queue(), ^{
        UIApplication* app = get_UIApplication( s );
        if( app ) [ app openURL:[ NSURL URLWithString:eMailURL ] options:@{} completionHandler:nil ];
    } );

    [ pool release ];
}

void ios_sundog_send_file_to_gallery( sundog_engine* s, const char* file_path )
{
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];

    switch( sfs_get_file_format( file_path, 0 ) )
    {
	case SFS_FILE_FMT_JPEG:
	case SFS_FILE_FMT_PNG:
	case SFS_FILE_FMT_GIF:
	    {
		UIImage* img = [ UIImage imageWithContentsOfFile:[ NSString stringWithUTF8String:file_path ] ];
		if( img == 0 )
		{
		    slog( "Can't load image %s\n", file_path );
		}
		else
		{
		    UIImageWriteToSavedPhotosAlbum( img, 0, 0, 0 );
		    slog( "Sent to Photos (%s)\n", file_path );
		}
	    }
	    break;
	case SFS_FILE_FMT_AVI:
	    {
		if( UIVideoAtPathIsCompatibleWithSavedPhotosAlbum( [ NSString stringWithUTF8String:file_path ] ) )
		{
		    UISaveVideoAtPathToSavedPhotosAlbum( [ NSString stringWithUTF8String:file_path ], 0, 0, 0 );
		    slog( "Sent to Videos (%s)\n", file_path );
		}
		else
		{
		    slog( "Video format is not supported (%s)\n", file_path );
		}
	    }
	    break;
	default:
	    break;
    }

    [ pool release ];
}

void ios_sundog_bluetooth_midi_settings( sundog_engine* s )
{
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    [ sd->view showBluetoothMIDICentral ];
}

int ios_sundog_export_import_file( sundog_engine* s, const char* filename, uint32_t flags )
{
    ios_sundog_engine* sd = (ios_sundog_engine*)s->device_specific;
    return [ sd->view showExportImport:flags withFile:filename ];
}

//
// Main View Controller
//

@implementation MainViewController
{
    ios_sundog_engine* sd;
    BOOL shouldHideStatusBar;
}

- (void)viewDidLoad //Called after the controller's view is loaded into memory (after the view initWithCoder)
{
    [ super viewDidLoad ];
    shouldHideStatusBar = YES;
    [ self setNeedsStatusBarAppearanceUpdate ];
}

- (BOOL)shouldAutorotate
{
    return YES;
}

//only if UIViewControllerBasedStatusBarAppearance = YES in plist
- (BOOL)prefersStatusBarHidden
{
    if( g_app_window_flags & WIN_INIT_FLAG_SHRINK_DESKTOP_TO_SAFE_AREA )
    {
	return shouldHideStatusBar;
    }
    return YES;
}

- (UIStatusBarStyle)preferredStatusBarStyle
{
    return UIStatusBarStyleLightContent;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    /*
     Normally, the screen-edge gestures defined by the system take precedence over any gesture recognizers that you define.
     The system uses its gestures to implement system-level behaviors, such as to display Control Center.
     However, immersive apps can use this property to allow app-defined gestures to take precedence over the system gestures.
     You do that by overriding this property and returning the screen edges for which your gestures should take precedence.
    */
    return UIRectEdgeTop; //чтобы control center не мешал элементам интерфейса в верхней части экрана
}

- (void)showStatusBar:(BOOL)show
{
    if( ( show == false ) != shouldHideStatusBar )
    {
	shouldHideStatusBar = ( show == false );
	[ self setNeedsStatusBarAppearanceUpdate ];
    }
}

- (void)setSunDog:(ios_sundog_engine*)s //connect already created controller to the SunDog app
{
    sd = s;
}

/*- (BOOL)prefersHomeIndicatorAutoHidden
{
    return YES;
}*/

@end

//
// MyView
//

@implementation MyView
{
    ios_sundog_engine* sd;
    uint32_t key_mods; //current key modifiers

    EAGLContext* context;

    // The pixel dimensions of the backbuffer
    GLint backingWidth;
    GLint backingHeight;

    // OpenGL names for the renderbuffer and framebuffers used to render to this view
    GLuint viewFramebuffer, viewRenderbuffer, depthRenderbuffer;

    //Export/Import:
    char* exportFileDelete;
    uint32_t eiFlags;
}

+ (Class)layerClass
{
    return [ CAEAGLLayer class ];
}

- (id)initWithCoder:(NSCoder*)coder //Init the view from the Interface Builder files
{
    //call the init method of our parent view
    self = [ super initWithCoder:coder ];
    
    if( !self )
	return nil;
    
    sd = NULL;
    viewFramebuffer = 0;
    viewRenderbuffer = 0;
    depthRenderbuffer = 0;
    self.keys = nil;
    
    self.multipleTouchEnabled = YES;
    
    //Init the layer:
    CAEAGLLayer* eaglLayer = (CAEAGLLayer*) self.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = [
	NSDictionary dictionaryWithObjectsAndKeys:
#ifdef GLNORETAIN
	[ NSNumber numberWithBool:FALSE ],
#else
	[ NSNumber numberWithBool:TRUE ],
#endif
	kEAGLDrawablePropertyRetainedBacking,
#ifdef COLOR32BITS
        kEAGLColorFormatRGBA8,
#else
	kEAGLColorFormatRGB565,
#endif
	kEAGLDrawablePropertyColorFormat, nil
    ];
    
    //Create context:
    context = [ [ EAGLContext alloc ] initWithAPI:kEAGLRenderingAPIOpenGLES2 ];
    
    return self;
}

- (void)didMoveToWindow //Tells the view that its window object changed
{
    [ super didMoveToWindow ];
    //self.contentScaleFactor = CONTENT_SCALE_FACTOR; //TODO: not required here? Need to check.
}

- (void)setSafeArea
{
    if( sd == NULL ) return;
    if( @available( iOS 11.0, * ) )
    {
	int x = self.safeAreaInsets.left * self.contentScaleFactor;
	int y = self.safeAreaInsets.top * self.contentScaleFactor;
	int x2 = self.safeAreaInsets.right * self.contentScaleFactor;
	int y2 = self.safeAreaInsets.bottom * self.contentScaleFactor;
	int w = sd->s.screen_xsize - x - x2;
	int h = sd->s.screen_ysize - y - y2;
	sd->s.screen_safe_area[ 0 ] = x;
	sd->s.screen_safe_area[ 1 ] = y;
	sd->s.screen_safe_area[ 2 ] = w;
	sd->s.screen_safe_area[ 3 ] = h;
	//printf( "SAFE AREA: %d %d %d %d\n", x, y, x2, y2 );
#ifndef SUNDOG_MODULE
	if( y2 >= 30*self.contentScaleFactor && sd->s.screen_xsize < sd->s.screen_ysize )
	    [ sd->view_controller showStatusBar:YES ];
	else
	    [ sd->view_controller showStatusBar:NO ];
#endif
    }
}

- (BOOL)createFramebuffer
{
    bool rv = NO;
    slog( "createFramebuffer() ...\n" );
    if( sd == NULL ) return NO;
    if( viewFramebuffer ) return YES; //already created
    
    ios_sundog_gl_lock( &sd->s );
    
    while( 1 )
    {
	if( context == 0 ) break;
	if( ![ EAGLContext setCurrentContext:context ] ) break;
    
	glGenFramebuffers( 1, &viewFramebuffer );
	glGenRenderbuffers( 1, &viewRenderbuffer );
    	sd->s.view_framebuffer = viewRenderbuffer;
    
	glBindFramebuffer( GL_FRAMEBUFFER, viewFramebuffer );
	glBindRenderbuffer( GL_RENDERBUFFER, viewRenderbuffer );
	[ context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(id<EAGLDrawable>)self.layer ];
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderbuffer );
	
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth );
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight );
	//printf( "Framebuffer size: %d %d\n", backingWidth, backingHeight );
	sd->s.screen_xsize = backingWidth;
	sd->s.screen_ysize = backingHeight;
        sd->s.screen_ppi = 160 * self.contentScaleFactor; //CONTENT_SCALE_FACTOR;
        //160 must be replaced by 163 for iPhone and 132 for iPad?
	[ self setSafeArea ];
	COMPILER_MEMORY_BARRIER();
	sd->s.screen_changed_w++;
    
#ifdef GLZBUF
	glGenRenderbuffers( 1, &depthRenderbuffer );
	glBindRenderbuffer( GL_RENDERBUFFER, depthRenderbuffer );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, backingWidth, backingHeight );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer );
#endif
    
	if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
	{
	    NSLog( @"failed to make complete framebuffer object %x", glCheckFramebufferStatus( GL_FRAMEBUFFER ) );
	    break;
	}
	
	rv = YES;
	break;
    }
    
    ios_sundog_gl_unlock( &sd->s );
    
    return rv;
}

- (void)removeFramebuffer
{
    slog( "removeFramebuffer() ...\n" );
    if( sd == NULL ) return;
   
    ios_sundog_gl_lock( &sd->s );
    
    while( 1 )
    {
	if( context == 0 ) break;
	if( ![ EAGLContext setCurrentContext:context ] ) break;

	if( viewFramebuffer )
	{
	    glDeleteFramebuffers( 1, &viewFramebuffer );
	    viewFramebuffer = 0;
	    sd->s.view_framebuffer = 0;
	}
	if( viewRenderbuffer )
	{
	    glDeleteRenderbuffers( 1, &viewRenderbuffer );
	    viewRenderbuffer = 0;
	}
	if( depthRenderbuffer )
	{
	    glDeleteRenderbuffers( 1, &depthRenderbuffer );
	    depthRenderbuffer = 0;
	}
	
	break;
    }
    
    ios_sundog_gl_unlock( &sd->s );
}

- (void)setSunDog:(ios_sundog_engine*)s //connect already created view to the SunDog app
{
    sd = s;
    sd->gl_context = context;
}

static NSString* get_appsupport_file_content( NSString* fname )
{
    NSArray* paths = NSSearchPathForDirectoriesInDomains( NSApplicationSupportDirectory, NSUserDomainMask, YES );
    NSString* dir = [paths objectAtIndex:0];
    dir = [dir stringByAppendingFormat:@"/%@", [NSString stringWithUTF8String:g_app_name_short]];
    NSString* path = [dir stringByAppendingPathComponent:fname];
    NSError* err;
    NSString* content = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&err];
    return content;
}

- (int)viewInit
{
    slog( "viewInit() ...\n" );

    //1.0 - logical coordinate system; points instead of pixels; PPI ~160;
    //2.0 - x2 resolution;
    //...
    //[ UIScreen mainScreen ].nativeScale - native resolution.
    //
    //When [ UIScreen mainScreen ].nativeScale != [ UIScreen mainScreen ].scale (for example 2.6 != 3.0 on iPhone 8 Plus) ->
    // -> iOS first renders the content at the SCALE factor (desired resolution with integer scale factor)
    // and then scales it to the native resolution (lower than desired).
    float scale = 1.0f;
#ifdef NATIVE_SCREEN_HIRES
    scale = [ UIScreen mainScreen ].nativeScale;
#else
    scale = 1.0f;
#endif
    NSString* opt_res = get_appsupport_file_content( @"opt_resolution" );
    if( opt_res )
    {
        if( [ opt_res containsString:@"low" ] ) scale = 1.0f; //low resolution
        if( [ opt_res containsString:@"high" ] ) scale = [ UIScreen mainScreen ].nativeScale; //high resolution
    }
    self.contentScaleFactor = scale;

    if( ![ self createFramebuffer ] )
    {
	[ self release ];
	return -1;
    }
#ifndef SUNDOG_MODULE
    [ self changeOrientation: [ [ UIApplication sharedApplication ] statusBarOrientation ] ];
    [ [ NSNotificationCenter defaultCenter ] addObserver:self selector:@selector(statusBarOrientationChange:) name:UIApplicationWillChangeStatusBarOrientationNotification object:nil ];
    [ [ NSNotificationCenter defaultCenter ] addObserver:self selector:@selector(audioSessionInterruptionChange:) name:AVAudioSessionInterruptionNotification object:nil ];
#endif
    return 0;
}

- (void)viewDeinit
{
    slog( "viewDeinit() ...\n" );
    self.keys = nil;
    [ self removeFramebuffer ];
    [ [ NSNotificationCenter defaultCenter ] removeObserver:self ];
}

#ifndef SUNDOG_MODULE
- (void)changeOrientation:(UIInterfaceOrientation)orient
{
    if( sd == NULL ) return;
    switch( orient )
    {
	case UIInterfaceOrientationPortrait: sd->s.screen_orientation = 0; break;
	case UIInterfaceOrientationPortraitUpsideDown: sd->s.screen_orientation = -2; break;
	case UIInterfaceOrientationLandscapeRight: sd->s.screen_orientation = -3; break; //home button on the right side
	case UIInterfaceOrientationLandscapeLeft: sd->s.screen_orientation = -1; break; //home button on the left side
	default: sd->s.screen_orientation = 0; break;
    }
}
- (void)statusBarOrientationChange:(NSNotification *)notification
{
    UIInterfaceOrientation orient = (UIInterfaceOrientation)[ notification.userInfo[UIApplicationStatusBarOrientationUserInfoKey ] integerValue ];
    [ self changeOrientation:orient ];
}
- (void)audioSessionInterruptionChange:(NSNotification *)notification
{
    if( sd == NULL ) return;
    if( sd->s.ss == NULL ) return;
    NSInteger type = [ notification.userInfo[ AVAudioSessionInterruptionTypeKey ] integerValue ];
    if( type == AVAudioSessionInterruptionTypeBegan )
    {
	//the system has interrupted your audio session:
	printf( "AUDIO SESSION INTERRUPT: BEGIN\n" );
#ifdef AUDIOBUS
	if( g_ab_connected )
	{
	    printf( "Audio session interrupted while connected to AB, restarting...\n" );
	    audiobus_connection_state_update( &sd->s );
	}
#endif
    }
    else
    {
	//the interruption has ended:
	printf( "AUDIO SESSION INTERRUPT: END\n" );
    }
}
#ifdef AUDIOBUS
- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if( context == kAudiobusConnectedChanged )
    {
	audiobus_connection_state_update( &sd->s );
    }
    else
    {
	[ super observeValueForKeyPath:keyPath ofObject:object change:change context:context ];
    }
}
#endif
#endif

- (void)layoutSubviews //Place where you can perform more precise layout of the subviews
{
    slog( "layoutSubviews() ...\n" );
    if( sd == NULL ) return;
   
    //self.contentScaleFactor = CONTENT_SCALE_FACTOR; //TODO: not required here? Need to check.
    CGRect b = self.bounds;
    if( sd->s.screen_xsize != (int)b.size.width || sd->s.screen_ysize != (int)b.size.height )
    {
	ios_sundog_gl_lock( &sd->s );
	[ self removeFramebuffer ];
	[ self createFramebuffer ];
	ios_sundog_gl_unlock( &sd->s );
    }
    else
    {
	slog( "layoutSubviews: we can use the same GL framebuffer\n" );
    }
    slog( "layoutSubviews() OK: %dx%d\n", (int)b.size.width, (int)b.size.height );
}

- (void)safeAreaInsetsDidChange
{
    if( sd == NULL ) return;
    [ self setSafeArea ];
    COMPILER_MEMORY_BARRIER();
    sd->s.screen_changed_w++;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    if( sd == NULL ) return;
    NSEnumerator* e = [ touches objectEnumerator ];
    UITouch* touch;
    while( ( touch = [ e nextObject ] ) )
    {
	CGPoint pos = [ touch locationInView: self ];
	pos.x *= self.contentScaleFactor;
	pos.y *= self.contentScaleFactor;
        //Если нажатие очень близко к краю экрана, то BEGIN не приходит сразу:
        //как только нажатие завершается - одномоментно приходят BEGIN и END.
	collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, key_mods, pos.x, pos.y, 1024, (WM_TOUCH_ID)touch, &sd->s.wm );
    }
    send_touch_events( &sd->s.wm );
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    if( sd == NULL ) return;
    size_t num = [ touches count ];
    NSEnumerator* e = [ touches objectEnumerator ];
    UITouch* touch;
    int touch_count = 0;
    while( ( touch = [ e nextObject ] ) )
    {
	uint evt_flags = key_mods;
	if( touch_count < num - 1 )
	{
	    evt_flags |= EVT_FLAG_DONTDRAW;
	    //for example, if we have 3 points (fingers) for the "touch move" event, we can redraw UI on the last point only.
	    //in SunDog there are no other ways to pack several multitouch events to the single event.
	}
	CGPoint pos = [ touch locationInView: self ];
	pos.x *= self.contentScaleFactor;
	pos.y *= self.contentScaleFactor;
	collect_touch_events( 1, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, pos.x, pos.y, 1024, (WM_TOUCH_ID)touch, &sd->s.wm );
	touch_count++;
    }
    send_touch_events( &sd->s.wm );
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    if( sd == NULL ) return;
    NSEnumerator* e = [ touches objectEnumerator ];
    UITouch* touch;
    while( ( touch = [ e nextObject ] ) )
    {
	CGPoint pos = [ touch locationInView: self ];
	pos.x *= self.contentScaleFactor;
	pos.y *= self.contentScaleFactor;
	collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, key_mods, pos.x, pos.y, 1024, (WM_TOUCH_ID)touch, &sd->s.wm );
    }
    send_touch_events( &sd->s.wm );
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    if( sd == NULL ) return;
    NSEnumerator* e = [ touches objectEnumerator ];
    UITouch* touch;
    while( ( touch = [ e nextObject ] ) )
    {
	CGPoint pos = [ touch locationInView: self ];
	pos.x *= self.contentScaleFactor;
	pos.y *= self.contentScaleFactor;
	collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, key_mods, pos.x, pos.y, 1024, (WM_TOUCH_ID)touch, &sd->s.wm );
    }
    send_touch_events( &sd->s.wm );
}

- (BOOL)canBecomeFirstResponder
{
    return YES; //for keyboard support
}

- (NSArray *)keyCommands
{
    if( @available( iOS 13.4, * ) ) return nil;
    if( self.keys == nil )
    {
	id* cmds = (id*)malloc( sizeof( id ) * 8192 );
	if( cmds )
	{
	    int cnt = 0;
	    for( int i = 0; i < 16; i++ )
	    {
		uint mods = 0;
		if( i & 1 ) mods |= UIKeyModifierShift;
		if( i & 2 ) mods |= UIKeyModifierControl;
		if( i & 4 ) mods |= UIKeyModifierAlternate;
		if( i & 8 ) mods |= UIKeyModifierCommand;
	    	cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:mods action:@selector(someKeyPress:) ];
	    	cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:mods action:@selector(someKeyPress:) ];
	    	cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:mods action:@selector(someKeyPress:) ];
	    	cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:mods action:@selector(someKeyPress:) ];
		cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:mods action:@selector(someKeyPress:) ];
		cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:@"\b" modifierFlags:mods action:@selector(someKeyPress:) ];
		cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:mods action:@selector(someKeyPress:) ];
		cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:@"\r" modifierFlags:mods action:@selector(someKeyPress:) ];
		for( int c = 0x20; c <= 0x7F; c++ )
		{
		    if( c >= 0x41 && c <= 0x5A ) continue; //ignore capital chars, because we have SHIFT mod
		    unichar cc = c;
		    cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:[ NSString stringWithCharacters:&cc length:1 ] modifierFlags:mods action:@selector(someKeyPress:) ];
		}
		// !!! UNDOCUMENTED !!!
		cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:@"UIKeyInputPageUp" modifierFlags:mods action:@selector(someKeyPress:) ];
		cmds[ cnt++ ] = [ UIKeyCommand keyCommandWithInput:@"UIKeyInputPageDown" modifierFlags:mods action:@selector(someKeyPress:) ];
		// !!!!!!!!!!!!!!!!!!!!
	    }
	    self.keys = [ NSArray arrayWithObjects:cmds count:cnt ]; //array with autorelease; internal objects are with autorelease too (array will increase the ref.count of every object)
	    //final release will be in viewDeinit: self.keys = nil
	    //printf( "KEYS COUNT %d\n", cnt );
	    free( cmds );
	}
    }
    return self.keys;
}

- (void)someKeyPress:(UIKeyCommand*)c
{
    if( sd == NULL ) return;
    int key = -1;
    uint mods = 0;
    while( 1 )
    {
	if( c.input == UIKeyInputLeftArrow ) { key = KEY_LEFT; break; }
	if( c.input == UIKeyInputRightArrow ) { key = KEY_RIGHT; break; }
	if( c.input == UIKeyInputUpArrow ) { key = KEY_UP; break; }
	if( c.input == UIKeyInputDownArrow ) { key = KEY_DOWN; break; }
	if( c.input == UIKeyInputEscape ) { key = KEY_ESCAPE; break; }
	if( [ c.input length ] == 1 )
	{
	    unichar code = [ c.input characterAtIndex:0 ];
	    switch( code )
	    {
                case 5: key = KEY_INSERT; break;
                case 8: key = KEY_BACKSPACE; break;
		case 9: key = KEY_TAB; break;
		case 0xD: key = KEY_ENTER; break;
		case 0x7F: key = KEY_DELETE; break;
		default:
		    if( code >= 0x20 && code <= 0x7E ) key = code;
		    break;
	    }
	    if( key != -1 ) break;
	}
	if( [ c.input length ] > 2 )
	{
	    if( [ c.input compare:@"UIKeyInputPageUp" ] == NSOrderedSame ) { key = KEY_PAGEUP; break; }
	    if( [ c.input compare:@"UIKeyInputPageDown" ] == NSOrderedSame ) { key = KEY_PAGEDOWN; break; }
	}
	break;
    }
    if( key != -1 )
    {
	if( c.modifierFlags & UIKeyModifierShift ) mods |= EVT_FLAG_SHIFT;
	if( c.modifierFlags & UIKeyModifierControl ) mods |= EVT_FLAG_CTRL;
	if( c.modifierFlags & UIKeyModifierAlternate ) mods |= EVT_FLAG_ALT;
	if( c.modifierFlags & UIKeyModifierCommand ) mods |= EVT_FLAG_CMD;
	send_event( 0, EVT_BUTTONDOWN, mods, 0, 0, key, key, 1024, &sd->s.wm );
	if( mods == 0 && ( ( key >= '0' && key <= '9' ) || ( key >= 0x61 && key <= 0x7A ) ) )
	    stime_sleep( 20 );
	send_event( 0, EVT_BUTTONUP, mods, 0, 0, key, key, 1024, &sd->s.wm );
    }
}

- (void)someKeyPress2:(UIKey*)c state:(int)state //state: 0 - off; 1 - on; -1 - cancelled
{
    if( sd == NULL ) return;
    int key = -1;
    uint32_t mods = 0;
    if( c.modifierFlags & UIKeyModifierShift ) mods |= EVT_FLAG_SHIFT;
    if( c.modifierFlags & UIKeyModifierControl ) mods |= EVT_FLAG_CTRL;
    if( c.modifierFlags & UIKeyModifierAlternate ) mods |= EVT_FLAG_ALT;
    if( c.modifierFlags & UIKeyModifierCommand ) mods |= EVT_FLAG_CMD;
    //if( c.modifierFlags & UIKeyModifierNumericPad ) mods |= ?;
    while( key == -1 )
    {
        if( c.charactersIgnoringModifiers.length == 0 )
        {
            uint32_t m = 0;
            if( mods & EVT_FLAG_SHIFT ) { m = EVT_FLAG_SHIFT; key = KEY_SHIFT; }
            if( mods & EVT_FLAG_CTRL ) { m = EVT_FLAG_CTRL; key = KEY_CTRL; }
            if( mods & EVT_FLAG_ALT ) { m = EVT_FLAG_ALT; key = KEY_ALT; }
            if( mods & EVT_FLAG_CMD ) { m = EVT_FLAG_CMD; key = KEY_CMD; }
            if( m )
            {
                mods &= ~m;
                if( state == 1 )
                    key_mods |= m;
                else
                    key_mods &= ~m;
            }
            break;
        }
        if( c.charactersIgnoringModifiers.length == 1 )
        {
            unichar code = [ c.charactersIgnoringModifiers characterAtIndex:0 ];
            switch( code )
            {
                case 5: key = KEY_INSERT; break;
                case 8: key = KEY_BACKSPACE; break;
                case 9: key = KEY_TAB; break;
                case 0xD: key = KEY_ENTER; break;
                case 0x7F: key = KEY_DELETE; break;
                default:
                    if( code >= 0x20 && code <= 0x7E ) key = code;
                    break;
            }
            if( key != -1 ) break;
        }
        if( c.charactersIgnoringModifiers == UIKeyInputEscape ) { key = KEY_ESCAPE; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF1 ) { key = KEY_F1; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF2 ) { key = KEY_F2; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF3 ) { key = KEY_F3; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF4 ) { key = KEY_F4; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF5 ) { key = KEY_F5; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF6 ) { key = KEY_F6; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF7 ) { key = KEY_F7; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF8 ) { key = KEY_F8; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF9 ) { key = KEY_F9; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF10 ) { key = KEY_F10; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF11 ) { key = KEY_F11; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputF12 ) { key = KEY_F12; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputUpArrow ) { key = KEY_UP; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputDownArrow ) { key = KEY_DOWN; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputLeftArrow ) { key = KEY_LEFT; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputRightArrow ) { key = KEY_RIGHT; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputHome ) { key = KEY_HOME; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputEnd ) { key = KEY_END; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputPageUp ) { key = KEY_PAGEUP; break; }
        if( c.charactersIgnoringModifiers == UIKeyInputPageDown ) { key = KEY_PAGEDOWN; break; }
        break;
    }
    if( key == -1 )
    {
        if( c.modifierFlags & UIKeyModifierAlphaShift )
            key = KEY_CAPS;
        else
            key = KEY_UNKNOWN + (int)c.keyCode;
    }
    send_event(
        NULL,
        state == 1 ? EVT_BUTTONDOWN : EVT_BUTTONUP,
        mods | EVT_FLAG_AUTOREPEAT,
        0, 0, //xy
        key,
        (int)c.keyCode,
        1024,
        &sd->s.wm );
}

- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
    for( UIPress* press in presses )
        [ self someKeyPress2:press.key state:1 ];
    //[ super pressesBegan:presses withEvent:event ];
}

- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
    for( UIPress* press in presses )
        [ self someKeyPress2:press.key state:0 ];
    //[ super pressesEnded:presses withEvent:event ];
}

- (void)pressesCancelled:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
    for( UIPress* press in presses )
        [ self someKeyPress2:press.key state:-1 ];
    //[ super pressesCancelled:presses withEvent:event ];
}

- (void)redrawAll
{
    if( sd == NULL ) return;
    
    //Invoke setNeedsDisplay method on the main thread:
    //[ self performSelectorOnMainThread: @selector(setNeedsDisplay) withObject:nil waitUntilDone: NO ];
    
#ifdef AUDIOBUS_TRIGGER_PLAY
    audiobus_triggers_update();
#endif
    
    ios_sundog_gl_lock( &sd->s );
    glBindRenderbuffer( GL_RENDERBUFFER, viewRenderbuffer );
    [ context presentRenderbuffer:GL_RENDERBUFFER ];
    glBindFramebuffer( GL_FRAMEBUFFER, viewFramebuffer );
    ios_sundog_gl_unlock( &sd->s );
}

- (void)btmidiDoneAction:(id)sender
{
    if( sd == NULL ) return;
    [ sd->view_controller dismissViewControllerAnimated:YES completion:nil ];
}

- (void)showBluetoothMIDICentral2
{
    if( sd == NULL ) return;

    CABTMIDICentralViewController* viewController = [ CABTMIDICentralViewController new ];
    UINavigationController* navController = [ [ UINavigationController alloc ] initWithRootViewController:viewController ];
    // this will present a view controller as a popover in iPad and modal VC on iPhone
    viewController.navigationItem.rightBarButtonItem =
    [ [ UIBarButtonItem alloc ] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
						     target:self
						     action:@selector(btmidiDoneAction:) ];
    navController.modalPresentationStyle = UIModalPresentationPopover;
    UIPopoverPresentationController* popC = navController.popoverPresentationController;
    popC.permittedArrowDirections = UIPopoverArrowDirectionAny;
    //popC.sourceRect = [ self frame ];
    CGRect r = [ self frame ];
    r.size.height = 0;
    r.origin.x = 0;
    r.origin.y = 0;
    popC.sourceRect = r;
    popC.sourceView = self;
    [ sd->view_controller presentViewController:navController animated:YES completion:nil ];
}

- (void)showBluetoothMIDICentral
{
    [ self performSelectorOnMainThread: @selector(showBluetoothMIDICentral2) withObject:nil waitUntilDone: NO ];
}

- (void)documentPickerIMPORT:(NSURL *)url
{
    if( sd == NULL ) return;
    sundog_state_import_from_url( sd, url, SUNDOG_STATE_TEMP );
}

- (void)documentPickerFINISH
{
    if( exportFileDelete )
    {
	remove( exportFileDelete );
	free( exportFileDelete );
	exportFileDelete = NULL;
    }
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentAtURL:(NSURL *)url
{ //deprecated
    if( ( eiFlags & EIFILE_MODE ) == EIFILE_MODE_IMPORT )
    {
	[ self documentPickerIMPORT:url ];
    }
    [ self documentPickerFINISH ];
    [ controller release ];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{ //ios 11+
    if( ( eiFlags & EIFILE_MODE ) == EIFILE_MODE_IMPORT )
    {
	for( NSURL* url in urls )
	{
	    [ self documentPickerIMPORT:url ];
	}
    }
    [ self documentPickerFINISH ];
    [ controller release ];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
    [ self documentPickerFINISH ];
    [ controller release ];
}

- (void)documentInteractionControllerDidDismissOptionsMenu:(UIDocumentInteractionController *)controller
{
    [ controller release ];
}

- (int)showExportImport:(uint32_t)flags withFile:(const char*)filename
{
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];

    NSURL* url = NULL;
    eiFlags = flags;
    if( ( eiFlags & EIFILE_MODE ) == EIFILE_MODE_IMPORT )
    {
    }
    else
    {
	url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];
        //NSLog( @"URL: %@", url );
	if( filename && ( flags & EIFILE_FLAG_DELFILE ) )
	    exportFileDelete = strdup( filename ); //NOT SAFE.......
    }

    dispatch_async( dispatch_get_main_queue(), ^{
	if( sd == NULL ) return;
	if( ( eiFlags & EIFILE_MODE ) == EIFILE_MODE_EXPORT2 )
	{
            UIDocumentInteractionController* ctl = [UIDocumentInteractionController interactionControllerWithURL:url];
            ctl.delegate = self;
	    CGRect r = [ self frame ];
	    r.size.height = 0;
	    r.origin.x = 0;
	    r.origin.y = 0;
            [ ctl retain ];
	    [ ctl presentOptionsMenuFromRect:r inView:self animated:YES ];
	}
	else
	{
	    UIDocumentPickerViewController* ctl;
	    if( ( eiFlags & EIFILE_MODE ) == EIFILE_MODE_IMPORT )
		ctl = [ [ UIDocumentPickerViewController alloc ] initWithDocumentTypes:@[@"public.item"] inMode:UIDocumentPickerModeImport ];
	    else
		ctl = [ [ UIDocumentPickerViewController alloc ] initWithURL:url inMode:UIDocumentPickerModeExportToService ];
	    ctl.delegate = self;
	    [ sd->view_controller presentViewController:ctl animated:YES completion:nil ];
	    //public.text, public.image, public.audiovisual-content, ...
            //https://developer.apple.com/documentation/mobilecoreservices/uttype/uti_abstract_types?language=objc
	}
    } );

    [ pool release ];
    return 0;
}

@end

//
// App Delegate
//

#ifndef SUNDOG_MODULE

@interface AppDelegate ()
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    sundog_init( (MainViewController*)self.window.rootViewController, &g_sd );
    return YES;
}

//iOS 12.5.7: картинку нельзя передать напрямую из галереи в приложение; но можно это сделать из других мест: например, из Files;
//в новых версиях системы возможность открывать из галереи появилась.

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
    //DEPRECATED
    const char* path = [ [ url path ] UTF8String ];
    if( !path ) return NO;
    sundog_state_set_( &g_sd, 0, sundog_state_new( path, SUNDOG_STATE_TEMP ) );
    return YES;
}

- (BOOL)application:(UIApplication *)app openURL:(NSURL *)url options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options
{
    //here sundog_init() is finished, but the SunDog engine (other thread) may be not initialized yet
    uint32_t flags = SUNDOG_STATE_TEMP;
    for( UIApplicationOpenURLOptionsKey key in options )
    {
	id value = options[ key ];
	if( key == UIApplicationOpenURLOptionsOpenInPlaceKey )
	{
	    if( [ value boolValue ] == YES )
	    {
		flags &= ~SUNDOG_STATE_TEMP;
		flags |= SUNDOG_STATE_ORIGINAL;
	    }
	}
	//NSLog( @"Value: %@ for key: %@", value, key );
    }
    if( sundog_state_import_from_url( &g_sd, url, flags ) == 0 )
	return YES;
    return NO;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message)
    // or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
    sundog_pause( &g_sd );
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // Called as part of transition from the background to the active state
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
    ios_sundog_engine* sd = &g_sd;
    sd->hold = 0;
    sundog_resume( sd );
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    sundog_deinit( &g_sd );
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
    // Free up as much memory as possible by purging cached data objects that can be recreated (or reloaded from disk) later.
}

@end

//
// iOS app main()
//

int main( int argc, char* argv[] )
{
    @autoreleasepool
    {
	return UIApplicationMain( argc, argv, nil, NSStringFromClass( [ AppDelegate class ] ) );
    }
}

#endif

//
// Audio Unit Extension
//

#ifdef AUDIOUNIT_EXTENSION

#import "sundogAudioUnit.h"

@interface AudioUnitViewController ()
@end

@implementation AudioUnitViewController
{
    sundogAudioUnit* audioUnit;
    int channel_count;
    int sample_rate;
    ios_sundog_engine sd;
}

- (void) viewDidLoad
{
    [super viewDidLoad];
    NSLog( @"AU VIEW DID LOAD" );

    // Check if the Audio Unit has been loaded
    if( audioUnit )
    {
	[ self connectUIToAudioUnit ];
    }

    NSLog( @"AU VIEW DID LOAD OK" );
}

/*- (void)viewDidAppear:(BOOL)animated
{
    //AU window is open now
    [super viewWillAppear:animated];
    NSLog( @"AU VIEW WILL APPEAR" );
}

- (void)viewWillDisappear:(BOOL)animated
{
    //AU window will be closed
    [super viewDidDisappear:animated];
    NSLog( @"AU VIEW DID DISAPPEAR" );
}*/

- (AUAudioUnit *)createAudioUnitWithComponentDescription:(AudioComponentDescription)desc error:(NSError **)error
{
    sundog_init_state_atomics( &sd );

    audioUnit = [[sundogAudioUnit alloc] initWithComponentDescription:desc error:error];
    NSLog( @"NEW AU" );

    // Check if the UI has been loaded
    if( self.isViewLoaded )
    {
	[ self connectUIToAudioUnit ];
    }

    [ audioUnit setParentCtl:(NSObject*)self ];
    NSLog( @"NEW AU OK" );

    return audioUnit;
}

- (void)connectUIToAudioUnit
{
    // Get the parameter tree and add observers for any parameters that the UI needs to keep in sync with the Audio Unit
    /*
    AUParameterTree* parameterTree = self.audioUnit.parameterTree;
    if( parameterTree )
    {
	_parameterObserverToken = [parameterTree tokenByAddingParameterObserver:^(AUParameterAddress address, AUValue value) {
    	    dispatch_sync(dispatch_get_main_queue(), ^{
    	    });
        }];
    }
    */
}

- (void)auInit2
{
    sundog_init( self, &sd );
}
- (sundog_engine*)auInit:(AVAudioFormat*)format //called from AUAudioUnit (sundogAudioUnit)
{
    channel_count = format.channelCount;
    sample_rate = format.sampleRate;
    COMPILER_MEMORY_BARRIER();
    [ self performSelectorOnMainThread: @selector(auInit2) withObject:nil waitUntilDone: YES ];
    return &sd.s;
}

- (void)auDeinit2
{
    sundog_deinit( &sd );
}
- (void)auDeinit //called from AUAudioUnit
{
    [ self performSelectorOnMainThread: @selector(auDeinit2) withObject:nil waitUntilDone: YES ];
}

- (int)auGetChannelCount { return( channel_count ); }
- (int)auGetSampleRate { return( sample_rate ); }

- (void)auSetFullState: (sundog_state*)state //called from AUAudioUnit
{
    sundog_state_set_( &sd, 0, state );
}

- (sundog_state*)auGetFullState //called from AUAudioUnit
{
    sundog_state* rv = NULL;
    sd.out_state_req = true;
    COMPILER_MEMORY_BARRIER();
    sd.state_req++;
    COMPILER_MEMORY_BARRIER();
    int t = 0;
    while( 1 )
    {
	rv = sundog_state_get( &sd.s, 1 );
	if( rv ) break;
	stime_sleep( 10 );
        t += 10;
        if( t > 1000 * 5 )
        {
    	    slog( "auGetFullState() timeout" );
    	    break;
	}
    }
    COMPILER_MEMORY_BARRIER();
    sd.out_state_req = false;
    return rv;
}

@end

#endif
