#pragma once

#ifdef __cplusplus
#define C_EXTERN extern "C"
#else
#define C_EXTERN extern
#endif

#ifdef __OBJC__

struct macos_sundog_engine;

@interface AppDelegate : NSApplication
{
    NSView* subview;
    IBOutlet NSWindow* window;
    IBOutlet NSView* view;
}
@end

@interface MyView : NSOpenGLView
{
    NSOpenGLContext* context;
}
- (void)viewBoundsChanged: (NSNotification*)aNotification;
- (void)viewFrameChanged: (NSNotification*)aNotification;
- (int)viewInit;
@end

#endif

void macos_sundog_screen_redraw( void );
void macos_sundog_rename_window( const char* name );
void macos_sundog_event_handler( window_manager* wm );
int macos_sundog_copy( sundog_engine* s, const char* filename, uint32_t flags );
char* macos_sundog_paste( sundog_engine* s, int type, uint32_t flags );

extern int g_macos_sundog_screen_xsize;
extern int g_macos_sundog_screen_ysize;
extern int g_macos_sundog_screen_ppi;
extern int g_macos_sundog_resize_request;
extern void* g_gl_context_obj;

extern char g_ui_lang[];
