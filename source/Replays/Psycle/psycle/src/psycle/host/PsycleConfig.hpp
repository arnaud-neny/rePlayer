///\file
///\interface psycle::host::Configuration.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "Configuration.hpp"
#include "CmdDef.hpp"
#include <map> // rePlayer

namespace psycle
{
	namespace host
	{
		#define PSYCLE__PATH__DEFAULT_PATTERN_HEADER_SKIN "Psycle Default (internal)"
		#define PSYCLE__PATH__DEFAULT_MACHINE_SKIN "Psycle Default (internal)"
		#define PSYCLE__PATH__DEFAULT_DIAL_SKIN "No Dial Bitmap"
		#define PSYCLE__PATH__DEFAULT_BACKGROUND_SKIN "No Background Bitmap"

		class AudioDriverSettings;

		class SSkinSource
		{
		public:
			int x;
			int y;
			int width;
			int height;
		};

		class SSkinDest
		{
		public:
			int x;
			int y;
		};

		class SPatternHeaderCoords
		{
		public:
			void SwitchToClassic();
			void SwitchToText();

			//Classic header without name
			SSkinSource sBackgroundClassic;
			SSkinSource sNumber0Classic;
			SSkinSource sRecordOnClassic;
			SSkinSource sMuteOnClassic;
			SSkinSource sSoloOnClassic;
			SSkinSource sPlayOnClassic;
			SSkinDest dDigitX0Classic;
			SSkinDest dDigit0XClassic;
			SSkinDest dRecordOnClassic;
			SSkinDest dMuteOnClassic;
			SSkinDest dSoloOnClassic;
			SSkinDest dPlayOnClassic;
			SSkinSource sMuteOnTrackingClassic;
			SSkinSource sSoloOnTrackingClassic;
			SSkinSource sRecordOnTrackingClassic;
			SSkinDest dMuteOnTrackClassic;
			SSkinDest dSoloOnTrackClassic;
			SSkinDest dRecordOnTrackClassic;
			// Header with track name.
			SSkinSource sBackgroundText;
			SSkinSource sTextCrop;
			SSkinSource sNumber0Text;
			SSkinSource sRecordOnText;
			SSkinSource sMuteOnText;
			SSkinSource sSoloOnText;
			SSkinSource sPlayOnText;
			SSkinDest dDigitX0Text;
			SSkinDest dDigit0XText;
			SSkinDest dRecordOnText;
			SSkinDest dMuteOnText;
			SSkinDest dSoloOnText;
			SSkinDest dPlayOnText;
			SSkinSource sMuteOnTrackingText;
			SSkinSource sSoloOnTrackingText;
			SSkinSource sRecordOnTrackingText;
			SSkinDest dMuteOnTrackText;
			SSkinDest dSoloOnTrackText;
			SSkinDest dRecordOnTrackText;
			// Skin being used
			SSkinSource sBackground;
			SSkinSource sNumber0;
			SSkinSource sRecordOn;
			SSkinSource sMuteOn;
			SSkinSource sSoloOn;
			SSkinSource sPlayOn;
			SSkinDest dDigitX0;
			SSkinDest dDigit0X;
			SSkinDest dRecordOn;
			SSkinDest dMuteOn;
			SSkinDest dSoloOn;
			SSkinDest dPlayOn;
			SSkinSource sMuteOnTracking;
			SSkinSource sSoloOnTracking;
			SSkinSource sRecordOnTracking;
			SSkinDest dMuteOnTrack;
			SSkinDest dSoloOnTrack;
			SSkinDest dRecordOnTrack;
			bool bHasTransparency;
			bool bHasPlaying;
			bool bHasTextSkin;
			COLORREF cTransparency;
			COLORREF cTextFontColour;
			std::string szTextFont;
			int textFontPoint;
			int textFontFlags;
		};

		class SMachineCoords
		{
		public:
			SSkinSource sMaster;
			SSkinSource sGenerator;
			SSkinSource sGeneratorVu0;
			SSkinSource sGeneratorVuPeak;
			SSkinSource sGeneratorPan;
			SSkinSource sGeneratorMute;
			SSkinSource sGeneratorSolo;
			SSkinSource sEffect;
			SSkinSource sEffectVu0;
			SSkinSource sEffectVuPeak;
			SSkinSource sEffectPan;
			SSkinSource sEffectMute;
			SSkinSource sEffectBypass;
			SSkinSource dGeneratorVu;
			SSkinSource dGeneratorPan;
			SSkinDest dGeneratorMute;
			SSkinDest dGeneratorSolo;
			SSkinDest dGeneratorName;
			SSkinSource dEffectVu;
			SSkinSource dEffectPan;
			SSkinDest dEffectMute;
			SSkinDest dEffectBypass;
			SSkinDest dEffectName;
			SSkinDest dGeneratorNameClip;
			SSkinDest dEffectNameClip;
			bool bHasTransparency;
			COLORREF cTransparency;
		};
		class SParamsCoords
		{
		public:
			//statically defined
			SSkinSource sMixerSlider;
			SSkinSource sMixerKnob;
			SSkinSource sMixerVuOff;
			SSkinDest   sMixerVuOn;
			SSkinSource sMixerSwitchOff;
			SSkinDest   sMixerSwitchOn;
			SSkinSource sMixerCheckOff;
			SSkinDest   sMixerCheckOn;
			SSkinSource sMasterVuLeftOff;
			SSkinDest   sMasterVuLeftOn;
			SSkinDest   sMasterVuRightOff;
			SSkinDest   sMasterVuRightOn;
			SSkinSource sMasterKnob;
			//Dynamically defined in .psc
			std::string dialBmp;
			std::string sendMixerBmp;
			std::string masterBmp;
			std::string szParamsFont;
			int paramsFontPoint;
			int paramsFontFlags;
			std::string szParamsBoldFont;
			int paramsBoldFontPoint;
			int paramsBoldFontFlags;
			COLORREF masterFontBackColour;
			COLORREF masterFontForeColour;
			std::string szMasterFont;
			int masterFontPoint;
			int masterFontFlags;
			SSkinSource dMasterNames;
			SSkinDest dMasterMasterNumbers;
			SSkinDest dMasterChannelNumbers;
		};
		
		/// configuration.
		class PsycleConfig : public Configuration
		{
		public:
			class MachineParam
			{
			public:
				MachineParam();
				virtual ~MachineParam();
				void SetDefaultSettings(bool include_others=true);
				void SetDefaultColours();
				void SetDefaultSkin();
				bool RefreshBitmaps();
				void Load(ConfigStorage &,std::string mainSkinDir, std::string machine_skin);
				void Save(ConfigStorage &);
				void RefreshSettings();
				void RefreshSkin();
			
				bool toolbarOnMachineParams;
				COLORREF fontTopColor;
				COLORREF fontBottomColor;
				COLORREF fonthTopColor;
				COLORREF fonthBottomColor;
				COLORREF fonttitleColor;
				COLORREF topColor;
				COLORREF bottomColor;
				COLORREF hTopColor;
				COLORREF hBottomColor;
				COLORREF titleColor;
				CFont	font;
				CFont	font_bold;
				CFont   masterNamesFont;
				CRect	deskrect;
				std::string szBmpControlsFilename;
				CBitmap dial;
				CBitmap mixerSkin;
				CBitmap masterSkin;
				HBITMAP hbmMachineDial;
				HBITMAP hbmMixerSkin;
				HBITMAP hbmMasterSkin;
				int dialwidth;
				int dialheight;
				int dialframes;
				bool masterRefresh;
				SParamsCoords coords;
			};

			class MachineView
			{
			public:
				MachineView();
				virtual ~MachineView();
				void SetDefaultSettings(bool include_others=true);
				void SetDefaultColours();
				void SetDefaultSkin();
				void SetDefaultBackground();
				void Load(ConfigStorage &, std::string mainSkinDir);
				void Save(ConfigStorage &);
				void RefreshSettings();
				void RefreshSkin();
				bool RefreshBitmaps();
				void RefreshBackground();
			public:
				COLORREF colour;
				COLORREF polycolour;
				int triangle_size;
				COLORREF wirecolour;
				COLORREF wireaacolour;
				COLORREF wireaacolour2;
				int  wirewidth;
				bool wireaa;

				COLORREF vu1;
				COLORREF vu2;
				COLORREF vu3;
				bool draw_vus;

				bool bBmpBkg;
				std::string szBmpBkgFilename;
				std::string machine_skin;
				SMachineCoords	MachineCoords;
				CBitmap machineskin;
				CBitmap machineskinmask;
				CBitmap machinebkg;
				HBITMAP hbmMachineSkin;
				HBITMAP hbmMachineBkg;
				bool draw_mac_index;
				int bkgx;
				int bkgy;

				COLORREF generator_fontcolour;
				std::string generator_fontface;
				int generator_font_point;
				UINT generator_font_flags;
				COLORREF effect_fontcolour;
				std::string effect_fontface;
				int effect_font_point;
				UINT effect_font_flags;
				CFont generatorFont;
				CFont effectFont;
			};

			class PatternView
			{
			public:
				static char* notes_tab_a440[256];
				static char* notes_tab_a220[256];

				PatternView();
				virtual ~PatternView();
				void SetDefaultSettings(bool include_others=true);
				void SetDefaultColours();
				void SetDefaultSkin();
				void Load(ConfigStorage &, std::string mainSkinDir);
				void Save(ConfigStorage &);
				void RefreshSettings();
				void RefreshSkin();
				bool RefreshBitmaps();

				COLORREF separator;
				COLORREF separator2;
				COLORREF background;
				COLORREF background2;
				COLORREF row4beat;
				COLORREF row4beat2;
				COLORREF rowbeat;
				COLORREF rowbeat2;
				COLORREF row;
				COLORREF row2;
				COLORREF font;
				COLORREF font2;
				COLORREF fontPlay;
				COLORREF fontPlay2;
				COLORREF fontCur;
				COLORREF fontCur2;
				COLORREF fontSel;
				COLORREF fontSel2;
				COLORREF selection;
				COLORREF selection2;
				COLORREF playbar;
				COLORREF playbar2;
				COLORREF cursor;
				COLORREF cursor2;

				std::string header_skin;
				SPatternHeaderCoords PatHeaderCoords;
				CBitmap patternheader;
				CBitmap patternheadermask;
				HBITMAP hbmPatHeader;

				CFont pattern_font;
				CFont pattern_tracknames_font;
				std::string font_name;
				UINT font_flags;
				int font_point;
				int font_x;
				int font_y;

				bool _centerCursor;
				bool draw_empty_data;
				int timesig;
				bool showTrackNames_;
				bool showA440;

				bool _linenumbers;
				bool _linenumbersHex;
				bool _linenumbersCursor;
			};

			class InputHandler
			{
			public:
				InputHandler();
				virtual ~InputHandler();
				void SetDefaultSettings(bool include_others=true);
				void SetDefaultKeys();
				void Load(ConfigStorage &);
				void Save(ConfigStorage &);
				void RefreshSettings();
				inline bool SetCmd(CmdSet cset, UINT modifiers, UINT key)
				{
					CmdDef cmd(cset);
					return SetCmd(cmd,modifiers,key, true);
				}
				inline bool SetCmd(CmdDef const &cmd, UINT modifiers, UINT key)
				{
					return SetCmd(cmd,modifiers, key, true);
				}
			protected:
				bool SetCmd(CmdDef const &cmd, UINT modifiers, UINT key, bool checkforduplicates=true);
			public:
				///  Settings from the Keyb and misc tab///
				/// right ctrl mapped to PLAY?
				bool bCtrlPlay;		
				/// FT2 style Home/End?
				bool bFT2HomeBehaviour;	
				/// FT2 style Delete line?
				bool bFT2DelBehaviour;	
				bool _windowsBlocks;
				/// Shift+Arrows do act as selection?
				bool bShiftArrowsDoSelect; 
				bool _wrapAround;
				bool _cursorAlwaysDown;
				bool _RecordMouseTweaksSmooth;
				bool _RecordUnarmed;
				bool _NavigationIgnoresStep;
				int _pageUpSteps;		// 0 -> one beat, 1 -> one bar , > 1 -> steps
				// Map of the existing command sets and their current mapping
				// pair is "modi, key". If command not set they are 0,0
				std::map<CmdSet,std::pair<int,int>> setMap;
				
				///  Settings from the sequencer bar///
				bool _RecordNoteoff;
				bool _RecordTweaks;
				/// multikey playback?
				bool bMultiKey;		
				bool _notesToEffects;
				/// Move cursor when paste?
				bool bMoveCursorPaste;		
				
				//Inverted map for easier access on keypress
				// pair is "modi, key".
				// Only mapped keys exists (there shouldn't be any cdefNull)
				// No two key pairs should map the same command.
				std::map<std::pair<int,int>,CmdDef> keyMap;
			};
		public:
			class Midi
			{
			public:
				class group_with_message;
				class group_t
				{
				public:
					enum {
						t_command=0,
						t_tweak,
						t_tweakslide,
						t_mcm,
						num_types
					};
					group_t(int const & command = 0) : record_(), type_(), command_(command), from_(), to_(0xff) {}
				public:
					bool const inline & record()  const throw() { return record_; }
					bool       inline & record()        throw() { return record_; }
					int  const inline & type()    const throw() { return type_; }
					int        inline & type()          throw() { return type_; }
					int  const inline & command() const throw() { return command_; }
					int        inline & command()       throw() { return command_; }
					int  const inline & from()    const throw() { return from_; }
					int        inline & from()          throw() { return from_; }
					int  const inline & to()      const throw() { return to_; }
					int        inline & to()            throw() { return to_; }
				private:
					bool               record_;
					int                type_;
					int                command_;
					int                from_;
					int                to_;
				public:
					typedef group_with_message with_message;
				};

				class group_with_message : public group_t
				{
				public:
					group_with_message() : message_() {}
				public:
					int const inline & message() const throw() { return message_; }
					int       inline & message()       throw() { return message_; }
				private:
					int                message_;
				};

				typedef std::vector<group_t::with_message> groups_t;

				typedef enum {
					MS_USE_SELECTED = 0,
					MS_BANK,
					MS_PROGRAM,
					MS_MIDI_CHAN
				} selector_t;

			public:
				Midi();
				virtual ~Midi();
				void SetDefaultSettings();
				void Load(ConfigStorage &);
				void Save(ConfigStorage &);
				void RefreshSettings();

				groups_t    const inline & groups()           const throw() { return groups_; }
				groups_t          inline & groups()                 throw() { return groups_; }
				group_t::with_message const inline & group (std::size_t const & index) const throw() { return groups()[index]; }
				group_t::with_message       inline & group (std::size_t const & index)       throw() { return groups()[index]; }
				group_t     const inline & velocity()         const throw() { return velocity_; }
				group_t           inline & velocity()               throw() { return velocity_; }
				group_t     const inline & pitch()            const throw() { return pitch_; }
				group_t           inline & pitch()                  throw() { return pitch_; }
				bool        const inline & raw()              const throw() { return raw_; }
				bool              inline & raw()                    throw() { return raw_; }
				selector_t  const inline & gen_select_with()  const throw() { return gen_select_with_; }
				selector_t        inline & gen_select_with()        throw() { return gen_select_with_; }
				selector_t  const inline & inst_select_with() const throw() { return inst_select_with_; }
				selector_t        inline & inst_select_with()       throw() { return inst_select_with_; }

			public:
				int _midiHeadroom;
				bool _midiMachineViewSeqMode;

			private:
				groups_t         groups_;
				group_t          velocity_;
				group_t          pitch_;
				bool             raw_;
				selector_t       gen_select_with_;
				selector_t       inst_select_with_;
			};
			typedef enum {
				STORE_EXE_DIR = 0,
				STORE_USER_DATA,
				STORE_USER_REGEDIT,
				STORE_ALL_DATA,
				STORE_ALL_REGEDIT,
				STORE_TYPES
			} store_t;

		public:
			PsycleConfig();
			virtual ~PsycleConfig() throw();
			//Actions
			virtual void SetDefaultSettings(bool include_others);
			virtual void Load(ConfigStorage &);
			virtual void Save(ConfigStorage &);
			virtual void RefreshSettings();
			virtual void RefreshAudio();

      ConfigStorage* CreateStore();
			bool LoadPsycleSettings();
			bool SavePsycleSettings();
			void DeleteStorage(store_t store);

			void AddRecentFile(std::string const &f);
			void OutputChanged(int newidx);
			void MidiChanged(int newidx);
			void SyncChanged(int newidx);

			//Members
			std::string       inline   GetAbsoluteSongDir     () const { return AsAbsolute(song_dir_); }
			std::string const inline & GetSongDir             () const { return song_dir_; }
			               void inline SetSongDir             (std::string const &d) { song_dir_ = d; }
			std::string const inline & GetCurrentSongDir      () const { return current_song_dir_; }
			               void inline SetCurrentSongDir      (std::string const &d) { current_song_dir_ = d;}
			std::string       inline   GetAbsoluteWaveRecDir  () const { return AsAbsolute(wave_rec_dir_); }
			std::string const inline & GetWaveRecDir          () const { return wave_rec_dir_; }
			               void inline SetWaveRecDir          (std::string const &d) { wave_rec_dir_ = d;}
			std::string const inline & GetCurrentWaveRecDir   () const { return current_wave_rec_dir_; }
			               void inline SetCurrentWaveRecDir   (std::string const &d) { current_wave_rec_dir_ = d;}
			bool		const inline   IsRecInPSYDir          () const { return rec_in_psy_dir_; }
						   void inline SetRecInPSYDir         (bool rec) {	rec_in_psy_dir_ = rec; }
			std::string       inline   GetAbsoluteInstrumentDir() const { return AsAbsolute(instrument_dir_); }
			std::string const inline & GetInstrumentDir       () const { return instrument_dir_; }
			               void inline SetInstrumentDir       (std::string const &d) { instrument_dir_ = d;}
			std::string const inline & GetCurrentInstrumentDir() const { return current_instrument_dir_; }
			               void inline SetCurrentInstrumentDir(std::string const &d) { current_instrument_dir_ = d; }
			std::string       inline   GetAbsoluteSkinDir     () const { return AsAbsolute(skin_dir_); }
			std::string const inline & GetSkinDir             () const { return skin_dir_; }
			               void inline SetSkinDir             (std::string const &d) { skin_dir_ = d; }
			std::string       inline   GetAbsolutePresetsDir  () const { return AsAbsolute(presets_dir_); }
			std::string const inline & GetPresetsDir          () const { return presets_dir_; }
			               void inline SetPresetsDir          (std::string const &d){ presets_dir_ = d; }
			std::vector<std::string> const inline & GetRecentFiles() const { return recent_files_; }
			PatternView const inline & patView                () const { return patView_; }
			PatternView       inline & patView                ()       { return patView_; }
			MachineView const inline & macView                () const { return macView_; }
			MachineView       inline & macView                ()       { return macView_; }
			MachineParam const inline & macParam              () const { return macParam_; }
			MachineParam       inline & macParam              ()       { return macParam_; }
			Midi        const inline & midi                   () const { return midi_; }
			Midi              inline & midi                   ()       { return midi_; }
			InputHandler const inline & inputHandler          () const { return input_; }
			InputHandler       inline & inputHandler          ()       { return input_; }
			int numOutputDrivers () const { return _numOutputDrivers; };
			int outputDriverIndex() const { return _outputDriverIndex; };
			int midiDriverIndex  () const { return _midiDriverIndex; };
			int syncDriverIndex  () const { return _syncDriverIndex; };
		public:
			//On KeyConfigDlg
			bool _allowMultipleInstances;
			bool _showAboutAtStart;
			bool bShowSongInfoOnLoad;
			bool bFileSaveReminders;
			bool autosaveSong;
			int autosaveSongTime;
			store_t store_place_;
			//On SequencerBar
			bool _bShowPatternNames;
			bool _followSong;
			CFont fixedFont;
			HICON iconless;
			HICON iconlessless;
			HICON iconmore;
			HICON iconmoremore;
			HICON iconplus;
			HICON iconminus;

			//On OutputDlg
			std::vector<AudioDriverSettings*> audioSettings;

		protected:
			static void Error(std::string const &);
			static bool CreatePsyFont(CFont & f, std::string const & sFontFace, int const HeightPx, bool const bBold, bool const bItalic);
			static void PrepareMask(CBitmap* pBmpSource, CBitmap* pBmpMask, COLORREF clrTrans);
		private:
			//On OutputDlg
			int _numOutputDrivers;
			int _outputDriverIndex;
			int _midiDriverIndex;
			int _syncDriverIndex;

			//On DirectoryDlg
			std::string song_dir_;
			std::string current_song_dir_;
			bool rec_in_psy_dir_;
			std::string wave_rec_dir_;
			std::string current_wave_rec_dir_;
			std::string instrument_dir_;
			std::string current_instrument_dir_;
			std::string skin_dir_;
			std::string presets_dir_;
			//Recent file menu
			std::vector<std::string> recent_files_;
			
			//On SkinDlg
			PatternView patView_;
			MachineView macView_;
			MachineParam macParam_;
			//On KeyConfigDlg
			InputHandler input_;
			//On MidiInputDlg
			Midi  midi_;
		};
	}
}
