/*
    sundog_bridge1.hpp - SunDog1<->Android bridge
    This file is part of the SunDog engine.
    Copyright (C) 2012 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#ifndef NOMAIN

#define DISP_SURFACE_ONLY		( 1 << 0 )
static int engine_init_display( android_sundog_engine* sd, uint32_t flags );
static void engine_term_display( android_sundog_engine* sd, uint32_t flags );

static int android_sundog_option_exists( android_sundog_engine* sd, const char* name )
{
    int rv = 0;
    char* ts = (char*)malloc( strlen( g_android_files_ext_path ) + strlen( name ) + 64 );
    if( ts )
    {
	ts[ 0 ] = 0;
	strcat( ts, g_android_files_ext_path );
	strcat( ts, name );
	FILE* f = fopen( ts, "rb" );
	if( f == 0 )
	{
	    strcat( ts, ".txt" );
	    f = fopen( ts, "rb" );
	}
	if( f )
	{
	    rv = fgetc( f );
	    if( rv < 0 ) rv = 1;
	    fclose( f );
	}
	free( ts );
    }
    return rv;
}

static void* sundog_thread( void* arg )
{
    android_sundog_engine* sd = (android_sundog_engine*)arg;

    LOGI( "sundog_thread() ..." );

    if( engine_init_display( sd, 0 ) )
    {
	ANativeActivity_finish( sd->app->activity );
	goto sundog_thread_end;
    }
    ANativeActivity_setWindowFlags( sd->app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0 );

    LOGI( "sundog_main() ..." );

    LOGI( "pause 100ms ..." );
    stime_sleep( 100 ); //Small pause due to strange NativeActivity behaviour (trying to close window just after the first start):
                        //in this case the sundog_main() will not be started, and we just close the thread and open it again;
    if( sd->exit_request == 0 )
    {
	sundog_main( &sd->s, true );
    }

    LOGI( "sundog_main() finished" );

    engine_term_display( sd, 0 );

    if( sd->camera_connections )
    {
	for( int i = 0; i < 32; i++ )
	{
	    if( sd->camera_connections & ( 1 << i ) )
		android_sundog_close_camera( &sd->s, i );
	}
	sd->camera_connections = 0;
    }

    if( sd->exit_request == 0 )
    {
	//No exit requests from the OS. SunDog just closed.
	//So tell the OS to close this app:
	LOGI( "force close the activity..." );
	ANativeActivity_finish( sd->app->activity );
    }

sundog_thread_end:

    LOGI( "sundog_thread() finished" );

    sd->pth_finished = 1;
    android_sundog_release_jni( sd );
    pthread_exit( NULL );
}

static void android_sundog_wait_for_suspend( android_sundog_engine* sd, bool gfx )
{
    if( !sd->initialized ) return;
    if( sd->pth_finished ) return;

    int i = 0;
    int timeout = 1000;
    int step = 1;
    for( i = 0; i < timeout; i += step )
    {
        if( sd->pth_finished ) break;
        if( gfx )
        {
    	    if( sd->eventloop_stop_answer && sd->surface == EGL_NO_SURFACE ) break;
        }
        else
        {
    	    if( sd->eventloop_stop_answer ) break;
    	}
        stime_sleep( step );
    }
    if( i >= timeout )
    {
        LOGI( "SunDog SUSPEND TIMEOUT" );
    }
}

static void android_sundog_wait_for_resume( android_sundog_engine* sd )
{
    if( !sd->initialized ) return;
    if( sd->pth_finished ) return;

    int i = 0;
    int timeout = 1000;
    int step = 1;
    for( i = 0; i < timeout; i += step )
    {
        if( sd->pth_finished ) break;
        if( sd->eventloop_stop_answer == false ) break;
        if( sd->eventloop_stop_request ) break; //suspended by some other request; don't wait here
        if( sd->eventloop_gfx_stop_request ) break; //gfx is also suspended, so we don't need to wait for resuming here
        stime_sleep( step );
    }
    if( i >= timeout )
    {
        LOGI( "SunDog RESUME TIMEOUT" );
    }
}

static void android_sundog_pause( android_sundog_engine* sd, bool pause )
{
    if( !sd->initialized ) return;
    if( sd->pth_finished ) return;
    if( pause )
    {
	//Suspend SunDog eventloop:
	sd->eventloop_stop_request = true;
	android_sundog_wait_for_suspend( sd, false );
    }
    else
    {
	//Resume SunDog eventloop:
	sd->eventloop_stop_request = false;
	android_sundog_wait_for_resume( sd );
    }
}

static void android_sundog_init( android_sundog_engine* sd )
{
    int err;

    LOGI( "android_sundog_init() ..." );

    while( 1 )
    {
	if( !sd->initialized ) break;
	if( sd->pth_finished ) break;
	//Resume:
	sd->eventloop_gfx_stop_request = false;
	android_sundog_wait_for_resume( sd );
	return;
	break;
    }

    size_t zsize = (size_t)( (char*)&sd->app - (char*)sd );
    memset( sd, 0, zsize );
    sd->s.device_specific = sd;
    sd->sys_ui_visible = 1;
    sem_init( &sd->eventloop_sem, 0, 0 );

    sd->s.screen_ppi = AConfiguration_getDensity( sd->app->config );
    sd->s.screen_orientation = AConfiguration_getOrientation( sd->app->config );

    err = pthread_create( &sd->pth, 0, &sundog_thread, sd );
    if( err == 0 )
    {
	//The pthread_detach() function marks the thread identified by thread as
	//detached. When a detached thread terminates, its resources are 
	//automatically released back to the system.
	err = pthread_detach( sd->pth );
	if( err != 0 )
	{
	    LOGW( "android_sundog_init(): pthread_detach error %d", err );
	    return;
	}
    }
    else
    {
	LOGW( "android_sundog_init(): pthread_create error %d", err );
	return;
    }

    LOGI( "android_sundog_init(): done" );

    sd->initialized = 1;
}

static void android_sundog_deinit( android_sundog_engine* sd, bool try_to_stay_in_background )
{
    if( sd->initialized == 0 ) return;
    if( sd->pth_finished ) return;

    LOGI( "android_sundog_deinit() ..." );

    while( try_to_stay_in_background )
    {
	if( !sd->s.wm.initialized ) break;
	if( sd->s.wm.exit_request ) break;
	if( sd->display == EGL_NO_DISPLAY ) break;
        if( sd->context == EGL_NO_CONTEXT ) break;
	if( sd->surface == EGL_NO_SURFACE ) break;
	//Suspend:
	LOGI( "android_sundog_deinit(): stay in background..." );
	sd->eventloop_gfx_stop_request = true;
	android_sundog_wait_for_suspend( sd, true );
	return;
	break;
    }

    //APP_CMD_DESTROY -> Stop the thread:
    //Here the system can completely kill the app in a short time
    //(~10ms on Samsung with Android 11 ?)
    //So don't save data here!
    sd->exit_request = 1;
    win_destroy_request( &sd->s.wm );
    sd->eventloop_stop_request = false; //Resume SunDog eventloop if it was in the Pause state
    sd->eventloop_gfx_stop_request = false;
    sem_post( &sd->eventloop_sem );
    int timeout_counter = 1000 * 7; //ms
    int step = 1; //ms
    while( timeout_counter > 0 )
    {
	win_destroy_request( &sd->s.wm );
	struct timespec delay;
	delay.tv_sec = 0;
	delay.tv_nsec = 1000000 * step;
	if( sd->pth_finished ) break;
	nanosleep( &delay, NULL ); //Sleep for delay time
	timeout_counter -= step;
    }
    if( timeout_counter <= 0 )
    {
	LOGW( "android_sundog_deinit(): thread timeout" );
    }
    else
    {
	sem_destroy( &sd->eventloop_sem );
    }

    LOGI( "android_sundog_deinit(): done" );

    sd->initialized = 0;
}

void android_sundog_screen_redraw( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( eglSwapBuffers( sd->display, sd->surface ) == EGL_FALSE )
    {
	LOGI( "eglSwapBuffers error %x", eglGetError() );
	//EGL_BAD_SURFACE!
    }
}

void android_sundog_event_handler( window_manager* wm )
{
    HANDLE_THREAD_EVENTS;
    android_sundog_engine* sd = (android_sundog_engine*)wm->sd->device_specific;

    if( sd->eventloop_stop_request || sd->eventloop_gfx_stop_request )
    {
#ifdef ALLOW_WORK_IN_BG
#ifndef SUNDOG_MODULE
        uint32_t ss_idle_start = wm->sd->ss_idle_frame_counter;
        uint32_t ss_sample_rate = wm->sd->ss_sample_rate;
#endif
#endif
	bool term_disp = false;
	stime_ms_t term_disp_t = 0;
	win_suspend( true, wm );
	while( sd->eventloop_stop_request || sd->eventloop_gfx_stop_request )
	{
    	    if( sd->eventloop_gfx_stop_request )
    	    {
    		if( !term_disp )
    		{
	    	    engine_term_display( sd, DISP_SURFACE_ONLY );
		    term_disp = true;
    		    term_disp_t = stime_ms();
		}
		else
		{
		    bool exit_app = false;
#ifdef ALLOW_WORK_IN_BG
#ifndef SUNDOG_MODULE
		    if( ss_sample_rate && wm->sd->ss_idle_frame_counter - ss_idle_start > ss_sample_rate * 60 * 4 )
        	    {
                	LOGI( "Sound idle timeout" );
                	exit_app = true;
        	    }
#endif
#else
		    stime_ms_t tt = stime_ms() - term_disp_t; //amount of time spent in a suspended state
		    if( tt > 1000 * 4 ) exit_app = true; //work in the background is not allowed for this app; so just close it...
#endif
		    if( exit_app )
		    {
			win_exit_request( wm );
			sd->eventloop_stop_request = false;
			sd->eventloop_gfx_stop_request = false;
			break;
		    }
		}
    	    }
	    sd->eventloop_stop_answer = true;
	    if( 0 )
	    {
    		stime_sleep( 100 );
    	    }
    	    else
    	    {
    		int timeout = 100; //ms
    		struct timespec t;
                clock_gettime( CLOCK_REALTIME, &t );
	        t.tv_sec += timeout / 1000;
    	        t.tv_nsec += ( timeout % 1000 ) * 1000000;
	        if( t.tv_nsec >= 1000000000 )
    	        {
        	    t.tv_sec++;
            	    t.tv_nsec -= 1000000000;
        	}
        	sem_timedwait( &sd->eventloop_sem, &t );
    	    }
	}
	if( term_disp )
	{
    	    engine_init_display( sd, DISP_SURFACE_ONLY );
	}
	sd->eventloop_stop_answer = false;
	win_suspend( false, wm );
    }

    if( sd->intent_file_name )
    {
	sundog_state* state = sundog_state_new( sd->intent_file_name, SUNDOG_STATE_TEMP );
	free( sd->intent_file_name ); sd->intent_file_name = NULL;
	if( state )
        {
	    sundog_state_set( wm->sd, 0, state );
    	    send_event( wm->root_win, EVT_LOADSTATE, wm );
        }
    }
}

bool android_sundog_get_gl_buffer_preserved( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return sd->gl_buffer_preserved;
}
EGLDisplay android_sundog_get_egl_display( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return sd->display;
}
EGLSurface android_sundog_get_egl_surface( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return sd->surface;
}

//
// Main
//

static const char* get_egl_error_str( EGLint err )
{
    const char* rv = "?";
    switch( err )
    {
	case EGL_SUCCESS: rv = "EGL_SUCCESS"; break;
	case EGL_NOT_INITIALIZED: rv = "EGL_NOT_INITIALIZED"; break;
	case EGL_BAD_ACCESS: rv = "EGL_BAD_ACCESS"; break;
	case EGL_BAD_ALLOC: rv = "EGL_BAD_ALLOC"; break;
	case EGL_BAD_ATTRIBUTE: rv = "EGL_BAD_ATTRIBUTE"; break;
	case EGL_BAD_CONTEXT: rv = "EGL_BAD_CONTEXT"; break;
	case EGL_BAD_CONFIG: rv = "EGL_BAD_CONFIG"; break;
	case EGL_BAD_CURRENT_SURFACE: rv = "EGL_BAD_CURRENT_SURFACE"; break;
	case EGL_BAD_DISPLAY: rv = "EGL_BAD_DISPLAY"; break;
	case EGL_BAD_SURFACE: rv = "EGL_BAD_SURFACE"; break;
	case EGL_BAD_MATCH: rv = "EGL_BAD_MATCH"; break;
	case EGL_BAD_PARAMETER: rv = "EGL_BAD_PARAMETER"; break;
	case EGL_BAD_NATIVE_PIXMAP: rv = "EGL_BAD_NATIVE_PIXMAP"; break;
	case EGL_BAD_NATIVE_WINDOW: rv = "EGL_BAD_NATIVE_WINDOW"; break;
	case EGL_CONTEXT_LOST: rv = "EGL_CONTEXT_LOST"; break;
    }
    return rv;
}

// Initialize an EGL context for the current display.
static int engine_init_display( android_sundog_engine* sd, uint32_t flags )
{
    if( sd->app->window == NULL )
    {
	LOGW( "NULL window" );
	return -1;
    }

    if( flags & DISP_SURFACE_ONLY )
    {
	while( 1 )
	{
	    LOGI( "Creating a new OpenGL ES surface..." );
	    EGLSurface s = eglCreateWindowSurface( sd->display, sd->cur_config, sd->app->window, NULL );
	    if( s == EGL_NO_SURFACE )
	    {
    		LOGW( "eglCreateWindowSurface() error %d", eglGetError() );
		break;
	    }
	    if( eglMakeCurrent( sd->display, s, s, sd->context ) != EGL_TRUE ) 
	    {
    		LOGW( "eglMakeCurrent error %d", eglGetError() );
    		break;
	    }
	    if( sd->gl_buffer_preserved )
	    {
    		eglSurfaceAttrib( sd->display, s, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED );
		EGLint sb = EGL_BUFFER_DESTROYED;
    		eglQuerySurface( sd->display, s, EGL_SWAP_BEHAVIOR, &sb );
    		if( sb != EGL_BUFFER_PRESERVED )
    		{
    		    //Something went wrong and we can no longer set EGL_SWAP_BEHAVIOR to EGL_BUFFER_PRESERVED :(
		    sd->s.wm.screen_buffer_preserved = false;
    	    	    sd->gl_buffer_preserved = false;
    		    LOGI( "EGL_BUFFER_DESTROYED" );
		}
	    }
	    sd->surface = s;
	    sd->s.screen_changed_w++;

	    return 0;
	    break;
	}
	engine_term_display( sd, 0 );
	window_manager* wm = &sd->s.wm;
	if( wm->initialized ) win_restart_request( wm );
    }

    //Main OpenGL ES init:
    sd->display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
    if( sd->display == EGL_NO_DISPLAY )
    {
	LOGW( "eglGetDisplay() error %d", eglGetError() );
	return -1;
    }
    if( eglInitialize( sd->display, NULL, NULL ) != EGL_TRUE )
    {
	LOGW( "eglInitialize() error %d", eglGetError() );
    	return -1;
    }

    //Get a list of EGL frame buffer configurations that match specified attributes:
    const EGLint attribs1[] =
    { //desired config 1:
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
	EGL_DEPTH_SIZE, 16, //minimum value
        EGL_NONE
    };
    const EGLint attribs2[] = 
    { //desired config 2:
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    EGLint numConfigs = 0;
    EGLint num = 0;
    EGLConfig config[ 2 ];
    bool buf_preserved = true;
#ifdef GLNORETAIN
    buf_preserved = false;
#endif
    if( android_sundog_option_exists( sd, "option_glnoretain" ) )
    {
        LOGI( "option_glnoretain found. OpenGL Buffer Preserved = NO" );
        buf_preserved = false;
    }
    if( buf_preserved )
    {
	if( eglChooseConfig( sd->display, attribs1, &config[ numConfigs ], 1, &num ) != EGL_TRUE )
	{
    	    LOGW( "eglChooseConfig() error %d", eglGetError() );
	}
	LOGI( "EGL Configs with EGL_SWAP_BEHAVIOR_PRESERVED_BIT: %d", num );
	numConfigs += num;
    }
    num = 0;
    if( eglChooseConfig( sd->display, attribs2, &config[ numConfigs ], 1, &num ) != EGL_TRUE )
    {
	LOGW( "eglChooseConfig() error %d", eglGetError() );
    }
    LOGI( "EGL Configs without EGL_SWAP_BEHAVIOR_PRESERVED_BIT: %d", num );
    numConfigs += num;
    if( numConfigs == 0 )
    {
	LOGW( "ERROR: No matching configs (numConfigs == 0)" );
	return -1;
    }

    //Print the configs:
    /*for( EGLint cnum = 0; cnum < numConfigs; cnum++ )
    {
	EGLint cv, r, g, b, d;
	eglGetConfigAttrib( sd->display, config[ cnum ], EGL_RED_SIZE, &r );
	eglGetConfigAttrib( sd->display, config[ cnum ], EGL_GREEN_SIZE, &g );
	eglGetConfigAttrib( sd->display, config[ cnum ], EGL_BLUE_SIZE, &b );
	eglGetConfigAttrib( sd->display, config[ cnum ], EGL_DEPTH_SIZE, &d );
	eglGetConfigAttrib( sd->display, config[ cnum ], EGL_SURFACE_TYPE, &cv );
	char stype[ 256 ];
	stype[ 0 ] = 0;
	if( cv & EGL_PBUFFER_BIT ) strcat( stype, "PBUF " );
	if( cv & EGL_PIXMAP_BIT ) strcat( stype, "PXM " );
	if( cv & EGL_WINDOW_BIT ) strcat( stype, "WIN " );
	if( cv & EGL_VG_COLORSPACE_LINEAR_BIT ) strcat( stype, "CL " );
	if( cv & EGL_VG_ALPHA_FORMAT_PRE_BIT ) strcat( stype, "AFP " );
	if( cv & EGL_MULTISAMPLE_RESOLVE_BOX_BIT ) strcat( stype, "MRB " );
	if( cv & EGL_SWAP_BEHAVIOR_PRESERVED_BIT ) strcat( stype, "PRESERVED " );
	LOGI( "EGL Config %d. RGBT: %d %d %d %d %x %s", cnum, r, g, b, d, cv, stype );
    }*/

    //Choose the config:
    EGLint w = 0, h = 0, format;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool configErr = 1;
    for( EGLint cnum = 0; cnum < numConfigs; cnum++ )
    {
	LOGI( "EGL Config %d", cnum );

	if( surface != EGL_NO_SURFACE ) eglDestroySurface( sd->display, surface );
	if( context != EGL_NO_CONTEXT ) eglDestroyContext( sd->display, context );
	surface = EGL_NO_SURFACE;
	context = EGL_NO_CONTEXT;

	/* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
         * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
         * As soon as we picked a EGLConfig, we can safely reconfigure the
         * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
        if( eglGetConfigAttrib( sd->display, config[ cnum ], EGL_NATIVE_VISUAL_ID, &format ) != EGL_TRUE )
        {
    	    LOGW( "eglGetConfigAttrib() error %d", eglGetError() );
	    continue;
	}

        ANativeWindow_setBuffersGeometry( sd->app->window, 0, 0, format );

        surface = eglCreateWindowSurface( sd->display, config[ cnum ], sd->app->window, NULL );
	if( surface == EGL_NO_SURFACE )
        {
    	    LOGW( "eglCreateWindowSurface() error %d", eglGetError() );
	    continue;
	}

	EGLint gles2_attrib[] = 
	{
	    EGL_CONTEXT_CLIENT_VERSION, 2,
	    EGL_NONE
	};
	context = eglCreateContext( sd->display, config[ cnum ], NULL, gles2_attrib );
	if( surface == EGL_NO_CONTEXT )
        {
	    LOGW( "eglCreateContext() error %d", eglGetError() );
	    continue;
	}

	if( eglMakeCurrent( sd->display, surface, surface, context ) != EGL_TRUE ) 
	{
    	    LOGW( "eglMakeCurrent error %d", eglGetError() );
    	    continue;
	}

	if( buf_preserved )
	{
    	    if( eglSurfaceAttrib( sd->display, surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED ) != EGL_TRUE )
    		LOGW( "eglSurfaceAttrib error %d", eglGetError() );
	    EGLint sb = EGL_BUFFER_DESTROYED;
    	    if( eglQuerySurface( sd->display, surface, EGL_SWAP_BEHAVIOR, &sb ) != EGL_TRUE )
    		LOGW( "eglQuerySurface error %d", eglGetError() );
    	    if( sb == EGL_BUFFER_PRESERVED )
	    {
		sd->gl_buffer_preserved = 1;
		LOGI( "EGL_BUFFER_PRESERVED" );
    	    }
	    else
    	    {
    	        sd->gl_buffer_preserved = 0;
    		LOGI( "EGL_BUFFER_DESTROYED" );
	    }
	}
	else
	{
    	    sd->gl_buffer_preserved = 0;
    	}

    	sd->cur_config = config[ cnum ];
	configErr = 0;
	break;
    }
    if( configErr )
    {
	LOGW( "ERROR: No matching configs." );
	return -1;
    }

    eglQuerySurface( sd->display, surface, EGL_WIDTH, &w );
    if( eglQuerySurface( sd->display, surface, EGL_HEIGHT, &h ) != EGL_TRUE )
	LOGW( "eglQuerySurface error %s", get_egl_error_str( eglGetError() ) );
    if( w <= 1 ) LOGW( "BAD EGL_WIDTH %d", w );
    if( h <= 1 ) LOGW( "BAD EGL_HEIGHT %d", h );

    sd->context = context;
    sd->surface = surface;

    sd->s.screen_xsize = w;
    sd->s.screen_ysize = h;
    android_sundog_set_safe_area( &sd->s );

    glDisable( GL_DEPTH_TEST );
#ifndef GLNORETAIN
    if( sd->gl_buffer_preserved == 0 )
    {
	glDisable( GL_DITHER );
    }
#endif

    return 0;
}

// Tear down the EGL context currently associated with the display.
static void engine_term_display( android_sundog_engine* sd, uint32_t flags )
{
    if( flags & DISP_SURFACE_ONLY )
    {
	if( sd->display != EGL_NO_DISPLAY )
	{
    	    eglMakeCurrent( sd->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ); //release the current context
    	    if( sd->surface != EGL_NO_SURFACE ) eglDestroySurface( sd->display, sd->surface );
	    sd->surface = EGL_NO_SURFACE;
    	}
    	return;
    }
    if( sd->display != EGL_NO_DISPLAY )
    {
        eglMakeCurrent( sd->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ); //release the current context
        if( sd->context != EGL_NO_CONTEXT ) eglDestroyContext( sd->display, sd->context );
        if( sd->surface != EGL_NO_SURFACE ) eglDestroySurface( sd->display, sd->surface );
        eglTerminate( sd->display );
    }
    sd->context = EGL_NO_CONTEXT;
    sd->display = EGL_NO_DISPLAY;
    sd->surface = EGL_NO_SURFACE;
}

static uint32_t convert_key( int32_t vk )
{
    uint32_t rv = 0;
    switch( vk )
    {
	case AKEYCODE_0: case 0x90: rv = '0'; break;
	case AKEYCODE_1: case 0x91: rv = '1'; break;
	case AKEYCODE_2: case 0x92: rv = '2'; break;
        case AKEYCODE_3: case 0x93: rv = '3'; break;
	case AKEYCODE_4: case 0x94: rv = '4'; break;
	case AKEYCODE_5: case 0x95: rv = '5'; break;
	case AKEYCODE_6: case 0x96: rv = '6'; break;
	case AKEYCODE_7: case 0x97: rv = '7'; break;
        case AKEYCODE_8: case 0x98: rv = '8'; break;
	case AKEYCODE_9: case 0x99: rv = '9'; break;

	case AKEYCODE_A: rv = 'a'; break;
	case AKEYCODE_B: rv = 'b'; break;
	case AKEYCODE_C: rv = 'c'; break;
	case AKEYCODE_D: rv = 'd'; break;
        case AKEYCODE_E: rv = 'e'; break;
	case AKEYCODE_F: rv = 'f'; break;
	case AKEYCODE_G: rv = 'g'; break;
        case AKEYCODE_H: rv = 'h'; break;
	case AKEYCODE_I: rv = 'i'; break;
        case AKEYCODE_J: rv = 'j'; break;
	case AKEYCODE_K: rv = 'k'; break;
	case AKEYCODE_L: rv = 'l'; break;
        case AKEYCODE_M: rv = 'm'; break;
	case AKEYCODE_N: rv = 'n'; break;
        case AKEYCODE_O: rv = 'o'; break;
	case AKEYCODE_P: rv = 'p'; break;
	case AKEYCODE_Q: rv = 'q'; break;
        case AKEYCODE_R: rv = 'r'; break;
	case AKEYCODE_S: rv = 's'; break;
	case AKEYCODE_T: rv = 't'; break;
	case AKEYCODE_U: rv = 'u'; break;
	case AKEYCODE_V: rv = 'v'; break;
	case AKEYCODE_W: rv = 'w'; break;
        case AKEYCODE_X: rv = 'x'; break;
        case AKEYCODE_Y: rv = 'y'; break;
        case AKEYCODE_Z: rv = 'z'; break;

        case AKEYCODE_STAR: rv = '*'; break;
        case AKEYCODE_POUND: rv = '#'; break;
        case AKEYCODE_COMMA: rv = ','; break;
        case AKEYCODE_PERIOD: rv = '.'; break;
        case AKEYCODE_SPACE: rv = ' '; break;
        case AKEYCODE_GRAVE: rv = '`'; break;
	case AKEYCODE_MINUS: rv = '-'; break;
        case AKEYCODE_EQUALS: rv = '='; break;
        case AKEYCODE_LEFT_BRACKET: rv = '['; break;
	case AKEYCODE_RIGHT_BRACKET: rv = ']'; break;
	case AKEYCODE_BACKSLASH: rv = '\\'; break;
	case AKEYCODE_SEMICOLON: rv = ';'; break;
	case AKEYCODE_APOSTROPHE: rv = '\''; break;
	case AKEYCODE_SLASH: rv = '/'; break;
	case AKEYCODE_AT: rv = '@'; break;
	case AKEYCODE_PLUS: rv = '+'; break;

	/*case AKEYCODE_BUTTON_A: rv = 'a'; break;
	case AKEYCODE_BUTTON_B: rv = 'b'; break;
        case AKEYCODE_BUTTON_C: rv = 'c'; break;
        case AKEYCODE_BUTTON_X: rv = 'x'; break;
        case AKEYCODE_BUTTON_Y: rv = 'y'; break;
        case AKEYCODE_BUTTON_Z: rv = 'z'; break;*/

        case 131: rv = KEY_F1; break;
        case 132: rv = KEY_F2; break;
        case 133: rv = KEY_F3; break;
        case 134: rv = KEY_F4; break;
        case 135: rv = KEY_F5; break;
        case 136: rv = KEY_F6; break;
        case 137: rv = KEY_F7; break;
        case 138: rv = KEY_F8; break;
        case 139: rv = KEY_F9; break;
        case 140: rv = KEY_F10; break;
        case 141: rv = KEY_F11; break;
        case 142: rv = KEY_F12; break;

        case AKEYCODE_ESCAPE: rv = KEY_ESCAPE; break;
        case AKEYCODE_TAB: rv = KEY_TAB; break;
	case AKEYCODE_ENTER: rv = KEY_ENTER; break;
        case AKEYCODE_DEL: rv = KEY_BACKSPACE; break;
        case AKEYCODE_FORWARD_DEL: rv = KEY_DELETE; break;
        case AKEYCODE_INSERT: rv = KEY_INSERT; break;
        case AKEYCODE_MOVE_HOME: rv = KEY_HOME; break;
        case AKEYCODE_MOVE_END: rv = KEY_END; break;
	case AKEYCODE_PAGE_UP: rv = KEY_PAGEUP; break;
	case AKEYCODE_PAGE_DOWN: rv = KEY_PAGEDOWN; break;

	case AKEYCODE_SOFT_LEFT: rv = KEY_LEFT; break;
	case AKEYCODE_SOFT_RIGHT: rv = KEY_RIGHT; break;
	case AKEYCODE_DPAD_UP: rv = KEY_UP; break;
	case AKEYCODE_DPAD_DOWN: rv = KEY_DOWN; break;
	case AKEYCODE_DPAD_LEFT: rv = KEY_LEFT; break;
	case AKEYCODE_DPAD_RIGHT: rv = KEY_RIGHT; break;
	case AKEYCODE_DPAD_CENTER: rv = KEY_SPACE; break;

	case AKEYCODE_ALT_LEFT: rv = KEY_ALT; break;
	case AKEYCODE_ALT_RIGHT: rv = KEY_ALT; break;
	case AKEYCODE_SHIFT_LEFT: rv = KEY_SHIFT; break;
        case AKEYCODE_SHIFT_RIGHT: rv = KEY_SHIFT; break;

	case AKEYCODE_BACK: rv = KEY_ESCAPE; break;
	case AKEYCODE_MENU: rv = KEY_MENU; break;

        default: rv = KEY_UNKNOWN + vk; break;
    }
    return rv;
}

static uint get_modifiers( int32_t ms )
{
    uint evt_flags = 0;
    if( ms & AMETA_ALT_ON ) evt_flags |= EVT_FLAG_ALT;
    if( ms & AMETA_SHIFT_ON ) evt_flags |= EVT_FLAG_SHIFT;
    if( ms & AMETA_CTRL_ON ) evt_flags |= EVT_FLAG_CTRL;
    return evt_flags;
}

static void send_key_event( bool down, uint16_t key, uint32_t scancode, uint16_t flags, window_manager* wm )
{
    if( !wm ) return;
    if( !wm->initialized ) return;
    int type;
    if( down )
        type = EVT_BUTTONDOWN;
    else
        type = EVT_BUTTONUP;
    send_event( 0, type, flags, 0, 0, key, scancode, 1024, wm );
}

// Process the next input event.
static int32_t engine_handle_input( struct android_app* app, AInputEvent* event ) 
{
    int32_t rv = 0;
    android_sundog_engine* sd = (android_sundog_engine*)app->userData;
    if( sd->initialized == 0 ) return 0;
    window_manager* wm = &sd->s.wm;
    int x, y;
    int pressure;
    int32_t id;
    int32_t evt_type = AInputEvent_getType( event );
    if( evt_type == AINPUT_EVENT_TYPE_KEY )
    {
	int32_t sc = AKeyEvent_getScanCode( event );
	int32_t vk = AKeyEvent_getKeyCode( event );
	uint32_t sundog_key = convert_key( vk );
	uint evt_flags = get_modifiers( AKeyEvent_getMetaState( event ) );
	int32_t evt = AKeyEvent_getAction( event );
	switch( evt )
	{
	    case AKEY_EVENT_ACTION_DOWN:
		//slog( "KEY DOWN sc:%d vk:%d ms:%x\n", sc, vk, ms );
		send_key_event( 1, sundog_key, sc, evt_flags, wm );
		break;
	    case AKEY_EVENT_ACTION_UP:
		//slog( "KEY UP sc:%d vk:%d ms:%x\n", sc, vk, ms );
		send_key_event( 0, sundog_key, sc, evt_flags, wm );
		break;
	}
	switch( vk )
	{
	    case AKEYCODE_BACK:
	    case AKEYCODE_MENU:
		rv = 1; //prevent default handler
		break;
	}
    }
    if( evt_type == AINPUT_EVENT_TYPE_MOTION )
    {
	size_t pcount = AMotionEvent_getPointerCount( event );
	int32_t evt = AMotionEvent_getAction( event );
	uint evt_flags = get_modifiers( AMotionEvent_getMetaState( event ) );
	switch( evt & AMOTION_EVENT_ACTION_MASK )
	{
	    case AMOTION_EVENT_ACTION_DOWN:
		//First touch (primary pointer):
		x = AMotionEvent_getX( event, 0 );
		y = AMotionEvent_getY( event, 0 );
		pressure = 1024; //AMotionEvent_getPressure( event, 0 ) * 1024;
		id = AMotionEvent_getPointerId( event, 0 );
		collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_MOVE:
		{
		    for( size_t i = 0; i < pcount; i++ )
    		    {
			id = AMotionEvent_getPointerId( event, i );
			x = AMotionEvent_getX( event, i );
			y = AMotionEvent_getY( event, i );
			pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
			uint evt_flags2 = 0;
			if( i < pcount - 1 ) evt_flags2 |= EVT_FLAG_DONTDRAW;
			collect_touch_events( 1, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags | evt_flags2, x, y, pressure, id, wm );
    		    }
		}
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_SCROLL:
		for( size_t i = 0; i < pcount; i++ )
    		{
		    id = AMotionEvent_getPointerId( event, i );
		    x = AMotionEvent_getX( event, i );
		    y = AMotionEvent_getY( event, i );
		    float v = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_VSCROLL, i );
		    float h = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_HSCROLL, i );
		    if( v )
		    {
			//vscroll; pressure: -1024 - down; 1024 - up;
			collect_touch_events( 3, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, v * 1024, id, wm );
		    }
		    if( h )
		    {
			//hscroll; pressure: -1024 - left; 1024 - right;
			collect_touch_events( 4, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, h * 1024, id, wm );
		    }
    		}
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_UP:
	    case AMOTION_EVENT_ACTION_CANCEL:
		{
		    for( size_t i = 0; i < pcount; i++ )
    		    {
			id = AMotionEvent_getPointerId( event, i );
			x = AMotionEvent_getX( event, i );
			y = AMotionEvent_getY( event, i );
			pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
			collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
    		    }
		}
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_POINTER_DOWN:
		{
		    int i = ( evt & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK ) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
		    id = AMotionEvent_getPointerId( event, i );
		    x = AMotionEvent_getX( event, i );
		    y = AMotionEvent_getY( event, i );
		    pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
		    collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
		}
		rv = 1;
		break;
	    case AMOTION_EVENT_ACTION_POINTER_UP:
		{
		    int i = ( evt & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK ) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
		    id = AMotionEvent_getPointerId( event, i );
		    x = AMotionEvent_getX( event, i );
		    y = AMotionEvent_getY( event, i );
		    pressure = 1024; //AMotionEvent_getPressure( event, i ) * 1024;
		    collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY | TOUCH_FLAG_LIMIT, evt_flags, x, y, pressure, id, wm );
		}
		rv = 1;
		break;
	}
	send_touch_events( wm );
    }
    return rv;
}

// Process the next main command.
static void engine_handle_cmd( struct android_app* app, int32_t cmd ) 
{
    android_sundog_engine* sd = (android_sundog_engine*)app->userData;
    switch( cmd )
    {
        case APP_CMD_DESTROY:
    	    LOGI( "APP_CMD_DESTROY" );
    	    break;

        case APP_CMD_START:
    	    LOGI( "APP_CMD_START" );
    	    break;
        case APP_CMD_STOP: //can come when you press Home or Power (lock)
    	    LOGI( "APP_CMD_STOP" );
    	    break;

	case APP_CMD_PAUSE:
    	    LOGI( "APP_CMD_PAUSE" );
    	    android_sundog_pause( sd, true );
	    break;
	case APP_CMD_RESUME:
    	    LOGI( "APP_CMD_RESUME" );
    	    {
    		char* new_intent = java_call2_s_i( sd, "GetIntentFile", 1 );
    		if( new_intent )
    		{
    		    free( sd->intent_file_name ); sd->intent_file_name = NULL;
    		    sd->intent_file_name = new_intent;
    		}
	    }
    	    android_sundog_pause( sd, false );
	    break;

        case APP_CMD_SAVE_STATE:
    	    LOGI( "APP_CMD_SAVE_STATE" );
            break;

        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
    	    LOGI( "APP_CMD_INIT_WINDOW" );
            if( app->window ) android_sundog_init( sd );
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
    	    LOGI( "APP_CMD_TERM_WINDOW" );
            if( app->window ) android_sundog_deinit( sd, true );
            break;
        case APP_CMD_GAINED_FOCUS:
            // When our app gains focus, we start monitoring the accelerometer.
    	    LOGI( "APP_CMD_GAINED_FOCUS" );
            /*if( sd->accelerometerSensor != NULL) 
	    {
                // We'd like to get 60 events per second (in us).
                //ASensorEventQueue_enableSensor( engine->sensorEventQueue, engine->accelerometerSensor );
                //ASensorEventQueue_setEventRate( engine->sensorEventQueue, engine->accelerometerSensor, (1000L/60)*1000 );
            }*/
            {
        	int v = -1;
        	if( sd->sys_ui_visible != 1 )
        	{
        	    v = android_sundog_set_system_ui_visibility( sd, sd->sys_ui_visible );
    		}
    		else
    		{
    		    //System bars are already visible (by default or after the previous set_system_ui_visibility() call)
    		}
    		if( v != 0 )
    		{
    		    //Screen size may change without APP_CMD_CONFIG_CHANGED ? (2021 Android 11)
    		    //So let's check it one more time just in case:
		    sd->s.screen_changed_w++;
		}
	    }
            break;
        case APP_CMD_LOST_FOCUS:
            // When our app loses focus, we stop monitoring the accelerometer.
            // This is to avoid consuming battery while not being used.
    	    LOGI( "APP_CMD_LOST_FOCUS" );
            /*if( sd->accelerometerSensor != NULL ) 
	    {
                //ASensorEventQueue_disableSensor( engine->sensorEventQueue, engine->accelerometerSensor );
            }*/
            break;
	case APP_CMD_CONFIG_CHANGED:
    	    LOGI( "APP_CMD_CONFIG_CHANGED" );
	    sd->s.screen_changed_w++;
	    break;
    }
}

static void engine_loop( android_sundog_engine* sd )
{
    while( 1 ) 
    {
        int ident;
        int events;
        struct android_poll_source* source;

        while( ( ident = ALooper_pollAll( -1, NULL, &events, (void**)&source ) ) >= 0 ) 
        {
            // Process this event.
            if( source != NULL ) 
            {
                source->process( sd->app, source );
            }

            // Check if we are exiting.
            if( sd->app->destroyRequested != 0 ) 
            {
	        /*
	        (after APP_CMD_DESTROY)
	        From the Android docs:
	        do not count on this method being called as a place for saving data!
	        For example, if an activity is editing data in a content provider, those edits should be committed in either onPause() or onSaveInstanceState(Bundle), not here.
	        */
                android_sundog_deinit( sd, false );
                return;
            }
        }
    }
}

#endif //!NOMAIN
