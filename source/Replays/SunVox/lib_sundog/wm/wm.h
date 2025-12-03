#pragma once

typedef uint8_t FONT_LINE_TYPE;
typedef uint8_t FONT_SPX_TYPE;
extern FONT_LINE_TYPE* g_fonts[]; //font: 257 bytes of header (ysize, xsize0, xsize1, xsize2, ...); ysize*256 (1bit pixel data);
extern FONT_SPX_TYPE* g_fonts_spx[]; //special pixels: char,x|y,char,x|y,...
extern const char* g_font_names[];
extern int g_fonts_count;
#define FONT_LEFT_PIXEL ( 1 << ( sizeof( FONT_LINE_TYPE ) * 8 - 1 ) )
#define FONT_MEDIUM_MONO 0
#define FONT_BIG 1
#define FONT_SMALL 2
#define DEFAULT_FONT_MEDIUM_MONO 	FONT_MEDIUM_MONO
#define DEFAULT_FONT_BIG 		FONT_BIG
#define DEFAULT_FONT_SMALL 		FONT_MEDIUM_MONO

#include "regions/devrgn.h"
#include "wm_struct.h"

#define STR_SHORT_SPACE "\x01"
#define STR_HATCHING "\x10"
#define STR_LARGEST_CHAR STR_HATCHING
#define STR_UP "\x11"
#define STR_DOWN "\x12"
#define STR_LEFT "\x13"
#define STR_RIGHT "\x14"
#define STR_UNDO "\x15"
#define STR_REDO "\x16"
#define STR_HOME "\x17\x18"
#define STR_DASH_BEGIN "\x19"
#define STR_DASH_END "\x1A"
#define STR_SMALL_DOT "\x1B"
#define STR_BIG_DOT "\x1C"
#define STR_CORNER_SMALL_LB "\x0E"
#define STR_CORNER_SMALL_LT "\x0F"
#define STR_CORNER_LB "\x1E"
#define STR_CORNER_LT "\x1F"
#define STR_MEMORY_CARD "\x0D"
#define STR_HEART "\x0C"
extern const char* g_str_short_space;
extern const char* g_str_hatching;
extern const char* g_str_largest_char;
extern const char* g_str_up;
extern const char* g_str_down;
extern const char* g_str_left;
extern const char* g_str_right;
extern const char* g_str_undo;
extern const char* g_str_redo;
extern const char* g_str_home;
extern const char* g_str_dash_begin;
extern const char* g_str_dash_end;
extern const char* g_str_small_dot;
extern const char* g_str_big_dot;
extern const char* g_str_corner_small_lb;
extern const char* g_str_corner_small_lt;
extern const char* g_str_corner_lb;
extern const char* g_str_corner_lt;

#ifdef X11
extern const char* g_msg_id_x11_paste;
#endif

//default = option is missing (or commented out) in the config file;
#define APP_CFG_WIN_XSIZE		"width" //window width
#define APP_CFG_WIN_YSIZE             	"height" //window height
#define APP_CFG_DONT_SAVE_WINSIZE   	"nosws" //don't save the window size: default = save; any value = don't save;
#define APP_CFG_ROTATE              	"rotate" //rotate the window contents at a specified angle (degrees) counterclockwise
#define APP_CFG_FULLSCREEN          	"fullscreen" //fullscreen mode: default = disabled; any value = enabled;
#define APP_CFG_MAXIMIZED           	"maximized" //maximized window: default = disabled; any value = enabled;
#define APP_CFG_ZOOM                	"zoom" //pixel size: default = 1;
#define APP_CFG_PPI                 	"ppi" //Pixels Per Inch (pixel density): default = auto;
#define APP_CFG_SCALE               	"scale" //UI scale factor (normal=256): default = 256;
#define APP_CFG_FONT_SCALE          	"fscale" //font scale factor (normal=256): default = 256;
#define APP_CFG_FONT_MEDIUM_MONO    	"font_mm" //medium mono font index: default = DEFAULT_FONT_MEDIUM_MONO;
#define APP_CFG_FONT_BIG            	"font_b" //big font index: default = DEFAULT_FONT_BIG;
#define APP_CFG_FONT_SMALL          	"font_s" //small font index: default = DEFAULT_FONT_SMALL;
#define APP_CFG_NO_FONT_UPSCALE     	"no_font_upscale" //no font upscaling algorithm (OpenGL only): default = auto; any value = no font upscaling;
#define APP_CFG_NOCURSOR            	"nocursor" //hide mouse cursor: default = auto; any value = hide;
#define APP_CFG_NOBORDER            	"noborder" //hide the window border and decorations: default = auto; any value = hide;
#define APP_CFG_NOSYSBARS           	"nosysbars" //hide system bars: default = auto; any value = hide;
#define APP_CFG_WINDOWNAME          	"windowname" //window name
#define APP_CFG_VIDEODRIVER         	"videodriver" //video driver name (only for Windows CE): default = gapi (requires gx.dll); raw (hires framebuffer); gdi (compatibility mode);
#define APP_CFG_TOUCHCONTROL        	"touchcontrol" //optimize for touchscreen; default = auto; any value = optimize;
#define APP_CFG_PENCONTROL          	"pencontrol" //optimize for mouse/pen; default = auto; any value = optimize;
#define APP_CFG_SHOW_ZOOM_BUTTONS   	"zoombtns" //show zoom buttons: default = auto; 0 = hide; 1 = show;
#define APP_CFG_DOUBLECLICK         	"doubleclick" //max double click length in ms: default = DEFAULT_DOUBLE_CLICK_TIME;
#define APP_CFG_KBD_AUTOREPEAT_DELAY	"ar_d1" //delay (ms) between the key press and the first repeat: default = DEFAULT_KBD_AUTOREPEAT_DELAY;
#define APP_CFG_KBD_AUTOREPEAT_FREQ     "ar_f1" //key autorepeat freq (Hz): default = DEFAULT_KBD_AUTOREPEAT_FREQ;
#define APP_CFG_MOUSE_AUTOREPEAT_DELAY  "ar_d2" //delay (ms) between the click and the first repeat: default = DEFAULT_MOUSE_AUTOREPEAT_DELAY;
#define APP_CFG_MOUSE_AUTOREPEAT_FREQ   "ar_f2" //mouse click autorepeat freq (Hz): default = DEFAULT_MOUSE_AUTOREPEAT_FREQ;
#define APP_CFG_MAXFPS              	"maxfps" //max FPS: default = auto;
#define APP_CFG_VSYNC               	"vsync" //enable VSync if possible: default = auto; any value = enable;
#define APP_CFG_FRAMEBUFFER         	"framebuffer" //use framebuffer: default = auto; 0 = disable; 1 = enable;
#define APP_CFG_NOWINDOW            	"nowin" //hide the app window (pure console mode without UI): default = show; any value = hide;
#define APP_CFG_SCREENBUF_SWAP_BEHAVIOR	"swap_behavior"
					//default = auto;
					//1 = OpenGL screen buffer will be preserved when switching to the next frame - this may increase FPS, but this feature is not supported by all graphics drivers;
					//0 = OpenGL screen buffer will be destroyed when switching to the next frame;
#define APP_CFG_SHOW_VIRT_KBD       	"show_virt_kbd" //show on-screen keyboard: default = auto; any value = show;
#define APP_CFG_HIDE_VIRT_KBD       	"hide_virt_kbd" //hide on-screen keyboard: default = auto; any value = hide;
#define APP_CFG_COLOR_THEME         	"builtin_theme" //built-in color theme index: default = 3;
#define APP_CFG_CUSTOM_COLORS		"theme" //use custom colors c_*; default = off; any value = on;
#define APP_CFG_FDIALOG_PREVIEW     	"fpreview" //f.dialog preview on/off: default = off; 0 = off; 1 - on;
#define APP_CFG_FDIALOG_PREVIEW_YSIZE   "fpreview_ys"
#define APP_CFG_FDIALOG_NORECENT    	"nofrecent" //f.dialog hide recent files: default = show; any value = hide;
#define APP_CFG_FDIALOG_RECENT_Y    	"frecent_y"
#define APP_CFG_FDIALOG_RECENT_XSIZE    "frecent_xs"
#define APP_CFG_FDIALOG_RECENT_MAXFILES	"frecent_fmax"
#define APP_CFG_FDIALOG_RECENT_MAXDIRS 	"frecent_dmax"
#define APP_CFG_FDIALOG_SHOWHIDDEN	"showhf" //f.dialog show hidden files: default = hide; any value = show;
#define APP_CFG_FDIALOG_DIR_POS_FILESIZE_LIMIT	"fdirpos_lim" //size limit for the "directory positions" file; 

//###################################
//## MAIN FUNCTIONS:               ##
//###################################

int win_init( const char* name, int xsize, int ysize, uint flags, sundog_engine* sd );
void win_change_name( const char* name, window_manager* wm ); //NULL - default name (from win_init());
void win_colors_init( window_manager* wm );
void win_reinit( window_manager* wm );
int win_calc_font_zoom( int screen_ppi, int screen_zoom, float screen_scale, float screen_font_scale );
void win_zoom_init( window_manager* wm );
void win_zoom_apply( window_manager* wm );
void win_angle_apply( window_manager* wm );
void win_vsync( bool vsync, window_manager* wm );
void win_exit_request( window_manager* wm );
void win_destroy_request( window_manager* wm ); //fast exit without saving state
void win_restart_request( window_manager* wm );
void win_suspend( bool suspend, window_manager* wm );
void win_suspend_devices( bool suspend, window_manager* wm ); //NOT FOR ALL SYSTEMS. Tested in iOS only!
void win_deinit( window_manager* wm ); //close window manager

//###################################
//## WINDOWS:                      ##
//###################################

WINDOWPTR new_window( 
    const char* name, 
    int x, 
    int y, 
    int xsize, 
    int ysize, 
    COLOR color,
    WINDOWPTR parent, 
    win_handler_t win_handler,
    window_manager* wm );
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
    window_manager* wm );
void rename_window( WINDOWPTR win, const char* name );
void set_window_controller( WINDOWPTR win, int ctrl_num, window_manager* wm, ... );
void remove_window_controllers( WINDOWPTR win );
void remove_window( WINDOWPTR win, window_manager* wm );
void add_child( WINDOWPTR win, WINDOWPTR child, window_manager* wm );
void remove_child( WINDOWPTR win, WINDOWPTR child, window_manager* wm );
WINDOWPTR get_parent_by_win_handler( WINDOWPTR win, win_handler_t win_handler );
WINDOWPTR find_win_by_handler( WINDOWPTR win, win_handler_t win_handler );
WINDOWPTR find_win_by_name( WINDOWPTR root, const char* name, win_handler_t win_handler );
void set_handler( WINDOWPTR win, win_action_handler_t handler, void* handler_data, window_manager* wm );
bool is_window_visible( WINDOWPTR win, window_manager* wm );
void draw_window( WINDOWPTR win, window_manager* wm );
void force_redraw_all( window_manager* wm ); //even if screen_buffer_preserved == 0
void show_window( WINDOWPTR win );
void hide_window( WINDOWPTR win );
bool show_window2( WINDOWPTR win );
bool hide_window2( WINDOWPTR win );
void bring_to_front( WINDOWPTR win, window_manager* wm );
void recalc_regions( window_manager* wm );

void set_focus_win( WINDOWPTR win, window_manager* wm );

//collect touch events;
//(first touch will be converted to the EVT_MOUSE* event)
//mode:
//  0 - touch down;
//  1 - touch move;
//  2 - touch up;
//  3 - vscroll; pressure: -1024 - down; 1024 - up;
//  4 - hscroll; pressure: -1024 - left; 1024 - right;
//flags:
#define TOUCH_FLAG_LIMIT		( 1 << 0 ) /*Don't allow touches outside of the screen*/
#define TOUCH_FLAG_REALWINDOW_XY	( 1 << 1 )
int collect_touch_events( int mode, uint touch_flags, uint evt_flags, int x, int y, int pressure, WM_TOUCH_ID touch_id, window_manager* wm );
void convert_real_window_xy( int& x, int& y, window_manager* wm ); //Real window XY -> SunDog screen XY
int scale_coord( int v, bool user_to_real, window_manager* wm );
int send_touch_events( window_manager* wm );
int send_events(
    sundog_event* events,
    int events_num,
    window_manager* wm );
int send_event( 
    WINDOWPTR win, 
    int type, 
    uint flags, 
    int x, 
    int y, 
    int key, 
    int scancode, 
    int pressure, 
    window_manager* wm );
int send_event( WINDOWPTR win, int type, window_manager* wm );
int handle_event( WINDOWPTR win, int type, window_manager* wm );
int handle_event( sundog_event* evt, window_manager* wm );
int handle_event_direct( WINDOWPTR win, int type );
int handle_event_by_window( sundog_event* evt, window_manager* wm );

int EVENT_LOOP_BEGIN( sundog_event* evt, window_manager* wm );
int EVENT_LOOP_END( window_manager* wm );

inline int window_handler_check_data( sundog_event* evt, window_manager* wm )
{
    if( evt->type != EVT_GETDATASIZE )
    {
        if( !evt->win->data )
        {
            slog( "ERROR: Event to window (%s) without data\n", evt->win->name );
            return 1;
        }
    }
    return 0;
}

//###################################
//## DATA CONTAINERS:              ##
//## (empty windows with some data)##
//###################################

WINDOWPTR add_data_container( WINDOWPTR win, const char* name, void* new_data_block );
//new_data_block - just created with smem_alloc(); pointer copy, NOT DATA!
//the data will be deleted automatically when the container is deleted;
void remove_data_container( WINDOWPTR win, const char* name );
WINDOWPTR get_data_container( WINDOWPTR win, const char* name );
void* get_data_container2( WINDOWPTR win, const char* name );

//###################################
//## STATUS MESSAGES:              ##
//###################################

void show_status_message( const char* msg, int t, window_manager* wm );
void hide_status_message( window_manager* wm );

//###################################
//## TIMERS:                       ##
//###################################

// -1 - empty timer;
// Timer is endlessly looped (with auto-repeat) by default; so if you want - just remove it in the callback
int add_timer( sundog_timer_handler hnd, void* data, stime_ms_t delay, window_manager* wm );
void reset_timer( int timer, window_manager* wm );
void reset_timer( int timer, stime_ms_t new_delay, window_manager* wm );
void remove_timer( int timer, window_manager* wm );

//###################################
//## WINDOWS DECORATIONS:          ##
//###################################

int decorator_handler( sundog_event* evt, window_manager* wm );
void resize_window_with_decorator( WINDOWPTR win, int xsize, int ysize, window_manager* wm );
WINDOWPTR new_window_with_decorator( 
    const char* name, 
    int xsize, 
    int ysize, 
    COLOR color,
    WINDOWPTR parent, 
    win_handler_t win_handler,
    uint flags, //DECOR_FLAG_*
    window_manager* wm ); //retval = decorator
WINDOWPTR new_window_with_decorator( 
    const char* name, 
    int xsize, 
    int ysize, 
    COLOR color,
    WINDOWPTR parent, 
    void* host,
    win_handler_t win_handler,
    uint flags, //DECOR_FLAG_*
    window_manager* wm ); //retval = decorator

//###################################
//## Auto alignment                ##
//###################################

struct btn_autoalign_data
{
    int y;
    int i; //current item (column);
    WINDOWPTR* ww;
    uint32_t flags;
    window_manager* wm;
};

void btn_autoalign_init( btn_autoalign_data* aa, window_manager* wm, uint32_t flags );
void btn_autoalign_deinit( btn_autoalign_data* aa );
void btn_autoalign_add( btn_autoalign_data* aa, WINDOWPTR w, uint32_t flags );
//#define BTN_AUTOALIGN_LINE_LEFT               0
//#define BTN_AUTOALIGN_LINE_CENTER     1
//#define BTN_AUTOALIGN_LINE_RIGHT      2
#define BTN_AUTOALIGN_LINE_EVENLY       3
void btn_autoalign_next_line( btn_autoalign_data* aa, uint32_t flags );

//###################################
//## DRAWING FUNCTIONS:            ##
//###################################

inline COLOR get_color( uint8_t r, uint8_t g, uint8_t b )
{
    COLOR res = 0; //resulted color

#ifdef COLOR8BITS
    #ifdef GRAYSCALE
	res = (COLOR)( (int)( ( r + g + b + b ) >> 2 ) );
    #else
	res += ( b & 192 );
	res += ( g & 224 ) >> 2;
	res += ( r >> 5 );
    #endif
#endif
    
#ifdef COLOR16BITS

#ifdef RGB556
    res += ( r >> 3 ) << 11;
    res += ( g >> 3 ) << 6;
    res += ( b >> 2 );
#endif
#ifdef RGB555
    res += ( r >> 3 ) << 10;
    res += ( g >> 3 ) << 5;
    res += ( b >> 3 );
#endif
#ifdef RGB565
    res += ( r >> 3 ) << 11;
    res += ( g >> 2 ) << 5;
    res += ( b >> 3 );
#endif

#endif
    
#ifdef COLOR32BITS
    #ifdef OPENGL
	res += r;
	res += g << 8;
	res += b << 16;
    #else
	res += r << 16;
	res += g << 8;
	res += b;
    #endif
#endif
    
    return res;
}

#ifdef RGB556
#define GET_RED16 \
    res = ( ( c >> 11 ) << 3 ) & 0xF8; \
    if( res ) res |= 7;
#endif
#ifdef RGB555
#define GET_RED16 \
    res = ( ( c >> 10 ) << 3 ) & 0xF8; \
    if( res ) res |= 7;
#endif
#ifdef RGB565
#define GET_RED16 \
    res = ( ( c >> 11 ) << 3 ) & 0xF8; \
    if( res ) res |= 7;
#endif

inline int red( COLOR c )
{
    int res = 0;
    
#ifdef COLOR8BITS
    #ifdef GRAYSCALE
	res = c;
    #else
	res = ( c << 5 ) & 255;
	if( res ) res |= 0x1F;
    #endif
#endif
    
#ifdef COLOR16BITS
    GET_RED16;
#endif

#ifdef COLOR32BITS
    #ifdef OPENGL
	res = c & 255;
    #else
	res = ( c >> 16 ) & 255;
    #endif
#endif

    return res;
}

#ifdef RGB556
#define GET_GREEN16 \
    res = ( ( c >> 6 ) << 3 ) & 0xF8; \
    if( res ) res |= 7;
#endif
#ifdef RGB555
#define GET_GREEN16 \
    res = ( ( c >> 5 ) << 3 ) & 0xF8; \
    if( res ) res |= 7;
#endif
#ifdef RGB565
#define GET_GREEN16 \
    res = ( ( c >> 5 ) << 2 ) & 0xFC; \
    if( res ) res |= 3;
#endif

inline int green( COLOR c )
{
    int res = 0;
    
#ifdef COLOR8BITS
    #ifdef GRAYSCALE
	res = c;
    #else
	res = ( c << 2 ) & 0xE0;
	if( res ) res |= 0x1F;
    #endif
#endif
    
#ifdef COLOR16BITS
    GET_GREEN16;
#endif

#ifdef COLOR32BITS
    res = ( c >> 8 ) & 255;
#endif
    
    return res;
}

#ifdef RGB556
#define GET_BLUE16 \
    res = ( c << 2 ) & 0xFC; \
    if( res ) res |= 3;
#endif
#ifdef RGB555
#define GET_BLUE16 \
    res = ( c << 3 ) & 0xF8; \
    if( res ) res |= 7;
#endif
#ifdef RGB565
#define GET_BLUE16 \
    res = ( c << 3 ) & 0xF8; \
    if( res ) res |= 7;
#endif

inline int blue( COLOR c )
{
    int res = 0;
    
#ifdef COLOR8BITS
    #ifdef GRAYSCALE
	res = c;
    #else
	res = ( c & 192 );
	if( res ) res |= 0x3F;
    #endif
#endif
    
#ifdef COLOR16BITS
    GET_BLUE16;
#endif
    
#ifdef COLOR32BITS
    #ifdef OPENGL
	res = ( c >> 16 ) & 255;
    #else
	res = c & 255;
    #endif
#endif
    
    return res;
}

inline COLOR correct_blend( COLOR c1, COLOR c2, uint8_t val )
{
    int r1 = red( c1 );
    int g1 = green( c1 );
    int b1 = blue( c1 );
    int r2 = red( c2 );
    int g2 = green( c2 );
    int b2 = blue( c2 );
    uint8_t ival = 255 - val;
    return get_color(
	( r1 * ival + r2 * val ) / 255,
	( g1 * ival + g2 * val ) / 255,
	( b1 * ival + b2 * val ) / 255
    );
}

inline COLOR fast_blend( COLOR c1, COLOR c2, uint8_t val )
{
    uint ival = 256 - val;
#ifdef COLOR32BITS
    uint v1_1 = c1 & 0xFF00FF;
    uint v1_2 = c1 & 0x00FF00;
    uint v2_1 = c2 & 0xFF00FF;
    uint v2_2 = c2 & 0x00FF00;
    uint res = 
	( ( ( ( v1_1 * ival ) + ( v2_1 * val ) ) >> 8 ) & 0xFF00FF ) |
	( ( ( ( v1_2 * ival ) + ( v2_2 * val ) ) >> 8 ) & 0x00FF00 );
    return res;
#endif
#ifdef COLOR16BITS
    #ifdef FAST_BLEND
        ival >>= 3;
        val >>= 3;
        #ifdef RGB556
            uint res = 
                ( ( ( ( ( c1 & 0xF83F ) * ival ) + ( ( c2 & 0xF83F ) * val ) ) >> 5 ) & 0xF83F ) |
                ( ( ( ( ( c1 & 0x7C0 ) * ival ) + ( ( c2 & 0x7C0 ) * val ) ) >> 5 ) & 0x7C0 );
        #endif
        #ifdef RGB555
            uint res = 
                ( ( ( ( ( c1 & 0x7C1F ) * ival ) + ( ( c2 & 0x7C1F ) * val ) ) >> 5 ) & 0x7C1F ) |
                ( ( ( ( ( c1 & 0x1E0 ) * ival ) + ( ( c2 & 0x1E0 ) * val ) ) >> 5 ) & 0x1E0 );
        #endif
        #ifdef RGB565
            uint res = 
                ( ( ( ( ( c1 & 0xF81F ) * ival ) + ( ( c2 & 0xF81F ) * val ) ) >> 5 ) & 0xF81F ) |
                ( ( ( ( ( c1 & 0x7E0 ) * ival ) + ( ( c2 & 0x7E0 ) * val ) ) >> 5 ) & 0x7E0 );
        #endif
    #else
        #ifdef RGB556
            int r1 = ( c1 >> 8 ) | 7;
            int g1 = ( ( c1 >> 3 ) & 255 ) | 7;
            int b1 = ( ( c1 << 2 ) & 255 ) | 3;
            int r2 = ( c2 >> 8 ) | 7;
            int g2 = ( ( c2 >> 3 ) & 255 ) | 7;
            int b2 = ( ( c2 << 2 ) & 255 ) | 3;
        #endif
        #ifdef RGB555
            int r1 = ( c1 >> 7 ) | 7;
            int g1 = ( ( c1 >> 2 ) & 255 ) | 7;
            int b1 = ( ( c1 << 3 ) & 255 ) | 7;
            int r2 = ( c2 >> 7 ) | 7;
            int g2 = ( ( c2 >> 2 ) & 255 ) | 7;
            int b2 = ( ( c2 << 3 ) & 255 ) | 7;
        #endif
        #ifdef RGB565
            int r1 = ( c1 >> 8 ) | 7;
            int g1 = ( ( c1 >> 3 ) & 255 ) | 3;
            int b1 = ( ( c1 << 3 ) & 255 ) | 7;
            int r2 = ( c2 >> 8 ) | 7;
            int g2 = ( ( c2 >> 3 ) & 255 ) | 3;
            int b2 = ( ( c2 << 3 ) & 255 ) | 7;
        #endif
            int r3 = r1 * ival + r2 * val;
            int g3 = g1 * ival + g2 * val;
            int b3 = b1 * ival + b2 * val;
        #ifdef RGB556
            r3 >>= 8 + 3;
            g3 >>= 8 + 3;
            b3 >>= 8 + 2;
            uint res = ( r3 << 11 ) | ( g3 << 6 ) | b3;
        #endif
        #ifdef RGB555
            r3 >>= 8 + 3;
            g3 >>= 8 + 3;
            b3 >>= 8 + 3;
            uint res = ( r3 << 10 ) | ( g3 << 5 ) | b3;
        #endif
        #ifdef RGB565
            r3 >>= 8 + 3;
            g3 >>= 8 + 2;
            b3 >>= 8 + 3;
            uint res = ( r3 << 11 ) | ( g3 << 5 ) | b3;
        #endif
    #endif
    return res;
#endif
#ifdef COLOR8BITS
    #ifdef GRAYSCALE
	return (COLOR)( ( c1 * ival + c2 * val ) >> 8 );
    #else
	int b1 = c1 >> 5; b1 &= ~1; if( b1 ) b1 |= 1; //because B is 2 bits only
	int g1 = ( c1 >> 3 ) & 7; 
	int r1 = c1 & 7; 
	int b2 = c2 >> 5; b2 &= ~1; if( b2 ) b2 |= 1;
	int g2 = ( c2 >> 3 ) & 7; 
	int r2 = c2 & 7; 
	int r3 = r1 * ival + r2 * val;
	int g3 = g1 * ival + g2 * val;
	int b3 = b1 * ival + b2 * val;
	b3 >>= 9;
	g3 >>= 8;
	r3 >>= 8;
	return ( b3 << 6 ) | ( g3 << 3 ) | r3;
    #endif
#endif
    return 0;
}

inline COLOR blend( COLOR c1, COLOR c2, int val )
{
    if( val <= 0 ) return c1;
    if( val >= 255 ) return c2;
    return fast_blend( c1, c2, (uint8_t)val );
}

COLOR get_color_from_string( char* str );
void get_string_from_color( char* dest, uint dest_size, COLOR c );

void win_draw_lock( WINDOWPTR win, window_manager* wm );    //Only draw_window() and event_handler (during the EVT_DRAW handling) do this automaticaly!
void win_draw_unlock( WINDOWPTR win, window_manager* wm );  //Only draw_window() and event_handler (during the EVT_DRAW handling) do this automaticaly!

void win_draw_rect( WINDOWPTR win, int x, int y, int xsize, int ysize, COLOR color, window_manager* wm );
void win_draw_frect( WINDOWPTR win, int x, int y, int xsize, int ysize, COLOR color, window_manager* wm );
void win_draw_image_ext( 
    WINDOWPTR win, 
    int x, 
    int y, 
    int dest_xsize, 
    int dest_ysize,
    int source_x,
    int source_y,
    sdwm_image* img, 
    window_manager* wm );
void win_draw_image( 
    WINDOWPTR win, 
    int x, 
    int y, 
    sdwm_image* img, 
    window_manager* wm );
bool line_clip( int* x1, int* y1, int* x2, int* y2, int clip_x1, int clip_y1, int clip_x2, int clip_y2 );
void win_draw_line( WINDOWPTR win, int x1, int y1, int x2, int y2, COLOR color, window_manager* wm );

void screen_changed( window_manager* wm );

//###################################
//## VIDEO CAPTURING:              ##
//###################################

bool video_capture_supported( window_manager* wm );
void video_capture_set_in_name( const char* name, window_manager* wm );
const char* video_capture_get_file_ext( window_manager* wm ); //example: "avi"
int video_capture_start( window_manager* wm );
int video_capture_frame_begin( window_manager* wm );
int video_capture_frame_end( window_manager* wm );
int video_capture_stop( window_manager* wm );
int video_capture_encode( window_manager* wm );

//###################################
//## FRAMEBUFFER:                  ##
//###################################

void fb_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm );
void fb_draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm );
void fb_draw_image(
    int dest_x, int dest_y,
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm );

//###################################
//## IMAGE FUNCTIONS:              ##
//###################################

sdwm_image* new_image( 
    int xsize, 
    int ysize, 
    void* src,
    int src_xsize,
    int src_ysize,
    uint flags,
    window_manager* wm );
void update_image( sdwm_image* img, int x, int y, int xsize, int ysize );
void update_image( sdwm_image* img );
sdwm_image* resize_image( sdwm_image* img, int resize_flags, int new_xsize, int new_ysize );
void remove_image( sdwm_image* img );

//###################################
//## KEYMAP FUNCTIONS:             ##
//###################################

int get_key_id( const char* name );
const char* get_key_name( int key );
sundog_keymap* keymap_new( window_manager* wm );
void keymap_remove( sundog_keymap* km, window_manager* wm );
void keymap_silent( sundog_keymap* km, bool silent, window_manager* wm );
int keymap_add_section( sundog_keymap* km, const char* section_name, const char* section_id, int section_num, int num_actions, window_manager* wm );
int keymap_add_action( sundog_keymap* km, int section_num, const char* action_name, const char* action_id, int action_num, window_manager* wm );
int keymap_set_group( sundog_keymap* km, int section_num, const char* group, window_manager* wm );
const char* keymap_get_action_name( sundog_keymap* km, int section_num, int action_num, window_manager* wm );
const char* keymap_get_action_group_name( sundog_keymap* km, int section_num, int action_num, window_manager* wm );
int keymap_bind2( sundog_keymap* km, int section_num, int key, uint flags, uint pars1, uint pars2, int action_num, int bind_num, int bind_flags, window_manager* wm );
int keymap_bind( sundog_keymap* km, int section_num, int key, uint flags, int action_num, int bind_num, int bind_flags, window_manager* wm );
int keymap_get_section( sundog_keymap* km, const char* section_id, window_manager* wm );
int keymap_get_action( sundog_keymap* km, int section_num, const char* action_id, window_manager* wm );
int keymap_get_action( sundog_keymap* km, int section_num, int key, uint flags, uint pars1, uint pars2, window_manager* wm );
sundog_keymap_key* keymap_get_key( sundog_keymap* km, int section_num, int action_num, int key_num, window_manager* wm );
bool keymap_midi_note_assigned( sundog_keymap* km, int note, int channel, window_manager* wm );
int keymap_save( sundog_keymap* km, sconfig_data* config, window_manager* wm );
int keymap_load( sundog_keymap* km, sconfig_data* config, window_manager* wm );
int keymap_print_available_keys( sundog_keymap* km, int section_num, int key_flags, window_manager* wm );

//###################################
//## WBD FUNCTIONS:                ##
//###################################

#define IMG_PREC 11 /* fixed point precision */

//See code in wbd.cpp
void wbd_lock( WINDOWPTR win );
void wbd_unlock( window_manager* wm );
void wbd_draw( window_manager* wm );

void draw_points( int16_t* coord2d, COLOR color, uint count, window_manager* wm );
void draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm );
void draw_rect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm );
void draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm );
void draw_hgradient( int x, int y, int xsize, int ysize, COLOR c, int t1, int t2, window_manager* wm );
void draw_vgradient( int x, int y, int xsize, int ysize, COLOR c, int t1, int t2, window_manager* wm );
void draw_corners( int x, int y, int xsize, int ysize, int size, int len, COLOR c, window_manager* wm );
void draw_polygon( sdwm_polygon* p, window_manager* wm );
int draw_char( uint32_t c, int x, int y, window_manager* wm );
int draw_char_vert( uint32_t c, int x, int y, window_manager* wm ); //top to bottom
void draw_string( const char* str, int x, int y, window_manager* wm );
void draw_string_vert( const char* str, int x, int y, window_manager* wm ); //top to bottom
void draw_string_wordwrap( const char* str, int x, int y, int xsize, int* out_xsize, int* out_ysize, bool dont_draw, window_manager* wm );
void draw_string_limited( const char* str, int x, int y, int limit, window_manager* wm ); //limit in utf8 chars
void draw_updown( int x, int y, COLOR color, int down_add, window_manager* wm );
void draw_leftright( int x, int y, COLOR color, int right_add, window_manager* wm );
void draw_image_scaled( int dest_x, int dest_y, sdwm_image_scaled* img, window_manager* wm );

int font_char_x_size( uint32_t c, int font, window_manager* wm );
int font_char_y_size( int font, window_manager* wm );
int font_string_x_size( const char* str, int font, window_manager* wm );
int font_string_x_size_limited( const char* str, int limit, int font, window_manager* wm ); //limit in utf8 chars
int font_string_y_size( const char* str, int font, window_manager* wm );
int char_x_size( uint32_t c, window_manager* wm );
int char_y_size( window_manager* wm );
int string_x_size( const char* str, window_manager* wm );
int string_x_size_limited( const char* str, int limit, window_manager* wm ); //limit in utf8 chars
int string_y_size( const char* str, window_manager* wm );

//###################################
//## STANDARD WINDOW HANDLERS:     ##
//###################################

int null_handler( sundog_event* evt, window_manager* wm );

int desktop_handler( sundog_event* evt, window_manager* wm );

#define DIVIDER_FLAG_DYNAMIC_XSIZE		( 1 << 0 )
#define DIVIDER_FLAG_DYNAMIC_YSIZE		( 1 << 1 )
#define DIVIDER_FLAG_PUSHING_XAXIS		( 1 << 2 )
#define DIVIDER_FLAG_PUSHING_YAXIS		( 1 << 3 )
#define DIVIDER_FLAG_WITH_SCROLL_BUTTON		( 1 << 4 )
int divider_handler( sundog_event* evt, window_manager* wm );
void bind_divider_to( WINDOWPTR win, WINDOWPTR bind_to, int bind_num, window_manager* wm );
void set_divider_push( WINDOWPTR win, WINDOWPTR push_win, window_manager* wm );
void set_divider_scroll_parameters( WINDOWPTR win, int min, int max, window_manager* wm );
int get_divider_scroll_value( WINDOWPTR win );
void set_divider_scroll_value( WINDOWPTR win, int val );
void divider_set_flags( WINDOWPTR win, uint32_t flags );
uint32_t divider_get_flags( WINDOWPTR win );

#define LABEL_FLAG_ALIGN_TOP			( 1 << 0 )
#define LABEL_FLAG_ALIGN_BOTTOM			( 1 << 1 )
#define LABEL_FLAGS_ALIGN			( LABEL_FLAG_ALIGN_TOP | LABEL_FLAG_ALIGN_BOTTOM )
#define LABEL_FLAG_WORDWRAP			( 1 << 2 )
#define LABEL_FLAG_WINCOLOR_IS_TEXTCOLOR	( 1 << 3 )
int label_handler( sundog_event* evt, window_manager* wm );
void label_set_flags( WINDOWPTR win, uint32_t flags );
uint32_t label_get_flags( WINDOWPTR win );

#define TEXT_FLAG_CALL_HANDLER_ON_ANY_CHANGES	( 1 << 0 )
#define TEXT_FLAG_CALL_HANDLER_ON_ESCAPE	( 1 << 1 )
#define TEXT_FLAG_NO_VIRTUAL_KBD		( 1 << 2 )
#define TEXT_ACTION_CHANGED	0
#define TEXT_ACTION_ENTER	1
#define TEXT_ACTION_ESCAPE	2
#define TEXT_ACTION_SWITCH	3
int text_handler( sundog_event* evt, window_manager* wm );
void text_changed( WINDOWPTR win );
void text_clipboard_cut( WINDOWPTR win );
void text_clipboard_copy( WINDOWPTR win );
void text_clipboard_paste( WINDOWPTR win );
void text_set_text( WINDOWPTR win, const char* text, window_manager* wm );
void text_insert_text( WINDOWPTR win, const char* text, int pos );
void text_delete_text( WINDOWPTR win, int pos, int len );
void text_set_label( WINDOWPTR win, const char* label );
void text_set_cursor_position( WINDOWPTR win, int cur_pos, window_manager* wm );
void text_set_value( WINDOWPTR win, int val, window_manager* wm );
void text_set_value2( WINDOWPTR win, int val, window_manager* wm );
void text_set_fvalue( WINDOWPTR win, double val );
char* text_get_text( WINDOWPTR win, window_manager* wm );
char* text_get_text( WINDOWPTR win, int pos, int len ); //pos/len - in utf32 chars
int text_get_cursor_position( WINDOWPTR win, window_manager* wm );
int text_get_value( WINDOWPTR win, window_manager* wm );
double text_get_fvalue( WINDOWPTR win );
int text_get_last_action( WINDOWPTR win );
void text_set_zoom( WINDOWPTR win, int zoom, window_manager* wm );
void text_set_color( WINDOWPTR win, COLOR c );
void text_set_range( WINDOWPTR win, int min, int max );
void text_set_step( WINDOWPTR win, int step );
bool text_is_readonly( WINDOWPTR win );
bool text_get_editing_state( WINDOWPTR win );
void text_set_flags( WINDOWPTR win, uint32_t flags );
uint32_t text_get_flags( WINDOWPTR win );

#define BUTTON_FLAG_HANDLER_CALLED_FROM_TIMER	( 1 << 0 )
#define BUTTON_FLAG_AUTOBACKCOLOR		( 1 << 1 )
#define BUTTON_FLAG_SHOW_VALUE			( 1 << 2 ) //menu-based switch: show current selected item (data->menu_val) of the popup menu
#define BUTTON_FLAG_SHOW_PREV_VALUE		( 1 << 3 ) //menu-based switch: show previous selected item in the popup menu
#define BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW	( 1 << 4 )
#define BUTTON_FLAG_AUTOREPEAT			( 1 << 5 )
#define BUTTON_FLAG_FLAT 			( 1 << 6 )
int button_handler( sundog_event* evt, window_manager* wm );
void button_set_begin_handler( WINDOWPTR win, win_action_handler2_t handler, void* handler_data );
void button_set_end_handler( WINDOWPTR win, win_action_handler2_t handler, void* handler_data );
void button_set_menu( WINDOWPTR win, const char* menu );
void button_set_menu_val( WINDOWPTR win, int val );
int button_get_menu_val( WINDOWPTR win );
int button_get_prev_menu_val( WINDOWPTR win );
void button_set_text( WINDOWPTR win, const char* text );
void button_set_text_color( WINDOWPTR win, COLOR c );
void button_set_text_opacity( WINDOWPTR win, uint8_t opacity );
int button_get_text_opacity( WINDOWPTR win );
void button_set_val( WINDOWPTR win, const char* val );
void button_set_images( WINDOWPTR win, sdwm_image_scaled* img1, sdwm_image_scaled* img2 );
void button_set_flags( WINDOWPTR win, uint32_t flags );
uint32_t button_get_flags( WINDOWPTR win );
int button_get_evt_flags( WINDOWPTR win );
int button_get_optimal_xsize( const char* button_name, int font, bool smallest_as_possible, window_manager* wm );
#define BUTTON_AUTOREPEAT_VARS \
    int autorepeat_timer; \
    stime_ms_t autorepeat_base;
#define BUTTON_AUTOREPEAT_INIT( DATA ) { DATA->autorepeat_timer = -1; }
#define BUTTON_AUTOREPEAT_DEINIT( DATA ) { remove_timer( DATA->autorepeat_timer, wm ); DATA->autorepeat_timer = -1; }
#define BUTTON_AUTOREPEAT_START( DATA, HANDLER ) \
{ \
    remove_timer( DATA->autorepeat_timer, wm ); \
    int autorepeat_delay = wm->mouse_autorepeat_delay; \
    DATA->autorepeat_timer = add_timer( HANDLER, DATA, autorepeat_delay, wm ); \
    DATA->autorepeat_base = stime_ms() + autorepeat_delay; \
}
#define BUTTON_AUTOREPEAT_ACCELERATE( DATA ) button_autorepeat_accelerate( timer, DATA->autorepeat_base, wm )
void button_autorepeat_accelerate( sundog_timer* timer, stime_ms_t base, window_manager* wm );

#define LIST_ACTION_UPDOWN	1
#define LIST_ACTION_ENTER	2
#define LIST_ACTION_CLICK	3
#define LIST_ACTION_RCLICK	4
#define LIST_ACTION_DOUBLECLICK	5
#define LIST_ACTION_ESCAPE	6
#define LIST_ACTION_BACK	7
#define LIST_FLAG_IGNORE_ESCAPE			( 1 << 0 )
#define LIST_FLAG_IGNORE_ENTER			( 1 << 1 )
#define LIST_FLAG_IGNORE_BACK			( 1 << 2 )
#define LIST_FLAG_RIGHT_ALIGNMENT_ON_OVERFLOW	( 1 << 3 )
#define LIST_FLAG_SHADE_LEFT_FILENAME_PART	( 1 << 4 ) //[shaded dir1/dir2/]filename
#define LIST_FLAG_SHADE_FILENAME_EXT		( 1 << 5 ) //filename[shaded .extension]
int list_handler( sundog_event* evt, window_manager* wm );
slist_data* list_get_data( WINDOWPTR win, window_manager* wm );
int list_get_last_action( WINDOWPTR win, window_manager* wm );
void list_select_item( WINDOWPTR win, int item_num, bool make_it_visible );
int list_get_selected_item( WINDOWPTR win );
int list_get_pressed( WINDOWPTR win );
void list_set_flags( WINDOWPTR win, uint32_t flags );
uint32_t list_get_flags( WINDOWPTR win );

enum scrollbar_hex
{
    scrollbar_hex_off,
    scrollbar_hex_scaled, //scaled: 0 (0%) ... 0x8000 (100%);
    scrollbar_hex_normal, //normal hex mode without show_offset;
    scrollbar_hex_normal_with_offset, //normal with show_offset;
};
//Scrollbar flags:
//first 4 bit = response function f() type;
//f(x) will be used to remap the slider's normalized position to a normalized output;
//x = normalized slider position; y = normalized output; final value = f(x)*(max-min)+min;
#define SCROLLBAR_FLAG_LIN	0 //linear: y = x
#define SCROLLBAR_FLAG_EXP2	1 //exponential fn approximation: y = x^2
#define SCROLLBAR_FLAG_EXP3	2 //... y = x^3
#define SCROLLBAR_FLAG_INVEXP3	3 //... (convex upwards) y = 1-(1-x)^3
#define SCROLLBAR_FLAGS_CURVE	15
#define SCROLLBAR_FLAG_INPUT	( 1 << 4 )
#define SCROLLBAR_FLAG_COLORIZE	( 1 << 5 ) //color algorithm 2: colorize all elements
void draw_scrollbar_horizontal_selection( WINDOWPTR win, int x );
int scrollbar_handler( sundog_event* evt, window_manager* wm );
#define SCROLLBAR_DONT_CHANGE_VALUE (-1024*1024)
void scrollbar_set_parameters( WINDOWPTR win, int cur, int max_value, int page_size, int step_size, window_manager* wm );
void scrollbar_set_value( WINDOWPTR win, int val, window_manager* wm );
void scrollbar_set_value2( WINDOWPTR win, int val, window_manager* wm ); //only if editing state == false
int scrollbar_get_value( WINDOWPTR win, window_manager* wm );
int scrollbar_get_evt_flags( WINDOWPTR win );
int scrollbar_get_step( WINDOWPTR win );
void scrollbar_set_name( WINDOWPTR win, const char* name, window_manager* wm );
void scrollbar_set_values( WINDOWPTR win, const char* values, char delimiter );
void scrollbar_set_showing_offset( WINDOWPTR win, int offset, window_manager* wm );
void scrollbar_set_hex_mode( WINDOWPTR win, scrollbar_hex hex, window_manager* wm );
void scrollbar_set_normal_value( WINDOWPTR win, int normal_value, window_manager* wm );
void scrollbar_set_flags( WINDOWPTR win, uint32_t flags );
uint32_t scrollbar_get_flags( WINDOWPTR win );
void scrollbar_call_handler( WINDOWPTR win );
bool scrollbar_get_editing_state( WINDOWPTR win );

#define RESIZER_FLAG_TOPDOWN	( 1 << 0 )
#define RESIZER_FLAG_OPTBTN	( 1 << 1 )
int resizer_handler( sundog_event* evt, window_manager* wm );
void resizer_set_parameters( WINDOWPTR win, int min_x, int min_y );
void resizer_get_vals( WINDOWPTR win, int* x, int* y );
void resizer_set_opt_handler( WINDOWPTR win, win_action_handler_t handler, void* handler_data );
void resizer_set_flags( WINDOWPTR win, uint32_t flags );
uint32_t resizer_get_flags( WINDOWPTR win );

int keyboard_handler( sundog_event* evt, window_manager* wm );
void hide_keyboard_for_text_window( window_manager* wm );
void show_keyboard_for_text_window( WINDOWPTR text, const char* name, window_manager* wm );

void clear_fdialog_constructor_options( window_manager* wm );
int fdialog_handler( sundog_event* evt, window_manager* wm );
char* fdialog_get_filename( WINDOWPTR win );

#define DIALOG_FLAG_AUTOREMOVE_ITEMS		( 1 << 0 ) //free dialog_item* items before close
void clear_dialog_constructor_options( window_manager* wm );
int dialog_handler( sundog_event* evt, window_manager* wm );
void dialog_reinit_item( dialog_item* item ); //change item properties
void dialog_reinit_items( WINDOWPTR pwin, bool show_and_focus ); //create new items + change item props; pwin = dialog or decorator with dialog
dialog_item* dialog_get_items( WINDOWPTR win ); //...
void dialog_set_flags( WINDOWPTR win, uint32_t flags ); //...
void dialog_set_par( WINDOWPTR win, int par_id, int val ); //...
int dialog_get_par( WINDOWPTR win, int par_id );
int dialog_get_item_ysize( int type, WINDOWPTR pwin ); //pwin = dialog or decorator with dialog
dialog_item* dialog_new_item( dialog_item** itemlist );
dialog_item* dialog_get_item( dialog_item* itemlist, uint32_t id );
void dialog_stack_add( WINDOWPTR win );
void dialog_stack_del( WINDOWPTR win );

#define POPUP_DIALOG_FLAG_DONT_AUTOCLOSE	( 1 << 0 )
void clear_popup_dialog_constructor_options( window_manager* wm );
int popup_handler( sundog_event* evt, window_manager* wm );
void popup_set_prev( WINDOWPTR win, int v );

extern bool g_color_theme_changed;
int colortheme_handler( sundog_event* evt, window_manager* wm );
COLOR colortheme_get_color( int theme, int item, window_manager* wm );

int ui_scale_handler( sundog_event* evt, window_manager* wm );

int keymap_handler( sundog_event* evt, window_manager* wm );

extern bool g_clear_settings;
#define PREFS_FLAG_NO_COLOR_THEME		( 1 << 0 )
#define PREFS_FLAG_NO_FONTS			( 1 << 1 )
#define PREFS_FLAG_NO_CONTROL_TYPE		( 1 << 2 )
#define PREFS_FLAG_NO_KEYMAP			( 1 << 3 )
int prefs_main_handler( sundog_event* evt, window_manager* wm );
int prefs_ui_handler( sundog_event* evt, window_manager* wm );
int prefs_svideo_handler( sundog_event* evt, window_manager* wm );
int prefs_audio_handler( sundog_event* evt, window_manager* wm );
int prefs_handler( sundog_event* evt, window_manager* wm );

//###################################
//## DIALOG CONSTRUCTORS           ##
//## & BUILT-IN WINDOWS:           ##
//###################################

const char* wm_get_string( wm_string str_id );

#define DIALOG_FLAG_SINGLE			( 1 << 0 )
WINDOWPTR dialog_open( const char* caption, const char* text, const char* buttons, uint32_t flags, window_manager* wm ); //retval: decorator
int dialog( const char* caption, const char* text, const char* buttons, bool single, window_manager* wm ); //INTERNAL EVENT LOOP!
int dialog( const char* caption, const char* text, const char* buttons, window_manager* wm ); //INTERNAL EVENT LOOP!
void edialog_open( const char* error_str1, const char* error_str2, window_manager* wm ); //Error information dialog with the log saving buttons
#define FDIALOG_FLAG_LOAD			( 1 << 0 )
#define FDIALOG_FLAG_FULLSCREEN			( 1 << 1 )
#define FDIALOG_FLAG_WITH_OVERWRITE_DIALOG	( 1 << 2 )
//mask: "jpg/png" or NULL for all files
WINDOWPTR fdialog_open( const char* name, const char* mask, const char* id, const char* def_filename, uint32_t flags, window_manager* wm );
char* fdialog( const char* name, const char* mask, const char* id, const char* def_filename, uint32_t flags, window_manager* wm ); //INTERNAL EVENT LOOP!
WINDOWPTR fdialog_overwrite_open( char* filename, window_manager* wm );
int dialog_overwrite( char* filename, window_manager* wm ); //0 - YES; 1 - NO; INTERNAL EVENT LOOP!
#define POPUP_MENU_POS_AUTO (-12345) //use it for X or Y
#define POPUP_MENU_POS_AUTO2 (-12346) //use it for X or Y
WINDOWPTR popup_menu_open( const char* name, const char* items, int x, int y, COLOR color, int prev, window_manager* wm );
int popup_menu( const char* name, const char* items, int x, int y, COLOR color, int prev, window_manager* wm ); //INTERNAL EVENT LOOP!
int popup_menu( const char* name, const char* items, int x, int y, COLOR color, window_manager* wm ); //INTERNAL EVENT LOOP!
void prefs_clear( window_manager* wm );
void prefs_add_sections( const char** names, void** handlers, int num, window_manager* wm );
void prefs_add_default_sections( window_manager* wm, bool without_main );
void prefs_open( void* host, window_manager* wm );
void colortheme_open( window_manager* wm );
void ui_scale_open( window_manager* wm );
void keymap_open( window_manager* wm );

extern bool g_webserver_available;
void webserver_open( window_manager* wm );
void webserver_wait_for_close( window_manager* wm );

//###################################
//## DEVICE DEPENDENT:             ##
//###################################

#ifdef OPENGL

#define GL_ORTHO_MAX_DEPTH	32768

//Small shift for correct (per-pixel accuracy) display of 2D UI elements:
#define GL_2D_LINE_SHIFT	0.5
//0.5 - most accurate value, but it leads to inaccuracy when scaling the screen by an even factor (gl_xscale, gl_yscale, screen_zoom);
//to avoid this inaccuracy - use 0.375

//THREAD-SAFE:
//(can be called from any thread with the same OpenGL context)
int gl_lock( window_manager* wm );
void gl_unlock( window_manager* wm );
int gl_init( window_manager* wm );
void gl_deinit( window_manager* wm );
void gl_resize( window_manager* wm );

//NOT THREAD-SAFE:
//(can only be called between gl_lock/unlock() or win_draw_lock/unlock())
#define GL_BIND_TEXTURE( WM, ID ) if( WM->gl_current_texture != ID ) { glBindTexture( GL_TEXTURE_2D, ID ); WM->gl_current_texture = ID; }
#define GL_DELETE_TEXTURES( WM, N, IDS ) \
{ \
    glDeleteTextures( N, IDS ); \
    for( int i = 0; i < (int)(N); i++ ) \
    { \
	if( (IDS)[ i ] == WM->gl_current_texture ) \
	{ \
	    WM->gl_current_texture = 0; \
	    break; \
	} \
    } \
}
#define GL_CHANGE_PROG_XY_ADD( P, XY_ADD ) \
{ \
    if( P->xy_add != (float)(XY_ADD) ) \
    { \
	P->xy_add = (float)(XY_ADD); \
	glUniform1f( P->uniforms[ GL_PROG_UNI_XY_ADD ], (float)(XY_ADD) ); \
    } \
}
#define GL_CHANGE_PROG_COLOR( P, COLOR, OPACITY ) \
{ \
    if( P->color != COLOR || P->opacity != OPACITY ) \
    { \
	P->color = COLOR; \
	P->opacity = OPACITY; \
	glUniform4f( P->uniforms[ GL_PROG_UNI_COLOR ], (float)red( COLOR ) / 255, (float)green( COLOR ) / 255, (float)blue( COLOR ) / 255, (float)(OPACITY) / 255 ); \
    } \
}
#define GL_CHANGE_PROG_COLOR_RGBA( P, R, G, B, A ) \
{ \
    COLOR c = get_color( R, G, B ); \
    if( P->color != c || P->opacity != A ) \
    { \
	P->color = c; \
	P->opacity = A; \
	glUniform4f( P->uniforms[ GL_PROG_UNI_COLOR ], (float)(R) / 255, (float)(G) / 255, (float)(B) / 255, (float)(A) / 255 ); \
    } \
}
void gl_draw_points( int16_t* coord2d, COLOR color, int count, window_manager* wm );
void gl_draw_triangles( int16_t* coord2d, COLOR color, int count, window_manager* wm );
void gl_draw_polygon( sdwm_polygon* p, window_manager* wm );
void gl_draw_image_scaled(
    int dest_x, int dest_y,
    int dest_xs, int dest_ys,
    float src_x, float src_y,
    float src_xs, float src_ys,
    sdwm_image* img,
    window_manager* wm );
void gl_draw_image_scaled_vert(
    int dest_x, int dest_y, //top left corner
    int dest_xs, int dest_ys,
    float src_x, float src_y, //source in normal (norizontal) orientation
    float src_xs, float src_ys,
    sdwm_image* img,
    window_manager* wm );
void gl_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm );
void gl_draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm );
void gl_draw_image(
    int dest_x, int dest_y,
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm );
void gl_bind_framebuffer( GLuint fb, window_manager* wm );
void gl_set_default_viewport( window_manager* wm );
void gl_set_default_blend_func( window_manager* wm );
void gl_print_shader_info_log( GLuint shader );
void gl_print_program_info_log( GLuint program );
GLuint gl_make_shader( const char* shader_source, GLenum type );
GLuint gl_make_program( GLuint vertex_shader, GLuint fragment_shader );
gl_program_struct* gl_program_new( GLuint vertex_shader, GLuint fragment_shader );
void gl_program_remove( gl_program_struct* p );
void gl_init_uniform( gl_program_struct* prog, int n, const char* name );
void gl_init_attribute( gl_program_struct* prog, int n, const char* name );
void gl_enable_attributes( gl_program_struct* prog, uint attr );
void gl_program_reset( window_manager* wm );

#else

inline int gl_lock( window_manager* wm ) { return 0; }
#define gl_unlock( wm ) {}
#define gl_resize( wm ) {}

#endif
