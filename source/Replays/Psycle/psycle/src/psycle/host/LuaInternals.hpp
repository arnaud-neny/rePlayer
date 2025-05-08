// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include <psycle/host/detail/project.hpp>
#include "plugininfo.hpp"
#include "LuaArray.hpp"
#include "XMSampler.hpp"
#include <psycle/helpers/resampler.hpp>
#include "LuaHelper.hpp"
#include "InputHandler.hpp"
#include "MidiInput.hpp"
#include "Song.hpp"


namespace psycle { 
namespace host {
namespace ui { 
	class Viewport; 
}
}
}

namespace psycle { namespace host {

class ConfigStorage;
class LuaMachine;

class LuaCmdDefBind {
 public:
  static int open(lua_State* L);
  static int keytocmd(lua_State* L);
  static int cmdtokey(lua_State* L); 
  static int currentoctave(lua_State* L);
};

class LuaActionListener : public ActionListener,  public LuaState {
 public:
  LuaActionListener(lua_State* L) : ActionListener(), LuaState(L), mac_(0) {}
  virtual void OnNotify(const ActionHandler&, ActionType action);
  void setmac(LuaMachine* mac) { mac_ = mac; }

 private:  
  LuaMachine* mac_;
};


struct LuaActionListenerBind {
	static const char* meta;
	static int open(lua_State *L);
	static int create(lua_State *L);
	static int gc(lua_State* L);
};
  
  class LuaConfig {
    public:
     LuaConfig();
     LuaConfig(const std::string& group);

     ConfigStorage* store() { return store_.get(); }
     void OpenGroup(const std::string& group);
     void CloseGroup();
     uint32_t color(const std::string& key);

    private:
     std::auto_ptr<ConfigStorage> store_;
  };
      
  struct LuaStockBind {
    static int open(lua_State *L);      
    static int value(lua_State* L);
    static int key(lua_State* L);    
  };

  struct LuaConfigBind {
    static int open(lua_State *L);
    static const char* meta;
  private:
    static int create(lua_State* L);
    static int get(lua_State* L);
    static int gc(lua_State* L);
    static int opengroup(lua_State* L);
    static int closegroup(lua_State* L);
    static int groups(lua_State* L);
    static int keys(lua_State* L);
    static int octave(lua_State* L);
    static int keytocmd(lua_State* L);
    static int plugindir(lua_State* L);
    static int presetdir(lua_State* L);    
  };

  struct LuaPluginInfoBind {
    static const char* meta;
    static int open(lua_State *L);    
    static int create(lua_State* L);
    static int gc(lua_State* L);    
    static int dllname(lua_State* L);
    static int name(lua_State* L);
    static int type(lua_State* L);
    static int mode(lua_State* L);
    static int version(lua_State* L);
    static int apiversion(lua_State* L);
    static int description(lua_State* L);
    static int identifier(lua_State* L);
    static int error(lua_State* L);
    static int vendor(lua_State* L);
    static int allow(lua_State* L);
  };

  struct LuaPluginCatcherBind {
    static const char* meta;
    static int open(lua_State *L);    
    static int create(lua_State* L);    
    static int info(lua_State* L);
    static int infos(lua_State* L);
    static int gc(lua_State* L);
    static int rescanall(lua_State* L);
    static int rescannew(lua_State* L);
  };

  class Machine;

  struct MachinePresetType {
    enum Value {
      NATIVE = 0,
      CHUNK = 1
    };
  };
  
  class LuaMachine {
  public:
	typedef boost::shared_ptr<LuaMachine> Ptr;
	typedef boost::shared_ptr<const LuaMachine> ConstPtr;
	typedef boost::weak_ptr<LuaMachine> WeakPtr;
    LuaMachine(lua_State* L);    
    ~LuaMachine();
    bool shared() const { return shared_; }
    void set_shared(bool shared) { shared_ = shared; }
    void lock() const;
    void unlock() const;
    void load(const char* name);
    void work(int samples);
    Machine* mac() { return mac_; }
    void set_mac(Machine* mac) { mac_ = mac; shared_=true; }
	int index() const { return mac_ ? mac_->_macIndex : -1; }
    PSArray* channel(int idx) { return &sampleV_[idx]; }
    void update_num_samples(int num);
    void offset(int offset);
    void build_buffer(std::vector<float*>& buf, int num);
    void set_buffer(std::vector<float*>& buf);
    psybuffer& samples() { return sampleV_; }
    void set_numparams(int num) { num_parameter_ = num;  }
    int numparams() const { return num_parameter_; }
    void set_numcols(int num) { num_cols_ = num; }
    int numcols() const { return num_cols_; }
    void set_numprograms(int num) { num_programs_ = num; }
    int numprograms() const { return num_programs_; }
    int numchannels() const { return sampleV_.size(); }
    MachineUiType::Value ui_type() const { return ui_type_; }
    void set_ui_type(MachineUiType::Value ui_type) { ui_type_ = ui_type; }
    void set_viewport(const boost::weak_ptr<ui::Viewport>& viewport);
    void ToggleViewPort();
    boost::weak_ptr<ui::Viewport> viewport() { return viewport_; }
    MachinePresetType::Value prsmode() const { return prsmode_; }
    void setprsmode(MachinePresetType::Value prsmode) { prsmode_ = prsmode; }
    void setproxy(class LuaProxy* proxy) { proxy_ = proxy; }	
    LuaProxy* proxy() { return proxy_; }
    void doexit();
    void reload();    
    void set_title(const std::string& title);    
    std::string title() const { return title_; }

	// virtual void OnMachineCreate(Machine& machine);
	// virtual void BeforeMachineDelete(Machine& machine);

  private:
    //LuaMachine(LuaMachine&)  {}
    //LuaMachine& operator=(LuaMachine) {}
    Machine* mac_;
	lua_State* L;
    psybuffer sampleV_;
    bool shared_;
    int num_parameter_, num_cols_, num_programs_;
    MachineUiType::Value ui_type_;    
    MachinePresetType::Value prsmode_;
    LuaProxy* proxy_; 
    boost::weak_ptr<ui::Viewport> viewport_;
    std::string title_;
  };
 
 struct LuaScopeMemoryBind {
    static int open(lua_State *L);
    static const char* meta;
    static int create(lua_State* L);
    static int gc(lua_State* L);
	static int channel(lua_State* L);	
};

  struct LuaMachineBind {
    static int open(lua_State *L);
    static const char* meta;
    static int create(lua_State* L);
    static int gc(lua_State* L);
    static int work(lua_State* L);
    static int tick(lua_State* L);
    static int channel(lua_State* L);
    static int resize(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::update_num_samples); }    
    static int setbuffer(lua_State* L);
    static int setnorm(lua_State* L);
    static int getnorm(lua_State* L);
    static int dllname(lua_State* L);
    static int label(lua_State* L);
    static int display(lua_State* L);
    static int getrange(lua_State* L);
    static int add_parameters(lua_State* L);
    static int set_parameters(lua_State* L);    
    static int setmenurootnode(lua_State* L);
    static int addhostlistener(lua_State* L);
    static int set_numchannels(lua_State* L);
    static int numchannels(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::numchannels); }
    static int set_numprograms(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::numprograms); }
    static int numprograms(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::numprograms); }
    static int set_numcols(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::set_numcols); }
    static int numcols(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::numcols); }
    static int show_native_gui(lua_State* L);
    static int show_custom_gui(lua_State* L);
    static int show_childview_gui(lua_State* L);
    static int getparam(lua_State* L);
    static int setpresetmode(lua_State* L);
    static int exit(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::doexit); }
    static int reload(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::reload); }
    static int setviewport(lua_State* L);
    static int toggleviewport(lua_State* L);	
    static int type(lua_State* L);
    static int info2(lua_State* L);
    static int name(lua_State* L);
    static int pluginname(lua_State* L);
    static int settitle(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::set_title); }
    static int settimeout(lua_State* L);  
    static int setinterval(lua_State* L);
    static int cleartimer(lua_State* L);
	static int automate(lua_State* L);
    static class LuaRun* createtimercallback(lua_State* L);
	static int createparams(lua_State* L, LuaMachine* machine);
	static int createparam(lua_State* L, int idx, LuaMachine* machine);
	static int insert(lua_State* L);
    static int at(lua_State* L);
	static int paramrange(lua_State* L);
	static int paramname(lua_State* L);
    static int muted(lua_State* L);
	static int buildbuffer(lua_State* L);
	static int addscopememory(lua_State* L);
	static int index(lua_State* L) { LUAEXPORTM(L, meta, &LuaMachine::index); }   
	static int audiorange(lua_State* L);
  };
  /*
  class LuaMachines : public MachineListener, public LuaState {
	public:
		LuaMachines() : LuaState(0) {}
		LuaMachines(lua_State* L) : LuaState(L) {}

	protected:		
		virtual void OnMachineCreate(Machine& machine);
		virtual void BeforeMachineDelete(Machine& machine);
  };

  struct LuaMachinesBind {
    static int open(lua_State *L);
    static const char* meta;  
    static int create(lua_State* L);
    static int gc(lua_State* L);
    static int insert(lua_State* L);
    static int at(lua_State* L);
    static int master(lua_State* L);
    static int muted(lua_State* L);        
  };
  */
  struct LuaSetting {
    LuaSetting(Machine* machine) : machine_(machine) {}
   private:
    Machine* machine_;
  };

  struct LuaSettingBind {
    static int open(lua_State *L);
    static const char* meta;  
    static int create(lua_State* L);
    static int gc(lua_State* L);        
  };

  struct LuaPlayerBind {
    static const char* meta;
    static int open(lua_State *L);    
    static int create(lua_State* L);
    static int gc(lua_State* L);
    static int samplerate(lua_State* L);
    static int tpb(lua_State* L);
	static int bpt(lua_State* L);
    static int spt(lua_State* L);
    static int line(lua_State* L);
    static int rline(lua_State* L);
    static int playing(lua_State* L);    
	static int playpattern(lua_State* L);
	static int setplayline(lua_State* L);
	static int setplayposition(lua_State* L);
	static int playnote(lua_State* L);
	static int stopnote(lua_State* L);
	static int toggletrackarmed(lua_State* L);
	static int istrackarmed(lua_State* L);
	static int toggletrackmuted(lua_State* L);
	static int istrackmuted(lua_State* L);
	static int toggletracksoloed(lua_State* L);
	static int istracksoloed(lua_State* L);
	static int playorder(lua_State* L);
	static int playposition(lua_State* L);	
  };

  struct LuaPatternEvent {
    PatternEntry entry;
    int len;
    int pos;
    bool has_off;
  };

  class LuaPatternData {
  public:
    LuaPatternData(lua_State* state, unsigned char** data, Song* song)
      : L(state), data_(data), song_(song) {}
    unsigned char** data() { return data_; }
    inline unsigned char * ptrackline(int ps, int track, int line) {
	  if (!has_pattern(ps)) {
	    std::stringstream str;
		str << "Index out of bound error. No pattern at pos " << ps << ".";
		std::string tmp(str.str());
	    throw std::runtime_error(tmp.c_str());
	  }
	  return data_[ps] + (track*EVENT_SIZE) + (line*MULTIPLY);
	}
    int numlines(int ps) const;
    int numtracks() const;
	bool has_pattern(int ps) const;
	bool IsTrackEmpty(int ps, int trk) const;
   private:
     lua_State* L;
     unsigned char** data_;
     Song* song_;
  };

  class Tweak {
	public:
		Tweak() {}
		~Tweak() {}
		virtual void OnAutomate(int macIdx, int param, int value, bool undo,
		    const ParamTranslator& translator, int track, int line) {
		}
  };

	class LuaTweak : public Tweak {
		public:
			LuaTweak(lua_State* L) : L(L) {}
			~LuaTweak() {}
  
			virtual void OnAutomate(int macIdx, int param, int value, bool undo,
			    const ParamTranslator& translator, int track, int line);
				
		private:
			lua_State* L;
	};

	struct LuaTweakBind {
		static const char* meta;
		static int open(lua_State *L);
		static int create(lua_State* L);
		static int gc(lua_State* L);
	};

  struct LuaPatternDataBind {
    static int open(lua_State *L);
    static int create(lua_State* L);
    static int gc(lua_State* L);
    static int pattern(lua_State* L);
    static int settrack(lua_State* L);
    static int track(lua_State* L);
	static int line(lua_State* L);
    static int numtracks(lua_State *L) { LUAEXPORTM(L, meta, &LuaPatternData::numtracks); }
    static int numlines(lua_State *L) { LUAEXPORTM(L, meta, &LuaPatternData::numlines); }
    static int eventat(lua_State* L);
    static int setevent(lua_State* L);
    static int buildevent(lua_State* L);    
    static int createevent(lua_State* L, LuaPatternEvent& ev);
    static int getevent(lua_State* L, int idx, LuaPatternEvent& ev);
	static int istrackempty(lua_State* L) { LUAEXPORTM(L, meta, &LuaPatternData::IsTrackEmpty); }
	static int patstep(lua_State* L);
	static int editquantizechange(lua_State* L);
	static int deleterow(lua_State* L);
	static int clearrow(lua_State* L);
	static int insertrow(lua_State* L);
	static int interpolate(lua_State* L);
	static int interpolatecurve(lua_State* L);
	static int loadblock(lua_State* L);
	static int addundo(lua_State* L);
	static int undo(lua_State* L);
	static int redo(lua_State* L);	
	static int movecursorpaste(lua_State* L);
	static int settrackedit(lua_State* L);
	static int settrackline(lua_State* L);
	static const char* meta;
  };

  struct LuaSequenceBarBind {
    static int open(lua_State *L);
    static int create(lua_State* L);
	static int gc(lua_State* L);
    static int currpattern(lua_State* L);
	static int editposition(lua_State* L);
	static int followsong(lua_State* L);
    static const char* meta;
  };

  struct LuaMachineBarBind {    
    static int open(lua_State *L);
    static int create(lua_State* L);
	static int gc(lua_State* L);
    static int currmachine(lua_State* L);
	static int currinst(lua_State* L);
	static int curraux(lua_State* L);
	static const char* meta; 
  };

  template <typename T>
  class range {
  public:
    typedef T value_type;
    range(T const& center) : min_(center), max_(center) {}
    range(T const& min, T const& max) : min_(min), max_(max) {}
    T min() const { return min_; }
    T max() const { return max_; }
  private:
    T min_, max_;
  };

  template <typename T>
  struct left_of_range : public std::binary_function<range<T>, range<T>, bool> {
    bool operator()(range<T> const & lhs, range<T> const & rhs) const {
      return lhs.min() < rhs.min()
        && lhs.max() <= rhs.min();
    }
  };

  struct SingleWorkInterface {
    virtual ~SingleWorkInterface() {}
    virtual PSArray* work(int numSamples, int val) { return 0; }
  };

  template<class T>
  struct WaveList {
    typedef std::map<range<double>,
      const XMInstrument::WaveData<T>*,
      left_of_range<double> > Type;
  };

  struct RWInterface {
    virtual ~RWInterface() {}
    virtual int work(int numSamples, float* pSamplesL, float* pSamplesR,
         SingleWorkInterface* master) = 0 {
      return 0;
    }
    virtual bool Playing() const { return false; }
    virtual void NoteOff() {}
    virtual void Start(double phase=0) {}
    virtual void set_frequency(double f) {}
    virtual double frequency() const { return 0; }
    virtual void set_quality(helpers::dsp::resampler::quality::type quality) {}
    virtual helpers::dsp::resampler::quality::type quality() const {
      return helpers::dsp::resampler::quality::linear;
    }
    virtual void Stop(double phase) {}
    virtual void SetData(WaveList<float>::Type& data) {}
    virtual void set_gain(float gain) { }
    virtual float gain() const { return 1.0; }
    virtual double phase() const { return 0; }
    virtual void setphase(double phase) {}
    virtual void set_sync_fadeout_size(int size) {}
    virtual void set_pw(float width) {} // set pulsewidth
    virtual float pw() const { return 0.0; }
    virtual void setpm(float* ptr) {}
    virtual void setfm(float* ptr) {}
    virtual void setam(float* ptr) {}
    virtual void setpwm(float* ptr) {}
  };

  template <class T, int VOL>
  class ResamplerWrap : public RWInterface {
  public:
    ResamplerWrap(const typename WaveList<T>::Type& waves) :
        adjust_vol(1/(float)VOL),
          dostop_(false),
          waves_(waves),
          speed_(1.0),
          f_(261.63),
          basef(440),
          last_wave(0),
          gain_(1.0),
          last_sgn(1),
          startfadeout(false),
          fadeout_sc(0),
          fsize(32),
          lpos_(0),
          lph_(0),
          oldtrigger(0) {
            fm_ = am_ = pm_ = 0;
            resampler.quality(helpers::dsp::resampler::quality::linear);
            set_frequency(f_);
            wavectrl.Playing(false);
            wave_it = waves_.find(range<double>(f_));
            last_wave = wave_it != waves_.end() ? wave_it->second : 0;
        }
        void Start(double phase=0);
        virtual int work(int numSamples, float* pSamplesL, float* pSamplesR,
             SingleWorkInterface* master);
        bool Playing() const { return wavectrl.Playing();  }
        void Stop(double phase) {
          if (wave_it != waves_.end()) {
            double curr_phase = wavectrl.Position() /
              (double) wave_it->second->WaveLength();
            if (curr_phase == phase) {
              wavectrl.Playing(false);
            } else {
              dostop_ = true;
              stopphase_ = phase;
            }
          } else {
            wavectrl.Playing(false);
          }
        }
        void NoteOff() { if (wavectrl.Playing()) wavectrl.NoteOff(); }
        void set_frequency(double f);
        double frequency() const { return f_; }
        void set_quality(helpers::dsp::resampler::quality::type quality) {
          resampler.quality(quality);
        }
        helpers::dsp::resampler::quality::type quality() const {
          return resampler.quality();
        }
        virtual void SetData(WaveList<float>::Type& data) {
          assert(sizeof(T)!=sizeof(int16_t));
          waves_ = reinterpret_cast<typename WaveList<T>::Type&>(data);
          wave_it = waves_.find(range<double>(f_));
        }
        void set_gain(float gain) { gain_ = gain; }
        float gain() const { return gain_; }
        void set_sync_fadeout_size(int size) { fsize = size; }
        virtual double phase() const {
          if (wave_it != waves_.end() && wavectrl.Playing()) {
            return wavectrl.Position2() / (double) wave_it->second->WaveLength();
          }
          return 0;
        }
        virtual void setphase(double phase) {
          if (wave_it != waves_.end() && wavectrl.Playing()) {
            while (phase >=1) {phase--;}
            while (phase<0) {phase++;}
            wavectrl.Position2((int) (phase * wave_it->second->WaveLength()));
          }
        }
       virtual void setpm(float* ptr) { pm_ = ptr; }
       virtual void setfm(float* ptr) { fm_ = ptr; }
       virtual void setam(float* ptr) { am_ = ptr; }
  protected:
    virtual void check_wave(double f);
    int next_trigger(int numSamples, float* trigger);
    virtual int work_trigger(int numSamples,
      float* pSamplesL,
      float* pSamplesR,
      float* fm,
      float* env,
      float* phase,
      SingleWorkInterface* master);
    virtual int work_samples(int numSamples,
      float* pSamplesL,
      float* pSamplesR,
      float* fm,
      float* env,
      float* phase);
    psycle::helpers::dsp::cubic_resampler resampler;
    float adjust_vol;
    ULARGE_INTEGER m_Position;
    XMSampler::WaveDataController<T> wavectrl;
    typename WaveList<T>::Type waves_;
    typename WaveList<T>::Type::iterator wave_it;
    const XMInstrument::WaveData<T>* last_wave;
    double speed_;
    double basef;
    double f_;
    double dostop_;
    double stopphase_;
    float gain_;
    int last_sgn;
    bool startfadeout;
    int fadeout_sc;
    int fsize;
    float* oldtrigger;
    int n;
    double lpos_;
    double lph_;
    float *pm_, *fm_, *am_;
  };

  // Pulse Width Modulation
  template <class T, int VOL>
  class PWMWrap : public ResamplerWrap<T, VOL> {
  public:
    PWMWrap();
    virtual void Start(double phase=0);
    virtual void set_frequency(double f);
    virtual void SetData(WaveList<float>::Type& data) {
      assert(sizeof(T)!=sizeof(int16_t));
      waves_ = reinterpret_cast<typename WaveList<T>::Type&>(data);
      wave_it = waves_.find(range<double>(f_));
      wave_it2 = waves_.find(range<double>(f_));
    }
    virtual void setphase(double phase) {
      if (wave_it != waves_.end() && wavectrl.Playing()) {
        wavectrl.Position((int) (phase * wave_it->second->WaveLength()));
      }
    }
    virtual void set_pw(float width) { pw_ = width*0.5; Start(phase()); }
    virtual float pw() const { return pw_*2; }
    virtual void setpwm(float* ptr) { pwm_ = ptr; }
  protected:
    virtual int work_trigger(int numSamples,
      float* pSamplesL,
      float* pSamplesR,
      float* fm,
      float* env,
      float* phase,
      SingleWorkInterface* master);
    virtual void check_wave(double f);
    virtual int work_samples(int numSamples,
      float* pSamplesL,
      float* pSamplesR,
      float* fm,
      float* env,
      float* phase);
  private:
    XMSampler::WaveDataController<T> wavectrl2;
    typename WaveList<T>::Type::iterator wave_it2;
    const XMInstrument::WaveData<T>* last_wave2;
    float pw_;
    float *pwm_;
  };

  // used by hardsinc
  struct LuaSingleWorker : public SingleWorkInterface {
    LuaSingleWorker(lua_State *s) : L(s) {}
    ~LuaSingleWorker() {}
    virtual PSArray* work(int numSamples, int val);
  private:
    lua_State *L;
  };

  struct LuaResamplerBind {
    static int open(lua_State *L);
    static const char* meta;
  private:
    static int create(lua_State *L);
    static int createwavetable(lua_State *L);
    static int gc(lua_State* L);
    static int work(lua_State* L);
    static int noteoff(lua_State* L) { LUAEXPORTM(L, meta, &RWInterface::NoteOff); }
    static int isplaying(lua_State* L) { LUAEXPORTM(L, meta, &RWInterface::Playing); }
    static int start(lua_State* L);
    static int stop(lua_State* L);
    static int set_frequency(lua_State* L) { LUAEXPORTM(L, meta, &RWInterface::set_frequency); }
    static int frequency(lua_State* L) { LUAEXPORTM(L, meta, &RWInterface::frequency); }
    static int set_wave_data(lua_State* L);
    static int set_quality(lua_State* L);
    static int quality(lua_State*);
    static int set_sync(lua_State*);
    static int set_sync_fadeout(lua_State* L) { LUAEXPORTM(L, meta, &RWInterface::set_sync_fadeout_size); }
    static int phase(lua_State* L) { LUAEXPORTM(L, meta, &RWInterface::phase); }
    static int setphase(lua_State* L) { LUAEXPORTM(L, meta, &RWInterface::setphase); }
    static int setpm(lua_State*);
    static int setam(lua_State*);
    static int setfm(lua_State*);
    static WaveList<float>::Type check_wavelist(lua_State* L);
  };

#if 0//!defined WINAMP_PLUGIN
  struct LuaPlotterBind {
    static int open(lua_State *L);
  private:
    static int create(lua_State* L);
    static int gc(lua_State* L);
    static int stem(lua_State* L);
  };
#endif // #if !defined WINAMP_PLUGIN

  struct PSDelay {
    PSDelay(int k) : mem(k, 0) {}
    PSArray mem;
    void work(PSArray& x, PSArray& y);
  };

  struct LuaDelayBind {
    static int open(lua_State *L);
    static const char* meta;
  private:
    static int create(lua_State *L);
    static int work(lua_State* L);
    static int tostring(lua_State* L);
    static int gc(lua_State* L);
  };

  struct WaveOscTables {
    enum Shape {
      SIN = 1,
      SAW = 2,
      SQR = 3,
      TRI = 4,
      PWM = 5,
      RND = 6
    };
  private:
    WaveOscTables();
    ~WaveOscTables() {  //  AtExit ctor (invoked at application ending)
      cleartbl(sin_tbl);
      cleartbl(tri_tbl);
      cleartbl(sqr_tbl);
      cleartbl(saw_tbl);
    }
  public:
    static WaveOscTables *getInstance() {  // Singleton instance
      static WaveOscTables theInstance;
      return &theInstance;
    }
    WaveList<float>::Type tbl(Shape shape) {
      WaveList<float>::Type list;
      switch (shape) {
      case SIN : list = getInstance()->sin_tbl; break;
      case SAW : list = getInstance()->saw_tbl; break;
      case SQR : list = getInstance()->sqr_tbl; break;
      case TRI : list = getInstance()->tri_tbl; break;
      default: list = sin_tbl;
      }
      return list;
    }
    void set_samplerate(int rate);
  private:
    static void saw(float* data, int num, int maxharmonic);
    static void sqr(float* data, int num, int maxharmonic);
    static void sin(float* data, int num, int maxharmonic);
    static void tri(float* data, int num, int maxharmonic);
    static void ConstructWave(double fh,  XMInstrument::WaveData<float>& wave,
      void (*func)(float*, int, int),
      int sr);
    static void cleartbl(WaveList<float>::Type&);
    WaveList<float>::Type sin_tbl, saw_tbl, sqr_tbl, tri_tbl, rnd_tbl;
  };

  struct WaveOsc {
    WaveOsc(WaveOscTables::Shape shape);
    void work(int num, float* data, SingleWorkInterface* master);
    float base_frequency() const { return resampler_->frequency(); }
    void set_frequency(float f) { resampler_->set_frequency(f); }
    void setpw(float pw) { resampler_->set_pw(pw); }
    float pw() const { return resampler_->pw(); }
    void setphase(double phase) { resampler_->setphase(phase); }
    double phase() const { return resampler_->phase(); }
    void Start(double phase) { resampler_->Start(phase); }
    void Stop(double phase) { resampler_->Stop(phase); }
    bool IsPlaying() const { return resampler_->Playing(); }
    void set_gain(float gain) { resampler_->set_gain(gain); }
    float gain() const { return resampler_->gain(); }
    void set_shape(WaveOscTables::Shape shape);
    void set_shape(double shape) {set_shape((WaveOscTables::Shape) (int)shape); }
    WaveOscTables::Shape shape() const { return shape_; }
    void set_quality(helpers::dsp::resampler::quality::type quality) {
      resampler_->set_quality(quality);
    }
    helpers::dsp::resampler::quality::type quality() const {
      return resampler_->quality();
    }
    RWInterface* resampler() { return resampler_.get(); }
    void setpm(float* ptr) { resampler_->setpm(ptr); }
    void setfm(float* ptr) { resampler_->setfm(ptr); }
    void setam(float* ptr) { resampler_->setam(ptr); }
    void setpwm(float* ptr) { resampler_->setpwm(ptr); }
  private:
    std::auto_ptr<RWInterface> resampler_;
    WaveOscTables::Shape shape_;
  };

  struct LuaWaveOscBind {
    static int open(lua_State *L);
    static std::map<WaveOsc*,  WaveOsc*> oscs; // map for samplerate change
    static void setsamplerate(double sr);
    static const char* meta;
  private:
    static int create(lua_State *L);
    static int work(lua_State* L);
    static int tostring(lua_State* L);
    static int gc(lua_State* L);
    static int get_base_frequency(lua_State* L);
    static int set_base_frequency(lua_State* L);
    static int stop(lua_State* L);
    static int start(lua_State* L);
    static int isplaying(lua_State* L);
    static int set_gain(lua_State* L);
    static int gain(lua_State* L);
    static int setpw(lua_State* L);
    static int pw(lua_State* L);
    static int set_shape(lua_State* L);
    static int shape(lua_State* L);
    static int set_quality(lua_State* L);
    static int quality(lua_State*);
    static int set_sync(lua_State*);
    static int set_sync_fadeout(lua_State*);
    static int phase(lua_State*);
    static int setphase(lua_State*);
    static int setpm(lua_State*);
    static int setam(lua_State*);
    static int setfm(lua_State*);
    static int setpwm(lua_State*);
  };

  struct LuaDspMathHelper {
    static int open(lua_State *L);   
    static int notetofreq(lua_State* L);
    static int freqtonote(lua_State* L);
  };

  class LuaFileHelper {
   public:
	static int open(lua_State *L);
	static int mkdir(lua_State* L);
	static int isdirectory(lua_State* L);
	static int filetree(lua_State* L);
	static int directorylist(lua_State* L);
	static int fileinfo(lua_State* L);
	static int parentdirectory(lua_State* L);
	static int remove(lua_State* L);
	static int rename(lua_State* L);
	static int createfileinfo(lua_State* L, boost::filesystem::path curr);
  };

  struct LuaMidiHelper {
    static int open(lua_State *L);
  private:
    static int gmpercussionnames(lua_State* L);
	static int gmpercussionname(lua_State* L);
    static int notename(lua_State* L);
    static int combine(lua_State* L);
  };

class LuaMidiInput : public LuaState, public MidiInputListener {
	public:
		LuaMidiInput();
		LuaMidiInput(lua_State* L);
		
		void SetDeviceId(unsigned int driver, int devId);
		bool Open();
		bool Close();
		bool Active();
		
		void OnMidiData(uint32_t, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

	private:
		std::auto_ptr<CMidiInput> imp_;						
};

  struct LuaMidiInputBind {
	static std::string meta;
	static int open(lua_State *L);
	static int create(lua_State *L);
	static int gc(lua_State* L);
	static int numdevices(lua_State* L);
	static int devicedescription(lua_State* L);
	static int finddevicebyname(lua_State* L);
	static int setdevice(lua_State* L);
	static int openinput(lua_State* L);
	static int closeinput(lua_State* L);
	static int active(lua_State* L);
  };

  struct LuaDspFilterBind {
    typedef psycle::helpers::dsp::Filter Filter;
    static int open(lua_State *L);
    // store map for samplerate change
    static std::map<Filter*, Filter*> filters;
    static void setsamplerate(double sr);
    static const char* meta;
  private:
    static int create(lua_State *L);
    static int setcutoff(lua_State* L);
    static int cutoff(lua_State* L);
    static int setresonance(lua_State* L);
    static int resonance(lua_State* L);
    static int setfiltertype(lua_State* L);
    static int work(lua_State* L);
    static int tostring(lua_State* L);
    static int gc(lua_State* L);
  };

  struct LuaWaveDataBind {
    static int open(lua_State *L);
    static const char* meta;
  private:
    static int create(lua_State *L);
    static int copy(lua_State *L);
    static int set_wave_sample_rate(lua_State *L);
    static int set_wave_tune(lua_State *L);
    static int set_wave_fine_tune(lua_State *L);
    static int set_loop(lua_State *L);
    static int set_bank(lua_State* L);
    static int gc(lua_State* L);
  };

  struct LEnvelope {
    LEnvelope(const std::vector<double>& times,
      const std::vector<double>& peaks,
      const std::vector<int>& types,
      int suspos,
      double startpeak,
      int samplerate)
      : out_(256, 0),
      fs_(samplerate),
      sus_(suspos),
      startpeak_(startpeak),
      lv_(startpeak),
      times_(times),
      peaks_(peaks),
      types_(types),
      up_(false),
      stage_(peaks.size()) {
    }
    ~LEnvelope() {};
    void setstartvalue(float val) { startpeak_ = val; }
    float lastvalue() const { return lv_; }
    void setstagetime(int stage, double t) { times_[stage]=t; }
    void setstagepeak(int stage, double p) { peaks_[stage]=p; }
    double peak(int stage) const { return peaks_[stage]; }
    double time(int stage) const { return times_[stage]; }
    int stage() const { return stage_; }
    void work(int num);
    void release();
    void start() {
      lv_ = startpeak_;
      stage_ = 0;
      up_ = false;
      calcstage(startpeak_, peaks_[0], times_[0], types_[0]);
      susdone_ = false;
    }
    bool is_playing() const {
      return !(stage_ > peaks_.size()-1);
    }
    PSArray& out() {return out_; }
    void set_samplerate(int sr) {
      int newsc = sc_ * static_cast<int>(sr/(double)fs_);
      calcstage(stage_ == 0 ? startpeak_ : peaks_[stage_-1], peaks_[stage_],
        times_[stage_]);
      sc_ = newsc;
      fs_ = sr;
    }

  private:
    PSArray out_;
    int fs_, sc_, sus_, stage_, nexttime_;
    double m_, lv_, startpeak_;
    bool susdone_ ,up_;
    std::vector<double> times_, peaks_;
    std::vector<int> types_;
    void calcstage(double peakStart, double peakEnd, double time, int type=0) {
      nexttime_ = time*fs_;
      lv_ = peakStart;
      sc_ = 0;
      if (type==1) {
        if (peakStart == 0) peakStart = 0.001;
        if (peakEnd == 0) peakEnd = 0.001;
        lv_ = peakStart;
        //up_ = peakEnd > peakStart;
        //if (up_) {
        // m_ = 1.0 + (log(peakStart) - log(peakEnd)) / (nexttime_);
        // lv_ = peakEnd;
        //} else {
        // m_ = 1.0 + (log(peakEnd) - log(peakStart)) / (nexttime_);
        //   lv_ = peakStart;
        //}
        //lv_ = peakStart;
        //if (peakStart < peakEnd) {
        m_ = 1.0 + (log(peakEnd) - log(peakStart)) / (nexttime_);
        //} else {
        //m_ = 1.0 + (log(peakEnd) - log(peakStart)) / (nexttime_);
        //}
      } else {
        m_ = (peakEnd-peakStart)/nexttime_;
      }
    }
  };

  struct LuaEnvelopeBind {
    static int open(lua_State *L);
    static const char* meta;
  private:
    static int create(lua_State *L);
    static int work(lua_State* L);
    static int release(lua_State* L) { LUAEXPORTM(L, meta, &LEnvelope::release); }
    static int start(lua_State* L) { LUAEXPORTM(L, meta, &LEnvelope::start); }
    static int isplaying(lua_State* L) { LUAEXPORTM(L, meta, &LEnvelope::is_playing); }
    static int setpeak(lua_State* L);
    static int peak(lua_State* L);
    static int tostring(lua_State* L);
    static int setstagetime(lua_State* L);
    static int time(lua_State* L);
    static int setstartvalue(lua_State* L) { LUAEXPORTM(L, meta, &LEnvelope::setstartvalue); }
    static int lastvalue(lua_State* L) { LUAEXPORTM(L, meta, &LEnvelope::lastvalue); }
    static int gc(lua_State* L);
  };
}}  // namespace