#pragma once

#include <jni.h>
#include <android/log.h>
#ifndef SUNDOG_VER
    #include <android_native_app_glue.h>
#endif
#if SUNDOG_VER == 2
    #include <game-activity/native_app_glue/android_native_app_glue.h>
#endif

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "native-activity", __VA_ARGS__))

extern char* g_android_cache_int_path;
extern char* g_android_cache_ext_path;
extern char* g_android_files_int_path;
extern char* g_android_files_ext_path;
#define ANDROID_NUM_EXT_STORAGE_DEVS 3 //Max number of shared/external storage devices (but not USB drives!), including the primary storage device
extern char* g_android_version;
extern char g_android_version_correct[ 16 ]; //"X.Y.Z"
extern int g_android_version_nums[ 8 ];
extern char* g_android_lang;
extern char* g_android_requested_permissions;

//Get the NativeActivity/GameActivity object handle:
inline jobject android_get_activity_obj( struct android_app* app )
{
#ifndef SUNDOG_VER
    return app->activity->clazz;
#endif
#if SUNDOG_VER == 2
    return app->activity->javaGameActivity;
#endif
    return NULL;
}

#ifndef NOMAIN

JNIEnv* android_sundog_get_jni( sundog_engine* s );
void android_sundog_release_jni( sundog_engine* s ); //-> DetachCurrentThread()
bool android_sundog_allfiles_access_supported( sundog_engine* s );
int android_sundog_allfiles_access( sundog_engine* s ); //show screen for controlling if the app can manage external storage (broad access)
#define ANDROID_PERM_WRITE_EXTERNAL_STORAGE		(1<<0)
#define ANDROID_PERM_RECORD_AUDIO			(1<<1)
#define ANDROID_PERM_CAMERA				(1<<2)
#define ANDROID_PERM_READ_EXTERNAL_STORAGE		(1<<3)
#define ANDROID_PERM_READ_MEDIA_AUDIO			(1<<4)
#define ANDROID_PERM_READ_MEDIA_IMAGES			(1<<5)
#define ANDROID_PERM_READ_MEDIA_VIDEO			(1<<6)
#define ANDROID_PERM_MANAGE_EXTERNAL_STORAGE		(1<<7) //request all-files access (Android 11+; API LEVEL 30+)
int android_sundog_check_for_permissions( sundog_engine* s, int p ); //p: set of ANDROID_PERM_*; retval: enabled permissions;
//android_sundog_get_dir()
//  n: 0 - primary; 1 - secondary;
//  type: internal_files, external_files, internal_cache, external_cache;
//  retval: string allocated with malloc()
char* android_sundog_get_dir( sundog_engine* s, const char* type, int n );
int android_sundog_copy_resources( sundog_engine* s );
char* android_sundog_get_host_ips( sundog_engine* s, int mode );
void android_sundog_open_url( sundog_engine* s, const char* url_text );
void android_sundog_send_file_to_gallery( sundog_engine* s, const char* path );
void android_sundog_clipboard_copy( sundog_engine* s, const char* txt );
char* android_sundog_clipboard_paste( sundog_engine* s );
int android_sundog_get_audio_buffer_size( sundog_engine* s );
int android_sundog_get_audio_sample_rate( sundog_engine* s );
int* android_sundog_get_exclusive_cores( sundog_engine* s, size_t* len );
int android_sundog_set_sustained_performance_mode( sundog_engine* s, int enable );
void android_sundog_set_safe_area( sundog_engine* s );
int android_sundog_set_system_ui_visibility( sundog_engine* s, int v );
int android_sundog_open_camera( sundog_engine* s, int cam_id, void* user_data );
int android_sundog_close_camera( sundog_engine* s, int con_index );
int android_sundog_get_camera_width( sundog_engine* s, int con_index );
int android_sundog_get_camera_height( sundog_engine* s, int con_index );
int android_sundog_get_camera_format( sundog_engine* s, int con_index );
int android_sundog_get_camera_focus_mode( sundog_engine* s, int con_index );
int android_sundog_set_camera_focus_mode( sundog_engine* s, int con_index, int mode );
struct android_app* android_sundog_get_app_struct( sundog_engine* s );
int android_sundog_midi_init( sundog_engine* s );
char* android_sundog_get_midi_devports( sundog_engine* s, int flags ); //get list of strings: device name + port name \n ...
int android_sundog_midi_open_devport( sundog_engine* s, const char* name, int port_id );
int android_sundog_midi_close_devport( sundog_engine* s, int con_index );
int android_sundog_midi_send( sundog_engine* s, int con_index, int msg, int msg_len, int t );

#ifndef SUNDOG_VER
//SunDog1:
void android_sundog_screen_redraw( sundog_engine* s );
void android_sundog_event_handler( window_manager* wm );
bool android_sundog_get_gl_buffer_preserved( sundog_engine* s );
EGLDisplay android_sundog_get_egl_display( sundog_engine* s );
EGLSurface android_sundog_get_egl_surface( sundog_engine* s );
#endif

#if SUNDOG_VER == 2
#endif

#endif
