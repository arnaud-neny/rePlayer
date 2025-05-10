///\file
///\brief interface file for psycle::host::Player.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"

#include <universalis/stdlib/condition_variable.hpp>
#include <universalis/stdlib/chrono.hpp>
#include <universalis/stdlib/mutex.hpp>
#include <universalis/stdlib/thread.hpp>
#include "Machine.hpp"
#include "ExclusiveLock.hpp"
#include "AudioDriver.hpp"
#include <psycle/helpers/riff.hpp>
#include <psycle/helpers/dither.hpp>
namespace psycle
{
	namespace host
	{
		/// schedule the processing of machines, sends signal buffers and sequence events to them, ...
		class Player
		{
		public:
			/// constructor.
			Player();
			/// destructor.
			virtual ~Player() throw();
		private:
			/// Moves the cursor one line forward, changing the pattern if needed.
			void AdvancePosition();
			/// Initial Loop. Read new line and execute Global commands/tweaks.
			void ExecuteGlobalCommands(void);
			/// Notify all machines that a new Line comes.
			void NotifyNewLine(void);
			/// Final Loop. Read the line again for notes to send to the Machines
			void ExecuteNotes(void);
			/// Everything sent. telling it to machines.
			void NotifyPostNewLine(void);
		public:
			/// Function to encapsulate the three functions above.
			void ExecuteLine();
			/// Calculates position (by line or by time) and returns the samples and the time to that position.
			/// if all are -1, then it returns total time.
			int CalcPosition(Song& song, int &inoutseqPos, int& inoutpatLine, int &inoutseektime_ms, int& inoutlinecount, bool allowLoop=false, int startPos = 0);
			int NumSubsongs(Song& song, int subsongIndex = -1);

			/// seeks to a position seektime_ms  in milliseconds
			void SeekToPosition(psycle::host::Song& song, int seektime_ms = -1,bool allowLoop=false);
			void CalculatePPQforVst();
			/// Indicates if the playback has moved to a new line. Used for GUI updating.
			bool _lineChanged;
			/// Contains the number of samples until a line change comes in.
			int _samplesRemaining;
			/// Used to indicate that the SamplesPerRow has been manually changed ( right now, in effects "pattern delay" and "fine delay" )
			bool _SPRChanged;
			/// the line currently being played in the current pattern
			int _lineCounter;
			/// the line at which to stop playing (used by save block to wav)
			int _lineStop;
			/// the sequence position currently being played
			int _playPosition;
			/// the pattern currently being played.
			int _playPattern;
			/// elapsed time since playing started. Units is seconds and the float type allows for storing milliseconds.
			float _playTime;
			/// elapsed time since playing started in minutes.It just serves to complement the previous variable
			///\todo There is no need for two vars.
			int _playTimem;
			/// the amount of samples elapsed since start to play (this is for the VstHost)
			double sampleCount;
			int runninglineCount;

			/// the offset of the machine work()/midi InjectMIDI() call versus this player Work() call.
			double sampleOffset;
			/// the current beats per minute at which to play the song.
			/// can be changed from the song itself using commands.
			int bpm;
			/// the current lines per beat at which to play the song.
			/// can be changed from the song itself using commands.
			int lpb;
			/// starts to play, also specifying a line to stop playing at.
			void Start(int pos,int lineStart,int lineStop,bool initialize=true);
			/// starts to play.
			void Start(int pos,int line,bool initialize=true);
			/// wether this player has been started.
			bool _playing;
			/// wether this player should only play the selected block in the sequence.
			bool _playBlock;
			/// wheter this player should play the song/block in loop.
			bool _loopSong;
			bool _looped;
			/// Indicates if the player is processing audio right now (It is in work function)
			bool _isWorking;
			/// stops playing (thread safe).
			void Stop();
			bool trackPlaying(int track);
			bool measure_cpu_usage_;
		private:
			// stops playing (non thread safe)
			void DoStop(void);
		public:
			/// work function. (Entrance for the callback function (audiodriver)
			static float * Work(void* context, int nsamples);
			float * Work(int numSamples);
			
			void SetBPM(int _bpm,int _lpb=-1, int _extraticks=-1);

			//Change the samplerate (Thread safe)
			void SetSampleRate(const int sampleRate);
		private:
			//Change the samplerate (non Thread safe)
			void SampleRate(const int sampleRate);
		public:
			int SampleRate() const { return m_SampleRate; }
			void RecalcSPR(int extraSamples=0);

			/// Sets the number of samples that it takes for each row of the pattern to be played
			void SamplesPerRow(const int samplePerRow) {m_SamplesPerRow = samplePerRow;}
			float SamplesPerRow() const { return m_SamplesPerRow; }
			/// Samples per (tracker) tick.
			float SamplesPerTick() const { return m_SamplesPerTick; }
			int ExtraTicks() const { return m_extraTicks; }
			//For VST (due to rounding in samples per line, it is not exact)
			double EffectiveBPM() const;
			//For display, related to "extraticks".
			int RealBPM() const;
			int RealTPB() const;

			///\name secondary output device, write to a file
			///\{
			/// starts the recording output device.
			void StartRecording(std::string psFilename,int bitdepth=-1,int samplerate =-1, channel_mode channelmode =no_mode, bool isFloat = false, bool dodither=false, int ditherpdf=0, int noiseshape=0, std::vector<char*> *clipboardmem=0);
			/// stops the recording output device.
			void StopRecording(bool bOk = true);
			bool ClipboardWriteMono(float sample);
			bool ClipboardWriteStereo(float left, float right);
			/// wether the recording device has been started.
			bool _recording;
			/// wether the recording is done to memory.
			bool _clipboardrecording;
			std::vector<char*> *pClipboardmem;
			int clipbufferindex;
			/// whether to apply dither to recording
			bool _dodither;
			///\}
		protected:
			/// Stores which machine played last in each track. this allows you to not specify the machine number everytime in the pattern.
			int prevMachines[MAX_TRACKS];
			bool playTrack[MAX_TRACKS];
			/// Stores which instrument/aux value last used in each track.
			int prevInstrument[MAX_TRACKS];
			/// Temporary buffer to get all the audio from Master (which work in small chunks), and send it to the soundcard after converting it to float.
			float* _pBuffer;
			/// file to which to output signal.
			WaveFile _outputWaveFile;
			/// dither handler
			helpers::dsp::Dither dither;
			int m_recording_depth;
			channel_mode m_recording_chans;
			/// Samples of a legacy (tracker) tick. There are 24ticks per beat.
			int m_SamplesPerTick;
			/// samples per row. (Number of samples that are produced for each line(row) of pattern)
			/// This is computed from  BeatsPerMin(), LinesPerBeat() and SamplesPerSecond, plus the extra ticks.
			int m_SamplesPerRow;

			int m_SampleRate;
			int m_extraTicks; // Patch for tracker mods. It is a replacement for the speed command with a partially different meaning.
			short _patternjump;
			short _linejump;
			short _loop_count;
			short _loop_line;



		///\name multithreaded scheduler
		///\{
		public:
			void start_threads(int thread_count);
			void stop_threads();
			CExclusiveLock GetLockObject();
		private:
			typedef std::list<std::thread> threads_type;
			threads_type threads_;
		public:
			std::size_t num_threads() const { if(threads_.empty()){return 1;} return threads_.size(); }
		private:
			void thread_function(std::size_t thread_number);

			typedef class std::unique_lock<std::mutex> scoped_lock;
			std::mutex mutable mutex_;
			std::condition_variable mutable condition_, main_condition_;

			bool stop_requested_;
			bool suspend_requested_;
			std::size_t suspended_;
			std::size_t waiting_;

			typedef std::list<Machine*> nodes_queue_type;
			/// nodes with no dependency.
			nodes_queue_type terminal_nodes_;
			/// nodes ready to be processed, just waiting for a free thread
			nodes_queue_type nodes_queue_;

			Machine::sched_deps input_nodes_, output_nodes_;

			std::size_t graph_size_, processed_node_count_;

			void suspend_and_compute_plan();
			void compute_plan();
			void clear_plan();
			void process_loop(std::size_t thread_number) throw(std::exception);
			int samples_to_process_;
		///\}
		};
	}
}
