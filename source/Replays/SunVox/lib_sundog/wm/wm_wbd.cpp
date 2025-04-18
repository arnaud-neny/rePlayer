/*
    wm_wbd.cpp - WBD (Window Buffer Draw)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

//How to use it in a window handler:
//EVT_DRAW:
//  wbd_lock( win );
//  ...
//  wbd_draw( wm );
//  wbd_unlock( wm );

#include "sundog.h"

#define LINE_PREC 19

#ifndef OPENGL
    #define WBD_OPENGL false
#else
    #define WBD_OPENGL !wm->fb
#endif

#define utf32_to_sundog_font_char( dest, src ) \
{ \
    while( 1 ) \
    { \
        if( src > 0x7F ) \
        { \
            if( ( src >> 8 ) == 0x04 ) \
            { \
                /* Russian: */ \
                dest = ( src & 127 ) + 128; \
                uint32_t src_t = src & 255; \
                break; \
            } \
        } \
        else \
        { \
            if( src != 0x0A ) { dest = src; break; } \
        } \
        dest = '*'; \
        break; \
    } \
}

void wbd_lock( WINDOWPTR win )
{
    if( win )
    {
	window_manager* wm = win->wm;
	if( WBD_OPENGL )
	{
	}
	else
	{
	    if( !wm->screen_pixels )
	    {
		wm->screen_pixels = (COLORPTR)smem_new( wm->screen_xsize * wm->screen_ysize * COLORLEN );
	    }
	    else
	    {
    		size_t real_size = smem_get_size( wm->screen_pixels ) / COLORLEN;
    		size_t new_size = wm->screen_xsize * wm->screen_ysize;
    		if( new_size > real_size )
    		{
    		    //Resize:
    		    wm->screen_pixels = (COLORPTR)smem_resize( wm->screen_pixels, ( new_size + ( new_size / 4 ) ) * COLORLEN );
		}
	    }
	}
	wm->cur_window = win;
	wm->cur_window_invisible = 0;
	if( win->xsize == 0 ||
	    win->ysize == 0 ||
	    win->visible == 0 ||
	    win->reg == 0 )
	{
	    wm->cur_window_invisible = 1;
	}
	if( win->reg && win->reg->numRects == 0 )
	{
	    wm->cur_window_invisible = 1;
	}
	wm->cur_font_color = COLORMASK;
	wm->cur_opacity = 255;
	wm->cur_font_scale = 256;
    }
}

void wbd_unlock( window_manager* wm )
{
    wm->cur_opacity = 255;
    wm->cur_window = 0;
    wm->cur_window_invisible = 1;
}

void wbd_draw( window_manager* wm )
{
    if( wm->cur_window_invisible ) return;
    if( WBD_OPENGL )
    {
    }
    else
    {
	WINDOWPTR win = wm->cur_window;
	sdwm_image img;
	img.xsize = wm->screen_xsize;
	img.ysize = wm->screen_ysize;
	img.flags = IMAGE_NATIVE_RGB | IMAGE_STATIC_SOURCE;
	img.data = wm->screen_pixels;
	win_draw_image_ext( win, 0, 0, win->xsize, win->ysize, win->screen_x, win->screen_y, &img, wm );
    }
}

#define GET_CLIPPING_RECT \
    MWRECT* extents = &win->reg->extents; \
    int clip_x1 = extents->left; \
    int clip_y1 = extents->top; \
    int clip_x2 = extents->right; \
    int clip_y2 = extents->bottom; \
    if( clip_x1 < 0 ) clip_x1 = 0; \
    if( clip_y1 < 0 ) clip_y1 = 0; \
    if( clip_x1 >= wm->screen_xsize ) return; \
    if( clip_y1 >= wm->screen_ysize ) return; \
    if( clip_x2 > wm->screen_xsize ) clip_x2 = wm->screen_xsize; \
    if( clip_y2 > wm->screen_ysize ) clip_y2 = wm->screen_ysize;

void draw_points( int16_t* coord2d, COLOR color, uint count, window_manager* wm )
{
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;
    if( coord2d == 0 || count == 0 ) return;

    WINDOWPTR win = wm->cur_window;
    
    if( WBD_OPENGL )
    {
#ifdef OPENGL
	uint bytes;
	if( wm->gl_no_points )
	    bytes = 6 * count * sizeof( int16_t );
	else
	    bytes = 2 * count * sizeof( int16_t );
	if( smem_get_size( wm->gl_points_array ) < bytes )
	{
	    if( wm->gl_points_array )
		wm->gl_points_array = smem_resize( wm->gl_points_array, bytes );
	    else
		wm->gl_points_array = smem_new( bytes );
	}
	if( wm->gl_points_array == 0 ) return;
	int16_t* new_points = (int16_t*)wm->gl_points_array;
	uint new_count = 0;
	for( uint i = 0; i < count; i++ )
	{
	    int x = (int)coord2d[ i * 2 + 0 ] + win->screen_x;
	    int y = (int)coord2d[ i * 2 + 1 ] + win->screen_y;
	    bool draw = false;
	    for( int r = 0; r < win->reg->numRects; r++ )
	    {
		int rx1 = win->reg->rects[ r ].left;
		int rx2 = win->reg->rects[ r ].right;
		int ry1 = win->reg->rects[ r ].top;
		int ry2 = win->reg->rects[ r ].bottom;
		if( x >= rx1 && y >= ry1 && x < rx2 && y < ry2 ) { draw = true; break; }
	    }
	    if( draw )
	    {
		if( wm->gl_no_points )
		{
		    new_points[ new_count * 6 + 0 ] = x;
		    new_points[ new_count * 6 + 1 ] = y;
		    new_points[ new_count * 6 + 2 ] = x + 1;
		    new_points[ new_count * 6 + 3 ] = y;
		    new_points[ new_count * 6 + 4 ] = x;
		    new_points[ new_count * 6 + 5 ] = y + 2;
		}
		else
		{
		    new_points[ new_count * 2 + 0 ] = x;
		    new_points[ new_count * 2 + 1 ] = y;
		}
		new_count++;
	    }
	}
	if( new_count )
	{
	    //Draw it:
    	    if( wm->gl_no_points )
	        gl_draw_triangles( new_points, color, new_count, wm );
	    else
		gl_draw_points( new_points, color, new_count, wm );
	    wm->screen_changed++;
	}
#endif
    }
    else
    {
	GET_CLIPPING_RECT;
	uint8_t opacity = wm->cur_opacity;
	int screen_changed = 0;
	for( uint i = 0; i < count; i++ )
	{
	    int x = (int)coord2d[ i * 2 + 0 ] + win->screen_x;
	    int y = (int)coord2d[ i * 2 + 1 ] + win->screen_y;
	    if( x < clip_x1 ) continue;
	    if( y < clip_y1 ) continue;
	    if( x >= clip_x2 ) continue;
	    if( y >= clip_y2 ) continue;
	    COLORPTR ptr = wm->screen_pixels + y * wm->screen_xsize + x;
	    *ptr = blend( *ptr, color, opacity );
	    screen_changed = 1;
	}
	wm->screen_changed += screen_changed;
    }
}

void draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm )
{
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    WINDOWPTR win = wm->cur_window;

    x1 += win->screen_x;
    y1 += win->screen_y;
    x2 += win->screen_x;
    y2 += win->screen_y;
    
    if( WBD_OPENGL )
    {
#ifdef OPENGL
	for( int r = 0; r < win->reg->numRects; r++ )
	{
    	    int rx1 = win->reg->rects[ r ].left;
    	    int rx2 = win->reg->rects[ r ].right;
    	    int ry1 = win->reg->rects[ r ].top;
    	    int ry2 = win->reg->rects[ r ].bottom;
    
    	    int lx1 = x1;
    	    int ly1 = y1;
    	    int lx2 = x2;
    	    int ly2 = y2;

    	    if( line_clip( &lx1, &ly1, &lx2, &ly2, rx1, ry1, rx2, ry2 ) )
    	    {
	        gl_draw_line( lx1, ly1, lx2, ly2, color, wm );
		wm->screen_changed++;
	    }
	}
#endif
    }
    else
    {
	int screen_xsize = wm->screen_xsize;
	int screen_ysize = wm->screen_ysize;
	COLORPTR screen_pixels = wm->screen_pixels;
	GET_CLIPPING_RECT;
    
	if( x1 == x2 )
	{
	    if( x1 >= clip_x1 && x1 < clip_x2 )
	    {
		wm->screen_changed++;
	    
		if( y1 > y2 ) { int temp = y1; y1 = y2; y2 = temp; }
		if( y1 < clip_y1 ) y1 = clip_y1;
		if( y1 >= clip_y2 ) return;
		if( y2 < clip_y1 ) return;
		if( y2 >= clip_y2 ) y2 = clip_y2 - 1;
		int ptr = y1 * screen_xsize + x1;
		uint8_t opacity = wm->cur_opacity;
		if( opacity >= 255 )
		{
		    for( ; y1 <= y2; y1++ )
		    {
			screen_pixels[ ptr ] = color;
			ptr += screen_xsize;
		    }
		}
		else
		{
		    for( ; y1 <= y2; y1++ )
		    {
			screen_pixels[ ptr ] = blend( screen_pixels[ ptr ], color, opacity );
			ptr += screen_xsize;
		    }
		}
	    }
	    return;
	}
	if( y1 == y2 )
	{
	    if( y1 >= 0 && y1 < screen_ysize )
	    {
		wm->screen_changed++;
	    
	        if( x1 > x2 ) { int temp = x1; x1 = x2; x2 = temp; }
		if( x1 < clip_x1 ) x1 = clip_x1;
		if( x1 >= clip_x2 ) return;
		if( x2 < clip_x1 ) return;
		if( x2 >= clip_x2 ) x2 = clip_x2 - 1;
		int ptr = y1 * screen_xsize + x1;
		uint8_t opacity = wm->cur_opacity;
		if( opacity == 255 )
		{
		    for( ; x1 <= x2; x1++ )
		    {
			screen_pixels[ ptr ] = color;
			ptr ++;
		    }
		}
		else
		{
		    for( ; x1 <= x2; x1++ )
		    {
			screen_pixels[ ptr ] = blend( screen_pixels[ ptr ], color, opacity );
			ptr ++;
		    }
		}
	    }
	    return;
	}

        if( line_clip( &x1, &y1, &x2, &y2, clip_x1, clip_y1, clip_x2, clip_y2 ) )
	{
	    //Draw line:
	    wm->screen_changed++;
	    bool x_inc;
	    bool y_inc;
	    int len_x = x2 - x1; if( len_x < 0 ) { len_x = -len_x; x_inc = 0; } else { x_inc = 1; }
	    int len_y = y2 - y1; if( len_y < 0 ) { len_y = -len_y; y_inc = 0; } else { y_inc = 1; }
	    int ptr = y1 * screen_xsize + x1;
	    int delta;
	    int v = 0, old_v = 0;
	    uint8_t opacity = wm->cur_opacity;
	    if( len_x > len_y )
	    {
		//Horizontal:
		if( len_x != 0 )
		{
		    delta = ( len_y << LINE_PREC ) / len_x;
		    delta += ( ( 1 << LINE_PREC ) / 2 ) / len_x;
		}
		else
		    delta = 0;
		if( opacity == 255 )
		    for( int a = 0; a <= len_x; a++ )
		    {
			screen_pixels[ ptr ] = color;
			old_v = v;
			v += delta;
			if( x_inc ) ptr++; else ptr--;
			if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
			{
			    if( y_inc )
				ptr += screen_xsize;
			    else
				ptr -= screen_xsize;
			}
		    }
		else
		    for( int a = 0; a <= len_x; a++ )
		    {
			screen_pixels[ ptr ] = blend( screen_pixels[ ptr ], color, opacity );
			old_v = v;
			v += delta;
			if( x_inc ) ptr++; else ptr--;
			if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
			{
			    if( y_inc )
				ptr += screen_xsize;
			    else
				ptr -= screen_xsize;
			}
		    }
	    }
	    else
	    {
		//Vertical:
		if( len_y != 0 )
		{
		    delta = ( len_x << LINE_PREC ) / len_y;
		    delta += ( ( 1 << LINE_PREC ) / 2 ) / len_y;
		}
		else
		    delta = 0;
		if( opacity == 255 )
		    for( int a = 0; a <= len_y; a++ )
		    {
			screen_pixels[ ptr ] = color;
			old_v = v;
			v += delta;
			if( y_inc ) 
			    ptr += screen_xsize;
			else
			    ptr -= screen_xsize;
			if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
		        {
			    if( x_inc ) ptr++; else ptr--;
			}
		    }
		else
		    for( int a = 0; a <= len_y; a++ )
		    {
			screen_pixels[ ptr ] = blend( screen_pixels[ ptr ], color, opacity );
			old_v = v;
			v += delta;
			if( y_inc ) 
			    ptr += screen_xsize;
			else
			    ptr -= screen_xsize;
			if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
			{
			    if( x_inc ) ptr++; else ptr--;
			}
		    }
	    }
	}
    }
}

void draw_rect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
    if( wm->cur_window_invisible ) return;
    draw_line( x, y, x + xsize, y, color, wm );
    draw_line( x + xsize, y, x + xsize, y + ysize, color, wm );
    draw_line( x + xsize, y + ysize, x, y + ysize, color, wm );
    draw_line( x, y + ysize, x, y, color, wm );
}

void draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    WINDOWPTR win = wm->cur_window;
    
    x += win->screen_x;
    y += win->screen_y;

    if( WBD_OPENGL )
    {
#ifdef OPENGL
	for( int r = 0; r < win->reg->numRects; r++ )
	{
	    int rx1 = win->reg->rects[ r ].left;
	    int rx2 = win->reg->rects[ r ].right;
	    int ry1 = win->reg->rects[ r ].top;
	    int ry2 = win->reg->rects[ r ].bottom;

	    //Control box size:
	    int nx = x;
	    int ny = y;
	    int nxsize = xsize;
	    int nysize = ysize;
	    if( nx < rx1 ) { nxsize -= ( rx1 - nx ); nx = rx1; }
	    if( ny < ry1 ) { nysize -= ( ry1 - ny ); ny = ry1; }
	    if( nx + nxsize <= rx1 ) continue;
	    if( ny + nysize <= ry1 ) continue;
	    if( nx + nxsize > rx2 ) nxsize -= nx + nxsize - rx2;
	    if( ny + nysize > ry2 ) nysize -= ny + nysize - ry2;
	    if( nx >= rx2 ) continue;
	    if( ny >= ry2 ) continue;
	    if( nxsize < 0 ) continue;
	    if( nysize < 0 ) continue;

	    //Draw it:
	    gl_draw_frect( nx, ny, nxsize, nysize, color, wm );
	    wm->screen_changed++;
	}
#endif
    }
    else
    {
	GET_CLIPPING_RECT;
	if( x < clip_x1 ) { xsize += x - clip_x1; x = clip_x1; }
	if( y < clip_y1 ) { ysize += y - clip_y1; y = clip_y1; }
	if( x + xsize > clip_x2 ) xsize = clip_x2 - x;
	if( y + ysize > clip_y2 ) ysize = clip_y2 - y;
	if( xsize <= 0 ) return;
	if( ysize <= 0 ) return;

	wm->screen_changed++;

	COLORPTR ptr = wm->screen_pixels + y * wm->screen_xsize + x;
	uint8_t opacity = wm->cur_opacity;
	int add = wm->screen_xsize - xsize;
	if( opacity == 255 )
	{
	    for( int cy = 0; cy < ysize; cy++ )
	    {
		COLORPTR size = ptr + xsize;
		while( ptr < size ) { *ptr = color; ptr++; }
		ptr += add;
	    }
	}
	else
	{
	    for( int cy = 0; cy < ysize; cy++ )
	    {
		COLORPTR size = ptr + xsize;
		while( ptr < size ) { *ptr = blend( *ptr, color, opacity ); ptr++; }
		ptr += add;
	    }
	}
    }
}

void draw_hgradient( int x, int y, int xsize, int ysize, COLOR c, int t1, int t2, window_manager* wm )
{
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    sdwm_polygon p;
    sdwm_vertex v[ 4 ];
    v[ 0 ].x = x;
    v[ 0 ].y = y;
    v[ 0 ].c = c;
    v[ 0 ].t = t1;
    v[ 1 ].x = x + xsize;
    v[ 1 ].y = y;
    v[ 1 ].t = t2;
    v[ 2 ].x = x + xsize;
    v[ 2 ].y = y + ysize;
    v[ 2 ].t = t2;
    v[ 3 ].x = x;
    v[ 3 ].y = y + ysize;
    v[ 3 ].t = t1;
    p.vnum = 4;
    p.v = v;
    wm->cur_flags = WBD_FLAG_ONE_COLOR;
    draw_polygon( &p, wm );
    wm->cur_flags = 0;
}

void draw_vgradient( int x, int y, int xsize, int ysize, COLOR c, int t1, int t2, window_manager* wm )
{
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    if( WBD_OPENGL )
    {
#ifdef OPENGL
	sdwm_polygon p;
	sdwm_vertex v[ 4 ];
	v[ 0 ].x = x;
	v[ 0 ].y = y;
	v[ 0 ].c = c;
	v[ 0 ].t = t1;
	v[ 1 ].x = x + xsize;
	v[ 1 ].y = y;
	v[ 1 ].t = t1;
	v[ 2 ].x = x + xsize;
	v[ 2 ].y = y + ysize;
	v[ 2 ].t = t2;
	v[ 3 ].x = x;
	v[ 3 ].y = y + ysize;
	v[ 3 ].t = t2;
	p.vnum = 4;
	p.v = v;
	wm->cur_flags = WBD_FLAG_ONE_COLOR;
	draw_polygon( &p, wm );
#endif
    }
    else
    {
	WINDOWPTR win = wm->cur_window;
    
	x += win->screen_x;
	y += win->screen_y;

	GET_CLIPPING_RECT;	
	if( x < clip_x1 ) { xsize += x - clip_x1; x = clip_x1; }
	if( y < clip_y1 ) 
	{
	    int yy = y - clip_y1;
	    if( ysize ) 
	    { 
		t1 += ( -yy * ( t2 - t1 ) ) / ysize; 
	    }
	    ysize += yy; y = clip_y1;
	}
	if( x + xsize > clip_x2 ) xsize = clip_x2 - x;
	if( y + ysize > clip_y2 ) 
	{
	    int yy = clip_y2 - y;
	    if( ysize )
	    {
		t2 = t1 + ( yy * ( t2 - t1 ) ) / ysize;
	    }
	    ysize = yy;
	}
	if( xsize <= 0 ) return;
	if( ysize <= 0 ) return;

	wm->screen_changed++;
    
	COLORPTR ptr = wm->screen_pixels + y * wm->screen_xsize + x;
	uint8_t opacity = wm->cur_opacity;
	if( opacity < 255 )
	{
	    t1 *= opacity; t1 /= 256;
	    t2 *= opacity; t2 /= 256;
	}
	int add = wm->screen_xsize - xsize;
	if( t1 >= 255 && t2 >= 255 )
	{
	    for( int cy = 0; cy < ysize; cy++ )
	    {
		COLORPTR size = ptr + xsize;
		while( ptr < size ) { *ptr = c; ptr++; }
		ptr += add;
	    }
	}
	else
	{
	    int tdelta;
	    if( ysize )
		tdelta = ( ( t2 - t1 ) << 15 ) / ysize;
	    else 
		tdelta = 0;
	    int t = t1 << 15;
	    for( int cy = 0; cy < ysize; cy++ )
	    {
		COLORPTR size = ptr + xsize;
		int tt = t >> 15;
		while( ptr < size ) { *ptr = blend( *ptr, c, tt ); ptr++; }
		t += tdelta;
		ptr += add;
	    }
	}
    }
}

void draw_corners( int x, int y, int xsize, int ysize, int size, int len, COLOR c, window_manager* wm )
{
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    draw_frect( x - size, y, size, len, c, wm );
    draw_frect( x - size, y - size, size + len, size, c, wm );
    
    draw_frect( x + xsize, y, size, len, c, wm );
    draw_frect( x + xsize - len, y - size, size + len, size, c, wm );
    
    draw_frect( x - size, y + ysize - len, size, len, c, wm );
    draw_frect( x - size, y + ysize, size + len, size, c, wm );
    
    draw_frect( x + xsize, y + ysize - len, size, len, c, wm );
    draw_frect( x + xsize - len, y + ysize, size + len, size, c, wm );
}

static void polygon_clipping_stage( sdwm_polygon* output, sdwm_polygon* input, int16_t clip, bool clip_direction, bool clip_vertical, window_manager* wm )
{
    input->vnum = output->vnum;
    smem_copy( input->v, output->v, input->vnum * sizeof( sdwm_vertex ) );
    
    output->vnum = 0;
    
    sdwm_vertex* s = &input->v[ input->vnum - 1 ];
    bool s_inside;
    if( clip_vertical )
    {
	if( s->x >= clip ) s_inside = 1; else s_inside = 0;
    }
    else 
    {
	if( s->y >= clip ) s_inside = 1; else s_inside = 0;
    }
    s_inside ^= clip_direction;
    for( int input_v = 0; input_v < input->vnum; input_v++ )
    {
	sdwm_vertex* e = &input->v[ input_v ];
	
	bool e_inside;
	if( clip_vertical )
	{
	    if( e->x >= clip ) e_inside = 1; else e_inside = 0;
	}
	else 
	{
	    if( e->y >= clip ) e_inside = 1; else e_inside = 0;
	}
	e_inside ^= clip_direction;
	
	bool add_intersection = 0;
	bool add_e = 0;
	
	if( e_inside )
	{
	    if( !s_inside )
	    {
		add_intersection = 1;
	    }
	    add_e = 1;
	}
	else 
	{
	    if( s_inside )
	    {
		add_intersection = 1;
	    }
	}
	
	sdwm_vertex* output_v;
	if( add_intersection )
	{
	    output_v = &output->v[ output->vnum ];
	    if( clip_vertical )
	    {
		output_v->x = clip;
		int len = e->x - s->x;
		if( len )
		{
		    if( len < 0 ) len = -len;
		    int d = ( (int)( e->y - s->y ) * 4096 ) / len;
		    if( clip >= s->x )
		    {
			d *= ( clip - s->x );
		    }
		    else
		    {
			d *= ( s->x - clip );
		    }
		    d /= 4096;
		    output_v->y = s->y + d;
		    if( ( wm->cur_flags & ( WBD_FLAG_ONE_OPACITY | WBD_FLAG_ONE_COLOR ) ) != ( WBD_FLAG_ONE_OPACITY | WBD_FLAG_ONE_COLOR ) )
		    {
			int coef;
			if( clip >= s->x )
			{
			    coef = ( (int)( clip - s->x ) * 256 ) / len;
			}
			else 
			{
			    coef = ( (int)( s->x - clip ) * 256 ) / len;
			}
			if( !( wm->cur_flags & WBD_FLAG_ONE_COLOR ) )
			{
			    output_v->c = blend( s->c, e->c, coef );
			}
			if( !( wm->cur_flags & WBD_FLAG_ONE_OPACITY ) )
			{
			    int t = ( ( 256 - coef ) * s->t + coef * e->t ) / 256;
			    if( t >= 255 ) t = 255;
			    output_v->t = (uint8_t)t;
			}
		    }
		}
		else 
		{
		    output_v->y = s->y;
		    output_v->c = s->c;
		    output_v->t = s->t;
		}
	    }
	    else 
	    {
		output_v->y = clip;
		int len = e->y - s->y;
		if( len )
		{
		    if( len < 0 ) len = -len;
		    int d = ( (int)( e->x - s->x ) * 4096 ) / len;
		    if( clip >= s->y )
		    {
			d *= ( clip - s->y );
		    }
		    else 
		    {
			d *= ( s->y - clip );
		    }
		    d /= 4096;
		    output_v->x = s->x + d;
		    if( ( wm->cur_flags & ( WBD_FLAG_ONE_OPACITY | WBD_FLAG_ONE_COLOR ) ) != ( WBD_FLAG_ONE_OPACITY | WBD_FLAG_ONE_COLOR ) )
		    {
			int coef;
			if( clip >= s->y )
			{
			    coef = ( (int)( clip - s->y ) * 256 ) / len;
			}
			else 
			{
			    coef = ( (int)( s->y - clip ) * 256 ) / len;
			}
			if( !( wm->cur_flags & WBD_FLAG_ONE_COLOR ) )
			{
			    output_v->c = blend( s->c, e->c, coef );
			}
			if( !( wm->cur_flags & WBD_FLAG_ONE_OPACITY ) )
			{
			    int t = ( ( 256 - coef ) * s->t + coef * e->t ) / 256;
			    if( t >= 255 ) t = 255;
			    output_v->t = (uint8_t)t;
			}
		    }
		}
		else 
		{
		    output_v->x = s->x;
		    output_v->c = s->c;
		    output_v->t = s->t;
		}
	    }
	    output->vnum++;
	}
	if( add_e )
	{
	    output_v = &output->v[ output->vnum ];
	    output_v->x = e->x;
	    output_v->y = e->y;
	    output_v->c = e->c;
	    output_v->t = e->t;
	    output->vnum++;
	}
	
	s = e;
	s_inside = e_inside;
    }
}

static void clip_polygon( 
    sdwm_polygon* subject, 
    sdwm_polygon* output, 
    sdwm_polygon* input, 
    int16_t offset_x, int16_t offset_y,
    int16_t clip_x, int16_t clip_y, int16_t clip_xsize, int16_t clip_ysize,
    window_manager* wm )
{
    //Sutherland–Hodgman algorithm:
    output->vnum = subject->vnum;
    smem_copy( output->v, subject->v, output->vnum * sizeof( sdwm_vertex ) );
    for( int i = 0; i < output->vnum; i++ )
    {
	output->v[ i ].x += offset_x;
	output->v[ i ].y += offset_y;
    }
    if( wm->cur_opacity < 255 )
    {
	if( !( wm->cur_flags & WBD_FLAG_ONE_OPACITY ) )
	{
	    for( int i = 0; i < output->vnum; i++ )
	    {
		int t = output->v[ i ].t;
		t *= wm->cur_opacity;
		t /= 256;
		output->v[ i ].t = t;
	    }
	}
    }
    polygon_clipping_stage( output, input, clip_x, 0, 1, wm );
    if( output->vnum == 0 ) return;
    polygon_clipping_stage( output, input, clip_y, 0, 0, wm );
    if( output->vnum == 0 ) return;
    polygon_clipping_stage( output, input, clip_x + clip_xsize, 1, 1, wm );
    if( output->vnum == 0 ) return;
    polygon_clipping_stage( output, input, clip_y + clip_ysize, 1, 0, wm );
    if( output->vnum == 0 ) return;
    if( wm->cur_flags & WBD_FLAG_ONE_COLOR )
    {
	output->v[ 0 ].c = subject->v[ 0 ].c;
    }
    if( wm->cur_flags & WBD_FLAG_ONE_OPACITY )
    {
	int t = subject->v[ 0 ].t;
	t *= wm->cur_opacity;
	t /= 256;
	output->v[ 0 ].t = t;
    }
}

static void draw_triangle( sdwm_vertex* v1, sdwm_vertex* v2, sdwm_vertex* v3, window_manager* wm )
{
    uint8_t one_t = v1->t;
    COLOR one_c = v1->c;
    
    //Sort vertices:
    sdwm_vertex* temp;
    if( v1->y > v2->y ) { temp = v1; v1 = v2; v2 = temp; }
    if( v1->y > v3->y ) { temp = v1; v1 = v3; v3 = temp; }
    if( v2->y > v3->y ) { temp = v2; v2 = v3; v3 = temp; }
    
    //Check:
    if( v1->y == v3->y ) return;
    bool color; //interpolate color
    bool opacity; //interpolate opacity
    color = ( wm->cur_flags & WBD_FLAG_ONE_COLOR ) == 0;
    opacity = ( wm->cur_flags & WBD_FLAG_ONE_OPACITY ) == 0;

    wm->screen_changed++;
    
    int dx1, dx2; //deltas...
    int dt1, dt2;
    int dr1, dg1, db1;
    int dr2, dg2, db2;
    int cx1, cx2; //current values...
    int ct1, ct2;
    int cr1, cg1, cb1;
    int cr2, cg2, cb2;
    
    //Calc deltas:
    int len = ( v2->y - v1->y );
    if( len ) 
    {
	dx1 = ( ( v2->x - v1->x ) << LINE_PREC ) / len;
	if( opacity )
	{
	    dt1 = ( ( v2->t - v1->t ) << LINE_PREC ) / len;
	}
	if( color )
	{
	    dr1 = ( ( red( v2->c ) - red( v1->c ) ) << LINE_PREC ) / len;
	    dg1 = ( ( green( v2->c ) - green( v1->c ) ) << LINE_PREC ) / len;
	    db1 = ( ( blue( v2->c ) - blue( v1->c ) ) << LINE_PREC ) / len;
	}
    }
    else
    {
	dx1 = 0;
	dt1 = 0;
	dr1 = 0;
	dg1 = 0;
	db1 = 0;
    }
    len = ( v3->y - v1->y );
    if( len ) 
    {
	dx2 = ( ( v3->x - v1->x ) << LINE_PREC ) / len;
	if( opacity )
	{
	    dt2 = ( ( v3->t - v1->t ) << LINE_PREC ) / len;
	}
	if( color )
	{
	    dr2 = ( ( red( v3->c ) - red( v1->c ) ) << LINE_PREC ) / len;
	    dg2 = ( ( green( v3->c ) - green( v1->c ) ) << LINE_PREC ) / len;
	    db2 = ( ( blue( v3->c ) - blue( v1->c ) ) << LINE_PREC ) / len;
	}
    }
    else
    {
	dx2 = 0;
	dt2 = 0;
	dr2 = 0;
	dg2 = 0;
	db2 = 0;
    }
    //Set start values:
    cx1 = v1->x << LINE_PREC;
    cx2 = cx1;
    if( opacity )
    {
	ct1 = v1->t << LINE_PREC;
	ct2 = ct1;
    }
    else 
    {
	ct1 = one_t;
    }
    if( color )
    {
	cr1 = red( v1->c ) << LINE_PREC;
	cg1 = green( v1->c ) << LINE_PREC;
	cb1 = blue( v1->c ) << LINE_PREC;
	cr2 = cr1;
	cg2 = cg1;
	cb2 = cb1;
    }
    for( int y = v1->y; y < v2->y; y++ )
    {
	int x1 = cx1 >> LINE_PREC;
	int x2 = cx2 >> LINE_PREC;
	bool xreverse = 0;
	if( x1 > x2 ) { int temp_x = x1; x1 = x2; x2 = temp_x; xreverse = 1; }
	int xsize = x2 - x1;
	if( xsize )
	{
	    COLORPTR ptr = wm->screen_pixels + y * wm->screen_xsize + x1;
	    COLORPTR size = ptr + xsize;
	    if( !opacity )
	    {
		if( !color )
		{
		    if( ct1 >= 255 )
		    {
			while( ptr < size ) { *ptr = one_c; ptr++; }
		    }
		    else 
		    {
			while( ptr < size ) { *ptr = blend( *ptr, one_c, ct1 ); ptr++; }
		    }
		}
		else
		{
		    int rlen = cr2 - cr1;
		    int glen = cg2 - cg1;
		    int blen = cb2 - cb1;
		    if( rlen == 0 && glen == 0 && blen == 0 )
		    {
			COLOR c = get_color( cr1 >> LINE_PREC, cg1 >> LINE_PREC, cb1 >> LINE_PREC );
			if( ct1 >= 255 )
			{
			    while( ptr < size ) { *ptr = c; ptr++; }
			}
			else 
			{
			    while( ptr < size ) { *ptr = blend( *ptr, c, ct1 ); ptr++; }
			}
		    }
		    else 
		    {
			int r;
			int g;
			int b;
			int dr = rlen / xsize;
			int dg = glen / xsize;
			int db = blen / xsize;
			if( xreverse )
			{
			    r = cr2;
			    g = cg2;
			    b = cb2;
			    dr = -dr;
			    dg = -dg;
			    db = -db;
			}
			else 
			{
			    r = cr1;
			    g = cg1;
			    b = cb1;
			}
			if( ct1 >= 255 )
			{
			    while( ptr < size ) 
			    { 
				*ptr++ = get_color( r >> LINE_PREC, g >> LINE_PREC, b >> LINE_PREC );
				r += dr;
				g += dg;
				b += db;
			    }
			}
			else 
			{
			    while( ptr < size ) 
			    {
				COLOR c = get_color( r >> LINE_PREC, g >> LINE_PREC, b >> LINE_PREC );
				*ptr = blend( *ptr, c, ct1 ); 
				ptr++;
				r += dr;
				g += dg;
				b += db;
			    }
			}
		    }
		}
	    }
	    else 
	    {
		if( !color )
		{
		    int t;
		    int dt;
		    if( xreverse )
		    {
			t = ct2;
			dt = ( ct1 - ct2 ) / xsize;
		    }
		    else 
		    {
			t = ct1;
			dt = ( ct2 - ct1 ) / xsize;
		    }
		    while( ptr < size ) 
		    { 
			*ptr = blend( *ptr, one_c, t >> LINE_PREC );
			ptr++;
			t += dt;
		    }
		}
		else 
		{
		    int t;
		    int dt;
		    int r;
		    int g;
		    int b;
		    int dr;
		    int dg;
		    int db;
		    if( xreverse )
		    {
			t = ct2;
			dt = ( ct1 - ct2 ) / xsize;
			r = cr2;
			g = cg2;
			b = cb2;
			dr = ( cr1 - cr2 ) / xsize;
			dg = ( cg1 - cg2 ) / xsize;
			db = ( cb1 - cb2 ) / xsize;
		    }
		    else 
		    {
			t = ct1;
			dt = ( ct2 - ct1 ) / xsize;
			r = cr1;
			g = cg1;
			b = cb1;
			dr = ( cr2 - cr1 ) / xsize;
			dg = ( cg2 - cg1 ) / xsize;
			db = ( cb2 - cb1 ) / xsize;
		    }
		    while( ptr < size ) 
		    { 
			COLOR c = get_color( r >> LINE_PREC, g >> LINE_PREC, b >> LINE_PREC );
			*ptr = blend( *ptr, c, t >> LINE_PREC );
			ptr++; 
			t += dt;
			r += dr;
			g += dg;
			b += db;
		    }
		}
	    }
	}
	cx1 += dx1;
	cx2 += dx2;
	if( opacity )
	{
	    ct1 += dt1;
	    ct2 += dt2;
	}
	if( color )
	{
	    cr1 += dr1;
	    cg1 += dg1;
	    cb1 += db1;
	    cr2 += dr2;
	    cg2 += dg2;
	    cb2 += db2;
	}
    }
    //Calc deltas:
    len = ( v3->y - v2->y );
    if( len ) 
    {
	dx1 = ( ( v3->x - v2->x ) << LINE_PREC ) / len;
	if( opacity )
	{
	    dt1 = ( ( v3->t - v2->t ) << LINE_PREC ) / len;
	}
	if( color )
	{
	    dr1 = ( ( red( v3->c ) - red( v2->c ) ) << LINE_PREC ) / len;
	    dg1 = ( ( green( v3->c ) - green( v2->c ) ) << LINE_PREC ) / len;
	    db1 = ( ( blue( v3->c ) - blue( v2->c ) ) << LINE_PREC ) / len;
	}
    }
    else 
    {
	dx1 = 0;
	dt1 = 0;
	dr1 = 0;
	dg1 = 0;
	db1 = 0;
    }
    //Set start values:
    cx1 = v2->x << LINE_PREC;
    if( opacity )
    {
	ct1 = v2->t << LINE_PREC;
    }
    if( color )
    {
	cr1 = red( v2->c ) << LINE_PREC;
	cg1 = green( v2->c ) << LINE_PREC;
	cb1 = blue( v2->c ) << LINE_PREC;
    }
    for( int y = v2->y; y < v3->y; y++ )
    {
	int x1 = cx1 >> LINE_PREC;
	int x2 = cx2 >> LINE_PREC;
	bool xreverse = 0;
	if( x1 > x2 ) { int temp_x = x1; x1 = x2; x2 = temp_x; xreverse = 1; }
	int xsize = x2 - x1;
	if( xsize )
	{
	    COLORPTR ptr = wm->screen_pixels + y * wm->screen_xsize + x1;
	    COLORPTR size = ptr + xsize;
	    if( !opacity )
	    {
		if( !color )
		{
		    if( ct1 >= 255 )
		    {
			while( ptr < size ) { *ptr = one_c; ptr++; }
		    }
		    else 
		    {
			while( ptr < size ) { *ptr = blend( *ptr, one_c, ct1 ); ptr++; }
		    }
		}
		else
		{
		    int rlen = cr2 - cr1;
		    int glen = cg2 - cg1;
		    int blen = cb2 - cb1;
		    if( rlen == 0 && glen == 0 && blen == 0 )
		    {
			COLOR c = get_color( cr1 >> LINE_PREC, cg1 >> LINE_PREC, cb1 >> LINE_PREC );
			if( ct1 >= 255 )
			{
			    while( ptr < size ) { *ptr = c; ptr++; }
			}
			else 
			{
			    while( ptr < size ) { *ptr = blend( *ptr, c, ct1 ); ptr++; }
			}
		    }
		    else 
		    {
			int r;
			int g;
			int b;
			int dr = rlen / xsize;
			int dg = glen / xsize;
			int db = blen / xsize;
			if( xreverse )
			{
			    r = cr2;
			    g = cg2;
			    b = cb2;
			    dr = -dr;
			    dg = -dg;
			    db = -db;
			}
			else 
			{
			    r = cr1;
			    g = cg1;
			    b = cb1;
			}
			if( ct1 >= 255 )
			{
			    while( ptr < size ) 
			    { 
				*ptr++ = get_color( r >> LINE_PREC, g >> LINE_PREC, b >> LINE_PREC );
				r += dr;
				g += dg;
				b += db;
			    }
			}
			else 
			{
			    while( ptr < size ) 
			    {
				COLOR c = get_color( r >> LINE_PREC, g >> LINE_PREC, b >> LINE_PREC );
				*ptr = blend( *ptr, c, ct1 ); 
				ptr++;
				r += dr;
				g += dg;
				b += db;
			    }
			}
		    }
		}
	    }
	    else 
	    {
		if( !color )
		{
		    int t;
		    int dt;
		    if( xreverse )
		    {
			t = ct2;
			dt = ( ct1 - ct2 ) / xsize;
		    }
		    else 
		    {
			t = ct1;
			dt = ( ct2 - ct1 ) / xsize;
		    }
		    while( ptr < size ) 
		    { 
			*ptr = blend( *ptr, one_c, t >> LINE_PREC );
			ptr++;
			t += dt;
		    }
		}
		else 
		{
		    int t;
		    int dt;
		    int r;
		    int g;
		    int b;
		    int dr;
		    int dg;
		    int db;
		    if( xreverse )
		    {
			t = ct2;
			dt = ( ct1 - ct2 ) / xsize;
			r = cr2;
			g = cg2;
			b = cb2;
			dr = ( cr1 - cr2 ) / xsize;
			dg = ( cg1 - cg2 ) / xsize;
			db = ( cb1 - cb2 ) / xsize;
		    }
		    else 
		    {
			t = ct1;
			dt = ( ct2 - ct1 ) / xsize;
			r = cr1;
			g = cg1;
			b = cb1;
			dr = ( cr2 - cr1 ) / xsize;
			dg = ( cg2 - cg1 ) / xsize;
			db = ( cb2 - cb1 ) / xsize;
		    }
		    while( ptr < size ) 
		    { 
			COLOR c = get_color( r >> LINE_PREC, g >> LINE_PREC, b >> LINE_PREC );
			*ptr = blend( *ptr, c, t >> LINE_PREC );
			ptr++;
			t += dt;
			r += dr;
			g += dg;
			b += db;
		    }
		}
	    }
	}
	cx1 += dx1;
	cx2 += dx2;
	if( opacity )
	{
	    ct1 += dt1;
	    ct2 += dt2;
	}
	if( color )
	{
	    cr1 += dr1;
	    cg1 += dg1;
	    cb1 += db1;
	    cr2 += dr2;
	    cg2 += dg2;
	    cb2 += db2;
	}
    }
}

void draw_polygon( sdwm_polygon* p, window_manager* wm )
{
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    WINDOWPTR win = wm->cur_window;

    sdwm_polygon input;
    sdwm_polygon output;
    input.v = wm->poly_vertices1;
    output.v = wm->poly_vertices2;
    
    if( WBD_OPENGL )
    {
#ifdef OPENGL
	for( int r = 0; r < win->reg->numRects; r++ )
	{
	    int rx1 = win->reg->rects[ r ].left;
	    int rx2 = win->reg->rects[ r ].right;
	    int ry1 = win->reg->rects[ r ].top;
	    int ry2 = win->reg->rects[ r ].bottom;

	    clip_polygon( p, &output, &input, win->screen_x, win->screen_y, rx1, ry1, rx2 - rx1, ry2 - ry1, wm );

	    if( output.vnum >= 3 )
	    {
		if( output.vnum > 8 ) slog( "ERROR: %d vertices!\n", output.vnum );
		gl_draw_polygon( &output, wm );
		wm->screen_changed++;
	    }
	}
#endif
    }
    else
    {
	GET_CLIPPING_RECT;
	clip_polygon( p, &output, &input, win->screen_x, win->screen_y, clip_x1, clip_y1, clip_x2 - clip_x1, clip_y2 - clip_y1, wm );
	if( output.vnum >= 3 )
	{
	    if( output.vnum > 8 ) slog( "ERROR: %d vertices!\n", output.vnum );
	    for( int i = 2; i < output.vnum; i++ )
	    {
		draw_triangle( &output.v[ 0 ], &output.v[ i - 1 ], &output.v[ i ], wm );
	    }
	}
    }
}

int draw_char( uint32_t c, int x, int y, window_manager* wm )
{
    if( c == 0x0A ) return 0; //Control symbols

    WINDOWPTR win = wm->cur_window;
    if( !win ) return 0;

    utf32_to_sundog_font_char( c, c );

    int font_num = win->font;
    sdwm_font* font = &wm->fonts[ font_num ];
    int char_xsize = font->char_xsize[ c ];
    int char_ysize = font->char_ysize;

    int zoom = wm->cur_font_scale * wm->font_zoom;
    int char_xsize_zoom = ( char_xsize * zoom ) / (256*256);
    int char_ysize_zoom = ( char_ysize * zoom ) / (256*256);

    if( c == ' ' ) return char_xsize_zoom;
    if( wm->cur_window_invisible ) return char_xsize_zoom;

    if( WBD_OPENGL )
    {
#ifdef OPENGL
	sdwm_image* font_img = font->img;

	font_img->opacity = wm->cur_opacity;
	font_img->color = wm->cur_font_color;

	float source_x = font->grid_xoffset + ( c & 15 ) * font->grid_cell_xsize;
	float source_y = font->grid_yoffset + ( c / 16 ) * font->grid_cell_ysize;
	float source_xsize = font->char_xsize2[ c ];
	float source_ysize = font->char_ysize2;
	int dest_xsize = char_xsize_zoom;
	int dest_ysize = char_ysize_zoom;
	float pixel_xsize = source_xsize / (float)dest_xsize;
	float pixel_ysize = source_ysize / (float)dest_ysize;

	x += win->screen_x;
	y += win->screen_y;
	for( int r = 0; r < win->reg->numRects; r++ )
	{
    	    int rx1 = win->reg->rects[ r ].left;
    	    int rx2 = win->reg->rects[ r ].right;
	    int ry1 = win->reg->rects[ r ].top;
	    int ry2 = win->reg->rects[ r ].bottom;

	    //Clipping:
	    float src_xoff = 0;
	    float src_yoff = 0;
	    float src_xs = source_xsize;
	    float src_ys = source_ysize;
	    int nx = x;
	    int ny = y;
	    int nxsize = dest_xsize;
	    int nysize = dest_ysize;
	    if( nx < rx1 ) 
	    { 
		//Destination:
		int sub = ( rx1 - nx );
		nxsize -= sub;
		nx = rx1; 
		//Source:
		src_xoff = (float)sub * pixel_xsize;
	    }
	    if( ny < ry1 ) 
	    { 
		//Destination:
		int sub = ( ry1 - ny ); 
		nysize -= sub;
		ny = ry1; 
		//Source:
		src_yoff = (float)sub * pixel_ysize; 
	    }
	    if( nx + nxsize <= rx1 ) continue;
	    if( ny + nysize <= ry1 ) continue;
	    if( nx + nxsize > rx2 ) 
	    {
		//Destination:
		int sub = nx + nxsize - rx2;
		nxsize -= sub;
		//Source:
		src_xs -= (float)sub * pixel_xsize;
	    }
	    if( ny + nysize > ry2 ) 
	    {
		//Destination:
		int sub = ny + nysize - ry2;
		nysize -= sub;
		//Source:
		src_ys -= (float)sub * pixel_ysize;
	    }
	    if( nx >= rx2 ) continue;
	    if( ny >= ry2 ) continue;
	    if( nxsize < 0 ) continue;
	    if( nysize < 0 ) continue;

	    //Draw it:
	    gl_draw_image_scaled( 
		nx, ny, 
		nxsize, nysize, 
		source_x + src_xoff, source_y + src_yoff,
		src_xs - src_xoff, src_ys - src_yoff,
		font_img,
		wm );
	    wm->screen_changed++;
	}
#endif
    }
    else
    {
	x += win->screen_x;
	y += win->screen_y;

	//Bounds control:
	if( x < 0 ) return char_xsize_zoom;
	if( y < 0 ) return char_xsize_zoom;
	if( x > wm->screen_xsize - char_xsize_zoom ) return char_xsize_zoom;
	if( x >= win->screen_x + win->xsize ) return char_xsize_zoom;

	wm->screen_changed++;

	uint8_t opacity = wm->cur_opacity;
	int screen_xsize = wm->screen_xsize;
	int screen_ysize = wm->screen_ysize;
	COLORPTR screen_pixels = wm->screen_pixels;
	COLOR cur_font_color = wm->cur_font_color;

	int p = c * char_ysize;
	int sp = y * screen_xsize + x;

	const FONT_LINE_TYPE* font_data = font->data + 257;

	zoom /= 256;
	if( zoom == 256 )
	{
	    if( y + char_ysize > screen_ysize ) 
	    {
		//y clipping:
		char_ysize -= ( y + char_ysize ) - screen_ysize;
		if( char_ysize <= 0 ) return char_xsize_zoom;
	    }
	    int add = screen_xsize - char_xsize;
	    if( opacity == 255 )
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p ];
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( fpart & FONT_LEFT_PIXEL ) 
			    screen_pixels[ sp ] = cur_font_color;
			fpart <<= 1;
			sp++;
		    }
		    sp += add;
		    p++;
		}
	    }
	    else
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p ];
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( fpart & FONT_LEFT_PIXEL )
			    screen_pixels[ sp ] = blend( screen_pixels[ sp ], cur_font_color, opacity );
			fpart <<= 1;
			sp++;
		    }
		    sp += add;
		    p++;
		}
	    }
	}
	else
        {
	    char_xsize = char_xsize_zoom;
	    char_ysize = char_ysize_zoom;
	    if( y + char_ysize > screen_ysize ) 
	    {
		//y clipping:
		char_ysize -= ( y + char_ysize ) - screen_ysize;
		if( char_ysize <= 0 ) return char_xsize;
	    }
	    int add = screen_xsize - char_xsize;
	    int yy = 0;
	    int delta = ( 256 << 15 ) / zoom;
	    if( opacity == 255 )
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p + ( yy >> 15 ) ];
		    int xx = 0;
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( ( fpart << ( xx >> 15 ) ) & FONT_LEFT_PIXEL )
			    screen_pixels[ sp ] = cur_font_color;
			xx += delta;
			sp++;
		    }
		    yy += delta;
		    sp += add;
		}
	    }
	    else
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p + ( yy >> 15 ) ];
		    int xx = 0;
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( ( fpart << ( xx >> 15 ) ) & FONT_LEFT_PIXEL )
			    screen_pixels[ sp ] = blend( screen_pixels[ sp ], cur_font_color, opacity );
			xx += delta;
			sp++;
		    }
		    yy += delta;
		    sp += add;
		}
	    }
	}
    }

    return char_xsize_zoom;
}

int draw_char_vert( uint32_t c, int x, int y, window_manager* wm ) //top to bottom
{
    if( c == 0x0A ) return 0; //Control symbols

    WINDOWPTR win = wm->cur_window;
    if( !win ) return 0;

    utf32_to_sundog_font_char( c, c );

    int font_num = win->font;
    sdwm_font* font = &wm->fonts[ font_num ];
    int char_xsize = font->char_xsize[ c ];
    int char_ysize = font->char_ysize;

    int zoom = wm->cur_font_scale * wm->font_zoom;
    int char_xsize_zoom = ( char_xsize * zoom ) / (256*256);
    int char_ysize_zoom = ( char_ysize * zoom ) / (256*256);

    if( c == ' ' ) return char_xsize_zoom;
    if( wm->cur_window_invisible ) return char_xsize_zoom;

    if( WBD_OPENGL )
    {
#ifdef OPENGL
	sdwm_image* font_img = font->img;

	font_img->opacity = wm->cur_opacity;
	font_img->color = wm->cur_font_color;

	float source_x = font->grid_xoffset + ( c & 15 ) * font->grid_cell_xsize;
	float source_y = font->grid_yoffset + ( c / 16 ) * font->grid_cell_ysize;
	float source_xsize = font->char_xsize2[ c ];
	float source_ysize = font->char_ysize2;
	float source_pixel_xsize = source_xsize / (float)char_xsize_zoom;
	float source_pixel_ysize = source_ysize / (float)char_ysize_zoom;
	int dest_xsize = char_ysize_zoom;
	int dest_ysize = char_xsize_zoom;

	x += win->screen_x;
	y += win->screen_y;
	for( int r = 0; r < win->reg->numRects; r++ )
	{
    	    int rx1 = win->reg->rects[ r ].left;
    	    int rx2 = win->reg->rects[ r ].right;
	    int ry1 = win->reg->rects[ r ].top;
	    int ry2 = win->reg->rects[ r ].bottom;

	    //Clipping:
	    float src_xoff = 0;
	    float src_yoff = 0;
	    float src_xs = source_xsize;
	    float src_ys = source_ysize;
	    int dx = x;
	    int dy = y;
	    int dxsize = dest_xsize;
	    int dysize = dest_ysize;
	    if( dx < rx1 )
	    {
		//Destination:
		int sub = ( rx1 - dx );
		dxsize -= sub;
		dx = rx1;
		//Source:
		src_ys -= (float)sub * source_pixel_ysize;
	    }
	    if( dy < ry1 )
	    {
		//Destination:
		int sub = ( ry1 - dy );
		dysize -= sub;
		dy = ry1;
		//Source:
		src_xoff = (float)sub * source_pixel_xsize;
	    }
	    if( dx + dxsize <= rx1 ) continue;
	    if( dy + dysize <= ry1 ) continue;
	    if( dx + dxsize > rx2 )
	    {
		//Destination:
		int sub = dx + dxsize - rx2;
		dxsize -= sub;
		//Source:
		src_yoff = (float)sub * source_pixel_ysize;
	    }
	    if( dy + dysize > ry2 )
	    {
		//Destination:
		int sub = dy + dysize - ry2;
		dysize -= sub;
		//Source:
		src_xs -= (float)sub * source_pixel_xsize;
	    }
	    if( dx >= rx2 ) continue;
	    if( dy >= ry2 ) continue;
	    if( dxsize < 0 ) continue;
	    if( dysize < 0 ) continue;

	    //Draw it:
	    gl_draw_image_scaled_vert( 
		dx, dy, //dest xy
		dxsize, dysize, //dest size
		source_x + src_xoff, source_y + src_yoff, //src xy
		src_xs - src_xoff, src_ys - src_yoff, //src size
		font_img,
		wm );
	    wm->screen_changed++;
	}
#endif
    }
    else
    {
	x += win->screen_x;
	y += win->screen_y;

	//Bounds control:
	if( x < 0 ) return char_xsize_zoom;
	if( x + char_ysize_zoom > wm->screen_xsize ) return char_xsize_zoom;
	if( y < 0 ) return char_xsize_zoom;
	if( y + char_xsize_zoom > wm->screen_ysize ) return char_xsize_zoom;
	if( y >= win->screen_y + win->ysize ) return char_xsize_zoom;

	wm->screen_changed++;

	uint8_t opacity = wm->cur_opacity;
	int screen_xsize = wm->screen_xsize;
	int screen_ysize = wm->screen_ysize;
	COLOR cur_font_color = wm->cur_font_color;

	int p = c * char_ysize + char_ysize - 1;
	COLORPTR sp = wm->screen_pixels + y * screen_xsize + x;

	const FONT_LINE_TYPE* font_data = font->data + 257;

	zoom /= 256;
	if( zoom == 256 )
	{
	    int add = -screen_xsize * char_xsize + 1;
	    if( opacity == 255 )
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p ];
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( fpart & FONT_LEFT_PIXEL )
			    *sp = cur_font_color;
			fpart <<= 1;
			sp += screen_xsize;
		    }
		    sp += add;
		    p--;
		}
	    }
	    else
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p ];
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( fpart & FONT_LEFT_PIXEL )
			    *sp = blend( *sp, cur_font_color, opacity );
			fpart <<= 1;
			sp += screen_xsize;
		    }
		    sp += add;
		    p--;
		}
	    }
	}
	else
        {
	    char_xsize = char_xsize_zoom;
	    char_ysize = char_ysize_zoom;
	    int add = -screen_xsize * char_xsize + 1;
	    int yy = 0;
	    int delta = ( 256 << 15 ) / zoom;
	    if( opacity == 255 )
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p - ( yy >> 15 ) ];
		    int xx = 0;
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( ( fpart << ( xx >> 15 ) ) & FONT_LEFT_PIXEL )
			    *sp = cur_font_color;
			xx += delta;
			sp += screen_xsize;
		    }
		    yy += delta;
		    sp += add;
		}
	    }
	    else
	    {
		for( int cy = 0; cy < char_ysize; cy++ )
		{
		    int fpart = font_data[ p - ( yy >> 15 ) ];
		    int xx = 0;
		    for( int cx = 0; cx < char_xsize; cx++ )
		    {
			if( ( fpart << ( xx >> 15 ) ) & FONT_LEFT_PIXEL )
			    *sp = blend( *sp, cur_font_color, opacity );
			xx += delta;
			sp += screen_xsize;
		    }
		    yy += delta;
		    sp += add;
		}
	    }
	}
    }

    return char_xsize_zoom;
}

void draw_string( const char* str, int x, int y, window_manager* wm )
{
    if( !str ) return;
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;
    if( y >= wm->cur_window->ysize ) return;
    if( x >= wm->cur_window->xsize ) return;

    int start_x = x;

    while( *str )
    {
	if( *str == 0xA )
	{
	    y += char_y_size( wm );
	    x = start_x;
	    str++;
	}
	else
	{
	    uint32_t c32;
	    str += utf8_to_utf32_char( str, &c32 );
	    draw_char( c32, x, y, wm );
	    x += char_x_size( c32, wm );
	}
    }
}

void draw_string_vert( const char* str, int x, int y, window_manager* wm )
{
    if( !str ) return;
    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;
    if( y >= wm->cur_window->ysize ) return;
    if( x >= wm->cur_window->xsize ) return;

    int start_y = y;

    while( *str )
    {
	if( *str == 0xA )
	{
	    x -= char_y_size( wm );
	    y = start_y;
	    str++;
	}
	else
	{
	    uint32_t c32;
	    str += utf8_to_utf32_char( str, &c32 );
	    draw_char_vert( c32, x, y, wm );
	    y += char_x_size( c32, wm );
	}
    }
}

/*bool draw_string2( const char* str, int* x, int* y, window_manager* wm )
{
    if( !str ) return false;
    if( wm->cur_opacity == 0 ) return false;
    if( wm->cur_window_invisible ) return false;
    if( y >= wm->cur_window->ysize ) return false;
    if( x >= wm->cur_window->xsize ) return false;

    int xx = *x;
    int yy = *y;
    int start_x = xx;

    while( *str )
    {
	if( *str == 0xA )
	{
	    yy += char_y_size( wm );
	    xx = start_x;
	    str++;
	}
	else
	{
	    uint32_t c32;
	    str += utf8_to_utf32_char( str, &c32 );
	    draw_char( c32, xx, yy, wm );
	    xx += char_x_size( c32, wm );
	}
    }

    *x = xx;
    *y = yy;

    return true;
}*/

void draw_string_wordwrap( const char* str, int x, int y, int xsize, int* out_xsize, int* out_ysize, bool dont_draw, window_manager* wm )
{
    if( !str ) return;
    if( out_xsize == 0 && out_ysize == 0 )
    {
	if( wm->cur_opacity == 0 ) return;
	if( wm->cur_window_invisible ) return;
	if( y >= wm->cur_window->ysize ) return;
	if( x >= wm->cur_window->xsize ) return;
    }

    int max_x = x;

    int start_y = y;
    int start_x = x;

    uint32_t ts[ 128 ];
    uint32_t c32;

    while( 1 )
    {
	//Get next word:
	int word_xsize = 0;
	int word_len = 0;
        while( 1 )
	{
	    str += utf8_to_utf32_char( str, &c32 );
    	    word_xsize += char_x_size( c32, wm );
    	    ts[ word_len ] = c32;
    	    word_len++;
    	    if( (unsigned)word_len >= sizeof( ts ) / sizeof( uint32_t ) ) break;
    	    if( c32 <= 0x30 ) break;
	}
	
	//Print this word:
	if( x + word_xsize > start_x + xsize )
	{
	    //No free space:
	    if( word_xsize <= xsize )
	    {
		y += char_y_size( wm );
	        x = start_x;
	        if( word_len == 1 && ts[ 0 ] == ' ' )
	        {
	        }
	        else
	        {
	    	    for( int i = 0; i < word_len; i++ )
		    {
			c32 = ts[ i ];
			if( dont_draw == 0 ) draw_char( c32, x, y, wm );
			x += char_x_size( c32, wm );
		    }
		}
		if( x > max_x ) max_x = x;
	    }
	    else
	    {
		//No free space at all:
	        for( int i = 0; i < word_len; i++ )
		{
		    c32 = ts[ i ];
		    int char_xsize = char_x_size( c32, wm );
		    if( x + char_xsize > start_x + xsize )
		    {
			y += char_y_size( wm );
		        x = start_x;
		    }
		    if( dont_draw == 0 ) draw_char( c32, x, y, wm );
		    x += char_x_size( c32, wm );
		    if( x > max_x ) max_x = x;
		}
	    }
	}
	else
	{
	    for( int i = 0; i < word_len; i++ )
	    {
	        c32 = ts[ i ];
	        if( dont_draw == 0 ) draw_char( c32, x, y, wm );
	        x += char_x_size( c32, wm );
	    }
	    if( x > max_x ) max_x = x;
	}

	if( ts[ word_len - 1 ] == 0xA )
	{
	    y += char_y_size( wm );
	    x = start_x;
	}

	if( ts[ word_len - 1 ] == 0 ) break;
    }
    
    if( out_xsize )
	*out_xsize = max_x - start_x;
    if( out_ysize )
	*out_ysize = ( y - start_y ) + char_y_size( wm );
}

void draw_string_limited( const char* str, int x, int y, int limit, window_manager* wm ) //limit in utf8 chars
{
    if( !str ) return;

    //Bounds control:
    if( y < -char_y_size( wm ) ) return;
    if( x >= wm->cur_window->xsize ) return;

    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    int start_x = x;
    int ptr = 0;

    while( 1 )
    {
	if( *str == 0 ) break;
	if( limit >= 0 )
	    if( ptr >= limit ) break;
	if( *str == 0xA ) 
	{ 
	    y += char_y_size( wm ); 
	    x = start_x; 
	    str++;
	    ptr++;
	}
	else
	{
	    uint32_t c32;
	    int add = utf8_to_utf32_char( str, &c32 );
	    str += add;
	    ptr += add;
	    int xx = char_x_size( c32, wm );
	    if( limit < 0 )
		if( x + xx > start_x - limit ) break;
	    draw_char( c32, x, y, wm );
	    x += xx;
	}
    }
}

void draw_updown( int x, int y, COLOR color, int down_add, window_manager* wm )
{
    WINDOWPTR win = wm->cur_window;

    int prev_font = win->font;
    COLOR prev_font_color = wm->cur_font_color;
    win->font = wm->font_medium_mono;
    wm->cur_font_color = color;

    int add = char_y_size( wm ) / 8;
    draw_string( g_str_up, x - char_x_size( '#', wm ) / 2, y - char_y_size( wm ) + add, wm );
    draw_string( g_str_down, x - char_x_size( '#', wm ) / 2, y - add + down_add, wm );

    win->font = prev_font;
    wm->cur_font_color = prev_font_color;
}

void draw_leftright( int x, int y, COLOR color, int right_add, window_manager* wm )
{
    WINDOWPTR win = wm->cur_window;

    int prev_font = win->font;
    COLOR prev_font_color = wm->cur_font_color;
    win->font = wm->font_medium_mono;
    wm->cur_font_color = color;

    int add = char_y_size( wm ) / 8;
    draw_string( g_str_left, x - char_x_size( '#', wm ) + add, y - char_y_size( wm ) / 2, wm );
    draw_string( g_str_right, x - add + right_add, y - char_y_size( wm ) / 2, wm );

    win->font = prev_font;
    wm->cur_font_color = prev_font_color;
}

void draw_image_scaled( int dest_x, int dest_y, sdwm_image_scaled* img, window_manager* wm )
{
    if( !img ) return;
    if( !img->img ) return;
    if( img->src_xsize <= 0 ) return;
    if( img->src_ysize <= 0 ) return;
    if( img->dest_xsize <= 0 ) return;
    if( img->dest_ysize <= 0 ) return;

    if( wm->cur_opacity == 0 ) return;
    if( wm->cur_window_invisible ) return;

    WINDOWPTR win = wm->cur_window;

    uint8_t opacity = wm->cur_opacity;
    img->img->opacity = opacity;

    if( WBD_OPENGL )
    {
#ifdef OPENGL
	float f_src_xsize = (float)img->src_xsize / (float)( (int)1 << IMG_PREC );
        float f_src_ysize = (float)img->src_ysize / (float)( (int)1 << IMG_PREC );
	float f_src_x = (float)img->src_x / (float)( (int)1 << IMG_PREC );
	float f_src_y = (float)img->src_y / (float)( (int)1 << IMG_PREC );
	float pixel_xsize = f_src_xsize / (float)img->dest_xsize;
	float pixel_ysize = f_src_ysize / (float)img->dest_ysize;

	dest_x += win->screen_x;
	dest_y += win->screen_y;
	for( int r = 0; r < win->reg->numRects; r++ )
	{
	    int rx1 = win->reg->rects[ r ].left;
	    int rx2 = win->reg->rects[ r ].right;
	    int ry1 = win->reg->rects[ r ].top;
	    int ry2 = win->reg->rects[ r ].bottom;

	    //Control box size:
	    float src_x2 = f_src_x;
	    float src_y2 = f_src_y;
	    float src_xsize2 = f_src_xsize;
	    float src_ysize2 = f_src_ysize;
	    int nx = dest_x;
	    int ny = dest_y;
	    int nxsize = img->dest_xsize;
	    int nysize = img->dest_ysize;
	    if( nx < rx1 ) 
	    { 
		//Destination:
		int sub = ( rx1 - nx );
		nxsize -= sub;
		nx = rx1; 
		//Source:
		float fsub = (float)sub * pixel_xsize;
		src_x2 += fsub;
		src_xsize2 -= fsub;
	    }
	    if( ny < ry1 ) 
	    { 
		//Destination:
		int sub = ( ry1 - ny ); 
		nysize -= sub;
		ny = ry1; 
		//Source:
		float fsub = (float)sub * pixel_ysize;
		src_y2 += fsub;
		src_ysize2 -= fsub;
	    }
	    if( nx + nxsize <= rx1 ) continue;
	    if( ny + nysize <= ry1 ) continue;
	    if( nx + nxsize > rx2 ) 
	    {
		//Destination:
		int sub = nx + nxsize - rx2;
		nxsize -= sub;
		//Source:
		src_xsize2 -= (float)sub * pixel_xsize;
	    }
	    if( ny + nysize > ry2 ) 
	    {
		//Destination:
		int sub = ny + nysize - ry2;
		nysize -= sub;
		//Source:
		src_ysize2 -= (float)sub * pixel_ysize;
	    }
	    if( nx >= rx2 ) continue;
	    if( ny >= ry2 ) continue;
	    if( nxsize < 0 ) continue;
	    if( nysize < 0 ) continue;

	    //Draw it:
	    gl_draw_image_scaled( 
		nx, ny, 
		nxsize, nysize, 
		src_x2, src_y2,
		src_xsize2, src_ysize2,
		img->img,
		wm );
	    wm->screen_changed++;
	}
#endif
    }
    else
    {
	//Calc deltas:
	int dx = img->src_xsize / img->dest_xsize;
	int dy = img->src_ysize / img->dest_ysize;
    
	COLORPTR scr = wm->screen_pixels;
	int screen_xsize = wm->screen_xsize;
	int screen_ysize = wm->screen_ysize;
	dest_x += win->screen_x;
	dest_y += win->screen_y;
	int dest_xsize = img->dest_xsize;
	int dest_ysize = img->dest_ysize;
	int src_x = img->src_x;
	int src_y = img->src_y;
	
	//Bounds control:
	GET_CLIPPING_RECT;
	if( dest_x < clip_x1 ) 
	{ 
	    int xx = dest_x - clip_x1;
	    dest_xsize += xx;
	    if( dest_xsize <= 0 ) return;
	    src_x += -xx * dx;
	    dest_x = clip_x1;
	}    
	if( dest_y < clip_y1 ) 
	{ 
	    int yy = dest_y - clip_y1;
	    dest_ysize += yy;
	    if( dest_ysize <= 0 ) return;
	    src_y += -yy * dy;
	    dest_y = clip_y1;
	}
        if( dest_x + dest_xsize > clip_x2 )
        {
	    dest_xsize -= ( dest_x + dest_xsize ) - clip_x2;
	    if( dest_xsize <= 0 ) return;
	}
	if( dest_y + dest_ysize > clip_y2 )
	{
	    dest_ysize -= ( dest_y + dest_ysize ) - clip_y2;
	    if( dest_ysize <= 0 ) return;
	}
    
	wm->screen_changed++;

	int dest_ptr = dest_y * screen_xsize + dest_x;
	int add = screen_xsize - dest_xsize;
	int ymask = 0xFFFFFFFF;
	if( img->img->flags & IMAGE_YREPEAT ) ymask = img->img->ysize - 1;
	if( img->img->flags & IMAGE_ALPHA8 )
	{
	    uint8_t* img_data = (uint8_t*)img->img->data;
	    COLOR color = img->img->color;
	    for( int yy = 0; yy < dest_ysize; yy++ )
	    {
		int cx = src_x;
		int img_ptr = ( ( src_y >> IMG_PREC ) & ymask ) * img->img->xsize;
		if( opacity == 255 )
		{
		    for( int xx = 0; xx < dest_xsize; xx++ )
		    {
			uint8_t v = img_data[ img_ptr + ( cx >> IMG_PREC ) ];
			scr[ dest_ptr ] = blend( scr[ dest_ptr ], color, v );
			dest_ptr++;
			cx += dx;
		    }
		}
		else
		{
		    for( int xx = 0; xx < dest_xsize; xx++ )
		    {
			uint v = img_data[ img_ptr + ( cx >> IMG_PREC ) ];
			v = ( v * opacity ) / 256;
			scr[ dest_ptr ] = blend( scr[ dest_ptr ], color, v );
		        dest_ptr++;
			cx += dx;
		    }
		}
		dest_ptr += add;
		src_y += dy;
	    }
	}
	else
	if( img->img->flags & IMAGE_NATIVE_RGB )
	{
	    COLORPTR img_data = (COLORPTR)img->img->data;
	    for( int yy = 0; yy < dest_ysize; yy++ )
	    {
		int cx = src_x;
		int img_ptr = ( src_y >> IMG_PREC ) * img->img->xsize;
		if( opacity == 255 )
		{
		    for( int xx = 0; xx < dest_xsize; xx++ )
		    {
			scr[ dest_ptr ] = img_data[ img_ptr + ( cx >> IMG_PREC ) ];
			dest_ptr++;
			cx += dx;
	    	    }
		}
		else
		{
		    for( int xx = 0; xx < dest_xsize; xx++ )
		    {
			scr[ dest_ptr ] = blend( scr[ dest_ptr ], img_data[ img_ptr + ( cx >> IMG_PREC ) ], opacity );
			dest_ptr++;
			cx += dx;
		    }
		}
		dest_ptr += add;
		src_y += dy;
	    }
	}
    }
}

int font_char_x_size( uint32_t c, int font, window_manager* wm )
{
    if( c == 0x0A ) return 0;
    utf32_to_sundog_font_char( c, c );
    return (int)wm->fonts[ font ].char_xsize[ c ] * wm->font_zoom / 256;
}

int font_char_y_size( int font, window_manager* wm )
{
    return (int)wm->fonts[ font ].char_ysize * wm->font_zoom / 256;
}

int font_string_x_size( const char* str, int font, window_manager* wm )
{
    int rv = 0;
    int size = 0;
    while( *str )
    {
	uint32_t c32;
	if( *str == 0xA )
	{
	    if( size > rv ) rv = size;
	    size = 0;
	}
	str += utf8_to_utf32_char( str, &c32 );
	size += font_char_x_size( c32, font, wm );
    }
    if( size > rv ) rv = size;
    return rv;
}

int font_string_x_size_limited( const char* str, int limit, int font, window_manager* wm ) //limit in utf8 chars
{
    int size = 0;
    int ptr = 0;
    while( *str && ptr < limit )
    {
	uint32_t c32;
	int add = utf8_to_utf32_char( str, &c32 );
	str += add;
	ptr += add;
	size += font_char_x_size( c32, font, wm );
    }
    return size;
}

int font_string_y_size( const char* str, int font, window_manager* wm )
{
    int size = 0;
    bool new_line = true;
    while( *str )
    {
	if( *str == 0xA )
	{
	    new_line = true;
	}
	else
	{
	    if( new_line )
	    {
		size += font_char_y_size( font, wm );
		new_line = false;
	    }
	}
	str++;
    }
    return size;
}

int char_x_size( uint32_t c, window_manager* wm )
{
    if( !wm->cur_window ) return 0;
    int rv = ( font_char_x_size( c, wm->cur_window->font, wm ) * wm->cur_font_scale ) / 256;
    if( rv < 1 ) rv = 1;
    return rv;
}

int char_y_size( window_manager* wm )
{
    if( !wm->cur_window ) return 0;
    int rv = ( font_char_y_size( wm->cur_window->font, wm ) * wm->cur_font_scale ) / 256;
    if( rv < 1 ) rv = 1;
    return rv;
}

int string_x_size( const char* str, window_manager* wm )
{
    if( !wm->cur_window ) return 0;
    int rv = ( font_string_x_size( str, wm->cur_window->font, wm ) * wm->cur_font_scale ) / 256;
    if( rv < 1 ) rv = 1;
    return rv;
}

int string_x_size_limited( const char* str, int limit, window_manager* wm ) //limit in utf8 chars
{
    if( !wm->cur_window ) return 0;
    int rv = ( font_string_x_size_limited( str, limit, wm->cur_window->font, wm ) * wm->cur_font_scale ) / 256;
    if( rv < 1 ) rv = 1;
    return rv;
}

int string_y_size( const char* str, window_manager* wm )
{
    if( !wm->cur_window ) return 0;
    int rv = ( font_string_y_size( str, wm->cur_window->font, wm ) * wm->cur_font_scale ) / 256;
    if( rv < 1 ) rv = 1;
    return rv;
}
