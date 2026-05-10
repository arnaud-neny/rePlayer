#ifndef _H_SOUNDENGINE
#define _H_SOUNDENGINE


/* Songs AND instruments are saved with a Mugiversion
// For Instruments:
// 1234 = v1.0

// For Songs:
// 3456 = v1.0
*/


//////////////////////////
//NOTES
//////////////////////////

//Internal waveforms have a fixed length of 256 samples.


//////////////////////////
//DEFINES
//////////////////////////

//beware of changing any of these values since they might influence existing musics.

#define SE_OVERLAP (100)			// number of bytes overlap to eliminate clicking and ticking
#define SE_MAXCHANS (16)			// MAX number of supported channels

#define SE_NROFFINETUNESTEPS (16)	// number of finetune scales


#define SE_NROFEFFECTS (18)			// number of available wave effects


//////////////////////////
//ENUMERATIONS
//////////////////////////

enum SE_PLAYMODE 
{
	SE_PM_SONG=0,
	SE_PM_PATTERN
};

//////////////////////////
//STRUCTURES
//////////////////////////


// effect is an structure which describes effects which can be performed on waveforms.
typedef struct effect effect;
struct effect {
	int			dsteffect;
	int			srceffect1;
	int			srceffect2;
	int			osceffect;
	int			effectvar1;
	int			effectvar2;
	int			effectspd;
	int			oscspd;
	int			effecttype;
	char		oscflg;
	char		reseteffect;
};

// inst is the structure which has the entire instrument definition.
typedef struct inst inst;
struct inst {
	short		mugiversion;
	char		instname[32];
	short		waveform;
	short		wavelength;
	short		mastervol;
	short		amwave;
	short		amspd;
	short		amlooppoint;
	short		finetune;
	short		fmwave;
	short		fmspd;
	short		fmlooppoint;
	short		fmdelay;
	short		arpeggio;
	char		resetwave[16];
	short		panwave;  
	short		panspd;
	short		panlooppoint;
	short		dummy4;		//for future stuff
	short		dummy5;
	short		dummy6;
	short		dummy7;
	short		dummy8;
	effect		fx[4];
	char		samplename[192]; // path naar de gebruikte sample (was _MAX_PATH lang... is nu getruncate naar 192)(in de toekomst nog kleiner?)
	int			ldummy1;
	int			ldummy2;
	int			ldummy3;
	int			ldummy4;
	int			ldummy5;
	int			ldummy6;
	int			ldummy7;
	int			ldummy8;
	int			ldummy9;
	int			ldummy10;
	int			ldummy11;
	int			ldummy12;
	short		ldummy13;
	short		sharing;	// sample sharing! sharing contains instr nr of shared sanpledata (0=no sharing)
	short		loopflg;    //does the sample loop or play one/shot? (0=1shot)
	short		bidirecflg; // does the sample loop birdirectional? (0=no)
	int			startpoint;
	int			looppoint;
	int			endpoint;
	unsigned char *sampledata;     // pointer naar de sample (mag 0 zijn)
	int			samplelength;	   // length of sample
	short		waves[4096];
};


// Chanfx is an internal structure which keeps track of the current effect parameters per active channel
typedef struct chanfx chanfx;
struct chanfx 
{
	int			fxcnt1;
	int			fxcnt2;
	int			osccnt;
	double		a0;
	double		b1;
	double		b2;
	double		y1;
	double		y2;
	int			Vhp;
	int			Vbp;
	int			Vlp;
};

// chandat is an internal structure which keeps track of the current instruemtns current variables per active channel
typedef struct chandat chandat;
struct chandat 
{
	int			songpos;
	int			patpos;
	int			instrument;
	int			volcnt;
	int			pancnt;
	int			arpcnt;
	int			curnote;
	int			curfreq;
	int			curvol;
	int			curpan;
	int			bendadd;	// for the pitchbend
	int			destfreq;	//...
	int			bendspd;	// ...
	int			bendtonote;
	int			freqcnt;
	int			freqdel;
	unsigned char *sampledata;
	int			looppoint;
	int			endpoint;
	short		loopflg;
	short		bidirecflg;
	short		curdirecflg;
	int			samplepos;
	int			lastplaypos;
	chanfx		fx[4];
	short		waves[4096];
};


// Patitem is the basic element out of which a pattern consists
typedef struct patitem patitem;
struct patitem 
{
	unsigned char	srcnote;
	unsigned char	dstnote;
	unsigned char	inst;
	signed char		param;
	unsigned char	script;

#ifdef __SYMBIAN32__
} __attribute__((__packed__));
#else
};
#endif

// the songitem structure is the basic element out of which a song consists 
typedef struct subsong subsong;
typedef struct songitem songitem;
struct songitem 
{
	short		patnr;		// welk pattern spelen...
	short		patlen;		// 0/16/32/48
};

//the subsong structure describes a complete song
struct subsong
{
	songitem *songchans[SE_MAXCHANS];
	char		mute[SE_MAXCHANS];   // which channels are muted? (1=muted)
	int			songspd;	// delay tussen de pattern-stepjes
	int			groove;		// groove value... 0=nothing, 1 = swing, 2=shuffle
	int			songpos;	// waar start song? (welke maat?)
	int			songstep;	// welke patternpos offset? (1/64 van maat)
	int			endpos;		// waar stopt song? (welke maat?)
	int			endstep;	// welke patternpos offset? (1/64 van maat)
	int			looppos;	// waar looped song? (welke maat?)
	int			loopstep;	// welke patternpos offset? (1/64 van maat)
	short		songloop;	// if true, the song loops inbetween looppos and endpos
	char		name[32];   // name of subsong
	short		nrofchans;	//nr of channels used
	unsigned short delaytime; // the delaytime (for the echo effect)
	unsigned char  delayamount[SE_MAXCHANS]; // amount per channel for the echo-effect
	short		amplification; //extra amplification factor (20 to 1000)
//	short		dummy2;		//dummys for future expansion
//	short		dummy3;		//dummys for future expansion
//	short		dummy4;		//dummys for future expansion
//	short		dummy5;		//dummys for future expansion
//	short		dummy6;		//dummys for future expansion
//	short		dummy7;		//dummys for future expansion
//	short		dummy8;		//dummys for future expansion
//	short		dummy9;		//dummys for future expansion
//	short		dummy10;	//dummys for future expansion
//	short		dummy11;	//dummys for future expansion
	short		dummy12;	//dummys for future expansion
	short		dummy13;	//dummys for future expansion
	short		dummy14;	//dummys for future expansion
	short		dummy15;	//dummys for future expansion
	short		dummy16;	//dummys for future expansion
};






// the song sturcture describes all the subsongs which are contained in the entire song
struct song
{
	short		mugiversion;//version of mugician this song was saved with
	int			nrofpats;	//aantal patterns beschikbaar
	int			nrofsongs;	//aantal beschikbare subsongs
	int			nrofinst;	//aantal gebruikte instruments
	int			dummy0;		//dummys for future expansion
	short		dummy1;		//dummys for future expansion
	short		dummy2;		//dummys for future expansion
	short		dummy3;		//dummys for future expansion
	short		dummy4;		//dummys for future expansion
	short		dummy5;		//dummys for future expansion
	short		dummy6;		//dummys for future expansion
	short		dummy7;		//dummys for future expansion
	short		dummy8;		//dummys for future expansion
	short		dummy9;		//dummys for future expansion
	short		dummy10;	//dummys for future expansion
	short		dummy11;	//dummys for future expansion
	short		dummy12;	//dummys for future expansion
	short		dummy13;	//dummys for future expansion
	short		dummy14;	//dummys for future expansion
	short		dummy15;	//dummys for future expansion
	short		dummy16;	//dummys for future expansion
};



//////////////////////////
//SoundEngine Class
//////////////////////////

class SoundEngine
{
public:
	SoundEngine();
	~SoundEngine();

	//Initialisation
	int		Allocate();
	void	DeAllocate();

	//Song management
	void	CreateEmptySong(void);
	int		LoadSongFromMemory(unsigned  char *Module);
	void	FreeSong(void);

	//Playback controls
	void	PlaySubSong(int subsongnr);		// Prepare playback for the indicated subsong
	void	PlayPattern(int PatternNumber);	// Playback only 1 pattern
	void	StopSong(void);
	void	PauseSong(void);
	void	ContinueSong(void);

	//Audiobuffers control
	void	ClearSoundBuffers(void);
	void	RenderBuffer( void *bufptr, int nrofsamples, int frequency, int routine, int nrofchannels);

	//Instrument control (useful for soundfx)
	void	PlayInstrument(int channr, int inst, int note);


	//private?
//private:
	// RenderCore Methods
	int		RenderChannels1(short *renderbuf, int nrofsamples, int frequency);
	void	RenderChannels2(short *renderbuf, int nrofsamples, int frequency);
	void	RenderChannels3(short *renderbuf, int nrofsamples, int frequency);
	void	RenderChannels1Mono(short *renderbuf, int nrofsamples, int frequency);
	void	RenderChannels2Mono(short *renderbuf, int nrofsamples, int frequency);
	void	RenderChannels3Mono(short *renderbuf, int nrofsamples, int frequency);
private:
	// SoundEngine methods
	void	PreCalcFinetune(void);			// Calculate fast lookup tables for frequency/finetune tables

	void	AdvanceSong(void);				// Advance the song to the next position and call all the handlers
	void	HandleSong(void);				// Update the song paramaters
	void	HandlePattern(int channr);		// Parse the current pattern position and spawn instruments/effects
	void	HandleInstrument(int channr);	// Calculate/update all the instrument related things on the given channel
	void	HandleEffects(int channr);		// Calculate all current effects on the given channel
	void	HandleScript(int f,int s, int d, int p, int channr); // parse and execute pattern scriptcommands


	//stuff accessible by the editor
public:
	// replayer vars
	char		m_MuteTab[SE_MAXCHANS];		// Contains info which channels are muted or not
	chandat		*m_ChannelData[SE_MAXCHANS];	// Current variables needed per channel
	short		m_TimeSpd;			        // Sample accurate counter which indicates every how many samples the song should progress 1 tick. Is dependant on rendering frequency and BPM
	int			m_PatternLength;			// Current length of a pattern (in pattern play mode)
	int			m_PatternOffset;				// Current play offset in the pattern (used for display)
	int			m_PlayPosition;				// Current position in song (coarse)
	int			m_PlayStep;					// Current position in song (fine)
	int			m_DisplayChannel;			// Last active channel (used for displaying realtime waves)

	// loaded song
	song		m_SongPack;					// Contains info about the complete songpack
	subsong   **m_Songs;					// Contains all loaded subsongs
	inst      **m_Instruments;				// Contains all instruments currently loaded
	patitem    *m_Patterns;					// Contains all patterndata in a sequential order
	char      **m_PatternNames;				// Pointer to list of patternname pointers
	char       *m_Arpeggios;				// Contains the arpeggio data of the loaded song
	short	   *m_Presets;					// Contains all preset waves

	// Play variables
	int			m_CurrentPattern;			// Which pattern are we currently playing (In pattern play mode)
	int			m_CurrentSubsong;			// Which subsong are we playing (In song play mode)
	int			m_PlayFlg;					// 0 if playback is stopped, 1 if song is being played
	int			m_PauseFlg;					// 0 if playback is not paused, 1 if playback is paused
	int			m_PatternDelay;				// Current delay in between notes (resets with m_PlaySpeed)
	int			m_PlaySpeed;				// Actual delay in between notes
	SE_PLAYMODE	m_PlayMode;					// in which mode is the replayer? Song or patternmode?
	int			m_MasterVolume;				// Mastervolume of the replayer (256=max - 0=min)
	int			m_NrOfChannels;				// The current number of channels the replayer is configured with(depends on the subsong)

	// Editor Helper things
	inst	   *m_DefaultInst;				// Default instrument template used for creating new instruments
	patitem    *m_CopyPatternBuf;			// Pattern buffer initialized with defaults used for copy buffer



private:
	short      *m_EmptyWave;				// Buffer which holds an 'empty' waveform
	int		  **m_FrequencyTable;			// 2 dimensional Table with all scale frequencies with finetuning
	unsigned short	m_DelayCnt;				// Internal counter used for delay
	short      *m_LeftDelayBuffer;			// buffer to simulate an echo on the left stereo channel
	short      *m_RightDelayBuffer;			// buffer to simulate an echo on the right stereo channel
	unsigned short  m_WavePosition[SE_MAXCHANS];	// Last rendering position in the waveform (8bit fixedpoint)
	short		m_TimeCnt;					// Samplecounter which stores the njumber of samples before the next songparams are calculated (is reinited with m_TimeSpd)

	//used for renderingcore
	short		m_OverlapBuffer[SE_OVERLAP*2];	// Buffer which stores overlap between waveforms to avoid clicks
	short		m_OverlapCnt;				// Used to store how much overlap we have already rendered

	short		m_HasLooped;				// rePlayer
};



#endif //_H_SOUNDENGINE