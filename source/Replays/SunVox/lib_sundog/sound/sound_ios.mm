/*
    sound_ios.cpp - iOS: audio session, JACK, audiobus
    This file is part of the SunDog engine.
    Copyright (C) 2013 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

//Modularity: 0% (one audiobus session per app)

#import <UIKit/UIKit.h>

#include "sundog.h"
#include "sound.h"
#include "sound_ios.hpp"

//
// Possible options (defines): 
//   JACK_AUDIO;
//   AUDIOBUS;
//   AUDIOBUS_API_KEY;
//   AUDIOBUS_SENDER;
//   AUDIOBUS_FILTER;
//   AUDIOBUS_MIDI_IN;
//   AUDIOBUS_MIDI_OUT;
//   AUDIOBUS_TRIGGER_PLAY;
//   AUDIOBUS_TRIGGER_REWIND;
//

//
// AVAudioSession
//

#import <AVFoundation/AVAudioSession.h>

int audio_session_init( bool record, int preferred_sample_rate, int preferred_buffer_size )
{
    int rv = -1;

    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];

    printf( "audio_session_init( %d, %d, %d )\n", record, preferred_sample_rate, preferred_buffer_size );
    while( 1 )
    {
	NSError* error = nil;
	NSString* category = AVAudioSessionCategoryPlayback;
	NSString* mode = NULL;
	if( record )
	    category = AVAudioSessionCategoryPlayAndRecord;
	if( sconfig_get_int_value( "audio_mode_measurement", 0, NULL ) == 1 )
	{
	    slog( "AVAudioSessionModeMeasurement\n" );
	    mode = AVAudioSessionModeMeasurement;
	}
	AVAudioSessionCategoryOptions options = AVAudioSessionCategoryOptionMixWithOthers;
	if( ![ [ AVAudioSession sharedInstance ] setCategory:category withOptions:options error:&error ] )
	{
	    slog( "Can't set audio session category: %s\n", [ [ error localizedDescription ] UTF8String ] );
	    if( record )
	    {
		error = nil;
		category = AVAudioSessionCategoryPlayback;
		if( ![ [ AVAudioSession sharedInstance ] setCategory:category withOptions:options error:&error ] ) 
		{
		    slog( "Can't set audio session category: %s\n", [ [ error localizedDescription ] UTF8String ] );
		    break;
		}
	    }
	    else
	    {
		break;
	    }
	}
	if( mode )
	{
	    if( ![ [ AVAudioSession sharedInstance ] setMode:mode error:&error ] )
	    {
		slog( "Can't set audio session mode %s. Error: %s\n", [ mode UTF8String ], [ [ error localizedDescription ] UTF8String ] );
	    }
	}

	if( preferred_sample_rate > 0 )
	{
	    error = nil;
	    if( ![ [ AVAudioSession sharedInstance ] setPreferredSampleRate:preferred_sample_rate error:&error ] )
	    {
		slog( "Can't set audio session sample rate: %s\n", [ [ error localizedDescription ] UTF8String ] );
	    }
	}
	
	if( preferred_buffer_size > 0 )
	{
	    error = nil;
	    if( ![ [ AVAudioSession sharedInstance ] setPreferredIOBufferDuration: ( (double)preferred_buffer_size / [ AVAudioSession sharedInstance ].preferredSampleRate ) error:&error ] )
	    {
		slog( "Can't set audio session buffer size: %s\n", [ [ error localizedDescription ] UTF8String ] );
	    }
	}

	error = nil;
	if( ![ [ AVAudioSession sharedInstance ] setActive:YES error:&error ] ) 
	{
	    slog( "Can't activate audio session: %s\n", [ [ error localizedDescription ] UTF8String ] );
	    break;
	}

	rv = 0;
	break;
    }

    [ pool release ];
    
    return rv;
}

int audio_session_deinit( void )
{
    int rv = -1;

    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];

    while( 1 )
    {
	NSError* error = nil;
	if( ![ [ AVAudioSession sharedInstance ] setActive:NO error:&error ] ) 
	{
	    slog( "Can't deactivate audio session: %s\n", [ [ error localizedDescription ] UTF8String ] );
	    break;
	}
	
	rv = 0;
	break;
    }

    [ pool release ];
    
    return rv;
}

int get_audio_session_freq()
{
    int rv = -1;
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    rv = [ AVAudioSession sharedInstance ].sampleRate;
    [ pool release ];
    return rv;
}

//
// JACK
//

#ifdef JACK_AUDIO

#import <jack/custom.h>

void jack_publish_client_icon( jack_client_t* client )
{
    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];
    NSString* iconFile = [ [ NSBundle mainBundle ] pathForResource:@"icon@2x" ofType:@"png" ];
    NSFileHandle* fileHandle = [ NSFileHandle fileHandleForReadingAtPath:iconFile ];
    NSData* data = [ fileHandle readDataToEndOfFile ];
    const void* rawData = [ data bytes ];
    const size_t size   = [ data length ];
    jack_custom_publish_data( client, "icon.png", rawData, size );
    [ fileHandle closeFile ];
    [ pool release ];
}

#endif

//
// Audiobus
//

#import <AudioUnit/AudioUnit.h>
#ifdef AUDIOBUS
    bool g_no_audiobus = false;
#else
    bool g_no_audiobus = true;
#endif
AudioComponentInstance g_au = NULL; //Main Audio Unit; it will be used for Audiobus or for the pure RemoteIO unit;
bool g_au_inuse = false;
//note that the SunDog sound engine can create the additional audio units too, but they will not be compatible with Audiobus

#ifdef AUDIOBUS

#import "Audiobus/Audiobus.h"

ABAudiobusController* g_ab_controller = NULL;
ABAudioSenderPort* g_ab_sender = NULL;
ABAudioFilterPort* g_ab_filter = NULL;
#ifdef AUDIOBUS_MIDI_IN
    ABMIDIReceiverPort* g_ab_midi_in = NULL; //main input MIDI port; will be connected to the "public port"
    volatile bool g_ab_disable_coremidi_in = false;
    struct ab_midi_in_callback { osx_midiin_callback_t f; device_midi_port* port; };
    #define AUDIOBUS_MIDI_IN_CALLBACKS 4
    ab_midi_in_callback g_ab_midi_in_cb[ AUDIOBUS_MIDI_IN_CALLBACKS ];
    volatile int g_ab_midi_in_cbn = 0;
    volatile uint32_t g_ab_midi_in_state = 0;
#endif
#ifdef AUDIOBUS_MIDI_OUT
    ABMIDISenderPort* g_ab_midi_out[ AUDIOBUS_MIDI_OUT ];
    const char* g_ab_midi_out_names[] = { "MIDI Output 1", "MIDI Output 2", "MIDI Output 3", "MIDI Output 4" }; //for Audiobus
    const char* g_ab_midi_out_names2[] = { "Audiobus MIDI 1", "Audiobus MIDI 2", "Audiobus MIDI 3", "Audiobus MIDI 4" }; //for SunDog
    volatile bool g_ab_disable_coremidi_out = false;
#endif
ABTrigger* g_ab_trigger_play = 0;
ABTrigger* g_ab_trigger_rewind = 0;
bool g_ab_connected = false;
void* kAudiobusConnectedChanged = &kAudiobusConnectedChanged;

int audiobus_init( void* observer ) //Called from sundog_bridge.m (main iOS thread)
{
    if( @available( iOS 14.0, * ) )
    {
	if( NSProcessInfo.processInfo.isiOSAppOnMac )
	{
	    //Can't use Audiobus on macOS?
	    g_no_audiobus = true;
	    slog( "NO AUDIOBUS\n" );
	}
    }
    if( g_no_audiobus ) return 0;

    slog( "audiobus_init(): session init...\n" );
    audio_session_init( false, 0, 0 );

    //Create new RemoteIO Audio Unit:
    slog( "audiobus_init(): Audio Unit creation...\n" );
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext( NULL, &desc );
    if( comp )
	AudioComponentInstanceNew( comp, &g_au );
    else
    {
	slog( "audiobus_init(): can't create the new Audio Unit\n" );
	return -1;
    }
    //Audio Unit will be initialized in the SunDog thread

    slog( "audiobus_init(): AB controller init...\n" );
    g_ab_controller = [ [ ABAudiobusController alloc ] initWithApiKey:@AUDIOBUS_API_KEY ];
    if( g_ab_controller == NULL )
    {
	slog( "Can't init ABAudiobusController\n" );
	return -2;
    }

    slog( "audiobus_init(): AB audio ports init...\n" );
#ifdef AUDIOBUS_SENDER
    g_ab_sender = [ [ ABAudioSenderPort alloc ] initWithName:@"Output" title:@"Output"
	audioComponentDescription:( AudioComponentDescription ) {
    	    .componentType = kAudioUnitType_RemoteGenerator,
            .componentSubType = AUDIOBUS_SENDER_SUBTYPE, // Note single quotes
            .componentManufacturer = 'nira' }
        audioUnit:g_au ];
    if( g_ab_sender == NULL )
        slog( "Can't create AB sender port\n" );
    else
        [ g_ab_controller addAudioSenderPort:g_ab_sender ];
#endif
#ifdef AUDIOBUS_FILTER
    g_ab_filter = [ [ ABAudioFilterPort alloc ] initWithName:@"Filter" title:@"Filter"
        audioComponentDescription:(AudioComponentDescription) {
    	    .componentType = kAudioUnitType_RemoteEffect,
	    .componentSubType = AUDIOBUS_FILTER_SUBTYPE,
	    .componentManufacturer = 'nira' }
        audioUnit:g_au ];
    if( g_ab_filter == NULL )
        slog( "Can't create AB filter port\n" );
    else
        [ g_ab_controller addAudioFilterPort:g_ab_filter ];
#endif

    slog( "audiobus_init(): AB midi ports init...\n" );
#ifdef AUDIOBUS_MIDI_IN
    smem_clear( &g_ab_midi_in_cb, sizeof( g_ab_midi_in_cb ) );
    g_ab_midi_in = [ [ ABMIDIReceiverPort alloc ] initWithName:@"MIDI Input" title:@"MIDI Input"
	receiverBlock:^( __unsafe_unretained ABPort * receiverPort, const MIDIPacketList * packetList ) {
	    // Process the received MIDI events:
	    if( g_ab_midi_in_cbn > 0 )
	    {
		g_ab_midi_in_state++;
		COMPILER_MEMORY_BARRIER();
		for( int i = 0; i < AUDIOBUS_MIDI_IN_CALLBACKS; i++ )
		{
		    osx_midiin_callback_t f = g_ab_midi_in_cb[ i ].f;
		    device_midi_port* port = g_ab_midi_in_cb[ i ].port;
		    if( f && port )
		    {
			f( port, packetList );
		    }
		}
		COMPILER_MEMORY_BARRIER();
		g_ab_midi_in_state++;
	    }
	} ];
    [ g_ab_controller addMIDIReceiverPort:g_ab_midi_in ];
    g_ab_controller.enableReceivingCoreMIDIBlock = ^( BOOL receivingEnabled ) {
	if( receivingEnabled ) {
	    // Core MIDI RECEIVING needs to be enabled:
	    g_ab_disable_coremidi_in = false;
	} else {
	    // Core MIDI RECEIVING needs to be disabled:
	    g_ab_disable_coremidi_in = true;
	}
    };
#endif
#ifdef AUDIOBUS_MIDI_OUT
    for( int i = 0; i < AUDIOBUS_MIDI_OUT; i++ )
    {
	NSString* midi_out_name = [ NSString stringWithUTF8String:g_ab_midi_out_names[ i ] ];
	g_ab_midi_out[ i ] = [ [ ABMIDISenderPort alloc ] initWithName:midi_out_name title:midi_out_name ];
	[ g_ab_controller addMIDISenderPort:g_ab_midi_out[ i ] ];
    }
    g_ab_controller.enableSendingCoreMIDIBlock = ^( BOOL sendingEnabled ) {
	if( sendingEnabled ) {
	    // Core MIDI SENDING needs to be enabled:
	    g_ab_disable_coremidi_out = false;
	} else {
	    // Core MIDI SENDING needs to be disabled:
	    g_ab_disable_coremidi_out = true;
	}
    };
#endif
    
    slog( "audiobus_init(): AB triggers init...\n" );
#ifdef AUDIOBUS_TRIGGER_PLAY
    g_ab_trigger_play = [ ABTrigger triggerWithSystemType:ABTriggerTypePlayToggle block:^( ABTrigger* trigger, NSSet* ports )
    {
        if( g_snd_play_status )
	{
            g_snd_stop_request++;
	    trigger.state = ABTriggerStateNormal;
	}
        else
	{
            g_snd_play_request++;
	    trigger.state = ABTriggerStateSelected;
	}
    } ];
    [ g_ab_controller addTrigger:g_ab_trigger_play ];
#endif
#ifdef AUDIOBUS_TRIGGER_REWIND
    g_ab_trigger_rewind = [ ABTrigger triggerWithSystemType:ABTriggerTypeRewind block:^( ABTrigger* trigger, NSSet* ports )
    {
        g_snd_rewind_request++;
    } ];
    [ g_ab_controller addTrigger:g_ab_trigger_rewind ];
#endif
    
    g_ab_connected = g_ab_controller.connected;
    [ g_ab_controller addObserver:(NSObject*)observer forKeyPath:@"connected" options:0 context:kAudiobusConnectedChanged ];

    slog( "audiobus_init(): ok.\n" );
    return 0;
}

void audiobus_deinit( void ) //Called from sundog_bridge.m (main thread)
{
    if( g_no_audiobus ) return;

    slog( "audiobus_deinit(): AB controller deinit...\n" );
    //[ g_ab_controller release ];
    g_ab_controller = 0;
    g_ab_sender = 0;
    g_ab_filter = 0;
#ifdef AUDIOBUS_MIDI_IN
    g_ab_midi_in = 0;
    g_ab_midi_in_cbn = 0;
    smem_clear( g_ab_midi_in_cb, sizeof( g_ab_midi_in_cb ) );
#endif
#ifdef AUDIOBUS_MIDI_OUT
    for( int i = 0; i < AUDIOBUS_MIDI_OUT; i++ )
    {
	g_ab_midi_out[ i ] = 0;
    }
#endif

    slog( "audiobus_deinit(): Audio Unit deinit...\n" );
    AudioComponentInstanceDispose( g_au );
    g_au = NULL;

    slog( "audiobus_deinit(): audio session deinit...\n" );
    audio_session_deinit();

    slog( "audiobus_deinit(): ok.\n" );
}

void audiobus_enable_ports( bool enable )
{
    if( g_no_audiobus ) return;
    if( g_ab_controller == NULL ) return;

    NSAutoreleasePool* pool = [ [ NSAutoreleasePool alloc ] init ];

#ifdef AUDIOBUS_SENDER
    if( g_ab_sender )
    {
	if( enable )
	    g_ab_sender.audioUnit = g_au;
	else
	    g_ab_sender.audioUnit = NULL;
    }
#endif

#ifdef AUDIOBUS_FILTER
    if( g_ab_filter )
    {
	if( enable )
	    g_ab_filter.audioUnit = g_au;
	else
	    g_ab_filter.audioUnit = NULL;
    }
#endif

    [ pool release ];
}

void audiobus_connection_state_update( sundog_engine* sd ) //called from sundog_bridge.mm -> view -> observeValueForKeyPath:
{
    if( g_no_audiobus ) return;
    if( g_ab_controller == NULL ) return;
    if( g_au == NULL ) return;

    g_ab_connected = g_ab_controller.connected;
    printf( "AB Connection: %d\n", (int) g_ab_connected );

    if( sd == NULL ) return;
    if( sd->ss == NULL ) return;
    if( !g_au_inuse ) return;

    if( [UIApplication sharedApplication].applicationState == UIApplicationStateBackground && !g_ab_connected )
    {
	// Audiobus session is finished. Time to sleep?
    }
    else if( g_ab_connected )
    {
	// Make sure we're running
	NSError* error = nil;
	[ [ AVAudioSession sharedInstance ] setActive:YES error:&error ];
	AudioOutputUnitStart( g_au );
    }
}

void audiobus_triggers_update( void )
{
    if( g_no_audiobus ) return;
#ifdef AUDIOBUS_TRIGGER_PLAY
    if( g_ab_controller )
    {
        if( g_snd_play_status )
        {
            if( g_ab_trigger_play.state != ABTriggerStateSelected )
            {
		dispatch_async( dispatch_get_main_queue(), ^{ g_ab_trigger_play.state = ABTriggerStateSelected; } );
            }
        }
        else
        {
            if( g_ab_trigger_play.state != ABTriggerStateNormal )
            {
		dispatch_async( dispatch_get_main_queue(), ^{ g_ab_trigger_play.state = ABTriggerStateNormal; } );
            }
        }
    }
#endif
}

#ifdef AUDIOBUS_MIDI_IN

int audiobus_add_midiin_callback( osx_midiin_callback_t f, device_midi_port* port )
{
    if( g_no_audiobus ) return 0;
    if( g_ab_controller == 0 ) return -10;
    if( g_ab_midi_in == 0 ) return -11;
    int i;
    for( i = 0; i < AUDIOBUS_MIDI_IN_CALLBACKS; i++ )
    {
	if( g_ab_midi_in_cb[ i ].f == f && g_ab_midi_in_cb[ i ].port == port )
	{
	    //Already exists:
	    return -1;
	}
    }
    for( i = 0; i < AUDIOBUS_MIDI_IN_CALLBACKS; i++ )
    {
	if( g_ab_midi_in_cb[ i ].port == 0 ) break;
    }
    if( i >= AUDIOBUS_MIDI_IN_CALLBACKS )
    {
	//No free slots:
	return -2;
    }
    g_ab_midi_in_cb[ i ].f = f;
    COMPILER_MEMORY_BARRIER();
    g_ab_midi_in_cb[ i ].port = port;
    g_ab_midi_in_cbn++;
    printf( "audiobus midi in callback added: %d %d\n", i, g_ab_midi_in_cbn );
    return 0;
}

bool audiobus_remove_midiin_callback( osx_midiin_callback_t f, device_midi_port* port )
{
    bool rv = false;
    if( g_no_audiobus ) return rv;
    if( g_ab_controller == 0 ) return false;
    if( g_ab_midi_in == 0 ) return false;
    for( int i = 0; i < AUDIOBUS_MIDI_IN_CALLBACKS; i++ )
    {
	if( g_ab_midi_in_cb[ i ].f == f && g_ab_midi_in_cb[ i ].port == port )
	{
	    g_ab_midi_in_cb[ i ].port = NULL;
	    g_ab_midi_in_cb[ i ].f = NULL;
	    g_ab_midi_in_cbn--;
	    rv = true;
	    printf( "audiobus midi in callback removed: %d %d\n", i, g_ab_midi_in_cbn );
	    break;
	}
    }
    if( rv )
    {
	uint32_t state = g_ab_midi_in_state;
	if( state & 1 )
	{
	    //MIDI IN callback state is active
	    STIME_WAIT_FOR( state != g_ab_midi_in_state, 50, 1, );
    	}
    }
    return rv;
}

#endif //...AUDIOBUS_MIDI_IN

#ifdef AUDIOBUS_MIDI_OUT

void audiobus_midiout( int out_port, const MIDIPacketList* plist ) //out_port: 1,2,3...; 0 - none
{
    if( g_no_audiobus ) return;
    if( g_ab_controller == 0 ) return;
    if( out_port <= 0 ) return;
    out_port--;
    if( out_port >= AUDIOBUS_MIDI_OUT ) return;
    ABMIDISenderPort* port = g_ab_midi_out[ out_port ];
    if( port == 0 ) return;
    ABMIDIPortSendPacketList( port, plist );
}

#endif //...AUDIOBUS_MIDI_OUT

#endif //...AUDIOBUS
