/////////////////////////////////////////////////////////////////////
// Skale Player v0.81
/////////////////////////////////////////////////////////////////////
//
//  This library contains the Skale Tracker Sound Engine. It plays
//  SKM, XM and MOD files. 
//
//  This library is freeware only for non-commercial projects. For 
//  commercial uses of this library you must to get my permision,
//  contact me at baktery@skale.org
//
//  Use it at your own risk.
//
//  Ruben Ramos Salvador aka Baktery/Chanka
//  email: baktery@skale.org
//  http://www.skale.org
//
/////////////////////////////////////////////////////////////////////


// WIN32
#ifdef _WIN32
#ifdef SKALEPLAYER_EXPORTS
#define SKALEPLAYER_API __declspec(dllexport)
#else
#define SKALEPLAYER_API __declspec(dllimport)
#endif
#endif


///////////////////////////////////////////////
// ISkalePlayer
///////////////////////////////////////////////
class ISkalePlayer
{
public:
  virtual ~ISkalePlayer () { }

  /////////////////////////
  // SkalePlayer Instance
  /////////////////////////
  // Skale Player Singleton Instance. 1 and only 1.
  SKALEPLAYER_API static ISkalePlayer*  GetSkalePlayer      ();

  /////////////////////////
  // Init/End
  /////////////////////////
#ifdef _WIN32
  // Windows Output Modes
  enum EOutputMode
  {
    INIT_MODE_WAVEOUT = 0,  // WaveOut
    INIT_MODE_DSOUND,       // DirectSound
    INIT_MODE_ASIO,         // ASIO
    INIT_MODE_NOSOUND,      // NoSound
    INIT_MODE_SLAVE,        // Slave mode
  };
#endif
  // InitFlags
  enum EInitFlags
  {
    INIT_FLAG_NONE = 0,     // None
  };
  // InitErrors
  enum EInitError
  {
    INIT_OK = 0,            // OK
    INIT_ERROR_UNDEFINED,   // Error
  };
  // InitData
  struct TInitData
  {
    TInitData() : m_eMode(INIT_MODE_WAVEOUT), m_iDevice(0), m_eFlags(INIT_FLAG_NONE), m_iSamplesPerSecond(44100) {}
      
    EOutputMode     m_eMode;              // Output Mode
    int             m_iDevice;            // Output Device number
    int             m_iSamplesPerSecond;  // SamplesPerSecond
    EInitFlags      m_eFlags;             // Flags
  };
  // Init SkalePlayer
  virtual EInitError    Init                (const TInitData& InitData) = 0;
  // Uninit SkalePlayer                           
  virtual void          End                 () = 0;


  /////////////////////////
  // ISong
  /////////////////////////
  class ISong  
  { 
  public: 
    virtual ~ISong() {}

    // Song Info
    struct TInfo
    {
      char  m_szTitle[256];         // Song Title
      int   m_iPatternListSize;     // Pattern list size
    };
    // Get Song Info.
    virtual void          GetInfo         (TInfo* pInfo) const = 0;
  };

  /////////////////////////
  // Load/Free/Set Songs
  /////////////////////////
  // Load Song from a File
  virtual const ISong*  LoadSongFromFile    (const char* pszFileName) = 0;
  // Load Song from Memory
  virtual const ISong*  LoadSongFromMemory  (void* pData, int iSize) = 0;
  // Free Song
  virtual void          FreeSong            (const ISong* pSong) = 0;
  // Set current Song for Playback
  virtual void          SetCurrentSong      (const ISong* pSong) = 0;


  /////////////////////////
  // Play Mode
  /////////////////////////
  enum EPlayMode
  {
    PLAY_MODE_PLAY_SONG_FROM_START = 0,       // Play song from start
    PLAY_MODE_PLAY_SONG_FROM_CURRENT_POS,     // Play song from current position
    PLAY_MODE_PLAY_PATTERN_FROM_START,        // Play patern from start
    PLAY_MODE_PLAY_PATTERN_FROM_CURRENT_POS,  // Play pattern from current position
    PLAY_MODE_STOP_PLAYBACK,                  // Stop playback
    PLAY_MODE_STOP_ENGINE,                    // Stop mixer engine
  };
  // Set Play Mode
  virtual void          SetPlayMode         (EPlayMode eMode = PLAY_MODE_PLAY_SONG_FROM_START) = 0;


  /////////////////////////
  // Info
  /////////////////////////
  struct TPlayInfo
  {
    int   m_iPatternListPos;      // Pattern list position
    int   m_iPattern;             // Current pattern
    int   m_iRow;                 // Current row
  };
  // Get Playback Info
  virtual void          GetPlayInfo         (TPlayInfo* pInfo) const = 0;


  /////////////////////////
  // Slave mode
  /////////////////////////
  // Get mixed output data (stereo, 1.f/-1.f), use this function only with INIT_MODE_SLAVE
  virtual void          SlaveProcess        (float* pOutputData, int iSamples) = 0;


  /////////////////////////
  // Events
  /////////////////////////
  class IEventHandler
  {
  public:
    virtual ~IEventHandler ()  {}

    // Events list
    enum EEvent
    {
      EVENT_ENDOFSONG = 0,      // End of song
    };
    struct TEventInfo { };

    virtual void      Event     (EEvent eEvent, const TEventInfo* pEventInfo) = 0;
  };
  // Sets the event callback
  virtual void          SetEventHandler     (IEventHandler* pEvent) = 0;

};


////////////////////////////////////////////////////
// EOF
