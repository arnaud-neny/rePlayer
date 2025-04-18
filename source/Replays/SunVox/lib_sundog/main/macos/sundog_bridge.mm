/*
    sundog_bridge.mm - SunDog<->System bridge
    This file is part of the SunDog engine.
    Copyright (C) 2009 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#import <stdio.h>
#import <time.h>
#import <string.h>
#import <pthread.h>
#import <sys/stat.h> //mkdir
#import <atomic>

#import <OpenGL/gl.h>
#import <OpenGL/OpenGL.h>

#import "sundog.h"
#import "sundog_bridge.h"

NSOperatingSystemVersion g_os_version;

void sundog_state_set_( macos_sundog_engine* sd, int io, sundog_state* state );

//Defined in wm_macos.h:
extern void macos_sundog_touches( int x, int y, int mode, int touch_id, window_manager* );
extern void macos_sundog_key( unsigned short vk, bool pushed, window_manager* );

#define DEFAULT_PPI 110
int g_macos_sundog_screen_xsize = 320;
int g_macos_sundog_screen_ysize = 480;
int g_macos_sundog_screen_ppi = DEFAULT_PPI;
int g_macos_sundog_resize_request = 0;

char g_ui_lang[ 64 ] = { 0 };

MyView* g_sundog_view;
NSWindow* g_sundog_window;
NSOpenGLContext* g_gl_context = NULL;
void* g_gl_context_obj = NULL;

struct macos_sundog_engine
{
    int initialized;
    pthread_t pth;
    int pth_finished;
    sundog_engine s;
    
    //The following part will be cleared only once (default zeroes + sundog_init_state_atomics()):
    bool 			state_atomics_initialized;
    std::atomic<sundog_state*> 	in_state; //Input state (document)
    std::atomic<sundog_state*> 	out_state; //Output state
    volatile int 		state_req; //state requests: "input available" or/and "output requested"
    int 			state_req_r; //state_req != state_req_r --> handle state requests;
    volatile bool 		out_state_req;
};
macos_sundog_engine g_sd;

//
// Main functions, will be called from the App Delegate or from the Audio Unit Extension
//

void* sundog_thread( void* arg )
{
    macos_sundog_engine* sd = (macos_sundog_engine*)arg;

    [ g_gl_context makeCurrentContext ];
    
    sundog_main( &sd->s, true );

    sd->pth_finished = 1;
    dispatch_async( dispatch_get_main_queue(), ^{
        [ NSApp terminate: NSApp ];
    } );
    pthread_exit( NULL );
}

void sundog_init_state_atomics( macos_sundog_engine* sd )
{
    if( !sd->state_atomics_initialized )
    {
	sd->state_atomics_initialized = true;
	atomic_init( &sd->in_state, (sundog_state*)NULL );
    }
    else
    {
	sundog_state_set_( sd, 1, NULL ); //previous output (if exists) can't be used anyomore
    }
}

void sundog_init( MyView* view, macos_sundog_engine* sd )
{
    int err;
    
    printf( "sundog_init() ...\n" );
    size_t zsize = (size_t)( (char*)&sd->state_atomics_initialized - (char*)sd );
    memset( sd, 0, zsize );
    sd->s.device_specific = sd;
    sundog_init_state_atomics( sd );

    //Init the view:
    if( [ view viewInit ] ) return;
 
    //Save link to current view:
    g_sundog_view = view;
    g_sundog_window = [ view window ];
    
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
    
    printf( "sundog_init(): screen %d x %d; lang %s\n", g_macos_sundog_screen_xsize, g_macos_sundog_screen_ysize, g_ui_lang );
    
    //Create main thread:    
    err = pthread_create( &sd->pth, 0, &sundog_thread, (void*)sd );
    if( err == 0 )
    {
	//The pthread_detach() function marks the thread identified by thread as
	//detached. When a detached thread terminates, its resources are 
	//automatically released back to the system.
	err = pthread_detach( sd->pth );
	if( err != 0 )
	{
	    printf( "sundog_init(): pthread_detach error %d\n", err );
	    return;
	}
    }
    else
    {
	printf( "sundog_init(): pthread_create error %d\n", err );
	return;
    }
    
    printf( "sundog_init(): done\n" );
    sd->initialized = 1;
}

void sundog_deinit( macos_sundog_engine* sd )
{
    int err;
    
    printf( "sundog_deinit() ...\n" );
    
    if( !sd->initialized ) return;

    //Stop the thread:
    win_exit_request( &sd->s.wm );
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
	printf( "sundog_deinit(): thread timeout\n" );
	err = pthread_cancel( sd->pth );
	if( err != 0 )
	{
	    printf( "sundog_deinit(): pthread_cancel error %d\n", err );
	}
    }
    
    //Free the paths and global names:
    //...

    printf( "sundog_deinit(): done\n" );
}

//
// The following functions will be called from the SunDog (wm_macos.h and others)
//

void sundog_state_set_( macos_sundog_engine* sd, int io, sundog_state* state ) //io: 0 - app input; 1 - app output;
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
    macos_sundog_engine* sd = (macos_sundog_engine*)s->device_specific;
    if( sd ) sundog_state_set_( sd, io, state );
}

sundog_state* sundog_state_get( sundog_engine* s, int io ) //io: 0 - app input; 1 - app output;
{
    macos_sundog_engine* sd = (macos_sundog_engine*)s->device_specific;
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

void macos_sundog_screen_redraw( void )
{
    CGLFlushDrawable( (CGLContextObj)g_gl_context_obj );
}

void macos_sundog_rename_window( const char* name )
{
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    [ g_sundog_window performSelectorOnMainThread:@selector(setTitle:) withObject:[ NSString stringWithUTF8String:name ] waitUntilDone:NO ];
    [ pool release ];
}

volatile bool g_eventloop_stop_request = 0;
volatile bool g_eventloop_stop_answer = 0;

void macos_sundog_event_handler( window_manager* wm )
{
    HANDLE_THREAD_EVENTS;
    macos_sundog_engine* sd = (macos_sundog_engine*)wm->sd->device_specific;
    if( g_eventloop_stop_request )
    {
	g_eventloop_stop_answer = 1;
	while( g_eventloop_stop_request )
	{
	}
	g_eventloop_stop_answer = 0;
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

int macos_sundog_copy( sundog_engine* s, const char* filename, uint32_t flags )
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
        
        NSPasteboard* board = [ NSPasteboard generalPasteboard ];
        if( !board )
        {
            NSLog( @"macos_sundog_copy: Can't open generalPasteboard" );
            break;
        }
        
        //Open file as binary data
        char* new_filename = sfs_make_filename( s, filename, true );
        NSData* dataFile = [ NSData dataWithContentsOfFile:[ NSString stringWithUTF8String:new_filename ] ];
        smem_free( new_filename );
        if( !dataFile )
        {
            NSLog( @"macos_sundog_copy: Can't open file" );
            break;
        }
        
        [ board clearContents ];
        [ board declareTypes:[ NSArray arrayWithObject:dataType ] owner:nil ];
        [ board setData:dataFile forType:dataType ];
        
        break;
    }
    
    [ pool release ];
    
    return rv;
}

char* macos_sundog_paste( sundog_engine* s, int type, uint32_t flags )
{
    char* rv = NULL;
    
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    
    while( 1 )
    {
        NSPasteboard* board = [ NSPasteboard generalPasteboard ];
        if( !board )
        {
            NSLog( @"macos_sundog_paste: Can't open generalPasteboard" );
            break;
        }
        
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
        
        NSPasteboardType avail = [ board availableTypeFromArray:[ NSArray arrayWithObject:dataType ] ];
        if( !avail )
        {
            NSLog( @"macos_sundog_paste: Nothing to paste" );
            break;
        }
        
        NSData* data = [ board dataForType:avail ];
        if( !data )
        {
            NSLog( @"macos_sundog_paste: No data" );
            break;
        }
        
        NSString* path = [ NSTemporaryDirectory() stringByAppendingPathComponent:@"temp-pasteboard" ];
        if( ! [ [ NSFileManager defaultManager ] createFileAtPath:path contents:nil attributes:nil ] )
        {
            NSLog( @"macos_sundog_paste: Can't create file" );
        }
        NSFileHandle* handle = [ NSFileHandle fileHandleForWritingAtPath:path ];
        if( !handle )
        {
            NSLog( @"macos_sundog_paste: Can't open file for writing" );
            break;
        }
        [ handle writeData:data ];
        [ handle closeFile ];
            
        rv = strdup( [ path UTF8String ] );
        
        break;
    }
    
    [ pool release ];
    
    return rv;
}

//
// Main Window Delegate
//

@interface MainWindowDelegate : NSObject
{
}
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize;
- (void)windowDidResize:(NSNotification *)notification;
- (void)windowWillStartLiveResize:(NSNotification *)notification;
- (void)windowDidEndLiveResize:(NSNotification *)notification;
@end
@implementation MainWindowDelegate
- (void)windowDidBecomeKey:(NSNotification *)notification
{
}
- (void)windowDidResignKey:(NSNotification *)notification
{
    macos_sundog_key( 56, 0, &g_sd.s.wm ); //SHIFT up
    macos_sundog_key( 60, 0, &g_sd.s.wm ); //SHIFT up
    macos_sundog_key( 55, 0, &g_sd.s.wm ); //CMD up
    macos_sundog_key( 59, 0, &g_sd.s.wm ); //CTRL up
    macos_sundog_key( 58, 0, &g_sd.s.wm ); //ALT up
}
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
    g_eventloop_stop_request = 1;
    while( g_eventloop_stop_answer == 0 ) { usleep( 1000 * 5 ); }
    
    /*NSRect frameRect;
    frameRect.origin.x = 0;
    frameRect.origin.y = 0;
    frameRect.size.width = frameSize.width;
    frameRect.size.height = frameSize.height;
    NSRect content_frame = [ NSWindow contentRectForFrameRect:frameRect styleMask:[ sender styleMask ] ];
    g_macos_sundog_screen_xsize = content_frame.size.width * g_sundog_window.backingScaleFactor;
    g_macos_sundog_screen_ysize = content_frame.size.height * g_sundog_window.backingScaleFactor;*/

    return frameSize;
}
- (void)windowDidResize:(NSNotification *)notification
{
    if( g_sundog_view )
    {
	NSRect r = [ g_sundog_view frame ];
        g_macos_sundog_screen_xsize = r.size.width * g_sundog_window.backingScaleFactor;
        g_macos_sundog_screen_ysize = r.size.height * g_sundog_window.backingScaleFactor;
    }
    
    g_eventloop_stop_request = 0;
    while( g_eventloop_stop_answer == 1 ) { usleep( 1000 * 5 ); }
}
- (void)windowWillStartLiveResize:(NSNotification *)notification
{
}
- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    g_eventloop_stop_request = 1;
    while( g_eventloop_stop_answer == 0 ) { usleep( 1000 * 5 ); }
    
    g_macos_sundog_resize_request = 1;
    
    g_eventloop_stop_request = 0;
    while( g_eventloop_stop_answer == 1 ) { usleep( 1000 * 5 ); }
}
@end

//
// MyView
//

static MainWindowDelegate* g_main_window_delegate;

@implementation MyView

- (id) initWithFrame: (NSRect) frame
{
    GLuint attribs[] = 
    {
#ifndef GLNORETAIN
	NSOpenGLPFABackingStore,
#endif
	NSOpenGLPFANoRecovery,
	NSOpenGLPFAAccelerated,
	NSOpenGLPFADoubleBuffer,
	NSOpenGLPFAColorSize, 24,
	NSOpenGLPFAAlphaSize, 8,
#ifdef GLZBUF
        NSOpenGLPFADepthSize, 32,
#else
	NSOpenGLPFADepthSize, 0,
#endif
	NSOpenGLPFAStencilSize, 0,
	NSOpenGLPFAAccumSize, 0,
	0
    };
    
    NSOpenGLPixelFormat* fmt = [ [ NSOpenGLPixelFormat alloc ] initWithAttributes: (NSOpenGLPixelFormatAttribute*) attribs ]; 
    
    if( !fmt )
	NSLog( @"No OpenGL pixel format" );
    
    self = [ super initWithFrame:frame pixelFormat: [ fmt autorelease ] ];
    
    return self;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return YES;
}

- (BOOL)resignFirstResponder
{
    return YES;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x - 1, (int)y - 1, 0, 0, &g_sd.s.wm );
}    

- (void)mouseUp:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x - 1, (int)y - 1, 2, 0, &g_sd.s.wm );
}    

- (void)mouseDragged:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x, (int)y, 1, 0, &g_sd.s.wm );
}    

- (void)rightMouseDown:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x - 1, (int)y - 1, 0 + ( 1 << 3 ), 0, &g_sd.s.wm );
}    

- (void)rightMouseUp:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x - 1, (int)y - 1, 2 + ( 1 << 3 ), 0, &g_sd.s.wm );
}    

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x, (int)y, 1 + ( 1 << 3 ), 0, &g_sd.s.wm );
}    

- (void)otherMouseDown:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x - 1, (int)y - 1, 0 + ( 2 << 3 ), 0, &g_sd.s.wm );
}    

- (void)otherMouseUp:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x - 1, (int)y - 1, 2 + ( 2 << 3 ), 0, &g_sd.s.wm );
}    

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    macos_sundog_touches( (int)x, (int)y, 1 + ( 2 << 3 ), 0, &g_sd.s.wm );
}    

- (void)scrollWheel:(NSEvent *)theEvent
{
    NSPoint point = [ theEvent locationInWindow ];
    NSPoint local_point = [ self convertPoint: point fromView: nil ];
    float scale = self.window.backingScaleFactor;
    local_point.x *= scale;
    local_point.y *= scale;
    float x = local_point.x;
    float y = g_macos_sundog_screen_ysize - 1 - local_point.y;
    float dy = [ theEvent deltaY ];
    if( dy )
    {
	if( dy < 0 )
	    macos_sundog_touches( (int)x, (int)y, 3, 0, &g_sd.s.wm );
	else
	    macos_sundog_touches( (int)x, (int)y, 4, 0, &g_sd.s.wm );
    }
}

- (void)keyDown:(NSEvent *)theEvent
{
    unsigned short vk = [ theEvent keyCode ];
    macos_sundog_key( vk, 1, &g_sd.s.wm );
}

- (void)keyUp:(NSEvent *)theEvent
{
    unsigned short vk = [ theEvent keyCode ];
    macos_sundog_key( vk, 0, &g_sd.s.wm );
}

- (void)flagsChanged:(NSEvent *)theEvent
{
    unsigned short vk = [ theEvent keyCode ];
    bool pushed = 0;
    if( vk == 0 )
    {
        //Wrong vk. Check all modifiers:
        macos_sundog_key( 55, ( [ theEvent modifierFlags ] & NSCommandKeyMask ) != 0, &g_sd.s.wm );
        macos_sundog_key( 56, ( [ theEvent modifierFlags ] & NSShiftKeyMask ) != 0, &g_sd.s.wm );
        macos_sundog_key( 60, 0, &g_sd.s.wm );
        macos_sundog_key( 59, ( [ theEvent modifierFlags ] & NSControlKeyMask ) != 0, &g_sd.s.wm );
        macos_sundog_key( 58, ( [ theEvent modifierFlags ] & NSAlternateKeyMask ) != 0, &g_sd.s.wm );
        return;
    }
    if( vk == 55 ) //Cmd
    {
	if( [ theEvent modifierFlags ] & NSCommandKeyMask )
	    pushed = 1;
	macos_sundog_key( vk, pushed, &g_sd.s.wm );
    }
    if( vk == 56 || vk == 60 ) //Shift
    {
	if( [ theEvent modifierFlags ] & NSShiftKeyMask )
	    pushed = 1;
	macos_sundog_key( vk, pushed, &g_sd.s.wm );
    }
    if( vk == 59 ) //Ctrl
    {
	if( [ theEvent modifierFlags ] & NSControlKeyMask )
	    pushed = 1;
	macos_sundog_key( vk, pushed, &g_sd.s.wm );
    }
    if( vk == 58 ) //Alt
    {
	if( [ theEvent modifierFlags ] & NSAlternateKeyMask )
	    pushed = 1;
	macos_sundog_key( vk, pushed, &g_sd.s.wm );
    }
    if( vk == 57 ) //Caps
    {
	macos_sundog_key( vk, 1, &g_sd.s.wm );
	macos_sundog_key( vk, 0, &g_sd.s.wm );
    }
}

- (void)viewBoundsChanged: (NSNotification*)aNotification
{
}

- (void)viewFrameChanged: (NSNotification*)aNotification
{
    /*NSRect r = [ self frame ];
    g_macos_sundog_screen_xsize = r.size.width * self.window.backingScaleFactor;
    g_macos_sundog_screen_ysize = r.size.height * self.window.backingScaleFactor;*/
}

- (int)viewInit
{
    g_gl_context = [ self openGLContext ];
    g_gl_context_obj = [ g_gl_context CGLContextObj ];
    
    NSRect r = [ self frame ];
    g_macos_sundog_screen_xsize = r.size.width * self.window.backingScaleFactor;
    g_macos_sundog_screen_ysize = r.size.height * self.window.backingScaleFactor;
    g_macos_sundog_screen_ppi = DEFAULT_PPI * self.window.backingScaleFactor;
    
    NSNotificationCenter* dnc;
    dnc = [ NSNotificationCenter defaultCenter ];
    [ dnc addObserver:self selector:@selector( viewFrameChanged:) name:NSViewFrameDidChangeNotification object:self ];
    [ dnc addObserver:self selector:@selector( viewBoundsChanged:) name:NSViewBoundsDidChangeNotification object:self ];

    g_main_window_delegate = [ [ MainWindowDelegate alloc ] init ];
    [ [ self window ] setDelegate:g_main_window_delegate ];
    
    {
	//Clear OpenGL context:
	
	[ g_gl_context makeCurrentContext ];
	
	int err = CGLLockContext( (CGLContextObj)g_gl_context_obj );
	if( err != 0 )
	{
	    printf( "[viewInit] CGLLockContext() error %d\n", err );
	}
	
	glClear( GL_COLOR_BUFFER_BIT );
	
	err = CGLUnlockContext( (CGLContextObj)g_gl_context_obj );
	if( err != 0 )
	{
	    printf( "[viewInit] CGLUnlockContext() error %d\n", err );
	}
    }
        
    return 0;
}

- (void)drawRect:(NSRect)rect
{
    if( g_gl_context_obj == 0 ) return;
    if( g_gl_context == 0 ) return;
}

@end

//
// App Delegate
//

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    if(  [ [ NSProcessInfo processInfo ] respondsToSelector:@selector(operatingSystemVersion) ] )
        g_os_version = [ [ NSProcessInfo processInfo ] operatingSystemVersion ];

    subview = [ [ [ MyView alloc ] initWithFrame: [ view frame ] ] autorelease ];
    if( g_os_version.majorVersion <= 10 && g_os_version.minorVersion <= 14 )
        subview.wantsBestResolutionOpenGLSurface = YES;
    [ subview setAutoresizingMask: 1 | 2 | 4 | 8 | 16 | 32 ];
    [ view setAutoresizesSubviews: YES ];
    [ view addSubview: subview ];

    macos_sundog_engine* sd = &g_sd;

    sundog_init( subview, sd );
}

/*- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename
{
    macos_sundog_engine* sd = &g_sd;
    sundog_state_set_( sd, 0, sundog_state_new( [ filename UTF8String ], SUNDOG_STATE_ORIGINAL ) );
    return YES;
}*/

- (void)applicationWillTerminate:(NSNotification *)notification
{
    macos_sundog_engine* sd = &g_sd;
    sundog_deinit( sd );
}

- (void)dealloc
{
    [subview release];
    [super dealloc];
}

@end

//
// macOS app main()
//

int main( int argc, char* argv[] )
{
    return NSApplicationMain( argc, (const char **) argv );
}
