/*
    video.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2014 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "video.h"

#ifndef NOVIDEO

#ifdef OS_ANDROID
    #include "video_android.hpp"
#else
    #ifdef OS_LINUX
	#include "video_linux.hpp"
    #endif
#endif

#ifdef OS_IOS
    #include "video_ios.hpp"
#endif

#endif //!NOVIDEO

int svideo_global_init()
{
    int rv = -1;
#ifndef NOVIDEO
#ifdef DEVICE_VIDEO_FUNCTIONS
    rv = device_video_global_init();
#endif
#endif //!NOVIDEO
    return rv;
}

int svideo_global_deinit()
{
    int rv = -1;
#ifndef NOVIDEO
    #ifdef DEVICE_VIDEO_FUNCTIONS
	rv = device_video_global_deinit();
    #endif
#endif //!NOVIDEO
    return rv;
}

int svideo_open( svideo_struct* vid, sundog_engine* sd, const char* name, uint flags, svideo_capture_callback_t capture_callback, void* capture_user_data )
{
    int rv = -1;
#ifndef NOVIDEO
    while( 1 )
    {
	if( vid == 0 ) break;
	smem_clear( vid, sizeof( svideo_struct ) );
	vid->sd = sd;
	vid->name = (char*)smem_strdup( name );
	vid->flags = flags;
	vid->capture_callback = capture_callback;
	vid->capture_user_data = capture_user_data;
	if( smem_strstr( name, "camera" ) )
	{
	    int rotate = sconfig_get_int_value( APP_CFG_CAM_ROTATE, 0, 0 ) / 90;
	    if( rotate )
	    {
	        rotate = ( rotate + ( ( vid->flags >> SVIDEO_OPEN_FLAGOFF_ROTATE ) & 3 ) ) & 3;
	        vid->flags &= ~( 3 << SVIDEO_OPEN_FLAGOFF_ROTATE );
	        vid->flags |= rotate << SVIDEO_OPEN_FLAGOFF_ROTATE;
	    }
	}
#ifdef DEVICE_VIDEO_FUNCTIONS
	rv = device_video_open( vid, name, flags );
#endif
	break;
    }
    if( rv != 0 )
    {
	slog( "svideo_open() error %d\n", rv );
	smem_free( vid->name );
    }
#endif //!NOVIDEO
    return rv;
}

int svideo_close( svideo_struct* vid )
{
    int rv = -1;
#ifndef NOVIDEO
    while( 1 )
    {
	if( vid == 0 ) break;
#ifdef DEVICE_VIDEO_FUNCTIONS
	rv = device_video_close( vid );
#endif
	smem_free( vid->name );
	break;
    }
    if( rv != 0 )
    {
	slog( "svideo_close() error %d\n", rv );
    }
#endif //!NOVIDEO
    return rv;
}

int svideo_start( svideo_struct* vid )
{
    int rv = -1;
#ifndef NOVIDEO
    while( 1 )
    {
	if( vid == 0 ) break;
#ifdef DEVICE_VIDEO_FUNCTIONS
	rv = device_video_start( vid );
#endif
	svideo_prop props[ 5 ];
	smem_clear( props, sizeof( props ) );
	props[ 0 ].id = SVIDEO_PROP_FRAME_WIDTH_I;
	props[ 1 ].id = SVIDEO_PROP_FRAME_HEIGHT_I;
	props[ 2 ].id = SVIDEO_PROP_PIXEL_FORMAT_I;
	props[ 3 ].id = SVIDEO_PROP_FOCUS_MODE_I;
	if( svideo_get_props( vid, props ) == 0 )
	{
	    vid->frame_width = (int)props[ 0 ].val.i;
	    vid->frame_height = (int)props[ 1 ].val.i;
	    vid->pixel_format = (int)props[ 2 ].val.i;
	    slog( "svideo_start(): %dx%d; pixel format:%d; focus mode:%d\n", vid->frame_width, vid->frame_height, vid->pixel_format, (int)props[ 3 ].val.i );
	}
	break;
    }
    if( rv != 0 )
    {
	slog( "svideo_start() error %d\n", rv );
    }
#endif //!NOVIDEO
    return rv;
}

int svideo_stop( svideo_struct* vid )
{
    int rv = -1;
#ifndef NOVIDEO
    while( 1 )
    {
	if( vid == 0 ) break;
#ifdef DEVICE_VIDEO_FUNCTIONS
	rv = device_video_stop( vid );
#endif
	break;
    }
    if( rv != 0 )
    {
	slog( "svideo_stop() error %d\n", rv );
    }
#endif //!NOVIDEO
    return rv;
}

int svideo_set_props( svideo_struct* vid, svideo_prop* props )
{
    int rv = -1;
#ifndef NOVIDEO
    while( 1 )
    {
	if( vid == 0 ) break;
	if( props == 0 ) break;
#ifdef DEVICE_VIDEO_FUNCTIONS
	rv = device_video_set_props( vid, props );
#endif
	break;
    }
    if( rv != 0 )
    {
	slog( "svideo_set_props() error %d\n", rv );
    }
#endif //!NOVIDEO
    return rv;
}

int svideo_get_props( svideo_struct* vid, svideo_prop* props )
{
    int rv = -1;
#ifndef NOVIDEO
    while( 1 )
    {
	if( vid == 0 ) break;
	if( props == 0 ) break;
#ifdef DEVICE_VIDEO_FUNCTIONS
	rv = device_video_get_props( vid, props );
#endif
	break;
    }
    if( rv != 0 )
    {
	slog( "svideo_get_props() error %d\n", rv );
    }
#endif //!NOVIDEO
    return rv;
}

#ifndef SUNDOG_VER
inline COLOR YCbCr_to_COLOR( int Y, int Cb, int Cr ) 
{
    int r, g, b;
    b = Y + ( ( (int)( 1.402f * 256.0f ) * Cb ) >> 8 );
    g = Y - ( ( (int)( 0.344f * 256.0f ) * Cr + (int)( 0.714f * 256.0f ) * Cb ) >> 8 );
    r = Y + ( ( (int)( 1.772f * 256.0f ) * Cr ) >> 8 );
    r = r > 255 ? 255 : r < 0 ? 0 : r;
    g = g > 255 ? 255 : g < 0 ? 0 : g;
    b = b > 255 ? 255 : b < 0 ? 0 : b;
    return get_color( r, g, b );
}
#endif

int svideo_pixel_convert( svideo_plan* src_plans, int src_plans_cnt, int src_pixel_format, void* dest, int dest_pixel_format, int xsize, int ysize )
{
    int rv = -1;
#ifndef NOVIDEO
    while( 1 )
    {
	if( !src_plans ) break;
	if( !dest ) break;

        uint8_t* srcY = (uint8_t*)src_plans[ 0 ].buf;

	if( src_plans_cnt == 3 )
	{
	    uint8_t* srcCb = (uint8_t*)src_plans[ 1 ].buf;
	    uint8_t* srcCr = (uint8_t*)src_plans[ 2 ].buf;
	    int srcY_ps = src_plans[ 0 ].pixel_stride;
    	    int srcY_rs = src_plans[ 0 ].row_stride;
	    int srcC_ps = src_plans[ 1 ].pixel_stride;
	    int srcC_rs = src_plans[ 1 ].row_stride;
	    switch( dest_pixel_format )
	    {
		case SVIDEO_PIXEL_FORMAT_GRAYSCALE8:
		    if( srcY_ps == 1 && srcY_rs == xsize )
			smem_copy( dest, srcY, xsize * ysize );
		    else
		    {
			uint8_t* dest2 = (uint8_t*)dest;
			for( int y = 0; y < ysize; y++ )
			{
			    for( int x = 0; x < xsize; x++ )
			    {
				*dest2 = *srcY;
				dest2++;
				srcY += srcY_ps;
			    }
			    srcY += srcY_rs - xsize;
			}
		    }
		    rv = 0;
		    break;
#ifndef SUNDOG_VER
		case SVIDEO_PIXEL_FORMAT_COLOR:
		    {
			COLORPTR dest2 = (COLORPTR)dest;
			switch( src_pixel_format )
			{
			    case SVIDEO_PIXEL_FORMAT_YCbCr420:
				for( int y = 0; y < ysize; y += 2 )
				{
				    for( int x = 0; x < xsize; x += 2 )
				    {
					int Cb = *srcCb;
					int Cr = *srcCr;
					Cb -= 128;
					Cr -= 128;

					int Y = srcY[ 0 ];
					dest2[ 0 ] = YCbCr_to_COLOR( Y, Cb, Cr );
					Y = srcY[ srcY_ps ];
					dest2[ 1 ] = YCbCr_to_COLOR( Y, Cb, Cr );
					Y = srcY[ srcY_rs ];
					dest2[ xsize ] = YCbCr_to_COLOR( Y, Cb, Cr );
					Y = srcY[ srcY_rs + srcY_ps ];
					dest2[ xsize + 1 ] = YCbCr_to_COLOR( Y, Cb, Cr );

					srcCb += srcC_ps;
					srcCr += srcC_ps;
					srcY += srcY_ps * 2;
					dest2 += 2;
				    }
				    srcY += ( srcY_rs - xsize ) + srcY_rs;
				    srcCb += srcC_rs - xsize;
				    srcCr += srcC_rs - xsize;
				    dest2 += xsize;
				}
				rv = 0;
				break;
			}
		    }
		    break;
#endif
	    }
	    break;
	}

	switch( src_pixel_format )
	{
	    case SVIDEO_PIXEL_FORMAT_YCbCr422:
		switch( dest_pixel_format )
		{
		    case SVIDEO_PIXEL_FORMAT_GRAYSCALE8:
			{
			    uint8_t* src2 = (uint8_t*)srcY;
			    uint8_t* dest2 = (uint8_t*)dest;
			    for( int i = 0; i < xsize * ysize; i++ )
			    {
				*dest2 = *src2;
				dest2++;
				src2 += 2;
			    }
			}
			rv = 0;
			break;
#ifndef SUNDOG_VER
		    case SVIDEO_PIXEL_FORMAT_COLOR:
			{
			    uint8_t* src2 = (uint8_t*)srcY;
			    COLORPTR dest2 = (COLORPTR)dest;
			    for( int i = 0; i < xsize * ysize; i += 2 )
			    {
				int Y1 = src2[ 0 ];
				int Y2 = src2[ 2 ];
				int Cb = src2[ 1 ];
				int Cr = src2[ 3 ];
				Cb -= 128;
				Cr -= 128;
				*dest2 = YCbCr_to_COLOR( Y1, Cb, Cr ); dest2++;
				*dest2 = YCbCr_to_COLOR( Y2, Cb, Cr ); dest2++;
				src2 += 4;
			    }
			}
			rv = 0;
			break;
#endif
		    default: break;
		}
		break;
	    case SVIDEO_PIXEL_FORMAT_YCbCr420_SEMIPLANAR:
	    case SVIDEO_PIXEL_FORMAT_YCrCb420_SEMIPLANAR:
		switch( dest_pixel_format )
		{
		    case SVIDEO_PIXEL_FORMAT_GRAYSCALE8:
			smem_copy( dest, srcY, xsize * ysize );
			rv = 0;
			break;
		    default: break;
		}
		break;
	    default: break;
	}
	if( rv != 0 ) switch( src_pixel_format )
	{
	    case SVIDEO_PIXEL_FORMAT_YCbCr420_SEMIPLANAR:
		switch( dest_pixel_format )
		{
#ifndef SUNDOG_VER
		    case SVIDEO_PIXEL_FORMAT_COLOR:
			{
			    uint8_t* src2 = (uint8_t*)srcY; //Y
			    uint8_t* src3 = src2 + xsize * ysize; //Cb and Cr
			    COLOR* dest2 = (COLOR*)dest;
			    for( int y = 0; y < ysize / 2; y++ )
			    {
				for( int x = 0; x < xsize / 2; x++ )
				{
				    int Y1 = src2[ 0 ];
				    int Y2 = src2[ 1 ];
				    int Y3 = src2[ xsize ];
				    int Y4 = src2[ xsize + 1 ];
				    int Cb = src3[ 0 ];
				    int Cr = src3[ 1 ];
				    Cb -= 128;
				    Cr -= 128;
				    dest2[ 0 ] = YCbCr_to_COLOR( Y1, Cb, Cr );
				    dest2[ 1 ] = YCbCr_to_COLOR( Y2, Cb, Cr );
				    dest2[ xsize ] = YCbCr_to_COLOR( Y3, Cb, Cr );
				    dest2[ xsize + 1 ] = YCbCr_to_COLOR( Y4, Cb, Cr );
				    src2 += 2;
				    src3 += 2;
				    dest2 += 2;
				}
				src2 += xsize;
				dest2 += xsize;
			    }
			}
			rv = 0;
			break;
#endif
		    default: break;
		}
		break;
	    case SVIDEO_PIXEL_FORMAT_YCrCb420_SEMIPLANAR:
		switch( dest_pixel_format )
		{
#ifndef SUNDOG_VER
		    case SVIDEO_PIXEL_FORMAT_COLOR:
			{
			    uint8_t* src2 = (uint8_t*)srcY; //Y
			    uint8_t* src3 = src2 + xsize * ysize; //Cb and Cr
			    COLOR* dest2 = (COLOR*)dest;
			    for( int y = 0; y < ysize / 2; y++ )
			    {
				for( int x = 0; x < xsize / 2; x++ )
				{
				    int Y1 = src2[ 0 ];
				    int Y2 = src2[ 1 ];
				    int Y3 = src2[ xsize ];
				    int Y4 = src2[ xsize + 1 ];
				    int Cr = src3[ 0 ];
				    int Cb = src3[ 1 ];
				    Cb -= 128;
				    Cr -= 128;
				    dest2[ 0 ] = YCbCr_to_COLOR( Y1, Cb, Cr );
				    dest2[ 1 ] = YCbCr_to_COLOR( Y2, Cb, Cr );
				    dest2[ xsize ] = YCbCr_to_COLOR( Y3, Cb, Cr );
				    dest2[ xsize + 1 ] = YCbCr_to_COLOR( Y4, Cb, Cr );
				    src2 += 2;
				    src3 += 2;
				    dest2 += 2;
				}
				src2 += xsize;
				dest2 += xsize;
			    }
			}
			rv = 0;
			break;
#endif
		    default: break;
		}
		break;
	    default: break;
	}
	break;
    }
#endif
    return rv;
}
