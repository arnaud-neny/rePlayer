#define DEVICE_VIDEO_FUNCTIONS

#include <jni.h>
#include <ctype.h>
#include <android/configuration.h>
#include "main/android/sundog_bridge.h"

enum 
{
    ANDROID_CAM_NONE,
    ANDROID_CAM_NATIVE, //not implemented yet; Camera Native API (Android 7+)
    ANDROID_CAM_JAVA,
};
int g_android_camera_mode = ANDROID_CAM_NONE;

extern "C" JNIEXPORT jint JNICALL Java_nightradio_androidlib_AndroidLib_camera_1frame_1callback1( JNIEnv* je, jclass jc, int con_index, jlong user_data, jbyteArray data )
{
    if( sizeof( user_data ) < sizeof( void* ) )
    {
	slog( "camera_frame_callback1(): can't convert jlong to pointer!\n" );
	return 0;
    }
    
    svideo_struct* vid = (svideo_struct*)user_data;
    if( vid == NULL ) return 0;
    if( vid->con_index != con_index ) return 0;

    int data_len = je->GetArrayLength( data );
    jbyte* data_ptr = je->GetByteArrayElements( data, NULL );

    smutex_lock( &vid->callback_mutex );
    if( vid->callback_active && vid->capture_callback )
    {
	stime_ms_t t = stime_ms();
        if( t - vid->fps_time > 1000 )
        {
            vid->fps = vid->fps_counter;
            vid->fps_counter = 0;
            vid->fps_time = t;
        }
        vid->fps_counter++;
        vid->frame_counter++;

	vid->capture_plans_cnt = 1;
	svideo_plan* plan;
	plan = &vid->capture_plans[ 0 ];
	plan->buf = data_ptr;
	plan->buf_size = data_len;

	vid->capture_callback( vid );
    }
    smutex_unlock( &vid->callback_mutex );

    je->ReleaseByteArrayElements( data, data_ptr, 0 );

    return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_nightradio_androidlib_AndroidLib_camera_1frame_1callback2( JNIEnv* je, jclass jc, int con_index, jlong user_data, jobject pl0, jobject pl1, jobject pl2, int ps0, int ps1, int rs0, int rs1 )
{
    if( sizeof( user_data ) < sizeof( void* ) )
    {
	slog( "camera_frame_callback2(): can't convert jlong to pointer!\n" );
	return 0;
    }

    svideo_struct* vid = (svideo_struct*)user_data;
    if( vid == NULL ) return 0;
    if( vid->con_index != con_index ) return 0;

    uint8_t* p0 = NULL;
    uint8_t* p1 = NULL;
    uint8_t* p2 = NULL;
    int p0_len = 0;
    int p1_len = 0;
    int p2_len = 0;
    if( pl0 ) { p0 = (uint8_t*)je->GetDirectBufferAddress( pl0 ); p0_len = je->GetDirectBufferCapacity( pl0 ); }
    if( pl1 ) { p1 = (uint8_t*)je->GetDirectBufferAddress( pl1 ); p1_len = je->GetDirectBufferCapacity( pl1 ); }
    if( pl2 ) { p2 = (uint8_t*)je->GetDirectBufferAddress( pl2 ); p2_len = je->GetDirectBufferCapacity( pl2 ); }

    if( p0_len <= 0 || p1_len <= 0 || p2_len <= 0 ) return 0;

    smutex_lock( &vid->callback_mutex );
    if( vid->callback_active && vid->capture_callback )
    {
	stime_ms_t t = stime_ms();
        if( t - vid->fps_time > 1000 )
        {
            vid->fps = vid->fps_counter;
            vid->fps_counter = 0;
            vid->fps_time = t;
        }
        vid->fps_counter++;
        vid->frame_counter++;

	vid->capture_plans_cnt = 3;
	svideo_plan* plan;
	plan = &vid->capture_plans[ 0 ];
	plan->buf = p0;
	plan->buf_size = p0_len;
        plan->pixel_stride = ps0;
        plan->row_stride = rs0;
        plan = &vid->capture_plans[ 1 ];
        plan->buf = p1;
        plan->buf_size = p1_len;
        plan->pixel_stride = ps1;
        plan->row_stride = rs1;
        plan = &vid->capture_plans[ 2 ];
        plan->buf = p2;
        plan->buf_size = p2_len;
        plan->pixel_stride = ps1;
        plan->row_stride = rs1;

	vid->capture_callback( vid );
    }
    smutex_unlock( &vid->callback_mutex );

    return 0;
}

int device_video_global_init()
{
    slog( "Android version (correct): %s\n", g_android_version_correct );

    g_android_camera_mode = ANDROID_CAM_NONE;
    if( 1 )
    {
	g_android_camera_mode = ANDROID_CAM_JAVA;
	return 0;
    }

    return 0;
}

int device_video_global_deinit()
{
    return 0;
}

int device_video_open( svideo_struct* vid, const char* name, uint flags )
{
    int rv = -1;
    vid->cam_index = -9999;
    vid->orientation = ( 0 + ( ( vid->flags >> SVIDEO_OPEN_FLAGOFF_ROTATE ) & 3 ) ) & 3;
    smutex_init( &vid->callback_mutex, 0 );
    if( g_android_camera_mode == ANDROID_CAM_JAVA )
    {
	while( 1 )
	{
	    if( smem_strcmp( name, "camera" ) == 0 ) 
	    {
		//Default:
		vid->cam_index = sconfig_get_int_value( APP_CFG_CAMERA, 0, 0 );
	    }
	    if( smem_strcmp( name, "camera_back" ) == 0 ) vid->cam_index = 0;
	    if( smem_strcmp( name, "camera_front" ) == 0 ) vid->cam_index = 1;
	    if( vid->cam_index == -9999 ) break;
	    rv = android_sundog_open_camera( vid->sd, vid->cam_index, vid );
	    if( rv >= 0 )
	    {
		vid->con_index = rv;
		if( vid->sd->screen_orientation == ACONFIGURATION_ORIENTATION_PORT )
		{
		    vid->orientation--;
		    vid->orientation &= 3;
		    /*
		    //We can use Camera.CameraInfo orientation, but it gives the angle relative to "natural orientation".
		    //The question is: what is the "natural orientation" and how to get it from the native activity?
		    vid->orientation -= android_sundog_get_camera_orientation() / 90;
		    vid->orientation &= 3;
		    */
		}
	    }
	    break;
	}
    }
    if( rv < 0 )
    {
	slog( "device_video_open() error %d\n", rv );
	smutex_destroy( &vid->callback_mutex );
    }
    else
    {
	slog( "device_video_open() successful: %s %d %d %d\n", name, vid->cam_index, vid->orientation, vid->con_index );
    }
    return rv;
}

int device_video_close( svideo_struct* vid )
{
    int rv = -1;
    slog( "Camera frames: %d\n", vid->frame_counter );
    slog( "device_video_close() ...\n" );
    if( g_android_camera_mode == ANDROID_CAM_JAVA )
    {
	rv = android_sundog_close_camera( vid->sd, vid->con_index );
    }
    smutex_destroy( &vid->callback_mutex );
    if( rv == -1 )
    {
	slog( "device_video_close() error %d\n", rv );
    }
    else
    {
	slog( "device_video_close() successful\n" );
    }
    return rv;
}

int device_video_start( svideo_struct* vid )
{
    int rv = -1;
    if( g_android_camera_mode == ANDROID_CAM_JAVA )
    {
	rv = 0;
    }
    if( rv == 0 )
    {
	smutex_lock( &vid->callback_mutex );
	vid->callback_active = 1;
	vid->fps_time = stime_ms();
        vid->fps_counter = 0;
	smutex_unlock( &vid->callback_mutex );
    }
    return rv;
}

int device_video_stop( svideo_struct* vid )
{
    int rv = -1;
    if( g_android_camera_mode == ANDROID_CAM_JAVA )
    {
	rv = 0;
    }
    if( rv == 0 )
    {
	smutex_lock( &vid->callback_mutex );
	vid->callback_active = 0;
	smutex_unlock( &vid->callback_mutex );
    }
    return rv;
}

int device_video_set_props( svideo_struct* vid, svideo_prop* props )
{
    int rv = -1;
    if( g_android_camera_mode == ANDROID_CAM_JAVA )
    {
        while( 1 )
	{
	    if( props == 0 ) break;
	    for( int i = 0; ; i++ )
	    {
		svideo_prop* prop = &props[ i ];
		if( prop->id == SVIDEO_PROP_NONE ) break;
		switch( prop->id )
		{
		    case SVIDEO_PROP_FOCUS_MODE_I: 
			{
			    int mode = -1;
			    switch( prop->val.i )
			    {
				case SVIDEO_FOCUS_MODE_AUTO: mode = 0; break;
				case SVIDEO_FOCUS_MODE_CONTINUOUS: mode = 1; break;
			    }
			    if( mode != -1 )
			    {
				android_sundog_set_camera_focus_mode( vid->sd, vid->con_index, mode );
			    }
			}
			break;
		    default: break;
		}
	    }
	    rv = 0;
	    break;
	}
    }
    return rv;
}

int device_video_get_props( svideo_struct* vid, svideo_prop* props )
{
    int rv = -1;
    if( g_android_camera_mode == ANDROID_CAM_JAVA )
    {
	while( 1 )
	{
	    if( props == 0 ) break;
	    for( int i = 0; ; i++ )
	    {
		svideo_prop* prop = &props[ i ];
		if( prop->id == SVIDEO_PROP_NONE ) break;
		switch( prop->id )
		{
		    case SVIDEO_PROP_FRAME_WIDTH_I: prop->val.i = android_sundog_get_camera_width( vid->sd, vid->con_index ); break;
		    case SVIDEO_PROP_FRAME_HEIGHT_I: prop->val.i = android_sundog_get_camera_height( vid->sd, vid->con_index ); break;
		    case SVIDEO_PROP_PIXEL_FORMAT_I:
			{
			    int fmt = android_sundog_get_camera_format( vid->sd, vid->con_index );
			    prop->val.i = SVIDEO_PIXEL_FORMAT_YCrCb420_SEMIPLANAR;
			    if( fmt == 2 ) prop->val.i = SVIDEO_PIXEL_FORMAT_YCbCr420;
			}
			break;
		    case SVIDEO_PROP_FPS_I: prop->val.i = vid->fps; break;
		    case SVIDEO_PROP_FOCUS_MODE_I: 
			{
			    int focus_mode = android_sundog_get_camera_focus_mode( vid->sd, vid->con_index );
			    switch( focus_mode )
			    {
				case 1: prop->val.i = SVIDEO_FOCUS_MODE_CONTINUOUS; break;
				default: prop->val.i = SVIDEO_FOCUS_MODE_AUTO; break;
			    }
			}
			break;
		    case SVIDEO_PROP_SUPPORTED_PREVIEW_SIZES_P: break;
		    case SVIDEO_PROP_ORIENTATION_I: prop->val.i = vid->orientation; break;
		    default: break;
		}
	    }
	    rv = 0;
	    break;
	}
    }
    return rv;
}
