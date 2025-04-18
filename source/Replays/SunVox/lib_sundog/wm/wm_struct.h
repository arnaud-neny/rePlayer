#pragma once

#define WM_EVENTS	128 //must be 2/4/8/16/32...
#define WM_TIMERS	16
#define WM_FONTS	3
#define WM_TOUCHES	10
#define WM_TOUCH_EVENTS	32 //buffer for collect_touch_events()

#define WM_TOUCH_ID	size_t

#ifdef DIRECTDRAW
    #define FRAMEBUFFER
#endif

#ifdef OPENGL
    #ifndef OPENGLES
	#ifdef OS_UNIX
	    //Include GL >1.1 entry points:
	    #define GL_GLEXT_PROTOTYPES
        #else
    	    //Windows OpenGL library only exposes OpenGL 1.1 entry points, 
    	    //so all functions beyond that version are loaded with wglGetProcAddress.
        #endif
    #endif
    #ifdef OS_IOS
	#include <OpenGLES/ES2/gl.h>
    	#include <OpenGLES/ES2/glext.h>
    #endif
    #ifdef OS_MACOS
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>
        #include <OpenGL/glu.h>
        #include <OpenGL/OpenGL.h>
    #endif
    #if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_WIN)
	#ifdef OPENGLES
	    #include <GLES2/gl2.h>
	    #include <GLES2/gl2ext.h>
	    #include <EGL/egl.h>
	#else
	    #include <GL/gl.h>
	    #include <GL/glext.h>
	    #include <GL/glu.h>
	#endif
    #endif
    #ifdef OS_WIN
	//Framebuffer:
	extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
        extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
	extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
	extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
	//Shaders:
	extern PFNGLATTACHSHADERPROC glAttachShader;
	extern PFNGLCOMPILESHADERPROC glCompileShader;
	extern PFNGLCREATEPROGRAMPROC glCreateProgram;
	extern PFNGLCREATESHADERPROC glCreateShader;
	extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
	extern PFNGLDELETESHADERPROC glDeleteShader;
	extern PFNGLDETACHSHADERPROC glDetachShader;
	extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
	extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
	extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
	extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
	extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
	extern PFNGLGETSHADERIVPROC glGetShaderiv;
	extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
	extern PFNGLLINKPROGRAMPROC glLinkProgram;
	extern PFNGLSHADERSOURCEPROC glShaderSource;
	extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
	extern PFNGLUSEPROGRAMPROC glUseProgram;
	extern PFNGLUNIFORM1FPROC glUniform1f;
	extern PFNGLUNIFORM2FPROC glUniform2f;
	extern PFNGLUNIFORM3FPROC glUniform3f;
	extern PFNGLUNIFORM4FPROC glUniform4f;
	extern PFNGLUNIFORM1IPROC glUniform1i;
	extern PFNGLUNIFORM2IPROC glUniform2i;
	extern PFNGLUNIFORM3IPROC glUniform3i;
	extern PFNGLUNIFORM4IPROC glUniform4i;
	extern PFNGLUNIFORM1FVPROC glUniform1fv;
	extern PFNGLUNIFORM2FVPROC glUniform2fv;
	extern PFNGLUNIFORM3FVPROC glUniform3fv;
	extern PFNGLUNIFORM4FVPROC glUniform4fv;
	extern PFNGLUNIFORM1IVPROC glUniform1iv;
	extern PFNGLUNIFORM2IVPROC glUniform2iv;
	extern PFNGLUNIFORM3IVPROC glUniform3iv;
	extern PFNGLUNIFORM4IVPROC glUniform4iv;
	extern PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
	extern PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
	extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
	extern PFNGLVALIDATEPROGRAMPROC glValidateProgram;
	extern PFNGLVERTEXATTRIB1DPROC glVertexAttrib1d;
	extern PFNGLVERTEXATTRIB1DVPROC glVertexAttrib1dv;
	extern PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
	extern PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
	extern PFNGLVERTEXATTRIB1SPROC glVertexAttrib1s;
	extern PFNGLVERTEXATTRIB1SVPROC glVertexAttrib1sv;
	extern PFNGLVERTEXATTRIB2DPROC glVertexAttrib2d;
	extern PFNGLVERTEXATTRIB2DVPROC glVertexAttrib2dv;
	extern PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f;
	extern PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
	extern PFNGLVERTEXATTRIB2SPROC glVertexAttrib2s;
	extern PFNGLVERTEXATTRIB2SVPROC glVertexAttrib2sv;
	extern PFNGLVERTEXATTRIB3DPROC glVertexAttrib3d;
	extern PFNGLVERTEXATTRIB3DVPROC glVertexAttrib3dv;
	extern PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f;
	extern PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
	extern PFNGLVERTEXATTRIB3SPROC glVertexAttrib3s;
	extern PFNGLVERTEXATTRIB3SVPROC glVertexAttrib3sv;
	extern PFNGLVERTEXATTRIB4NBVPROC glVertexAttrib4Nbv;
	extern PFNGLVERTEXATTRIB4NIVPROC glVertexAttrib4Niv;
	extern PFNGLVERTEXATTRIB4NSVPROC glVertexAttrib4Nsv;
	extern PFNGLVERTEXATTRIB4NUBPROC glVertexAttrib4Nub;
	extern PFNGLVERTEXATTRIB4NUBVPROC glVertexAttrib4Nubv;
	extern PFNGLVERTEXATTRIB4NUIVPROC glVertexAttrib4Nuiv;
	extern PFNGLVERTEXATTRIB4NUSVPROC glVertexAttrib4Nusv;
	extern PFNGLVERTEXATTRIB4BVPROC glVertexAttrib4bv;
	extern PFNGLVERTEXATTRIB4DPROC glVertexAttrib4d;
	extern PFNGLVERTEXATTRIB4DVPROC glVertexAttrib4dv;
	extern PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f;
	extern PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
	extern PFNGLVERTEXATTRIB4IVPROC glVertexAttrib4iv;
	extern PFNGLVERTEXATTRIB4SPROC glVertexAttrib4s;
	extern PFNGLVERTEXATTRIB4SVPROC glVertexAttrib4sv;
	extern PFNGLVERTEXATTRIB4UBVPROC glVertexAttrib4ubv;
	extern PFNGLVERTEXATTRIB4UIVPROC glVertexAttrib4uiv;
	extern PFNGLVERTEXATTRIB4USVPROC glVertexAttrib4usv;
	extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
	extern PFNGLACTIVETEXTUREPROC glActiveTexture;
	//Other:
	extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
    #endif
#endif

#ifdef X11
    #ifdef OPENGL
	#ifndef OPENGLES
	    #include <GL/glx.h>
	#endif
    #endif
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/Xatom.h>
    #include <X11/keysym.h>
    #include <X11/XKBlib.h>
#ifdef MULTITOUCH
    #include <X11/extensions/XInput.h>
    #include <X11/extensions/XInput2.h>
#endif
#endif

#if defined(DIRECTDRAW) && defined(OS_WIN)
    #include "ddraw.h"
#endif

#if SDL >= 2 && defined(DIRECTDRAW)
    #define MULTITOUCH
#endif

#ifdef COLOR8BITS
    #define COLOR	uint8_t
    #define COLORSIGNED	int8_t
    #define COLORPTR	uint8_t*
    #define COLORLEN	(int)1
    #define COLORBITS	8
    #define COLORMASK	0xFF
    #define CLEARCOLOR	0
#endif
#ifdef COLOR16BITS
    #define COLOR	uint16_t
    #define COLORSIGNED	int16_t
    #define COLORPTR  	uint16_t*
    #define COLORLEN  	(int)2
    #define COLORBITS 	16
    #define COLORMASK 	0xFFFF
    #define CLEARCOLOR 	0
    #if !defined(RGB556) && !defined(RGB555) && !defined(RGB565)
	#if defined(GDI) && !defined(OPENGL)
	    #define RGB555
	#else
	    #define RGB565
	#endif
    #endif
#endif
#ifdef COLOR32BITS
    #define COLOR     	uint32_t
    #define COLORSIGNED	int32_t
    #define COLORPTR  	uint32_t*
    #define COLORLEN  	(int)4
    #define COLORBITS 	32
    #define COLORMASK 	0x00FFFFFF
    #define CLEARCOLOR 	0x00000000
#endif

#ifdef OPENGL
    #define SCREEN_ZOOM_SUPPORTED 1
    #ifndef OS_IOS
	#define SCREEN_ROTATE_SUPPORTED 1
    #endif
#endif
#if defined(DIRECTDRAW) && defined(OS_UNIX)
    #define SCREEN_ZOOM_SUPPORTED 1
    #define SCREEN_ROTATE_SUPPORTED 1
#endif
#ifdef OS_WINCE
    #define SCREEN_ZOOM_SUPPORTED 1
    #define SCREEN_ROTATE_SUPPORTED 1
    enum
    {
	VIDEODRIVER_NONE = 0,
        VIDEODRIVER_GAPI,
    	VIDEODRIVER_RAW,
	VIDEODRIVER_DDRAW,
        VIDEODRIVER_GDI
    };
#endif
#if defined(OS_IOS) || defined(OS_ANDROID)
    #define SCREEN_SAFE_AREA_SUPPORTED 1
#endif

//WM strings:
enum wm_string
{
    STR_WM_OK,
    STR_WM_OKCANCEL,
    STR_WM_CANCEL,
    STR_WM_YESNO,
    STR_WM_YES,
    STR_WM_YES_CAP,
    STR_WM_NO,
    STR_WM_NO_CAP,
    STR_WM_ON_CAP,
    STR_WM_OFF_CAP,
    STR_WM_CLOSE,
    STR_WM_RESET,
    STR_WM_RESET_ALL,
    STR_WM_LOAD,
    STR_WM_SAVE,
    STR_WM_INFO,
    STR_WM_AUTO,
    STR_WM_FIND,
    STR_WM_EDIT,
    STR_WM_NEW,
    STR_WM_DELETE,
    STR_WM_DELETE2,
    STR_WM_RENAME,
    STR_WM_RENAME_FILE2,
    STR_WM_RENAME_DIR2,
    STR_WM_CUT,
    STR_WM_CUT2,
    STR_WM_COPY,
    STR_WM_COPY2,
    STR_WM_COPY_ALL,
    STR_WM_PASTE,
    STR_WM_PASTE2,
    STR_WM_CREATE_DIR,
    STR_WM_DELETE_DIR2,
    STR_WM_DELETE_CUR_DIR,
    STR_WM_DELETE_CUR_DIR2,
    STR_WM_RECURS,
    STR_WM_ARE_YOU_SURE,
    STR_WM_ERROR,
    STR_WM_NOT_FOUND,
    STR_WM_LOG,
    STR_WM_SAVE_LOG,
    STR_WM_EMAIL_LOG,
    STR_WM_SYSEXPORT,
    STR_WM_SYSEXPORT2, //open in
    STR_WM_SYSIMPORT,
    STR_WM_SELECT_FILE_TO_EXPORT,
    STR_WM_APPLY,
    STR_WM_APPLY2,

    STR_WM_MS,
    STR_WM_SEC,
    STR_WM_MINUTE,
    STR_WM_MINUTE_LONG,
    STR_WM_HOUR,
    STR_WM_HZ,
    STR_WM_INCH,
    STR_WM_DECIBEL,
    STR_WM_BIT,
    STR_WM_BYTES,

#ifdef DEMO_MODE
    STR_WM_DEMOVERSION_FN,
    STR_WM_DEMOVERSION,
    STR_WM_DEMOVERSION_BUY,
#endif

#ifdef OS_ANDROID
    STR_WM_RES_UNPACKING,
    STR_WM_RES_UNKNOWN,
    STR_WM_RES_LOCAL_RES,
    STR_WM_RES_SMEM_CARD,
    STR_WM_RES_UNPACK_ERROR,
#endif

    STR_WM_PREFERENCES,
    STR_WM_PREFS_CHANGED,
    STR_WM_INTERFACE,
    STR_WM_AUDIO,
    STR_WM_VIDEO,
    STR_WM_CAMERA,
    STR_WM_BACK_CAM,
    STR_WM_FRONT_CAM,
    STR_WM_MAXFPS,
    STR_WM_UI_ROTATION,
    STR_WM_UI_ROTATION2,
    STR_WM_CAM_ROTATION,
    STR_WM_CAM_ROTATION2,
    STR_WM_CAM_MODE,
    STR_WM_DOUBLE_CLICK_TIME,
    STR_WM_CTL_TYPE,
    STR_WM_CTL_FINGERS,
    STR_WM_CTL_PEN,
    STR_WM_SHOW_ZOOM_BTNS,
    STR_WM_SHOW_ZOOM_BUTTONS,
    STR_WM_SHOW_KBD,
    STR_WM_HIDE_SYSTEM_BARS,
    STR_WM_LOWRES,
    STR_WM_LOWRES_IOS_NOTICE,
    STR_WM_WINDOW_PARS,
    STR_WM_WINDOW_WIDTH,
    STR_WM_WINDOW_HEIGHT,
    STR_WM_WINDOW_FULLSCREEN,
    STR_WM_SET_COLOR_THEME,
    STR_WM_SET_UI_SCALE,
    STR_WM_SHORTCUTS_SHORT,
    STR_WM_SHORTCUTS,
    STR_WM_UI_SCALE,
    STR_WM_COLOR_THEME,
    STR_WM_COLOR_THEME_MSG_RESTART,
    STR_WM_FONTS,
    STR_WM_FONT_MEDIUM_MONO,
    STR_WM_FONT_BIG,
    STR_WM_FONT_SMALL,
    STR_WM_FONT_UPSCALING,
    STR_WM_FONT_FSCALING,
    STR_WM_LANG,
    STR_WM_DRIVER,
    STR_WM_OUTPUT,
    STR_WM_INPUT,
    STR_WM_BUFFER,
    STR_WM_SAMPLE_RATE,
    STR_WM_DEVICE,
    STR_WM_INPUT_DEVICE,
    STR_WM_BUFFER_SIZE,
    STR_WM_ASIO_OPTIONS,
    STR_WM_FIRST_OUT_CH,
    STR_WM_FIRST_IN_CH,
    STR_WM_OPTIONS,
    STR_WM_ADD_OPTIONS,
    STR_WM_ADD_OPTIONS_ASIO,
    STR_WM_MEASUREMENT_MODE,
    STR_WM_CUR_DRIVER,
    STR_WM_CUR_SAMPLE_RATE,
    STR_WM_CUR_LATENCY,
    STR_WM_ACTION,
    STR_WM_SHORTCUTS2,
    STR_WM_SHORTCUT,
    STR_WM_RESET_TO_DEF,
    STR_WM_ENTER_SHORTCUT,
    STR_WM_AUTO_SCALE,
    STR_WM_PPI,
    STR_WM_BUTTON_SCALE,
    STR_WM_FONT_SCALE,
    STR_WM_BUTTON,
    STR_WM_RED,
    STR_WM_GREEN,
    STR_WM_BLUE,
    STR_WM_HIDE_RECENT_FILES,
    STR_WM_SHOW_HIDDEN_FILES,

    STR_WM_OFF_ON_MENU,
    STR_WM_AUTO_ON_OFF_MENU,
    STR_WM_AUTO_YES_NO_MENU,
    STR_WM_CTL_TYPE_MENU,

    STR_WM_FILE_NAME,
    STR_WM_FILE_PATH,
    STR_WM_FILE_RECENT,
    STR_WM_FILE_MSG_NONAME,
    STR_WM_FILE_MSG_NOFILE,
    STR_WM_FILE_OVERWRITE,
};

#define WIN_INIT_FLAG_SCALABLE	    	( 1 << 0 )
#define WIN_INIT_FLAG_NOBORDER	    	( 1 << 1 )
#define WIN_INIT_FLAG_FULLSCREEN    	( 1 << 2 )
#define WIN_INIT_FLAG_MAXIMIZED    	( 1 << 3 )
#define WIN_INIT_FLAG_TOUCHCONTROL	( 1 << 4 )
#define WIN_INIT_FLAG_OPTIMIZE_MOVE_EVENTS ( 1 << 5 )
#define WIN_INIT_FLAG_NOSCREENROTATE	( 1 << 6 )
#define WIN_INIT_FLAG_LANDSCAPE_ONLY	( 1 << 7 )
#define WIN_INIT_FLAG_PORTRAIT_ONLY     ( 1 << 8 )
#define WIN_INIT_FLAG_NOCURSOR		( 1 << 9 )
#define WIN_INIT_FLAG_VSYNC		( 1 << 10 )
#define WIN_INIT_FLAG_FRAMEBUFFER	( 1 << 11 )
#define WIN_INIT_FLAG_NOWINDOW		( 1 << 12 )
#define WIN_INIT_FLAG_NOSYSBARS		( 1 << 13 )
#define WIN_INIT_FLAG_SHRINK_DESKTOP_TO_SAFE_AREA ( 1 << 14 )
#define WIN_INIT_FLAG_NO_FONT_UPSCALE	( 1 << 15 )
#define WIN_INIT_FLAGOFF		16
#define WIN_INIT_FLAGOFF_ANGLE		( WIN_INIT_FLAGOFF ) //2bits (0..3)
#define WIN_INIT_FLAGOFF_ZOOM           ( WIN_INIT_FLAGOFF + 2 ) //3bits (0..7)

enum 
{
    EVT_NULL = 0,
    EVT_GETDATASIZE,
    EVT_AFTERCREATE,
    EVT_BEFORECLOSE,
    EVT_BEFOREHIDE,
    EVT_BEFORESHOW,
    EVT_BEFORERESIZE, //just before recalc_controllers(); only if WIN_FLAG_BEFORERESIZE_ENABLED is set
    EVT_AFTERRESIZE, //just after recalc_controllers(); only if WIN_FLAG_AFTERRESIZE_ENABLED is set
    EVT_DRAW,
    EVT_FRAME,
    EVT_FOCUS,
    EVT_UNFOCUS, //after click on other window
    EVT_MOUSEBUTTONDOWN,
    EVT_MOUSEBUTTONUP,
    EVT_MOUSEMOVE,
    EVT_TOUCHBEGIN,
    EVT_TOUCHEND,
    EVT_TOUCHMOVE,
    EVT_BUTTONDOWN,
    EVT_BUTTONUP,
    EVT_SCREENRESIZE, //after the main screen resize
    EVT_SCREENFOCUS,
    EVT_SCREENUNFOCUS,
    EVT_SUSPEND, //after the "suspended" parameter change
    EVT_DEVSUSPEND, //after the "devices_suspended" parameter change; before device(s) stop/play;
    EVT_CLOSEREQUEST, //CLOSE WINDOW request; can be ignored
    EVT_HELPREQUEST, //HELP request; can be ignored
    EVT_LOADSTATE, //"Load App State" request: the app should open the document OR the module should restore its state (previously stored in the host)
    EVT_SAVESTATE, //"Save App State" request: the module should save its state for the host
    EVT_QUIT, //"Close Application" request; e.g. when user click Main Window Close button; can be ignored
              //available not for all systems;
    //Special window-dependent events:
    EVT_SHOWCHAR, //show unicode character from the parameter "key"
    EVT_PIXICMD, //command from Pixilang to SunDog-based app
    EVT_SUNVOXCMD, //command from SunVox to SunDog-based app
    EVT_REINIT, //UI reinit request (example: from SunVox UI to PSynth module UI)
    EVT_OPT, //toggle options
    EVT_USER //other user-defined event types: EVT_USER + x
};

//Event flags (modifiers and other):
#define EVT_FLAG_SHIFT 			( 1 << 0 )
#define EVT_FLAG_CTRL 			( 1 << 1 )
#define EVT_FLAG_ALT 			( 1 << 2 )
#define EVT_FLAG_MODE 			( 1 << 3 )
#define EVT_FLAG_CMD			( 1 << 4 )
#define EVT_FLAG_MODS			( EVT_FLAG_SHIFT | EVT_FLAG_CTRL | EVT_FLAG_ALT | EVT_FLAG_MODE | EVT_FLAG_CMD )
#define EVT_FLAG_DOUBLECLICK		( 1 << 5 )
#define EVT_FLAG_DONTDRAW		( 1 << 6 )
#define EVT_FLAG_REPEAT			( 1 << 7 ) //this is not the first EVT_BUTTONDOWN, but a repeat, between the first press and the final release
#define EVT_FLAGS_NUM			8 //number of flags available for the user
#define EVT_FLAGS_MASK                  ( ( 1 << EVT_FLAGS_NUM ) - 1 )
//other flags, not available for the user:
#define EVT_FLAG_OPTIMIZED		( 1 << 8 )
#define EVT_FLAG_AUTOREPEAT             ( 1 << 9 ) //for EVT_BUTTONDOWN and EVT_BUTTONUP

//Mouse buttons:
#define MOUSE_BUTTON_LEFT 		(1<<0)
#define MOUSE_BUTTON_MIDDLE 		(1<<1)
#define MOUSE_BUTTON_RIGHT 		(1<<2)
#define MOUSE_BUTTON_SCROLLUP 		(1<<3)
#define MOUSE_BUTTON_SCROLLDOWN 	(1<<4)
#define MOUSE_BUTTON_SCROLLRIGHT 	(1<<5)
#define MOUSE_BUTTON_SCROLLLEFT 	(1<<6)

//Virtual key codes (ASCII):
// 00      : empty;
// 01 - 1F : control codes;
// 20 - 7E : standard ASCII symbols (all letters are small - from 61);
// 7F      : not defined;
// 80 - xx : additional key codes
#define KEY_BACKSPACE   		0x08
#define KEY_TAB         		0x09
#define KEY_ENTER       		0x0D
#define KEY_ESCAPE      		0x1B
#define KEY_SPACE       		0x20

//Additional key codes:
enum 
{
    KEY_F1 = 0x80,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_INSERT,
    KEY_DELETE,
    KEY_HOME,
    KEY_END,
    KEY_PAGEUP,
    KEY_PAGEDOWN,
    KEY_CAPS,
    KEY_MIDI_NOTE, //x = note or ctl; y = channel;
    KEY_MIDI_CTL,
    KEY_MIDI_NRPN,
    KEY_MIDI_RPN,
    KEY_MIDI_PROG,
    KEY_SHIFT, //only modifiers allowed between SHIFT and UNKNOWN! Otherwise the keymap_handler may not work correctly
    KEY_CTRL,
    KEY_ALT,
    KEY_MENU,
    KEY_CMD,
    KEY_FN,
    KEY_UNKNOWN, //virtual key code = code - KEY_UNKNOWN;
};

//Scroll flags:
#define SF_LEFT         		1
#define SF_RIGHT        		2
#define SF_UP           		4
#define SF_DOWN         		8
#define SF_STATICWIN    		16

#define WINDOWPTR			sundog_window*
#define WINDOW				sundog_window

#define DECOR_FLAG_STATIC		( 1 << 0 )
#define DECOR_FLAG_NOBORDER		( 1 << 1 )
#define DECOR_FLAG_NOHEADER		( 1 << 2 )
#define DECOR_FLAG_NORESIZE		( 1 << 3 )
#define DECOR_FLAG_NORESIZE_WHEN_MAX	( 1 << 4 )
#define DECOR_FLAG_MINIMIZED		( 1 << 5 )
#define DECOR_FLAG_MAXIMIZED		( 1 << 6 )
#define DECOR_FLAG_WITH_CLOSE		( 1 << 7 )
#define DECOR_FLAG_WITH_MINIMIZE	( 1 << 8 )
#define DECOR_FLAG_WITH_HELP		( 1 << 9 )
#define DECOR_FLAG_CLOSE_ON_ESCAPE	( 1 << 10 )

#define BORDER_OPACITY			32
#define BORDER_COLOR_WITHOUT_OPACITY	wm->color3
#define PUSHED_OPACITY			wm->pressed_button_color_opacity
#define PUSHED_COLOR_WITHOUT_OPACITY	wm->pressed_button_color
#define PUSHED_COLOR( orig ) 		blend( orig, PUSHED_COLOR_WITHOUT_OPACITY, PUSHED_OPACITY )
#define BORDER_COLOR( orig )		blend( orig, BORDER_COLOR_WITHOUT_OPACITY, BORDER_OPACITY )
#define BUTTON_HIGHLIGHT_COLOR		blend( wm->button_color, wm->selection_color, 90 )
#define LABEL_OPACITY			128

//Difference between these coefficients must be smallest as possible,
//so user can easily change it by changing the single UI Scale parameter:
#define BIG_BUTTON_YSIZE_COEFF 		0.265
#define SCROLLBAR_SIZE_COEFF		0.245
#define POPUP_BUTTON_YSIZE_COEFF	0.2
#define CONTROLLER_YSIZE_COEFF		0.2
#define TEXT_YSIZE_COEFF		0.19

#define DEFAULT_DOUBLE_CLICK_TIME	200
#define DEFAULT_KBD_AUTOREPEAT_DELAY	(1000/3)
#define DEFAULT_KBD_AUTOREPEAT_FREQ	20
#define DEFAULT_MOUSE_AUTOREPEAT_DELAY	(1000/2)
#define DEFAULT_MOUSE_AUTOREPEAT_FREQ	20

typedef size_t                          WCMD;
#define CWIN		    		(WCMD)30000
#define CX1		    		(WCMD)30001
#define CX2		    		(WCMD)30002
#define CY1		    		(WCMD)30003
#define CY2		    		(WCMD)30004
#define CXSIZE		    		(WCMD)30005
#define CYSIZE		    		(WCMD)30006
#define CSUB		    		(WCMD)30007
#define CADD		    		(WCMD)30008
#define CPERC		    		(WCMD)30009
#define CBACKVAL0	    		(WCMD)30010 //disable size-X mode
#define CBACKVAL1	    		(WCMD)30011 //enable size-X mode
#define CMAXVAL		    		(WCMD)30012
#define CMINVAL		    		(WCMD)30013
#define CMULDIV256			(WCMD)30014
#define CPUTR0		    		(WCMD)30015
#define CGETR0		    		(WCMD)30016
#define CR0		    		(WCMD)30017
#define CEND		    		(WCMD)30018

#define IMAGE_NATIVE_RGB		( 1 << 0 )
#define IMAGE_ALPHA8			( 1 << 1 ) //only alpha channel
#define IMAGE_STATIC_SOURCE		( 1 << 2 ) //static external source
#define IMAGE_INTERP			( 1 << 3 ) //interpolation
#define IMAGE_CLEAN			( 1 << 4 ) //new image must be clean
#define IMAGE_NO_XREPEAT		( 1 << 5 ) //default value is unknown, but we can force this
#define IMAGE_NO_YREPEAT		( 1 << 6 ) //...
#define IMAGE_XREPEAT			( 1 << 7 ) //...
#define IMAGE_YREPEAT			( 1 << 8 ) //...

#define VCAP_FLAG_AUDIO_FROM_INPUT	( 1 << 0 )

struct window_manager;
struct sundog_timer;
struct sundog_window;
struct sundog_event;

struct sdwm_rect
{
    int x, y, w, h;
};

struct sdwm_image
{
    window_manager* 	wm;
    int			xsize;
    int			ysize;
    void*		data;
    int			pixelsize; //number of bytes per pixel
    COLOR		color;
    COLOR		backcolor;
    uint8_t		opacity; //0..255
#ifdef OPENGL
    uint		gl_texture_id;
    int			gl_xsize; //internal xsize of the OpenGL texture; may be != xsize;
    int			gl_ysize; //internal ysize...
#endif
    uint		flags;
};

struct sdwm_image_scaled
{
    sdwm_image*		img;
    int 		src_x; //fixed point (IMG_PREC)
    int 		src_y; //fixed point
    int 		src_xsize; //fixed point
    int			src_ysize; //fixed point
    int 		dest_xsize;
    int 		dest_ysize;
};

#define KEYMAP_BIND_DEFAULT 		( 1 << 0 ) //remember it as default value (app startup)
#define KEYMAP_BIND_OVERWRITE		( 1 << 1 )
#define KEYMAP_BIND_RESET_TO_DEFAULT	( 1 << 2 )
#define KEYMAP_BIND_FIX_CONFLICTS	( 1 << 3 ) //pass the binding to the conflicting action (where it is also needed)
#define KEYMAP_ACTION_KEYS		4

struct sundog_keymap_key
{
    uint32_t			key;
    uint16_t			flags;
    uint			pars1;
    uint			pars2;
};

struct sundog_keymap_action
{
    const char*			name; //External string pointer (no autoremove)
    const char*			id; //...
    const char*			group; //...
    sundog_keymap_key		keys[ KEYMAP_ACTION_KEYS ];
    sundog_keymap_key		default_keys[ KEYMAP_ACTION_KEYS ];
};

struct sundog_keymap_section
{
    int				last_added_action;
    const char*			name; //External string pointer (no autoremove)
    const char*			id; //...
    const char*			group; //... group name for the new actions
    sundog_keymap_action*	actions;
    ssymtab*			bindings;
};

struct sundog_keymap
{
    bool 			silent;
    sundog_keymap_section*	sections;
    uint32_t			midi_notes[ ( 128 * 16 ) / 32 ]; //128 notes * 16 channels: 0 - free; 1 - linked to some shortcut;
};

#define WBD_FLAG_ONE_COLOR		1
#define WBD_FLAG_ONE_OPACITY		2

struct sdwm_vertex
{
    int16_t x;
    int16_t y;
    COLOR c;
    uint8_t t;
};

struct sdwm_polygon
{
    int vnum;
    sdwm_vertex* v;
};

#define WIN_FLAG_ALWAYS_INVISIBLE	( 1 << 0 )
#define WIN_FLAG_ALWAYS_ON_TOP		( 1 << 1 )
#define WIN_FLAG_ALWAYS_UNFOCUSED	( 1 << 2 )
#define WIN_FLAG_TRANSPARENT_FOR_FOCUS	( 1 << 3 )
#define WIN_FLAG_TRASH			( 1 << 4 )
#define WIN_FLAG_DOUBLECLICK		( 1 << 5 )
#define WIN_FLAG_DONT_USE_CONTROLLERS	( 1 << 6 )
#define WIN_FLAG_UNFOCUS_HANDLING	( 1 << 7 )
#define WIN_FLAG_FOCUS_HANDLING		( 1 << 8 )
#define WIN_FLAG_ALWAYS_HANDLE_DRAW_EVT	( 1 << 9 )
#define WIN_FLAG_BEFORERESIZE_ENABLED	( 1 << 10 )
#define WIN_FLAG_AFTERRESIZE_ENABLED	( 1 << 11 )

typedef int (*win_handler_t)( sundog_event*, window_manager* );
typedef int (*win_action_handler_t)( void*, WINDOWPTR, window_manager* );
typedef int (*win_action_handler2_t)( void*, WINDOWPTR );

struct sundog_window
{
    window_manager*	wm;
    bool		visible;
    uint16_t	    	flags;
    uint16_t	    	id;
    char*		name; //window name
    int16_t	    	x, y; //x,y (relative)
    int16_t	    	screen_x, screen_y; //real x,y (global screen coordinates)
    int16_t	    	xsize, ysize; //window size
    COLOR	    	color;
    int8_t    		font;
    MWCLIPREGION*	reg;
    WINDOWPTR	    	parent;
    void*		host; //hosting object - engine or some window; parent != host!
    WINDOWPTR*		childs;
    int		    	childs_num;
    win_handler_t 	win_handler;
    void*		data;

    WCMD*		x1com; //x1 window controller - sequence of the commands (controller virtual machine (cvm))
    WCMD*		y1com; //...
    WCMD*		x2com; //...
    WCMD*		y2com; //...
    bool	    	controllers_defined;
    bool	    	controllers_calculated;

    //Action handler:
    win_action_handler_t action_handler;
    int		    	action_result;
    void*		handler_data;

    stime_ticks_t	click_time;
};

//SunDog event handling:
// EVENT 
//   |
// app_event_handler()
//   | (if not handled)
// window 
//   | (if not handled)
// window children
//   | (if not handled)
// handler_of_unhandled_events()

struct sundog_event
{
    uint16_t          	type; //event type
    uint16_t	    	flags;
    stime_ticks_t	time;
    int16_t           	x; //x (absolute) OR midi parameter (note/ctl)
    int16_t           	y; //y (absolute) OR midi channel
    uint32_t	    	key; //virtual key code (ASCII 0..127) OR additional key code (see KEY_* defines)
    uint16_t	    	scancode; //device dependent OR touch count for multitouch devices
    uint16_t	    	pressure; //key pressure (0..1024)
    WINDOWPTR	    	win;
};

typedef void (*sundog_timer_handler)( void*, sundog_timer*, window_manager* );
struct sundog_timer
{
    sundog_timer_handler	handler;
    void*		data;
    stime_ms_t	    	deadline;
    stime_ms_t	    	delay;
    int			id;
};

enum
{
    DIALOG_ITEM_NONE = 0,
    DIALOG_ITEM_EMPTY_LINE,
    DIALOG_ITEM_NUMBER,
    DIALOG_ITEM_NUMBER_HEX,
    DIALOG_ITEM_NUMBER_FLOAT,
    DIALOG_ITEM_NUMBER_TIME, //1.0=1s;
    DIALOG_ITEM_SLIDER,
    DIALOG_ITEM_TEXT,
    DIALOG_ITEM_LABEL,
    DIALOG_ITEM_POPUP,
    DIALOG_ITEM_CHECKBOX
};

#define DIALOG_ITEM_FLAG_FOCUS				( 1 << 0 )
#define DIALOG_ITEM_FLAG_STR_AUTOREMOVE			( 1 << 1 )
#define DIALOG_ITEM_FLAG_CALL_HANDLER_ON_ANY_CHANGES	( 1 << 2 ) //action_result = 1000 + item_num
#define DIALOG_ITEM_FLAG_COLUMNS_OFFSET			3
#define DIALOG_ITEM_FLAG_COLUMNS_MASK			15
#define DIALOG_ITEM_FLAG_2COLUMNS			( 1 << DIALOG_ITEM_FLAG_COLUMNS_OFFSET )
#define DIALOG_ITEM_FLAG_3COLUMNS			( 2 << DIALOG_ITEM_FLAG_COLUMNS_OFFSET )

struct dialog_item
{
    WINDOWPTR		dwin; //dialog_handler

    int			type;
    int			min;
    int			max;
    int			int_val;
    int			normal_val;
    double		float_val;
    char*		str_val; //DIALOG_ITEM_TEXT will replace the original string (WITHOUT FREEING!)
				 //by its dynamic copy (which you must remove manually or using the DIALOG_ITEM_FLAG_STR_AUTOREMOVE flag);
    const char*		menu; //"item1\nitem2\nitem3" for DIALOG_ITEM_POPUP
                              //will be set to NULL after each init/reinit;
                              //the original string can be deleted immediately after creating the dialog;
    uint32_t		id;
    uint32_t		flags; //DIALOG_ITEM_FLAG_*

    WINDOWPTR		win;
};

#ifdef OPENGL
enum {
    GL_PROG_ATT_POSITION = 0,
    GL_PROG_ATT_COLOR,
    GL_PROG_ATT_TEX_COORD,
    GL_PROG_ATT_MAX
};
enum {
    GL_PROG_UNI_TRANSFORM1 = 0,
    GL_PROG_UNI_TRANSFORM2,
    GL_PROG_UNI_XY_ADD,
    GL_PROG_UNI_COLOR,
    GL_PROG_UNI_TEXTURE,
    GL_PROG_UNI_MAX
};
struct gl_program_struct
{
    GLuint		program;
    GLint		attributes[ GL_PROG_ATT_MAX ]; //Vertex attributes
    GLint		uniforms[ GL_PROG_UNI_MAX ]; //Global parameters for all vertex and fragment shaders
    uint		attributes_enabled; //Enabled attributes bits: ( 1 << GL_PROG_ATT_POSITION ), etc.
    int			transform_counter;
    float		xy_add;
    //Check the change of the following variables before changing GL_PROG_UNI_COLOR:
    COLOR		color;
    uint8_t		opacity;
};
#endif

enum fdialog_preview_status
{
    FPREVIEW_NONE = 0,
    FPREVIEW_OPEN,
    FPREVIEW_CLOSE,
    FPREVIEW_FILE_SELECTED,
};
struct fdialog_preview_data //Zero-filled by default (after the file dialog window creation)
{
    fdialog_preview_status status;
    char* name;
    WINDOWPTR win;
    void* user_data;
    int user_pars[ 16 ];
};

//Control type: 0 - mouse, pen, stylus; 1 - touch/multitouch screen.
#define PENCONTROL			0
#define TOUCHCONTROL			1

typedef void (*tdevice_event_handler)( window_manager* );
typedef int (*tdevice_start)( const char* name, int xsize, int ysize, window_manager* wm ); //before main loop
typedef void (*tdevice_end)( window_manager* ); //after main loop
typedef void (*tdevice_draw_line)( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm );
typedef void (*tdevice_draw_frect)( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm );
typedef void (*tdevice_draw_image)( 
    int dest_x, int dest_y,
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm );
typedef void (*tdevice_screen_lock)( WINDOWPTR win, window_manager* wm );
typedef void (*tdevice_screen_unlock)( WINDOWPTR win, window_manager* wm );
typedef void (*tdevice_vsync)( bool vsync, window_manager* wm );
typedef void (*tdevice_redraw_framebuffer)( window_manager* wm );
typedef void (*tdevice_change_name)( const char* name, window_manager* wm );

struct dstack_item //dialog stack
{
    WINDOWPTR win; //parent dialog window (not decorator)
    WINDOWPTR focus; //element in focus
};

struct sdwm_font
{
    const FONT_LINE_TYPE*	data; //font source (binary data)
    sdwm_image* 		img;

    //grid inside img:
    uint16_t			grid_xoffset;
    uint16_t			grid_yoffset;
    uint16_t 			grid_cell_xsize;
    uint16_t 			grid_cell_ysize;

    //Original char size:
    //may be != char size from the OpenGL texture;
    //final char size on the screen = char_size * wm->cur_font_scale * wm->font_zoom / 256;
    uint8_t			char_xsize[ 256 ];
    uint8_t			char_ysize;

    //Upscaled char size on the OpenGL texture:
    //may be != original char size;
    uint16_t			char_xsize2[ 256 ];
    uint16_t			char_ysize2;
};

struct window_manager
{
    sundog_engine*	sd; //Parent SunDog engine
    
    volatile bool      	initialized;
    bool		suspended : 1; //App is inactive, WM suspended
    bool		devices_suspended : 1; //Devices (sound, camera, etc.) may be suspended too in some systems (iOS)

    int			restart_request;
    int 	    	exit_request; //1 - exit; 2 - fast exit without saving state (if possible);

    char*		name;
    char*		name2;
    uint	    	flags;
    
    void*		user_data; //WM user can save some data pointer here
    
    volatile int       	events_count; //Number of events to execute
    int	        	current_event_num;
    sundog_event    	events[ WM_EVENTS ];
    smutex    		events_mutex;
    sundog_event 	frame_evt;
    sundog_event 	empty_evt; //Empty event. The following fields can be modified: win; type; time;
    uint16_t		prev_btn_key;
    uint16_t		prev_btn_scancode;
    uint16_t		prev_btn_flags;
    
#ifdef MULTITOUCH
    sundog_event	touch_evts[ WM_TOUCH_EVENTS ];
    uint		touch_evts_cnt;
    WM_TOUCH_ID		touch_id[ WM_TOUCHES ];
    bool 		touch_busy[ WM_TOUCHES ];
#endif

    sundog_keymap*	km; //Default keymap (shortcuts)

    WINDOWPTR*		trash;
    uint32_t		trash_ptr;
    uint	    	window_counter; //For ID calculation.
    WINDOWPTR	    	root_win;
    int			creg0; //Temp register for the window XY controller calculation.

    sundog_timer    	timers[ WM_TIMERS ];
    int		    	timers_num;
    int			timers_id_counter;
    
    int                 autorepeat_timer; //for EVT_BUTTONDOWN and EVT_BUTTONUP
    sundog_event        autorepeat_timer_evt;
    int			kbd_autorepeat_delay; //(ms) delay between the key press and the first repeat
    int			kbd_autorepeat_freq; //(Hz)
    int			mouse_autorepeat_delay; //(ms) delay between the click and the first repeat
    int			mouse_autorepeat_freq; //(Hz)

    int			desktop_xsize; //may be != screen_xsize; for example, if the safe area is used; initialized in desktop_handler();
    int			desktop_ysize; //...
    int             	screen_xsize;
    int             	screen_ysize;
#ifdef SCREEN_SAFE_AREA_SUPPORTED
    sdwm_rect		screen_safe_area;
#endif
    int			screen_angle; //UI rotation (degrees = acreen_angle * 90; counterclockwise (против часовой стрелки)) without native window rotation;
    bool		screen_angle_lock;
    int 		screen_zoom; //pixel size; don't use it - will may be removed soon!
    int			screen_ppi; //pixels per inch
    float		screen_scale; //UI scale factor; normal = 1.0
    float		screen_font_scale; //font scale factor; normal = 1.0
    bool		screen_buffer_preserved; //TRUE - screen buffer will be preserved after displaying the content on the screen;
						 //FALSE - can't preserve the screen content, so WM will redraw all windows on every frame;
    int		    	screen_lock_counter;
    bool            	screen_is_active;
    int			screen_changed;
    
    int			vcap; //Video capture status
    int 		vcap_in_fps;
    int 		vcap_in_bitrate_kb;
    uint		vcap_in_flags;
    char* 		vcap_in_name; //Encoding: final file name
    
    char* 		status_message;
    WINDOWPTR 		status_window;
    int 		status_timer;

    int			color_theme;
    bool		custom_colors; //Custom colors can be loaded from the config file
    COLOR	    	color0;
    COLOR		color1;
    COLOR		color2;
    COLOR	    	color3;
    bool		color3_is_brighter;
    int			color1_darkness;
    COLOR	    	yellow;
    COLOR	    	green;
    COLOR	    	red;
    COLOR	    	blue;
    COLOR		header_text_color;
    COLOR		alternative_text_color;
    COLOR	    	dialog_color;
    COLOR	    	decorator_color;
    COLOR	    	decorator_border;
    COLOR	    	button_color;
    COLOR		pressed_button_color;
    int			pressed_button_color_opacity;
    COLOR	    	menu_color;
    COLOR	    	selection_color;
    COLOR	    	text_background;
    COLOR	    	list_background;
    COLOR	    	scroll_color;
    COLOR	    	scroll_background_color;
    COLOR		scroll_pressed_color;
    int			scroll_pressed_color_opacity;

    sdwm_image*	texture0;

    int			control_type;
    int			double_click_time; //ms
    bool		show_virtual_keyboard;
    bool		show_zoom_buttons;
    int                 max_fps; //Max FPS, provided by window manager. Less value - less CPU usage.
    bool		frame_event_request;
    
    int			normal_window_xsize;
    int			normal_window_ysize;
    int			large_window_xsize;
    int			large_window_ysize;
    int		    	decor_border_size;
    int		    	decor_header_size;
    int		    	scrollbar_size; //Scrollbar, divider, normal button
    int			small_button_xsize; //Smallest width of the button; at least one char of the biggest font
    int		    	button_xsize; //Big button
    int		    	button_ysize; //Big button
    int			popup_button_ysize; //Button with popup menu and visible current value; at least two lines of text
    int			controller_ysize; //Two text lines with the smallest font
    int			interelement_space;
    int			interelement_space2; //Smallest
    int			list_item_ysize;
    int			text_ysize; //Width of the text editor or the small button with text
    int			text_with_label_ysize;
    int			corners_size;
    int			corners_len;

    // WBD (Window Buffer Draw):
    WINDOWPTR		cur_window;
    bool		cur_window_invisible;
    COLOR*		screen_pixels; //Pixel screen buffer for direct read/write
    COLOR           	cur_font_color;
    int		    	cur_font_scale; //256 - normal; 512 - x2
    uint8_t		cur_opacity;
    uint		cur_flags;
    void*		points_array;
    sdwm_vertex 	poly_vertices1[ 8 ];
    sdwm_vertex 	poly_vertices2[ 8 ];

    sdwm_font		fonts[ WM_FONTS ];
    int			font_zoom; //Global font zoom: 1.0 = 256
    int8_t		default_font; //by default = font_big
    int8_t		font_medium_mono; //pat.editor
    int8_t		font_big; //main
    int8_t		font_small; //controllers and other similar two-line (label+value) UI elements
    //int8_t		font_smallest; //small labels

    int		    	pen_x;
    int		    	pen_y;
    WINDOWPTR	    	focus_win;
    WINDOWPTR	    	prev_focus_win;
    uint16_t	    	focus_win_id;
    uint16_t	    	prev_focus_win_id;
    WINDOWPTR	    	last_unfocused_window;

    dstack_item*	dstack; //dialog stack: some app win, dialog1, dialog2, ...

    WINDOWPTR	    	handler_of_unhandled_events;

    //Dialog windows data:
    int 		fdialog_cnt;
    char* 		fdialog_filename;
    char* 		fdialog_copy_file_name;  // 1:/dir/file.txt
    char* 		fdialog_copy_file_name2; // file.txt
    bool 		fdialog_cut_file_flag;
    bool		fdialog_deldir_confirm;
    bool		fdialog_delfile_confirm;
    WINDOWPTR 		prefs_win;
    const char* 	prefs_section_names[ 32 ];
    int 		prefs_sections;
    void* 		prefs_section_handlers[ 32 ];
    int			prefs_section_ysize;
    bool 		prefs_restart_request;
    uint		prefs_flags;
    WINDOWPTR		colortheme_win;
    WINDOWPTR		ui_scale_win;
    WINDOWPTR		keymap_win;
    WINDOWPTR		vk_win; //virtual keyboard
    
    //Window creation options:
    int 			opt_divider_scroll_min;
    int 			opt_divider_scroll_max;
    bool 			opt_divider_vertical : 1;
    bool 			opt_text_ro : 1; //Read Only
    int 			opt_text_numeric;
    int 			opt_text_num_min;
    int 			opt_text_num_max;
    bool 			opt_text_num_hide_zero : 1;
    uint32_t 			opt_button_flags;
    sdwm_image_scaled        	opt_button_image1;
    sdwm_image_scaled        	opt_button_image2;
    bool 			opt_scrollbar_vertical : 1;
    bool 			opt_scrollbar_reverse : 1;
    bool 			opt_scrollbar_compact : 1;
    bool 			opt_scrollbar_flat : 1;
    int 			(*opt_scrollbar_begin_handler)( void*, WINDOWPTR, int );
    int 			(*opt_scrollbar_end_handler)( void*, WINDOWPTR, int );
    int 			(*opt_scrollbar_opt_handler)( void*, WINDOWPTR, int ); //"options" (right mouse button for example)
    void* 			opt_scrollbar_begin_handler_data;
    void* 			opt_scrollbar_end_handler_data;
    void* 			opt_scrollbar_opt_handler_data;
    dialog_item* 		opt_dialog_items;
    const char* 		opt_dialog_buttons_text;
    const char* 		opt_dialog_text;
    const char* 		opt_fdialog_id; //File dialog ID (string, will be used as part of the properties file name)
    const char* 		opt_fdialog_mask; //Example: "xm/mod/it" (or NULL for all files)
    const char* 		opt_fdialog_preview;
    int 			(*opt_fdialog_preview_handler)( fdialog_preview_data*, window_manager* );
    void* 			opt_fdialog_preview_user_data;
    const char* 		opt_fdialog_user_button[ 4 ];
    int 			(*opt_fdialog_user_button_handler[ 4 ])( void* user_data, WINDOWPTR win, window_manager* );
    void* 			opt_fdialog_user_button_data[ 4 ];
    const char* 		opt_fdialog_def_filename;
    uint			opt_fdialog_flags;
    bool 			opt_list_numbered : 1;
    bool 			opt_list_without_scrollbar : 1;
    bool 			opt_list_without_extensions : 1;
    const char*			opt_popup_text;
    
    COLORPTR		fb; //Pixel buffer for direct read/write
    int		    	fb_xpitch; //framebuffer xpitch
    int		    	fb_ypitch; //...
    int		    	fb_offset; //...
    int			fb_xsize;
    int			fb_ysize;
    int		    	real_window_width;
    int		    	real_window_height;
#ifdef SCREEN_SAFE_AREA_SUPPORTED
    sdwm_rect		real_window_safe_area;
#endif

    //DEVICE DEPENDENT PART:

    //Device init/deinit:
    tdevice_event_handler	device_event_handler;
    tdevice_start		device_start; //before main loop
    tdevice_end			device_end; //after main loop
    //Device window drawing functions:
    tdevice_draw_line		device_draw_line;
    tdevice_draw_frect		device_draw_frect;
    tdevice_draw_image		device_draw_image;
    //...
    tdevice_screen_lock		device_screen_lock;
    tdevice_screen_unlock	device_screen_unlock;
    tdevice_vsync		device_vsync;
    tdevice_redraw_framebuffer	device_redraw_framebuffer;
    tdevice_change_name		device_change_name;

    uint		mods; //Modifier flags (EVT_FLAG_SHIFT, EVT_FLAG_CTRL, ...)
    uint 		frame_len; //in ms
    stime_ticks_t 	ticks_per_frame;
    stime_ticks_t 	prev_frame_time;
    
    void*		dd; //some private device depentent data

#ifdef OPENGL
    bool		gl_initialized;
    smutex        	gl_mutex;
    float		gl_xscale;
    float		gl_yscale;
    int			gl_transform_counter;
    float		gl_projection_matrix[ 4 * 4 ];
    uint		gl_current_texture;
    gl_program_struct*	gl_current_prog;
    gl_program_struct*	gl_prog_solid;
    gl_program_struct*	gl_prog_gradient;
    gl_program_struct*	gl_prog_tex_alpha; //texture: rgb = 1.0; a = ...;
    gl_program_struct*	gl_prog_tex_rgb; //texture: rgb = ...; a = 1.0;
    void*		gl_points_array;
    bool		gl_no_points; //no GL_POINTS
    int16_t 		gl_array_s[ 16 ];
    uint8_t 		gl_array_c[ 16 ];
    float 		gl_array_f[ 16 ];
#endif //OPENGL

#ifdef GDI
    uint           	gdi_bitmap_info[ 512 ]; //GDI bitmap info (for win32).
#endif //GDI

#ifdef OS_WIN
    HDC	            	hdc; //Graphics content handler.
    HWND	    	hwnd;
    bool		ignore_mouse_evts;
    stime_ms_t		ignore_mouse_evts_before;
#ifdef OPENGL
    HGLRC 		hGLRC;
    BOOL 		(WINAPI *wglSwapIntervalEXT)( int interval );
#endif //OPENGL
#ifdef DIRECTDRAW
    LPDIRECTDRAW 	lpDD; // DirectDraw object
    LPDIRECTDRAWSURFACE	lpDDSPrimary; //DirectDraw primary surface
    LPDIRECTDRAWSURFACE	lpDDSBack; //DirectDraw back surface
    LPDIRECTDRAWSURFACE	lpDDSOne;
    DDSURFACEDESC 	dd_sd;
    bool		dd_primary_locked;
#endif //DIRECTDRAW
#endif //OS_WIN

#ifdef OS_WINCE
    int 		hires;
    int			vd; //video-driver
    char		rfb[ 32 ]; //raw framebuffer info
    HDC             	hdc; //graphics content handler
    HWND	    	hwnd;
    int		    	os_version;
    int		    	gx_suspended;
    int			raw_fb_xpitch;
    int			raw_fb_ypitch;
    void* 		wince_zoom_buffer;
#endif //OS_WINCE

#ifdef OS_MACOS
    uint		mods_original;
#endif

#ifdef OS_UNIX
#ifdef X11
    Display*		dpy;
    Colormap	    	cmap;
    int 		auto_repeat;
    Atom		xatom_wmstate; //xatom = property identifier
    Atom		xatom_wmstate_fullscreen;
    Atom		xatom_wmstate_maximized_v;
    Atom		xatom_wmstate_maximized_h;
    Atom 		xatom_delete_window;
    int			paste_lock;
    pthread_t		main_loop_thread;
    Window          	win;
    GC              	win_gc;
    XImage*		win_img;
    XImage*		win_img_back_pattern;
    char*		win_img_back_pattern_data;
    int 	    	win_img_depth;
    char*		win_buffer;
    int	            	win_depth;
#ifdef MULTITOUCH
    bool		xi; //X Input Extension
    int			xi_opcode;
    int			xi_event;
    int			xi_error;
#endif
#ifndef OPENGL
    Visual*		win_visual;
    uint8_t* 		temp_bitmap;
    size_t		temp_bitmap_size;
    XImage*		last_ximg;
#endif //X11 + !OPENGL
#if defined(OPENGL) && !defined(OPENGLES)
    GLXContext 		gl_context;
    XVisualInfo*	win_visual;
    int 		(*glXSwapIntervalEXT)( Display* dpy, GLXDrawable drw, int interval );
    void		(*glXSwapIntervalMESA)( GLint );
    void		(*glXSwapIntervalSGI)( GLint );
#endif //OPENGL + !OPENGLES
#ifdef OPENGLES
    EGLDisplay 		egl_display;
    EGLContext 		egl_context;
    EGLSurface 		egl_surface;
#endif //OPENGLES
#endif //X11
#endif //OS_UNIX
};
