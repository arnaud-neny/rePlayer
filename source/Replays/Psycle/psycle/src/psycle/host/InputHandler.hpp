///\file
///\brief interface file for psycle::host::InputHandler.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PsycleConfig.hpp"
#include <universalis/stdlib/mutex.hpp>

namespace psycle
{
	namespace host
	{
		class CChildView;
		class Machine;
		class ParamTranslator;
		class Tweak;

		class SPatternUndo
		{
		public:
			SPatternUndo();
			SPatternUndo(const SPatternUndo& other);
			virtual ~SPatternUndo();
			SPatternUndo& operator=(const SPatternUndo &other);
		private:
			void Copy(const SPatternUndo &other);
		public:
			int type;
			unsigned char* pData;
			int dataSize;
			int pattern;
			int x;
			int y;
			int	tracks;
			int	lines;
			// store positional data plz
			int edittrack;
			int editline;
			int editcol;
			int seqpos;
			// counter for tracking, works like ID
			int counter;
		};


		/// input handler, keyboard configuration.
		class InputHandler  
		{
		public:
			/// constructor.
			InputHandler();
			virtual ~InputHandler();
		public:
			void SetChildView(CChildView* p) { pChildView=p; }
			void SetMainFrame(CMainFrame* p) { pMainFrame=p; }
		private:
			CChildView* pChildView;
			CMainFrame* pMainFrame;
			std::auto_ptr<ParamTranslator> param_translator_dummy;
		public:	
			///\name translation
			///\{
			/// .
			void CmdToKey(CmdSet cse,WORD & key,WORD & mods);	
			/// .
			CmdDef KeyToCmd(UINT nChar, UINT nFlags) { 
				return KeyToCmd(nChar, nFlags, GetKeyState(VK_SHIFT)<0,
								GetKeyState(VK_CONTROL)<0);
			}
			CmdDef KeyToCmd(UINT nChar, UINT nFlags, bool has_shift, bool has_ctrl);			
			/// .
			CmdDef StringToCmd(LPCTSTR str);
			///\}
		public:
			/// control 	
			void PerformCmd(CmdDef &cmd,BOOL brepeat);
			
			///\name commands
			///\{
			/// .
			PatternEntry BuildNote(int note, int instr=255, int velocity=127, bool bTranspose=true, Machine* mac=NULL, bool forcevolume=false);
			PatternEntry BuildTweak(int machine, int tweakidx, int value, bool slide, const ParamTranslator& translator);
			void PlayNote(int note,int instr=255, int velocity=127,bool bTranspose=true,Machine*pMachine=NULL);
			void PlayNote(PatternEntry * entry, int track);
			/// .
			void StopNote(int note,int instr=255, bool bTranspose=true,Machine*pMachine=NULL);
			///\}

			void Stop();
			void PlaySong();
			void PlayFromCur();

			int GetTrackToPlay(int note, int macNo, int instNo, bool noteoff);
			int GetTrackAndLineToEdit(int note, int macNo, int instNo, bool noteoff, bool bchord, int& outline);

			void MidiPatternNote(int outnote , int macidx, int channel, int velocity);	// called by the MIDI input to insert pattern notes
			void MidiPatternCommand(int busMachine, int command, int value); // called by midi to insert pattern commands
			void MidiPatternTweak(int busMachine, int command, int value, bool slide=false); // called by midi to insert pattern commands
			void MidiPatternMidiCommand(int busMachine, int command, int value); // called by midi to insert midi pattern commands
			void Automate(int macIdx, int param, int value, bool undo, const ParamTranslator& translator);

			void AddUndo(int pattern, int x, int y, int tracks, int lines, int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo=true, int counter=0);
			void AddRedo(int pattern, int x, int y, int tracks, int lines, int edittrack, int editline, int editcol, int seqpos, int counter);
			void AddUndoLength(int pattern, int lines, int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo=true, int counter=0);
			void AddRedoLength(int pattern, int lines, int edittrack, int editline, int editcol, int seqpos, int counter);
			void AddUndoSequence(int lines, int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo=true, int counter=0);
			void AddRedoSequence(int lines, int edittrack, int editline, int editcol, int seqpos, int counter);
			void AddUndoSong(int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo=true, int counter=0);
			void AddRedoSong(int edittrack, int editline, int editcol, int seqpos, int counter);
			void AddMacViewUndo(); // place holder
			void KillUndo();
			void KillRedo();
			bool IsModified();
			bool HasUndo(int viewMode);
			bool HasRedo(int viewMode);
			void SafePoint();
			void AddTweakListener(const boost::shared_ptr<Tweak>& listener) {
			  tweak_listeners_.push_back(listener);
			}
			
		private:
			/// get key modifier index.
			UINT GetModifierIdx(UINT nFlags, bool has_shift, bool has_ctrl)
			{
				UINT idx=0;
				if (has_shift) idx|=MOD_S;		// shift
				if (has_ctrl) idx|=MOD_C;		// ctrl
				if (nFlags&(1<<8)) idx|=MOD_E;	// extended
				//todo: detection of alt? (simple alt is captured by windows. alt-gr could be captured)
				return idx;
			}
			typedef class std::unique_lock<std::mutex> scoped_lock;
			std::mutex mutable mutex_;
			std::vector<boost::weak_ptr<Tweak> > tweak_listeners_;

		public:	
			/// Indicates that Shift+Arrow is Selection.
			bool bDoingSelection;		

			/// multi-key playback state stuff
			int notetrack[MAX_TRACKS];
			int instrtrack[MAX_TRACKS];
			int mactrack[MAX_TRACKS];
			/// last track output to	
			int outtrack;

			std::list<SPatternUndo> pUndoList;
			std::list<SPatternUndo> pRedoList;

			int UndoCounter;
			int UndoSaved;

			int UndoMacCounter;
			int UndoMacSaved;
		};
    
		namespace Actions {
			enum {
				tpb = 1,
				bpm = 2,
				trknum = 3,
				play = 4,
				playstart = 5,
				playseq = 6,
				stop = 7,
				rec = 8,
				seqsel = 9,
				seqmodified = 10,
				seqfollowsong = 11,
				patkeydown = 12,
				patkeyup = 13,
				songload = 14,
				songloaded = 15,
				songnew = 16,
				tracknumchanged = 17,
				octaveup = 18,
				octavedown = 19,
				undopattern = 20,
				patternlength = 21
			};
		};

		typedef int ActionType;

		class ActionHandler;
    
		class ActionListener {
			public:
				ActionListener() {};
				virtual ~ActionListener() = 0;
				virtual void OnNotify(const ActionHandler&, ActionType action) = 0;
		};

		typedef std::vector<ActionListener*> ActionListenerList;

		inline ActionListener::~ActionListener() {}

		class ActionHandler {
			public:
				ActionHandler() {}
				void AddListener(ActionListener* listener) {
					using namespace std;
					ActionListenerList::iterator it = 
						find(listeners_.begin(), listeners_.end(), listener);
					if (it!=listeners_.end()) {
						throw runtime_error("Listener already in PsycleActions.");
					}
					listeners_.push_back(listener);
				}
				void RemoveListener(ActionListener* listener) {
					using namespace std;
					ActionListenerList::iterator it = 
						find(listeners_.begin(), listeners_.end(), listener);
					if (it!=listeners_.end()) {
						listeners_.erase(it);
					}
				}            				
				void Notify(ActionType action);
				std::string ActionAsString(ActionType action) const;

			private:        
				ActionListenerList listeners_;
			};
		}
}

#define ANOTIFY(action) (PsycleGlobal::actionHandler().Notify(action))
#define BNOTIFY(action) (PsycleGlobal::actionHandler().NotifyBefore(action))