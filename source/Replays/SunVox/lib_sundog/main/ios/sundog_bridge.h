#pragma once

#ifdef __OBJC__

//
// ObjC
//

#import <UIKit/UIKit.h>
#ifdef AUDIOUNIT_EXTENSION
#import <CoreAudioKit/CoreAudioKit.h>
#endif

#ifdef __cplusplus
    #define C_EXTERN extern "C"
#else
    #define C_EXTERN extern
#endif

struct ios_sundog_engine;

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow* window;
@end

@interface MainViewController : UIViewController
- (void)setSunDog:(ios_sundog_engine*)s; //connect already created controller to the SunDog app
@end

@interface MyView : UIView <UIDocumentPickerDelegate,UIDocumentInteractionControllerDelegate>

//Keyboard:
@property (retain) NSArray* keys;

- (void)setSunDog:(ios_sundog_engine*)s; //connect already created view to the SunDog app
- (int)viewInit;
- (void)viewDeinit;
- (void)redrawAll;
- (void)showBluetoothMIDICentral;
- (int)showExportImport:(uint32_t)flags withFile:(const char*)filename;

@end //MyView

#ifdef AUDIOUNIT_EXTENSION
@interface AudioUnitViewController : AUViewController <AUAudioUnitFactory>
- (sundog_engine*)auInit:(AVAudioFormat*)format;
- (void)auDeinit;
- (int)auGetChannelCount;
- (int)auGetSampleRate;
- (void)auSetFullState:(sundog_state*)state;
- (sundog_state*)auGetFullState;
@end
#endif

#endif // __OBJC__

//
// ObjC / C / C++
//

extern char g_ui_lang[];

void ios_sundog_gl_lock( sundog_engine* s );
void ios_sundog_gl_unlock( sundog_engine* s );
void ios_sundog_screen_redraw( sundog_engine* s );
void ios_sundog_event_handler( window_manager* wm );
int ios_sundog_copy( sundog_engine* s, const char* filename, uint32_t flags );
char* ios_sundog_paste( sundog_engine* s, int type, uint32_t flags );
void ios_sundog_open_url( sundog_engine* s, const char* url );
void ios_sundog_send_text_to_email( sundog_engine* s, const char* email_text, const char* subj_text, const char* body_text );
void ios_sundog_send_file_to_gallery( sundog_engine* s, const char* file_path );
void ios_sundog_bluetooth_midi_settings( sundog_engine* s );
int ios_sundog_export_import_file( sundog_engine* s, const char* filename, uint32_t flags );

#ifdef AUDIOUNIT_EXTENSION
struct au_midi_out_event
{
    uint8_t*            data;
    int16_t             size;
    int8_t              port;
    int                 n; //sorting: evt = events[ events[ i ].n ]
    int                 t; //offset (in frames)
};
#endif
