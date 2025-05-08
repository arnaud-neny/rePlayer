// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#include <psycle/host/detail/project.hpp>
#include "LuaHost.hpp"
#include "LuaInternals.hpp"
#include "LuaPlugin.hpp"
#include "Player.hpp"
#include "ExtensionBar.hpp"
#include "plugincatcher.hpp"
#include "Song.hpp"
#include <boost/filesystem.hpp>
#include <boost/tokenizer.hpp>
#include <lua.hpp>
#include "LuaGui.hpp"
#include "NewMachine.hpp"
#include <algorithm>
#include "resources\resources.hpp"

#include "MainFrm.hpp"

#if defined LUASOCKET_SUPPORT && !defined WINAMP_PLUGIN
extern "C" {
#include <luasocket/luasocket.h>
#include <luasocket/mime.h>
}
#endif //defined LUASOCKET_SUPPORT && !defined WINAMP_PLUGIN

#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>

namespace psycle { 
namespace host {

using namespace ui;

LuaControl::LuaControl() :
    L(0), 
    LM(0),
    invokelater_(new ui::Commands()),
    timer_(new ui::Commands()) {  
}

LuaControl::~LuaControl() {
  Free();  
}

void LuaControl::Load(const std::string& filename) {  
  L = LuaGlobal::load_script(filename);
} 

void LuaControl::Run() { 
  int status = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (status) {      
    const char* msg = lua_tostring(L, -1);    
    throw std::runtime_error(msg);       
  }   
}

void LuaControl::yield() {
  lua_yieldk(L, 0, 0, 0);
}

void LuaControl::resume() {
  lua_resume(L, 0, 0);
}

void LuaControl::Start() {
  lua_getglobal(L, "psycle");
  if (lua_isnil(L, -1)) {
    throw std::runtime_error("Psycle not available.");
  }
  lua_getfield(L, -1, "start");
  if (!lua_isnil(L, -1)) {
    int status = lua_pcall(L, 0, 0, 0);
    if (status) {         
      const char* msg = lua_tostring(L, -1);      
      throw std::runtime_error(msg);       
    }
  }
}

void LuaControl::Free() {
  if (L) {
    invokelater_->Clear();
    timer_->Clear();
    clear_timers_.clear();
    lua_close(L);    
  }
  L = 0;
}

void LuaControl::PrepareState() {
  static const luaL_Reg methods[] = {{NULL, NULL}};
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  LuaControl** ud = (LuaControl **)lua_newuserdata(L, sizeof(LuaProxy *));
  luaL_newmetatable(L, "psyhostmeta");
  lua_setmetatable(L, -2);
  *ud = this;
  lua_setfield(L, -2, "__self");
  lua_setglobal(L, "psycle");
  lua_getglobal(L, "psycle"); 
  lua_pushinteger(L, CHILDVIEWPORT);
  lua_setfield(L, -2, "CHILDVIEWPORT");
  lua_pushinteger(L, FRAMEVIEWPORT);
  lua_setfield(L, -2, "FRAMEVIEWPORT");
  lua_pushinteger(L, TOOLBARVIEWPORT);
  lua_setfield(L, -2, "TOOLBARVIEWPORT");
  lua_pushinteger(L, MDI);
  lua_setfield(L, -2, "MDI");
  lua_pushinteger(L, SDI);
  lua_setfield(L, -2, "SDI");
  lua_pushinteger(L, CREATELAZY);
  lua_setfield(L, -2, "CREATELAZY");
  lua_pushinteger(L, CREATEATSTART);
  lua_setfield(L, -2, "CREATEATSTART");
  lua_newtable(L); // table for canvasdata
  lua_setfield(L, -2, "userdata");
  lua_newtable(L); // table for canvasdata
  lua_newtable(L); // metatable
  lua_pushstring(L, "kv");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, "weakuserdata");
  lua_pop(L, 1);
}

boost::shared_ptr<LuaPlugin> nullPtr;
struct NullDeleter {template<typename T> void operator()(T*) {} };
struct ReleaseImpDeleter {
  template<typename T> void operator()(T* frame) { 
    frame->release_imp(); 
  }
};

std::string LuaControl::install_script() const {
  std::string result;
  lua_getglobal(L, "psycle");
  if (lua_isnil(L, -1)) {
    throw std::runtime_error("Psycle not available.");
  }
  lua_getfield(L, -1, "install");
  if (!lua_isnil(L, -1)) {
    int status = lua_pcall(L, 0, 1, 0);    
    if (status) {         
      const char* msg = lua_tostring(L, -1); 
      ui::alert(msg);
      throw std::runtime_error(msg);       
    }
    result = luaL_checkstring(L, -1);
  }
  return result;
}

PluginInfo LuaControl::meta() const {
  PluginInfo result;
  lua_getglobal(L, "psycle");
  if (lua_isnil(L, -1)) {
    throw std::runtime_error("Psycle not available.");
  }
  lua_getfield(L, -1, "info");
  if (!lua_isnil(L, -1)) {
    int status = lua_pcall(L, 0, 1, 0);    
    if (status) {         
      const char* msg = lua_tostring(L, -1); 
      ui::alert(msg);
      throw std::runtime_error(msg);       
    }
    result = parse_info();    
  } else {
    throw std::runtime_error("no info found");
  }
  return result;
}

PluginInfo LuaControl::parse_info() const {
  PluginInfo result;
  result.type = MACH_LUA;
  result.mode = MACHMODE_FX;
  result.allow = true;
  result.identifier = 0;    
  if (lua_istable(L,-1)) {
    size_t len;
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      const char* key = luaL_checklstring(L, -2, &len);
      if (strcmp(key, "vendor") == 0) {
        const char* value = luaL_checklstring(L, -1, &len);
        result.vendor = std::string(value);
      } else
        if (strcmp(key, "name") == 0) {
          const char* value = luaL_checklstring(L, -1, &len);
          result.name = std::string(value);
        } else
          if (strcmp(key, "mode") == 0) {
            if (lua_isnumber(L, -1) == 0) {
              throw std::runtime_error("info mode not a number"); 
            }
            int value = luaL_checknumber(L, -1);
            switch (value) {
              case 0 : result.mode = MACHMODE_GENERATOR; break;
              case 3 : result.mode = MACHMODE_HOST; break;
              default: result.mode = MACHMODE_FX; break;
            }
          } else
          if (strcmp(key, "generator") == 0) {
            // deprecated, use mode instead
            int value = luaL_checknumber(L, -1);
            result.mode = (value==1) ? MACHMODE_GENERATOR : MACHMODE_FX;
          } else
            if (strcmp(key, "version") == 0) {
              const char* value = luaL_checklstring(L, -1, &len);
              result.version = value;
            } else
              if (strcmp(key, "api") == 0) {
                int value = luaL_checknumber(L, -1);
                result.APIversion = value;
              } else
                if (strcmp(key, "noteon") == 0) {
                  int value = luaL_checknumber(L, -1);
                  result.flags = value;
                }
    }
  }   
  std::ostringstream s;
  s << (result.mode == MACHMODE_GENERATOR ? "Lua instrument"
                                          : "Lua effect")
    << " by "
    << result.vendor;
  result.desc = s.str();
  return result;
}

void LuaStarter::PrepareState() {
  static const luaL_Reg methods[] = {    
    {"addmenu", addmenu},
    {"replacemenu", replacemenu},
    {"addextension", addextension},
    {"output", LuaProxy::terminal_output},
	{"cursortest", LuaProxy::cursor_test},
    {"alert", LuaProxy::alert},
    {"confirm", LuaProxy::confirm},	
    {NULL, NULL}
  };
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  LuaControl** ud = (LuaControl **)lua_newuserdata(L, sizeof(LuaProxy *));
  luaL_newmetatable(L, "psyhostmeta");
  lua_setmetatable(L, -2);
  *ud = this;
  lua_setfield(L, -2, "__self");
  lua_setglobal(L, "psycle");
  lua_getglobal(L, "psycle"); 
  lua_pushinteger(L, CHILDVIEWPORT);
  lua_setfield(L, -2, "CHILDVIEWPORT");
  lua_pushinteger(L, FRAMEVIEWPORT);
  lua_setfield(L, -2, "FRAMEVIEWPORT");
  lua_pushinteger(L, TOOLBARVIEWPORT);
  lua_setfield(L, -2, "TOOLBARVIEWPORT");
  lua_pushinteger(L, MDI);
  lua_setfield(L, -2, "MDI");
  lua_pushinteger(L, SDI);
  lua_setfield(L, -2, "SDI");
  lua_pushinteger(L, CREATELAZY);
  lua_setfield(L, -2, "CREATELAZY");
  lua_pushinteger(L, CREATEATSTART);
  lua_setfield(L, -2, "CREATEATSTART");
  lua_newtable(L); // table for canvasdata
  lua_setfield(L, -2, "userdata");
  lua_newtable(L); // table for canvasdata
  lua_newtable(L); // metatable
  lua_pushstring(L, "kv");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, "weakuserdata");
  lua_pop(L, 1);
  LuaHelper::require<LuaPluginBind>(L, "psycle.extension");
}

int LuaStarter::addmenu(lua_State* L) {
  std::string menu_name(luaL_checkstring(L, 1));  
  lua_getfield(L, 2, "plugin");
  lua_getfield(L, 2, "label");    
  lua_getfield(L, 2, "viewport");
  lua_getfield(L, 2, "userinterface");  

  LuaPluginPtr plugin = LuaHelper::test_sptr<LuaPlugin>(L, -4, LuaPluginBind::meta);
  std::string plugin_name = !plugin ? luaL_checkstring(L, -4) : "test";  
  Link link(plugin_name,
            luaL_checkstring(L, -3),
            luaL_checkinteger(L, -2),
            luaL_checkinteger(L, -1));   
  HostExtensions& host_extensions = HostExtensions::Instance();
  if (plugin) {
    link.plugin = plugin;
    plugin->proxy().set_userinterface(link.user_interface());
	plugin->Init();
    plugin->ViewPortChanged.connect(boost::bind(&HostExtensions::OnPluginViewPortChanged, &host_extensions,  _1, _2));     
    if (link.user_interface() == MDI) {
      host_extensions.AddToWindowsMenu(link); 
    }
  }  
  if (menu_name == "view") {
    host_extensions.AddViewMenu(link);
  } else
  if (menu_name == "help") {
    host_extensions.AddHelpMenu(link);
  }
  lua_getfield(L, 2, "creation");  
  if (!lua_isnil(L, -1)) {
    int mode = luaL_checkinteger(L, -1);
	if (mode == CREATEATSTART) {
	  host_extensions.OnExecuteLink(link);
	}
  }
  lua_pop(L, 1);
  return 0;
}

int LuaStarter::replacemenu(lua_State* L) {
  std::string menu_name(luaL_checkstring(L, 1));  
  int menu_pos(luaL_checkinteger(L, 2));
  lua_getfield(L, 3, "plugin");
  lua_getfield(L, 3, "label");    
  lua_getfield(L, 3, "viewport");
  lua_getfield(L, 3, "userinterface");

  LuaPluginPtr plugin = LuaHelper::test_sptr<LuaPlugin>(L, -4, LuaPluginBind::meta);
  std::string plugin_name = !plugin ? luaL_checkstring(L, -4) : "test";  
  Link link(plugin_name,
            luaL_checkstring(L, -3),
            luaL_checkinteger(L, -2),
            luaL_checkinteger(L, -1));   
  HostExtensions& host_extensions = HostExtensions::Instance();
  if (plugin) {
    link.plugin = plugin;
    plugin->proxy().set_userinterface(link.user_interface());
	  plugin->Init();			    
    plugin->ViewPortChanged.connect(boost::bind(&HostExtensions::OnPluginViewPortChanged, &host_extensions,  _1, _2));     
    if (link.user_interface() == MDI) {
      host_extensions.AddToWindowsMenu(link); 
    }
  }  
  if (menu_name == "view") {
    // host_extensions.AddViewMenu(link);
  } else
  if (menu_name == "help") {
    host_extensions.ReplaceHelpMenu(link, menu_pos);
  }
  return 0;
}

int LuaStarter::addextension(lua_State* L) {
  const char* extension_name = luaL_checkstring(L, 1);
  PluginCatcher* plug_catcher =
    static_cast<PluginCatcher*>(&Global::machineload());
  PluginInfo* info = plug_catcher->info(extension_name);
  if (info) {
    if (info->mode == MACHMODE_HOST) {          
     LuaPlugin* mac =new LuaPlugin(info->dllname, -1);     
     mac->proxy().set_userinterface(SDI);
	   mac->Init();			
     luaL_requiref(L, "psycle.extension", &LuaPluginBind::open, true);    
     LuaPluginPtr plugin = LuaHelper::new_shared_userdata<>(L, LuaPluginBind::meta, mac, lua_gettop(L));
     lua_remove(L, -2);     
     HostExtensions::Instance().Add(plugin);     
    }
  } else {
    ui::alert("Error in start.lua : Extension " + std::string(extension_name) + " not found.");
    lua_pushnil(L);
  }
  return 1;
}

// Class Proxy : export and import between psycle and lua
LuaProxy::LuaProxy(LuaPlugin* host, const std::string& dllname) : 
    host_(host),
    is_meta_cache_updated_(false),
    lua_mac_(0),
    user_interface_(SDI),
	default_viewport_mode_(CHILDVIEWPORT),
    clock_(0),
	has_error_(false),
	psycle_meditation_(new PsycleMeditation()) {     
  Load(dllname);
}

LuaProxy::~LuaProxy() {
}

struct Invoke {
  Invoke(LuaRun* run) : run_(run) {}  
  
  bool operator()() const { 
    run_->Run();
    return true;
  }

 private:
  LuaRun* run_;
};

int LuaProxy::invokelater(lua_State* L) { 
  if lua_isnil(L, 1) {
    return luaL_error(L, "Argument is nil.");
  }
  boost::shared_ptr<LuaRun> run = LuaHelper::check_sptr<LuaRun>(L, 1, LuaRunBind::meta);  
  Invoke f(run.get());   
  LuaProxy* proxy = LuaGlobal::proxy(L);    
  if (!proxy) {
    return luaL_error(L, "invokelater: Proxy not found.");
  }
  if (proxy->host_->crashed()) {
    return 0;
  }
  proxy->invokelater_->Add(f);  
  return 0;
}

void LuaProxy::OnTimer() {      
  try {
    lock();    
    invokelater_->Invoke();
    timer_->Invoke();
    if (clock_ >= 100) { 
      clock_ = 0;            
      lua_gc(L, LUA_GCCOLLECT, 0);           
    }
    ++clock_;
    unlock();
  } catch(std::exception& e) {    
    std::string msg = std::string("LuaRun Errror.") + e.what();
    ui::alert(msg);
    unlock();
    throw std::runtime_error(msg.c_str());
  }     
}

void LuaProxy::OnFrameClose(ui::Frame&) {
  if (lua_mac_ && user_interface_ == MDI) {
    lua_mac_->doexit();
  }
}

void LuaProxy::PrepareState() {  
  LuaGlobal::proxy_map[L] = this;
  ExportCFunctions();
  // require c modules
  // config
  LuaHelper::require<LuaConfigBind>(L, "psycle.config");
  LuaHelper::require<LuaPluginInfoBind>(L, "psycle.plugininfo");
  LuaHelper::require<LuaPluginCatcherBind>(L, "psycle.plugincatcher");  
  // sound engine
  LuaHelper::require<LuaArrayBind>(L, "psycle.array");
  LuaHelper::require<LuaScopeMemoryBind>(L, "psycle.scopememory");
  LuaHelper::require<LuaWaveDataBind>(L, "psycle.dsp.wavedata");  
  LuaHelper::require<LuaMachineBind>(L, "psycle.machine");
  LuaHelper::require<LuaTweakBind>(L, "psycle.tweak"); 
  // LuaHelper::require<LuaMachinesBind>(L, "psycle.machines");
  LuaHelper::require<LuaWaveOscBind>(L, "psycle.osc");
  LuaHelper::require<LuaResamplerBind>(L, "psycle.dsp.resampler");
  LuaHelper::require<LuaDelayBind>(L, "psycle.delay");
  LuaHelper::require<LuaDspFilterBind>(L, "psycle.dsp.filter");
  LuaHelper::require<LuaEnvelopeBind>(L, "psycle.envelope");
  LuaHelper::require<LuaDspMathHelper>(L, "psycle.dsp.math");
  LuaHelper::require<LuaFileHelper>(L, "psycle.file");
  LuaHelper::require<LuaFileObserverBind>(L, "psycle.fileobserver");
  LuaHelper::require<LuaMidiHelper>(L, "psycle.midi");
  LuaHelper::require<LuaMidiInputBind>(L, "psycle.midiinput");
  LuaHelper::require<LuaPlayerBind>(L, "psycle.player");
  LuaHelper::require<LuaPatternDataBind>(L, "psycle.pattern");
  // ui host interaction
  LuaHelper::require<LuaSequenceBarBind>(L, "psycle.sequencebar");
  LuaHelper::require<LuaMachineBarBind>(L, "psycle.machinebar");
  LuaHelper::require<LuaActionListenerBind>(L, "psycle.ui.hostactionlistener");
  LuaHelper::require<LuaCmdDefBind>(L, "psycle.ui.cmddef");
  LuaHelper::require<LuaRunBind>(L, "psycle.run");
  LuaHelper::require<LuaCommandBind>(L, "psycle.command");
  lua_ui_requires(L);
  LuaHelper::require<LuaStockBind>(L, "psycle.stock");
#if 0//!defined WINAMP_PLUGIN
  LuaHelper::require<LuaPlotterBind>(L, "psycle.plotter");
#endif //!defined WINAMP_PLUGIN
#if defined LUASOCKET_SUPPORT && !defined WINAMP_PLUGIN
  luaL_requiref(L, "socket.core", luaopen_socket_core, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "mime", luaopen_mime_core, 1);
  lua_pop(L, 1);
#endif  //LUASOCKET_SUPPORT  
}

void LuaProxy::Reload() {
  try {      
    lock();       
    host_->set_crashed(true);
	has_error_ = false;
    lua_State* old_state = L;
    LuaMachine* old_machine = lua_mac_;
	int oldviewportmode = frame_ && !viewport().expired() ? FRAMEVIEWPORT 
															: default_viewport_mode();     
    try {          	 
      RemoveCanvasFromFrame();      
      L = LuaGlobal::load_script(host_->GetDllName());          
      PrepareState();
      Run();
      Start();
      if (old_state) {
        LuaGlobal::proxy(old_state)->invokelater_->Clear();
        LuaGlobal::proxy(old_state)->timer_->Clear();
        LuaGlobal::proxy(old_state)->clear_timers_.clear();                   
      }
      Init();      
      assert(host_);
      UpdateFrameCanvas();	 
      if (oldviewportmode != FRAMEVIEWPORT) {
        host_->ViewPortChanged(*host_, oldviewportmode);	
		if (!host_->viewport().expired()) {
			host_->viewport().lock()->FLS();
		}
      }
	  if (old_state) {
        lua_close(old_state); 
      }	  
      OnActivated(2);            
    } catch(std::exception &e) {
      if (L) {        
        lua_close(L);
      }
      L = old_state;      
      lua_getglobal(L, "psycle");
      lua_getfield(L, -1, "__self");
      LuaProxy* proxy = *(LuaProxy**)luaL_checkudata(L, -1, "psyhostmeta");    
      old_machine->set_mac(proxy->host_);
      proxy->lua_mac_ = old_machine;
      proxy->lua_mac_->setproxy(proxy);
      std::string s = std::string("Reload Error, old script still running!\n") + e.what(); 
      UpdateFrameCanvas();
	  assert(host_);
	  psycle_meditation_->set_plugin(*host_);
	  ShowPsycleMeditation(e.what());
	  host_->ViewPortChanged(*host_, oldviewportmode); 
	  psycle_meditation_->FLS();
      unlock();	  
      throw std::runtime_error(s.c_str());  
    }     
    host_->Mute(false);
    host_->set_crashed(false);    
    unlock();
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::ShowPsycleMeditation(const std::string& message) {
  psycle_meditation_->set_error_text(message);
  has_error_ = true;
}

int LuaProxy::alert(lua_State* L) {
  if (lua_isboolean(L, 1)) {
    std::string out;
    int v = lua_toboolean(L, 1);   
    ui::alert(v == 1 ? "true" : "false");
  } else {    
    const char* msg = luaL_checkstring(L, 1);  
    ui::alert(msg);
  }
  return 0;
}

int LuaProxy::flsmain(lua_State* L) {
  HostExtensions::Instance().FlsMain();
  return 0;
}

int LuaProxy::confirm(lua_State* L) {    
  const char* msg = luaL_checkstring(L, 1); 
  bool result = ui::confirm(msg);  
  lua_pushboolean(L, result);  
  return 1;  
}

int LuaProxy::terminal_output(lua_State* L) {
  int n = lua_gettop(L);  // number of arguments
  const char* out = 0;
  if (lua_isboolean(L, 1)) {
    int v = lua_toboolean(L, 1);
    if (v==1) out = "true"; else out = "false";
    ui::TerminalFrame::instance().output(out);
  } else {
    int i;
    lua_getglobal(L, "tostring");
    for (i=1; i<=n; i++) {
      const char *s;
      size_t l;
      lua_pushvalue(L, -1);  /* function to be called */
      lua_pushvalue(L, i);   /* value to print */
      lua_call(L, 1, 1);
      s = lua_tolstring(L, -1, &l);  /* get result */
      if (s == NULL)
        return luaL_error(L,
        LUA_QL("tostring") " must return a string to " LUA_QL("print"));
      lua_pop(L, 1);  /* pop result */
      ui::TerminalFrame::instance().output(s);
    }
  }
  return 0;
}

int LuaProxy::addtweaklistener(lua_State* L) {
	boost::shared_ptr<Tweak> tweak = LuaHelper::check_sptr<Tweak>(L, 1, LuaTweakBind::meta);
	PsycleGlobal::inputHandler().AddTweakListener(tweak);
	return LuaHelper::chaining(L);
}

int LuaProxy::cursor_test(lua_State* L) {
  HCURSOR c = ::LoadCursor(0, IDC_ARROW);
  ::SetCursor(c);
  return 0;
}

int LuaProxy::call_selmachine(lua_State* L) {    
  CNewMachine dlg(AfxGetMainWnd());
  dlg.DoModal();
  if (dlg.Outputmachine >= 0) {
    std::string filename = dlg.psOutputDll;
    boost::filesystem::path p(filename);
    lua_pushstring(L, p.stem().string().c_str());
    lua_pushstring(L, dlg.psOutputDll.c_str());
  } else {
    lua_pushnil(L);
  }   
  return 2;
}

int LuaProxy::set_machine(lua_State* L) {
  lua_getglobal(L, "psycle");
  lua_getfield(L, -1, "__self");
  LuaProxy* proxy = *(LuaProxy**)luaL_checkudata(L, -1, "psyhostmeta");
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_getfield(L, 1, "__self");
  LuaMachine** ud = (LuaMachine**) luaL_checkudata(L, -1, "psypluginmeta");
  (*ud)->set_mac(proxy->host_);
  proxy->lua_mac_ = *ud;
  proxy->lua_mac_->setproxy(proxy);    
  lua_pushvalue(L, 1);
  LuaHelper::register_weakuserdata(L, proxy->lua_mac());
  lua_setfield(L, 2, "proxy");
  // share samples
  (*ud)->build_buffer(proxy->host_->samplesV, 256);
  return 0;
}

int LuaProxy::setmenurootnode(lua_State* L) {
  boost::shared_ptr<ui::Node> node = LuaHelper::check_sptr<ui::Node>(L, 1, LuaNodeBind::meta);  
  lua_getglobal(L, "psycle");
  lua_getfield(L, -1, "__self");
  LuaProxy* proxy = *(LuaProxy**)luaL_checkudata(L, -1, "psyhostmeta");    
  proxy->menu_root_node_ = node;
  return 0;
}

int LuaProxy::setsetting(lua_State* L) {
  if (!lua_istable(L, 1)) {
    return luaL_error(L, "Setting must be a table.");
  }
  lua_getglobal(L, "psycle");
  lua_pushvalue(L, 1);
  lua_setfield(L, -2, "__setting");  
  return LuaHelper::chaining(L);
}

int LuaProxy::setting(lua_State* L) {
  lua_getglobal(L, "psycle"); 
  lua_getfield(L, -1, "__setting");
  return 1;
}

int LuaProxy::reloadstartscript(lua_State* L) {
  lua_getglobal(L, "psycle");
  HostExtensions::Instance().RestoreViewMenu();
  HostExtensions::Instance().StartScript();
  return 1;
}

void LuaProxy::ExportCFunctions() {
  static const luaL_Reg methods[] = {
    {"invokelater", invokelater},
    {"output", terminal_output },
	{"cursortest", cursor_test },
    {"alert", alert},
    {"confirm", confirm},
    {"setmachine", set_machine},    
    {"setmenurootnode", setmenurootnode},
    {"selmachine", call_selmachine},
    {"setsetting", setsetting},
    {"setting", setting},
    {"reloadstartscript", reloadstartscript}, 
	{"flsmain", flsmain},
	{"addtweaklistener", addtweaklistener},
    {NULL, NULL}
  };
  lua_newtable(L);
  luaL_setfuncs(L, methods, 0);
  LuaProxy** ud = (LuaProxy **)lua_newuserdata(L, sizeof(LuaProxy *));
  luaL_newmetatable(L, "psyhostmeta");
  lua_setmetatable(L, -2);
  *ud = this;
  lua_setfield(L, -2, "__self");
  lua_setglobal(L, "psycle");
  lua_getglobal(L, "psycle");  

  lua_pushinteger(L, CHILDVIEWPORT);
  lua_setfield(L, -2, "CHILDVIEWPORT");
  lua_pushinteger(L, FRAMEVIEWPORT);
  lua_setfield(L, -2, "FRAMEVIEWPORT");
  lua_pushinteger(L, TOOLBARVIEWPORT);
  lua_setfield(L, -2, "TOOLBARVIEWPORT");
  lua_pushinteger(L, MDI);
  lua_setfield(L, -2, "MDI");
  lua_pushinteger(L, SDI);
  lua_setfield(L, -2, "SDI");
  lua_pushinteger(L, CREATELAZY);
  lua_setfield(L, -2, "CREATELAZY");
  lua_pushinteger(L, CREATEATSTART);
  lua_setfield(L, -2, "CREATEATSTART");

  lua_newtable(L); // table for canvasdata
  lua_setfield(L, -2, "userdata");
  lua_newtable(L); // table for canvasdata
  lua_newtable(L); // metatable
  lua_pushstring(L, "kv");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_setfield(L, -2, "weakuserdata");
  lua_pop(L, 1);
}

bool LuaProxy::IsPsyclePlugin() const {   
  bool result = lua_mac_ != 0;  
  if (!result) {
    lua_getglobal(L, "psycle");
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
    } else {
      lua_getfield(L, -1, "info");
      result = !lua_isnil(L, -1);      
      lua_pop(L, 2);
    }
  }  
  return result;
}

bool LuaProxy::HasDirectMetaAccess() const {
  bool result(false);
  lua_getglobal(L, "psycle");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
  } else {
    lua_getfield(L, -1, "info");
    result = !lua_isnil(L, -1);      
    lua_pop(L, 2);
  }
  return result;
}

PluginInfo LuaProxy::meta() const {
  if (is_meta_cache_updated_) {
    return meta_cache_;
  }
  PluginInfo result;  
  try {
    result = cache_meta(LuaControl::meta());
  } catch (std::exception&) {
    try {        
      LuaImport in(L, lua_mac_, this);
      if (!in.open("info")) {
        throw std::runtime_error("no info found"); 
      }
      in.pcall(1);    
      result = cache_meta(parse_info());
    } CATCH_WRAP_AND_RETHROW(host())
  }
  return result;
}

void LuaProxy::Init() {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("init")) {
      lua_pushnumber(L, Global::player().SampleRate());
      in.pcall(1);          
    } else {    
      const char* msg = "no init found";        
      throw std::runtime_error(msg);
    }    
  } catch(std::exception& e) {
    std::string msg = std::string("LuaProxy Init Errror.") + e.what();
    ui::alert(msg);
    throw std::runtime_error(msg.c_str());
  }
}
  
void LuaProxy::call_execute() {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("onexecute")) {
      in.pcall(0);      
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::OnActivated(int viewport) {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("onactivated")) {
      in << viewport << pcall(0);           
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::OnDeactivated() {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("ondeactivated")) {
      in.pcall(0);      
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::SequencerTick() {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("sequencertick")) {
      in.pcall(0);      
    }    
  } CATCH_WRAP_AND_RETHROW(host())
}

bool LuaProxy::DescribeValue(int param, char * txt) {
  try {
    if (host().crashed() || param < 0) {
      std::string par_display("Out of range or Crashed");
      std::sprintf(txt, "%s", par_display.c_str());
      return false;
    }
    if(param >= 0 && param < host().GetNumParams()) {
      try {
        std::string par_display = ParDisplay(param);
        std::string par_label = ParLabel(param);
        if (par_label == "")
          std::sprintf(txt, "%s", par_display.c_str());
        else {
          std::sprintf(txt, "%s(%s)", par_display.c_str(), par_label.c_str());
        }
        return true;
      } catch(std::exception &e) {
        e;
        std::string par_display("Out of range");
        std::sprintf(txt, "%s", par_display.c_str());
        return true;
      } //do nothing.
    }
    return false;
  } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host())
}

void LuaProxy::SeqTick(int channel, int note, int ins, int cmd, int val) {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("seqtick")) {
      in << channel << note << ins << cmd << val << pcall(0);   
    }
  } CATCH_WRAP_AND_RETHROW(host())
}
  
struct setcmd {
  void operator()(LuaImport& import) const {
    lua_State* L = import.L();        
    if (lastnote != notecommands::empty) {
      lua_pushinteger(L, lastnote);
    } else {
      lua_pushnil(L);
    }
    lua_pushinteger(L, inst);
    lua_pushinteger(L, cmd);
    lua_pushinteger(L, val);        
  }
  int lastnote, inst, cmd, val;
};

void LuaProxy::Command(int lastnote, int inst, int cmd, int val) {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("command")) {
      setcmd set = {lastnote, inst, cmd, val};
      in << set << pcall(0);      
    }
  } CATCH_WRAP_AND_RETHROW(host())
}
  
void LuaProxy::NoteOn(int note, int lastnote, int inst, int cmd, int val) {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("noteon")) {
      setcmd set = {lastnote, inst, cmd, val};
      in << note << set << pcall(0);      
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::NoteOff(int note, int lastnote, int inst, int cmd, int val) {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("noteoff")) {
      setcmd set = {lastnote, inst, cmd, val};
      in << note << set << pcall(0);      
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

std::string LuaProxy::call_help() { 
  std::string str;   
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("help")) {        
      in << pcall(1) >> str;   
    }       
  } CATCH_WRAP_AND_RETHROW(host())
  ui::alert(str);
  return str;
}

int LuaProxy::GetData(unsigned char **ptr, bool all) {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("data")) {      
      std::string str;
      in << all << pcall(1) >> str;
      *ptr = new unsigned char[str.size()];
      std::copy(str.begin(), str.end(), *ptr);
      return str.size();
    }      
    return 0;
  } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host()) 
}

uint32_t LuaProxy::GetDataSize() {
  try {
    std::string str;
    LuaImport in(L, lua_mac_, this);
    if (in.open("putdata")) {    
      in << pcall(1) >> str;
      return str.size();
    }
    return 0;
  } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host())
}

void LuaProxy::PutData(unsigned char* data, int size) {
  try {
    LuaImport in(L, lua_mac_, this);
    if (in.open("putdata")) {
      std::string s(reinterpret_cast<char const*>(data), size);    
      in << s << pcall(0);
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::Work(int numSamples, int offset) {
  try {
    if (numSamples > 0) {      
      LuaImport in(L, lua_mac_, this);
      if (in.open("work")) {
        if (offset > 0) {
          lua_mac()->offset(offset);
        }
        lua_mac()->update_num_samples(numSamples);
        in << numSamples << pcall(0);      
        if (offset > 0) {
          lua_mac()->offset(-offset);
        }      
        lua_gc(L, LUA_GCSTEP, 5);            
      }
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::Stop() {
  try {
    LuaImport in(L, lua_mac_, this);
    in.open("stop");
    in.pcall(0);
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::call_sr_changed(int rate) {
  try {
    LuaImport in(L, lua_mac_, this);
    in.open("onsrchanged"); 
    WaveOscTables::getInstance()->set_samplerate(rate);
    LuaDspFilterBind::setsamplerate(rate);
    LuaWaveOscBind::setsamplerate(rate);
    in << rate << pcall(0);
  } CATCH_WRAP_AND_RETHROW(host())
}

void LuaProxy::call_aftertweaked(int numparam) {
  LuaImport in(L, lua_mac_, this);
  in.open(); 
  in << getparam(numparam, "afternotify") << pcall(0);
}

// Parameter tweak range is [0..1]
void LuaProxy::ParameterTweak(int par, double val) {
  try {
    LuaImport in(L, lua_mac_, this);
    in.open(); 
    in << getparam(par, "setnorm") << val << pcall(0);
  } CATCH_WRAP_AND_RETHROW(host())
}

double LuaProxy::Val(int par) {
  try {
    double v(0);
    LuaImport in(L, lua_mac_, this);
    in.open(); 
    in << getparam(par, "norm") << pcall(1) >> v;
    return v;
  } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host())
}

void LuaProxy::Range(int par,int &minval, int &maxval) {
  try {
    int v(0);
    LuaImport in(L, lua_mac_, this);
    in.open(); 
    in << getparam(par, "range") << pcall(3) >> v >> v >> maxval;    
    minval = 0;
  } CATCH_WRAP_AND_RETHROW(host())
}

std::string LuaProxy::Name(int par) {
  try {
    std::string str;
    LuaImport in(L, lua_mac_, this);
    in.open(); 
    in << getparam(par, "name") << pcall(1) >> str;          
    return str;
  } CATCH_WRAP_AND_RETHROW(host())
  return "";
}

std::string LuaProxy::Id(int par) {
  try {
    std::string str;
    LuaImport in(L, lua_mac_, this);
    in.open(); 
    in << getparam(par, "id") << pcall(1) >> str;    
    return str;
  } CATCH_WRAP_AND_RETHROW(host())
  return "";
}

std::string LuaProxy::ParDisplay(int par) {    
  std::string str;
  LuaImport in(L, lua_mac_, this);
  in.open(); 
  in << getparam(par, "display") << pcall(1) >> str;    
  return str;    
}

std::string LuaProxy::ParLabel(int par) {    
  std::string str;
  LuaImport in(L, lua_mac_, this);
  in.open(); 
  in << getparam(par, "label") << pcall(1) >> str;    
  return str;    
}

int LuaProxy::Type(int par) {
  try {
    int v(0);
    LuaImport in(L, lua_mac_, this);
    in.open(); 
    in << getparam(par, "mpf") << pcall(1) >> v;    
    return v;
  } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host())
}

// call events
void LuaProxy::call_setprogram(int idx) {
  try {
    LuaImport in(L, lua_mac_, this);    
    if (in.open("setprogram")) {
      in << idx << pcall(0);
    }
  } CATCH_WRAP_AND_RETHROW(host())
}

// call events
int LuaProxy::get_curr_program() {
  try {
    int prog;
    LuaImport in(L, lua_mac_, this);    
    if (in.open("getprogram")) {
      in << pcall(1) >> prog;
    }
    return -1;
  } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host())
}

// call events
int LuaProxy::call_numprograms() {
    return lua_mac()->numprograms();
}

std::string LuaProxy::get_program_name(int bnkidx, int idx) {
  try {
    std::string str;
    LuaImport in(L, lua_mac_, this);    
    if (in.open("programname")) {
      in << bnkidx << idx << pcall(1) >> str;
    }    
    return str;
  } CATCH_WRAP_AND_RETHROW(host())
  return "";
}

bool LuaProxy::has_frame() const {
  return frame_.get() != 0;
}

void LuaProxy::OpenInFrame() {
  using namespace ui;
  if (!frame_ && !viewport().expired()) {
    frame_.reset(new Frame());    
    frame_->set_viewport(viewport().lock());
    frame_->close.connect(boost::bind(&LuaProxy::OnFrameClose, this, _1));
    FrameAligner::Ptr right_frame_aligner(new FrameAligner(AlignStyle::RIGHT));
    right_frame_aligner->SizeToScreen(0.4, 0.8);
    frame_->set_min_dimension(ui::Dimension(830, 600));
    frame_->Show(right_frame_aligner);
    Node::Ptr tmp = menu_root_node_.lock();
    frame_->SetMenuRootNode(tmp);
	frame_->KeyDown.connect(boost::bind(&LuaProxy::OnFrameKeyDown, this, _1));
	frame_->KeyUp.connect(boost::bind(&LuaProxy::OnFrameKeyUp, this, _1));
  }
}

void LuaProxy::OnFrameKeyDown(ui::KeyEvent& ev) {  
	CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
	CChildView* view = &main->m_wndView;
	UINT nFlags(0);		
	if (ev.extendedkey()) {
		nFlags |= 0x100;
	}
	view->KeyDown(ev.keycode(), 0, nFlags);
}

void LuaProxy::OnFrameKeyUp(ui::KeyEvent& ev) {  
	CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
	CChildView* view = &main->m_wndView;
	UINT nFlags(0);		
	if (ev.extendedkey()) {
		nFlags |= 0x100;
	}
	view->KeyUp(ev.keycode(), 0, nFlags);
}

void LuaProxy::UpdateFrameCanvas() {
  if (has_frame()) {
    if (!menu_root_node().expired()) {
      frame_->SetMenuRootNode(menu_root_node().lock());
    } else {
      Node::Ptr empty_menu;
      frame_->SetMenuRootNode(empty_menu);
    }
    frame_->set_viewport(viewport().lock());    
  }
}

void LuaProxy::RemoveCanvasFromFrame() {
  if (frame_ && !viewport().expired()) {
    frame_->set_viewport(ui::Frame::Ptr());
  }
}

void LuaProxy::ToggleViewPort() {    
  if (!has_frame()) {    
    assert(host_);
    host_->ViewPortChanged(*host_, FRAMEVIEWPORT); 
	OpenInFrame();   
  } else {
    frame_.reset();
    assert(host_);
    host_->ViewPortChanged(*host_, CHILDVIEWPORT);    
  }
}

void LuaProxy::UpdateWindowsMenu() {
  HostExtensions::Instance().ChangeWindowsMenuText(host_);
}

struct InvokeTimer {
  InvokeTimer(LuaRun* runnable, LuaProxy* proxy, int interval, bool timeout) : 
      interval_(static_cast<int>(interval / static_cast<double>(GlobalTimer::refresh_rate) + 0.5)),
      clock_(0),
      timeout_(timeout),
      runnable_(runnable),
      id_(++idcount),
      proxy_(proxy) {
  }  
  
  bool operator()() const {   
    bool result = proxy_->has_timer_clear(id_);
    if (!result) {
      ++clock_ ;
      if (clock_ >= interval_) {
        runnable_->Run();
        clock_ = 0;
        if (timeout_) {
          LuaHelper::unregister_userdata(runnable_->state(), runnable_);		 
        }
        result = timeout_;
      }
    }
    return result;
  }

  int id() const { return id_; }

 private:
   static int idcount;
   mutable int clock_;
   int interval_;
   bool timeout_;
   LuaRun* runnable_;
   int id_;
   LuaProxy* proxy_;
};

int InvokeTimer::idcount(0);

int LuaProxy::set_time_out(LuaRun* runnable, int interval) {
  InvokeTimer timeout(runnable, this, interval, true);
  timer_->Add(timeout);
  return timeout.id();
}

int LuaProxy::set_interval(LuaRun* runnable, int interval) {
  InvokeTimer timeinterval(runnable, this, interval, false);
  timer_->Add(timeinterval);
  return timeinterval.id();
}

void LuaProxy::clear_timer(int id) {
  clear_timers_.insert(id);
}

// End of Class Proxy

const char* LuaPluginBind::meta = "pluginmeta";

int  LuaPluginBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods);
}

int LuaPluginBind::create(lua_State* L) {
  const char* dllname = luaL_checkstring(L, 2);
  LuaHelper::new_shared_userdata(L, meta, new LuaPlugin(dllname, AUTOID));
  return 1;
}

// Class HostExtensions : Container for HostExtensions
HostExtensions& HostExtensions::Instance() {
  static HostExtensions extensions_;
  return extensions_;
}

void HostExtensions::InitInstance(HostViewPort* child_view) {
  child_view_ = child_view;
}

void HostExtensions::ExitInstance() {
  extensions_.clear();
}

void HostExtensions::StartScript() {    
  try {
    AutoInstall();
    LuaStarter starter;
    starter.Load(PsycleGlobal::configuration().GetAbsoluteLuaDir() + "\\" +
                 "start.lua");    
    starter.PrepareState();
    starter.Run();
  } catch (std::exception& e) {
    ui::alert(e.what());
  }
}

void HostExtensions::AutoInstall() {
  typedef std::vector<std::string> InstalledList;
  InstalledList installed = search_auto_install();
  PluginCatcher* plug_catcher = dynamic_cast<PluginCatcher*>(&Global::machineload()); 
	assert(plug_catcher);
  PluginInfoList list = plug_catcher->GetLuaExtensions();
	if (list.size() > 0) {	          
    std::string start_script_path =
        PsycleGlobal::configuration().GetAbsoluteLuaDir() + "\\start.lua";
    std::ofstream outfile;
    outfile.open(start_script_path.c_str(), std::ios_base::app);
	  PluginInfoList::iterator it = list.begin();	  
	  for (; it != list.end(); ++it) {
		  PluginInfo* info = *it;     		
      boost::filesystem::path p(info->dllname.c_str());            
	    InstalledList::iterator k = std::find(installed.begin(), installed.end(), p.filename().string());
      if (k == installed.end()) {         
         LuaControl mac;
         mac.Load(info->dllname.c_str());
         mac.PrepareState();
         mac.Run();
         std::string install_script = mac.install_script();                  
         //std::string install_script = luaL_checkstring(L, -1); // mac->install_script();                  
         outfile << std::endl << "-- @" << p.filename().string() << std::endl << install_script;         
      }
	  }         
  }
}

std::vector<std::string> HostExtensions::search_auto_install() {
  std::vector<std::string> result;
  std::string script_path = PsycleGlobal::configuration().GetAbsoluteLuaDir();
  std::string start_script_path = script_path + "\\start.lua";
  using namespace std;
  ifstream fileInput;  
  string line;
  char* search = "@"; // test variable to search in file
  // open file to search
  fileInput.open(start_script_path.c_str());
  if(fileInput.is_open()) {
    unsigned int curLine = 0;
    while(getline(fileInput, line)) { // I changed this, see below
      curLine++;
      if (line.find(search, 0) != string::npos) {
        cout << "found: " << search << "line: " << curLine << endl;
        std::string line_str(line.c_str());
        std::size_t pos = line_str.find("@");
        if (pos != std::string::npos) {
          result.push_back(line_str.substr(pos + 1));
        }       
      }
    }
    fileInput.close();
  }
  return result;
}

void HostExtensions::OnExecuteLink(Link& link) {
  child_view_->OnExecuteLink(link);  
}

void HostExtensions::AddViewMenu(Link& link) {
  child_view_->OnAddViewMenu(link);  
}

void HostExtensions::RestoreViewMenu() {
  child_view_->OnRestoreViewMenu();  
}

void HostExtensions::AddHelpMenu(Link& link) {  
  child_view_->OnAddHelpMenu(link);
}

void HostExtensions::ReplaceHelpMenu(Link& link, int pos) {
  child_view_->OnReplaceHelpMenu(link, pos);
}

std::string HostExtensions::menu_label(const Link& link) const {
  std::string result;
  if (!link.plugin.expired()) {  
    std::string filename_noext;
    filename_noext = boost::filesystem::path(link.dll_name()).stem().string();
    std::transform(filename_noext.begin(), filename_noext.end(), filename_noext.begin(), ::tolower);
    result = filename_noext + " - " + link.plugin.lock()->title();
  } else {
    result = link.label();
  }
  return result;
}

void HostExtensions::AddToWindowsMenu(Link& link) {
  child_view_->OnAddWindowsMenu(link);
}

void HostExtensions::RemoveFromWindowsMenu(LuaPlugin* plugin) {
  child_view_->OnRemoveWindowsMenu(plugin);
}

LuaPluginPtr HostExtensions::Execute(Link& link) {
	std::string script_path = PsycleGlobal::configuration().GetAbsoluteLuaDir();
	LuaPluginPtr mac;	
	mac.reset(new LuaPlugin(script_path + "\\" + link.dll_name().c_str(), -1));
	mac->proxy().set_userinterface(link.user_interface());
	mac->proxy().set_default_viewport_mode(link.viewport());
	mac->Init();			
	Add(mac); 
	mac->ViewPortChanged.connect(boost::bind(&HostExtensions::OnPluginViewPortChanged, this,  _1, _2));  
	link.plugin = mac;
	if (link.user_interface() == MDI) {
		AddToWindowsMenu(link); 
	}
	if (link.viewport() == TOOLBARVIEWPORT) {
		has_toolbar_extension_ = true;
	}	
	return mac;
}

void HostExtensions::FlsMain() {
   child_view_->OnFlsMain();
}

void HostExtensions::OnPluginViewPortChanged(LuaPlugin& plugin, int viewport) {
	switch (viewport) {
		case TOOLBARVIEWPORT:
			child_view_->ShowExtensionInToolbar(plugin);
		break;
		default:
			child_view_->OnHostViewportChange(plugin, viewport);
		break;
	}		
}

bool HostExtensions::HasToolBarExtension() const {
  return has_toolbar_extension_;
}

void HostExtensions::Free() {
  HostExtensions::List& plugs_ = extensions_;
  HostExtensions::List::iterator it = plugs_.begin();
  for ( ; it != plugs_.end(); ++it) {
    LuaPluginPtr ptr = *it;       
    ptr->Free();    
  }
}

HostExtensions::List HostExtensions::Get(const std::string& name) {
  HostExtensions::List list;
  HostExtensions::List& plugs_ = extensions_;
  HostExtensions::List::iterator it = plugs_.begin();
  for ( ; it != plugs_.end(); ++it) {
    LuaPluginPtr ptr = *it;       
    if (ptr->_editName == name) {
      list.push_back(ptr);
    }
  }
  return list;
}

LuaPluginPtr HostExtensions::Get(int idx) {
  HostExtensions::List list;     
  HostExtensions::List::iterator it = extensions_.begin();
  for (; it != extensions_.end(); ++it) {
    LuaPluginPtr ptr = *it;       
    if (ptr->_macIndex == idx) {
      return ptr;
    }
  }
  return nullPtr;
}

void HostExtensions::ChangeWindowsMenuText(LuaPlugin* plugin) {
  child_view_->OnChangeWindowsMenuText(plugin);
}

void HostExtensions::UpdateStyles() {
	HostExtensions::List::iterator it = extensions_.begin();
	for (; it != extensions_.end(); ++it) {
		LuaPluginPtr ptr = *it;
		if (!ptr->viewport().expired()) {
			ptr->viewport().lock()->RefreshRules();
			ptr->viewport().lock()->FLS();
		}
	}
}
   
// Host
std::map<lua_State*, LuaProxy*> LuaGlobal::proxy_map;  

lua_State* LuaGlobal::load_script(const std::string& dllpath) {
  lua_State* L = luaL_newstate();   
  luaL_openlibs(L);
  
  // set search path for require
  std::string filename_noext;
  boost::filesystem::path p(dllpath);
  std::string dir = p.parent_path().string();
  std::string fn = p.stem().string();
  lua_getglobal(L, "package");
  std::string path1 = dir + "/?.lua;" + dir + "/" + fn + "/?.lua;"+dir + "/"+ "psycle/?.lua";
  std::replace(path1.begin(), path1.end(), '/', '\\' );
  lua_pushstring(L, path1.c_str());
  lua_setfield(L, -2, "path");

  std::string path = dllpath;
  /// This is needed to prevent loading problems
  std::replace(path.begin(), path.end(), '\\', '/');
  int status = luaL_loadfile(L, path.c_str());
  if (status) {
    const char* msg =lua_tostring(L, -1);
    std::ostringstream s; s
      << "Failed: " << msg << std::endl;
    throw psycle::host::exceptions::library_errors::loading_error(s.str());
  }
  return L;
}

PluginInfo LuaGlobal::LoadInfo(const std::string& dllpath) {  
  LuaPlugin plug(dllpath, 0, false);
  return plug.info();    
}

Machine* LuaGlobal::GetMachine(int idx) {
    Machine* mac = 0;
    if (idx < MAX_MACHINES) {
      mac = Global::song()._pMachine[idx];
    } else {      
      mac = HostExtensions::Instance().Get(idx).get();      
    }
    return mac;
}

std::vector<LuaPlugin*> LuaGlobal::GetAllLuas() {
  std::vector<LuaPlugin*> plugs;
  for (int i=0; i < MAX_MACHINES; ++i) {
    Machine* mac = Global::song()._pMachine[i];
    if (mac && mac->_type == MACH_LUA) {
      plugs.push_back((LuaPlugin*)mac);
    }
  }
  HostExtensions& host_extensions = HostExtensions::Instance();
  for (HostExtensions::iterator it = host_extensions.begin();
       it != host_extensions.end(); ++it) {
    plugs.push_back((*it).get());
  }
  return plugs;
}

namespace luaerrhandler {

int error_handler(lua_State* L) {
	LuaProxy* proxy = LuaGlobal::proxy(L);
	if (proxy) {	 
		int oldviewportmode = CHILDVIEWPORT;   
		LuaGlobal::proxy(L)->psycle_meditation_->set_plugin(proxy->host());
		LuaGlobal::proxy(L)->ShowPsycleMeditation(lua_tostring(L, -1));
		LuaGlobal::proxy(L)->host().ViewPortChanged(proxy->host(), oldviewportmode); 
		LuaGlobal::proxy(L)->psycle_meditation_->FLS();		
	}


  // first make sure that the error didn't occured in the plugineditor itself
  std::string edit_name = LuaGlobal::proxy(L)->host().GetEditName();
  if (edit_name == "Plugineditor") {
    const char* msg = lua_tostring(L, -1);
    ui::alert(msg);
    return 1; // error in error handler
  }
  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");
  lua_pushvalue(L, 1);  // pass error message 
  lua_pushinteger(L, 2);  // skip this function and traceback
  lua_call(L, 2, 1);
  HostExtensions& host_extensions = HostExtensions::Instance();
  HostExtensions::List uilist = host_extensions.Get("Plugineditor");
  HostExtensions::iterator uit = uilist.begin();
  LuaPluginPtr editor;
  int viewportmode(FRAMEVIEWPORT);
  // try to find an open instance of plugineditor
  // editing the error plugin
  int macIdx = LuaGlobal::proxy(L)->host()._macIndex;    
  for ( ; uit != uilist.end(); ++uit) {
    LuaPluginPtr plug = *uit;
    LuaProxy& proxy = plug->proxy();
    int idx(-1);      
    LuaImport in(proxy.state(), proxy.lua_mac(), 0);
    if (in.open("editmachineindex")) {
      in << pcall(1) >> idx;
      if (macIdx == idx) {
        editor = plug;
		viewportmode = editor->proxy().has_frame() ? FRAMEVIEWPORT : CHILDVIEWPORT; 
        break;
      }
	  in.close();
    }
	if (!editor.get()) {
		LuaImport in(proxy.state(), proxy.lua_mac(), 0);
		if (in.open("projectinfo")) {
		  in.pcall(1);
		  if (!lua_isnil(L, -1)) {
		    int n = lua_gettop(proxy.state());
		    boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(proxy.state(), n, LuaPluginInfoBind::meta);						
			if (ud->dllname == LuaGlobal::proxy(L)->host().GetDllName()) {
				editor = plug;
				viewportmode = editor->proxy().has_frame() ? FRAMEVIEWPORT : CHILDVIEWPORT;
				break;
			}
		  }	
		  in.close();
		}
	}
  }
  if (!editor.get()) {    
    Link link("plugineditor.lua",
              "Plugin Editor",
              FRAMEVIEWPORT,
              MDI);
    try {
      editor = host_extensions.Execute(link);    
    } catch (std::exception&) {
      return 1;
    }
  }
  if (!editor.get()) {
    return 1; // uhps, plugin editor luascript missing ..
  }    
  lua_State* LE = editor->proxy().state();  
  LuaImport in(LE, editor->proxy().lua_mac(), 0);
  try {
    if (in.open("onexecute")) {      
      lua_pushstring(LE, lua_tostring(L, -1));
      lua_pushnumber(LE, LuaGlobal::proxy(L)->host()._macIndex);
      std::string filename_noext;
      filename_noext = boost::filesystem::path(
          LuaGlobal::proxy(L)->host().GetDllName()).stem().string();
      std::transform(filename_noext.begin(), filename_noext.end(),
          filename_noext.begin(), ::tolower);
      PluginCatcher* plug_catcher =
        static_cast<PluginCatcher*>(&Global::machineload());
      PluginInfo* info = plug_catcher->info(filename_noext);
      if (info) {       
        LuaHelper::requirenew<LuaPluginInfoBind>(LE, "psycle.plugininfo",
            new PluginInfo(*info));
      } else {        
        lua_pop(LE, 4);
        throw std::runtime_error("No Plugininfo available.");
      }       
      int i(1);
      int depth(0);
      lua_Debug entry;
      lua_newtable(LE);
      while (lua_getstack(L, depth, &entry)) {
        int status = lua_getinfo(L, "nSl", &entry);
        assert(status);
        if (status == 0) {
          luaL_error(L, "lua getinfo error");
        }
        lua_newtable(LE);
        lua_pushinteger(LE, entry.linedefined);
        lua_setfield(LE, -2, "line");
        lua_pushstring(LE, entry.source);
        lua_setfield(LE, -2, "source");
        lua_pushstring(LE, entry.name ? entry.name : "");
        lua_setfield(LE, -2, "name");
        lua_rawseti(LE, -2, i);
        ++i;
        ++depth;
      }              
      in.pcall(0);          
      editor->ViewPortChanged(*editor.get(), viewportmode);          
      editor->OnActivated(viewportmode);
	  if (viewportmode == FRAMEVIEWPORT) {
        editor->proxy().OpenInFrame();
	  }
      host_extensions.ChangeWindowsMenuText(editor.get());
      return 1;
    }
  } catch (std::exception& e ) {
    // error in plugineditor    
    ui::alert((std::string("Error in error handler! Plugineditor failed!") +
               std::string(e.what())).c_str());    
  }  
  return 1;
}

} // namespace luaerrhandler
} // namespace host
} // namespace psycle