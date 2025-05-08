///\file
///\brief implementation file for psycle::host::CMidiInput.
/// original code 21st April by Mark McCormack (mark_jj_mccormak@yahoo.co.uk) for Psycle - v2.2b -virtually complete-
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"

#if defined DIVERSALIS__COMPILER__MICROSOFT
	#pragma warning(push)
	#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#endif

	#include <mmsystem.h>
	#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
		#pragma comment(lib, "winmm")
	#endif

#if defined DIVERSALIS__COMPILER__MICROSOFT
	#pragma warning(pop)
#endif

namespace psycle
{
	namespace host
	{
		#define MAX_MIDI_CHANNELS 16
		#define MAX_CONTROLLERS   127
		#define MAX_PARAMETERS    127

		///\todo might want to make these two user-configurable rather than hard-coded values.
		#define MIDI_BUFFER_SIZE 1024
		#define MIDI_PREDELAY_MS 200

		#define VERSION_STRING "v2.3"

		///\name FLAGS
		///\{
			/// are we waiting to inject MIDI?
			#define FSTAT_ACTIVE                0x0001
			/// the three MIDI sync codes
			#define FSTAT_FASTART               0x0002
			/// the three MIDI sync codes
			#define FSTAT_F8CLOCK               0x0004
			/// the three MIDI sync codes
			#define FSTAT_FCSTOP                0x0008

			/// Controller 121 - emulated FASTART
			#define FSTAT_EMULATED_FASTART      0x0010
			/// Controller 122 - emulated F8CLOCK
			#define FSTAT_EMULATED_F8CLOCK      0x0020
			/// Controller 124 - emulated FCSTOP
			#define FSTAT_EMULATED_FCSTOP       0x0040
			/// midi buffer overflow?
			#define FSTAT_BUFFER_OVERFLOW       0x0080

			/// midi data going into the buffer?
			#define FSTAT_IN_BUFFER             0x0100
			/// midi data going out of the buffer?
			#define FSTAT_OUT_BUFFER            0x0200
			/// midi data being cleared from the buffer?
			#define FSTAT_CLEAR_BUFFER          0x0400
			/// simulated tracker tick?
			#define FSTAT_SYNC_TICK             0x0800

			/// syncing with the audio?
			#define FSTAT_SYNC                  0x1000
			/// just resynced with the audio?
			#define FSTAT_RESYNC                0x2000
			/// we are receiving MIDI input data?
			#define FSTAT_MIDI_INPUT            0x4000

			/// to clear correct flags
			#define FSTAT_CLEAR_WHEN_READ       0x0001
		///\}

		class MIDI_BUFFER
		{
		public:
			MIDI_BUFFER() : timeStamp(0), channel(0), orignote(255) {};
			/// tracker pattern info struct
			PatternEntry entry;
			/// MIDI input device's timestamp
			uint32_t timeStamp;
			/// MIDI channel
			int channel;
			/// original note, if this is a noteoff
			int orignote;
		};

		class MIDI_CONFIG
		{
		public:
			/// version string (e.g. v2.0b)
			char versionStr[ 16 ];
			/// milliseconds allowed for MIDI buffer fill
			int midiHeadroom;
		};

		class MIDI_STATS
		{
		public:
			/// amount of MIDI messages currently in the buffer (events)
			int bufferCount;
			/// capacity of the MIDI message buffer (events)
			int bufferSize;
			/// how many events have been lost and not played? (events)
			int eventsLost;
			/// clock deviation between the PC clock and the MIDI clock on last clock (ms)
			int clockDeviation;
			/// last sync adjustment value (samples)
			int syncAdjuster;
			/// how far off the audio engine is from the MIDI (ms)
			int syncOffset;
			/// bitmapped channel active map	(CLEAR AFTER READ)
			unsigned int channelMap;
			/// strobe for the channel map list
			bool channelMapUpdate;
			/// 32 bits of boolean info (see FLAGS, CLEAR AFTER READ)
			uint32_t flags;
		};

		enum MODES
		{
			MODE_REALTIME, ///< use Psycle as a cool softsynth via MIDI
			MODE_STEP,     ///< enter pattern notes using MIDI
			MAX_MODES
		};

		enum DRIVERS
		{
			DRIVER_MIDI, ///< the main MIDI input driver
			DRIVER_SYNC, ///< the driver for MIDI Sync
			MAX_DRIVERS
		};

		/// midi input handler.

		class MidiInputListener {
			public:
				virtual ~MidiInputListener() {}
				virtual void OnMidiData(uint32_t, DWORD_PTR dwParam1, DWORD_PTR dwParam2) = 0;
		};

		class CMidiInput
		{
		public:
			CMidiInput();
			virtual ~CMidiInput();

			/// returns the number of midi devices on the system
			static int GetNumDevices( void );	
			/// Return the description of a device by its index
			static void GetDeviceDesc( int idx, std::string& result);	
			/// convert a name identifier into a index identifier (or -1 if fail)
			static int FindDevByName( CString nameString );	

			/// set MIDI input device identifier
			void SetDeviceId(unsigned int driver, int devId);
			/// open the midi input devices
			bool Open();
			/// resync the MIDI with the audio engine
			void ReSync();
			/// close the midi input device
			bool Close( );

			/// find out if we are open
			bool Active() { return m_midiInHandle[ DRIVER_MIDI ]!=NULL; }

			/// for external access
			MIDI_STATS * GetStatsPtr() { return &m_stats; }		
			/// for external access
			MIDI_CONFIG * GetConfigPtr() { return &m_config; }

			/// set a instrument map
			void SetInstMap( int machine, int inst );	
			/// get the mapped instrument for the given machine
			int GetInstMap( int machine );	

			/// set a instrument map
			void SetGenMap( int channel, int generator );	
			/// get the mapped instrument for the given machine
			int GetGenMap( int channel );	

			/// set a controller map
			void SetControllerMap( int channel, int controller, int parameter );	
			/// get a controller map
			int GetControllerMap( int channel, int controller );	

			/// get the channel's note off status
			bool GetNoteOffStatus( int channel );	

			/// called to inject MIDI data
			void InjectMIDI( int amount );	

			/// the master MIDI handler mode (see enum MODES), external objects can change this at will
			int m_midiMode;	

			void SetListener(MidiInputListener* listener) { listener_ = listener; }

		private:
			/// the midi callback functions (just a static linker to the instance one)
			static void CALLBACK fnMidiCallbackStatic( HMIDIIN handle, uint32_t uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 );
			/// the real callbacks
			void CALLBACK fnMidiCallback_Inject( HMIDIIN handle, DWORD_PTR dwInstance, int p1HiWordLB, int p1LowWordHB, int p1LowWordLB, DWORD_PTR dwTime);
			/// the real callbacks
			void CALLBACK fnMidiCallback_Step( HMIDIIN handle, DWORD_PTR dwInstance, int p1HiWordLB, int p1LowWordHB, int p1LowWordLB, DWORD_PTR dwTime);

			void OnMidiData(uint32_t, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
			/// return the current device handle
			HMIDIIN GetHandle(unsigned int driver) { assert(driver < MAX_DRIVERS); return m_midiInHandle[driver]; }

			/// We've received a resync message
			void InternalReSync( DWORD_PTR dwParam2 );
			/// We've received a clock message
			void InternalClock( DWORD_PTR dwParam2 );

			/// midi device identifiers
			int m_devId[ MAX_DRIVERS ];		
			/// current input device handles
			HMIDIIN m_midiInHandle[ MAX_DRIVERS ];	
			/// for once-only tell of problems
			bool m_midiInHandlesTried;	

			/// channel->instrument map
			uint32_t m_channelInstMap[ MAX_MACHINES ];				
			/// channel->generator map
			uint32_t m_channelGeneratorMap[ MAX_MIDI_CHANNELS ];	
			/// channel, note off setting
			bool m_channelNoteOff[ MAX_MIDI_CHANNELS ];			
			/// channel->controller->parameter map
			int m_channelController[ MAX_MIDI_CHANNELS ][ MAX_CONTROLLERS ];		
			/// helper variable used in setting a controller map
			int m_channelSetting[ MAX_MIDI_CHANNELS ];	

			/// external telling us we need a resync
			bool m_reSync;			
			/// have we been synced with the audio engine yet?
			bool m_synced;			
			/// are we in the process of syncing?
			bool m_syncing;

			///\name audio-engine timing vars
			///\{
				/// Time (in millis) at which the MIDI port was started (can be used for own clock. Currently unused)
				///\todo use std::chrono
				uint32_t m_pc_clock_base;
				/// Time (in millis) at last resync (used to calculate the MIDI IN Clock deviation)
				///\todo use std::chrono
				uint32_t m_resyncClockBase;
				/// MIDI In clock (in millis) at the time of Resync. Used to verify clock deviation between MIDI IN clock calls
				///\todo use std::chrono
				uint32_t m_resyncMidiStampTime;
				/// play position (Sample being played), at the time of Resync. Used to verify clock deviation between MIDI IN clock calls
				int m_resyncPlayPos;	
				/// difference in samples between the clock position that we should be and the one we are
				int m_fixOffset;
				/// Adjusted MIDI In clock (in millis) at the time of Resync for the next event to be written into the audio buffer
				///\todo use std::chrono
				uint32_t m_resyncAdjStampTime;
				/// Accumulator (in samples) since last resync.
				uint32_t m_timingAccumulator;
				/// Accumulator (in millis) since last resync. (Used to know which events to process for this injectMIDI call).
				///\todo use std::chrono
				uint32_t m_timingCounter;
			///\}

			/// configuration information
			MIDI_CONFIG	m_config;	
			/// statistics information
			MIDI_STATS	m_stats;	
			
			/// midi buffer
			MIDI_BUFFER m_midiBuffer[ MIDI_BUFFER_SIZE ];	
			///\name buffer indexes
			///\{
				/// Position in the buffer to receive a new event.
				int m_bufWriteIdx;
				/// Position in the buffer for the first event to be read.
				int m_bufReadIdx;
				/// Amount of valid events in the buffer, starting in m_bufReadIdx
				int m_bufCount;

			///\}
			MidiInputListener* listener_;
		};
	}
}
