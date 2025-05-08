#pragma once
#include <psycle/host/detail/project.hpp>
#include "Version.hpp"
#include <psycle/plugin_interface.hpp>
namespace psycle
{
	namespace host
	{
		/// number of samples per tweak slide update
		#define TWEAK_SLIDE_SAMPLES		64
		/// number of tws commands that can be active on one machine
		#define MAX_TWS					16
		/// Slots avaiable to load machines of each class (gen and FX). Power of 2! Important!
		#define MAX_BUSES				64
		/// Max number of machines+1 (master)
		#define MAX_MACHINES			129
		/// Max number of machines+ virtual instruments ( sampled instrument used as a generator). 255 is not usable because it is the empty value.
		#define MAX_VIRTUALINSTS		255
		/// Index of MasterMachine
		#define MASTER_INDEX			128
		/// Max number of instruments.
		#define MAX_INSTRUMENTS			256
		/// Instrument index of the wave preview.
		#define PREV_WAV_INS			255
		/// Number of tracks of the sequence (psycle just support one sequence now). Modify this, CURRENT_FILE_VERSION_SEQD and add the appropiated load and save code.
		#define MAX_SEQUENCES			1
		/// maximum number of different patterns. PSY3 Fileformat supports up to 2^32. UI is limited to 256 for now.
		#define MAX_PATTERNS			256
		/// Max number of pattern tracks
		/*unsigned*/ int const MAX_TRACKS = plugin_interface::MAX_TRACKS;
		/// harcoded maximal number of lines per pattern
		#define MAX_LINES				1024
		/// maximum number of positions in the sequence. PSY3 Fileformat supports up to 2^32. UI is limited to 256 for now.
		#define MAX_SONG_POSITIONS		256
		/// Max input connections and output connections a machine can have. (\todo: should be replaced by a dynamic array)
		#define MAX_CONNECTIONS		12
		/// Size in bytes of an event (note-aux-mac-effect). It is better to have an alternate structure for additional columns in the event.
		#define EVENT_SIZE				5
		/// PSY2-fileformat Constants
		#define OLD_MAX_TRACKS			32
		#define OLD_MAX_WAVES			16
		#define OLD_MAX_INSTRUMENTS		255
		#define OLD_MAX_PLUGINS			256

		/// Miscellaneous offset data.
		#define MULTIPLY				MAX_TRACKS * EVENT_SIZE
		#define MULTIPLY2				MULTIPLY * MAX_LINES		
		#define MAX_PATTERN_BUFFER_LEN	MULTIPLY2 * MAX_PATTERNS	

		/// Temporary buffer to get all the audio from Master (which work in small chunks), and send it to the soundcard after converting it to float.
		#define MAX_SAMPLES_WORKFN		65536
		/// Sampler
		#define OVERLAPTIME				128
		/// Maximum size of the audio block to be passed to the Work() function.
		/*unsigned*/ int const STREAM_SIZE = plugin_interface::MAX_BUFFER_LENGTH;

		/// Current version of the Song file chunks. 0xAAAABBBB  A= Major version (can't be loaded, skip the whole chunk), B=minor version. It can be loaded with the existing loader, the loader skips the extra info.
		//Version for the metadata information (author, comments..)
		#define CURRENT_FILE_VERSION_INFO	0x00000000
		//Version for the song data information (BPM, number of tracks..)
		#define CURRENT_FILE_VERSION_SNGI	0x00000002
		//Version for the sequence data (playback order)
		#define CURRENT_FILE_VERSION_SEQD	0x00000000
		//Version for the pattern data
		#define CURRENT_FILE_VERSION_PATD	0x00000001
		//Version for the machine data
		#define CURRENT_FILE_VERSION_MACD	0x00000002
		//Version for the instrument (classic sampler) data
		#define CURRENT_FILE_VERSION_INSD	0x00000002
		//Version for the wave (classic sampler) data subchunk (legacy)
		#define CURRENT_FILE_VERSION_WAVE	0x00000000
		//Version for the instrument (sampulse sampler) data  (legacy)
		#define CURRENT_FILE_VERSION_EINS	0x00010001
		//Version for the instrument (sampulse sampler) data
		#define CURRENT_FILE_VERSION_SMID	0x00000001
		//Version for the wave sample bank data
		#define CURRENT_FILE_VERSION_SMSB	0x00000001
		//Version for the wave sample bank data
		#define CURRENT_FILE_VERSION_VIRG	0x00000000
		//Combined value for the fileformat.
		#define CURRENT_FILE_VERSION CURRENT_FILE_VERSION_INFO+CURRENT_FILE_VERSION_SNGI+CURRENT_FILE_VERSION_SEQD+CURRENT_FILE_VERSION_PATD+CURRENT_FILE_VERSION_MACD+CURRENT_FILE_VERSION_INSD+CURRENT_FILE_VERSION_SMID+CURRENT_FILE_VERSION_SMSB

		unsigned const int VERSION_MAJOR_ZERO = 0x00000000;
		unsigned const int VERSION_MAJOR_ONE  = 0x00010000;

		#define CURRENT_CACHE_MAP_VERSION	0x0002
	}
}
