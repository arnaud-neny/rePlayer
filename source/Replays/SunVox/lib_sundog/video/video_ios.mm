#include "sundog.h"

#ifndef NOVIDEO

#include "video.h"
#include "main/ios/sundog_bridge.h"

#import "sys/utsname.h"

#import <AVFoundation/AVFoundation.h>

@interface iOSAVCapture: NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    AVCaptureSession* session;
    AVCaptureDevice* device;
    int width;
    int height;
    svideo_struct* vid;
}
+ (iOSAVCapture*)newCapture: (const char*)name baseVideoObj: (svideo_struct*)vid;
- (int)openSession: (const char*) name;
- (void)closeSession;
- (void)startCapture;
- (void)stopCapture;
- (int)getWidth;
- (int)getHeight;
- (AVCaptureFocusMode)getFocusMode;
- (void)setFocusMode: (AVCaptureFocusMode) mode;
@end

@implementation iOSAVCapture

+ (iOSAVCapture*)newCapture: (const char*)name baseVideoObj: (svideo_struct*)vid;
{
    iOSAVCapture* cap = [ [ iOSAVCapture alloc ] init ];
    cap->vid = vid;
    if( [ cap openSession: name ] != 0 )
    {
	[ cap release ];
	cap = NULL;
    }
    return cap;
}

- (int)openSession: (const char*) name
{
    int rv = -1;
    while( 1 )
    {
	NSError* error = nil;

	// Create the session
	session = [ [ AVCaptureSession alloc ] init ];
	if( !session )
	{
	    slog( "AVCapture: session creation error\n" );
	    break;
	}

	// Find a suitable AVCaptureDevice
	int i = -1;
	bool device_hd_support = 0;
	if( smem_strcmp( name, "camera" ) == 0 )
	{
	    i = sconfig_get_int_value( APP_CFG_CAMERA, 0, 0 );
	}
	if( smem_strcmp( name, "camera_back" ) == 0 ) i = 0;
	if( smem_strcmp( name, "camera_front" ) == 0 ) i = 1;
	NSArray* devices = [ AVCaptureDevice devices ];
	AVCaptureDevice* back = NULL;
	AVCaptureDevice* front = NULL;
	for( AVCaptureDevice* d in devices )
	{
	    if( [ d hasMediaType:AVMediaTypeVideo ] )
	    {
		if( [ d position ] == AVCaptureDevicePositionBack && back == NULL ) back = d;
		if( [ d position ] == AVCaptureDevicePositionFront && front == NULL ) front = d;
	    }
	}
        if( i == 0 && back ) device = back;
	if( i == 1 && front ) device = front;
	if( device == NULL ) device = back;
	if( device == NULL ) device = front;
	if( i >= 2 )
	{
	    int i2 = 2;
	    for( AVCaptureDevice* d in devices )
	    {
		if( d == back || d == front ) continue;
		if( ![ d hasMediaType:AVMediaTypeVideo ] ) continue;
		if( i2 == i ) { device = d; break; }
		i2++;
	    }
	}
	if( device == NULL )
	{
	    slog( "AVCapture: can't find the device\n" );
	    break;
	}
	if( AVCaptureSessionPreset1280x720 != 0 && [ device supportsAVCaptureSessionPreset:AVCaptureSessionPreset1280x720 ] == YES )
	{
	    device_hd_support = 1;
	    slog( "HD mode is supported by selected device\n" );
	}

	//Get device info:
	struct utsname systemInfo;
        uname( &systemInfo );
    	slog( "Machine: %s\n", systemInfo.machine );

	// Create a device input with the device and add it to the session.
	AVCaptureDeviceInput* input = [ AVCaptureDeviceInput deviceInputWithDevice:device error:&error ];
	if( input == 0 )
	{
	    // Handling the error appropriately.
	    slog( "AVCapture: input device creation error\n" );
	    break;
	}
	[ session addInput:input ];

	// Create a VideoDataOutput and add it to the session
	AVCaptureVideoDataOutput* output = [ [ [ AVCaptureVideoDataOutput alloc ] init ] autorelease ];
	[ session addOutput:output ];

	if( device_hd_support && [ session canSetSessionPreset:AVCaptureSessionPreset1280x720 ] )
	{
	    session.sessionPreset = AVCaptureSessionPreset1280x720;
	    width = 1280;
	    height = 720;
	    slog( "AVCaptureSessionPreset1280x720\n" );
	}
	else
	{
	    session.sessionPreset = AVCaptureSessionPreset640x480;
	    width = 640;
	    height = 480;
	    slog( "AVCaptureSessionPreset640x480\n" );
	}

	// Configure the output
	dispatch_queue_t queue = dispatch_queue_create( "myQueue", NULL );
	[ output setSampleBufferDelegate:self queue:queue ];
	dispatch_release( queue );

	// Specify the pixel format
	output.videoSettings = [ NSDictionary dictionaryWithObject: [ NSNumber numberWithInt:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ] forKey:(id)kCVPixelBufferPixelFormatTypeKey ];

	rv = 0;
	break;
    }
    return rv;
}

- (void)closeSession
{
    [ session release ];
    session = 0;
}

- (void)startCapture
{
    [ session startRunning ];
}

- (void)stopCapture
{
    [ session stopRunning ];
}

- (int)getWidth
{
    return width;
}

- (int)getHeight
{
    return height;
}

- (AVCaptureFocusMode)getFocusMode
{
    return device.focusMode;
}

- (void)setFocusMode: (AVCaptureFocusMode) mode
{
    if( [ device isFocusModeSupported:mode ] )
    {
        NSError* err;
        if( [ device lockForConfiguration:&err ] )
        {
            device.focusMode = mode;
            [ device unlockForConfiguration ];
        }
        else
        {
            slog( "AVCaptureDevice lock error %d\n", (int)err.code );
        }
    }
}

- (void)dealloc
{
    [ self closeSession ];
    [ super dealloc ];
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer( sampleBuffer );
    CVPixelBufferLockBaseAddress( imageBuffer, 0 );
    CVPlanarPixelBufferInfo_YCbCrBiPlanar* info = (CVPlanarPixelBufferInfo_YCbCrBiPlanar*)CVPixelBufferGetBaseAddress( imageBuffer );
    void* buffer = ( (char*)CVPixelBufferGetBaseAddress( imageBuffer ) + INT32_SWAP( info->componentInfoY.offset ) );
    size_t buffer_size = CVPixelBufferGetDataSize( imageBuffer ) - INT32_SWAP( info->componentInfoY.offset );
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
	int connection_orientation = ( 0 + ( ( vid->flags >> SVIDEO_OPEN_FLAGOFF_ROTATE ) & 3 ) ) & 3;
	switch( [ connection videoOrientation ] )
	{
	    case AVCaptureVideoOrientationPortrait: connection_orientation = 0; break;
	    case AVCaptureVideoOrientationPortraitUpsideDown: connection_orientation = -2; break;
	    case AVCaptureVideoOrientationLandscapeRight: connection_orientation = -3; break;
	    case AVCaptureVideoOrientationLandscapeLeft: connection_orientation = -1; break;
	    default: break;
	}
	int ui_orientation = 0; if( vid->sd ) ui_orientation = vid->sd->screen_orientation;
	if( [ device position ] == AVCaptureDevicePositionFront )
	{
	    //Actually we should H.hlip the image... or not??
	    //Here is the temp hack:
	    if( ui_orientation == 0 )
	    {
		ui_orientation = -2;
	    }
	    else
	    {
		if( ui_orientation == -2 )
		    ui_orientation = 0;
	    }
	}
	vid->orientation = ( ui_orientation - connection_orientation + ( ( vid->flags >> SVIDEO_OPEN_FLAGOFF_ROTATE ) & 3 ) ) & 3;
	vid->capture_plans_cnt = 1;
	svideo_plan* plan = &vid->capture_plans[ 0 ];
	plan->buf = buffer;
	plan->buf_size = buffer_size;
	vid->capture_callback( vid );
    }
    smutex_unlock( &vid->callback_mutex );
    CVPixelBufferUnlockBaseAddress( imageBuffer, 0 );
}

@end

int device_video_global_init()
{
    return 0;
}

int device_video_global_deinit()
{
    return 0;
}

int device_video_open( svideo_struct* vid, const char* name, uint flags )
{
    int rv = -1;
    
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    
    while( 1 )
    {
	smutex_init( &vid->callback_mutex, 0 );
	vid->orientation = 0;
	
	iOSAVCapture* cap = [ iOSAVCapture newCapture: name baseVideoObj:vid ];
	vid->ios_capture_obj = (void*)cap;
	if( !cap ) break;
	[ cap startCapture ];
	slog( "AVCapture initialized\n" );
	
	rv = 0;
	break;
    }
    if( rv == - 1 )
    {
	smutex_destroy( &vid->callback_mutex );
    }
    
    [ pool release ];
    
    return rv;
}

int device_video_close( svideo_struct* vid )
{
    int rv = -1;
    
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    
    while( 1 )
    {
	iOSAVCapture* cap = (iOSAVCapture*)vid->ios_capture_obj;
	[ cap release ];
	vid->ios_capture_obj = NULL;
	
	smutex_destroy( &vid->callback_mutex );
	rv = 0;
	break;
    }
    
    [ pool release ];
    
    return rv;
}

int device_video_start( svideo_struct* vid )
{
    int rv = -1;
    while( 1 )
    {
	iOSAVCapture* cap = (iOSAVCapture*)vid->ios_capture_obj;
	if( cap == 0 ) break;
	smutex_lock( &vid->callback_mutex );
	vid->callback_active = 1;
	vid->fps_time = stime_ms();
	vid->fps_counter = 0;
	smutex_unlock( &vid->callback_mutex );
	//[ cap startCapture ];
	rv = 0;
	break;
    }
    return rv;
}

int device_video_stop( svideo_struct* vid )
{
    int rv = -1;
    while( 1 )
    {
	iOSAVCapture* cap = (iOSAVCapture*)vid->ios_capture_obj;
	if( cap == 0 ) break;
	smutex_lock( &vid->callback_mutex );
	vid->callback_active = 0;
	smutex_unlock( &vid->callback_mutex );
	//[ cap stopCapture ]; //HANGS :(
	rv = 0;
	break;
    }
    return rv;
}

int device_video_set_props( svideo_struct* vid, svideo_prop* props )
{
    int rv = -1;
    while( 1 )
    {
	iOSAVCapture* cap = (iOSAVCapture*)vid->ios_capture_obj;
	if( !cap ) break;
        if( !props ) break;
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
                            case SVIDEO_FOCUS_MODE_AUTO: mode = AVCaptureFocusModeAutoFocus; break;
                            case SVIDEO_FOCUS_MODE_CONTINUOUS: mode = AVCaptureFocusModeContinuousAutoFocus; break;
                        }
                        if( mode != -1 ) [ cap setFocusMode:(AVCaptureFocusMode)mode ];
                    }
                    break;
                default: break;
            }
        }
	rv = 0;
	break;
    }
    return rv;
}

int device_video_get_props( svideo_struct* vid, svideo_prop* props )
{
    int rv = -1;
    while( 1 )
    {
	iOSAVCapture* cap = (iOSAVCapture*)vid->ios_capture_obj;
	if( !cap ) break;
	if( !props ) break;
	bool props_handled = false;
	for( int i = 0; ; i++ )
	{
	    svideo_prop* prop = &props[ i ];
	    if( prop->id == SVIDEO_PROP_NONE ) break;
	    switch( prop->id )
	    {
		case SVIDEO_PROP_FRAME_WIDTH_I:
		    prop->val.i = [ cap getWidth ];
		    props_handled = true;
		    break;
		case SVIDEO_PROP_FRAME_HEIGHT_I:
		    prop->val.i = [ cap getHeight ];
		    props_handled = true;
		    break;
		case SVIDEO_PROP_PIXEL_FORMAT_I:
		    prop->val.i = SVIDEO_PIXEL_FORMAT_YCbCr420_SEMIPLANAR;
		    props_handled = true;
		    break;
		case SVIDEO_PROP_FPS_I:
		    prop->val.i = vid->fps;
		    props_handled = true;
		    break;
		case SVIDEO_PROP_FOCUS_MODE_I:
		    switch( [ cap getFocusMode ] )
                    {
                        case AVCaptureFocusModeContinuousAutoFocus: prop->val.i = SVIDEO_FOCUS_MODE_CONTINUOUS; break;
                        default: prop->val.i = SVIDEO_FOCUS_MODE_AUTO; break;
                    }
                    break;
		case SVIDEO_PROP_ORIENTATION_I: 
		    prop->val.i = vid->orientation; 
		    props_handled = true;
		    break;
	    }
	}
	if( props_handled == false ) break;
	rv = 0;
	break;
    }
    return rv;
}

#endif
