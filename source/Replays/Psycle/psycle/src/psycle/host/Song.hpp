///\file
///\brief interface file for psycle::host::Song
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "FileIO.hpp"
#include "Instrument.hpp"
#include "XMInstrument.hpp"
#include "ExclusiveLock.hpp"
#include "cpu_time_clock.hpp"
#include "InstPreview.hpp"
#include <psycle/host/LoaderHelper.hpp> 
#include <universalis/stdlib/chrono.hpp>
#include <universalis/os/loggers.hpp>

class CCriticalSection;

namespace psycle
{
	namespace host
	{
		class Machine; // forward declaration
		class CProgressDialog;

		/// songs hold everything comprising a "tracker module",
		/// this include patterns, pattern sequence, machines and their initial parameters and coordinates, wavetables, ...		

		class Song
		{
			static int defaultPatLines;
		public:
			/// constructor.
			Song();
			/// destructor.
			virtual ~Song() throw();

			static void SetDefaultPatLines(int lines);
			static int GetDefaultPatLines() { return defaultPatLines; };

			/// the name of the song.
			std::string name;
			/// the author of the song.
			std::string author;
			/// the comments on the song
			std::string comments;
			/// the initial beats per minute (BPM) when the song is started playing.
			/// This can be changed in patterns using a command, but this value will not be affected.
			int m_BeatsPerMin;
			/// the initial lines per beat (LPB) when the song is started playing.
			/// This can be changed in patterns using a command, but this value will not be affected.
			int m_LinesPerBeat;
			/// The song Ticks per beat (ticks as 24 ticks per beat). Changing this does not change the real bpm, but changes the amount of ticks that sampulse sees. 
			/// Used in some commands, like the slide commands. In the future, this setting might also affect other effects and slides.
			int m_TicksPerBeat;
			/// The initial extraticks per line when the song is started playing. Changing this affects the real bpm. Each like will increase its length by the amount of ticks.
			/// This can be changed in patterns using a command, but this value will not be affected.
			/// It is intended to emulate correctly the classic trackers "speed" command. I.e speed 5 = 6 lpb + 1 extratick.
			int m_ExtraTicksPerLine;
			/// \todo This is a GUI thing... should not be here.
			char currentOctave;
			/// Array of Pattern data.
			unsigned char * ppPatternData[MAX_PATTERNS];
			/// Length, in patterns, of the sequence.
			int playLength;
			/// Sequence of patterns.
			unsigned char playOrder[MAX_SONG_POSITIONS];
			/// Selection of patterns (for the "playBlock()" play mode)
			bool playOrderSel[MAX_SONG_POSITIONS];
			/// number of lines of each pattern
			int patternLines[MAX_PATTERNS];
			/// Pattern name 
			char patternName[MAX_PATTERNS][32];
			/// The number of tracks in each pattern of this song.
			int SONGTRACKS;
			/// ???
			///\name instrument
			///\{
			///
			/// From now on, instSelected means "xminstruments" selected (only sampulse machines)
			int instSelected;
			/// From now on, waveSelected means "samples" and _pInstrument selected (for classic sampler and wave editor)
			int waveSelected;
			///
			Instrument * _pInstrument[MAX_INSTRUMENTS];
			SampleList samples;
			InstrumentList xminstruments;

			//Maps for virtual instruments.
			typedef std::pair<int, int> macinstpair;
			typedef std::pair<int, bool> instsampulsepair;
			std::map<int,macinstpair> virtualInst;
			std::map<instsampulsepair,int> virtualInstInv;

			void DeleteVirtualInstrument(int virtidx);
			void DeleteVirtualOfInstrument(int inst, bool sampulse);
			void DeleteVirtualsOfMachine(int macidx);

			void SetVirtualInstrument(int virtidx, int macidx, int targetins);
			inline macinstpair VirtualInstrument(int virtidx) const {
				std::map<int,macinstpair>::const_iterator it = virtualInst.find(virtidx);
				if(it != virtualInst.end()) {
					return it->second;
				}
				return macinstpair(-1,-1);
			}
			inline int VirtualInstrumentInverted(int targetins, bool sampulse) const {
				std::map<instsampulsepair,int>::const_iterator it = virtualInstInv.find(instsampulsepair(targetins,sampulse));
				if(it != virtualInstInv.end()) {
					return it->second;
				}
				return -1;
			}


			///\}
			/// The index of the selected machine parameter for note entering
			/// \todo This is a gui thing... should not be here.
			int paramSelected;
			/// The index for the auxcolumn selected (would be waveselected, midiselected, or paramSelected)
			/// \todo This is a gui thing... should not be here.
			int auxcolSelected;
			/// Wether each of the tracks is muted.
			bool _trackMuted[MAX_TRACKS];
			void ToggleTrackMuted(int ttm) { _trackMuted[ttm] = !_trackMuted[ttm]; }
			/// The number of tracks Armed (enabled for record)
			/// \todo should this be here? (used exclusively in childview)
			int _trackArmedCount;
			/// Wether each of the tracks is armed (selected for recording data in)
			bool _trackArmed[MAX_TRACKS];

			void ToggleTrackArmed(int ttm) {
				_trackArmed[ttm] = !_trackArmed[ttm];
				_trackArmedCount = 0;
				for (int i=0; i < MAX_TRACKS; i++)
				{
					if (_trackArmed[i])
					{
						_trackArmedCount++;
					}
				}
			}

			/// The names of the trakcs
			std::string _trackNames[MAX_PATTERNS][MAX_TRACKS];
			void ChangeTrackName(int patIdx, int trackidx, std::string name);
			void SetTrackNameShareMode(bool shared);
			void CopyNamesFrom(int patorigin, int patdest);
			bool shareTrackNames;
			///\name machines
			///\{
			/// the array of machines.
			Machine* _pMachine[MAX_MACHINES];
			/// Current selected machine number in the GUI
			/// \todo This is a gui thing... should not be here.			
			int seqBus;
			///\name wavetable
			///\{
			/// Wave Loader
			int WavAlloc(LoaderHelper& loadhelp,const std::string & sName);
			/// Wave allocator
			XMInstrument::WaveData<>& WavAlloc(int sampleIdx, bool bStereo, uint32_t iSamplesPerChan, const std::string & sName);
			XMInstrument::WaveData<>& WavAlloc(LoaderHelper& loadhelp, bool bStereo, uint32_t iSamplesPerChan, const std::string & sName, int &instout);
			/// SVX Loader
				//instmode=true then create/initialize an instrument (both, sampler and sampulse ones).
				//returns -1 error, else if instmode= true returns real inst index, else returns real sample idx.
			int IffAlloc(LoaderHelper& loadhelp, bool instMode, const std::string & sFileName);
			/// AIFF loader
			int AIffAlloc(LoaderHelper& loadhelp,const std::string & sFileName);
			void SavePsyInstrument(const std::string& filename, int instIdx) const;
			bool LoadPsyInstrument(LoaderHelper& loadhelp, const std::string& filename);
			/// wave preview allocation.  (thread safe)
			void SetWavPreview(XMInstrument::WaveData<> * wave);
			/// wave preview allocation.  (thread safe)
			void SetWavPreview(int newinst);
			///\}
			/// Initializes the song to an empty one. (thread safe)
			void New();
			/// Initializes the song to an empty one. (non thread safe)
			void DoNew();
			/// Resets some variables to their default values (used inside New(); )
			void Reset();
			/// Gets the first free slot in the pMachine[] Array
			int GetFreeMachine() const ;
			/// creates a new machine in this song.
		public:
			Machine* CreateMachine(MachineType type, char const* psPluginDll,int songIdx,int32_t shellIdx);
		public:
			/// creates a new machine in this song.
			bool CreateMachine(MachineType type, int x, int y, char const* psPluginDll, int songIdx,int32_t shellIdx=0);
			/// Creates a new machine, replacing an existing one.
			bool ReplaceMachine(Machine* origmac, MachineType type, int x, int y, char const* psPluginDll, int songIdx,int32_t shellIdx=0);
			/// exchanges the position of two machines.
			bool ExchangeMachines(int one, int two);
			/// destroy a machine of this song.
			void DestroyMachine(int mac);
			/// destroys a machine, but rewires its neighbours
			void DeleteMachineRewiring(int macIdx);
			/// destroys all the machines of this song.
			void DestroyAllMachines();
			/// Tells all the samplers of this song, that if they play this sample, stop playing it. (I know this isn't exactly a thing to do for a Song Class)
			void StopInstrument(int instrumentIdx);
			// the highest index of the samples (sampler instruments) used
			int GetHighestInstrumentIndex() const;
			// the highest index of the sampulse instruments
			int GetHighestXMInstrumentIndex() const;
			// the highest index of the patterns used
			int GetHighestPatternIndexInSequence() const;
			// the number of instruments used.
			int GetNumInstruments() const;
			/// the number of pattern used in this song.
			int GetNumPatterns() const;

			/// creates a new connection between two machines. returns index in the dest machine, or -1 if error.
			int InsertConnectionBlocking(Machine* srcMac,Machine* dstMac,int srctype=0, int dsttype=0,float value = 1.0f)
			{
				CExclusiveLock lock(&semaphore, 2, true);
				return InsertConnectionNonBlocking(srcMac, dstMac, srctype, dsttype, value);
			}
			int InsertConnectionNonBlocking(Machine* srcMac,Machine* dstMac,int srctype=0, int dsttype=0,float value = 1.0f);
			int InsertConnectionNonBlocking(int srcMac,int dstMac,int srctype=0, int dsttype=0,float value = 1.0f)
			{
				if ( srcMac >= MAX_MACHINES || dstMac >= MAX_MACHINES) return -1;
				if ( !_pMachine[srcMac] || !_pMachine[dstMac] ) return -1;
				return InsertConnectionNonBlocking(_pMachine[srcMac],_pMachine[dstMac],srctype,dsttype,value);
			}
			/// Changes the destination of a wire. srcMac is its origin, newdstMac is its new destionation. 
			/// outputwiresrc is the output wire index in srcMac. newinputwiredest is the input wire index in newdstMac
			/// returns true if succeeded. 
			bool ChangeWireDestMacBlocking(Machine* srcMac, Machine* newdstMac, int outputwiresrc, int newinputwiredest) {
				CExclusiveLock lock(&semaphore, 2, true);
				return ChangeWireDestMacNonBlocking(srcMac, newdstMac, outputwiresrc, newinputwiredest);
			}
			bool ChangeWireDestMacNonBlocking(Machine* srcMac, Machine* newdstMac, int outputwiresrc, int newinputwiredest);
			/// Changes the source of a wire. newsrcMac is the new source. dstMac is its destination. 
			/// newoutputwiresrc is the output wire index in newsrcMac. inputwiredest is the input wire index in dstMac
			/// returns true if succeeded. 
			bool ChangeWireSourceMacBlocking(Machine* newsrcMac, Machine* dstMac, int newoutputwiresrc, int inputwiredest) {
				CExclusiveLock lock(&semaphore, 2, true);
				return ChangeWireSourceMacNonBlocking(newsrcMac, dstMac, newoutputwiresrc, inputwiredest);
			}
			bool ChangeWireSourceMacNonBlocking(Machine* newsrcMac, Machine* dstMac, int newoutputwiresrc, int inputwiredest);
			/// Verifies that the new connection doesn't conflict with the mixer machine.
			bool ValidateMixerSendCandidate(Machine& mac,bool rewiring=false);
			void RestoreMixerSendsReturns();
			Machine* GetSamplerIfExists();
			Machine* GetSampulseIfExists();
			// This method is added because of virtual instruments, that are not machines, but represent a machine.
			Machine* GetMachineOfBus(int bus, int& alternateinst) const;
			std::string GetVirtualMachineName(Machine* mac, int alternateins) const;
			/// Gets the first free slot in the Machines' bus (slots 0 to MAX_BUSES-1)
			int GetFreeBus() const;
			/// Gets the first free slot in the Effects' bus (slots MAX_BUSES  to 2*MAX_BUSES-1)
			int GetFreeFxBus() const;
			/// Returns the first unused and empty pattern in the pPatternData[] Array. If none found, initializes to empty one not used in the sequence
			int GetBlankPatternUnused(int rval = 0);
			/// creates a new pattern.
			bool AllocNewPattern(int pattern,char *name,int lines,bool adaptsize);
			/// Adds an empty track at the index indicated. If pattern is -1, it does it for all patterns.
			void AddNewTrack(int pattern, int trackIdx);
			/// clones a machine.
			bool CloneMac(int src,int dst);
			/// clones an instrument.
			bool CloneIns(int src,int dst);
			/// Exchanges the index of two instruments
			void ExchangeInstruments(int one, int two);
			/// deletes all the patterns of this song.
			void DeleteAllPatterns();
			/// deletes (resets) the instrument and deletes (and resets) each sample/layer that it uses.
			void DeleteInstrument(int i);
			/// deletes (resets) the instrument and deletes (and resets) each sample/layer that it uses. (all instruments)
			void DeleteInstruments();
			/// destroy all instruments in this song.
			void DestroyAllInstruments();
			///  loads a file into this song object.
			///\param fullopen  used in context of the winamp/foobar player plugins, where it allows to get the info of the file, without needing to open it completely.
			bool Load(RiffFile* pFile,CProgressDialog& progress,bool fullopen=true);
			/// saves this song to a file.
			bool Save(RiffFile* pFile,CProgressDialog& progress,bool autosave=false);
			/// Used to detect if an especific pattern index is used in the sequence.
			bool IsPatternUsed(int i, bool onlyInSequence=false) const;
			//Used to check the contents of the pattern.
			bool IsPatternEmpty(int i) const;
			///\name wave file previewing
			///\todo shouldn't belong to the song class.
			///\{
		public:
			//todo these ought to be dynamically allocated
			/// Wave preview/wave editor playback.
			InstPreview wavprev;
#if defined WINAMP_PLUGIN
			int filesize;
#endif // WINAMP_PLUGIN
			/// runs the wave previewing.
			void DoPreviews(int amount);
			///\}

			/// Returns the start offset of the requested pattern in memory, and creates one if none exists.
			/// This function now is the same as doing &pPatternData[ps]
			inline unsigned char * _ppattern(int ps){
				if(!ppPatternData[ps]) return CreateNewPattern(ps);
				return ppPatternData[ps];
			}
			/// Returns the start offset of the requested track of pattern ps in the
			/// pPatternData Array and creates one if none exists.
			inline unsigned char * _ptrack(int ps, int track){
				if(!ppPatternData[ps]) return CreateNewPattern(ps)+ (track*EVENT_SIZE);
				return ppPatternData[ps] + (track*EVENT_SIZE);
			}
			/// Returns the start offset of the requested line of the track of pattern ps in
			/// the pPatternData Array and creates one if none exists.
			inline unsigned char * _ptrackline(int ps, int track, int line){
				if(!ppPatternData[ps]) return CreateNewPattern(ps)+ (track*EVENT_SIZE) + (line*MULTIPLY);
				return ppPatternData[ps] + (track*EVENT_SIZE) + (line*MULTIPLY);
			}
			inline const unsigned char * _ptrackline(int ps, int track, int line) const {
				return ppPatternData[ps] + (track*EVENT_SIZE) + (line*MULTIPLY);
			}
			/// Allocates the memory fo a new pattern at position ps of the array pPatternData.
			unsigned char * CreateNewPattern(int ps);
			/// removes a pattern from this song.
			void RemovePattern(int ps);

			
			int SongTracks() const {return SONGTRACKS;}
			void SongTracks(const int value){ SONGTRACKS = value;}

			//This one is because we truncate samples per line
			double EffectiveBPM(int samplerate) const
			{
				float ticksperline = static_cast<float>(TicksPerBeat())/LinesPerBeat();
				float sptfloat = (samplerate*60.f)/(static_cast<float>(BeatsPerMin())*TicksPerBeat());
				int spr = static_cast<int>(sptfloat*(ExtraTicksPerLine()+ticksperline));
				double effbpm = (static_cast<double>(samplerate)*60.0)/
					(static_cast<double>(spr)*static_cast<double>(LinesPerBeat()));
				return effbpm;
			}
			int BeatsPerMin() const {return m_BeatsPerMin;}
			void BeatsPerMin(const int value)
			{ 
				if ( value < 32 ) m_BeatsPerMin = 32;
				else if ( value > 999 ) m_BeatsPerMin = 999;
				else m_BeatsPerMin = value;
			}

			int LinesPerBeat() const {return m_LinesPerBeat;}
			void LinesPerBeat(const int value)
			{
				if ( value < 1 ) m_LinesPerBeat = 1;
				else if ( value > 31 ) m_LinesPerBeat = 31;
				else m_LinesPerBeat = value;
			}
			int TicksPerBeat() const {return m_TicksPerBeat;}
			void TicksPerBeat(const int value)
			{
				if ( value < 1 ) m_TicksPerBeat = 1;
				else m_TicksPerBeat = value;
			}
			int ExtraTicksPerLine() const {return m_ExtraTicksPerLine;}
			void ExtraTicksPerLine(const int value)
			{
				if ( value < 0 ) m_ExtraTicksPerLine = 0;
				else if ( value > 31 ) m_ExtraTicksPerLine = 31;
				else m_ExtraTicksPerLine = value;
			}
			float tickstomillis(int ticks) {
				return static_cast<float>(ticks)
					*60000.f/
					(m_BeatsPerMin*(m_TicksPerBeat+(m_ExtraTicksPerLine*m_LinesPerBeat)));
			}
			float millistoticks(int millis) {
				return (static_cast<float>(millis)
					*m_BeatsPerMin*(m_TicksPerBeat+(m_ExtraTicksPerLine*m_LinesPerBeat)))
					/60000.f;
			}

			/*Solo/unsolo machine. Use -1 to unset*/
			void SoloMachine(int macIdx);
			/// The file name this song was loaded from.
			std::string fileName;
			/// The index of the machine which plays in solo.
			int machineSoloed;
			/// Is this song saved to a file?
			bool _saved;
			/// The index of the track which plays in solo.
			int _trackSoloed;
			void ToggleTrackSoloed(int ttm) {
				if (_trackSoloed != ttm )
				{
					for ( int i=0;i<MAX_TRACKS;i++ )
					{
						_trackMuted[i] = true;
					}
					_trackMuted[ttm] = false;
					_trackSoloed = ttm;
				}
				else
				{
					for ( int i=0;i<MAX_TRACKS;i++ )
					{
						_trackMuted[i] = false;
					}
					_trackSoloed = -1;
				}
			}
			//Semaphore used for song (and machine) manipulation. The semaphore accepts at much two threads to run:
			//Player::Work and ChildView::OnTimer (GUI update).
			//There is a third CSingleLock in InfoDlg (which is not a perfect situation, but it's fast enough so it shouldn't disturb Player::Work).
			//For an exclusive lock (i.e. the rest of the cases) use the CExclusiveLock.
			CSemaphore mutable semaphore;

			cpu_time_clock::duration accumulated_processing_time_, accumulated_routing_time_;

		/// cpu time usage measurement
		void reset_time_measurement() { accumulated_processing_time_ = accumulated_routing_time_ = cpu_time_clock::duration::zero(); } // rePlayer

		/// total processing cpu time usage measurement
		cpu_time_clock::duration accumulated_processing_time() const throw() { return accumulated_processing_time_; }
		/// total processing cpu time usage measurement
		void accumulate_processing_time(cpu_time_clock::duration d) throw() {
			if(loggers::warning() && d.count() < 0) {
				std::ostringstream s;
				s << "time went backward by: " << ::std::chrono::nanoseconds(d).count() * 1e-9 << 's'; // rePlayer
				loggers::warning()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
			} else accumulated_processing_time_ += d;
		}

		/// routing cpu time usage measurement
		cpu_time_clock::duration accumulated_routing_time() const throw() { return accumulated_routing_time_; }
		/// routing cpu time usage measurement
		void accumulate_routing_time(cpu_time_clock::duration d) throw() {
			if(loggers::warning() && d.count() < 0) {
				std::ostringstream s;
				s << "time went backward by: " << ::std::chrono::nanoseconds(d).count() * 1e-9 << 's'; // rePlayer
				loggers::warning()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
			} else accumulated_routing_time_ += d;
		}
		
		};
	}
}