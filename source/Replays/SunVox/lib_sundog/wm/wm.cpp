/*
    wm.cpp - SunDog window manager
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

//Modularity: 80% (need to remove global variables of some device-dependent modules)

#include "sundog.h"

const char* g_str_short_space = STR_SHORT_SPACE;
const char* g_str_hatching = STR_HATCHING;
const char* g_str_largest_char = STR_LARGEST_CHAR;
const char* g_str_up = STR_UP;
const char* g_str_down = STR_DOWN;
const char* g_str_left = STR_LEFT;
const char* g_str_right = STR_RIGHT;
const char* g_str_undo = STR_UNDO;
const char* g_str_redo = STR_REDO;
const char* g_str_home = STR_HOME;
const char* g_str_dash_begin = STR_DASH_BEGIN;
const char* g_str_dash_end = STR_DASH_END;
const char* g_str_small_dot = STR_SMALL_DOT;
const char* g_str_big_dot = STR_BIG_DOT;
const char* g_str_corner_small_lb = STR_CORNER_SMALL_LB;
const char* g_str_corner_small_lt = STR_CORNER_SMALL_LT;
const char* g_str_corner_lb = STR_CORNER_LB;
const char* g_str_corner_lt = STR_CORNER_LT;

//
// Device-dependent modules
//

#ifdef OS_WIN
    #include "wm_win.hpp"
#endif

#ifdef OS_WINCE
    #include "wm_wince.hpp"
#endif

#ifdef OS_UNIX
    #ifdef X11
	#include "wm_x11.hpp"
    #else
	#ifdef DIRECTDRAW
	    #include "wm_sdl.hpp"
	#endif
	#ifdef OS_IOS
	    #include "wm_ios.hpp"
	#endif
	#ifdef OS_MACOS
	    #include "wm_macos.hpp"
	#endif
	#ifdef OS_ANDROID
	    #include "wm_android.hpp"
	#endif
    #endif
#endif

void empty_device_event_handler( window_manager* wm )
{
    int pause = 1000 / wm->max_fps;
    if( wm->events_count )
        pause = 0;
    if( pause )
        stime_sleep( pause );
    stime_ticks_t cur_t = stime_ticks();
    if( cur_t - wm->prev_frame_time >= wm->ticks_per_frame )
    {
        wm->frame_event_request = 1;
        wm->prev_frame_time = cur_t;
    }
}
int empty_device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    wm->screen_xsize = 128;
    wm->screen_ysize = 128;
    wm->real_window_width = wm->screen_xsize;
    wm->real_window_height = wm->screen_ysize;
    win_angle_apply( wm );
    if( wm->screen_ppi == 0 ) wm->screen_ppi = 110;
    win_zoom_init( wm );

    wm->fb_xsize = wm->screen_xsize;
    wm->fb_ysize = wm->screen_ysize;
    wm->fb = (COLORPTR)smem_new( wm->fb_xsize * wm->fb_ysize * COLORLEN );
    wm->fb_offset = 0;
    wm->fb_ypitch = wm->fb_xsize;
    wm->fb_xpitch = 1;

    wm->prev_frame_time = 0;
    wm->frame_len = 1000 / wm->max_fps;
    wm->ticks_per_frame = stime_ticks_per_second() / wm->max_fps;
    return 0;
}
void empty_device_end( window_manager* wm )
{
    smem_free( wm->fb ); wm->fb = 0;
}
void empty_device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm )
{
}
void empty_device_draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
}
void empty_device_draw_image(
    int dest_x, int dest_y,
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm )
{
}
void empty_device_screen_lock( WINDOWPTR win, window_manager* wm )
{
}
void empty_device_screen_unlock( WINDOWPTR win, window_manager* wm )
{
}
void empty_device_vsync( bool vsync, window_manager* wm )
{
}
void empty_device_redraw_framebuffer( window_manager* wm )
{
    if( wm->screen_changed == 0 ) return;
    if( wm->exit_request ) return;
    wm->screen_changed = 0;
}
void empty_device_change_name( const char* name, window_manager* wm )
{
}

//
// Main functions
//

static void get_glfont_grid( int max_char_xsize, int max_char_ysize, int zoom, int& grid_cell_xsize, int& grid_cell_ysize, int& img_xsize, int& img_ysize )
{
    grid_cell_xsize = ( max_char_xsize * zoom ) + 3; // + additional pixels for correct OpenGL char drawing:
    grid_cell_ysize = ( max_char_ysize * zoom ) + 3; //   [ * [GLYPH] * 0 ]
    img_xsize = 16 * grid_cell_xsize;
    img_ysize = 16 * grid_cell_ysize;
    img_xsize = round_to_power_of_two( img_xsize );
    img_ysize = round_to_power_of_two( img_ysize );
}

static void load_font( int f, int zoom, uint8_t* tab, window_manager* wm )
{
    const FONT_LINE_TYPE* font_data = (const FONT_LINE_TYPE*)g_fonts[ f ];
    const FONT_SPX_TYPE* font_spx = (const FONT_SPX_TYPE*)g_fonts_spx[ f ]; //special pixels
    sdwm_font* font = &wm->fonts[ f ];
    font->data = font_data;
    font->img = NULL;
    for( int i = 0; i < 256; i++ ) font->char_xsize[ i ] = font_data[ 1 + i ];
    font->char_ysize = font_data[ 0 ];
#if defined(OPENGL)
    int max_char_xsize = sizeof( FONT_LINE_TYPE ) * 8;
    int max_char_ysize = font_data[ 0 ];
    if( !( wm->flags & WIN_INIT_FLAG_NO_FONT_UPSCALE ) && zoom > 1 )
    {
	for( int i = 0; i < 256; i++ ) font->char_xsize2[ i ] = font->char_xsize[ i ] * zoom;
	font->char_ysize2 = font->char_ysize * zoom;
	//
	font->grid_xoffset = 1;
	font->grid_yoffset = 1;
	int grid_cell_xsize = 0;
	int grid_cell_ysize = 0;
	int img_xsize = 0;
	int img_ysize = 0;
	get_glfont_grid( max_char_xsize, max_char_ysize, zoom, grid_cell_xsize, grid_cell_ysize, img_xsize, img_ysize );
	font->grid_cell_xsize = grid_cell_xsize;
	font->grid_cell_ysize = grid_cell_ysize;
	int tmp_char_xsize = max_char_xsize + 4; // [ 0 0 [GLYPH] 0 0 ]
	int tmp_char_ysize = max_char_ysize + 4; // ...
	int tmp_char2_xsize = ( max_char_xsize + 2 ) * zoom; // ZOOMED [ 0 [GLYPH] 0 ]
	int tmp_char2_ysize = ( max_char_ysize + 2 ) * zoom; // ...
	uint8_t* img = (uint8_t*)smem_znew( img_xsize * img_ysize );
	uint8_t* tmp_char = (uint8_t*)smem_znew( tmp_char_xsize * tmp_char_ysize );
	uint8_t* tmp_char2 = (uint8_t*)smem_znew( tmp_char2_xsize * tmp_char2_ysize );
	font_data += 257;
	int char_n = 0;
	for( int yy = 0; yy < grid_cell_ysize * 16; yy += grid_cell_ysize )
	{
    	    for( int xx = 0; xx < grid_cell_xsize * 16; xx += grid_cell_xsize )
    	    {
    		bool tmp_char_borders_changed = false;
    		//Fill the tmp_char[] image:
    		int tmp_char_ptr = tmp_char_xsize * 2 + 2;
		for( int y = 0; y < max_char_ysize; y++ )
		{
	    	    FONT_LINE_TYPE v = *font_data;
	    	    for( int x = 0; x < max_char_xsize; x++ )
	    	    {
		        tmp_char[ tmp_char_ptr ] = v & FONT_LEFT_PIXEL ? 255 : 0;
			v <<= 1;
			tmp_char_ptr++;
		    }
		    tmp_char_ptr += 4;
		    font_data++;
		}
		//Fix special pixels:
		while( *font_spx == char_n )
		{
		    font_spx++;
		    uint32_t xy = *font_spx;
		    font_spx++;
		    int sx = xy >> 4;
		    int sy = xy & 15;
		    tmp_char[ ( sy + 2 ) * tmp_char_xsize + sx + 2 ] = 128; //special pixel (see upscaling algorithm below)
		}
		//Fix special chars:
		if( char_n == STR_HOME[0] || char_n == STR_DASH_BEGIN[0] )
		{
		    //CUR | <- | NEXT
    		    tmp_char_ptr = tmp_char_xsize * 2 + 2 + max_char_xsize;
		    for( int y = 0; y < max_char_ysize; y++ )
		    {
	    		FONT_LINE_TYPE v = font_data[ y ]; //line of the next char
		    	tmp_char[ tmp_char_ptr ] = v & FONT_LEFT_PIXEL ? 255 : 0;
			tmp_char_ptr += tmp_char_xsize;
		    }
		    tmp_char_borders_changed = true;
		}
		if( char_n == STR_HOME[1] || char_n == STR_DASH_END[0] )
		{
		    //PREV | -> | CUR
    		    tmp_char_ptr = tmp_char_xsize * 2 + 1;
		    for( int y = 0; y < max_char_ysize; y++ )
		    {
			FONT_LINE_TYPE v = *( font_data - max_char_ysize*2 + y ); //line of the prev char
		    	tmp_char[ tmp_char_ptr ] = v & ( FONT_LEFT_PIXEL >> (max_char_xsize-1) ) ? 255 : 0;
			tmp_char_ptr += tmp_char_xsize;
		    }
		    tmp_char_borders_changed = true;
		}
		if( char_n == STR_HATCHING[0] )
		{
		    for( int y = 2; y < 2 + max_char_ysize; y++ )
		    {
			int p = y * tmp_char_xsize + 1;
			tmp_char[ p ] = tmp_char[ p + max_char_xsize ];
			tmp_char[ p + max_char_xsize + 1 ] = tmp_char[ p + 1 ];
		    }
		    for( int x = 0; x < tmp_char_xsize; x++ )
		    {
			int p = tmp_char_xsize + x;
			tmp_char[ p ] = tmp_char[ p + tmp_char_xsize * max_char_ysize ];
			tmp_char[ p + tmp_char_xsize * ( max_char_ysize + 1 ) ] = tmp_char[ p + tmp_char_xsize ];
		    }
		    tmp_char_borders_changed = true;
		}
		//tmp_char[] -> upscale -> tmp_char2[]:
    		tmp_char_ptr = tmp_char_xsize + 1;
		for( int y = 0; y < max_char_ysize + 2; y++ )
		{
    		    int tmp_char2_ptr = y * zoom * tmp_char2_xsize;
	    	    for( int x = 0; x < max_char_xsize + 2; x++ )
	    	    {
	    		bool c0 = tmp_char[ tmp_char_ptr - tmp_char_xsize - 1 ] == 255;
	    		bool c1 = tmp_char[ tmp_char_ptr - tmp_char_xsize ] == 255;
	    		bool c2 = tmp_char[ tmp_char_ptr - tmp_char_xsize + 1 ] == 255;
	    		bool c3 = tmp_char[ tmp_char_ptr - 1 ] == 255;
	    		bool c4 = tmp_char[ tmp_char_ptr ] > 0;
	    		bool c5 = tmp_char[ tmp_char_ptr + 1 ] == 255;
	    		bool c6 = tmp_char[ tmp_char_ptr + tmp_char_xsize - 1 ] == 255;
	    		bool c7 = tmp_char[ tmp_char_ptr + tmp_char_xsize ] == 255;
	    		bool c8 = tmp_char[ tmp_char_ptr + tmp_char_xsize + 1 ] == 255;
	    		int out = 0;
	    		//out bits: 0 1
	    		//          2 3
	    		if( c4 )
	    		    out = 15;
	    		else
	    		{
	    		    if( !( c2 && c6 ) )
	    		    {
	    			if( c1 && c3 ) out |= 1 << 0;
	                	if( c5 && c7 ) out |= 1 << 3;
	            	    }
	            	    if( !( c0 && c8 ) )
	            	    {
    	        		if( c1 && c5 ) out |= 1 << 1;
        	        	if( c3 && c7 ) out |= 1 << 2;
        	    	    }
	    		}
	    		int tp = out * zoom * zoom;
	    		for( int ty = 0; ty < zoom; ty++ )
	    		{
	    		    for( int tx = 0; tx < zoom; tx++ )
	    		    {
	    			tmp_char2[ tmp_char2_ptr ] = tab[ tp ];
	    			tp++;
	    			tmp_char2_ptr++;
			    }
			    tmp_char2_ptr += tmp_char2_xsize - zoom;
	    		}
	    		tmp_char2_ptr -= tmp_char2_xsize * zoom - zoom;
			tmp_char_ptr++;
	    	    }
		    tmp_char_ptr += 2;
	    	}
	    	/*if( char_n == 'A' )
	    	{
	    	    int pp = 0;
	    	    for( int y = 0; y < tmp_char2_ysize; y++ )
	    	    {
	    		for( int x = 0; x < tmp_char2_xsize; x++ )
	    		{
	    		    printf( "%c", tmp_char2[ pp ] ? '#' : '.' );
	    		    pp++;
			}
	    	        printf( "\n" );
		    }
	    	    printf( "\n" );
		}*/
		//tmp_char2[] -> img[]:
    		int tmp_char2_ptr = tmp_char2_xsize * (zoom-1) + (zoom-1);
    		int img_ptr = yy * img_xsize + xx;
		for( int y = 0; y < max_char_ysize * zoom + 2; y++ )
		{
	    	    for( int x = 0; x < max_char_xsize * zoom + 2; x++ )
	    	    {
	    		img[ img_ptr ] = tmp_char2[ tmp_char2_ptr ];
	    		img_ptr++;
	    		tmp_char2_ptr++;
	    	    }
	    	    img_ptr += img_xsize - ( max_char_xsize * zoom + 2 );
		    tmp_char2_ptr += (zoom-1) * 2;
	    	}
	    	//
		if( tmp_char_borders_changed )
		{
		    smem_zero( tmp_char );
		}
	    	char_n++;
    	    }
    	}
	font->img = new_image( img_xsize, img_ysize, img, img_xsize, img_ysize, IMAGE_ALPHA8 | IMAGE_INTERP, wm );
	smem_free( img );
	smem_free( tmp_char );
	smem_free( tmp_char2 );
    }
    else
    {
	for( int i = 0; i < 256; i++ ) font->char_xsize2[ i ] = font->char_xsize[ i ];
	font->char_ysize2 = font->char_ysize;
	//
	font->grid_xoffset = 0;
	font->grid_yoffset = 0;
	font->grid_cell_xsize = max_char_xsize + 1; // + additional pixel for correct OpenGL char drawing
	font->grid_cell_ysize = max_char_ysize + 1; //...
	int img_xsize = 16 * font->grid_cell_xsize;
	int img_ysize = 16 * font->grid_cell_ysize;
	img_xsize = round_to_power_of_two( img_xsize );
	img_ysize = round_to_power_of_two( img_ysize );
	uint8_t* img = (uint8_t*)smem_znew( img_xsize * img_ysize );
	font_data += 257;
	int grid_cell_xsize = font->grid_cell_xsize;
	int grid_cell_ysize = font->grid_cell_ysize;
	for( int yy = 0; yy < grid_cell_ysize * 16; yy += grid_cell_ysize )
	{
    	    for( int xx = 0; xx < grid_cell_xsize * 16; xx += grid_cell_xsize )
    	    {
    		int img_ptr = yy * img_xsize + xx;
		for( int fc = 0; fc < max_char_ysize; fc++ )
		{
	    	    FONT_LINE_TYPE v = *font_data;
	    	    for( int x = 0; x < max_char_xsize; x++ )
	    	    {
	    		if( v & FONT_LEFT_PIXEL )
		    	    img[ img_ptr ] = 255;
			v <<= 1;
			img_ptr++;
		    }
		    img_ptr += img_xsize - max_char_xsize;
		    font_data++;
		}
	    }
	}
	font->img = new_image( img_xsize, img_ysize, img, img_xsize, img_ysize, IMAGE_ALPHA8, wm );
	smem_free( img );
    }
#endif
}

static void load_fonts( window_manager* wm )
{
    uint8_t* tab = NULL;
    int zoom = 1;

#if defined(OPENGL)
    int biggest_char_xsize = 8;
    int biggest_char_ysize = 0;
    for( int f = 0; f < WM_FONTS; f++ )
    {
	const FONT_LINE_TYPE* font_data = (const FONT_LINE_TYPE*)g_fonts[ f ];
	if( font_data[ 0 ] > biggest_char_ysize ) biggest_char_ysize = font_data[ 0 ];
    }
    zoom = wm->font_zoom / 256;
    if( wm->font_zoom & 255 )
    {
	if( biggest_char_ysize * zoom != biggest_char_ysize * wm->font_zoom / 256 )
	    zoom++;
    }

    int glfont_texture_limit = 2048;
#ifdef OPENGLES
    glfont_texture_limit = 1024;
#endif
    while( 1 )
    {
	int grid_cell_xsize = 0;
	int grid_cell_ysize = 0;
	int img_xsize = 0;
	int img_ysize = 0;
	get_glfont_grid( biggest_char_xsize, biggest_char_ysize, zoom, grid_cell_xsize, grid_cell_ysize, img_xsize, img_ysize );
	if( zoom >= 2 && ( img_xsize > glfont_texture_limit || img_ysize > glfont_texture_limit ) )
	    zoom--;
	else
	    break;
    }

    if( !( wm->flags & WIN_INIT_FLAG_NO_FONT_UPSCALE ) && zoom > 1 )
    {
	int zoom2 = zoom * zoom;
	tab = (uint8_t*)smem_znew( zoom2 * 16 );
	//Prepare the table:
	for( int i = 1; i < 15; i++ )
	{
	    if( i & ( 1 << 0 ) )
	    {
		// # -
	    	// - -
		int p = i * zoom2;
		for( int y = 0; y < zoom; y++ )
		    for( int x = 0; x < zoom; x++ )
		    {
			if( x < zoom - y - 1 ) tab[ p ] = 255;
			p++;
		    }
	    }
	    if( i & ( 1 << 1 ) )
	    {
		// - #
	    	// - -
		int p = i * zoom2;
		for( int y = 0; y < zoom; y++ )
		    for( int x = 0; x < zoom; x++ )
		    {
			if( x > y ) tab[ p ] = 255;
			p++;
		    }
	    }
	    if( i & ( 1 << 2 ) )
	    {
		// - -
	    	// # -
		int p = i * zoom2;
		for( int y = 0; y < zoom; y++ )
		    for( int x = 0; x < zoom; x++ )
		    {
			if( x < y ) tab[ p ] = 255;
			p++;
		    }
	    }
	    if( i & ( 1 << 3 ) )
	    {
		// - -
	    	// - #
		int p = i * zoom2;
		for( int y = 0; y < zoom; y++ )
		    for( int x = 0; x < zoom; x++ )
		    {
			if( x >= zoom - y ) tab[ p ] = 255;
			p++;
		    }
	    }
	    /*{
		int p = i * zoom2;
		printf("  >>> %d:\n", i );
		for( int y = 0; y < zoom; y++ )
		{
		    for( int x = 0; x < zoom; x++ )
		    {
			printf( "%02x ", tab[ p++ ] );
		    }
		    printf("\n");
		}
	    }*/
	}
	for( int i = 15 * zoom2; i < 16 * zoom2; i++ ) tab[ i ] = 255;
    }
#endif

    for( int f = 0; f < WM_FONTS; f++ ) load_font( f, zoom, tab, wm );

    smem_free( tab );
}

static void unload_font( int f, window_manager* wm )
{
    sdwm_font* font = &wm->fonts[ f ];
    remove_image( font->img );
} 

static void unload_fonts( window_manager* wm )
{
    for( int f = 0; f < WM_FONTS; f++ ) unload_font( f, wm );
}

int win_init( const char* name, int xsize, int ysize, uint flags, sundog_engine* sd )
{
    int retval = 0;

    //FIRST STEP: set defaults

    window_manager* wm = &sd->wm;
    smem_clear( wm, sizeof( window_manager ) );
    wm->sd = sd;

    wm->vcap = 0;
    wm->vcap_in_fps = 30;
    wm->vcap_in_bitrate_kb = 1000;
    wm->vcap_in_flags = 0;
    wm->vcap_in_name = 0;

    smem_clear( &wm->frame_evt, sizeof( wm->frame_evt ) );
    smem_clear( &wm->empty_evt, sizeof( wm->empty_evt ) );
    wm->frame_evt.type = EVT_FRAME;
    smutex_init( &wm->events_mutex, 0 );

#ifdef OPENGL
    smutex_init( &wm->gl_mutex, 0 );
#endif

    wm->pen_x = -1;
    wm->pen_y = -1;
    wm->status_timer = -1;
    wm->autorepeat_timer = -1;
    wm->km = keymap_new( wm );
    wm->prefs_section_ysize = 200;

    //Load settings from configuration file:

    name = (const char*)sconfig_get_str_value( APP_CFG_WINDOWNAME, name, 0 );
    int angle = sconfig_get_int_value( APP_CFG_ROTATE, 0, 0 );
    if( angle > 0 ) flags |= ( ( angle / 90 ) & 3 ) << WIN_INIT_FLAGOFF_ANGLE;
    int zoom = sconfig_get_int_value( APP_CFG_ZOOM, 0, 0 );
    if( zoom > 0 ) flags |= ( zoom & 7 ) << WIN_INIT_FLAGOFF_ZOOM;
    if( sconfig_get_int_value( APP_CFG_FULLSCREEN, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_FULLSCREEN;
    if( sconfig_get_int_value( APP_CFG_MAXIMIZED, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_MAXIMIZED;
    if( sconfig_get_int_value( APP_CFG_VSYNC, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_VSYNC;
#ifdef FRAMEBUFFER
    flags |= WIN_INIT_FLAG_FRAMEBUFFER;
#endif
    int fb = sconfig_get_int_value( APP_CFG_FRAMEBUFFER, -1, 0 );
    if( fb == 1 ) flags |= WIN_INIT_FLAG_FRAMEBUFFER;
    if( fb == 0 ) flags &= ~WIN_INIT_FLAG_FRAMEBUFFER;
    flags |= WIN_INIT_FLAG_OPTIMIZE_MOVE_EVENTS;
    if( sconfig_get_int_value( APP_CFG_NOWINDOW, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_NOWINDOW;
    if( sconfig_get_int_value( APP_CFG_NOBORDER, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_NOBORDER;
    if( sconfig_get_int_value( APP_CFG_NOCURSOR, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_NOCURSOR;
    if( sconfig_get_int_value( APP_CFG_NOSYSBARS, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_NOSYSBARS;
    if( sconfig_get_int_value( APP_CFG_NO_FONT_UPSCALE, -1, 0 ) != -1 ) flags |= WIN_INIT_FLAG_NO_FONT_UPSCALE;

    //Save flags & name:
    wm->flags = flags;
    wm->name = smem_strdup( name );

    wm->screen_buffer_preserved = 1;
#if defined(OPENGL) || defined(OPENGLES)
    #ifdef GLNORETAIN
        wm->screen_buffer_preserved = 0;
    #else
        wm->screen_buffer_preserved = 1;
    #endif
#endif
    wm->screen_buffer_preserved = sconfig_get_int_value( APP_CFG_SCREENBUF_SWAP_BEHAVIOR, wm->screen_buffer_preserved, 0 );
    wm->screen_angle = ( flags >> WIN_INIT_FLAGOFF_ANGLE ) & 3;
    wm->screen_zoom = ( flags >> WIN_INIT_FLAGOFF_ZOOM ) & 7;
    if( wm->screen_zoom == 0 ) wm->screen_zoom = 1;
#ifndef SCREEN_ROTATE_SUPPORTED
    wm->screen_angle = 0;
#endif
#ifndef SCREEN_ZOOM_SUPPORTED
    wm->screen_zoom = 1;
#endif
    wm->screen_ppi = sconfig_get_int_value( APP_CFG_PPI, 0, 0 ); //Default - 0 (unknown)
    wm->screen_scale = (float)sconfig_get_int_value( APP_CFG_SCALE, 0, 0 ) / 256.0F; //Default - 0 (unknown)
    wm->screen_font_scale = (float)sconfig_get_int_value( APP_CFG_FONT_SCALE, 0, 0 ) / 256.0F; //Default - 0 (unknown)
    wm->screen_lock_counter = 0;
    wm->screen_is_active = false;
    wm->screen_changed = 0;

    wm->show_virtual_keyboard = false;
#ifdef VIRTUALKEYBOARD
    wm->show_virtual_keyboard = true;
#endif
    if( sconfig_get_int_value( APP_CFG_SHOW_VIRT_KBD, -1, 0 ) != -1 )
	wm->show_virtual_keyboard = true;
    if( sconfig_get_int_value( APP_CFG_HIDE_VIRT_KBD, -1, 0 ) != -1 )
	wm->show_virtual_keyboard = false;

#ifdef MULTITOUCH
    wm->show_zoom_buttons = false;
#else
    wm->show_zoom_buttons = true;
#endif
    int show = sconfig_get_int_value( APP_CFG_SHOW_ZOOM_BUTTONS, -1, 0 );
    if( show == 1 ) wm->show_zoom_buttons = true;
    if( show == 0 ) wm->show_zoom_buttons = false;

    wm->double_click_time = sconfig_get_int_value( APP_CFG_DOUBLECLICK, DEFAULT_DOUBLE_CLICK_TIME, 0 );
    wm->kbd_autorepeat_delay = sconfig_get_int_value( APP_CFG_KBD_AUTOREPEAT_DELAY, DEFAULT_KBD_AUTOREPEAT_DELAY, 0 );
    wm->kbd_autorepeat_freq = sconfig_get_int_value( APP_CFG_KBD_AUTOREPEAT_FREQ, DEFAULT_KBD_AUTOREPEAT_FREQ, 0 );
    wm->mouse_autorepeat_delay = sconfig_get_int_value( APP_CFG_MOUSE_AUTOREPEAT_DELAY, DEFAULT_MOUSE_AUTOREPEAT_DELAY, 0 );
    wm->mouse_autorepeat_freq = sconfig_get_int_value( APP_CFG_MOUSE_AUTOREPEAT_FREQ, DEFAULT_MOUSE_AUTOREPEAT_FREQ, 0 );

    wm->font_medium_mono = sconfig_get_int_value( APP_CFG_FONT_MEDIUM_MONO, DEFAULT_FONT_MEDIUM_MONO, 0 );
    wm->font_big = sconfig_get_int_value( APP_CFG_FONT_BIG, DEFAULT_FONT_BIG, 0 );
    wm->font_small = sconfig_get_int_value( APP_CFG_FONT_SMALL, DEFAULT_FONT_SMALL, 0 );
    wm->default_font = wm->font_big;

    //SECOND STEP: SCREEN SIZE SETTING and device specific init
    slog( "WM: initializing...\n" );

device_restart:
    int err;
    wm->device_event_handler = device_event_handler;
    wm->device_start = device_start;
    wm->device_end = device_end;
#ifdef OPENGL
    wm->device_draw_line = gl_draw_line;
    wm->device_draw_frect = gl_draw_frect;
    wm->device_draw_image = gl_draw_image;
#else
    wm->device_draw_line = device_draw_line;
    wm->device_draw_frect = device_draw_frect;
    wm->device_draw_image = device_draw_image;
#endif
    wm->device_screen_lock = device_screen_lock;
    wm->device_screen_unlock = device_screen_unlock;
    wm->device_vsync = device_vsync;
    wm->device_redraw_framebuffer = device_redraw_framebuffer;
    wm->device_change_name = device_change_name;
    if( flags & WIN_INIT_FLAG_FRAMEBUFFER )
    {
	wm->device_draw_line = fb_draw_line;
	wm->device_draw_frect = fb_draw_frect;
	wm->device_draw_image = fb_draw_image;
    }
    if( flags & WIN_INIT_FLAG_NOWINDOW )
    {
	wm->device_event_handler = empty_device_event_handler;
	wm->device_start = empty_device_start;
	wm->device_end = empty_device_end;
	wm->device_draw_line = empty_device_draw_line;
	wm->device_draw_frect = empty_device_draw_frect;
	wm->device_draw_image = empty_device_draw_image;
	wm->device_screen_lock = empty_device_screen_lock;
	wm->device_screen_unlock = empty_device_screen_unlock;
	wm->device_vsync = empty_device_vsync;
	wm->device_redraw_framebuffer = empty_device_redraw_framebuffer;
	wm->device_change_name = empty_device_change_name;
    }

    wm->max_fps = 60;
#if CPUMARK < 10 && !defined(OPENGL)
    wm->max_fps = 30;
#endif
#if defined(MAX_FPS)
    wm->max_fps = MAX_FPS;
#endif
    wm->max_fps = sconfig_get_int_value( APP_CFG_MAXFPS, wm->max_fps, 0 );

    wm->fb_offset = 0;
    wm->fb_xpitch = 1;
    wm->fb_ypitch = 0;
    
    err = wm->device_start( name, xsize, ysize, wm ); //DEVICE DEPENDENT PART
    if( err )
    {
	slog( "Error in device_start() %d\n", err );
#if defined(OS_LINUX) || defined(SUNDOG_MODULE)
	if( ( flags & WIN_INIT_FLAG_NOWINDOW ) == 0 )
	{
	    flags |= WIN_INIT_FLAG_NOWINDOW;
	    goto device_restart;
	}
#endif
	return 1; //Error
    }

    if( wm->screen_ppi == 0 ) wm->screen_ppi = 110;
    if( wm->screen_zoom > 1 ) wm->screen_ppi /= wm->screen_zoom;
    if( wm->screen_scale == 0 ) wm->screen_scale = 1;
    if( wm->screen_font_scale == 0 ) wm->screen_font_scale = 1;
    if( wm->font_zoom == 0 ) wm->font_zoom = 256;

    if( sconfig_get_int_value( APP_CFG_TOUCHCONTROL, -1, 0 ) != -1 ) wm->flags |= WIN_INIT_FLAG_TOUCHCONTROL;
    if( sconfig_get_int_value( APP_CFG_PENCONTROL, -1, 0 ) != -1 ) wm->flags &= ~WIN_INIT_FLAG_TOUCHCONTROL;
    if( wm->flags & WIN_INIT_FLAG_TOUCHCONTROL )
	wm->control_type = TOUCHCONTROL;
    else
	wm->control_type = PENCONTROL;
    if( wm->flags & WIN_INIT_FLAG_VSYNC )
	win_vsync( 1, wm );
    else
	win_vsync( 0, wm );

    slog( "WM: %d x %d; PPI %d; lang %s\n", wm->screen_xsize, wm->screen_ysize, wm->screen_ppi, slocale_get_lang() );
    if( wm->screen_angle ) slog( "WM: screen_angle = %d\n", wm->screen_angle * 90 );
    if( wm->screen_zoom > 1 ) slog( "WM: screen_zoom = %d\n", wm->screen_zoom );
    if( wm->screen_scale != 1 ) slog( "WM: screen_scale = %d / 256\n", (int)( wm->screen_scale * 256 ) );
    if( wm->screen_font_scale != 1 ) slog( "WM: screen_font_scale = %d / 256\n", (int)( wm->screen_font_scale * 256 ) );
    if( wm->font_zoom != 256 ) slog( "WM: font_zoom = %d\n", wm->font_zoom );
    if( wm->flags & ~( ( 3 << WIN_INIT_FLAGOFF_ANGLE ) | ( 7 << WIN_INIT_FLAGOFF_ZOOM ) ) )
    {
	slog( "WM: flags " );
	if( wm->flags & WIN_INIT_FLAG_SCALABLE ) slog( "SCALABLE " );
	if( wm->flags & WIN_INIT_FLAG_NOBORDER ) slog( "NOBORDER " );
	if( wm->flags & WIN_INIT_FLAG_FULLSCREEN ) slog( "FULLSCREEN " );
	if( wm->flags & WIN_INIT_FLAG_MAXIMIZED ) slog( "MAXIMIZED " );
	if( wm->flags & WIN_INIT_FLAG_TOUCHCONTROL ) slog( "TOUCHCONTROL " );
	if( wm->flags & WIN_INIT_FLAG_OPTIMIZE_MOVE_EVENTS ) slog( "OPTIMIZE_MOVE_EVENTS " );
	if( wm->flags & WIN_INIT_FLAG_NOSCREENROTATE ) slog( "NOSCREENROTATE " );
	if( wm->flags & WIN_INIT_FLAG_LANDSCAPE_ONLY ) slog( "LANDSCAPE_ONLY " );
	if( wm->flags & WIN_INIT_FLAG_PORTRAIT_ONLY ) slog( "PORTRAIT_ONLY " );
	if( wm->flags & WIN_INIT_FLAG_NOCURSOR ) slog( "NOCURSOR " );
	if( wm->flags & WIN_INIT_FLAG_VSYNC ) slog( "VSYNC " );
	if( wm->flags & WIN_INIT_FLAG_FRAMEBUFFER ) slog( "FRAMEBUFFER " );
	if( wm->flags & WIN_INIT_FLAG_NOWINDOW ) slog( "NOWINDOW " );
	if( wm->flags & WIN_INIT_FLAG_NOSYSBARS ) slog( "NOSYSBARS " );
	if( wm->flags & WIN_INIT_FLAG_NO_FONT_UPSCALE ) slog( "NO_FONT_UPSCALE " );
	slog( "\n" );
    }

    load_fonts( wm );

    {
	int size, min;

	int biggest_font = FONT_BIG; //for UI elements with variable font size

	//!!!
	//For future updates:
	//some of the following values must be dynamically calculated in the UI element code itself
	//(only for UI elements with variable font size)

	for( int i = 0; i < 2; i++ )
	{
	    min = 18; //smallest icon size = 16
	    if( min < font_char_y_size( biggest_font, wm ) + 2 )
		min = font_char_y_size( biggest_font, wm ) + 2;
	    float scrollbar_size = (float)wm->screen_ppi * SCROLLBAR_SIZE_COEFF;
	    size = (int)( scrollbar_size * wm->screen_scale );
	    if( size < min ) size = min;
	    wm->scrollbar_size = size;

	    int scrsize = wm->screen_xsize;
	    if( wm->screen_ysize < scrsize ) scrsize = wm->screen_ysize;
	    if( scrsize <= 32 ) break; //bad screen size
	    const int max_xbuttons = 6;
	    if( wm->scrollbar_size * max_xbuttons > scrsize )
	    {
		slog( "WM: PPI or Scale are too big! Scale will be reduced.\n" );
		wm->screen_scale = (float)scrsize / ( scrollbar_size * max_xbuttons );
		if( wm->font_zoom > 256 )
		{
		    wm->font_zoom /= 2;
		    unload_fonts( wm );
		    load_fonts( wm );
		}
	    }
	    else
	    {
		break;
	    }
	}

	min = 1;
	size = ( wm->screen_ppi * 256 ) / 100;
	size += 250; // += 0.97
	size /= 256;
	//100:1; 110:2; 160:2; 480:5...
	if( size < min ) size = min;
	wm->interelement_space = size;

	min = 1;
	size = wm->interelement_space / 2;
	if( size < min ) size = min;
	wm->interelement_space2 = size;

	min = 4;
    	size = (int)( (float)wm->screen_ppi * 0.07 * wm->screen_scale );
    	if( wm->control_type == TOUCHCONTROL ) size /= 2;
	if( size < min ) size = min;
	wm->decor_border_size = size;

	min = font_char_y_size( wm->default_font, wm ) + wm->interelement_space2 * 2;
	size = (int)( (float)wm->screen_ppi * 0.175 * wm->screen_scale );
	if( size < min ) size = min;
	wm->decor_header_size = size;

	min = font_string_x_size( "#", 1, wm ) + wm->interelement_space2 * 2;
        size = (int)( (float)wm->scrollbar_size * 0.8 * wm->screen_scale );
        if( size < min ) size = min;
        wm->small_button_xsize = size;

	min = wm->scrollbar_size * 2;
	size = (int)( (float)font_string_x_size( "Button ||23", biggest_font, wm ) );
	if( size < min ) size = min;
	wm->button_xsize = size;

	min = font_char_y_size( biggest_font, wm ) * 2;
	size = (int)( (float)wm->screen_ppi * BIG_BUTTON_YSIZE_COEFF * wm->screen_scale );
	if( size < min ) size = min;
	wm->button_ysize = size;

	wm->normal_window_xsize = (int)( (float)wm->button_xsize * 4.5 );
	wm->normal_window_ysize = ( wm->normal_window_xsize * 11 ) / 16;
	wm->large_window_xsize = (int)( (float)wm->button_xsize * 7 );
	wm->large_window_ysize = ( wm->large_window_xsize * 11 ) / 16;

	min = font_char_y_size( wm->default_font, wm );
	size = (int)( (float)wm->screen_ppi * 0.14 * wm->screen_scale );
	if( size < min ) size = min;
	wm->list_item_ysize = size;

	min = font_char_y_size( biggest_font, wm ) + wm->interelement_space2 * 2;
	size = (int)( (float)wm->screen_ppi * TEXT_YSIZE_COEFF * wm->screen_scale );
	if( size < min ) size = min;
	wm->text_ysize = size;

	min = font_char_y_size( wm->font_small, wm ) * 2 + wm->interelement_space2 * 2;
	size = (int)( (float)wm->screen_ppi * POPUP_BUTTON_YSIZE_COEFF * wm->screen_scale );
	if( size < min ) size = min;
	wm->popup_button_ysize = size;

	min = font_char_y_size( wm->font_small, wm ) * 2 + wm->interelement_space2 * 2;
	size = (int)( (float)wm->screen_ppi * CONTROLLER_YSIZE_COEFF * wm->screen_scale );
	if( size < min ) size = min;
	wm->controller_ysize = size;
	wm->text_with_label_ysize = size;

	wm->corners_size = (int)( (float)( 2.0F * (float)wm->screen_ppi * wm->screen_scale ) / 160.0 + 0.5 );
        wm->corners_len = (int)( (float)( 4.0F * (float)wm->screen_ppi * wm->screen_scale ) / 160.0 + 0.5 );
        if( wm->corners_size < 2 ) wm->corners_size = 2;
        if( wm->corners_len < 4 ) wm->corners_len = 4;
    }

    wm->color_theme = sconfig_get_int_value( APP_CFG_COLOR_THEME, 3, 0 );
    win_colors_init( wm );

    slog( "WM: ready\n" );

    COMPILER_MEMORY_BARRIER();
    wm->initialized = true;

    return retval;
}

void win_change_name( const char* name, window_manager* wm )
{
    smem_free( wm->name2 );
    wm->name2 = smem_strdup( name );
    if( wm->name2 )
    {
	wm->device_change_name( wm->name2, wm );
    }
    else
    {
	wm->device_change_name( wm->name, wm );
    }
}

void win_colors_init( window_manager* wm )
{
    wm->custom_colors = sconfig_get_str_value( APP_CFG_CUSTOM_COLORS, 0, 0 );

    COLOR cc[ 4 ];
    for( int c = 0; c < 4; c++ )
    {
	cc[ c ] = colortheme_get_color( wm->color_theme, c, wm );
    }
    wm->color0 = cc[ 0 ];
    wm->color1 = cc[ 1 ];
    wm->color2 = cc[ 2 ];
    wm->color3 = cc[ 3 ];
    wm->green = get_color( 0, 255, 0 );
    wm->yellow = get_color( 255, 255, 0 );
    wm->red = get_color( 255, 0, 0 );
    wm->blue = get_color( 0, 0, 255 );

    if( wm->custom_colors )
    {
	char* v;
	v = sconfig_get_str_value( "c_0", 0, 0 ); if( v ) wm->color0 = get_color_from_string( v );
	v = sconfig_get_str_value( "c_1", 0, 0 ); if( v ) wm->color1 = get_color_from_string( v );
	v = sconfig_get_str_value( "c_2", 0, 0 ); if( v ) wm->color2 = get_color_from_string( v );
	v = sconfig_get_str_value( "c_3", 0, 0 ); if( v ) wm->color3 = get_color_from_string( v );
	v = sconfig_get_str_value( "c_yellow", 0, 0 ); if( v ) wm->yellow = get_color_from_string( v );
	v = sconfig_get_str_value( "c_green", 0, 0 ); if( v ) wm->green = get_color_from_string( v );
	v = sconfig_get_str_value( "c_red", 0, 0 ); if( v ) wm->red = get_color_from_string( v );
	v = sconfig_get_str_value( "c_blue", 0, 0 ); if( v ) wm->blue = get_color_from_string( v );
    }

    if( red( wm->color3 ) + green( wm->color3 ) + blue( wm->color3 ) >= red( wm->color0 ) + green( wm->color0 ) + blue( wm->color0 ) )
    {
	wm->color3_is_brighter = 1;
    }
    else 
    {
	wm->color3_is_brighter = 0;
    }

    int v1 = 255 - red( wm->color1 );
    if( v1 < 0 ) v1 = -v1;
    int v2 = 255 - green( wm->color1 );
    if( v2 < 0 ) v2 = -v2;
    int v3 = 255 - blue( wm->color1 );
    if( v3 < 0 ) v3 = -v3;
    wm->color1_darkness = ( v1 + v2 + v3 ) / 3;

    wm->header_text_color = wm->color2;
    wm->alternative_text_color = wm->yellow;
    if( wm->color1_darkness < 64 )
    {
        wm->alternative_text_color = blend( wm->alternative_text_color, wm->color3, 140 );
    }
    wm->dialog_color = wm->color1;
    wm->decorator_color = wm->color1;
    wm->decorator_border = wm->color2;
    wm->button_color = blend( wm->color1, wm->color3, 12 );
    wm->pressed_button_color = wm->color3;
    wm->pressed_button_color_opacity = 40;
    wm->menu_color = wm->color1;
    wm->selection_color = blend( get_color( 255, 255, 255 ), wm->green, 145 );
    wm->text_background = wm->color1;
    wm->list_background = wm->color1;
    wm->scroll_color = wm->button_color;
    wm->scroll_background_color = wm->color1;
    wm->scroll_pressed_color = wm->green;
    wm->scroll_pressed_color_opacity = 64;

    if( wm->custom_colors )
    {
	char* v;
	v = sconfig_get_str_value( "c_header", 0, 0 ); if( v ) wm->header_text_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_alt", 0, 0 ); if( v ) wm->alternative_text_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_dlg", 0, 0 ); if( v ) wm->dialog_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_dec", 0, 0 ); if( v ) wm->decorator_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_decbord", 0, 0 ); if( v ) wm->decorator_border = get_color_from_string( v );
	v = sconfig_get_str_value( "c_btn", 0, 0 ); if( v ) wm->button_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_pbtn", 0, 0 ); if( v ) wm->pressed_button_color = get_color_from_string( v );
	wm->pressed_button_color_opacity = sconfig_get_int_value( "c_pbtn_op", wm->pressed_button_color_opacity, 0 );
	v = sconfig_get_str_value( "c_menu", 0, 0 ); if( v ) wm->menu_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_sel", 0, 0 ); if( v ) wm->selection_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_txtback", 0, 0 ); if( v ) wm->text_background = get_color_from_string( v );
	v = sconfig_get_str_value( "c_lstback", 0, 0 ); if( v ) wm->list_background = get_color_from_string( v );
	v = sconfig_get_str_value( "c_scroll", 0, 0 ); if( v ) wm->scroll_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_scrollback", 0, 0 ); if( v ) wm->scroll_background_color = get_color_from_string( v );
	v = sconfig_get_str_value( "c_pscroll", 0, 0 ); if( v ) wm->scroll_pressed_color = get_color_from_string( v );
	//wm->scroll_pressed_color_opacity = sconfig_get_int_value( "c_pscroll_op", wm->scroll_pressed_color_opacity, 0 ); //255 will hide some details in the standard scrollbar?
    }
}

void win_reinit( window_manager* wm )
{
    win_colors_init( wm );

    smem_clear( wm->timers, sizeof( wm->timers ) );
    wm->timers_num = 0;
    wm->timers_id_counter = 0;

    wm->handler_of_unhandled_events = 0;

    wm->restart_request = 0;
    wm->exit_request = 0;
}

int win_calc_font_zoom( int screen_ppi, int screen_zoom, float screen_scale, float screen_font_scale )
{
    int font_zoom = 256;
    int fixed_ppi = sconfig_get_int_value( "fixedppi", 0, 0 );
    bool int_scaling = false;
#ifndef OPENGL
    int_scaling = true;
#endif
    int ifs = sconfig_get_int_value( "int_font_scaling", -1, 0 );
    if( ifs != -1 )
    {
	if( ifs == 1 ) int_scaling = true;
	if( ifs == 0 ) int_scaling = false;
    }
#ifdef FIXEDPPI
    fixed_ppi = FIXEDPPI;
#endif
    if( fixed_ppi )
    {
#ifdef SCREEN_ZOOM_SUPPORTED
	screen_zoom = screen_ppi / fixed_ppi;
#endif
    }
    else
    {
	int base_font_ppi = 160;
	int base_font_ppi_limit = 200;
	int dec = base_font_ppi * 2 - base_font_ppi_limit;
	if( !int_scaling || screen_ppi / screen_zoom >= base_font_ppi * 2 - dec )
	{
    	    font_zoom = ( screen_ppi / screen_zoom + dec ) * 256 / base_font_ppi;
	}
    }
    if( screen_scale <= 0 ) screen_scale = 1;
    if( screen_font_scale <= 0 ) screen_font_scale = 1;
    font_zoom = font_zoom * screen_scale * screen_font_scale;
    if( int_scaling ) font_zoom &= ~255;
    if( font_zoom < 256 ) font_zoom = 256;
    return font_zoom;
}

void win_zoom_init( window_manager* wm )
{
    wm->font_zoom = 256;
    int fixed_ppi = sconfig_get_int_value( "fixedppi", 0, 0 );
#ifdef FIXEDPPI
    fixed_ppi = FIXEDPPI;
#endif
    if( fixed_ppi )
    {
#ifdef SCREEN_ZOOM_SUPPORTED
	wm->screen_zoom = wm->screen_ppi / fixed_ppi;
#endif
    }
    wm->font_zoom = win_calc_font_zoom( wm->screen_ppi, wm->screen_zoom, wm->screen_scale, wm->screen_font_scale );
}

void win_zoom_apply( window_manager* wm )
{
    if( wm->screen_zoom > 1 )
    {
	wm->screen_xsize /= wm->screen_zoom;
        wm->screen_ysize /= wm->screen_zoom;
#ifdef SCREEN_SAFE_AREA_SUPPORTED
	wm->screen_safe_area.x /= wm->screen_zoom;
	wm->screen_safe_area.y /= wm->screen_zoom;
	wm->screen_safe_area.w /= wm->screen_zoom;
	wm->screen_safe_area.h /= wm->screen_zoom;
#endif
    }
}

void win_angle_apply( window_manager* wm )
{
#ifdef SCREEN_SAFE_AREA_SUPPORTED
    sdwm_rect a = wm->screen_safe_area;
    switch( wm->screen_angle & 3 )
    {
	case 1:
	    wm->screen_safe_area.x = wm->screen_ysize - ( a.y + a.h );
	    wm->screen_safe_area.y = a.x;
	    break;
	case 2:
	    wm->screen_safe_area.x = wm->screen_xsize - ( a.x + a.w );
	    wm->screen_safe_area.y = wm->screen_ysize - ( a.y + a.h );
	    break;
	case 3:
	    wm->screen_safe_area.x = a.y;
	    wm->screen_safe_area.y = wm->screen_xsize - ( a.x + a.w );
	    break;
	default: break;
    }
#endif
    if( wm->screen_angle & 1 )
    {
        int temp = wm->screen_xsize;
        wm->screen_xsize = wm->screen_ysize;
        wm->screen_ysize = temp;
#ifdef SCREEN_SAFE_AREA_SUPPORTED
        wm->screen_safe_area.w = a.h;
        wm->screen_safe_area.h = a.w;
#endif
    }
}

void win_vsync( bool vsync, window_manager* wm )
{
    if( vsync )
	wm->flags |= WIN_INIT_FLAG_VSYNC;
    else
	wm->flags &= ~WIN_INIT_FLAG_VSYNC;
    wm->device_vsync( vsync, wm );
}

void win_exit_request( window_manager* wm )
{
    if( !wm ) return;
    wm->exit_request = 1;
}

void win_destroy_request( window_manager* wm ) //fast exit without saving state
{
    if( !wm ) return;
    wm->exit_request = 2;
}

void win_restart_request( window_manager* wm )
{
    if( !wm ) return;
    wm->exit_request = 1;
    wm->restart_request = 1;
}

//Mark app as suspended (do it in the same thread with WM!):
void win_suspend( bool suspend, window_manager* wm )
{
    if( !wm ) return;
    if( wm->suspended != suspend )
    {
	wm->suspended = suspend;
	sundog_event evt;
	smem_clear_struct( evt );
	evt.type = EVT_SUSPEND;
	handle_event( &evt, wm );
    }
}

//Try to suspend some devices (do it in the same thread with WM!):
//NOT FOR ALL SYSTEMS. Tested in iOS only!
void win_suspend_devices( bool suspend, window_manager* wm )
{
    if( !wm ) return;
    wm->devices_suspended = suspend;

    //Don't use if( wm->devices_suspended != suspend ) here,
    //because the sound stream can be stopped by some other code outside the SunDog (for example, by Audiobus in iOS).
    //So we should try to resume the sound, even if the wm->devices_suspended == false

    sundog_event evt;
    smem_clear_struct( evt );
    evt.type = EVT_DEVSUSPEND;
    handle_event( &evt, wm );

    if( suspend )
    {
	//Devices can be suspended (app hidden, WM inactive):
	if( wm->sd && wm->sd->ss )
	    sundog_sound_device_stop( wm->sd->ss ); //stop the main sound stream
    }
    else
    {
	//Exit from suspended state:
	if( wm->sd && wm->sd->ss )
	    sundog_sound_device_play( wm->sd->ss ); //resume the main sound stream
    }
}

void win_deinit( window_manager* wm )
{
    wm->initialized = false;
    COMPILER_MEMORY_BARRIER();

    //Clear trash with deleted windows:
    if( wm->trash )
    {
	for( size_t i = 0; i < smem_get_size( wm->trash ) / sizeof( WINDOWPTR ); i++ )
	{
	    WINDOWPTR w = wm->trash[ i ];
	    if( !w ) continue;
	    smem_free( w->name );
	    smem_free( w->x1com );
	    smem_free( w->y1com );
	    smem_free( w->x2com );
	    smem_free( w->y2com );
	    smem_free( w );
	}
	smem_free( wm->trash );
    }

    unload_fonts( wm );

    if( wm->screen_lock_counter > 0 )
    {
	slog( "WM: WARNING. Screen is still locked (%d)\n", wm->screen_lock_counter );
	while( wm->screen_lock_counter > 0 ) wm->device_screen_unlock( 0, wm );
    }

    smem_free( wm->dstack );
    smem_free( wm->name ); wm->name = NULL;
    smem_free( wm->name2 ); wm->name2 = NULL;
    smem_free( wm->screen_pixels ); wm->screen_pixels = NULL;
    smem_free( wm->fdialog_filename ); wm->fdialog_filename = NULL;
    smem_free( wm->fdialog_copy_file_name ); wm->fdialog_copy_file_name = NULL;
    smem_free( wm->fdialog_copy_file_name2 ); wm->fdialog_copy_file_name2 = NULL;
    keymap_remove( wm->km, wm ); wm->km = NULL;

    video_capture_stop( wm );
    smem_free( wm->vcap_in_name );
    wm->vcap_in_name = NULL;

    wm->device_end( wm ); //DEVICE DEPENDENT PART (defined in eventloop.h)

    smutex_destroy( &wm->events_mutex );
#ifdef OPENGL
    smutex_destroy( &wm->gl_mutex );
#endif

#if !defined(OS_IOS) && !defined(OS_ANDROID) && !defined(OS_WINCE) && !defined(OS_MACOS)
    if( !( wm->flags & WIN_INIT_FLAG_NOWINDOW ) && !( wm->flags & WIN_INIT_FLAG_FULLSCREEN ) )
    {
	if( sconfig_get_int_value( APP_CFG_DONT_SAVE_WINSIZE, -1, 0 ) == -1 )
	{
	    bool save = 0;
	    int w = sconfig_get_int_value( APP_CFG_WIN_XSIZE, 0, 0 );
    	    int h = sconfig_get_int_value( APP_CFG_WIN_YSIZE, 0, 0 );
    	    bool m = 0;
    	    if( sconfig_get_int_value( APP_CFG_MAXIMIZED, -1, 0 ) != -1 ) m = 1;
	    if( wm->real_window_width != w || wm->real_window_height != h )
	    {
		sconfig_set_int_value( APP_CFG_WIN_XSIZE, wm->real_window_width, NULL );
		sconfig_set_int_value( APP_CFG_WIN_YSIZE, wm->real_window_height, NULL );
	    	save = 1;
	    }
	    if( m != ( ( wm->flags & WIN_INIT_FLAG_MAXIMIZED ) != 0 ) )
	    {
		if( wm->flags & WIN_INIT_FLAG_MAXIMIZED )
		    sconfig_set_int_value( APP_CFG_MAXIMIZED, 1, NULL );
		else
		    sconfig_remove_key( APP_CFG_MAXIMIZED, NULL );
		save = 1;
	    }
	    if( save ) sconfig_save( NULL );
	}
    }
#endif
}

//
// Working with windows
//

static WINDOWPTR get_from_trash( window_manager* wm )
{
    if( !wm->trash ) return NULL;
    uint32_t trash_len = smem_get_size( wm->trash ) / sizeof( WINDOWPTR );
    for( uint32_t i = 0; i < trash_len; i++ )
    {
	WINDOWPTR win = wm->trash[ wm->trash_ptr ];
	if( win )
	{
	    wm->trash[ wm->trash_ptr ] = NULL;
	    win->id = wm->window_counter;
	    wm->window_counter++;
	    win->flags = 0;
	    return win;
	}
	wm->trash_ptr++; if( wm->trash_ptr >= trash_len ) wm->trash_ptr = 0;
    }
    return NULL;
}

WINDOWPTR new_window( 
    const char* name, 
    int x, 
    int y, 
    int xsize, 
    int ysize, 
    COLOR color, 
    WINDOWPTR parent,
    void* host,
    win_handler_t win_handler,
    window_manager* wm )
{
    //Create window structure:
    WINDOWPTR win = get_from_trash( wm );
    if( win == NULL )
    {
	win = (WINDOWPTR)smem_znew( sizeof( WINDOW ) );
	win->id = (int16_t)wm->window_counter;
	wm->window_counter++;
    }

    //Setup properties:
    win->wm = wm;
    smem_free( win->name );
    win->name = smem_strdup( name );
    win->flags = 0;
    win->x = x;
    win->y = y;
    win->xsize = xsize;
    win->ysize = ysize;
    win->color = color;
    win->parent = parent;
    win->host = host;
    win->win_handler = win_handler;
    win->click_time = stime_ticks() - stime_ticks_per_second() * 10;

    win->controllers_calculated = false;
    win->controllers_defined = false;

    win->action_handler = NULL;
    win->handler_data = NULL;
    win->action_result = 0;

    win->font = wm->default_font;

    //Start init:
    if( win_handler )
    {
	sundog_event* evt = &wm->empty_evt;
	evt->win = win;
	evt->type = EVT_GETDATASIZE;
	int datasize = win_handler( evt, wm );
	if( datasize > 0 )
	{
	    win->data = smem_znew( datasize );
	}
	evt->type = EVT_AFTERCREATE;
	win_handler( evt, wm );
    }

    //Save it to window manager:
    if( !wm->root_win )
        wm->root_win = win;
    add_child( parent, win, wm );

    return win;
}

WINDOWPTR new_window( 
    const char* name, 
    int x, 
    int y, 
    int xsize, 
    int ysize, 
    COLOR color, 
    WINDOWPTR parent,
    win_handler_t win_handler,
    window_manager* wm )
{
    void* host = NULL;
    if( parent )
    {
	host = parent->host;
    }
    return new_window( name, x, y, xsize, ysize, color, parent, host, win_handler, wm );
}

void rename_window( WINDOWPTR win, const char* name )
{
    if( smem_strcmp( win->name, name ) )
    {
	smem_free( win->name );
	win->name = smem_strdup( name );
    }
}

void set_window_controller( WINDOWPTR win, int ctrl_num, window_manager* wm, ... )
{
    va_list p;
    va_start( p, wm );
    uint ptr = 0;
    win->controllers_defined = true;
    WCMD* cmds = NULL;
    switch( ctrl_num )
    {
	case 0: cmds = win->x1com; break;
	case 1: cmds = win->y1com; break;
	case 2: cmds = win->x2com; break;
	case 3: cmds = win->y2com; break;
    }
    if( cmds == 0 )
	cmds = (WCMD*)smem_new( sizeof( WCMD ) * 4 );
    while( 1 )
    {
	WCMD command = va_arg( p, WCMD );
	if( smem_get_size( cmds ) / sizeof( WCMD ) <= ptr )
	    cmds = (WCMD*)smem_resize( cmds, sizeof( WCMD ) * ( ptr + 4 ) );
	cmds[ ptr ] = command; 
	if( command == CEND ) break;
	ptr++;
    }
    switch( ctrl_num )
    {
	case 0: win->x1com = cmds; break;
	case 1: win->y1com = cmds; break;
	case 2: win->x2com = cmds; break;
	case 3: win->y2com = cmds; break;
    }
    va_end( p );
}

void remove_window_controllers( WINDOWPTR win )
{
    win->controllers_defined = false;
    smem_free( win->x1com ); win->x1com = NULL;
    smem_free( win->y1com ); win->y1com = NULL;
    smem_free( win->x2com ); win->x2com = NULL;
    smem_free( win->y2com ); win->y2com = NULL;
}

static void move_to_trash( WINDOWPTR win, window_manager* wm )
{
    if( !win ) return;
    win->visible = 0;
    win->flags |= WIN_FLAG_TRASH;
    win->win_handler = NULL;
    uint32_t trash_len = smem_get_size( wm->trash ) / sizeof( WINDOWPTR );
    for( uint32_t i = 0; i < trash_len; i++ )
    {
        if( wm->trash[ wm->trash_ptr ] == NULL )
        {
    	    wm->trash[ wm->trash_ptr ] = win;
	    return;
	}
	wm->trash_ptr++; if( wm->trash_ptr >= trash_len ) wm->trash_ptr = 0;
    }
    wm->trash_ptr = trash_len;
    wm->trash = (WINDOWPTR*)smem_resize2( wm->trash, ( trash_len + 256 ) * sizeof( WINDOWPTR ) );
    wm->trash[ wm->trash_ptr ] = win;
}

void remove_window( WINDOWPTR win, window_manager* wm )
{
    if( !win ) return;
    if( win->flags & WIN_FLAG_TRASH )
    {
        slog( "ERROR: can't remove already removed window (%s)\n", win->name );
        return;
    }
    if( win->win_handler )
    {
        //Sent EVT_BEFORECLOSE to window handler:
        sundog_event* evt = &wm->empty_evt;
        evt->win = win;
        evt->type = EVT_BEFORECLOSE;
        win->win_handler( evt, wm );
    }
    if( win->childs )
    {
        //Remove childs:
        while( win->childs_num )
    	    remove_window( win->childs[ 0 ], wm );
	smem_free( win->childs );
	win->childs = NULL;
    }
    //Remove data:
    smem_free( win->data );
    win->data = NULL;
    //Remove region:
    if( win->reg ) GdDestroyRegion( win->reg );
    win->reg = NULL;
    //Remove commands:
    smem_free( win->x1com ); win->x1com = NULL;
    smem_free( win->y1com ); win->y1com = NULL;
    smem_free( win->x2com ); win->x2com = NULL;
    smem_free( win->y2com ); win->y2com = NULL;
    //Remove window:
    if( win == wm->focus_win ) wm->focus_win = NULL;
    if( win == wm->root_win )
    {
        wm->root_win = NULL;
    }
    else
    {
        remove_child( win->parent, win, wm );
    }
    move_to_trash( win, wm );
}

void add_child( WINDOWPTR win, WINDOWPTR child, window_manager* wm )
{
    if( win == NULL || child == NULL ) return;

    if( win->childs == 0 )
    {
	win->childs = (WINDOWPTR*)smem_new( sizeof( WINDOWPTR ) * 4 );
    }
    else
    {
	size_t old_size = smem_get_size( win->childs ) / sizeof( WINDOWPTR );
	if( (unsigned)win->childs_num >= old_size )
	    win->childs = (WINDOWPTR*)smem_resize( win->childs, ( old_size + 4 ) * sizeof( WINDOWPTR ) );
    }
    win->childs[ win->childs_num ] = child;
    win->childs_num++;
    child->parent = win;
}

void remove_child( WINDOWPTR win, WINDOWPTR child, window_manager* wm )
{
    if( win == NULL || child == NULL ) return;

    //Remove link from parent window:
    int c;
    for( c = 0; c < win->childs_num; c++ )
    {
	if( win->childs[ c ] == child ) break;
    }
    if( c < win->childs_num )
    {
	for( ; c < win->childs_num - 1; c++ )
	{
	    win->childs[ c ] = win->childs[ c + 1 ];
	}
	win->childs_num--;
	child->parent = NULL;
    }
}

WINDOWPTR get_parent_by_win_handler( WINDOWPTR win, win_handler_t win_handler )
{
    WINDOWPTR rv = 0;
    while( 1 )
    {
	if( win->win_handler == win_handler )
	{
	    rv = win;
	    break;
	}
	win = win->parent;
	if( win == NULL ) break;
    }
    if( rv == 0 )
    {
	//slog( "Can't find described parent for window %s\n", win->name ); //for debug only
    }
    return rv;
}

static WINDOWPTR find_win_by_handler2( WINDOWPTR root, win_handler_t win_handler, bool silent )
{
    WINDOWPTR rv = NULL;
    while( 1 )
    {
	if( root->win_handler == win_handler )
	{
	    rv = root;
	    break;
	}
	for( int i = 0; i < root->childs_num; i++ )
	{
	    WINDOWPTR w = root->childs[ i ];
	    if( w )
	    {
		if( w->win_handler == win_handler )
		{
		    rv = w;
		    break;
		}
	    }
	}
	if( rv ) break;
	for( int i = 0; i < root->childs_num; i++ )
	{
	    WINDOWPTR w = find_win_by_handler2( root->childs[ i ], win_handler, true );
	    if( w )
	    {
		rv = w;
		break;
	    }
	}
	break;
    }
    if( rv == NULL && !silent )
    {
	//slog( "Can't find described child for window %s\n", win->name ); //for debug only
    }
    return rv;
}

WINDOWPTR find_win_by_handler( WINDOWPTR root, win_handler_t win_handler )
{
    return find_win_by_handler2( root, win_handler, false );
}

WINDOWPTR find_win_by_name( WINDOWPTR root, const char* name, win_handler_t win_handler )
{
    WINDOWPTR rv = NULL;
    while( 1 )
    {
	if( ( root->win_handler == win_handler || win_handler == NULL ) && smem_strcmp( root->name, name ) == 0 )
	{
	    rv = root;
	    break;
	}
	for( int i = 0; i < root->childs_num; i++ )
	{
	    WINDOWPTR w = root->childs[ i ];
	    if( w )
	    {
		if( ( w->win_handler == win_handler || win_handler == NULL ) && smem_strcmp( w->name, name ) == 0 )
		{
		    rv = w;
		    break;
		}
	    }
	}
	if( rv ) break;
	for( int i = 0; i < root->childs_num; i++ )
	{
	    WINDOWPTR w = find_win_by_name( root->childs[ i ], name, win_handler );
	    if( w )
	    {
		rv = w;
		break;
	    }
	}
	break;
    }
    return rv;
}

void set_handler( WINDOWPTR win, win_action_handler_t handler, void* handler_data, window_manager* wm )
{
    if( win )
    {
	win->action_handler = handler;
	win->handler_data = handler_data;
    }
}
 
bool is_window_visible( WINDOWPTR win, window_manager* wm )
{
    if( win && win->visible && win->reg )
    {
	if( win->reg->numRects )
	{
	    return 1;
	}
    }
    return 0;
}

void draw_window( WINDOWPTR win, window_manager* wm )
{
    if( wm->screen_buffer_preserved == 0 ) 
    {
	screen_changed( wm );
	return;
    }
    if( wm->exit_request ) return;
    if( win && win->visible && win->reg )
    {
	win_draw_lock( win, wm );
	if( win->reg->numRects || ( win->flags & WIN_FLAG_ALWAYS_HANDLE_DRAW_EVT ) )
	{
	    sundog_event* evt = &wm->empty_evt;
    	    evt->win = win;
	    evt->type = EVT_DRAW;
	    if( win->win_handler && win->win_handler( evt, wm ) )
	    {
		//Draw event was handled
	    }
	    else
	    {
	        win_draw_frect( win, 0, 0, win->xsize, win->ysize, win->color, wm );
	    }
	}
	if( win->childs_num )
	{
	    for( int c = 0; c < win->childs_num; c++ )
	    {
		draw_window( win->childs[ c ], wm );
	    }
	}
	win_draw_unlock( win, wm );
    }
}

void force_redraw_all( window_manager* wm ) //even if screen_buffer_preserved == 0
{
    if( wm->screen_buffer_preserved == 0 )
    {
        wm->screen_buffer_preserved = 1;
#if !defined(NOVCAP)
        if( wm->vcap ) video_capture_frame_begin( wm );
#endif
        draw_window( wm->root_win, wm );
#if !defined(NOVCAP)
        if( wm->vcap ) video_capture_frame_end( wm );
#endif
        wm->screen_buffer_preserved = 0;
    }
    else
    {
        draw_window( wm->root_win, wm );
    }
    wm->device_redraw_framebuffer( wm );
}

void show_window( WINDOWPTR win )
{
    if( win && ( win->flags & WIN_FLAG_ALWAYS_INVISIBLE ) == 0 )
    {
	window_manager* wm = win->wm;
	if( win->visible == false )
	{
	    sundog_event* evt = &wm->empty_evt;
	    evt->win = win;
	    evt->type = EVT_BEFORESHOW;
	    win->win_handler( evt, wm );
	    win->visible = true;
	}
	for( int c = 0; c < win->childs_num; c++ )
	{
	    show_window( win->childs[ c ] );
	}
    }
}

void hide_window( WINDOWPTR win )
{
    if( win )
    {
	window_manager* wm = win->wm;
	if( wm->focus_win == win )
	{
	    //Focus on hidden window
	    wm->focus_win = NULL;
	}
	if( win->visible )
	{
	    sundog_event* evt = &wm->empty_evt;
	    evt->win = win;
	    evt->type = EVT_BEFOREHIDE;
	    win->win_handler( evt, wm );
	    win->visible = false;
	}
	for( int c = 0; c < win->childs_num; c++ )
	{
	    hide_window( win->childs[ c ] );
	}
    }
}

bool show_window2( WINDOWPTR win )
{
    bool changed = false;
    if( win )
    {
	win->flags &= ~WIN_FLAG_ALWAYS_INVISIBLE;
	if( !win->visible )
	{
	    show_window( win );
	    changed = true;
	}
    }
    return changed;
}

bool hide_window2( WINDOWPTR win )
{
    bool changed = false;
    if( win )
    {
	win->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
	if( win->visible )
	{
    	    hide_window( win );
    	    changed = true;
    	}
    }
    return changed;
}

void recalc_controllers( WINDOWPTR win, window_manager* wm );

//Controller Virtual Machine:
static inline void cvm_math_op( int* val1, int val2, int mode )
{
    switch( mode )
    {
        case 0: // =
    	    *val1 = val2; 
    	    break;
        case 1: // SUB
    	    *val1 -= val2; 
    	    break;
        case 2: // ADD
    	    *val1 += val2; 
    	    break;
        case 3: // MAX
    	    if( *val1 > val2 ) *val1 = val2; 
    	    break;
        case 4: // MIN
    	    if( *val1 < val2 ) *val1 = val2; 
    	    break;
    	case 5: // MUL, DIV256
    	    *val1 = ( *val1 * val2 ) >> 8;
    	    break;
    }
}
static void cvm_exec( WINDOWPTR win, WCMD* c, int* val, int size, window_manager* wm )
{
    if( !c ) return;
    int p = 0;
    WINDOWPTR other_win = NULL;
    int mode = 0;
    int perc = 0;
    int backval = 0;
    bool brk = false;
    while( !brk )
    {
	switch( c[ p ] )
	{
	    case CWIN: 
		p++;
		other_win = (WINDOWPTR)c[ p ];
		if( other_win->controllers_calculated == false ) recalc_controllers( other_win, wm );
		break;
	    case CX1:
		cvm_math_op( val, other_win->x, mode );
		mode = 0;
		perc = 0;
		break;
	    case CY1: 
		cvm_math_op( val, other_win->y, mode );
		mode = 0;
		perc = 0;
		break;
	    case CX2:
		cvm_math_op( val, other_win->x + other_win->xsize, mode );
		mode = 0;
		perc = 0;
		break;
	    case CY2: 
		cvm_math_op( val, other_win->y + other_win->ysize, mode );
		mode = 0;
		perc = 0;
		break;
	    case CXSIZE:
		cvm_math_op( val, other_win->xsize, mode );
		mode = 0;
		perc = 0;
		break;
	    case CYSIZE: 
		cvm_math_op( val, other_win->ysize, mode );
		mode = 0;
		perc = 0;
		break;
	    case CSUB: mode = 1; break;
	    case CADD: mode = 2; break;
	    case CPERC: perc = 1; break;
	    case CBACKVAL0: backval = 0; break;
	    case CBACKVAL1: backval = 1; break;
	    case CMAXVAL: mode = 3; break;
	    case CMINVAL: mode = 4; break;
	    case CMULDIV256: mode = 5; break;
	    case CPUTR0: wm->creg0 = *val; break;
	    case CGETR0: *val = wm->creg0; break;
	    case CR0:	
		cvm_math_op( val, wm->creg0, mode );
		mode = 0;
		perc = 0;
		break;
	    case CEND: brk = true; break;
	    default:
		if( perc )
		{
		    cvm_math_op( val, ( (int)c[ p ] * size ) / 100, mode );
		}
		else
		{
		    if( backval )
		    {
			cvm_math_op( val, size - (int)c[ p ], mode );
		    }
		    else
		    {
			cvm_math_op( val, (int)c[ p ], mode ); //Just a number
		    }
		}
		mode = 0;
		perc = 0;
		break;
	}
	p++;
    }
}

void bring_to_front( WINDOWPTR win, window_manager* wm )
{
    if( win == NULL ) return;
    if( win->parent == NULL ) return;
    
    int i;
    for( i = 0; i < win->parent->childs_num; i++ )
    {
	if( win->parent->childs[ i ] == win ) break;
    }
    if( i < win->parent->childs_num - 1 )
    {
	for( int i2 = i; i2 < win->parent->childs_num - 1; i2++ )
	    win->parent->childs[ i2 ] = win->parent->childs[ i2 + 1 ];
	win->parent->childs[ win->parent->childs_num - 1 ] = win;
    }
}

void recalc_controllers( WINDOWPTR win, window_manager* wm )
{
    if( win && win->controllers_calculated == false && win->parent )
    {
	if( win->flags & WIN_FLAG_BEFORERESIZE_ENABLED )
	{
	    handle_event( win, EVT_BEFORERESIZE, wm );
	}
	if( win->controllers_defined == false || ( win->flags & WIN_FLAG_DONT_USE_CONTROLLERS ) ) 
	    win->controllers_calculated = true;
	else
	{
	    int x1 = win->x;
	    cvm_exec( win, win->x1com, &x1, win->parent->xsize, wm );

	    int y1 = win->y;
	    cvm_exec( win, win->y1com, &y1, win->parent->ysize, wm );

	    int x2 = x1 + win->xsize;
	    int y2 = y1 + win->ysize;
	    cvm_exec( win, win->x2com, &x2, win->parent->xsize, wm );
	    cvm_exec( win, win->y2com, &y2, win->parent->ysize, wm );

	    int temp;
	    if( x1 > x2 ) { temp = x1; x1 = x2; x2 = temp; }
	    if( y1 > y2 ) { temp = y1; y1 = y2; y2 = temp; }
	    win->x = x1;
	    win->y = y1;
	    win->xsize = x2 - x1;
	    win->ysize = y2 - y1;
	    win->controllers_calculated = true;
	}
	if( win->flags & WIN_FLAG_AFTERRESIZE_ENABLED )
	{
	    handle_event( win, EVT_AFTERRESIZE, wm );
	}
    }
}

//reg - global mask (busy area);
//      initially empty;
//      will be the sum of all app regions.
void recalc_region( WINDOWPTR win, MWCLIPREGION* reg, int cut_x, int cut_y, int cut_x2, int cut_y2, int px, int py, window_manager* wm )
{
    if( !win->visible )
    {
	if( win->reg ) GdDestroyRegion( win->reg );
	win->reg = 0;
	return;
    }
    if( win->controllers_defined && win->controllers_calculated == false )
    {
	recalc_controllers( win, wm );
    }
    win->screen_x = win->x + px;
    win->screen_y = win->y + py;
    int x1 = win->x + px;
    int y1 = win->y + py;
    int x2 = win->x + px + win->xsize;
    int y2 = win->y + py + win->ysize;
    if( cut_x > x1 ) x1 = cut_x;
    if( cut_y > y1 ) y1 = cut_y;
    if( cut_x2 < x2 ) x2 = cut_x2;
    if( cut_y2 < y2 ) y2 = cut_y2;
    if( win->childs_num && !( x1 > x2 || y1 > y2 ) )
    {
	for( int c = win->childs_num - 1; c >= 0; c-- )
	{
	    recalc_region( 
		win->childs[ c ], 
		reg, 
		x1, y1, 
		x2, y2, 
		win->x + px, 
		win->y + py, 
		wm );
	}
    }
    if( !win->reg )
    {
	if( x1 > x2 || y1 > y2 )
	    win->reg = GdAllocRegion();
	else
	    win->reg = GdAllocRectRegion( x1, y1, x2, y2 );
    }
    else
    {
	if( x1 > x2 || y1 > y2 )
	{
	    GdSetRectRegion( win->reg, 0, 0, 0, 0 );
	}
	else
	{
	    GdSetRectRegion( win->reg, x1, y1, x2, y2 );
	}
    }
    //Calc corrected win region:
    GdSubtractRegion( win->reg, win->reg, reg ); //win->reg = win->reg - reg
    //Calc corrected invisible region:
    GdUnionRegion( reg, reg, win->reg ); //reg = reg + win->reg
}

void clean_regions( WINDOWPTR win, window_manager* wm )
{
    if( win )
    {
	for( int c = 0; c < win->childs_num; c++ )
	    clean_regions( win->childs[ c ], wm );
	if( win->reg ) 
	{
	    GdSetRectRegion( win->reg, 0, 0, 0, 0 );
	}
	win->controllers_calculated = false;
    }
}

void recalc_regions( window_manager* wm )
{
    MWCLIPREGION* reg = GdAllocRegion();
    if( wm->root_win )
    {
	//Make the windows with the ALWAYS_ON_TOP flag the topmost (visible):
	int size = wm->root_win->childs_num;
	for( int i = 0; i < size; i++ )
	{
	    WINDOWPTR win = wm->root_win->childs[ i ];
	    if( win && ( win->flags & WIN_FLAG_ALWAYS_ON_TOP ) && i < size - 1 )
	    {
		//Bring this window to front:
		for( int i2 = i; i2 < size - 1; i2++ )
		{
		    wm->root_win->childs[ i2 ] = wm->root_win->childs[ i2 + 1 ];
		}
		wm->root_win->childs[ size - 1 ] = win;
	    }
	}
	//Recalc all regions:
	clean_regions( wm->root_win, wm );
	recalc_region( wm->root_win, reg, 0, 0, wm->screen_xsize, wm->screen_ysize, 0, 0, wm );
    }
    if( reg ) GdDestroyRegion( reg );
    //Don't put any immediate event handlers here!
    //recalc_regions() should not draw!
}

void set_focus_win( WINDOWPTR win, window_manager* wm )
{
    if( win && ( win->flags & WIN_FLAG_TRASH ) )
    {
	//This window removed by someone. But we can't focus on removed window.
	//So.. Just focus on NULL window:
	win = NULL;
    }

    sundog_event evt;
    smem_clear( &evt, sizeof( sundog_event ) );
    evt.time = stime_ticks();

    //Unfocus:
    
    WINDOWPTR prev_focus_win = wm->focus_win;
    uint16_t prev_focus_win_id = wm->focus_win_id;

    WINDOWPTR new_focus_win = win;
    uint16_t new_focus_win_id = 0;
    if( win ) new_focus_win_id = win->id;
    
    if( prev_focus_win != new_focus_win || 
	( prev_focus_win == new_focus_win && prev_focus_win_id != new_focus_win_id ) )
    {
	//Focus changed:
	
	if( prev_focus_win && !( prev_focus_win->flags & WIN_FLAG_TRASH ) )
	{
	    //Send UNFOCUS event:
	    evt.type = EVT_UNFOCUS;
	    evt.win = prev_focus_win;
	    if( !( evt.win->flags & WIN_FLAG_UNFOCUS_HANDLING ) )
	    {
		evt.win->flags |= WIN_FLAG_UNFOCUS_HANDLING;
		handle_event( &evt, wm );
		evt.win->flags &= ~WIN_FLAG_UNFOCUS_HANDLING;
	    }
	}
    }
    
    //Focus on the new window:

    prev_focus_win = wm->focus_win;
    prev_focus_win_id = wm->focus_win_id;

    new_focus_win = win;
    new_focus_win_id = 0;
    if( win ) new_focus_win_id = win->id;

    wm->focus_win = new_focus_win;
    wm->focus_win_id = new_focus_win_id;

    if( prev_focus_win != new_focus_win || 
	( prev_focus_win == new_focus_win && prev_focus_win_id != new_focus_win_id ) )
    {
	//Focus changed:

        wm->prev_focus_win = prev_focus_win;
        wm->prev_focus_win_id = prev_focus_win_id;

	if( new_focus_win )
	{
	    //Send FOCUS event:
	    //(user can remember previous focused window (wm->prev_focus_win))
	    evt.type = EVT_FOCUS;
	    evt.win = new_focus_win;
	    if( !( evt.win->flags & WIN_FLAG_FOCUS_HANDLING ) )
	    {
		evt.win->flags |= WIN_FLAG_FOCUS_HANDLING;
		handle_event( &evt, wm );
		evt.win->flags &= ~WIN_FLAG_FOCUS_HANDLING;
	    }
	}
    }
}

static int find_focus_window( WINDOWPTR win, WINDOW** focus_win, window_manager* wm )
{
    if( win == NULL ) return 0;
    if( win->reg && GdPtInRegion( win->reg, wm->pen_x, wm->pen_y ) )
    {
	if( win->flags & WIN_FLAG_TRANSPARENT_FOR_FOCUS )
	{
	    while( 1 )
	    {
		win = win->parent;
		if( win == NULL ) return 0;
		if( ( win->flags & WIN_FLAG_TRANSPARENT_FOR_FOCUS ) == 0 ) break;
	    }
	}
	*focus_win = win;
	return 1;
    }
    for( int c = 0; c < win->childs_num; c++ )
    {
	if( find_focus_window( win->childs[ c ], focus_win, wm ) )
	    return 1;
    }
    return 0;
}

void convert_real_window_xy( int& x, int& y, window_manager* wm )
{
    int temp;
    switch( wm->screen_angle )
    {
	case 1:
    	    temp = x;
    	    x = ( wm->real_window_height - 1 ) - y;
    	    y = temp;
    	    break;
    	case 2:
    	    x = ( wm->real_window_width - 1 ) - x;
    	    y = ( wm->real_window_height - 1 ) - y;
    	    break;
    	case 3:
    	    temp = x;
    	    x = y;
    	    y = ( wm->real_window_width - 1 ) - temp;
    	    break;
    }
	
    if( wm->screen_zoom )
    {
        x /= wm->screen_zoom;
        y /= wm->screen_zoom;
    }
}

int scale_coord( int v, bool user_to_real, window_manager* wm )
{
    if( user_to_real )
    {
	if( wm->screen_zoom )
	{
    	    v *= wm->screen_zoom;
	}
    }
    else
    {
	if( wm->screen_zoom )
	{
    	    v /= wm->screen_zoom;
	}
    }
    return v;
}

#ifdef MULTITOUCH

//mode:
//  0 - touch down;
//  1 - touch move;
//  2 - touch up;
//  3 - vscroll; pressure: -1024 - down; 1024 - up;
//  4 - hscroll; pressure: -1024 - left; 1024 - right;
int collect_touch_events( int mode, uint touch_flags, uint evt_flags, int x, int y, int pressure, WM_TOUCH_ID touch_id, window_manager* wm )
{
    if( !wm ) return -1;
    if( !wm->initialized ) return -1;
    
    if( wm->touch_evts_cnt >= WM_TOUCH_EVENTS )
	send_touch_events( wm );
    
    sundog_event* evt = &wm->touch_evts[ wm->touch_evts_cnt ];
    wm->touch_evts_cnt++;

    if( touch_flags & TOUCH_FLAG_REALWINDOW_XY )
    {
	convert_real_window_xy( x, y, wm );
    }
    
    if( ( touch_flags & TOUCH_FLAG_LIMIT ) || ( wm->flags & WIN_INIT_FLAG_FULLSCREEN ) )
    {
	if( x < -16 ) x = -16;
	if( y < -16 ) y = -16;
	if( x > wm->screen_xsize + 16 ) x = wm->screen_xsize + 16;
	if( y > wm->screen_ysize + 16 ) y = wm->screen_ysize + 16;
    }

    evt->time = stime_ticks();
    evt->win = 0;
    evt->flags = evt_flags;
    evt->x = x;
    evt->y = y;
    evt->scancode = 0;
    evt->key = MOUSE_BUTTON_LEFT;
    evt->pressure = pressure;
    
    switch( mode )
    {
        case 0:
    	    //Touch down:
	    for( uint t = 0; t < WM_TOUCHES; t++ )
	    {
	        if( wm->touch_busy[ t ] == false )
	        {
	    	    wm->touch_busy[ t ] = true;
		    wm->touch_id[ t ] = touch_id;
		    evt->scancode = t;
		    if( t == 0 )
		        evt->type = EVT_MOUSEBUTTONDOWN;
		    else
		        evt->type = EVT_TOUCHBEGIN;
		    break;
		}
	    }
	    break;
	case 1:
	    //Touch move:
	    for( uint t = 0; t < WM_TOUCHES; t++ )
	    {
	        if( wm->touch_busy[ t ] && wm->touch_id[ t ] == touch_id )
	        {
	    	    evt->scancode = t;
		    if( t == 0 )
		        evt->type = EVT_MOUSEMOVE;
		    else
		        evt->type = EVT_TOUCHMOVE;
		    break;
		}
	    }
	    break;
	case 2:
	    //Touch up:
	    for( uint t = 0; t < WM_TOUCHES; t++ )
	    {
	        if( wm->touch_busy[ t ] && wm->touch_id[ t ] == touch_id )
	        {
		    wm->touch_busy[ t ] = false;
		    evt->scancode = t;
		    if( t == 0 )
		        evt->type = EVT_MOUSEBUTTONUP;
		    else
		        evt->type = EVT_TOUCHEND;
		    break;
		}
	    }
	    break;
	case 3:
	    //vscroll:
	    evt->type = EVT_MOUSEBUTTONDOWN;
	    if( pressure > 0 )
		evt->key = MOUSE_BUTTON_SCROLLUP;
	    else
	    {
		pressure = -pressure;
		evt->key = MOUSE_BUTTON_SCROLLDOWN;
	    }
	    break;
	case 4:
	    //hscroll:
	    evt->type = EVT_MOUSEBUTTONDOWN;
	    if( pressure > 0 )
		evt->key = MOUSE_BUTTON_SCROLLRIGHT;
	    else
	    {
		pressure = -pressure;
		evt->key = MOUSE_BUTTON_SCROLLLEFT;
	    }
	    break;
	default: break;
    }

    return 0;
}

int send_touch_events( window_manager* wm )
{
    if( !wm ) return -1;
    if( !wm->initialized ) return -1;
    if( wm->touch_evts_cnt == 0 ) return 0;
    send_events( wm->touch_evts, wm->touch_evts_cnt, wm );
    wm->touch_evts_cnt = 0;
    return 0;
}

#endif

static void evt_autorepeat_handler( void* user_data, sundog_timer* t, window_manager* wm )
{
    send_events( &wm->autorepeat_timer_evt, 1, wm );
    reset_timer( t->id, 1000 / wm->kbd_autorepeat_freq, wm );
}

static void evt_autorepeat( sundog_event* evt, window_manager* wm )
{
    if( evt->type == EVT_BUTTONDOWN )
    {
        wm->autorepeat_timer_evt = *evt;
        wm->autorepeat_timer_evt.flags &= ~EVT_FLAG_AUTOREPEAT;
        remove_timer( wm->autorepeat_timer, wm );
        wm->autorepeat_timer = add_timer( evt_autorepeat_handler, NULL, wm->kbd_autorepeat_delay, wm );
    }
    else
    {
        //EVT_BUTTONUP
        remove_timer( wm->autorepeat_timer, wm );
        wm->autorepeat_timer = -1;
    }
}

int send_events(
    sundog_event* events,
    int events_num,
    window_manager* wm )
{
    if( wm == 0 ) return -1;
    if( !wm->initialized ) return -1;
    int retval = 0;
    
    smutex_lock( &wm->events_mutex );

    for( int e = 0; e < events_num; e++ )
    {
	sundog_event* evt = &events[ e ];
	if( wm->events_count + 1 <= WM_EVENTS )
	{
	    //Get pointer to new event:
	    int new_ptr = ( wm->current_event_num + wm->events_count ) & ( WM_EVENTS - 1 );
	
	    if( evt->type == EVT_BUTTONDOWN || evt->type == EVT_BUTTONUP )
	    {
		if( evt->key >= 0x41 && evt->key <= 0x5A ) //Capital
		    evt->key += 0x20; //Make it small
	    }
	
	    //Save new event to FIFO buffer:
	    smem_copy( &wm->events[ new_ptr ], evt, sizeof( sundog_event ) );
            
            if( evt->flags & EVT_FLAG_AUTOREPEAT ) evt_autorepeat( evt, wm );
	
	    //Increment number of unhandled events:
	    volatile int new_event_count = wm->events_count + 1;
	    wm->events_count = new_event_count;
	}
	else
	{
	    retval = 1;
	    break;
	}
    }
    
    smutex_unlock( &wm->events_mutex );
    
    return retval;
}

int send_event(
    WINDOWPTR win,
    int type,
    uint flags,
    int x,
    int y,
    int key,
    int scancode,
    int pressure,
    window_manager* wm )
{
    if( !wm ) return -1;
    if( !wm->initialized ) return -1;
    int retval = 0;
    
    smutex_lock( &wm->events_mutex );
    
    if( wm->events_count + 1 <= WM_EVENTS )
    {
	//Get pointer to new event:
	int new_ptr = ( wm->current_event_num + wm->events_count ) & ( WM_EVENTS - 1 );

	if( type == EVT_BUTTONDOWN || type == EVT_BUTTONUP )
	{
	    if( key >= 0x41 && key <= 0x5A ) //Capital
		key += 0x20; //Make it small
	}

	//Save new event to FIFO buffer:
        sundog_event* evt = &wm->events[ new_ptr ];
	evt->type = type;
        evt->time = stime_ticks();
        evt->win = win;
        evt->flags = flags;
        evt->x = x;
        evt->y = y;
        evt->key = key;
        evt->scancode = scancode;
        evt->pressure = pressure;

        if( flags & EVT_FLAG_AUTOREPEAT ) evt_autorepeat( evt, wm );

	//Increment number of unhandled events:
	wm->events_count = wm->events_count + 1;
    }
    else
    {
	retval = 1;
    }

    smutex_unlock( &wm->events_mutex );
    
    return retval;
}

int send_event( WINDOWPTR win, int type, window_manager* wm )
{
    sundog_event evt;
    smem_clear_struct( evt );
    evt.win = win;
    evt.type = type;
    return send_events( &evt, 1, wm );
}

static int check_event( sundog_event* evt, window_manager* wm )
{
    if( evt == NULL ) return 1;
    if( evt->win ) return 0;
    if( !wm->initialized ) return 1;
    switch( evt->type )
    {
	case EVT_BUTTONDOWN:
	    if( wm->prev_btn_key == evt->key &&
		wm->prev_btn_scancode == evt->scancode && 
		wm->prev_btn_flags == ( evt->flags & EVT_FLAGS_MASK ) )
	    {
		evt->flags |= EVT_FLAG_REPEAT;
	    }
	    else
	    {
		wm->prev_btn_key = evt->key;
		wm->prev_btn_scancode = evt->scancode;
		wm->prev_btn_flags = evt->flags & EVT_FLAGS_MASK;
	    }
	    break;
	case EVT_BUTTONUP:
	    wm->prev_btn_key = 0;
	    break;
	case EVT_MOUSEBUTTONDOWN:
        case EVT_MOUSEBUTTONUP:
        case EVT_MOUSEMOVE:
	{
    	    wm->pen_x = evt->x;
    	    wm->pen_y = evt->y;
    	    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    	    if( wm->last_unfocused_window )
    	    {
    		evt->win = wm->last_unfocused_window;
		if( evt->type == EVT_MOUSEBUTTONUP )
		{
	    	    wm->last_unfocused_window = NULL;
		}
		return 0;
    	    }
    	    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    	    if( evt->type == EVT_MOUSEBUTTONDOWN )
    	    { //If mouse click:
    		if( evt->key & MOUSE_BUTTON_SCROLLUP ||
	    	    evt->key & MOUSE_BUTTON_SCROLLDOWN )
		{
	    	    //Mouse scroll up/down...
	    	    WINDOWPTR scroll_win = NULL;
		    if( find_focus_window( wm->root_win, &scroll_win, wm ) )
		    {
			evt->win = scroll_win;
			return 0;
		    }
		    else
		    {
			//Window not found under the pointer:
			return 1;
		    }
		}
		else
		{
	    	    //Mouse click on some window...
	    	    WINDOWPTR focus_win = NULL;
		    if( find_focus_window( wm->root_win, &focus_win, wm ) )
		    {
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			if( focus_win->flags & WIN_FLAG_ALWAYS_UNFOCUSED )
			{
		    	    evt->win = focus_win;
		    	    wm->last_unfocused_window = focus_win;
		    	    return 0;
			}
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			set_focus_win( focus_win, wm );
		    }
		    else
		    {
			//Window not found under the pointer:
			return 1;
		    }
		}
	    }
	    break;
	}
    }
    if( wm->focus_win )
    {
        //Set pointer to the window:
        evt->win = wm->focus_win;

        if( evt->type == EVT_MOUSEBUTTONDOWN )
        {
    	    if( ( evt->time - wm->focus_win->click_time ) < ( wm->double_click_time * stime_ticks_per_second() ) / 1000 )
	    {
	        //Double click detected:
	        wm->focus_win->flags |= WIN_FLAG_DOUBLECLICK;
	        wm->focus_win->click_time = evt->time - stime_ticks_per_second() * 10; //Reset click time
	    }
	    else
	    {
	        //One click. Clear double-click flags:
	        wm->focus_win->flags &= ~WIN_FLAG_DOUBLECLICK;
	        wm->focus_win->click_time = evt->time;
	    }
	}
	if( wm->focus_win->flags & WIN_FLAG_DOUBLECLICK )
	{
	    evt->flags |= EVT_FLAG_DOUBLECLICK;
	}
    }
    return 0;
}

int handle_event( WINDOWPTR win, int type, window_manager* wm )
{
    sundog_event evt;
    smem_clear_struct( evt );
    evt.type = type;
    evt.win = win;
    return handle_event( &evt, wm );
}

int handle_event( sundog_event* evt, window_manager* wm )
{
    sundog_event evt2 = *evt;
    int handled = app_event_handler( &evt2, wm );
    if( handled == 0 )
    {
	//Event hot handled by first event handler.
	//Send it to window:
	handled = handle_event_by_window( &evt2, wm );
	if( handled == 0 )
	{
	    evt2.win = wm->handler_of_unhandled_events;
	    handled = handle_event_by_window( &evt2, wm );
	}
    }
    return handled;
}

int handle_event_direct( WINDOWPTR win, int type )
{
    if( !win ) return 0;
    sundog_event evt;
    smem_clear_struct( evt );
    evt.type = type;
    evt.win = win;
    return win->win_handler( &evt, win->wm );
}

int handle_event_by_window( sundog_event* evt, window_manager* wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    if( win )
    {
	if( win->flags & WIN_FLAG_TRASH )
	{
	    slog( "ERROR: can't handle event %d by removed window (%s)\n", evt->type, win->name );
	    retval = 1;
	}
	else
	if( evt->type == EVT_DRAW ) 
	{
	    draw_window( win, wm );
	    retval = 1;
	}
	else
	{
	    if( win->win_handler )
	    {
		if( !win->win_handler( evt, wm ) )
		{
		    //Send event to children:
		    for( int c = 0; c < win->childs_num; c++ )
		    {
			evt->win = win->childs[ c ];
			if( handle_event_by_window( evt, wm ) )
			{
			    evt->win = win;
			    retval = 1;
			    goto end_of_handle;
			}
		    }
		    evt->win = win;
		}
		else
		{
		    retval = 1;
		    goto end_of_handle;
		}
	    }
end_of_handle:
	    if( win == wm->root_win )
	    {
		if( evt->type == EVT_SCREENRESIZE )
		{
		    //On screen resize:
		    recalc_regions( wm );
		    draw_window( wm->root_win, wm );
		}
	    }
	}
    }
    return retval;
}

int EVENT_LOOP_BEGIN( sundog_event* evt, window_manager* wm )
{
    int rv = 0;
    wm->device_event_handler( wm );
    if( wm->exit_request ) return 0;
    if( wm->events_count )
    {
	smutex_lock( &wm->events_mutex );

get_next_event:
	//There are unhandled events:
	//Copy current event to the "evt" buffer (prepare it for handling):
	*evt = wm->events[ wm->current_event_num ];
	//This event will be handled. So decrement count of events:
	wm->events_count--;
	//And increment FIFO pointer:
	wm->current_event_num = ( wm->current_event_num + 1 ) & ( WM_EVENTS - 1 );
	if( evt->type == EVT_NULL && wm->events_count ) goto get_next_event;

	smutex_unlock( &wm->events_mutex );

	//Check the event and handle it:
	if( evt->type != EVT_NULL )
	{
	    if( check_event( evt, wm ) == 0 )
	    {
	    	handle_event( evt, wm );
	    	rv = 1;
	    }
	}
    }
    return rv;
}

//#define SHOW_OPTIMIZED_EVENTS 1

int EVENT_LOOP_END( window_manager* wm )
{
    if( wm->exit_request ) return 1;
    if( wm->frame_event_request )
    {
	if( g_smem_error )
	{
	    size_t size = g_smem_error;
	    g_smem_error = 0;
	    char ts[ 256 ];
	    sprintf( ts, "Memory allocation error\nCan't allocate " PRINTF_SIZET " bytes", PRINTF_SIZET_CONV size );
	    dialog( 0, ts, "Close", wm );
	}
	if( wm->timers_num )
	{
	    stime_ms_t cur_time = stime_ms();
	    for( int t = 0; t < wm->timers_num; t++ )
	    {
		sundog_timer* timer = &wm->timers[ t ];
		if( timer->handler )
		{
		    if( timer->deadline <= cur_time )
		    {
			int timer_id = timer->id;
			timer->handler( timer->data, timer, wm ); 
			if( timer->handler && timer->id == timer_id )
			{
			    timer->deadline = stime_ms() + timer->delay;
			}
		    }
		}
	    }
	}
	app_event_handler( &wm->frame_evt, wm );
	wm->frame_event_request = 0;
    }
    if( wm->screen_buffer_preserved == 0 && wm->screen_changed )
    {
	//If buffer is not preserved,
	//gfx can be redrawn in this place only
	//Ways to this place:
	// 1) draw_window();
	// 2) some gfx function called;
	// 3) screen_change().
	wm->screen_buffer_preserved = 1;
#if !defined(NOVCAP)
	if( wm->vcap ) video_capture_frame_begin( wm );
#endif
	draw_window( wm->root_win, wm );
#if !defined(NOVCAP)
	if( wm->vcap ) video_capture_frame_end( wm );
#endif
	wm->screen_buffer_preserved = 0;
    }
    wm->device_redraw_framebuffer( wm ); //Show screen changes, only if they exists.
    if( wm->flags & WIN_INIT_FLAG_OPTIMIZE_MOVE_EVENTS )
    {
	//Optimize mouse and touch events,
	//but only if we can receive the batch of the events
	if( wm->events_count )
	{
	    smutex_lock( &wm->events_mutex );
#ifdef SHOW_OPTIMIZED_EVENTS
	    int optimized = 0;
#endif
	    bool mark_optimized_area = false;
	    if( wm->events[ wm->current_event_num ].flags & EVT_FLAG_OPTIMIZED )
	    {
		//The next event to be processed is in the optimized area:
		mark_optimized_area = false;
	    }
	    else
	    {
		//The next event to be processed is in the unoptimized area:
		//this area will be optimized now and marked as OPTIMIZED (to avoid further optimization)
		mark_optimized_area = true;
	    }
	    bool move_flag = true;
#ifdef MULTITOUCH
	    uint mt_move_flags = 0xFFFFFFFF;
#endif
	    for( int i = wm->events_count - 1; i >= 0; i-- )
	    {
		int event_num = ( wm->current_event_num + i ) & ( WM_EVENTS - 1 );
		sundog_event* evt = &wm->events[ event_num ];
		if( evt->flags & EVT_FLAG_OPTIMIZED ) break; //tail of the preivous optimization
		bool reset_optimization = false;
		if( evt->type == EVT_MOUSEBUTTONDOWN || evt->type == EVT_MOUSEBUTTONUP ) reset_optimization = true;
#ifdef MULTITOUCH
		if( evt->type == EVT_TOUCHBEGIN || evt->type == EVT_TOUCHEND ) reset_optimization = true;
#endif
		if( reset_optimization )
		{
		    move_flag = true;
#ifdef MULTITOUCH
		    mt_move_flags = 0xFFFFFFFF;
		    //uint tbit = (uint)1 << evt->scancode;
		    //mt_move_flags |= tbit;
#endif
		}
		else
		{
		    if( evt->type == EVT_MOUSEMOVE )
		    {
		    	if( move_flag == false /*&& !( evt->flags & EVT_FLAG_DONTDRAW )*/ ) //we can't ignore the events with EVT_FLAG_DONTDRAW here! (we can't optimize the part of the multitouch event pack (3 fingers and more))
		    	{
#ifdef SHOW_OPTIMIZED_EVENTS
			    optimized++;
#endif
			    evt->type = EVT_NULL;
		    	}
		    	move_flag = false;
		    }
#ifdef MULTITOUCH
		    if( evt->type == EVT_TOUCHMOVE )
		    {
		    	uint tbit = (uint)1 << evt->scancode;
		    	if( ( mt_move_flags & tbit ) == 0 /*&& !( evt->flags & EVT_FLAG_DONTDRAW )*/ )
		    	{
#ifdef SHOW_OPTIMIZED_EVENTS
			    optimized++;
#endif
			    evt->type = EVT_NULL;
		    	}
		    	mt_move_flags &= ~tbit;
		    }
#endif
		}
		if( mark_optimized_area ) evt->flags |= EVT_FLAG_OPTIMIZED;
	    }
#ifdef SHOW_OPTIMIZED_EVENTS
	    if( optimized ) printf( "opt %d\n", optimized );
#endif
	    smutex_unlock( &wm->events_mutex );
	}
    }
    return 0;
}

//
// Data containers (empty windows with some data)
//

static int empty_window_handler( sundog_event* evt, window_manager* wm )
{
    return 0;
}

WINDOWPTR add_data_container( WINDOWPTR win, const char* name, void* new_data_block )
{
    remove_data_container( win, name );
    WINDOWPTR rv = new_window( name, 0, 0, 0, 0, 0, win, empty_window_handler, win->wm );
    if( !rv ) return NULL;
    rv->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
    rv->data = new_data_block;
    return rv;
}

void remove_data_container( WINDOWPTR win, const char* name )
{
    WINDOWPTR w = find_win_by_name( win, name, empty_window_handler );
    if( !w ) return;
    remove_window( w, win->wm );
}

WINDOWPTR get_data_container( WINDOWPTR win, const char* name )
{
    return find_win_by_name( win, name, empty_window_handler );
}

void* get_data_container2( WINDOWPTR win, const char* name )
{
    WINDOWPTR w = get_data_container( win, name );
    if( w )
    {
	return w->data;
    }
    return NULL;
}

//
// Status messages
//

void status_timer( void* user_data, sundog_timer* t, window_manager* wm )
{
    hide_status_message( wm );
}

int status_message_handler( sundog_event* evt, window_manager* wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    switch( evt->type )
    {
	case EVT_DRAW:
	    wbd_lock( win );
	    draw_frect( 0, 1, win->xsize, win->ysize, wm->color0, wm );
	    if( wm->status_message )
	    {
		wm->cur_font_color = wm->color3;
		draw_string( wm->status_message, 0, 1, wm );
	    }
	    draw_frect( 0, 0, win->xsize, 1, blend( win->color, BORDER_COLOR_WITHOUT_OPACITY, BORDER_OPACITY ), wm );
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    hide_status_message( wm );
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    smem_free( wm->status_message );
	    wm->status_message = 0;
	    retval = 1;
	    break;
    }
    return retval;
}

void show_status_message( const char* msg, int t, window_manager* wm )
{
    if( t == 0 ) t = 2;
    int lines = 1;
    for( int i = 0; ; i++ )
    {
	int c = msg[ i ];
	if( c == 0 ) break;
	if( c == 0xA ) lines++;
    }
    smem_free( wm->status_message );
    wm->status_message = (char*)smem_new( smem_strlen( msg ) + 1 );
    wm->status_message[ 0 ] = 0;
    smem_strcat_resize( wm->status_message, msg );
    if( wm->status_window == 0 )
    {
	wm->status_window = new_window( "status", 0, 0, 8, 8, wm->color0, wm->root_win, status_message_handler, wm );
	wm->status_timer = add_timer( status_timer, 0, 1000 * t, wm );
    }
    else
    {
	reset_timer( wm->status_timer, 1000 * t, wm );
    }

    set_window_controller( wm->status_window, 0, wm, (WCMD)0, CEND );
    set_window_controller( wm->status_window, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)font_char_y_size( wm->status_window->font, wm ) * lines + 1, CEND );
    set_window_controller( wm->status_window, 2, wm, CPERC, (WCMD)100, CEND );
    set_window_controller( wm->status_window, 3, wm, CPERC, (WCMD)100, CEND );

    show_window( wm->status_window );
    recalc_regions( wm );
    force_redraw_all( wm );
}

void hide_status_message( window_manager* wm )
{
    remove_window( wm->status_window, wm );
    remove_timer( wm->status_timer, wm );
    wm->status_window = 0;
    wm->status_timer = -1;
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
}

//
// Timers
//

void pack_timers( window_manager* wm )
{
    int r = 0;
    int w = 0;
    for( ; r < wm->timers_num; r++ )
    {
	if( wm->timers[ r ].handler )
	{
	    smem_copy( &wm->timers[ w ], &wm->timers[ r ], sizeof( sundog_timer ) );
	    w++;
	}
    }
    wm->timers_num = w;
}

int add_timer( sundog_timer_handler hnd, void* data, stime_ms_t delay, window_manager* wm )
{
    if( wm->timers_num >= WM_TIMERS )
	return -1;
    int t = wm->timers_num;
    wm->timers[ t ].handler = hnd;
    wm->timers[ t ].data = data;
    wm->timers[ t ].deadline = stime_ms() + delay;
    wm->timers[ t ].delay = delay;
    wm->timers_id_counter++;
    wm->timers_id_counter &= 0xFFFFFFF;
    wm->timers[ t ].id = wm->timers_id_counter;
    wm->timers_num++;
    return wm->timers[ t ].id;
}

void reset_timer( int timer, window_manager* wm )
{
    for( int i = 0; i < wm->timers_num; i++ )
    {
	if( wm->timers[ i ].id == timer )
	{
	    if( wm->timers[ i ].handler )
	    {
		wm->timers[ i ].deadline = stime_ms() + wm->timers[ i ].delay;
		pack_timers( wm );
	    }
	    break;
	}
    }
}

void reset_timer( int timer, stime_ms_t new_delay, window_manager* wm )
{
    for( int i = 0; i < wm->timers_num; i++ )
    {
	if( wm->timers[ i ].id == timer )
	{
	    if( wm->timers[ i ].handler )
	    {
		wm->timers[ i ].deadline = stime_ms() + new_delay;
		wm->timers[ i ].delay = new_delay;
		pack_timers( wm );
	    }
	    break;
	}
    }
}

void remove_timer( int timer, window_manager* wm )
{
    if( timer == -1 ) return;
    for( int i = 0; i < wm->timers_num; i++ )
    {
	if( wm->timers[ i ].id == timer )
	{
	    if( wm->timers[ i ].handler )
	    {
		wm->timers[ i ].handler = 0;
		pack_timers( wm );
	    }
	    break;
	}
    }
}

//
// Window decorations
//

#define DRAG_LEFT	1
#define DRAG_RIGHT	2
#define DRAG_TOP	4
#define DRAG_BOTTOM	8
#define DRAG_MOVE	16
struct decorator_data
{
    WINDOWPTR win;
    
    int start_win_x;
    int start_win_y;
    int start_win_xs;
    int start_win_ys;
    int start_pen_x;
    int start_pen_y;
    int drag_mode; //DRAG_*
    int prev_x;
    int prev_y;
    int prev_xsize;
    int prev_ysize;
    uint flags;
    
    int border;
    int header;
    
    int initial_xsize; //desired initial size of the child window (without decorator)
    int initial_ysize; //...
    
    WINDOWPTR close;
    WINDOWPTR minimize;
    WINDOWPTR help;
};

static void check_decorator_position_and_size( WINDOWPTR dec );
static void handle_decorator_flags( WINDOWPTR dec );
static void reset_decorator( WINDOWPTR dec );

decorator_data* get_decorator_data( WINDOWPTR child )
{
    decorator_data* rv = 0;
    while( 1 )
    {
	WINDOWPTR dec = child->parent;
	if( !dec ) break;
	if( dec->win_handler != decorator_handler ) break;
	rv = (decorator_data*)dec->data;
	break;
    }
    if( !rv )
    {
	if( child )
	    slog( "Can't get decorator data of window %s\n", child->name );
    }
    return rv;
}

int decorator_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    decorator_data* data = (decorator_data*)win->data;
    int dx, dy;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( decorator_data );
	    break;
	case EVT_AFTERCREATE:
	    data->win = win;
	    data->drag_mode = 0;
	    data->flags = 0;
	    retval = 1;
	    break;
	case EVT_BUTTONDOWN:
	    if( evt->key == KEY_ESCAPE )
            {
                if( data->flags & DECOR_FLAG_CLOSE_ON_ESCAPE )
                {
		    WINDOWPTR cwin = win->childs[ 0 ];
		    if( cwin )
		    {
			send_event( cwin, EVT_CLOSEREQUEST, wm );
		    }
            	    retval = 1;
                }
            }
            break;
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		bring_to_front( win, wm );
		recalc_regions( wm );
		if( ( data->flags & DECOR_FLAG_STATIC ) == 0 )
		{
		    data->start_pen_x = evt->x;
		    data->start_pen_y = evt->y;
		    data->start_win_x = win->x;
		    data->start_win_y = win->y;
		    data->start_win_xs = win->xsize;
		    data->start_win_ys = win->ysize;
		    data->drag_mode = DRAG_MOVE;
		    if(
			ry >= data->border && 
			!( data->flags & DECOR_FLAG_NOBORDER ) && 
			!( data->flags & DECOR_FLAG_NORESIZE )
		    )
		    {
			if( rx < data->border + 8 ) data->drag_mode |= DRAG_LEFT;
			if( rx >= win->xsize - data->border - 8 ) data->drag_mode |= DRAG_RIGHT;
			if( ry >= win->ysize - data->border - 8 ) data->drag_mode |= DRAG_BOTTOM;
	    	    }
	    	    if( data->drag_mode & ( DRAG_LEFT | DRAG_RIGHT | DRAG_TOP | DRAG_BOTTOM ) )
	    	    {
			if( ( data->flags & DECOR_FLAG_NORESIZE_WHEN_MAX ) && ( data->flags & DECOR_FLAG_MAXIMIZED ) )
	    		    data->drag_mode = 0;
	    	    }
	    	}
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    if( data->drag_mode )
	    {
		data->drag_mode = 0;
		if( ( data->flags & DECOR_FLAG_STATIC ) ||
		    ( data->flags & DECOR_FLAG_NOBORDER ) )
		{
		    break;
		}
		if( ( evt->key == MOUSE_BUTTON_LEFT ) &&
		    ( evt->flags & EVT_FLAG_DOUBLECLICK ) )
		{
		    //Toggle maximize:
		    if( data->flags & DECOR_FLAG_MAXIMIZED )
		    {
			//Back to the previous size:
			win->x = data->prev_x;
			win->y = data->prev_y;
			win->xsize = data->prev_xsize;
			win->ysize = data->prev_ysize;
		    }
		    else
		    {
			//Save current size and maximize:
			data->prev_x = win->x;
			data->prev_y = win->y;
			data->prev_xsize = win->xsize;
			data->prev_ysize = win->ysize;
		    }
		    data->flags ^= DECOR_FLAG_MAXIMIZED;
		    handle_decorator_flags( win );
		    recalc_regions( wm );
		    draw_window( wm->root_win, wm );
		}
	    }
	    break;
	case EVT_UNFOCUS:
	    data->drag_mode = 0;
	    retval = 1;
	    break;
	case EVT_MOUSEMOVE:
	    if( data->flags & DECOR_FLAG_STATIC )
	    {
		retval = 1;
		break;
	    }
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {	
		dx = evt->x - data->start_pen_x;
		dy = evt->y - data->start_pen_y;
		if( dx == 0 && dy == 0 )
		{
		    retval = 1;
		    break;
		}
		if( data->drag_mode == DRAG_MOVE )
		{
		    //Move:
		    int prev_x = win->x;
		    int prev_y = win->y;
		    win->x = data->start_win_x + dx;
		    win->y = data->start_win_y + dy;
		    if( prev_x != win->x || prev_y != win->y )
		    {
			if( data->flags & DECOR_FLAG_MAXIMIZED )
			{
			    win->xsize = data->prev_xsize;
			    win->ysize = data->prev_ysize;
			    data->flags &= ~DECOR_FLAG_MAXIMIZED;
			}
		    }
		}
		if( !( data->flags & DECOR_FLAG_NOBORDER ) )
		{
		    if( data->drag_mode & DRAG_LEFT )
		    {
			int prev_x = win->x;
			int prev_xsize = win->xsize;
			win->x = data->start_win_x + dx;
			win->xsize = data->start_win_xs - dx;
			if( prev_x != win->x || prev_xsize != win->xsize )
			{
			    data->flags &= ~DECOR_FLAG_MAXIMIZED;
			}
		    }
		    if( data->drag_mode & DRAG_RIGHT )
		    {
			int prev_xsize = win->xsize;
			win->xsize = data->start_win_xs + dx;
			if( prev_xsize != win->xsize )
			{
			    data->flags &= ~DECOR_FLAG_MAXIMIZED;
			}
		    }
		    if( data->drag_mode & DRAG_BOTTOM )
		    {
			int prev_ysize = win->ysize;
			win->ysize = data->start_win_ys + dy;
			if( prev_ysize != win->ysize )
			{
			    data->flags &= ~DECOR_FLAG_MAXIMIZED;
			}
		    }
		} //!NOBORDER
    	        handle_decorator_flags( win );
		check_decorator_position_and_size( win );
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    if( win->childs_num )
	    {
		WINDOWPTR cw = win->childs[ 0 ];
		if( !cw ) break;
		
		wbd_lock( win );
		
		wm->cur_font_color = wm->header_text_color;
		bool noname = false;
		int xborder = 0;
		int yborder = 0;
		if( data->flags & DECOR_FLAG_NOHEADER ) noname = true;
		if( !( data->flags & DECOR_FLAG_NOBORDER ) )
		{
		    xborder = data->border;
		    yborder = data->border;
		}
		draw_frect( xborder, 0, win->xsize - xborder * 2, win->ysize, cw->color, wm );
		if( cw->name && !noname )
		{
		    draw_string( cw->name, xborder + wm->interelement_space, ( data->header + yborder - char_y_size( wm ) ) / 2, wm );
		}
		if( xborder )
		{
		    draw_frect( 0, 0, xborder, win->ysize, wm->decorator_border, wm );
		    draw_frect( win->xsize - xborder, 0, xborder, win->ysize, wm->decorator_border, wm );
		}

		wbd_draw( wm );
		wbd_unlock( wm );
	    }
	    else
	    {
		//Empty decorator. Just remove it:
		remove_window( win, wm );
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_SCREENRESIZE:
	    //Native app window parameters has been changed (resized or rotated):
	    if( wm->flags & WIN_INIT_FLAG_FULLSCREEN ) 
	    {
		//Just reset window with decorator to its initial state (if it is not minimized/maximized):
		//(in future updates we can add some flag like DONT_CENTER)
		reset_decorator( win );
	    }
	    break;
    }
    return retval;
}

static void check_decorator_position_and_size( WINDOWPTR dec )
{
    if( !dec ) return;
    if( dec->childs_num == 0 || !dec->childs ) return;
    WINDOWPTR cw = dec->childs[ 0 ];
    if( !cw ) return;
    window_manager* wm = dec->wm;
    decorator_data* data = (decorator_data*)dec->data;

    if( data->flags & DECOR_FLAG_MAXIMIZED ) return;

    if( dec->y < 0 ) 
	dec->y = 0;
    if( dec->y > dec->parent->ysize - ( wm->decor_header_size + wm->decor_border_size ) )
	dec->y = dec->parent->ysize - ( wm->decor_header_size + wm->decor_border_size );
    if( dec->x > dec->parent->xsize - wm->scrollbar_size )
        dec->x = dec->parent->xsize - wm->scrollbar_size;
    if( dec->x + dec->xsize < wm->scrollbar_size )
	dec->x = wm->scrollbar_size - dec->xsize;
    if( dec->xsize < wm->scrollbar_size + data->border * 2 ) 
	dec->xsize = wm->scrollbar_size + data->border * 2;
    if( dec->ysize < ( wm->decor_header_size + wm->decor_border_size ) ) 
	dec->ysize = ( wm->decor_header_size + wm->decor_border_size );
}

static void handle_decorator_flags( WINDOWPTR dec )
{
    if( !dec ) return;
    if( dec->childs_num == 0 || !dec->childs ) return;
    WINDOWPTR cw = dec->childs[ 0 ];
    if( !cw ) return;
    window_manager* wm = dec->wm;
    decorator_data* data = (decorator_data*)dec->data;
    
    int border = wm->decor_border_size;
    int header = wm->decor_header_size;
    if( data->flags & DECOR_FLAG_NOBORDER )
	border = 0;
    if( data->flags & DECOR_FLAG_NOHEADER )
	header = 0;
    data->border = border;
    data->header = header;

    set_window_controller( cw, 0, wm, (WCMD)border, CEND );
    set_window_controller( cw, 1, wm, (WCMD)( border + header ), CEND );
    set_window_controller( cw, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)border, CEND );
    set_window_controller( cw, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)border, CMINVAL, (WCMD)( border + header ), CEND );

    if( data->flags & DECOR_FLAG_MAXIMIZED )
    {
	set_window_controller( dec, 0, wm, (WCMD)0, CEND );
	set_window_controller( dec, 1, wm, (WCMD)0, CEND );
	set_window_controller( dec, 2, wm, CPERC, (WCMD)100, CEND );
	set_window_controller( dec, 3, wm, CPERC, (WCMD)100, CEND );
    }
    else
    {
        remove_window_controllers( dec );
    }
    if( data->flags & DECOR_FLAG_MINIMIZED )
    {
	dec->xsize = data->border * 2 + wm->scrollbar_size;
	if( data->close ) dec->xsize += wm->scrollbar_size;
	if( data->minimize ) dec->xsize += wm->scrollbar_size;
	dec->ysize = data->border + data->header;
    }
}

static void reset_decorator( WINDOWPTR dec )
{
    if( !dec ) return;
    if( dec->childs_num == 0 || !dec->childs ) return;
    WINDOWPTR cw = dec->childs[ 0 ];
    if( !cw ) return;
    window_manager* wm = dec->wm;
    decorator_data* data = (decorator_data*)dec->data;

    handle_decorator_flags( dec );
    int border = data->border;
    int header = data->header;
    uint flags = data->flags;
    if( 1 ) //( flags & DECOR_FLAG_MAXIMIZED ) == 0 && ( flags & DECOR_FLAG_MINIMIZED ) == 0 )
    {
	int x = 0;
	int y = 0;
	int xsize = data->initial_xsize;
	int ysize = data->initial_ysize;
	x -= border;
	y -= border + header;
	int dec_xsize = xsize + border * 2;
	int dec_ysize = ysize + border * 2 + header;
	x = ( dec->parent->xsize - dec_xsize ) / 2; //centering
	y = ( dec->parent->ysize - dec_ysize ) / 2; //centering
	if( x < 0 )
	{
	    dec_xsize += x;
    	    x = 0;
        }
	if( y < 0 )
        {
	    dec_ysize += y;
    	    y = 0;
        }
	if( x + dec_xsize > dec->parent->xsize )
        {
	    dec_xsize -= ( ( x + dec_xsize ) - dec->parent->xsize );
        }
	if( y + dec_ysize > dec->parent->ysize )
        {
	    dec_ysize -= ( ( y + dec_ysize ) - dec->parent->ysize );
        }
	dec->x = x;
        dec->y = y;
	dec->xsize = dec_xsize;
        dec->ysize = dec_ysize;
        data->prev_x = x;
        data->prev_y = y;
        data->prev_xsize = dec_xsize;
        data->prev_ysize = dec_ysize;
    }

    //Get correct child window size, because its size may be used between the reset_decorator() and recalc_regions():
    recalc_controllers( dec, wm ); 
    recalc_controllers( cw, wm ); 
}

void resize_window_with_decorator( WINDOWPTR win, int xsize, int ysize, window_manager* wm ) //Centered resize
{
    decorator_data* ddata = get_decorator_data( win );
    if( !ddata ) return;
    WINDOWPTR dec = win->parent;
    if( xsize > 0 ) ddata->initial_xsize = xsize;
    if( ysize > 0 ) ddata->initial_ysize = ysize;
    int xdelta = 0;
    int ydelta = 0;
    bool use_prev_values = false;
    if( ( ddata->flags & DECOR_FLAG_MINIMIZED ) || ( ddata->flags & DECOR_FLAG_MAXIMIZED ) )
    {
	//We can't change size of the minimized/maximized decorator, so we should change the prev_xxx values:
	if( xsize > 0 )
	    xdelta = ddata->prev_xsize - ( xsize + ddata->border * 2 );
	if( ysize > 0 )
	    ydelta = ddata->prev_ysize - ( ysize + ddata->border * 2 + ddata->header );
	use_prev_values = true;
    }
    else
    {
	if( xsize > 0 )
	    xdelta = win->xsize - xsize;
	if( ysize > 0 )
	    ydelta = win->ysize - ysize;
    }
    if( xdelta )
    {
	int new_xsize;
	int new_x;
	if( use_prev_values )
	{
	    new_xsize = ddata->prev_xsize;
	    new_x = ddata->prev_x;
	}
	else
	{
	    new_xsize = dec->xsize;
	    new_x = dec->x;
	}
	new_xsize -= xdelta;
	if( new_xsize > dec->parent->xsize ) //fit to the parent window
	{
	    int s = new_xsize - dec->parent->xsize;
	    new_xsize -= s;
	    xdelta += s;
	}
	new_x += xdelta / 2; //centering
	if( use_prev_values )
	{
	    ddata->prev_xsize = new_xsize;
	    ddata->prev_x = new_x;
	}
	else
	{
	    dec->xsize = new_xsize;
	    dec->x = new_x;
	}
    }
    if( ydelta )
    {
	int new_ysize;
	int new_y;
	if( use_prev_values )
	{
	    new_ysize = ddata->prev_ysize;
	    new_y = ddata->prev_y;
	}
	else
	{
	    new_ysize = dec->ysize;
	    new_y = dec->y;
	}
	new_ysize -= ydelta;
	if( new_ysize > dec->parent->ysize ) //fit to the parent window
	{
	    int s = new_ysize - dec->parent->ysize;
	    new_ysize -= s;
	    ydelta += s;
	}
	new_y += ydelta / 2; //centering
	if( use_prev_values )
	{
	    ddata->prev_ysize = new_ysize;
	    ddata->prev_y = new_y;
	}
	else
	{
	    dec->ysize = new_ysize;
	    dec->y = new_y;
	}
    }
}

int decor_close_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    decorator_data* data = (decorator_data*)user_data;
    WINDOWPTR cwin = data->win->childs[ 0 ];
    
    if( cwin )
    {
	send_event( cwin, EVT_CLOSEREQUEST, wm );
    }
    
    return 0;
}

int decor_minimize_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    decorator_data* data = (decorator_data*)user_data;
    WINDOWPTR dwin = data->win;
    WINDOWPTR cwin = dwin->childs[ 0 ];
    if( !cwin ) return 0;
    
    //Minimize / maximize:
    if( data->flags & DECOR_FLAG_MINIMIZED )
    {
	dwin->xsize = data->prev_xsize;
	dwin->ysize = data->prev_ysize;
	data->flags &= ~DECOR_FLAG_MINIMIZED;
	rename_window( win, "-" );
    }
    else
    {
	data->prev_xsize = dwin->xsize;
	data->prev_ysize = dwin->ysize;
	data->flags |= DECOR_FLAG_MINIMIZED;
	rename_window( win, "+" );
    }
    handle_decorator_flags( dwin );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    
    return 0;
}

int decor_help_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    decorator_data* data = (decorator_data*)user_data;
    WINDOWPTR cwin = data->win->childs[ 0 ];
    if( !cwin ) return 0;
    send_event( cwin, EVT_HELPREQUEST, wm );
    return 0;
}

WINDOWPTR new_window_with_decorator(
    const char* name, 
    int xsize, 
    int ysize, 
    COLOR color,
    WINDOWPTR parent,
    void* host,
    win_handler_t win_handler,
    uint flags,
    window_manager* wm )
{
    WINDOWPTR dec = new_window( 
	"decorator", 
	0, 0, xsize, ysize,
	wm->decorator_color,
	parent,
	host,
	decorator_handler,
	wm );
    WINDOWPTR win = new_window( name, 0, 0, xsize, ysize, color, dec, host, win_handler, wm );
    decorator_data* data = (decorator_data*)dec->data;
    data->initial_xsize = xsize;
    data->initial_ysize = ysize;
    data->flags = flags;
    reset_decorator( dec );
    int xx = data->border; // + wm->interelement_space;
    if( flags & DECOR_FLAG_WITH_CLOSE )
    {
	wm->opt_button_flags = BUTTON_FLAG_FLAT;
	data->close = new_window( "x", 0, 0, wm->small_button_xsize, data->header + data->border, dec->color, dec, host, button_handler, wm );
	set_handler( data->close, decor_close_button_handler, data, wm );
	set_window_controller( data->close, 0, wm, CPERC, (WCMD)100, CSUB, (WCMD)xx + wm->small_button_xsize, CEND );
	set_window_controller( data->close, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)xx, CEND );
	xx += wm->small_button_xsize;
    }
    if( flags & DECOR_FLAG_WITH_MINIMIZE )
    {
	wm->opt_button_flags = BUTTON_FLAG_FLAT;
	data->minimize = new_window( "-", 0, 0, wm->small_button_xsize, data->header + data->border, dec->color, dec, host, button_handler, wm );
	set_handler( data->minimize, decor_minimize_button_handler, data, wm );
	set_window_controller( data->minimize, 0, wm, CPERC, (WCMD)100, CSUB, (WCMD)xx + wm->small_button_xsize, CEND );
	set_window_controller( data->minimize, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)xx, CEND );
	xx += wm->small_button_xsize;
    }
    if( flags & DECOR_FLAG_WITH_HELP )
    {
	wm->opt_button_flags = BUTTON_FLAG_FLAT;
	data->help = new_window( "?", 0, 0, wm->small_button_xsize, data->header + data->border, dec->color, dec, host, button_handler, wm );
	set_handler( data->help, decor_help_button_handler, data, wm );
	set_window_controller( data->help, 0, wm, CPERC, (WCMD)100, CSUB, (WCMD)xx + wm->small_button_xsize, CEND );
	set_window_controller( data->help, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)xx, CEND );
	xx += wm->small_button_xsize;
    }
    return dec;
}

WINDOWPTR new_window_with_decorator( 
    const char* name, 
    int xsize, 
    int ysize, 
    COLOR color,
    WINDOWPTR parent,
    win_handler_t win_handler,
    uint flags,
    window_manager* wm )
{
    void* host = NULL;
    if( parent )
    {
	host = parent->host;
    }
    return new_window_with_decorator( name, xsize, ysize, color, parent, host, win_handler, flags, wm );
}

//
// Auto alignment
//

void btn_autoalign_init( btn_autoalign_data* aa, window_manager* wm, uint32_t flags )
{
    smem_clear( aa, sizeof( btn_autoalign_data ) );
    aa->flags = flags;
    aa->wm = wm;
}

void btn_autoalign_deinit( btn_autoalign_data* aa )
{
    smem_free( aa->ww );
}

void btn_autoalign_add( btn_autoalign_data* aa, WINDOWPTR w, uint32_t flags )
{
    if( !w || !aa ) return;

    if( smem_objarray_write_d( (void***)&aa->ww, w, false, aa->i ) ) return;
    aa->i++;
}

void btn_autoalign_next_line( btn_autoalign_data* aa, uint32_t flags )
{
    if( !aa ) return;
    if( !aa->ww ) return;
    if( aa->i == 0 ) return;

    window_manager* wm = aa->wm;

    int items = 0;
    int ysize = 0;
    for( int i = 0; i < aa->i; i++ )
    {
	WINDOWPTR w = (WINDOWPTR)aa->ww[ i ];
	if( !w ) continue;
	if( w->flags & WIN_FLAG_ALWAYS_INVISIBLE ) continue;
	if( w->ysize > ysize ) ysize = w->ysize;
	items++;
    }

    if( items > 0 )
    {
	int align = flags & 7;
	if( align == BTN_AUTOALIGN_LINE_EVENLY )
	{
	    int item = 0;
	    for( int i = 0; i < aa->i; i++ )
    	    {
		WINDOWPTR w = (WINDOWPTR)aa->ww[ i ];
		if( !w ) continue;
		if( w->flags & WIN_FLAG_ALWAYS_INVISIBLE ) continue;
		w->y = aa->y;
		int x1 = item * 100 / items;
	        int x2 = (item+1) * 100 / items;
	        set_window_controller( w, 0, wm, CPERC, (WCMD)x1, CEND );
	        if( item == items - 1 )
	            set_window_controller( w, 2, wm, CPERC, (WCMD)100, CEND );
	        else
	            set_window_controller( w, 2, wm, CPERC, (WCMD)x2, CSUB, (WCMD)wm->interelement_space2, CEND );
		item++;
	    }
	}
	aa->y += ysize + wm->interelement_space;
    }

    aa->i = 0;
}

//
// Drawing functions
//

void win_draw_lock( WINDOWPTR win, window_manager* wm )
{
    wm->cur_opacity = 255;
    wm->device_screen_lock( win, wm );
}

void win_draw_unlock( WINDOWPTR win, window_manager* wm )
{
    wm->device_screen_unlock( win, wm );
}

COLOR get_color_from_string( char* str )
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    get_color_from_string( str, &r, &g, &b );
    return get_color( r, g, b );
}

void get_string_from_color( char* dest, uint dest_size, COLOR c )
{
    get_string_from_color( dest, dest_size, red( c ), green( c ), blue( c ) );
}

void win_draw_rect( WINDOWPTR win, int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
    win_draw_line( win, x, y, x + xsize - 1, y, color, wm );
    win_draw_line( win, x + xsize - 1, y, x + xsize - 1, y + ysize - 1, color, wm );
    win_draw_line( win, x + xsize - 1, y + ysize - 1, x, y + ysize - 1, color, wm );
    win_draw_line( win, x, y + ysize - 1, x, y, color, wm );
}

void win_draw_frect( WINDOWPTR win, int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x += win->screen_x;
	y += win->screen_y;
	if( win->reg->numRects )
	{
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
		wm->device_draw_frect( nx, ny, nxsize, nysize, color, wm );
		wm->screen_changed++;
	    }
	}
    }
}

void win_draw_image_ext( 
    WINDOWPTR win, 
    int x, 
    int y, 
    int dest_xsize, 
    int dest_ysize,
    int source_x,
    int source_y,
    sdwm_image* img, 
    window_manager* wm )
{
    if( source_x < 0 ) { dest_xsize += source_x; x -= source_x; source_x = 0; }
    if( source_y < 0 ) { dest_ysize += source_y; y -= source_y; source_y = 0; }
    if( source_x >= img->xsize ) return;
    if( source_y >= img->ysize ) return;
    if( source_x + dest_xsize > img->xsize ) dest_xsize -= ( source_x + dest_xsize ) - img->xsize;
    if( source_y + dest_ysize > img->ysize ) dest_ysize -= ( source_y + dest_ysize ) - img->ysize;
    if( dest_xsize <= 0 ) return;
    if( dest_ysize <= 0 ) return;
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x += win->screen_x;
	y += win->screen_y;
	int xsize = dest_xsize;
	int ysize = dest_ysize;
	if( win->reg->numRects )
	{
	    for( int r = 0; r < win->reg->numRects; r++ )
	    {
		int rx1 = win->reg->rects[ r ].left;
		int rx2 = win->reg->rects[ r ].right;
		int ry1 = win->reg->rects[ r ].top;
		int ry2 = win->reg->rects[ r ].bottom;

		//Control box size:
		int src_x = source_x;
		int src_y = source_y;
		int nx = x;
		int ny = y;
		int nxsize = xsize;
		int nysize = ysize;
		if( nx < rx1 ) { nxsize -= ( rx1 - nx ); src_x += ( rx1 - nx ); nx = rx1; }
		if( ny < ry1 ) { nysize -= ( ry1 - ny ); src_y += ( ry1 - ny ); ny = ry1; }
		if( nx + nxsize <= rx1 ) continue;
		if( ny + nysize <= ry1 ) continue;
		if( nx + nxsize > rx2 ) nxsize -= nx + nxsize - rx2;
		if( ny + nysize > ry2 ) nysize -= ny + nysize - ry2;
		if( nx >= rx2 ) continue;
		if( ny >= ry2 ) continue;
		if( nxsize < 0 ) continue;
		if( nysize < 0 ) continue;
        	
		//Draw it:
		wm->device_draw_image( nx, ny, nxsize, nysize, src_x, src_y, img, wm );
		wm->screen_changed++;
	    }
	}
    }
}

void win_draw_image( 
    WINDOWPTR win, 
    int x, 
    int y, 
    sdwm_image* img, 
    window_manager* wm )
{
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x += win->screen_x;
	y += win->screen_y;
	if( win->reg->numRects )
	{
	    for( int r = 0; r < win->reg->numRects; r++ )
	    {
		int rx1 = win->reg->rects[ r ].left;
		int rx2 = win->reg->rects[ r ].right;
		int ry1 = win->reg->rects[ r ].top;
		int ry2 = win->reg->rects[ r ].bottom;

		//Control box size:
		int src_x = 0;
		int src_y = 0;
		int nx = x;
		int ny = y;
		int nxsize = img->xsize;
		int nysize = img->ysize;
		if( nx < rx1 ) { nxsize -= ( rx1 - nx ); src_x += ( rx1 - nx ); nx = rx1; }
		if( ny < ry1 ) { nysize -= ( ry1 - ny ); src_y += ( ry1 - ny ); ny = ry1; }
		if( nx + nxsize <= rx1 ) continue;
		if( ny + nysize <= ry1 ) continue;
		if( nx + nxsize > rx2 ) nxsize -= nx + nxsize - rx2;
		if( ny + nysize > ry2 ) nysize -= ny + nysize - ry2;
		if( nx >= rx2 ) continue;
		if( ny >= ry2 ) continue;
		if( nxsize < 0 ) continue;
		if( nysize < 0 ) continue;
        	
		//Draw it:
		wm->device_draw_image( nx, ny, nxsize, nysize, src_x, src_y, img, wm );
		wm->screen_changed++;
	    }
	}
    }
}

#define cbottom 1
#define ctop 2
#define cleft 4
#define cright 8
inline int line_clip_make_code( int x, int y, int clip_x1, int clip_y1, int clip_x2, int clip_y2 )
{
    int code = 0;
    if( y >= clip_y2 ) code = cbottom;
    else if( y < clip_y1 ) code = ctop;
    if( x >= clip_x2 ) code += cright;
    else if( x < clip_x1 ) code += cleft;
    return code;
}

bool line_clip( int* x1, int* y1, int* x2, int* y2, int clip_x1, int clip_y1, int clip_x2, int clip_y2 )
{
    //Cohen-Sutherland line clipping algorithm:
    int code0;
    int code1;
    int out_code;
    int x;
    int y;
    code0 = line_clip_make_code( *x1, *y1, clip_x1, clip_y1, clip_x2, clip_y2 );
    code1 = line_clip_make_code( *x2, *y2, clip_x1, clip_y1, clip_x2, clip_y2 );
    while( code0 || code1 )
    {
	if( code0 & code1 ) 
	{
	    //Trivial reject
	    return 0;
	}
	else
	{
	    //Failed both tests, so calculate the line segment to clip
	    if( code0 )
		out_code = code0; //Clip the first point
	    else
		out_code = code1; //Clip the last point
	
	    while( 1 )
	    {    
		if( out_code & cbottom )
		{
		    //Clip the line to the bottom of the viewport
		    y = clip_y2 - 1;
		    x = *x1 + ( *x2 - *x1 ) * ( y - *y1 ) / ( *y2 - *y1 );
		    break;
		}
		if( out_code & ctop )
		{
		    y = clip_y1;
		    x = *x1 + ( *x2 - *x1 ) * ( y - *y1 ) / ( *y2 - *y1 );
		    break;
		}
		if( out_code & cright )
		{
		    x = clip_x2 - 1;
		    y = *y1 + ( *y2 - *y1 ) * ( x - *x1 ) / ( *x2 - *x1 );
		    break;
		}
		if( out_code & cleft )
		{
		    x = clip_x1;
		    y = *y1 + ( *y2 - *y1 ) * ( x - *x1 ) / ( *x2 - *x1 );
		    break;
		}
		return 0; //Wrong code!
	    }
	    
	    if( out_code == code0 )
	    { 
		//Modify the first coordinate 
		*x1 = x; *y1 = y;
		code0 = line_clip_make_code( *x1, *y1, clip_x1, clip_y1, clip_x2, clip_y2 );
	    }
	    else
	    { 
		//Modify the second coordinate
		*x2 = x; *y2 = y;
		code1 = line_clip_make_code( *x2, *y2, clip_x1, clip_y1, clip_x2, clip_y2 );
	    }
	}
    }
    
    return 1;
}

void win_draw_line( WINDOWPTR win, int x1, int y1, int x2, int y2, COLOR color, window_manager* wm )
{
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x1 += win->screen_x;
	y1 += win->screen_y;
	x2 += win->screen_x;
	y2 += win->screen_y;
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
		wm->device_draw_line( lx1, ly1, lx2, ly2, color, wm );
		wm->screen_changed++;
	    }
	}
    }
}

void screen_changed( window_manager* wm )
{
    wm->screen_changed++;
}

//
// Video capture
//

bool video_capture_supported( window_manager* wm )
{
#ifndef NOVCAP
    return device_video_capture_supported( wm );
#else
    return false;
#endif
}

void video_capture_set_in_name( const char* name, window_manager* wm )
{
#ifndef NOVCAP
    smem_free( wm->vcap_in_name );
    wm->vcap_in_name = smem_strdup( name );
#endif
}

const char* video_capture_get_file_ext( window_manager* wm )
{
#ifndef NOVCAP
    return device_video_capture_get_file_ext( wm );
#else
    return 0;
#endif
}

int video_capture_start( window_manager* wm )
{
    int rv = -1;
#ifndef NOVCAP
    if( wm->vcap == 0 )
    {
	rv = device_video_capture_start( wm );
	if( rv == 0 )
	{
	    slog( "video_capture_start( %d, %d, %d, %d ) ok\n", wm->screen_xsize, wm->screen_ysize, wm->vcap_in_fps, wm->vcap_in_bitrate_kb );
	    wm->vcap = 1;
	}
	else
	{
	    slog( "video_capture_start( %d, %d, %d, %d ) error %d\n", wm->screen_xsize, wm->screen_ysize, wm->vcap_in_fps, wm->vcap_in_bitrate_kb, rv );
	}
    }
#endif
    return rv;
}

int video_capture_frame_begin( window_manager* wm )
{
#ifndef NOVCAP
    if( wm->vcap )
	return device_video_capture_frame_begin( wm );
#endif
    return -1;
}

int video_capture_frame_end( window_manager* wm )
{
#ifndef NOVCAP
    if( wm->vcap )
	return device_video_capture_frame_end( wm );
#endif
    return -1;
}

int video_capture_stop( window_manager* wm )
{
    int rv = -1;
#ifndef NOVCAP
    if( wm->vcap )
    {
	rv = device_video_capture_stop( wm );
	if( rv == 0 )
	{
	    slog( "video_capture_stop() ok\n" );
    	    wm->vcap = 0;
    	}
    	else
	    slog( "video_capture_stop() error %d\n", rv );
    }
#endif
    return rv;
}

int video_capture_encode( window_manager* wm )
{
    int rv = -1;
#ifndef NOVCAP
    slog( "Video capture encoding to %s ...\n", wm->vcap_in_name );
    stime_ms_t t = stime_ms();
    rv = device_video_capture_encode( wm );
    slog( "Video capture encoding finished (%d). Time: %f sec\n", rv, (float)( stime_ms() - t ) / (float)1000 );
#endif
    return rv;
}

//
// Strings
//

const char* wm_get_string( wm_string str_id )
{
    const char* str = 0;
    const char* lang = slocale_get_lang();
    while( 1 )
    {
	if( smem_strstr( lang, "ru_" ) )
	{
	    switch( str_id )
	    {
		case STR_WM_OKCANCEL: str = "OK;Отмена"; break;
		case STR_WM_CANCEL: str = "Отмена"; break;
		case STR_WM_YESNO: str = "Да;Нет"; break;
		case STR_WM_YES: str = "Да"; break;
		case STR_WM_YES_CAP: str = "ДА"; break;
    		case STR_WM_NO: str = "Нет"; break;
    		case STR_WM_NO_CAP: str = "НЕТ"; break;
		case STR_WM_ON_CAP: str = "ВКЛ"; break;
		case STR_WM_OFF_CAP: str = "ВЫКЛ"; break;
		case STR_WM_CLOSE: str = "Закрыть"; break;
		case STR_WM_RESET: str = "Сброс"; break;
    		case STR_WM_RESET_ALL: str = "Сбросить все"; break;
		case STR_WM_LOAD: str = "Загрузить"; break;
    		case STR_WM_SAVE: str = "Сохранить"; break;
    	        case STR_WM_INFO: str = "Инфо"; break;
		case STR_WM_AUTO: str = "Авто"; break;
		case STR_WM_FIND: str = "Найти"; break;
		case STR_WM_EDIT: str = "Редакт."; break;
		case STR_WM_NEW: str = "Новый"; break;
		case STR_WM_DELETE: str = "Удалить"; break;
		case STR_WM_DELETE2: str = "УДАЛИТЬ"; break;
		case STR_WM_RENAME: str = "Переименовать"; break;
		case STR_WM_RENAME_FILE2: str = "ПЕРЕИМЕНОВАТЬ ФАЙЛ"; break;
		case STR_WM_RENAME_DIR2: str = "ПЕРЕИМЕНОВАТЬ ДИРЕКТОРИЮ"; break;
		case STR_WM_CUT: str = "Вырезать"; break;
		case STR_WM_CUT2: str = "Вырез."; break;
		case STR_WM_COPY: str = "Скопировать"; break;
		case STR_WM_COPY2: str = "Скопир."; break;
		case STR_WM_COPY_ALL: str = "Скопировать все"; break;
		case STR_WM_PASTE: str = "Вставить"; break;
		case STR_WM_PASTE2: str = "Встав."; break;
		case STR_WM_CREATE_DIR: str = "Создать директорию"; break;
    		case STR_WM_DELETE_DIR2: str = "УДАЛИТЬ ДИРЕКТРИЮ"; break;
    		case STR_WM_DELETE_CUR_DIR: str = "Удалить текущ. директорию"; break;
    		case STR_WM_DELETE_CUR_DIR2: str = "УДАЛИТЬ ТЕКУЩУЮ ДИРЕКТРИЮ"; break;
    	        case STR_WM_RECURS: str = "РЕКУРСИВНО"; break;
    	        case STR_WM_ARE_YOU_SURE: str = "Вы уверены, что хотите выполнить следующую операцию?"; break;
		case STR_WM_ERROR: str = "Ошибка"; break;
		case STR_WM_NOT_FOUND: str = "не найден"; break;
		case STR_WM_LOG: str = "Лог"; break;
		case STR_WM_SAVE_LOG: str = "Сохранить лог"; break;
		case STR_WM_EMAIL_LOG: str = "Послать лог"; break;
		case STR_WM_SYSEXPORT: str = "Системный экспорт"; break;
		case STR_WM_SYSEXPORT2: str = "Открыть в..."; break;
		case STR_WM_SYSIMPORT: str = "Системный импорт"; break;
		case STR_WM_SELECT_FILE_TO_EXPORT: str = "Выберите файл для экспорта"; break;
		case STR_WM_APPLY: str = "Применить"; break;
		case STR_WM_APPLY2: str = "Примен."; break;

		case STR_WM_MS: str = "мс"; break;
		case STR_WM_SEC: str = "с"; break;
		case STR_WM_MINUTE: str = "м"; break;
		case STR_WM_MINUTE_LONG: str = "мин"; break;
		case STR_WM_HOUR: str = "ч"; break;
		case STR_WM_HZ: str = "Гц"; break;
		case STR_WM_INCH: str = "Дюйм"; break;
		case STR_WM_DECIBEL: str = "дБ"; break;
		case STR_WM_BIT: str = "бит"; break;
		case STR_WM_BYTES: str = "байт"; break;

#ifdef DEMO_MODE
		case STR_WM_DEMOVERSION_FN: str = "!К сожалению, эта функция не доступна в демо-версии"; break;
		case STR_WM_DEMOVERSION: str = "Это пробная версия"; break;
    		case STR_WM_DEMOVERSION_BUY: str = "!Пожалуйста, скачайте полную версию приложения"; break;
#endif

#ifdef OS_ANDROID
		case STR_WM_RES_UNPACKING: str = "Распаковка и проверка ..."; break;
                case STR_WM_RES_UNKNOWN: str = "неизвестная ошибка"; break;
                case STR_WM_RES_LOCAL_RES: str = "нет доступа к лок. ресурсам"; break;
                case STR_WM_RES_SMEM_CARD: str = "нет доступа к карте памяти, или на ней кончилось место"; break;
                case STR_WM_RES_UNPACK_ERROR: str = "Ошибка распаковки"; break;
#endif

		case STR_WM_PREFERENCES: str = "Настройки"; break;
		case STR_WM_PREFS_CHANGED: str = "!Настройки были изменены.\nНужно перезапустить программу для активации новых настроек.\nПерезапустить сейчас?"; break;
		case STR_WM_INTERFACE: str = "Интерфейс"; break;
    		case STR_WM_AUDIO: str = "Звук"; break;
    		case STR_WM_VIDEO: str = "Видео"; break;
    		case STR_WM_CAMERA: str = "Камера"; break;
    		case STR_WM_BACK_CAM: str = "Задняя"; break;
    		case STR_WM_FRONT_CAM: str = "Передняя"; break;
		case STR_WM_MAXFPS: str = "Макс. FPS"; break;
		case STR_WM_UI_ROTATION: str = "Поворот"; break;
		case STR_WM_UI_ROTATION2: str = "Поворот (против часов.стрелки)"; break;
		case STR_WM_CAM_ROTATION: str = "Поворот камеры"; break;
		case STR_WM_CAM_ROTATION2: str = "Поворот камеры (против час.стрелки)"; break;
		case STR_WM_CAM_MODE: str = "Режим камеры"; break;
		case STR_WM_DOUBLE_CLICK_TIME: str = "Двойной клик"; break;
		case STR_WM_CTL_TYPE: str = "Тип управления"; break;
		case STR_WM_CTL_FINGERS: str = "пальцами"; break;
		case STR_WM_CTL_PEN: str = "пером или мышкой"; break;
		case STR_WM_SHOW_ZOOM_BTNS: str = "Кнопки масштаб."; break;
		case STR_WM_SHOW_ZOOM_BUTTONS: str = "Кнопки масштабирования"; break;
    		case STR_WM_SHOW_KBD: str = "Текст.клавиатура"; break;
    		case STR_WM_HIDE_SYSTEM_BARS: str = "Скрыть системные панели"; break;
    		case STR_WM_LOWRES: str = "Низкое разрешение"; break;
    		case STR_WM_LOWRES_IOS_NOTICE: str = "Разрешение изменено. Для активации новых настроек вам необходимо перезапустить приложение (с принудительным завершением)"; break;
    		case STR_WM_WINDOW_PARS: str = "Параметры окна"; break;
		case STR_WM_WINDOW_WIDTH: str = "Ширина по умолчанию:"; break;
    	        case STR_WM_WINDOW_HEIGHT: str = "Высота по умолчанию:"; break;
    		case STR_WM_WINDOW_FULLSCREEN: str = "Полноэкранный режим"; break;
		case STR_WM_SET_COLOR_THEME: str = "Установка цветовой схемы"; break;
		case STR_WM_SET_UI_SCALE: str = "Масштабирование интерфейса"; break;
		case STR_WM_SHORTCUTS_SHORT: str = "Сочетания клавиш"; break;
		case STR_WM_SHORTCUTS: str = "Сочетания клавиш (shortcuts)"; break;
		case STR_WM_UI_SCALE: str = "Масштаб"; break;
		case STR_WM_COLOR_THEME: str = "Цветовая схема"; break;
		case STR_WM_COLOR_THEME_MSG_RESTART: str = "!Чтобы увидеть новые цвета, нужно перезапустить программу.\nПерезапустить сейчас?"; break;
		case STR_WM_FONTS: str = "Шрифты"; break;
		case STR_WM_FONT_MEDIUM_MONO: str = "Средний моно (паттерн)"; break;
		case STR_WM_FONT_BIG: str = "Большой (основной)"; break;
		case STR_WM_FONT_SMALL: str = "Малый (контроллеры)"; break;
		case STR_WM_FONT_UPSCALING: str = "Шрифты высокого разрешения"; break;
		case STR_WM_FONT_FSCALING: str = "Дробное масштабирование шрифтов"; break;
		case STR_WM_LANG: str = "Язык"; break;
		case STR_WM_DRIVER: str = "Драйвер"; break;
		case STR_WM_OUTPUT: str = "Выход"; break;
		case STR_WM_INPUT: str = "Вход"; break;
		case STR_WM_BUFFER: str = "Буфер"; break;
		case STR_WM_SAMPLE_RATE: str = "Частота дискр."; break;
		case STR_WM_DEVICE: str = "Устройство"; break;
		case STR_WM_INPUT_DEVICE: str = "Входное устройство"; break;
		case STR_WM_BUFFER_SIZE: str = "Размер аудиобуфера"; break;
		case STR_WM_ASIO_OPTIONS: str = "Опции ASIO"; break;
		case STR_WM_FIRST_OUT_CH: str = "Номер первого вых.канала:"; break;
		case STR_WM_FIRST_IN_CH: str = "Номер первого вх.канала:"; break;
		case STR_WM_OPTIONS: str = "Опции"; break;
		case STR_WM_ADD_OPTIONS: str = "Доп. опции"; break;
		case STR_WM_ADD_OPTIONS_ASIO: str = "Доп. опции ASIO"; break;
		case STR_WM_MEASUREMENT_MODE: str = "Без обработки звука системой"; break;
		case STR_WM_CUR_DRIVER: str = "Текущий драйвер"; break;
		case STR_WM_CUR_SAMPLE_RATE: str = "Текущая частота дискр."; break;
		case STR_WM_CUR_LATENCY: str = "Текущая задержка"; break;
		case STR_WM_ACTION: str = "Действие:"; break;
		case STR_WM_SHORTCUTS2: str = "Сочетания клавиш и MIDI:"; break;
		case STR_WM_SHORTCUT: str = "Сочетание клавиш"; break;
		case STR_WM_RESET_TO_DEF: str = "Сбросить"; break;
		case STR_WM_ENTER_SHORTCUT: str = "Нажмите что-нибудь"; break;
		case STR_WM_AUTO_SCALE: str = "Автомасштаб"; break;
		case STR_WM_PPI: str = "Кол-во пикселей на дюйм"; break;
		case STR_WM_BUTTON_SCALE: str = "Масштаб кнопок (норм.=256)"; break;
		case STR_WM_FONT_SCALE: str = "Масштаб шрифта (норм.=256)"; break;
		case STR_WM_BUTTON: str = "Кнопка"; break;
		case STR_WM_RED: str = "Красный"; break;
                case STR_WM_GREEN: str = "Зеленый"; break;
                case STR_WM_BLUE: str = "Синий"; break;
                case STR_WM_HIDE_RECENT_FILES: str = "Скрыть недавние файлы"; break;
                case STR_WM_SHOW_HIDDEN_FILES: str = "Показ. скрытые файлы"; break;

		case STR_WM_OFF_ON_MENU: str = "Выкл\nВкл"; break;
		case STR_WM_AUTO_ON_OFF_MENU: str = "Авто\nВкл\nВыкл"; break;
		case STR_WM_AUTO_YES_NO_MENU: str = "Авто\nДа\nНет"; break;
		case STR_WM_CTL_TYPE_MENU: str = "Авто\nПальцами\nПером или мышкой"; break;

		case STR_WM_FILE_NAME: str = "Имя:"; break;
		case STR_WM_FILE_PATH: str = "Путь:"; break;
		case STR_WM_FILE_RECENT: str = "Недавние:"; break;
		case STR_WM_FILE_MSG_NONAME: str = "!Введите имя файла"; break;
		case STR_WM_FILE_MSG_NOFILE: str = "!Файл пустой или не существует"; break;
	        case STR_WM_FILE_OVERWRITE: str = "Записать поверх"; break;
	        
	        default: break;
	    }
	    if( str ) break;
	}
	//Default:
	switch( str_id )
	{
	    case STR_WM_OK: str = "OK"; break;
	    case STR_WM_OKCANCEL: str = "OK;Cancel"; break;
	    case STR_WM_CANCEL: str = "Cancel"; break;
	    case STR_WM_YESNO: str = "Yes;No"; break;
	    case STR_WM_YES: str = "Yes"; break;
	    case STR_WM_YES_CAP: str = "YES"; break;
	    case STR_WM_NO: str = "No"; break;
	    case STR_WM_NO_CAP: str = "NO"; break;
	    case STR_WM_ON_CAP: str = "ON"; break;
	    case STR_WM_OFF_CAP: str = "OFF"; break;
	    case STR_WM_CLOSE: str = "Close"; break;
	    case STR_WM_RESET: str = "Reset"; break;
	    case STR_WM_RESET_ALL: str = "Reset all"; break;
	    case STR_WM_LOAD: str = "Load"; break;
    	    case STR_WM_SAVE: str = "Save"; break;
    	    case STR_WM_INFO: str = "Info"; break;
	    case STR_WM_AUTO: str = "Auto"; break;
	    case STR_WM_FIND: str = "Find"; break;
	    case STR_WM_EDIT: str = "Edit"; break;
	    case STR_WM_NEW: str = "New"; break;
	    case STR_WM_DELETE: str = "Delete"; break;
	    case STR_WM_DELETE2: str = "DELETE"; break;
	    case STR_WM_RENAME: str = "Rename"; break;
	    case STR_WM_RENAME_FILE2: str = "RENAME FILE"; break;
	    case STR_WM_RENAME_DIR2: str = "RENAME DIRECTORY"; break;
	    case STR_WM_CUT: str = "Cut"; break;
	    case STR_WM_CUT2: str = "Cut"; break;
	    case STR_WM_COPY: str = "Copy"; break;
	    case STR_WM_COPY2: str = "Copy"; break;
	    case STR_WM_COPY_ALL: str = "Copy all"; break;
	    case STR_WM_PASTE: str = "Paste"; break;
	    case STR_WM_PASTE2: str = "Paste"; break;
	    case STR_WM_CREATE_DIR: str = "Create directory"; break;
    	    case STR_WM_DELETE_DIR2: str = "DELETE DIRECTORY"; break;
    	    case STR_WM_DELETE_CUR_DIR: str = "Delete current directory"; break;
    	    case STR_WM_DELETE_CUR_DIR2: str = "DELETE CURRENT DIRECTORY"; break;
    	    case STR_WM_RECURS: str = "RECURSIVELY"; break;
    	    case STR_WM_ARE_YOU_SURE: str = "Are you sure you want to perform the following operation?"; break;
	    case STR_WM_ERROR: str = "Error"; break;
	    case STR_WM_NOT_FOUND: str = "not found"; break;
	    case STR_WM_LOG: str = "Log"; break;
	    case STR_WM_SAVE_LOG: str = "Save log"; break;
	    case STR_WM_EMAIL_LOG: str = "Send log"; break;
	    case STR_WM_SYSEXPORT: str = "System export"; break;
	    case STR_WM_SYSEXPORT2: str = "Open in..."; break;
	    case STR_WM_SYSIMPORT: str = "System import"; break;
	    case STR_WM_SELECT_FILE_TO_EXPORT: str = "Select file to export"; break;
	    case STR_WM_APPLY: str = "Apply"; break;
	    case STR_WM_APPLY2: str = "Apply"; break;

	    case STR_WM_MS: str = "ms"; break;
	    case STR_WM_SEC: str = "s"; break;
	    case STR_WM_MINUTE: str = "m"; break;
	    case STR_WM_MINUTE_LONG: str = "min"; break;
	    case STR_WM_HOUR: str = "h"; break;
	    case STR_WM_HZ: str = "Hz"; break;
	    case STR_WM_INCH: str = "Inch"; break;
	    case STR_WM_DECIBEL: str = "dB"; break;
	    case STR_WM_BIT: str = "bit"; break;
	    case STR_WM_BYTES: str = "bytes"; break;

#ifdef DEMO_MODE
	    case STR_WM_DEMOVERSION_FN: str = "!Sorry, but this feature is not available in demo version"; break;
	    case STR_WM_DEMOVERSION: str = "This is a demo version"; break;
	    case STR_WM_DEMOVERSION_BUY: str = "!Please download the full version of this application"; break;
#endif

#ifdef OS_ANDROID
	    case STR_WM_RES_UNPACKING: str = "Unpacking / Verifying ..."; break;
            case STR_WM_RES_UNKNOWN: str = "unknown"; break;
            case STR_WM_RES_LOCAL_RES: str = "can't open local resource"; break;
            case STR_WM_RES_SMEM_CARD: str = "memory card is locked by someone or it has no free space"; break;
            case STR_WM_RES_UNPACK_ERROR: str = "Unpacking error"; break;
#endif

	    case STR_WM_PREFERENCES: str = "Preferences"; break;
	    case STR_WM_PREFS_CHANGED: str = "!Some preferences changed.\nYou must restart the program to apply these changes.\nRestart now?"; break;
	    case STR_WM_INTERFACE: str = "Interface"; break;
	    case STR_WM_AUDIO: str = "Audio"; break;
	    case STR_WM_VIDEO: str = "Video"; break;
    	    case STR_WM_CAMERA: str = "Camera"; break;
    	    case STR_WM_BACK_CAM: str = "Back"; break;
    	    case STR_WM_FRONT_CAM: str = "Front"; break;
	    case STR_WM_MAXFPS: str = "Max FPS"; break;
	    case STR_WM_UI_ROTATION: str = "Rotation"; break;
	    case STR_WM_UI_ROTATION2: str = "Rotation (counterclockwise)"; break;
	    case STR_WM_CAM_ROTATION: str = "Camera rotation"; break;
	    case STR_WM_CAM_ROTATION2: str = "Cam rotation (counterclockwise)"; break;
	    case STR_WM_CAM_MODE: str = "Camera mode"; break;
	    case STR_WM_DOUBLE_CLICK_TIME: str = "Double click"; break;
	    case STR_WM_CTL_TYPE: str = "Control type"; break;
	    case STR_WM_CTL_FINGERS: str = "Fingers"; break;
	    case STR_WM_CTL_PEN: str = "Pen or Mouse"; break;
	    case STR_WM_SHOW_ZOOM_BTNS: str = "Zoom buttons"; break;
	    case STR_WM_SHOW_ZOOM_BUTTONS: str = "Zoom buttons"; break;
    	    case STR_WM_SHOW_KBD: str = "Text keyboard"; break;
    	    case STR_WM_HIDE_SYSTEM_BARS: str = "Hide system bars"; break;
    	    case STR_WM_LOWRES: str = "Low resolution"; break;
    	    case STR_WM_LOWRES_IOS_NOTICE: str = "The screen resolution has been changed. To apply the new settings, you need to restart the application (force quit and reopen)"; break;
    	    case STR_WM_WINDOW_PARS: str = "Window parameters"; break;
    	    case STR_WM_WINDOW_WIDTH: str = "Default window width:"; break;
    	    case STR_WM_WINDOW_HEIGHT: str = "Default window height:"; break;
    	    case STR_WM_WINDOW_FULLSCREEN: str = "Fullscreen"; break;
	    case STR_WM_SET_COLOR_THEME: str = "Select a color theme"; break;
	    case STR_WM_SET_UI_SCALE: str = "Set UI scale"; break;
	    case STR_WM_SHORTCUTS_SHORT:
	    case STR_WM_SHORTCUTS: str = "Shortcuts"; break;
	    case STR_WM_UI_SCALE: str = "Scale"; break;
	    case STR_WM_COLOR_THEME: str = "Color theme"; break;
	    case STR_WM_COLOR_THEME_MSG_RESTART: str = "!You must restart the program to see the new color theme.\nRestart now?"; break;
	    case STR_WM_FONTS: str = "Fonts"; break;
	    case STR_WM_FONT_MEDIUM_MONO: str = "Middle mono (pat.editor)"; break;
	    case STR_WM_FONT_BIG: str = "Big (main)"; break;
	    case STR_WM_FONT_SMALL: str = "Small (controllers)"; break;
	    case STR_WM_FONT_UPSCALING: str = "High resolution fonts"; break;
	    case STR_WM_FONT_FSCALING: str = "Fractional font scaling"; break;
	    case STR_WM_LANG: str = "Language"; break;
	    case STR_WM_DRIVER: str = "Driver"; break;
	    case STR_WM_OUTPUT: str = "Output"; break;
	    case STR_WM_INPUT: str = "Input"; break;
	    case STR_WM_BUFFER: str = "Buffer"; break;
	    case STR_WM_SAMPLE_RATE: str = "Sample rate"; break;
	    case STR_WM_DEVICE: str = "Device"; break;
	    case STR_WM_INPUT_DEVICE: str = "Input device"; break;
	    case STR_WM_BUFFER_SIZE: str = "Audio buffer size"; break;
	    case STR_WM_ASIO_OPTIONS: str = "ASIO options"; break;
	    case STR_WM_FIRST_OUT_CH: str = "First output channel:"; break;
	    case STR_WM_FIRST_IN_CH: str = "First input channel:"; break;
	    case STR_WM_OPTIONS: str = "Options"; break;
	    case STR_WM_ADD_OPTIONS: str = "Additional options"; break;
	    case STR_WM_ADD_OPTIONS_ASIO: str = "Add. ASIO options"; break;
	    case STR_WM_MEASUREMENT_MODE: str = "Minimize system signal processing"; break;
	    case STR_WM_CUR_DRIVER: str = "Current driver"; break;
	    case STR_WM_CUR_SAMPLE_RATE: str = "Current sample rate"; break;
	    case STR_WM_CUR_LATENCY: str = "Current latency"; break;
	    case STR_WM_ACTION: str = "Action:"; break;
	    case STR_WM_SHORTCUTS2: str = "Shortcuts (kbd+MIDI):"; break;
	    case STR_WM_SHORTCUT: str = "Shortcut"; break;
	    case STR_WM_RESET_TO_DEF: str = "Reset to defaults"; break;
	    case STR_WM_ENTER_SHORTCUT: str = "Enter the shortcut"; break;
	    case STR_WM_AUTO_SCALE: str = "Auto scale"; break;
	    case STR_WM_PPI: str = "PPI (Pixels Per Inch)"; break;
	    case STR_WM_BUTTON_SCALE: str = "Button scale (norm.=256)"; break;
	    case STR_WM_FONT_SCALE: str = "Font scale (norm.=256)"; break;
	    case STR_WM_BUTTON: str = "Button"; break;
	    case STR_WM_RED: str = "Red"; break;
            case STR_WM_GREEN: str = "Green"; break;
            case STR_WM_BLUE: str = "Blue"; break;
            case STR_WM_HIDE_RECENT_FILES: str = "Hide recent files"; break;
            case STR_WM_SHOW_HIDDEN_FILES: str = "Show hidden files"; break;

	    case STR_WM_OFF_ON_MENU: str = "OFF\nON"; break;
	    case STR_WM_AUTO_ON_OFF_MENU: str = "Auto\nON\nOFF"; break;
	    case STR_WM_AUTO_YES_NO_MENU: str = "Auto\nYES\nNO"; break;
	    case STR_WM_CTL_TYPE_MENU: str = "Auto\nFingers\nPen or Mouse"; break;

	    case STR_WM_FILE_NAME: str = "Name:"; break;
	    case STR_WM_FILE_PATH: str = "Path:"; break;
	    case STR_WM_FILE_RECENT: str = "Recent:"; break;
	    case STR_WM_FILE_MSG_NONAME: str = "!Please enter the file name"; break;
	    case STR_WM_FILE_MSG_NOFILE: str = "!The file is empty or does not exist"; break;
	    case STR_WM_FILE_OVERWRITE: str = "Overwrite"; break;

	    default: break;
	}
	break;
    }
    return str;
}

//
// Dialogs & built-in windows
//

// Universal dialog

WINDOWPTR dialog_open( const char* caption, const char* text, const char* buttons, uint32_t flags, window_manager* wm )
{
    dialog_item* ditems = wm->opt_dialog_items;

    uint32_t decor_flags = 0;
    const char* dialog_name = "";
    if( text )
	dialog_name = text; //it is required for the find_win_by_name()
    if( caption ) 
	dialog_name = caption;
    else
    	decor_flags |= DECOR_FLAG_NOHEADER;

    if( flags & DIALOG_FLAG_SINGLE )
    {
	WINDOWPTR w = find_win_by_name( wm->root_win, dialog_name, dialog_handler );
        if( w )
        {
    	    //Already exists!
    	    clear_dialog_constructor_options( wm );
    	    return NULL;
        }
    }

    wm->opt_dialog_buttons_text = buttons;
    wm->opt_dialog_text = text;
    int xsize = wm->normal_window_xsize;
    int ysize = 0;
    WINDOWPTR win = new_window_with_decorator( 
	dialog_name,
	xsize, 64,
	wm->dialog_color, 
	wm->root_win, 
	dialog_handler,
	decor_flags,
	wm );
    if( !win )
    {
	clear_dialog_constructor_options( wm );
	return NULL;
    }
    if( text && text[ 0 ] )
    {
	wbd_lock( win );
	int text_xsize;
	int text_ysize;
	draw_string_wordwrap( text, 0, 0, xsize - wm->interelement_space * 2, &text_xsize, &text_ysize, 1, wm );
	if( text_ysize >= wm->desktop_ysize - wm->scrollbar_size * 2 )
	{
	    draw_string_wordwrap( text, 0, 0, wm->desktop_xsize - wm->interelement_space * 2 - wm->decor_border_size * 2, &text_xsize, &text_ysize, 1, wm );
	}
	if( text_xsize > xsize ) 
	{
	    xsize = text_xsize;
	}
	ysize = text_ysize;
	if( ysize > wm->desktop_ysize - wm->scrollbar_size )
	{
	    ysize = wm->desktop_ysize - wm->scrollbar_size;
	}
	wbd_unlock( wm );
    }
    if( ditems )
    {
	int cols = 1; //current number of columns
        int colcnt = 0; //current column number
	int i = 0;
	while( 1 )
	{
	    dialog_item* item = &ditems[ i ];
	    if( item->type == DIALOG_ITEM_NONE )
	    {
		ysize -= wm->interelement_space;
		break;
	    }
	    if( ( item->flags >> DIALOG_ITEM_FLAG_COLUMNS_OFFSET ) & DIALOG_ITEM_FLAG_COLUMNS_MASK )
		cols = 1 + ( ( item->flags >> DIALOG_ITEM_FLAG_COLUMNS_OFFSET ) & DIALOG_ITEM_FLAG_COLUMNS_MASK );
	    colcnt++;
            if( colcnt >= cols )
            {
                ysize += dialog_get_item_ysize( item->type, win ) + wm->interelement_space;
                colcnt = 0;
                cols = 1;
            }
	    i++;
	}
    }
    resize_window_with_decorator( win->childs[ 0 ], xsize, ysize + wm->button_ysize + wm->interelement_space * 3, wm );
    show_window( win );
    bring_to_front( win, wm );
    recalc_regions( wm );
    draw_window( win, wm );

    return win;
}

int dialog_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    int* result = (int*)user_data;
    if( result )
    {
	*result = win->action_result;
    }
    return 1;
}

int dialog( const char* caption, const char* text, const char* buttons, bool single, window_manager* wm )
{
    int result = -1;

    WINDOWPTR win = dialog_open( caption, text, buttons, DIALOG_FLAG_SINGLE, wm );
    if( win == NULL ) return -1;
    set_handler( win->childs[ 0 ], dialog_action_handler, &result, wm );

    while( 1 )
    {
	sundog_event evt;
	EVENT_LOOP_BEGIN( &evt, wm );
	if( EVENT_LOOP_END( wm ) ) break;
	if( win->visible == 0 ) break;
    }

    return result;
}

int dialog( const char* caption, const char* text, const char* buttons, window_manager* wm )
{
    return dialog( caption, text, buttons, false, wm );
}

int edialog_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    int res = win->action_result;
    remove_window( win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    if( res == 1 )
    {
	//Save log:
	char* fname = fdialog( wm_get_string( STR_WM_SAVE_LOG ), "txt", ".sundog_log_s", "log.txt", 0, wm );
	if( fname )
	{
	    sfs_copy_file( fname, slog_get_file() );
	}
    }
    if( res == 2 )
    {
	//Send log by email:
	char* log = slog_get_latest( wm->sd, 1024 * 1024 );
	if( log )
	{
	    char* ts = (char*)smem_new( 1024 );
	    if( ts )
	    {
    		sprintf( ts, "%s log", g_app_name );
    		send_text_to_email( wm->sd, "nightradio@gmail.com", ts, log );
    		smem_free( ts );
    	    }
    	    smem_free( log );
	}
    }
    return 0;
}

void edialog_open( const char* error_str1, const char* error_str2, window_manager* wm )
{
    size_t len1 = smem_strlen( error_str1 );
    size_t len2 = smem_strlen( error_str2 );
    if( len1 == 0 ) error_str1 = "";
    if( len2 == 0 ) error_str2 = "";
    uint error_log_bytes = 256;
    size_t ts_len = len1 + len2 + 1024 + error_log_bytes;
    char* ts = (char*)smem_new( ts_len );
    char* log = slog_get_latest( wm->sd, error_log_bytes );
    char* buttons = (char*)smem_new( 1024 );
    if( len1 || len2 )
    {
	slog( "Error: %s %s\n", error_str1, error_str2 );
    }
    if( len1 == 0 && len2 == 0 && log )
    {
	//Only log:
	sprintf( ts, "log: ... %s", log );
    }
    else
    {
	if( log )
	    sprintf( ts, "!%s %s\n\nlog: ... %s", error_str1, error_str2, log );
	else
    	    sprintf( ts, "!%s %s", error_str1, error_str2 );
    }
    buttons[ 0 ] = 0;
    smem_strcat_resize( buttons, wm_get_string( STR_WM_CLOSE ) );
    smem_strcat_resize( buttons, ";" );
    smem_strcat_resize( buttons, wm_get_string( STR_WM_SAVE_LOG ) );
#ifdef CAN_SEND_TO_EMAIL
    smem_strcat_resize( buttons, ";" );
    smem_strcat_resize( buttons, wm_get_string( STR_WM_EMAIL_LOG ) );
#endif
    WINDOWPTR win = dialog_open( NULL, ts, buttons, 0, wm );
    if( win )
    {
	set_handler( win->childs[ 0 ], edialog_action_handler, NULL, wm );
    }
    smem_free( log );
    smem_free( ts );
    smem_free( buttons );
}

// File dialog

WINDOWPTR fdialog_open( const char* name, const char* mask, const char* id, const char* def_filename, uint32_t flags, window_manager* wm )
{
#ifdef DEMO_MODE
    if( strstr( name, "Save" ) ||
        strstr( name, "save" ) ||
        strstr( name, "Export" ) ||
        strstr( name, "export" ) ||
        strstr( name, "храни" ) ||
        strstr( name, "хране" ) ||
        strstr( name, "кспорт" ) )
    {
	dialog_open( NULL, wm_get_string( STR_WM_DEMOVERSION_FN ), wm_get_string( STR_WM_OK ), DIALOG_FLAG_SINGLE, wm );
	return NULL;
    }
#endif

#ifdef OS_ANDROID
    android_sundog_check_for_permissions( wm->sd, (1<<0) | (1<<3) | (1<<4) | (1<<5) | (1<<6) );
#endif

    int xsize = wm->large_window_xsize;
    int ysize = wm->button_ysize * 2 + wm->list_item_ysize * 32 + wm->text_ysize * 2;
    uint decor_flags = DECOR_FLAG_CLOSE_ON_ESCAPE;
    if( sconfig_get_int_value( APP_CFG_FDIALOG_NORECENT, -1, 0 ) == -1 )
    {
	int recent_xsize = wm->scrollbar_size * 4;
	xsize += recent_xsize;
    }

    if( wm->control_type == TOUCHCONTROL || ( flags & FDIALOG_FLAG_FULLSCREEN ) )
    {
        decor_flags |= DECOR_FLAG_STATIC | DECOR_FLAG_NOBORDER | DECOR_FLAG_MAXIMIZED;
    }

    wm->opt_fdialog_id = id;
    wm->opt_fdialog_mask = mask;
    wm->opt_fdialog_def_filename = def_filename;
    wm->opt_fdialog_flags = flags;
    WINDOWPTR win = new_window_with_decorator( 
	name, 
	xsize, ysize, 
	wm->dialog_color,
	wm->root_win, 
	fdialog_handler,
	decor_flags,
	wm );
    if( !win )
    {
	clear_fdialog_constructor_options( wm );
	return NULL;
    }
    //win - decorator
    show_window( win );
    bring_to_front( win, wm );
    recalc_regions( wm );
    draw_window( win, wm );

    return win;
}

static int fdialog_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    char* fname = fdialog_get_filename( win );
    smem_free( wm->fdialog_filename );
    wm->fdialog_filename = NULL;
    if( fname )
    {
	if( fname[ 0 ] )
	{
	    wm->fdialog_filename = smem_strdup( fname );
	}
    }
    return 0;
}

char* fdialog( const char* name, const char* mask, const char* id, const char* def_filename, uint32_t flags, window_manager* wm )
{
    if( wm->fdialog_cnt ) return NULL;
    wm->fdialog_cnt++;

    smem_free( wm->fdialog_filename );
    wm->fdialog_filename = NULL;

    WINDOWPTR win = fdialog_open( name, mask, id, def_filename, flags, wm );
    //win - decorator
    if( !win ) return NULL;
    set_handler( win->childs[ 0 ], fdialog_action_handler, NULL, wm );

    while( 1 )
    {
	sundog_event evt;
	EVENT_LOOP_BEGIN( &evt, wm );
	if( EVENT_LOOP_END( wm ) ) break;
	if( win->visible == 0 ) break;
    }

    wm->fdialog_cnt--;

    return wm->fdialog_filename;
}

WINDOWPTR fdialog_overwrite_open( char* filename, window_manager* wm )
{
    char* ts = (char*)smem_new( smem_strlen( filename ) + 1024 );
    sprintf( ts, "!%s?\n%s", wm_get_string( STR_WM_FILE_OVERWRITE ), sfs_get_filename_without_dir( filename ) );
    WINDOWPTR d = dialog_open( NULL, ts, wm_get_string( STR_WM_YESNO ), 0, wm );
    smem_free( ts );
    return d;
}

int dialog_overwrite( char* filename, window_manager* wm ) //0 - YES; 1 - NO;
{
    char* ts = (char*)smem_new( smem_strlen( filename ) + 1024 );
    sprintf( ts, "!%s?\n%s", wm_get_string( STR_WM_FILE_OVERWRITE ), sfs_get_filename_without_dir( filename ) );
    int rv = dialog( 0, ts, wm_get_string( STR_WM_YESNO ), wm );
    smem_free( ts );
    return rv;
}

// Pupup menu

WINDOWPTR popup_menu_open( const char* name, const char* items, int x, int y, COLOR color, int prev, window_manager* wm )
{
    wm->opt_popup_text = items;
    if( x == POPUP_MENU_POS_AUTO )
	x = wm->pen_x;
    if( x == POPUP_MENU_POS_AUTO2 )
	x = wm->pen_x - wm->scrollbar_size;
    if( y == POPUP_MENU_POS_AUTO )
	y = wm->pen_y;
    if( y == POPUP_MENU_POS_AUTO2 )
	y = wm->pen_y - wm->scrollbar_size;
    WINDOWPTR win = new_window( name, x, y, 1, 1, color, wm->root_win, popup_handler, wm );
    if( !win )
    {
	clear_popup_dialog_constructor_options( wm );
	return NULL;
    }
    popup_set_prev( win, prev );
    show_window( win );
    recalc_regions( wm );
    draw_window( win, wm );
    return win;
}

static int popup_menu_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    int* result = (int*)user_data;
    if( result )
	*result = win->action_result;
    return 1;
}

int popup_menu( const char* name, const char* items, int x, int y, COLOR color, int prev, window_manager* wm )
{
    int result = -1;
    WINDOWPTR win = popup_menu_open( name, items, x, y, color, prev, wm );
    if( !win ) return -1;
    set_handler( win, popup_menu_action_handler, &result, wm );
    while( 1 )
    {
	sundog_event evt;
	EVENT_LOOP_BEGIN( &evt, wm );
	if( EVENT_LOOP_END( wm ) ) break;
	if( win->visible == 0 ) break;
    }
    return result;
}

int popup_menu( const char* name, const char* items, int x, int y, COLOR color, window_manager* wm )
{
    return popup_menu( name, items, x, y, color, -1, wm );
}

// Preferences

void prefs_clear( window_manager* wm )
{
    wm->prefs_section_names[ 0 ] = 0;
    wm->prefs_sections = 0;
    wm->prefs_flags = 0;
}

void prefs_add_sections( const char** names, void** handlers, int num, window_manager* wm )
{
    for( int s = 0; s < num; s++ )
    {
	wm->prefs_section_names[ wm->prefs_sections ] = names[ s ];
	wm->prefs_section_handlers[ wm->prefs_sections ] = handlers[ s ];
	wm->prefs_sections++;
    }
    wm->prefs_section_names[ wm->prefs_sections ] = 0;
}

void prefs_add_default_sections( window_manager* wm )
{
    const char* def_names[ 3 ];
    void* def_handlers[ 3 ];
    int s = 0;
    def_handlers[ s ] = (void*)prefs_ui_handler;
    def_names[ s ] = wm_get_string( STR_WM_INTERFACE ); s++;
#ifndef NOVIDEO
    def_handlers[ s ] = (void*)prefs_svideo_handler;
    def_names[ s ] = wm_get_string( STR_WM_VIDEO ); s++;
#endif
    def_handlers[ s ] = (void*)prefs_audio_handler;
    def_names[ s ] = wm_get_string( STR_WM_AUDIO ); s++;
    prefs_add_sections( def_names, def_handlers, s, wm );
}

void prefs_open( void* host, window_manager* wm )
{
    if( !wm->prefs_win )
    {
	wm->prefs_restart_request = false;

        uint flags = DECOR_FLAG_CLOSE_ON_ESCAPE;
        wm->prefs_win = new_window_with_decorator(
            wm_get_string( STR_WM_PREFERENCES ),
            wm->large_window_xsize, wm->large_window_ysize,
            wm->dialog_color,
            wm->root_win,
            host,
            prefs_handler,
            flags,
            wm );
        set_focus_win( wm->prefs_win, wm );
	show_window( wm->prefs_win );
        bring_to_front( wm->prefs_win, wm );
	recalc_regions( wm );
        draw_window( wm->root_win, wm );
    }
}

// Color theme

void colortheme_open( window_manager* wm )
{
    if( !wm->colortheme_win )
    {
        uint flags = DECOR_FLAG_CLOSE_ON_ESCAPE;
        wm->colortheme_win = new_window_with_decorator(
            wm_get_string( STR_WM_SET_COLOR_THEME ),
            wm->large_window_xsize, wm->large_window_ysize,
            wm->dialog_color,
            wm->root_win,
            colortheme_handler,
            flags,
            wm );
        set_focus_win( wm->colortheme_win, wm );
        show_window( wm->colortheme_win );
        bring_to_front( wm->colortheme_win, wm );
        recalc_regions( wm );
        draw_window( wm->root_win, wm );
    }
}

// UI Scale

void ui_scale_open( window_manager* wm )
{
    if( !wm->ui_scale_win )
    {
        uint flags = DECOR_FLAG_CLOSE_ON_ESCAPE;
        wm->ui_scale_win = new_window_with_decorator(
            wm_get_string( STR_WM_SET_UI_SCALE ),
            wm->normal_window_xsize, wm->normal_window_ysize,
            wm->dialog_color,
            wm->root_win,
            ui_scale_handler,
            flags,
            wm );
        set_focus_win( wm->ui_scale_win, wm );
        show_window( wm->ui_scale_win );
        bring_to_front( wm->ui_scale_win, wm );
        recalc_regions( wm );
        draw_window( wm->root_win, wm );
    }
}

// Keymap

void keymap_open( window_manager* wm )
{
    if( !wm->keymap_win )
    {
        uint flags = DECOR_FLAG_CLOSE_ON_ESCAPE;
        wm->keymap_win = new_window_with_decorator(
            wm_get_string( STR_WM_SHORTCUTS ),
            wm->large_window_xsize, wm->large_window_ysize,
            wm->dialog_color,
            wm->root_win,
            keymap_handler,
            flags,
            wm );
        set_focus_win( wm->keymap_win, wm );
        show_window( wm->keymap_win );
        bring_to_front( wm->keymap_win, wm );
        recalc_regions( wm );
        draw_window( wm->root_win, wm );
    }
}
