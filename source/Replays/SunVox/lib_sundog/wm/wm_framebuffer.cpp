/*
    wm_framebuffer.cpp - drawing in the framebuffer
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#define LINE_PREC 19

void fb_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm )
{
    if( !wm->screen_is_active ) return;
    
#ifdef OS_WINCE
    if( wm->vd == VIDEODRIVER_GDI )
    {
	color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
    }
#endif

    int fb_xpitch = wm->fb_xpitch;
    int fb_ypitch = wm->fb_ypitch;
    COLORPTR fb = wm->fb;

    fb[ wm->fb_offset + y1 * fb_ypitch + x1 * fb_xpitch ] = color;
    fb[ wm->fb_offset + y2 * fb_ypitch + x2 * fb_xpitch ] = color;

    int len_x = x2 - x1; if( len_x < 0 ) len_x = -len_x;
    int len_y = y2 - y1; if( len_y < 0 ) len_y = -len_y;
    if( len_x ) len_x++;
    if( len_y ) len_y++;
    int xdir; if( x2 - x1 >= 0 ) xdir = fb_xpitch; else xdir = -fb_xpitch;
    int ydir; if( y2 - y1 >= 0 ) ydir = fb_ypitch; else ydir = -fb_ypitch;
    COLORPTR ptr = fb + wm->fb_offset + y1 * fb_ypitch + x1 * fb_xpitch;
    int delta;
    int v = 0, old_v = 0;
    if( len_x > len_y )
    {
	//Horisontal:
	if( len_x != 0 )
	    delta = ( len_y << LINE_PREC ) / len_x;
	else
	    delta = 0;
	for( int a = 0; a < len_x; a++ )
	{
	    *ptr = color;
	    old_v = v;
	    v += delta;
	    ptr += xdir;
	    if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
		ptr += ydir;
	}
    }
    else
    {
	//Vertical:
	if( len_y != 0 ) 
	    delta = ( len_x << LINE_PREC ) / len_y;
	else
	    delta = 0;
	for( int a = 0; a < len_y; a++ )
	{
	    *ptr = color;
	    old_v = v;
	    v += delta;
	    ptr += ydir;
	    if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
		ptr += xdir;
	}
    }
}

void fb_draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
    if( !wm->screen_is_active ) return;

#ifdef OS_WINCE
    if( wm->vd == VIDEODRIVER_GDI )
    {
	color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
    }
#endif

    int fb_xpitch = wm->fb_xpitch;
    int fb_ypitch = wm->fb_ypitch;
    COLORPTR fb = wm->fb;

    COLORPTR ptr = fb + wm->fb_offset + y * fb_ypitch + x * fb_xpitch;
    if( fb_xpitch == 1 )
    {
	int add = fb_ypitch - xsize;
	for( int cy = 0; cy < ysize; cy++ )
	{
	    COLORPTR size = ptr + xsize;
	    while( ptr < size ) *ptr++ = color;
	    ptr += add;
	}
    }
    else
    {
	int add = fb_ypitch - ( xsize * fb_xpitch );
	for( int cy = 0; cy < ysize; cy++ )
	{
	    for( int cx = 0; cx < xsize; cx++ )
	    {
		*ptr = color;
		ptr += fb_xpitch;
	    }
	    ptr += add;
	}
    }
}

void fb_draw_image( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm )
{
    if( !wm->screen_is_active ) return;

    COLORPTR fb = wm->fb;
    int fb_xpitch = wm->fb_xpitch;
    int fb_ypitch = wm->fb_ypitch;

    COLORPTR data = (COLORPTR)img->data; //Source
    COLORPTR ptr = fb + wm->fb_offset + dest_y * fb_ypitch + dest_x * fb_xpitch; //Destination

    //Add offset:
    int bp = src_y * img->xsize + src_x;
    data += bp;
    
    //Draw:
    int data_add = img->xsize - dest_xs;
#ifdef OS_WINCE
    if( wm->vd == VIDEODRIVER_GDI )
    {
	COLOR color;
	if( fb_xpitch == 1 )
	{
	    int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
	    for( int cy = 0; cy < dest_ys; cy++ )
	    {
		COLORPTR size = ptr + dest_xs;
		while( ptr < size ) 
		{
		    color = *data++;
		    color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
		    *ptr++ = color;
		}
		ptr += ptr_add;
		data += data_add;
	    }
	}
	else
	{
	    for( int cy = 0; cy < dest_ys; cy++ )
	    {
		int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
		int cx = dest_xs + 1;
		while( --cx )
		{
		    color = *data;
		    color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
		    *ptr = color;
		    ptr += fb_xpitch;
		    data++;
		}
		ptr += ptr_add;
		data += data_add;
	    }
	}
    }
    else
#endif
    {
    
    if( fb_xpitch == 1 )
    {
	int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
	for( int cy = 0; cy < dest_ys; cy++ )
	{
	    COLORPTR size = ptr + dest_xs;
	    while( ptr < size ) *ptr++ = *data++;
	    ptr += ptr_add;
	    data += data_add;
	}
    }
    else
    {
	for( int cy = 0; cy < dest_ys; cy++ )
	{
	    int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
	    int cx = dest_xs + 1;
	    while( --cx )
	    {
		*ptr = *data;
		ptr += fb_xpitch;
		data++;
	    }
	    ptr += ptr_add;
	    data += data_add;
	}
    }
    
    }
}
