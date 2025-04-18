#pragma once

// possible names for svideo_open():
//   * video file name; (not yet implemented)
//   * device name;
//   * camera - default camera or value from the config file (APP_CFG_CAMERA; 0 - back, 1 - front, 2, 3, 4 ...);
//   * camera_front;
//   * camera_back;

//default = option is missing (or commented out) in the config file;
#define APP_CFG_CAMERA			"camera" //default camera ID
#define APP_CFG_CAM_ROTATE		"cam_rotate" //camera image rotation angle counterclockwise: default = 0;

enum 
{
    SVIDEO_PROP_NONE = 0,
    SVIDEO_PROP_FRAME_WIDTH_I,
    SVIDEO_PROP_FRAME_HEIGHT_I,
    SVIDEO_PROP_PIXEL_FORMAT_I,
    SVIDEO_PROP_FPS_I,
    SVIDEO_PROP_FOCUS_MODE_I,
    SVIDEO_PROP_SUPPORTED_PREVIEW_SIZES_P,
    SVIDEO_PROP_ORIENTATION_I, //image orientation; relative to the native window (in its original orientation (wm->screen_angle = 0));
		               //angle in degrees = orientation * 90;
		               //angle that the image needs to be rotated counterclockwise (против часовой стрелки) so it shows correctly on the display
};

enum
{
    SVIDEO_PIXEL_FORMAT_YCbCr422, //packed: Y Cb Y Cr ... ; planar: [ YYYY YYYY ] [ CbCb CbCb ] [ CrCr CrCr ]
    SVIDEO_PIXEL_FORMAT_YCbCr420, //packed: ????          ; planar: [ YYYY YYYY ] [ CbCb ] [ CrCr ]
    SVIDEO_PIXEL_FORMAT_YCbCr420_SEMIPLANAR, //semiplanar or biplanar: 2 planes instead of 3; [ YYYY YYYY ] [ CbCrCbCr ]
    SVIDEO_PIXEL_FORMAT_YCrCb420_SEMIPLANAR,
    SVIDEO_PIXEL_FORMAT_GRAYSCALE8,
    SVIDEO_PIXEL_FORMAT_COLOR, //native app pixel format (COLOR type)
};

enum 
{
    SVIDEO_FOCUS_MODE_AUTO = 0, //Basic automatic focus mode. In this mode, the lens does not move unless the autofocus trigger action is called. (trigger = set mode to AUTO)
    SVIDEO_FOCUS_MODE_CONTINUOUS, //In this mode, the AF algorithm modifies the lens position continually to attempt to provide a constantly-in-focus image stream.
    //SVIDEO_FOCUS_MODE_MANUAL,
    //SVIDEO_FOCUS_MODE_EDOF,
    //SVIDEO_FOCUS_MODE_INFINITY,
    //SVIDEO_FOCUS_MODE_MACRO,
};

#define SVIDEO_OPEN_FLAG_READ		( 1 << 0 )
#define SVIDEO_OPEN_FLAG_WRITE		( 1 << 1 )
#define SVIDEO_OPEN_FLAGOFF_ROTATE	2 //this value (two bits) will be added to the orientation

struct svideo_struct;
typedef void (*svideo_capture_callback_t)( svideo_struct* vid );

union svideo_prop_val
{
    int64_t i;
    double f;
    void* p;
};

struct svideo_prop //for svideo_set_props() and svideo_get_props()
{
    int id;
    svideo_prop_val val;
};

struct svideo_plan
{
    void* buf;
    int buf_size; //in bytes
    int pixel_stride; //the distance between adjacent pixel samples, in bytes
    int row_stride; //the distance between the start of two consecutive rows of pixels, in bytes
};

struct svideo_struct
{
    sundog_engine* sd; //Parent SunDog engine (may be null)
    char* name;
    uint flags; //SVIDEO_OPEN_FLAG_xxxx
    int frame_width;
    int frame_height;
    int pixel_format;
    int fps;
    int orientation;
    //Capture callback:
    svideo_capture_callback_t capture_callback;
    void* capture_user_data;
    svideo_plan capture_plans[ 3 ];
    int capture_plans_cnt;
    //Device specific:
#ifdef OS_ANDROID
    int cam_index;
    int con_index; //connection index
#endif
#ifdef OS_IOS
    void* ios_capture_obj;
#endif
#ifdef OS_LINUX
    const char* file_name;
    bool is_device;
    int dev_fd;
    int dev_io_method;
    sthread dev_thread;
    volatile bool dev_thread_exit_request;
    void* buffers;
    int buffers_num;
#endif
    smutex callback_mutex;
    bool callback_active;
    int fps_counter;
    stime_ms_t fps_time;
    int frame_counter;
};

int svideo_global_init();
int svideo_global_deinit();
int svideo_open( svideo_struct* vid, sundog_engine* sd, const char* name, uint flags, svideo_capture_callback_t capture_callback, void* capture_user_data );
int svideo_close( svideo_struct* vid );
int svideo_start( svideo_struct* vid );
int svideo_stop( svideo_struct* vid );
int svideo_set_props( svideo_struct* vid, svideo_prop* props ); //call this function outside of the start() ... stop() block only!
int svideo_get_props( svideo_struct* vid, svideo_prop* props );
int svideo_pixel_convert( svideo_plan* src_plans, int src_plans_cnt, int src_pixel_format, void* dest, int dest_pixel_format, int xsize, int ysize );
// To do:
// svideo_read_frame()
// svideo_write_frame()

int device_video_global_init();
int device_video_global_deinit();
int device_video_open( svideo_struct* vid, const char* name, uint flags );
int device_video_close( svideo_struct* vid );
int device_video_start( svideo_struct* vid );
int device_video_stop( svideo_struct* vid );
int device_video_set_props( svideo_struct* vid, svideo_prop* props );
int device_video_get_props( svideo_struct* vid, svideo_prop* props );
