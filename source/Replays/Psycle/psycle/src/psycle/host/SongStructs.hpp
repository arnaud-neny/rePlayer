#pragma once
#include <psycle/host/detail/project.hpp>
#include <universalis/stdlib/cstdint.hpp>

namespace psycle
{
	namespace host
	{
		#pragma pack(push, 1)

		class PatternEntry
		{
			public:
				inline PatternEntry()
				: _note(255)
				, _inst(255)
				, _mach(255)
				, _cmd(0)
				, _parameter(0)
				{
				}

				inline PatternEntry(uint8_t note, uint8_t inst, uint8_t machine, uint8_t cmd, uint8_t param)
				: _note(note)
				, _inst(inst)
				, _mach(machine)
				, _cmd(cmd)
				, _parameter(param)
				{
				}
				uint8_t _note;
				uint8_t _inst;
				uint8_t _mach;
				uint8_t _cmd;
				uint8_t _parameter;
		};

		// Patterns are organized in rows.
		// i.e. pattern[rows][tracks], being a row = NUMTRACKS*sizeof(PatternEntry) bytes
		// belong to the first line.
		#pragma warning(push)
		#pragma warning(disable:4200) // nonstandard extension used : zero-sized array in struct/union; Cannot generate copy-ctor or copy-assignment operator when UDT contains a zero-sized array
		class Pattern
		{
			public:
				PatternEntry _data[];
		};
		#pragma warning(pop)

		enum MachineType
		{
			MACH_UNDEFINED = -1,
			MACH_MASTER = 0,
				MACH_SINE = 1, ///< now a plugin
				MACH_DIST = 2, ///< now a plugin
			MACH_SAMPLER = 3,
				MACH_DELAY = 4, ///< now a plugin
				MACH_2PFILTER = 5, ///< now a plugin
				MACH_GAIN = 6, ///< now a plugin
				MACH_FLANGER = 7, ///< now a plugin
			MACH_PLUGIN = 8,
			MACH_VST = 9,
			MACH_VSTFX = 10,
			MACH_SCOPE = 11, ///< Test machine. removed
			MACH_XMSAMPLER = 12,
			MACH_DUPLICATOR = 13,
			MACH_MIXER = 14,
			MACH_RECORDER = 15,
			MACH_DUPLICATOR2 = 16,
			MACH_LUA = 17,
			MACH_LADSPA = 18,
			MACH_DUMMY = 255
		};

		enum MachineMode
		{
			MACHMODE_UNDEFINED = -1,
			MACHMODE_GENERATOR = 0,
			MACHMODE_FX = 1,
			MACHMODE_MASTER = 2,
      MACHMODE_HOST = 3      
		};

    struct MachineUiType {
      enum Value {      
        NATIVE = 0,
        CUSTOMWND = 1,
        CHILDVIEW = 2
      };    
    };

		struct PatternCmd
		{
			enum{
				EXTENDED	= 0xFE, //(see below)
				SET_TEMPO	= 0xFF,
				NOTE_DELAY	= 0xFD,
				RETRIGGER   = 0xFB,
				RETR_CONT	= 0xFA,
				SET_VOLUME	= 0x0FC,
				SET_PANNING = 0x0F8,
				BREAK_TO_LINE = 0xF2,
				JUMP_TO_ORDER = 0xF3,
				ARPEGGIO	  = 0xF0,

				// Extended Commands from 0xFE
				SET_LINESPERBEAT0 = 0x00,  // 
				SET_LINESPERBEAT1 = 0x10, // Range from FE00 to FE1F is reserved for changing lines per beat.
				SET_BYPASS = 0x20,
				SET_MUTE = 0x30,
				PATTERN_LOOP  = 0xB0, // Loops the current pattern x times. 0xFEB0 sets the loop start point.
				PATTERN_DELAY =	0xD0, // causes a "pause" of x rows ( i.e. the current row becomes x rows longer)
				ROW_EXTRATICKS =	0xE0, // causes a "pause" of x ticks for all rows including this ( i.e. all rows becomes x ticks longer)
				FINE_PATTERN_DELAY=	0xF0 // causes a "pause" of x ticks ( i.e. the current row becomes x ticks longer)
			};
		};

		namespace notecommands
		{
			enum notecommands {
				c0 = 0,   // In MIDI, it is actually c minus 1
				middleC = 60,
				middleA = 69,
				b9 = 119, // In MIDI, it is actualy b8
				release = 120,
				tweak,
				tweakeffect, //old. for compatibility only.
				midicc,
				tweakslide,
				//Place whatever that can be written in the pattern above invalid, and anything else below it
				invalid,
				midi_sync = 254,
				empty = 255
			};
		}

		#pragma pack(pop)
	}
}
