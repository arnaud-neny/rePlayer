// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include <psycle/host/detail/project.hpp>
#include "plugininfo.hpp"
#include "LuaArray.hpp"
#include "LuaInternals.hpp"
#include "LockIF.hpp"

struct lua_State;
struct luaL_Reg;

namespace psycle {
namespace host {

namespace ui {
  class KeyEvent;
  class Node;
  class Commands;
  class Viewport;  
  class Frame;
  class MenuContainer;
}
    
class LuaPlugin;
class LuaRun;

typedef boost::shared_ptr<LuaPlugin> LuaPluginPtr;
extern boost::shared_ptr<LuaPlugin> nullPtr;

class LuaControl : public Lock {
 public:
  LuaControl();
  virtual ~LuaControl();
  
  void Load(const std::string& filename);
  virtual void PrepareState();
  void Run();
  void Start();
  virtual void Free();  
  lua_State* state() const { return L; }
  std::string install_script() const;
  virtual PluginInfo meta() const;
  void yield();
  void resume();
 
  protected:
   lua_State* L;
   lua_State* LM;
   std::auto_ptr<ui::Commands> invokelater_;
   std::auto_ptr<ui::Commands> timer_;
   std::set<int> clear_timers_;       
   PluginInfo parse_info() const;
};

class LuaStarter : public LuaControl {
 public:  
  virtual void PrepareState();  
  static int addmenu(lua_State* L);
  static int replacemenu(lua_State* L);
  static int addextension(lua_State* L);
};

static const int CHILDVIEWPORT = 1;
static const int FRAMEVIEWPORT = 2;
static const int TOOLBARVIEWPORT = 3;

static const int MDI = 3;
static const int SDI = 4;

static const int CREATELAZY = 5;
static const int CREATEATSTART = 5;

class LuaProxy : public LuaControl {
 public:
  LuaProxy(LuaPlugin* plug, const std::string& dllname);
  ~LuaProxy();

  // Host accessors
  LuaPlugin& host() { return *host_; }
  LuaPlugin& host() const { return *host_; }  
  LuaMachine* lua_mac() { return lua_mac_; };

  virtual PluginInfo meta() const;
	
  // Script Control	
  void Init();  
  void Reload();
  void PrepareState();
  virtual void Free() {
    LuaControl::Free();
    frame_.reset();
  }

  boost::weak_ptr<ui::Viewport> viewport() { return lua_mac_->viewport(); }
  boost::weak_ptr<class PsycleMeditation> psycle_meditation() { return psycle_meditation_;  }

  // Plugin calls
  void SequencerTick();
  void ParameterTweak(int par, double val);
  void Work(int numsamples, int offset=0);
  void Stop();
  void OnTimer();
  void PutData(unsigned char* data, int size);
  int GetData(unsigned char **ptr, bool all);
  boost::uint32_t GetDataSize();
  void Command(int lastnote, int inst, int cmd, int val);
  void NoteOn(int note, int lastnote, int inst, int cmd, int val);
  void NoteOff(int note, int lastnote, int inst, int cmd, int val);
  bool DescribeValue(int parameter, char * txt);
  void SeqTick(int channel, int note, int ins, int cmd, int val);	
  double Val(int par);
  std::string Id(int par);
	std::string Name(int par);		
	void Range(int par,int &minval, int &maxval);
	int Type(int par);
  void call_execute();
	void call_sr_changed(int rate);
	void call_aftertweaked(int idx);	  
	std::string call_help();
	    
  MachineUiType::Value ui_type() const { return lua_mac_->ui_type(); }
  void call_setprogram(int idx);
  int call_numprograms();
  int get_curr_program();
  std::string get_program_name(int bnkidx, int idx);
  MachinePresetType::Value prsmode() const { return lua_mac_->prsmode(); }  
  int num_cols() const { return lua_mac_->numcols(); }
	int num_parameter() const { return lua_mac_->numparams(); }          
  void OnActivated(int viewport);
  void OnDeactivated();
  
  boost::weak_ptr<ui::Node> menu_root_node() { return menu_root_node_; }

  template<typename T>   
  void InvokeLater(T& f) { 
    lock();
    invokelater_->Add(f);
    unlock();
  }  
  bool IsPsyclePlugin() const;
  bool HasDirectMetaAccess() const;
  void RemoveCanvasFromFrame();
  bool has_frame() const;
  void OpenInFrame();
  void UpdateFrameCanvas();
  void ToggleViewPort();
  void set_userinterface(int user_interface) { user_interface_ = user_interface; }
  int userinterface() const { return user_interface_; }
  void set_default_viewport_mode(int viewport_mode) { default_viewport_mode_ = viewport_mode; }
  int default_viewport_mode() const { return default_viewport_mode_; }
  std::string title() const { return lua_mac_ ? lua_mac_->title() : "noname"; }
  void UpdateWindowsMenu();
  
  static int invokelater(lua_State* L);
	// script callbacks
  static int set_parameter(lua_State* L);
  static int alert(lua_State* L);
  static int flsmain(lua_State* L);
  static int confirm(lua_State* L);  
  static int terminal_output(lua_State* L);
  static int cursor_test(lua_State* L);    
  static int call_selmachine(lua_State* L);
  static int set_machine(lua_State* L);  
  static int setmenurootnode(lua_State* L);
  static int setsetting(lua_State* L);
  static int setting(lua_State* L);
  static int reloadstartscript(lua_State* L);
  static int addtweaklistener(lua_State* L);
  int set_time_out(LuaRun* runnable, int interval);
  int set_interval(LuaRun* runnable, int interval);
  void clear_timer(int id);

  bool has_timer_clear(int id) {
    bool result(false);
    std::set<int>::iterator it = clear_timers_.find(id);
    if (it != clear_timers_.end()) {
      clear_timers_.erase(it);
      result = true;
    }
    return result;
  }

  void ShowPsycleMeditation(const std::string& message);
  bool has_error() const { return has_error_; }
  std::string last_error() const { return last_error_; }

 private:
  void ExportCFunctions();
  std::string ParDisplay(int par);
  std::string ParLabel(int par);
  const PluginInfo& cache_meta(const PluginInfo& meta) const {
     meta_cache_ = meta;
     is_meta_cache_updated_ = true;
     return meta_cache_;
  }  
  void OnFrameClose(ui::Frame&);
  void OnFrameKeyDown(ui::KeyEvent&);
  void OnFrameKeyUp(ui::KeyEvent&);
      
  mutable bool is_meta_cache_updated_;
  mutable PluginInfo meta_cache_; 
  LuaPlugin* host_;
  LuaMachine* lua_mac_;
  boost::weak_ptr<ui::Node> menu_root_node_;
  boost::shared_ptr<ui::Frame> frame_;
  int user_interface_;
  int clock_;
  int default_viewport_mode_;  
  bool has_error_;
  std::string last_error_;
  public:
  boost::shared_ptr<class PsycleMeditation> psycle_meditation_;
};

class LuaPluginBind {
 public:
  static int open(lua_State *L);
  static const char* meta;  
  static int create(lua_State* L);
  static int gc(lua_State* L);
};

class Link {
  public:
   Link() : viewport_(CHILDVIEWPORT), user_interface_(MDI) {}
   Link(const std::string& dll_name, const std::string& label, int viewport, int user_interface) : 
      dll_name_(dll_name),
      label_(label),
      viewport_(viewport),
      user_interface_(user_interface) {
   }

   const std::string& dll_name() const { return dll_name_; }
   const std::string& label() const { return label_; }
   const int viewport() const { return viewport_; }
   const int user_interface() const { return user_interface_; }

   boost::weak_ptr<LuaPlugin> plugin;
   
 private:
   std::string label_, dll_name_;   
   int viewport_, user_interface_;
};

// Container for HostExtensions
class HostExtensions {     
 public:
  typedef std::list<LuaPluginPtr> List;  
 
  HostExtensions() : child_view_(0), active_lua_(0), has_toolbar_extension_(false) {}
  ~HostExtensions() {}  

  static HostExtensions& Instance();
  void InitInstance(class HostViewPort* child_view);
  void ExitInstance();

  void StartScript();
  void Free();  
  typedef HostExtensions::List::iterator iterator;
  virtual iterator begin() { return extensions_.begin(); }
  virtual iterator end() { return extensions_.end(); }
  virtual bool empty() const { return true; }
  void Add(const LuaPluginPtr& ptr) { extensions_.push_back(ptr); }
  void Remove(const LuaPluginPtr& ptr) {         
    RemoveFromWindowsMenu(ptr.get());
    extensions_.remove(ptr);        
  }
  HostExtensions::List Get(const std::string& name);
  LuaPluginPtr Get(int idx);  
  void ReplaceHelpMenu(Link& link, int pos);
  void RestoreViewMenu();
  void AddViewMenu(Link& link);
  void AddHelpMenu(Link& link);
  void AddWindowsMenu(Link& link);
  void OnPluginViewPortChanged(LuaPlugin& plugin, int viewport);
  void RemoveFromWindowsMenu(LuaPlugin* plugin);
  LuaPluginPtr Execute(Link& link);
  void ChangeWindowsMenuText(LuaPlugin* plugin);
  void AddToWindowsMenu(Link& link);
  LuaPlugin* active_lua() { return active_lua_; }
  void set_active_lua(LuaPlugin* active_lua) { active_lua_ = active_lua; }
  void OnExecuteLink(Link& link);
  void FlsMain();
  bool HasToolBarExtension() const;
  void UpdateStyles();

 private:   
  lua_State* load_script(const std::string& dllpath);
  void AutoInstall();
  std::vector<std::string> search_auto_install();
  std::string menu_label(const Link& link) const;
  HostExtensions::List extensions_;
  class HostViewPort* child_view_;
  LuaPlugin* active_lua_;
  bool has_toolbar_extension_;
};

struct LuaGlobal {   
   static std::map<lua_State*, LuaProxy*> proxy_map;
   static LuaProxy* proxy(lua_State* L) {
     std::map<lua_State*, LuaProxy*>::iterator it = proxy_map.find(L);
     return it != proxy_map.end() ? it->second : 0;
   }   
   static lua_State* load_script(const std::string& dllpath);   
   static PluginInfo LoadInfo(const std::string& dllpath);         
   static Machine* GetMachine(int idx);
   static class LuaPlugin* GetLuaPlugin(int idx) {
      Machine* mac = GetMachine(idx);
      assert(mac);
      if (mac->_type == MACH_LUA) {
        return (LuaPlugin*) mac;
      }
      return 0;
   }
   static std::vector<LuaPlugin*> GetAllLuas();
   template<typename T>   
   static void InvokeLater(lua_State* L, T& f) {
     LuaGlobal::proxy(L)->InvokeLater(f); 
   }      
};

 
}  // namespace host
}  // namespace psycle