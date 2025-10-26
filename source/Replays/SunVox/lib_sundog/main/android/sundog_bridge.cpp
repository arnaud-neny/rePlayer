/*
    sundog_bridge.cpp - SunDog<->Android bridge
    This file is part of the SunDog engine.
    Copyright (C) 2012 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "sundog_bridge.h"

#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <android/sensor.h>
#include <android/log.h>
#include <android/window.h>

#if SUNDOG_VER == 2
#include "gui/sd_os_window.h"
#include "gui/sd_os_window_desc_pairs.h"
#endif

struct android_sundog_engine
{
#ifndef SUNDOG_VER
    pthread_t 		pth;
    volatile bool 	pth_finished;
    volatile bool 	exit_request;
    volatile bool 	eventloop_stop_request;
    volatile bool 	eventloop_stop_answer;
    volatile bool 	eventloop_gfx_stop_request;
    sem_t		eventloop_sem; //instead of stime_sleep()
#endif
    volatile bool 	initialized;
    sundog_engine 	s;

    bool 		sustained_performance_mode;
    int 		sys_ui_visible;
    bool 		sys_ui_vchanged;
    volatile uint32_t 	camera_connections = 0; // 1 << con_index

    //android_sundog_init() will clear everything up to that point.

    struct android_app*	app;
    JNIEnv*		jni; //JNI of the main thread
    sthread_tid_t 	main_thread_id;

    char 		package_name[ 256 ];

    //Global references:
    jclass 		gref_na_class; //android/app/NativeActivity
    jclass 		gref_main_class; //PackageName/MyNativeActivity
    jclass 		alib_class; //nightradio/androidlib/AndroidLib
    jobject 		alib_obj;

    ASensorManager* 	sensorManager;
    const ASensor* 	accelerometerSensor;
    ASensorEventQueue* 	sensorEventQueue;

#ifndef SUNDOG_VER
    EGLDisplay 		display;
    volatile EGLSurface surface;
    EGLContext 		context; //state, textures and other GL objects
    EGLConfig 		cur_config;
    bool 		gl_buffer_preserved;
#endif

#if SUNDOG_VER == 2
    void** 		redraw_requests; //redraw_requests[i] = win_id = gui_win_native* (for sd_gui-based app)
    int 		redraw_requests_cnt;
    app_gui_evt* 	gui_events_output;
    sundog::os_window_desc_pairs*	wins;
    #define ACTIVITY_STATUS_ACTIVE		1
    #define ACTIVITY_STATUS_RESUMED		2
    #define ACTIVITY_STATUS_WINDOW_READY	4
    #define ACTIVITY_STATUS_FOCUSED		8
    #define ACTIVITY_STATUS_ALL			15
    uint32_t 		activity_status;
    int32_t 		button_state; //AMOTION_EVENT_BUTTON_*
    sundog::os_timer**	timers;
    int			timers_cnt;
    bool		timers_changed;
#endif

    char*		intent_file_name;

    sundog_state*	in_state; //Input state (document)
};

char* g_android_cache_int_path = NULL;
char* g_android_cache_ext_path = NULL;
char* g_android_files_int_path = NULL;
char* g_android_files_ext_path = NULL;
#define ANDROID_EXT_STORAGE_DEV_NAMELEN		1024
char* g_android_files_ext_paths[ ANDROID_NUM_EXT_STORAGE_DEVS ]; //[0] is always NULL
char* g_android_version = NULL;
char g_android_version_correct[ 16 ] = { 0 };
int g_android_version_nums[ 8 ] = { 0 };
char* g_android_lang = NULL;
char* g_android_requested_permissions = NULL;
int g_android_supported_permissions = 0xFFFFFFFF; //for android_sundog_check_for_permissions(): set of ANDROID_PERM_*

#ifndef NOMAIN

//
// JNI helpers
//

static pthread_key_t g_key_jni;
static pthread_once_t g_key_once = PTHREAD_ONCE_INIT;
static void make_global_pthread_keys()
{
    pthread_key_create( &g_key_jni, NULL );
    //The pthread_key_create() function shall create a thread-specific data key visible to all threads in the process.
    //The values bound to the key by pthread_setspecific() are maintained on a per-thread basis and persist for the life of the calling thread.
}

static JNIEnv* android_sundog_get_jni( android_sundog_engine* sd )
{
    JNIEnv* jni = (JNIEnv*)pthread_getspecific( g_key_jni );
    if( !jni )
    {
	//Attach current thread to the VM and obtain a JNI interface pointer:
	sd->app->activity->vm->AttachCurrentThread( &jni, NULL );
        pthread_setspecific( g_key_jni, jni );
    }
    return jni;
}

JNIEnv* android_sundog_get_jni( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return android_sundog_get_jni( sd );
}

static void android_sundog_release_jni( android_sundog_engine* sd )
{
    JNIEnv* jni = (JNIEnv*)pthread_getspecific( g_key_jni );
    if( jni )
    {
	sd->app->activity->vm->DetachCurrentThread();
	pthread_setspecific( g_key_jni, NULL );
    }
}

void android_sundog_release_jni( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    android_sundog_release_jni( sd );
}

static jclass java_find_class( android_sundog_engine* sd, JNIEnv* jni, const char* class_name )
{
    jclass class1 = sd->gref_na_class;
    if( class1 == NULL )
    {
        LOGW( "NativeActivity class not found!" );
    }
    else
    {
        jmethodID getClassLoader = jni->GetMethodID( class1, "getClassLoader", "()Ljava/lang/ClassLoader;" );
        jobject class_loader_obj = jni->CallObjectMethod( android_get_activity_obj( sd->app ), getClassLoader );
        jclass class_loader = jni->FindClass( "java/lang/ClassLoader" );
        jmethodID loadClass = jni->GetMethodID( class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;" );
        jstring class_name_str = jni->NewStringUTF( class_name );
        jclass class2 = (jclass)jni->CallObjectMethod( class_loader_obj, loadClass, class_name_str );
        if( class2 == NULL )
        {
    	    LOGW( "%s class not found!", class_name );
        }
        return class2;
    }
    return 0;
}

static int java_call_i_s( android_sundog_engine* sd, const char* method_name, const char* str_par )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jclass c = sd->gref_main_class;
    if( c )
    {
        jmethodID m = jni->GetMethodID( c, method_name, "(Ljava/lang/String;)I" );
	if( m )
    	{
	    rv = jni->CallIntMethod( android_get_activity_obj( sd->app ), m, jni->NewStringUTF( str_par ) );
    	}
    }

    return rv;
}

static int java_call_i_v( android_sundog_engine* sd, const char* method_name )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jclass c = sd->gref_main_class;
    if( c )
    {
        jmethodID m = jni->GetMethodID( c, method_name, "()I" );
	if( m )
    	{
	    rv = jni->CallIntMethod( android_get_activity_obj( sd->app ), m );
    	}
    }

    return rv;
}

static int java_call_i_i( android_sundog_engine* sd, const char* method_name, int int_par )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jclass c = sd->gref_main_class;
    if( c )
    {
        jmethodID m = jni->GetMethodID( c, method_name, "(I)I" );
	if( m )
    	{
	    rv = jni->CallIntMethod( android_get_activity_obj( sd->app ), m, int_par );
    	}
    }

    return rv;
}

static int java_call2_i_si( android_sundog_engine* sd, const char* method_name, const char* str_par, int int_par )
{
    int rv = -1;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jmethodID m = jni->GetMethodID( sd->alib_class, method_name, "(Ljava/lang/String;I)I" );
    if( m )
    {
	rv = jni->CallIntMethod( sd->alib_obj, m, jni->NewStringUTF( str_par ), int_par );
    }

    return rv;
}

static int java_call2_i_i( android_sundog_engine* sd, const char* method_name, int pars_count, int int_par1, int int_par2, int int_par3, int int_par4 )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jmethodID m;
    switch( pars_count )
    {
	case 0: m = jni->GetMethodID( sd->alib_class, method_name, "()I" ); if( m ) rv = jni->CallIntMethod( sd->alib_obj, m ); break;
	case 1: m = jni->GetMethodID( sd->alib_class, method_name, "(I)I" ); if( m ) rv = jni->CallIntMethod( sd->alib_obj, m, int_par1 ); break;
	case 2: m = jni->GetMethodID( sd->alib_class, method_name, "(II)I" ); if( m ) rv = jni->CallIntMethod( sd->alib_obj, m, int_par1, int_par2 ); break;
	case 3: m = jni->GetMethodID( sd->alib_class, method_name, "(III)I" ); if( m ) rv = jni->CallIntMethod( sd->alib_obj, m, int_par1, int_par2, int_par3 ); break;
	case 4: m = jni->GetMethodID( sd->alib_class, method_name, "(IIII)I" ); if( m ) rv = jni->CallIntMethod( sd->alib_obj, m, int_par1, int_par2, int_par3, int_par4 ); break;
	default: break;
    }

    return rv;
}

static int java_call2_i_ii64( android_sundog_engine* sd, const char* method_name, int int_par1, int64_t int_par2 )
{
    int rv = 0;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jmethodID m = jni->GetMethodID( sd->alib_class, method_name, "(IJ)I" );
    if( m )
    {
	rv = jni->CallIntMethod( sd->alib_obj, m, int_par1, int_par2 );
    }

    return rv;
}

static int64_t java_call2_i64_i( android_sundog_engine* sd, const char* method_name, int pars_count, int int_par1 )
{
    int64_t rv = 0;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jmethodID m;
    switch( pars_count )
    {
	case 0: m = jni->GetMethodID( sd->alib_class, method_name, "()J" ); if( m ) rv = jni->CallLongMethod( sd->alib_obj, m ); break;
	case 1: m = jni->GetMethodID( sd->alib_class, method_name, "(I)J" ); if( m ) rv = jni->CallLongMethod( sd->alib_obj, m, int_par1 ); break;
	default: break;
    }

    return rv;
}

static int* java_call2_iarr_i( android_sundog_engine* sd, const char* method_name, size_t* len, int par )
{
    int* rv = NULL;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jmethodID m = jni->GetMethodID( sd->alib_class, method_name, "(I)[I" );
    if( m )
    {
	jintArray arr = (jintArray)jni->CallObjectMethod( sd->alib_obj, m, par );
	if( arr )
	{
	    const jsize length = jni->GetArrayLength( arr );
	    *len = length;
	    int* ptr = jni->GetIntArrayElements( arr, NULL );
    	    rv = (int*)malloc( length * sizeof( int ) );
    	    smem_copy( rv, ptr, length * sizeof( int ) );
    	    jni->ReleaseIntArrayElements( arr, ptr, 0 );
    	}
    }

    return rv;
}

static char* java_call2_s_i( android_sundog_engine* sd, const char* method_name, int par )
{
    char* rv = NULL;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jmethodID m = jni->GetMethodID( sd->alib_class, method_name, "(I)Ljava/lang/String;" );
    if( m )
    {
	jstring s = (jstring)jni->CallObjectMethod( sd->alib_obj, m, par );
	if( s )
	{
    	    const char* str = jni->GetStringUTFChars( s, 0 );
    	    rv = strdup( str );
    	    jni->ReleaseStringUTFChars( s, str );
    	}
    }

    return rv;
}

static char* java_call2_s_s( android_sundog_engine* sd, const char* method_name, const char* str_par )
{
    char* rv = NULL;

    JNIEnv* jni = android_sundog_get_jni( sd );
    jmethodID m = jni->GetMethodID( sd->alib_class, method_name, "(Ljava/lang/String;)Ljava/lang/String;" );
    if( m )
    {
	jstring s = (jstring)jni->CallObjectMethod( sd->alib_obj, m, jni->NewStringUTF( str_par ) );
	if( s )
	{
    	    const char* str = jni->GetStringUTFChars( s, 0 );
    	    rv = strdup( str );
    	    jni->ReleaseStringUTFChars( s, str );
    	}
    }

    return rv;
}

bool android_sundog_allfiles_access_supported( sundog_engine* s )
{
    if( g_android_supported_permissions & ANDROID_PERM_MANAGE_EXTERNAL_STORAGE ) return 1;
    return 0;
}

int android_sundog_allfiles_access( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( g_android_supported_permissions & ANDROID_PERM_MANAGE_EXTERNAL_STORAGE )
    {
	//Show all-files access setgings (Android 11+; API LEVEL 30+)
	java_call2_i_i( sd, "AllFilesAccess", 1, 0, 0, 0, 0 );
    }
    return 0;
}

int android_sundog_check_for_permissions( sundog_engine* s, int p )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    p &= g_android_supported_permissions;
    p &= ~ANDROID_PERM_MANAGE_EXTERNAL_STORAGE;
    int rv = 0;
    while( 1 )
    {
	if( p == 0 ) break;
	int rv = java_call2_i_i( sd, "CheckForPermissions", 1, p, 0, 0, 0 );
	if( ( rv & p ) == p )
	{
	    break;
	}
	rv = 0;
	int t = 0;
	while( 1 )
	{
	    int rv2 = java_call2_i_i( sd, "CheckForPermissions", 1, -1, 0, 0, 0 );
	    if( rv2 != -1 )
	    {
		rv = rv2;
		break;
	    }
	    stime_sleep( 20 );
    	    t += 100;
    	    if( t > 60 * 1000 )
    	    {
    	        slog( "check_for_permissions() timeout\n" );
        	break;
    	    }
	}
	break;
    }
    return rv;
}

//n: 0 - primary; 1 - secondary;
//type: internal_files, external_files, internal_cache, external_cache;
//retval: string allocated with malloc()
static char* android_sundog_get_dir( android_sundog_engine* sd, const char* type, int n, bool replace_null_str_with_empty )
{
    char* rv = NULL;
    if( sd )
    {
	//Current value:
	char ts[ 32 ];
	const char* nstr = type;
	if( n >= 1 )
	{
	    snprintf( ts, sizeof( ts ), "%s%d", type, n + 1 );
	    nstr = (const char*)ts;
	}
	char* str = java_call2_s_s( sd, "GetDir", nstr );
	if( str )
	{
	    if( strstr( type, "external_files" ) && n < 2 )
	    {
		if( strstr( str, "Android/data/" ) )
		{
		    size_t s = strlen( str ) + 8;
		    char* media = (char*)malloc( s ); media[ 0 ] = 0;
		    // Android/data/appname/files -> Android/media/appname/files :
		    smem_replace_str( media, s, str, "Android/data/", "Android/media/" );
		    // Android/media/appname/files -> Android/media/appname :
		    char* tmp = strstr( media, "Android/media/" );
		    if( tmp )
		    {
			tmp = strstr( tmp + 14, "/" );
			if( tmp )
			{
			    tmp[ 0 ] = 0;
			    mkdir( media, 0777 );
			    //slog( "CREATE DIR: \"%s\"", media );
			}
		    }
		    free( media );
		}
	    }
	    mkdir( str, 0770 );
	    rv = (char*)malloc( strlen( str ) + 8 ); rv[ 0 ] = 0;
	    strcat( rv, str );
	    strcat( rv, "/" );
	    free( str );
	}
	if( strcmp( type, "external_files" ) == 0 )
	{
	    if( n > 0 && n < ANDROID_NUM_EXT_STORAGE_DEVS )
	    {
		//Save current value:
		char* path = g_android_files_ext_paths[ n ];
		if( rv == NULL )
		{
		    path[ 0 ] = 0;
		}
		else
		{
		    if( strcmp( path, rv ) )
		    {
			int len = strlen( rv );
			if( len <= ANDROID_EXT_STORAGE_DEV_NAMELEN )
			{
			    memcpy( path, rv, len + 1 );
			}
		    }
		}
	    }
	}
    }
    else
    {
	//Use cached value:
	while( 1 )
	{
	    if( strcmp( type, "external_files" ) == 0 )
	    {
		if( n == 0 ) rv = strdup( g_android_files_ext_path );
		if( n > 0 && n < ANDROID_NUM_EXT_STORAGE_DEVS )
		{
		    rv = g_android_files_ext_paths[ n ];
		    if( rv[ 0 ] == 0 )
			rv = NULL;
		    else
			rv = strdup( rv );
		}
		break;
	    }
	    if( strcmp( type, "internal_files" ) == 0 )
	    {
		if( n == 0 ) rv = strdup( g_android_files_int_path );
		break;
	    }
	    if( strcmp( type, "external_cache" ) == 0 )
	    {
		if( n == 0 ) rv = strdup( g_android_cache_ext_path );
		break;
	    }
	    if( strcmp( type, "internal_cache" ) == 0 )
	    {
		if( n == 0 ) rv = strdup( g_android_cache_int_path );
		break;
	    }
	    break;
	}
    }
    if( !rv )
    {
        if( replace_null_str_with_empty )
	    rv = strdup( "" );
    }
    return rv;
}
char* android_sundog_get_dir( sundog_engine* s, const char* type, int n )
{
    android_sundog_engine* sd = NULL;
    if( s ) sd = (android_sundog_engine*)s->device_specific;
    return android_sundog_get_dir( sd, type, n, false );
}

int android_sundog_copy_resources( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call_i_v( sd, "CopyResources" );
}

char* android_sundog_get_host_ips( sundog_engine* s, int mode )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_s_i( sd, "GetHostIPs", mode );
}

void android_sundog_open_url( sundog_engine* s, const char* url_text )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    java_call2_i_si( sd, "OpenURL", url_text, 0 );
}

void android_sundog_send_file_to_gallery( sundog_engine* s, const char* path )
{
    if( g_android_version_nums[ 0 ] < 10 ) android_sundog_check_for_permissions( s, ANDROID_PERM_WRITE_EXTERNAL_STORAGE );
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    java_call2_i_si( sd, "SendFileToGallery", path, 0 );
}

void android_sundog_clipboard_copy( sundog_engine* s, const char* txt )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    java_call2_i_si( sd, "ClipboardCopy", txt, 0 );
}

char* android_sundog_clipboard_paste( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_s_i( sd, "ClipboardPaste", 0 );
}

//Get the native or optimal output buffer size - 
//number of audio frames that the HAL (Hardware Abstraction Layer) buffer can hold.
//You should construct your audio buffers so that they contain an exact multiple of this number.
//If you use the correct number of audio frames, your callbacks occur at regular intervals, which reduces jitter.
int android_sundog_get_audio_buffer_size( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "GetAudioOutputBufferSize", 0, 0, 0, 0, 0 );
}

//Get the native or optimal output sample rate
int android_sundog_get_audio_sample_rate( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "GetAudioOutputSampleRate", 0, 0, 0, 0, 0 );
}

int* android_sundog_get_exclusive_cores( sundog_engine* s, size_t* len )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( g_android_version_nums[ 0 ] < 7 )
    {
	//Not supported:
	return NULL;
    }
    return java_call2_iarr_i( sd, "GetExclusiveCores", len, 0 );
}

int android_sundog_set_sustained_performance_mode( sundog_engine* s, int enable )
{
    int rv = -1;
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( g_android_version_nums[ 0 ] < 7 )
    {
	//Not supported:
	return -1;
    }
    if( (bool)enable != sd->sustained_performance_mode )
    {
	sd->sustained_performance_mode = enable;
	rv = java_call2_i_i( sd, "SetSustainedPerformanceMode", 1, enable, 0, 0, 0 );
    }
    return rv;
}

static uint64_t android_sundog_get_window_insets( android_sundog_engine* sd )
{
    if( g_android_version_nums[ 0 ] < 10 )
    {
	//Not supported:
	return 0;
    }
    if( sd->sys_ui_visible == 0 ) return 0;
    return java_call2_i64_i( sd, "GetWindowInsets", 0, 0 );
}

void android_sundog_set_safe_area( sundog_engine* s )
{
    if( g_android_version_nums[ 0 ] < 10 )
    {
	//Not supported:
	return;
    }
    if( !s ) return;
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( !sd ) return;
    uint64_t i = android_sundog_get_window_insets( sd );
#ifndef SUNDOG_VER
    int x = ( i & 0xFFFF );
    int y = ( ( i >> 16 ) & 0xFFFF );
    int x2 = ( ( i >> 32 ) & 0xFFFF );
    int y2 = ( ( i >> 48 ) & 0xFFFF );
    int w = 0;
    int h = 0;
    if( i )
    {
	w = s->screen_xsize - x - x2; if( w < 0 ) w = 0;
	h = s->screen_ysize - y - y2; if( h < 0 ) h = 0;
    }
    s->screen_safe_area[ 0 ] = x;
    s->screen_safe_area[ 1 ] = y;
    s->screen_safe_area[ 2 ] = w;
    s->screen_safe_area[ 3 ] = h;
    //LOGI( "SAFE AREA: %d %d %d %d\n", x, y, x2, y2 );
#endif
}

static int android_sundog_set_system_ui_visibility( android_sundog_engine* sd, int v )
{
    if( sd->sys_ui_vchanged == false && v == 1 )
    {
	//Already visible (by default)
	return -1;
    }
    if( g_android_version_nums[ 0 ] < 4 )
    {
	//Not supported:
	return -1;
    }
    sd->sys_ui_vchanged = true;
    sd->sys_ui_visible = v;
    int rv = java_call2_i_i( sd, "SetSystemUIVisibility", 1, v, 0, 0, 0 );
#ifndef SUNDOG_VER
    if( rv == 0 ) sd->s.screen_changed_w++;
#endif
    return rv;
}

int android_sundog_set_system_ui_visibility( sundog_engine* s, int v )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    if( !sd ) return -1;
    return android_sundog_set_system_ui_visibility( sd, v );
}

int android_sundog_open_camera( sundog_engine* s, int cam_id, void* user_data )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    android_sundog_check_for_permissions( s, ANDROID_PERM_CAMERA );
    int rv = java_call2_i_ii64( sd, "OpenCamera", cam_id, (int64_t)user_data );
    if( rv >= 0 )
    {
	sd->camera_connections |= ( 1 << rv );
    }
    return rv;
}

int android_sundog_close_camera( sundog_engine* s, int con_index )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    int rv = java_call2_i_i( sd, "CloseCamera", 1, con_index, 0, 0, 0 );
    if( rv == 0 )
    {
	sd->camera_connections &= ~( 1 << con_index );
    }
    return rv;
}

int android_sundog_get_camera_width( sundog_engine* s, int con_index )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "GetCameraWidth", 1, con_index, 0, 0, 0 );
}

int android_sundog_get_camera_height( sundog_engine* s, int con_index )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "GetCameraHeight", 1, con_index, 0, 0, 0 );
}

int android_sundog_get_camera_format( sundog_engine* s, int con_index )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "GetCameraFormat", 1, con_index, 0, 0, 0 );
}

int android_sundog_get_camera_focus_mode( sundog_engine* s, int con_index )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "GetCameraFocusMode", 1, con_index, 0, 0, 0 );
}

int android_sundog_set_camera_focus_mode( sundog_engine* s, int con_index, int mode )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "SetCameraFocusMode", 2, con_index, mode, 0, 0 );
}

struct android_app* android_sundog_get_app_struct( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return sd->app;
}

int android_sundog_midi_init( sundog_engine* s )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "MIDIInit", 0, 0, 0, 0, 0 );
}

//flags: 1 - for reading; 2 - for writing;
char* android_sundog_get_midi_devports( sundog_engine* s, int flags )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_s_i( sd, "GetMIDIDevports", flags );
}

//rw: 0 - for reading; 1 - for writing;
int android_sundog_midi_open_devport( sundog_engine* s, const char* name, int port_id )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_si( sd, "OpenMIDIDevport", name, port_id );
}

int android_sundog_midi_close_devport( sundog_engine* s, int con_index )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "CloseMIDIDevport", 1, con_index, 0, 0, 0 );
}

int android_sundog_midi_send( sundog_engine* s, int con_index, int msg, int msg_len, int t )
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    return java_call2_i_i( sd, "MIDISend", 4, con_index, msg, msg_len, t );
}

//
// Main
//

#ifndef SUNDOG_VER
    #include "sundog_bridge1.hpp"
#endif
#if SUNDOG_VER == 2
    #include "sundog_bridge2.hpp"
#endif

void sundog_state_set( sundog_engine* s, int io, sundog_state* state ) //io: 0 - app input; 1 - app output;
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    sundog_state* prev_state = NULL;
    if( io == 0 )
    {
        prev_state = sd->in_state;
        sd->in_state = state;
    }
    else
    {
    }
    sundog_state_remove( prev_state );
}

sundog_state* sundog_state_get( sundog_engine* s, int io ) //io: 0 - app input; 1 - app output;
{
    android_sundog_engine* sd = (android_sundog_engine*)s->device_specific;
    sundog_state* state = NULL;
    if( io == 0 )
    {
        state = sd->in_state;
        sd->in_state = NULL;
    }
    else
    {
    }
    return state;
}

#ifdef SUNDOG_MODULE
static sundog::lazy_init_helper engine_global;
#endif
static int engine_global_init( android_sundog_engine* sd )
{
#ifdef SUNDOG_MODULE
    //Global init once per process
    int rv = engine_global.init_begin( "engine_global_init()", 5000, 5 );
    if( rv <= 0 ) return rv;
#endif

    g_android_cache_int_path = android_sundog_get_dir( sd, "internal_cache", 0, true ); //Files in this folder may be deleted by system
    g_android_files_int_path = android_sundog_get_dir( sd, "internal_files", 0, true ); //Hidden for the user
    g_android_cache_ext_path = android_sundog_get_dir( sd, "external_cache", 0, true ); //Primary storage device: cache files that may be deleted by system
    g_android_files_ext_path = android_sundog_get_dir( sd, "external_files", 0, true ); //Primary storage device: working directory
    LOGI( "Internal Cache Dir: %s", g_android_cache_int_path );
    LOGI( "Internal Files Dir: %s", g_android_files_int_path );
    LOGI( "External Cache Dir: %s", g_android_cache_ext_path );
    LOGI( "External Files Dir: %s", g_android_files_ext_path );
    for( int i = 1; i < ANDROID_NUM_EXT_STORAGE_DEVS; i++ )
    {
	char* path = (char*)malloc( ANDROID_EXT_STORAGE_DEV_NAMELEN + 1 ); //+1 zero byte at the end (for safety reasons)
	memset( path, 0, ANDROID_EXT_STORAGE_DEV_NAMELEN + 1 );
	g_android_files_ext_paths[ i ] = path;
    }
    for( int i = 1; i < ANDROID_NUM_EXT_STORAGE_DEVS; i++ )
    {
	char* dir = android_sundog_get_dir( sd, "external_files", i, false );
	if( !dir ) break;
	LOGI( "External Files Dirs [%d]: %s", i, dir );
	free( dir );
    }

    char* str = NULL;
    str = java_call2_s_i( sd, "GetOSVersion", 0 );
    if( str )
	g_android_version = str;
    else
	g_android_version = strdup( "2.3" );
    LOGI( "OS Version: %s", g_android_version );

    str = java_call2_s_i( sd, "GetLanguage", 0 );
    if( str )
	g_android_lang = str;
    else
	g_android_lang = strdup( "en_US" );
    LOGI( "Language: %s", g_android_lang );

    memset( g_android_version_nums, 0, sizeof( g_android_version_nums ) );
    for( int i = 0, android_version_ptr = 0; ; i++ )
    {
        char c = g_android_version[ i ];
        if( c == 0 ) break;
        if( c == '.' ) android_version_ptr++;
        if( c >= '0' && c <= '9' )
        {
            g_android_version_nums[ android_version_ptr ] *= 10;
            g_android_version_nums[ android_version_ptr ] += c - '0';
        }
    }
    sprintf( g_android_version_correct, "%d.%d.%d", g_android_version_nums[ 0 ], g_android_version_nums[ 1 ], g_android_version_nums[ 2 ] );
    LOGI( "Android version (correct): %s\n", g_android_version_correct );

    str = java_call2_s_i( sd, "GetRequestedPermissions", 0 );
    if( str )
	g_android_requested_permissions = str;
    else
	g_android_requested_permissions = strdup( "" );
    //LOGI( "Req.Permissions: %s", g_android_requested_permissions );
    g_android_supported_permissions = 0xFFFFFFFF;
    if( g_android_version_nums[ 0 ] < 13 )
    {
        //Not supported:
	g_android_supported_permissions &= ~( 
	    ANDROID_PERM_READ_MEDIA_AUDIO |
	    ANDROID_PERM_READ_MEDIA_IMAGES |
	    ANDROID_PERM_READ_MEDIA_VIDEO
	);
    }
    else
    {
	//Android 13+ (API LEVEL 33)
	g_android_supported_permissions &= ~ANDROID_PERM_READ_EXTERNAL_STORAGE; //READ_EXTERNAL_STORAGE has no effect
	if( strstr( g_android_requested_permissions, "READ_MEDIA_AUDIO" ) == nullptr )
	    g_android_supported_permissions &= ~ANDROID_PERM_READ_MEDIA_AUDIO;
	if( strstr( g_android_requested_permissions, "READ_MEDIA_IMAGES" ) == nullptr )
	    g_android_supported_permissions &= ~ANDROID_PERM_READ_MEDIA_IMAGES;
	if( strstr( g_android_requested_permissions, "READ_MEDIA_VIDEO" ) == nullptr )
    	    g_android_supported_permissions &= ~ANDROID_PERM_READ_MEDIA_VIDEO;
    }
    if( g_android_version_nums[ 0 ] < 11 )
    {
        //Not supported:
	g_android_supported_permissions &= ~( ANDROID_PERM_MANAGE_EXTERNAL_STORAGE );
    }
    else
    {
	//Android 11+ (API LEVEL 30)
	g_android_supported_permissions &= ~ANDROID_PERM_WRITE_EXTERNAL_STORAGE; //WRITE_EXTERNAL_STORAGE has no effect
	if( strstr( g_android_requested_permissions, "MANAGE_EXTERNAL_STORAGE" ) == nullptr )
	    g_android_supported_permissions &= ~ANDROID_PERM_MANAGE_EXTERNAL_STORAGE;
    }

#ifdef SUNDOG_MODULE
    COMPILER_MEMORY_BARRIER();
    engine_global.init_end();
#endif
    return 0;
}
int engine_global_deinit( android_sundog_engine* sd )
{
#ifdef SUNDOG_MODULE
    //Global deinit once per process
    int rv = engine_global.deinit_begin();
    if( rv <= 0 ) return rv;
#endif

    free( g_android_cache_int_path );
    free( g_android_cache_ext_path );
    free( g_android_files_int_path );
    free( g_android_files_ext_path );
    for( int i = 1; i < ANDROID_NUM_EXT_STORAGE_DEVS; i++ )
    {
	char* path = g_android_files_ext_paths[ i ];
	free( path );
	g_android_files_ext_paths[ i ] = NULL;
    }
    free( g_android_version );
    free( g_android_lang );
    free( g_android_requested_permissions );

#ifdef SUNDOG_MODULE
    COMPILER_MEMORY_BARRIER();
    engine_global.deinit_end();
#endif
    return 0;
}

static JNIEnv* engine_jni_init( android_sundog_engine* sd )
{
    JNIEnv* jni = NULL;
    int err = 0;
    while( 1 )
    {
	pthread_once( &g_key_once, make_global_pthread_keys );
        //The first call to pthread_once() by any thread in a process, will call the make_global_pthread_keys() with no arguments.
	//Subsequent calls of pthread_once() with the same &g_key_once will not call the make_global_pthread_keys().

	jni = android_sundog_get_jni( sd );
	if( !jni )
	{
	    LOGE( "No JNI!" );
	    err = 1;
	    break;
	}

	//Get some Java classes and objects:

	jclass c = NULL;

	c = jni->FindClass( "android/app/NativeActivity" );
	if( !c ) { LOGE( "NativeActivity class not found" ); err = 2; break; }
        sd->gref_na_class = (jclass)jni->NewGlobalRef( c );
	jni->DeleteLocalRef( c ); c = NULL;

        jmethodID getApplicationContext = jni->GetMethodID( sd->gref_na_class, "getApplicationContext", "()Landroid/content/Context;" );
	jobject context_obj = jni->CallObjectMethod( android_get_activity_obj( sd->app ), getApplicationContext );
	if( !context_obj ) { LOGE( "Context error" ); err = 3; break; }
	jclass context_class = jni->GetObjectClass( context_obj );
	jmethodID getPackageName = jni->GetMethodID( context_class, "getPackageName", "()Ljava/lang/String;" );
	jstring packName = (jstring)jni->CallObjectMethod( context_obj, getPackageName );
	if( !packName ) { LOGE( "Can't get package name" ); err = 4; break; }

	char ts[ 256 ];
	const char* pkg_name_cstr = jni->GetStringUTFChars( packName, NULL );
	sd->package_name[ 0 ] = 0;
	sprintf( sd->package_name, "%s", pkg_name_cstr );
	sprintf( ts, "%s/MyNativeActivity", pkg_name_cstr );
	jni->ReleaseStringUTFChars( packName, pkg_name_cstr );

	c = java_find_class( sd, jni, ts ); //Main app class "PackageName/MyNativeActivity"
	if( !c ) { LOGE( "MyNativeActivity class not found" ); err = 5; break; }
    	sd->gref_main_class = (jclass)jni->NewGlobalRef( c );
	jni->DeleteLocalRef( c ); c = NULL;

	jfieldID fid = jni->GetFieldID( sd->gref_main_class, "lib", "Lnightradio/androidlib/AndroidLib;" );
	if( fid )
	{
	    jobject lib_obj = jni->GetObjectField( android_get_activity_obj( sd->app ), fid );
	    if( lib_obj )
	    {
	        c = java_find_class( sd, jni, "nightradio/androidlib/AndroidLib" );
	        if( c )
	        {
    	    	    sd->alib_class = (jclass)jni->NewGlobalRef( c );
    		    sd->alib_obj = jni->NewGlobalRef( lib_obj );
		    jni->DeleteLocalRef( c ); c = NULL;
		    jni->DeleteLocalRef( lib_obj ); lib_obj = NULL;
		}
	    }
	    else
	    {
	        LOGE( "AndroidLib object not found" );
	        err = 6;
	        break;
	    }
	}
	else
	{
	    LOGE( "AndroidLib field not found" );
	    err = 7;
	    break;
	}

	break;
    }
    if( err )
    {
	jni = NULL;
    }
    return jni;
}
static void engine_jni_deinit( android_sundog_engine* sd, JNIEnv* jni )
{
    jni->DeleteGlobalRef( sd->gref_na_class );
    jni->DeleteGlobalRef( sd->gref_main_class );
    jni->DeleteGlobalRef( sd->alib_class );
    jni->DeleteGlobalRef( sd->alib_obj );
    android_sundog_release_jni( sd );
}

static void engine_delete( android_sundog_engine* sd )
{
    if( !sd ) return;

    free( sd->intent_file_name ); sd->intent_file_name = NULL;

    engine_global_deinit( sd );
    if( sd->jni ) engine_jni_deinit( sd, sd->jni );

    free( sd );
}
static android_sundog_engine* engine_new( struct android_app* app )
{
    android_sundog_engine* sd = NULL;
    int err = 0;
    while( 1 )
    {
	sd = (android_sundog_engine*)malloc( sizeof( android_sundog_engine ) );
	if( !sd ) { err = 1; break; }
	memset( sd, 0, sizeof( android_sundog_engine ) );
	//For future updates: don't forget to clear in_state at the first initialization, like in iOS bridge!
	sd->app = app;
        app->userData = sd;
	app->onAppCmd = engine_handle_cmd;
#ifndef SUNDOG_VER
        app->onInputEvent = engine_handle_input;
#endif
#if SUNDOG_VER == 2
	app->textInputState = 0;
#endif

	sd->main_thread_id = sthread_gettid();

	sd->jni = engine_jni_init( sd );
	if( !sd->jni ) { err = 2; break; }

	engine_global_init( sd );

	sd->intent_file_name = java_call2_s_i( sd, "GetIntentFile", 0 );
	if( sd->intent_file_name ) LOGI( "Intent File Name: %s\n", sd->intent_file_name );

#if SUNDOG_VER == 2
	//android_app_set_key_event_filter( app, NULL );
	android_app_set_motion_event_filter( app, NULL );
#endif

	break;
    }
    if( err )
    {
	engine_delete( sd );
	return NULL;
    }
    return sd;
}

#if SUNDOG_VER == 2
extern "C" {
    void android_main( struct android_app* app );
};
#endif
volatile int g_activity_state = 0;
void android_main( struct android_app* app )
{
    //This code always runs on a new thread
    LOGI( "ANDROID MAIN " ARCH_NAME " " __DATE__ " " __TIME__ );

    if( g_activity_state )
    {
	//Only one app instance is allowed in this version of SunDog engine
	//For future updates (SunDog2?):
	// 1) add support of multiple instances (without globals, like g_sundog_bridge etc.);
	// 2) manifest: remove android:launchMode="singleTask" or replace it by "singleTop"; then test "open in..." from the other apps;
	//              OS 4.4.2: "singleTop" does not work as expected?...
	// 3) add SUNDOG_MODULE define?
	// 4) manifest: add android:resizeableActivity ? (level 24, OS 7.0);
	LOGE( "Only one app instance is allowed. Closing current activity..." );
#ifndef SUNDOG_VER
	ANativeActivity_finish( app->activity );
#endif
#if SUNDOG_VER == 2
    	GameActivity_finish( app->activity );
#endif
	while( 1 )
        {
	    int ident;
            int events;
	    struct android_poll_source* source;
    	    while( 1 )
	    {
		int poll_res = ALooper_pollOnce( -1, NULL, &events, (void**)&source );
		if( poll_res >= 0 || poll_res == ALOOPER_POLL_CALLBACK )
		{
        	    if( source ) source->process( app, source );
    		    if( app->destroyRequested ) return;
    		}
    		else break;
    	    }
	}
    	return;
    }
    g_activity_state = 1;

    android_sundog_engine* sd = engine_new( app );
    engine_loop( sd );
    engine_delete( sd );

    g_activity_state = 0;
    LOGI( "ANDROID MAIN FINISHED" );
}

#endif //!NOMAIN
