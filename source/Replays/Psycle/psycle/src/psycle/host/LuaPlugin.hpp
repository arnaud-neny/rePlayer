// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

///\file
///\brief interface file for psycle::host::LuaPlugin.

#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "Machine.hpp"
#include "LuaArray.hpp"
#include "LuaHost.hpp"
#define BOOST_SIGNALS_NO_DEPRECATION_WARNING
#include <boost/signal.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

struct lua_State;

namespace psycle { namespace host {
  
  const int AUTOID = -1;

  class LuaPlugin : public Machine,
                    public boost::enable_shared_from_this<LuaPlugin>,
                    public psycle::host::Timer
  {
  public:
    typedef boost::shared_ptr<LuaPlugin> Ptr;
	typedef boost::shared_ptr<const LuaPlugin> ConstPtr;
	typedef boost::weak_ptr<LuaPlugin> WeakPtr;
	typedef boost::weak_ptr<const LuaPlugin> ConstWeakPtr;

    LuaPlugin(const std::string& dllpath, int index, bool full=true);    
    virtual ~LuaPlugin();
    void Free();
    LuaProxy& proxy() { return proxy_; }    
    const LuaProxy& proxy() const { return proxy_; }

    virtual int GenerateAudioInTicks( int startSample, int numSamples );
    virtual float GetAudioRange() const { return 1.0f; }
    virtual const char* const GetName(void) const {
      name_ = proxy_.meta().name; 
      return name_.c_str();
    }
    // todo other paramter save that doesnt invalidate tweaks when changing num parameters
    virtual bool LoadSpecificChunk(RiffFile* pFile, int version);
    virtual void SaveSpecificChunk(RiffFile * pFile);
    // todo testing
    virtual const char * const GetDllName() const throw() { return dll_path_.c_str(); }
    //PluginInfo
    virtual const std::string GetAuthor() { return proxy_.meta().vendor; }
    virtual const uint32_t GetAPIVersion() { return proxy_.meta().APIversion; }
    virtual const uint32_t GetPlugVersion() { return atoi(proxy_.meta().version.c_str()); }
    bool IsSynth() const throw() { return (proxy_.meta().mode == MACHMODE_GENERATOR); }

    //TODO: testing
    virtual void NewLine();
    //TODO testing
    virtual void Tick(int track, PatternEntry * pData);
    virtual void Stop();
    //TODO: testing
    virtual int GetNumCols() { return !crashed() ? proxy_.num_cols() : 0; }
    virtual void GetParamRange(int numparam,int &minval, int &maxval);
    virtual int GetNumParams() { return !crashed() ? proxy_.num_parameter() : 0; }
    virtual int GetParamType(int numparam);
    virtual void GetParamName(int numparam, char * parval);
    virtual void GetParamValue(int numparam, char * parval);
    virtual int GetParamValue(int numparam);
    virtual void GetParamId(int numparam, std::string& id);
    virtual bool SetParameter(int numparam, int value); //{ return false;}    
    virtual int GetNumInputPins() const { return this->IsSynth() ? 0 : samplesV.size(); }
    virtual int GetNumOutputPins() const { return samplesV.size(); }
    PluginInfo info() const { return proxy().meta(); }
    virtual void SetSampleRate(int sr) { try {Machine::SetSampleRate(sr); proxy_.call_sr_changed((float)sr); }catch(...){} }
    virtual void AfterTweaked(int numparam);    
    std::string help();
    virtual MachineUiType::Value ui_type() const { return proxy_.ui_type(); }    
    void OnExecute() { proxy_.call_execute(); } // called by HostUI view menu
    boost::weak_ptr<ui::Viewport> viewport() { return proxy().viewport(); }
    virtual void OnReload();
    bool LoadBank(const std::string& filename);
    void SaveBank(const std::string& filename);
    void OnActivated(int viewport) { proxy_.OnActivated(viewport); }
    void OnDeactivated() { proxy_.OnDeactivated();  }

    // Bank & Programs
    virtual void SetCurrentProgram(int idx);
		virtual int GetCurrentProgram();
		virtual void GetCurrentProgramName(char* val) {
      GetIndexProgramName(0, curr_prg_, val);
    }
		virtual void GetIndexProgramName(int bnkidx, int prgIdx, char* val); //{
			//GetProgramNameIndexed(-1, bnkidx*128 + prgIdx, val);
		//};
    virtual int GetNumPrograms();
		/*virtual int GetTotalPrograms(){ return numPrograms();};
		virtual void SetCurrentBank(int idx) { SetProgram(idx*128+GetCurrentProgram());};
		virtual int GetCurrentBank() { try {return GetProgram()/128; } catch(...){return 0;}};
		virtual void GetCurrentBankName(char* val) {GetIndexBankName(GetCurrentBank(),val);};
		virtual void GetIndexBankName(int bnkidx, char* val){
		  if(bnkidx < GetNumBanks())
		 	  sprintf(val,"Internal %d", bnkidx+1);
		  else
				val[0]='\0';
		}
		virtual int GetNumBanks(){ return (numPrograms()/128)+1;};*/
    
    std::string dll_path_;
    bool usenoteon_;
    MachinePresetType::Value prsmode() const { return proxy_.prsmode(); }
    void lock() const { proxy_.lock(); }
    void unlock() const { proxy_.unlock(); }
    void InvalidateMenuBar() {}
    void DoExit() { do_exit_ = true; }
    void DoReload() { do_reload_ = true; }
    boost::shared_ptr<LuaPlugin> this_ptr() { return shared_from_this(); }    
    boost::signal<void (LuaPlugin&, int)> ViewPortChanged;
    std::string install_script() const { return proxy_.install_script(); }
    std::string const title() { return proxy().title(); }
	bool has_error() const { return proxy().has_error(); }
	std::string last_error() const { return proxy().last_error(); }

  protected:
    LuaProxy proxy_;
    int curr_prg_;

  private:
    void MachineInits();
    virtual void OnTimerViewRefresh();
    // additions if noteon mode is used
    struct note {
      unsigned char key;
      unsigned char midichan;
    };
    note trackNote[MAX_TRACKS];
    void SendNoteOn(unsigned char channel,
      unsigned char key,
      unsigned char inst,
      unsigned char cmd,
      unsigned char val);
    void SendNoteOff(unsigned char channel,
      unsigned char key,
      unsigned char lastkey,
      unsigned char inst,
      unsigned char cmd,
      unsigned char val);
    void SendCommand(unsigned char channel,
      unsigned char inst,
      unsigned char cmd,
      unsigned char val);
    public:
      bool do_exit_, do_reload_;    

    static int idex_; // auto index for host extensions
    mutable std::string name_;
  };
}   // namespace
}   // namespace