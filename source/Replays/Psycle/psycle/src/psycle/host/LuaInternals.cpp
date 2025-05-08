// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#include <psycle/host/detail/project.hpp>

#include "LuaInternals.hpp"
#include "LuaPlugin.hpp"
#include "Player.hpp"
#include "Registry.hpp"
#include "WinIniFile.hpp"
#include "Configuration.hpp"
#include "PsycleConfig.hpp"

#if 0//!defined WINAMP_PLUGIN
#include "PlotterDlg.hpp"
#endif //!defined WINAMP_PLUGIN

#include "plugincatcher.hpp"
#include "Song.hpp"

#include <boost/filesystem.hpp>
#include <psycle/helpers/resampler.hpp>
#include <universalis/os/terminal.hpp>
#include "Mainfrm.hpp"
#include "LuaGui.hpp"

#include <lua.hpp>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include "PsycleConfig.hpp"
#include "PresetIO.hpp"

namespace psycle {
namespace host {
/////////////////////////////////////////////////////////////////////////////
// LuaConfig+Bind
/////////////////////////////////////////////////////////////////////////////

LuaConfig::LuaConfig() : store_(PsycleGlobal::conf().CreateStore()) { }

LuaConfig::LuaConfig(const std::string& group)
  : store_(PsycleGlobal::conf().CreateStore()){
    OpenGroup(group);
}

void LuaConfig::OpenGroup(const std::string& group) {
  // Do not open group if loading version 1.8.6
  if(!store_->GetVersion().empty()) {
		store_->OpenGroup(group);
	}
}

void LuaConfig::CloseGroup() {
  // Do not open group if loading version 1.8.6
  if(!store_->GetVersion().empty()) {
		store_->CloseGroup();
	}
}

ui::ARGB LuaConfig::color(const std::string& key) {  
  COLORREF color;
  using namespace ui;  
  store_->Read(key, color);  
  return ToARGB(color);      
}

// MidiHelperBind
int LuaStockBind::open(lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"value", value},
    {"key", key},
    {NULL, NULL}
  };  
  luaL_newlib(L, funcs);
  lua_newtable(L);
  {    
    static const char* const e[] = {"PVBACKGROUND", "PVROW", "PVROW4BEAT", "PVROWBEAT", "PVSEPARATOR",
                "PVFONT", "PVCURSOR", "PVSELECTION", "PVPLAYBAR"};               
    size_t size = sizeof(e)/sizeof(e[0]);
    LuaHelper::buildenum(L, e, size, 1);
  }  
  {    
    static const char* const e[] = {"MVFONTBOTTOMCOLOR", "MVFONTHTOPCOLOR",
                "MVFONTHBOTTOMCOLOR", "MVFONTTITLECOLOR", "MVTOPCOLOR", "MVBOTTOMCOLOR", 
                "MVHTOPCOLOR", "MVHBOTTOMCOLOR", "MVTITLECOLOR"};               
    size_t size = sizeof(e)/sizeof(e[0]);
    LuaHelper::buildenum(L, e, size, 20);
  }  
  lua_setfield(L, -2, "color");
  return 1;
}

/*
  
bool search_pair (lua_State *L, std::string& keystr, int index) {
  bool result = false;
  if (lua_type(L, index) == LUA_TSTRING) {
     keystr = std::string(lua_tostring(L, index));
     result = true;
  }
  return result;
}

void walk(lua_State* L, int enum_key, std::string& keystr, int index) {
  lua_pushvalue(L, lua_gettop(L));
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    lua_pushvalue(L, -2);
    const char *key = lua_tostring(L, -1);
    if (lua_type(L, -2) == LUA_TNUMBER && enum_key == lua_tonumber(L, -2)) {
      keystr = enum_key;
      result = false;
    }
    const char *value = lua_tostring(L, -2);
    if 
  }
}
*/
int LuaStockBind::key(lua_State* L) {  
  int stock_key = luaL_checkinteger(L, 1);  
  PsycleStock stock;
  lua_pushstring(L, stock.key_tostring(stock_key).c_str());
  return 1;  
}

int LuaStockBind::value(lua_State* L) {
  using namespace ui;
  int stock_key = luaL_checkinteger(L, 1);    
  PsycleStock stock;
  ui::MultiType result =stock.value(stock_key);  
  if (result.which() == 1) {
    lua_pushnumber(L, boost::get<ARGB>(stock.value(stock_key)));
  } else {
    luaL_error(L, "Wrong Property type.");
  }
  return 1;  
}

const char* LuaConfigBind::meta = "psyconfigmeta";

int LuaConfigBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"get", get},
    {"opengroup", opengroup},
    {"closegroup", closegroup},
    {"groups", groups},
    {"keys", keys},
    {"keytocmd", keytocmd},
    {"octave", octave},
    {"luapath", plugindir},
    {"presetpath", presetdir},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods,  gc);  
  return 1;
}

int LuaConfigBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1 and n!=2) {
    return luaL_error(L, "Got %d arguments expected 2 (self or self and group)", n);
  }
  LuaConfig* cfg = (n==1) ?  new LuaConfig() : new LuaConfig(luaL_checkstring(L, 2));
  LuaHelper::new_shared_userdata<LuaConfig>(L, meta, cfg);    
  return 1;
}

int LuaConfigBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaConfig>(L, meta);
}

int LuaConfigBind::opengroup(lua_State *L) {
  LuaHelper::bind<LuaConfig>(L, meta, &LuaConfig::OpenGroup);
  return LuaHelper::chaining(L);
}

int LuaConfigBind::closegroup(lua_State *L) {
  LuaHelper::bind(L, meta, &LuaConfig::CloseGroup);
  return LuaHelper::chaining(L);
}

int LuaConfigBind::get(lua_State *L) {
  int err = LuaHelper::check_argnum(L, 2, "self, key");
  if (err!=0) return err;
  boost::shared_ptr<LuaConfig> cfg = LuaHelper::check_sptr<LuaConfig>(L, 1, meta);
  ConfigStorage* store = cfg->store();
  if (store) {
    const std::string key(luaL_checkstring(L, 2));    
    if (key=="dialwidth") {      
      lua_pushnumber(L, PsycleGlobal::conf().macParam().dialwidth);
    } else if (key=="dialheight") {      
      lua_pushnumber(L, PsycleGlobal::conf().macParam().dialheight);
    } else {      
      lua_pushnumber(L, cfg->color(key));
    }
  } else {
    return 0;
  }
  return 1;
}

int LuaConfigBind::groups(lua_State *L) {
  boost::shared_ptr<LuaConfig> cfg = LuaHelper::check_sptr<LuaConfig>(L, 1, meta);
  typedef std::list<std::string> storelist;
  storelist l = cfg->store()->GetGroups();
  lua_newtable(L);
  int i=1;
  for (storelist::iterator it = l.begin(); it!=l.end(); ++it, ++i) {
    const char* name = (*it).c_str();
    lua_pushstring(L, name);
    lua_rawseti(L, -2, i);
  }
  return 1;
}

int LuaConfigBind::keys(lua_State* L) {
  boost::shared_ptr<LuaConfig> cfg = LuaHelper::check_sptr<LuaConfig>(L, 1, meta);
  typedef std::list<std::string> storelist;
  storelist l = cfg->store()->GetKeys();
  lua_newtable(L);
  int i=1;
  for (storelist::iterator it = l.begin(); it!=l.end(); ++it, ++i) {
    const char* name = (*it).c_str();
    lua_pushstring(L, name);
    lua_rawseti(L, -2, i);
  }
  return 1;
}

int LuaConfigBind::keytocmd(lua_State* L) {
  boost::shared_ptr<LuaConfig> cfg = LuaHelper::check_sptr<LuaConfig>(L, 1, meta);
  ui::alert("in cmddef implemented.");
  return 1;
}

int LuaConfigBind::octave(lua_State* L) {
  boost::shared_ptr<LuaConfig> cfg = LuaHelper::check_sptr<LuaConfig>(L, 1, meta);
  char c = luaL_checknumber(L, 2);
  CmdDef cmd = PsycleGlobal::inputHandler().KeyToCmd(c, 0);
  lua_pushnumber(L, cmd.GetNote());
  return 1;
}

int LuaConfigBind::plugindir(lua_State* L) {
  lua_pushstring(L, PsycleGlobal::configuration().GetAbsoluteLuaDir().c_str());
  return 1;
}

int LuaConfigBind::presetdir(lua_State* L) {
  lua_pushstring(L, PsycleGlobal::conf().GetAbsolutePresetsDir().c_str());
  return 1;
}

// PluginInfoBind

const char* LuaPluginInfoBind::meta = "psyplugininfometa";

int LuaPluginInfoBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"identifier", identifier},
    {"dllname", dllname},
    {"error", error},
    {"name", name},
    {"type", type},
    {"mode", mode},   
    {"vendor", vendor},
    {"description", description},
    {"version", version},
    {"apiversion", apiversion},
    {"allow", allow},	 
    { NULL, NULL }
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaPluginInfoBind::create(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  LuaHelper::new_shared_userdata<PluginInfo>(L, meta, new PluginInfo());  
  return 1;
}

int LuaPluginInfoBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<PluginInfo>(L, meta);
  return 0;
}

int LuaPluginInfoBind::dllname(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushstring(L, ud->dllname.c_str());
  return 1;
}

int LuaPluginInfoBind::description(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushstring(L, ud->desc.c_str());
  return 1;
}

int LuaPluginInfoBind::version(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushstring(L, ud->version.c_str());
  return 1;
}

int LuaPluginInfoBind::identifier(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushinteger(L, ud->identifier);
  return 1;
}

int LuaPluginInfoBind::vendor(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushstring(L, ud->vendor.c_str());
  return 1;
}

int LuaPluginInfoBind::error(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushstring(L, ud->error.c_str());
  return 1;
}

int LuaPluginInfoBind::apiversion(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushinteger(L, ud->APIversion);
  return 1;
}

int LuaPluginInfoBind::allow(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushboolean(L, ud->allow);
  return 1;
}    

int LuaPluginInfoBind::name(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushstring(L, ud->name.c_str());
  return 1;
}

int LuaPluginInfoBind::type(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushnumber(L, ud->type);
  return 1;
}

int LuaPluginInfoBind::mode(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<PluginInfo> ud = LuaHelper::check_sptr<PluginInfo>(L, 1, meta);  
  lua_pushnumber(L, ud->mode);
  return 1;
}

// LuaPluginCatcherBind
const char* LuaPluginCatcherBind::meta = "psyplugincatchermeta";

int LuaPluginCatcherBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"info", info},
    {"infos", infos},
    {"rescanall", rescanall},
    {"rescannew", rescannew},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaPluginCatcherBind::create(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  PluginCatcher* plug_catcher =
    static_cast<PluginCatcher*>(&Global::machineload());
  LuaHelper::new_shared_userdata<>(L, meta, plug_catcher, 1, true);
  return 1;
}

int LuaPluginCatcherBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<PluginCatcher>(L, meta);
  return 0;
}

int LuaPluginCatcherBind::info(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, name)", n);
  }
  boost::shared_ptr<PluginCatcher> ud = LuaHelper::check_sptr<PluginCatcher>(L, 1, meta);
  const char* name = luaL_checkstring(L, 2); 
  PluginInfo* info = ud->info(name);
  if (info) {
    luaL_requiref(L, "psycle.plugininfo", LuaPluginInfoBind::open, true);
    LuaHelper::new_shared_userdata<PluginInfo>(L, LuaPluginInfoBind::meta, new PluginInfo(*info), 3);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int LuaPluginCatcherBind::infos(lua_State* L) {
  boost::shared_ptr<PluginCatcher> catcher = LuaHelper::check_sptr<PluginCatcher>(L, 1, meta);
  const int num_plugins = static_cast<PluginCatcher*>(&Global::machineload())->_numPlugins;
  PluginInfo** const _pPlugsInfo = static_cast<PluginCatcher*>(&Global::machineload())->_pPlugsInfo;
  lua_newtable(L);
  for (int i=0; i < num_plugins; ++i) {
    PluginInfo* plugin_info = _pPlugsInfo[i];
    PluginInfo* new_plugin_info = new PluginInfo(*plugin_info);    
    luaL_requiref(L, "psycle.plugininfo", LuaPluginInfoBind::open, true);
    LuaHelper::new_shared_userdata<PluginInfo>(L, LuaPluginInfoBind::meta, new_plugin_info, 3);
    lua_remove(L, -2);
    lua_rawseti(L, -2, i+1);
  }
  return 1;
}

int LuaPluginCatcherBind::rescanall(lua_State* L) {
  boost::shared_ptr<PluginCatcher> catcher = LuaHelper::check_sptr<PluginCatcher>(L, 1, meta);
  catcher->ReScan(true);
  return LuaHelper::chaining(L);
}

int LuaPluginCatcherBind::rescannew(lua_State* L) {
  boost::shared_ptr<PluginCatcher> catcher = LuaHelper::check_sptr<PluginCatcher>(L, 1, meta);
  catcher->ReScan(false);
  return LuaHelper::chaining(L);
}


/////////////////////////////////////////////////////////////////////////////
// LuaMachine+Bind
/////////////////////////////////////////////////////////////////////////////


LuaMachine::LuaMachine(lua_State* L) 
    : L(L),
	  proxy_(0),
      mac_(0),
      shared_(false),
      num_parameter_(0),
      num_cols_(0),
      ui_type_(MachineUiType::NATIVE),      
      prsmode_(MachinePresetType::NATIVE),
      num_programs_(0) {
}


LuaMachine::~LuaMachine() {  
  if (mac_ != 0 && !shared_) {   
    delete mac_;
  }
  mac_ = 0;
}

void LuaMachine::lock() const {
  if (proxy_) {
    proxy_->lock();
  }
}

void LuaMachine::unlock() const {
  if (proxy_) {
    proxy_->unlock();
  }
}

void LuaMachine::load(const char* name) {
  PluginCatcher* plug_catcher =
    static_cast<PluginCatcher*>(&Global::machineload());
  PluginInfo* info = plug_catcher->info(name);
  if (info) {
    Song& song =  Global::song();
    mac_ = song.CreateMachine(info->type, info->dllname.c_str(), AUTOID, 0);
    mac_->Init();
    build_buffer(mac_->samplesV, 256);
    shared_ = false;
    num_cols_ = mac_->GetNumCols();
  } else {
    mac_ = 0;
    throw std::runtime_error("plugin not found error");
  }
}

void LuaMachine::work(int samples) {
  update_num_samples(samples);
  mac_->GenerateAudio(samples, false);
}

void LuaMachine::build_buffer(std::vector<float*>& buf, int num) {
  sampleV_.clear();
  std::vector<float*>::iterator it = buf.begin();
  for ( ; it != buf.end(); ++it) {
    sampleV_.push_back(PSArray(*it, num));
  }
}

void LuaMachine::set_buffer(std::vector<float*>& buf) {
  mac_->change_buffer(buf);
  build_buffer(mac_->samplesV, 256);
}

void LuaMachine::update_num_samples(int num) {
  psybuffer::iterator it = sampleV_.begin();
  for ( ; it != sampleV_.end(); ++it) {
    (*it).set_len(num);
  }
}

void LuaMachine::offset(int offset) {
  psybuffer::iterator it = sampleV_.begin();
  for ( ; it != sampleV_.end(); ++it) {
    (*it).offset(offset);
  }
}

void LuaMachine::doexit() {
  LuaPlugin* plug = LuaGlobal::GetLuaPlugin(mac()->_macIndex);
  if (plug) {
    plug->DoExit();
  }
}

void LuaMachine::reload() {
  LuaPlugin* plug = LuaGlobal::GetLuaPlugin(mac()->_macIndex);
  if (plug) {
    plug->DoReload();  
  }
}

void LuaMachine::set_viewport(const ui::Viewport::WeakPtr& viewport) { 
  viewport_ = viewport;
 // proxy_->OnCanvasChanged();
}

void LuaMachine::ToggleViewPort() {
  proxy_->ToggleViewPort();
}

// PsycleCmdDefBind
int LuaCmdDefBind::open(lua_State* L) {   
  static const luaL_Reg funcs[] = {
    {"keytocmd", keytocmd},
	{"cmdtokey", cmdtokey},
	{"currentoctave", currentoctave},        
    {NULL, NULL}
  };
  luaL_newlib(L, funcs);
  {
	static const char* const e[] = {
		"KEYC_0", "KEYCS0", "KEYD_0", "KEYDS0", "KEYE_0", "KEYF_0",
		"KEYFS0", "KEYG_0", "KEYGS0", "KEYA_0", "KEYAS0", "KEYB_0",
		"KEYC_1", "KEYCS1", "KEYD_1", "KEYDS1", "KEYE_1", "KEYF_1",
		"KEYFS1", "KEYG_1", "KEYGS1", "KEYA_1", "KEYAS1", "KEYB_1",
		"KEYC_2", "KEYCS2", "KEYD_2", "KEYDS2", "KEYE_2", "KEYF_2",
		"KEYFS2", "KEYG_2", "KEYGS2", "KEYA_2", "KEYAS2", "KEYB_2",
		"KEYC_3", "KEYCS3", "KEYD_3", "KEYDS3", "KEYE_3", "KEYF_3",
		"KEYFS3", "KEYG_3", "KEYGS3", "KEYA_3", "KEYAS3", "KEYB_3"
	};
	size_t size = sizeof(e)/sizeof(e[0]);    
	LuaHelper::buildenum(L, e, size, CS_KEY_START);
	LuaHelper::setfield(L, "KEYSTOP", cdefKeyStop);
  }
  {	 
	  static const char* const e[] = {
		"TRANSPOSECHANNELINC", "TRANSPOSECHANNELDEC", "TRANSPOSECHANNELINC12", 
		"TRANSPOSECHANNELDEC12", "TRANSPOSEBLOCKINC", "TRANSPOSEBLOCKDEC",
		"TRANSPOSEBLOCKINC12", "TRANSPOSEBLOCKDEC12", "PATTERNCUT",
		"PATTERNCOPY", "PATTERNPASTE", "ROWINSERT", "ROWDELETE", "ROWCLEAR",
		"BLOCKSTART", "BLOCKEND", "BLOCKUNMARK", "BLOCKDOUBLE", "BLOCKHALVE",
		"BLOCKCUT", "BLOCKCOPY", "BLOCKPASTE", "BLOCKMIX", "BLOCKINTERPOLATE",
		"BLOCKSETMACHINE", "BLOCKSETINSTR", "SELECTALL", "SELECTCOL",
		"EDITQUANTIZEDEC", "EDITQUANTIZEINC", "PATTERNMIXPASTE",
		"PATTERNTRACKMUTE", "KEYSTOPANY", "PATTERNDELETE", "BLOCKDELETE",
		"PATTERNTRACKSOLO", "PATTERNTRACKRECORD", "SELECTBAR"
		};
		size_t size = sizeof(e)/sizeof(e[0]);    
		LuaHelper::buildenum(L, e, size, CS_EDT_START);
  }
  {
		static const char* const e[] = {"NULL", "NOTE", "EDITOR", "IMMEDIATE"};
		size_t size = sizeof(e)/sizeof(e[0]);
		LuaHelper::buildenum(L, e, size, 0);
	  }
  {
	  static const char* const e[] = {
		"EDITTOGGLE", "OCTAVEUP", "OCTAVEDN", "MACHINEDEC", "MACHINEINC",
		"INSTRDEC", "INSTRINC", "PLAYROWTRACK", "PLAYROWPATTERN", "PLAYSTART",
		"PLAYSONG", "PLAYFROMPOS", "PLAYBLOCK", "PLAYSTOP", "INFOPATTERN",
		"INFOMACHINE", "EDITMACHINE", "EDITPATTERN", "EDITINSTR", "ADDMACHINE",
		"PATTERNINC", "PATTERNDEC",	"SONGPOSINC", "SONGPOSDEC", "COLUMNPREV",
		"COLUMNNEXT", "NAVUP", "NAVDN", "NAVLEFT", "NAVRIGHT",
		"NAVPAGEUP", "NAVPAGEDN", "NAVTOP", "NAVBOTTOM", "SELECTMACHINE",
		"UNDO", "REDO", "FOLLOWSONG", "MAXPATTERN",
	  };	  
	  size_t size = sizeof(e)/sizeof(e[0]);    
	  LuaHelper::buildenum(L, e, size, CS_IMM_START);	 	  
  }
  return 1;
};

void LuaMachine::set_title(const std::string& title) { 
  title_ = title; 
  if (proxy_) {
     proxy_->UpdateWindowsMenu();
  }
}

/*void LuaMachine::OnMachineCreate(Machine& machine) {	
	LuaImport in(L, this, LuaGlobal::proxy(L));
	if (in.open("onmachinecreate")) {
		LuaMachine* lua_machine = new LuaMachine(L);
		lua_machine->set_mac(&machine);
		LuaHelper::requirenew<LuaMachineBind>(L, "psycle.machine", lua_machine);
		LuaMachineBind::createparams(L, lua_machine);
		in << pcall(0);
	}	
}*/

/*void LuaMachine::BeforeMachineDelete(Machine& machine) {
	try {
		LuaImport in(L, this, LuaGlobal::proxy(L));
		if (in.open("beforemachinedelete")) {
			LuaMachine* lua_machine = new LuaMachine(L);
			lua_machine->set_mac(&machine);			
			LuaHelper::requirenew<LuaMachineBind>(L, "psycle.machine", lua_machine);
			LuaMachineBind::createparams(L, lua_machine);
			in << pcall(0);
			lua_machine->set_mac(0);					 
		}
	}  catch (std::exception& e) {
	    LuaHelper::collect_full_garbage(L);
		throw std::runtime_error(e.what());
	}
	LuaHelper::collect_full_garbage(L);
}*/

int LuaCmdDefBind::keytocmd(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 0 || n > 4) {
    return luaL_error(L, "Wrong number of arguments.");
  }
  int keycode = luaL_checknumber(L, 1);
  UINT nFlags(0);
  bool hasshift(false);
  bool hasctrl(false);
  if (lua_gettop(L) > 1) {
    hasshift = lua_toboolean(L, 2);    
  }
  if (lua_gettop(L) > 2) {
    hasctrl = lua_toboolean(L, 3);	
  }
  if (lua_gettop(L) > 3) {
    bool hasextended = lua_toboolean(L, 4);
	if (hasextended) {
	  nFlags |= (1<<8); // set extended keyboard
	}
  }
  CmdDef cmd = PsycleGlobal::inputHandler().KeyToCmd(keycode, nFlags, hasshift,
      hasctrl); 
  lua_createtable(L, 0, 3);
  LuaHelper::setfield(L, "note", cmd.GetNote());
  LuaHelper::setfield(L, "id", cmd.GetID());
  LuaHelper::setfield(L, "type", cmd.GetType());  
  return 1;
}

int LuaCmdDefBind::cmdtokey(lua_State* L) {
  int id = luaL_checkinteger(L, -1);
  WORD key(0);
  WORD mods(0);
  PsycleGlobal::inputHandler().CmdToKey((CmdSet)id, key, mods);
  lua_pushinteger(L, key);
  bool has_shift = mods&HOTKEYF_SHIFT;
  bool has_ctrl = mods&HOTKEYF_CONTROL;
  bool is_extended = mods&HOTKEYF_EXT;
  lua_pushboolean(L, has_shift);
  lua_pushboolean(L, has_ctrl);
  lua_pushboolean(L, is_extended);
  return 4;
}

int LuaCmdDefBind::currentoctave(lua_State* L) {
  int octave = Global::song().currentOctave;
  lua_pushinteger(L, octave);
  return 1;
}
  
// PsycleActions + Lua Bind
void LuaActionListener::OnNotify(const ActionHandler& handler, ActionType action) {   
	std::string action_str = handler.ActionAsString(action);
	if (action_str != "") {
		LuaImport in(L, this, LuaGlobal::proxy(L));
		if (in.open("on" + action_str)) {
			in << static_cast<int>(action) << pcall(0);
		} 
	}
}

const char* LuaActionListenerBind::meta = "psyactionlistenermeta";

int LuaActionListenerBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    { NULL, NULL }
  };
  LuaHelper::open(L, meta, methods,  gc);
  static const char* const e[] = {
    "TPB", "BPM", "TRKNUM", "PLAY", "PLAYSTART", "PLAYSEQ", "STOP", "REC",
    "SEQSEL", "SEQMODIFIED", "SEQFOLLOWSONG", "PATKEYDOWN", "PATKEYUP",
	"SONGLOAD", "SONGLOADED", "SONGNEW", "TRACKNUMCHANGED", "OCTAVEUP", 
	"OCTAVEDOWN", "UNDOPATTERN", "PATTERNLENGTH"
  };
  LuaHelper::buildenum(L, e, sizeof(e)/sizeof(e[0]), 1);
  return 1;
}

int LuaActionListenerBind::create(lua_State* L) {
  LuaHelper::createwithstate<LuaActionListener>(L, meta, true);
  return 1;
}

int LuaActionListenerBind::gc(lua_State* L) {
  typedef boost::shared_ptr<LuaActionListener> T;
  T listener = *(T *)luaL_checkudata(L, 1, meta);
  PsycleGlobal::actionHandler().RemoveListener(listener.get());
  return LuaHelper::delete_shared_userdata<LuaActionListener>(L, meta);
}


const char* LuaScopeMemoryBind::meta = "psyscopemeta";

int LuaScopeMemoryBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
	{"channel", channel},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods,  gc);  
  return 1;
}

int LuaScopeMemoryBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }  
  LuaHelper::new_shared_userdata<ScopeMemory>(L, meta, new ScopeMemory());
  return 1;
}

int LuaScopeMemoryBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ScopeMemory>(L, meta);
}

int LuaScopeMemoryBind::channel(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<ScopeMemory> scope_memory = LuaHelper::check_sptr<ScopeMemory>(L, 1, meta);
    int idx = luaL_checknumber(L, 2);
    PSArray ** udata = (PSArray **)lua_newuserdata(L, sizeof(PSArray *));  
	if (idx == 0)  {
      *udata = new PSArray(scope_memory->samples_left->data(), scope_memory->samples_left->len());
	} else
	if (idx == 1)  {
	  *udata = new PSArray(scope_memory->samples_right->data(), scope_memory->samples_right->len());
	} else {
	  luaL_error(L, "Wrong channel index. Only stereo so far supported.");
	}
    luaL_setmetatable(L, LuaArrayBind::meta);
  }  else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return 1;
}

/////////////////////////////////////////////////////////////////////////////
// LuaMachineBind 
/////////////////////////////////////////////////////////////////////////////

const char* LuaMachineBind::meta = "psypluginmeta";

int LuaMachineBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"work", work},
    {"info2", info2},
    {"pluginname", pluginname},
    {"tick", tick},
    {"channel", channel},
    {"setnumchannels", set_numchannels},
    {"numchannels", numchannels},
    {"setnumprograms", set_numprograms},
    {"numprograms", numprograms},
    {"resize", resize},
    {"setbuffer", setbuffer},
    {"addparameters", add_parameters},
    {"setparameters", set_parameters},
    {"setmenurootnode", setmenurootnode},
    {"setnumcols", set_numcols},
    {"numcols", numcols},
    {"shownativegui", show_native_gui},
    {"showcustomgui", show_custom_gui},
    {"showchildviewgui", show_childview_gui},
    {"parambyid", getparam},
    {"setpresetmode", setpresetmode},
    {"addhostlistener", addhostlistener},
    {"exit", exit},
    {"reload", reload},
    {"setviewport", setviewport},
    {"toggleviewport", toggleviewport},
    {"type", type},  
    {"settitle", settitle},
    {"settimeout", settimeout},
    {"setinterval", setinterval},
    {"cleartimeout", cleartimer},
    {"clearinterval", cleartimer},
	{"automate", automate},
	{"insert", insert},
	{"muted", muted},
	{"at", at},
	{"buildbuffer", buildbuffer},
	{"addscopememory", addscopememory},
	{"index", index},
	{"audiorange", audiorange},
	{"paramrange", paramrange},
	{"paramname", paramname},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods,  gc);
  // define constants
  LuaHelper::setfield(L, "FX", MACHMODE_FX);
  LuaHelper::setfield(L, "GENERATOR", MACHMODE_GENERATOR);
  LuaHelper::setfield(L, "HOST", MACHMODE_HOST);
  LuaHelper::setfield(L, "PRSNATIVE", 3);  
  LuaHelper::setfield(L, "PRSCHUNK", 1);
  LuaHelper::setfield(L, "MACH_UNDEFINED", -1);
	LuaHelper::setfield(L, "MACH_MASTER", 0);
	LuaHelper::setfield(L, "MACH_SINE", 1); ///< now a plugin
	LuaHelper::setfield(L, "MACH_DIST", 2); ///< now a plugin
  LuaHelper::setfield(L, "MACH_SAMPLER", 3);
  LuaHelper::setfield(
  L, "MACH_DELAY", 4); ///< now a plugin
	LuaHelper::setfield(L, "MACH_2PFILTER", 5); ///< now a plugin
	LuaHelper::setfield(L, "MACH_GAIN", 6); ///< now a plugin
	LuaHelper::setfield(L, "MACH_FLANGER", 7); ///< now a plugin
	LuaHelper::setfield(L, "MACH_PLUGIN", 8);
	LuaHelper::setfield(L, "MACH_VST", 9);
	LuaHelper::setfield(L, "MACH_VSTFX", 10);
	LuaHelper::setfield(L, "MACH_SCOPE", 11); ///< Test machine. removed
	LuaHelper::setfield(L, "MACH_XMSAMPLER", 12);
	LuaHelper::setfield(L, "MACH_DUPLICATOR", 13);
	LuaHelper::setfield(L, "MACH_MIXER", 14);
	LuaHelper::setfield(L, "MACH_RECORDER", 15);
	LuaHelper::setfield(L, "MACH_DUPLICATOR2", 16);
	LuaHelper::setfield(L, "MACH_LUA", 17);
	LuaHelper::setfield(L, "MACH_LADSPA", 18);
	LuaHelper::setfield(L, "MACH_DUMMY", 255);
  return 1;
}

int LuaMachineBind::create(lua_State* L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n==1) {
    LuaHelper::new_shared_userdata<LuaMachine>(L, meta, new LuaMachine(L));
    lua_newtable(L);
    lua_setfield(L, -2, "params");
    return 1;
  }
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 1 (pluginname)", n);
  }
  LuaMachine* udata = 0;
  if (lua_isnumber(L, 2)) {
    int idx = luaL_checknumber(L, 2);
    if (idx < 0 ) {
      return luaL_error(L, "negative index not allowed");
    }
    Machine* mac = LuaGlobal::GetMachine(idx);
    if (mac) {
      udata = new LuaMachine(L);
      udata->set_mac(mac);
      LuaHelper::new_shared_userdata<LuaMachine>(L, meta, udata);      
    } else {
      lua_pushnil(L);
      return 1;
    }
  } else {
    try {
      size_t len;
      const char* plug_name = luaL_checklstring(L, 2, &len);
	  udata = new LuaMachine(L);	
	  udata->load(plug_name);		
	  LuaHelper::new_shared_userdata<LuaMachine>(L, meta, udata);
    } catch (std::exception &e) {
      e; luaL_error(L, "plugin not found error");
    }
  }
  createparams(L, udata);
  return 1;
}

int LuaMachineBind::createparams(lua_State* L, LuaMachine* machine) {
  assert(machine);
  lua_createtable(L, machine->mac()->GetNumParams(), 0);
  for (int idx = 0; idx < machine->mac()->GetNumParams(); ++idx) {
    createparam(L, idx, machine);
    lua_rawseti(L, -2, idx+1);
  }  
  lua_setfield(L, -2, "params");  
  return 0;
}

int LuaMachineBind::paramrange(lua_State* L) {
  int mac = luaL_checkinteger(L, 1);
  int numparam = luaL_checkinteger(L, 2);
  if (!(mac >=0 && mac < MAX_MACHINES)) {
    luaL_error(L, "Mac index out of range.");
  }
  if (Global::song()._pMachine[mac]) {
    int minrange(0);
	int maxrange(0);
    Global::song()._pMachine[mac]->GetParamRange(numparam, minrange, maxrange);
	lua_pushinteger(L, minrange);
	lua_pushinteger(L, maxrange);
	lua_pushinteger(L, Global::song()._pMachine[mac]->_type);
  } else {
    lua_pushnil(L); lua_pushnil(L); lua_pushnil(L);
  }
  return 3;
}

int LuaMachineBind::paramname(lua_State* L) {
  int mac = luaL_checkinteger(L, 1);
  int numparam = luaL_checkinteger(L, 2);
  if (!(mac >=0 && mac < MAX_MACHINES)) {
    luaL_error(L, "Mac index out of range.");
  }
  if (Global::song()._pMachine[mac]) { 
	char buf[1024];
    Global::song()._pMachine[mac]->GetParamName(numparam, buf);
	std::string tmp(buf);
	lua_pushstring(L, tmp.c_str());	
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int LuaMachineBind::createparam(lua_State* L, int idx, LuaMachine* machine) {
	lua_getglobal(L, "require");
    lua_pushstring(L, "parameter");
    lua_pcall(L, 1, 1, 0);
    lua_getfield(L, -1, "new");
    lua_pushvalue(L, -2);
    lua_pushstring(L, "");
    lua_pushstring(L, "");
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 1);
    lua_pushnumber(L, 10);
    lua_pushnumber(L, 1);
    lua_pushnumber(L, 2); // mpf state
    std::string id;
    machine->mac()->GetParamId(idx, id);
    lua_pushstring(L, id.c_str());
    lua_pcall(L, 9, 1, 0);
    lua_pushcclosure(L, setnorm, 0);
    lua_setfield(L, -2, "setnorm");
	lua_pushcclosure(L, automate, 0);
    lua_setfield(L, -2, "automate");
    lua_pushcclosure(L, name, 0);
    lua_setfield(L, -2, "name");
    lua_pushcclosure(L, display, 0);
    lua_setfield(L, -2, "display");
    lua_pushcclosure(L, getnorm, 0);
    lua_setfield(L, -2, "norm");
    lua_pushcclosure(L, getrange, 0);
    lua_setfield(L, -2, "range");
    lua_pushvalue(L,3);
    lua_setfield(L, -2, "plugin");
    lua_pushnumber(L, idx);
    lua_setfield(L, -2, "idx");
	lua_remove(L, -2);
	return 1;
}

int LuaMachineBind::gc(lua_State* L) {  
  return LuaHelper::delete_shared_userdata<LuaMachine>(L, meta);
}

int LuaMachineBind::show_native_gui(lua_State* L) {
  LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  plug->set_ui_type(MachineUiType::NATIVE);
  return 0;
}

int LuaMachineBind::type(lua_State* L) {
  LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  lua_pushinteger(L, (int)plug->mac()->_type);
  return 1;
}

int LuaMachineBind::info2(lua_State* L) {
  LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  const PluginInfo& info = LuaGlobal::GetLuaPlugin(plug->mac()->_macIndex)->info();
  lua_newtable(L);
  LuaHelper::setfield(L, "dllname", info.dllname);
  LuaHelper::setfield(L, "name", info.name);  
  return 1;
}

int LuaMachineBind::pluginname(lua_State* L) {
  LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  lua_pushstring(L, plug->mac()->GetName());
  return 1;
}

int LuaMachineBind::show_custom_gui(lua_State* L) {
  LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  plug->set_ui_type(MachineUiType::CUSTOMWND);
  return 0;
}

int LuaMachineBind::show_childview_gui(lua_State* L) {
  LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  plug->set_ui_type(MachineUiType::CHILDVIEW);
  return 0;
}

int LuaMachineBind::tick(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 7) {
    LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
    int track = luaL_checknumber(L, 2);
    int note = luaL_checknumber(L, 3);
    int inst = luaL_checknumber(L, 4);
    int mach = luaL_checknumber(L, 5);
    int cmd = luaL_checknumber(L, 6);
    int param = luaL_checknumber(L, 7);
    PatternEntry data(note, inst, mach, cmd, param);
    plug->mac()->Tick(track, &data);
  } else {
    luaL_error(L, "Got %d arguments expected 7 (self)", n);
  }
  return 0;
}

int LuaMachineBind::setnorm(lua_State* L) {
  double newval = luaL_checknumber(L, 2);
  lua_pushvalue(L, 1);
  lua_getfield(L, -1, "idx");
  int idx = luaL_checknumber(L, -1);
  lua_pop(L,1);
  lua_getfield(L, -1, "plugin");
  lua_getfield(L, -1, "__self");
  LuaMachine::Ptr ud = *(boost::shared_ptr<LuaMachine>*) luaL_checkudata(L, -1, meta);
  int minval; int maxval;
  ud->mac()->GetParamRange(idx, minval, maxval);
  int quantization = (maxval-minval);
  ud->mac()->SetParameter(idx, newval*quantization);
  return 0;
}

int LuaMachineBind::automate(lua_State* L) {
	double newval = luaL_checknumber(L, 2);
	lua_pushvalue(L, 1);
	lua_getfield(L, -1, "idx");
	int idx = luaL_checknumber(L, -1);
	lua_pop(L,1);
	lua_getfield(L, -1, "plugin");
	lua_getfield(L, -1, "__self");
	LuaMachine::Ptr ud = *(boost::shared_ptr<LuaMachine>*) luaL_checkudata(L, -1, meta);
	Machine* machine = ud->mac();
	int minval; int maxval;
	machine->GetParamRange(idx, minval, maxval);
	int quantization = (maxval-minval);
	PsycleGlobal::inputHandler().Automate(machine->_macIndex, idx, newval*quantization - minval, true, machine->param_translator());
	return LuaHelper::chaining(L);
}

int LuaMachineBind::name(lua_State* L) {
  lua_getfield(L, -1, "idx");
  int idx = luaL_checknumber(L, -1);
  lua_pop(L,1);
  lua_getfield(L, -1, "plugin");
  lua_getfield(L, -1, "__self");
  LuaMachine::Ptr ud = *(boost::shared_ptr<LuaMachine>*) luaL_checkudata(L, -1, meta);
  char buf[128];
  ud->mac()->GetParamName(idx, buf);
  lua_pushstring(L, buf);
  return 1;
}

int LuaMachineBind::display(lua_State* L) {
  lua_getfield(L, -1, "idx");
  int idx = luaL_checknumber(L, -1);
  lua_pop(L,1);
  lua_getfield(L, -1, "plugin");
  lua_getfield(L, -1, "__self");
  LuaMachine::Ptr ud = *(boost::shared_ptr<LuaMachine>*) luaL_checkudata(L, -1, meta);
  char buf[128];
  ud->mac()->GetParamValue(idx, buf);
  lua_pushstring(L, buf);
  return 1;
}

int LuaMachineBind::getnorm(lua_State* L) {
  lua_getfield(L, -1, "idx");
  int idx = luaL_checknumber(L, -1);
  lua_pop(L,1);
  lua_getfield(L, -1, "plugin");
  lua_getfield(L, -1, "__self");
  LuaMachine::Ptr ud = *(boost::shared_ptr<LuaMachine>*) luaL_checkudata(L, -1, meta);
  int minval; int maxval;
  ud->mac()->GetParamRange(idx, minval, maxval);
  int quantization = (maxval-minval);
  double val = ud->mac()->GetParamValue(idx)-minval;
  val = val  / quantization;
  lua_pushnumber(L, val);
  return 1;
}

int LuaMachineBind::getrange(lua_State* L) {
  lua_getfield(L, -1, "idx");
  int idx = luaL_checknumber(L, -1);
  lua_pop(L,1);
  lua_getfield(L, -1, "plugin");
  lua_getfield(L, -1, "__self");
  LuaMachine::Ptr ud = *(boost::shared_ptr<LuaMachine>*) luaL_checkudata(L, -1, meta);
  int minval; int maxval;
  ud->mac()->GetParamRange(idx, minval, maxval);
  lua_pushnumber(L, 0);
  lua_pushnumber(L, 1);
  int steps = maxval - minval;
  lua_pushnumber(L, steps);
  return 3;
}

int LuaMachineBind::work(lua_State* L) {
   LuaHelper::bind(L, meta, &LuaMachine::work);
   // return LuaHelper::chaining(L);
   return 0;
}

int LuaMachineBind::channel(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    LuaMachine::Ptr plug = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
    int idx = luaL_checknumber(L, 2);
    PSArray ** udata = (PSArray **)lua_newuserdata(L, sizeof(PSArray *));
    PSArray* a = plug->channel(idx);
    *udata = new PSArray(a->data(), a->len());
    luaL_setmetatable(L, LuaArrayBind::meta);
  }  else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return 1;
}

int LuaMachineBind::buildbuffer(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  plugin->build_buffer(plugin->mac()->samplesV, 256);
  return LuaHelper::chaining(L);
}

int LuaMachineBind::addscopememory(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  boost::shared_ptr<ScopeMemory> scope_memory = LuaHelper::check_sptr<ScopeMemory>(L, 2, LuaScopeMemoryBind::meta);
  plugin->mac()->AddScopeMemory(scope_memory);
  return LuaHelper::chaining(L);
}

int LuaMachineBind::audiorange(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  float audio_range = plugin->mac()->GetAudioRange();
  lua_pushnumber(L, audio_range);
  return 1;
}

int LuaMachineBind::getparam(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
    std::string search = luaL_checkstring(L, 2);
    lua_getfield(L, -2, "params");
    size_t len = lua_rawlen(L, -1);
    for (size_t i = 1; i <= len; ++i) {
      lua_rawgeti(L, 3, i);
      lua_getfield(L, -1, "id");
      lua_pushvalue(L, -2);
      lua_pcall(L, 1, 1, 0);
      std::string id(luaL_checkstring(L, -1));
      if (id==search) {
          lua_pop(L, 1);
          return 1;
      }
      lua_pop(L, 1);
    }
    lua_pushnil(L);
    return 1;
  }  else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return 0;
}

int LuaMachineBind::set_numchannels(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  int num = luaL_checknumber(L, 2);
  plugin->mac()->InitializeSamplesVector(num);
  plugin->build_buffer(plugin->mac()->samplesV, 256);
  return 0;
}

int LuaMachineBind::setbuffer(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushvalue(L, 2);
    std::vector<float*> sampleV;
    for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
      PSArray* v = *(PSArray **)luaL_checkudata(L, -1, LuaArrayBind::meta);
      sampleV.push_back(v->data());
    }
    plugin->set_buffer(sampleV);
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

int LuaMachineBind::add_parameters(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  lua_getfield(L, 1, "params");
  lua_pushvalue(L, 2);
  luaL_checktype(L, 2, LUA_TTABLE);
  // t:opairs()
  lua_getfield(L, 2, "opairs");
  lua_pushvalue(L, -2);
  lua_call(L, 1, 2);
  size_t len;
  // iter, self (t), nil
  for(lua_pushnil(L); LuaHelper::luaL_orderednext(L);)
  {
    luaL_checklstring(L, -2, &len);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, "id_");
    lua_rawseti(L, 3, lua_rawlen(L, 3)+1); // params[#params+1] = newparam
  }
  plugin->set_numparams(lua_rawlen(L, 3));
  return LuaHelper::chaining(L);
}

int LuaMachineBind::setmenurootnode(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  lua_setfield(L, 1, "__menurootnode");
  return LuaHelper::chaining(L);
}

int LuaMachineBind::set_parameters(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  plugin->set_numparams(lua_rawlen(L, 2));
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_setfield(L, -2, "params");
  return LuaHelper::chaining(L);
}

int LuaMachineBind::setpresetmode(lua_State* L) {
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  MachinePresetType::Value mode = (MachinePresetType::Value) ((int) luaL_checknumber(L, 2));
  plugin->setprsmode(mode);
  return 0;
}

int LuaMachineBind::addhostlistener(lua_State* L) {  
  LuaMachine::Ptr plugin = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  boost::shared_ptr<LuaActionListener> listener = LuaHelper::check_sptr<LuaActionListener>(L, 2, LuaActionListenerBind::meta);
  listener->setmac(plugin.get());
  PsycleGlobal::actionHandler().AddListener(listener.get());
  return 0;
}

int LuaMachineBind::setviewport(lua_State* L) {
  LuaMachine::Ptr machine = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  if (!lua_isnil(L, 2)) {
    LuaViewport::Ptr viewport = LuaHelper::check_sptr<LuaViewport>(L, 2, LuaViewportBind<>::meta);
    machine->set_viewport(viewport);
  } else {
    machine->set_viewport(boost::weak_ptr<ui::Viewport>());
  }
  return LuaHelper::chaining(L);
}

int LuaMachineBind::toggleviewport(lua_State* L) {
  LuaMachine::Ptr machine = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  machine->ToggleViewPort();
  return LuaHelper::chaining(L);
}

LuaRun* LuaMachineBind::createtimercallback(lua_State* L) {
  LuaRun* result(0);
  if (lua_isfunction(L, 2)) {
     LuaRun* runnable = new LuaRun(L);
     LuaHelper::requirenew<LuaRunBind>(L, "psycle.run", runnable);
     LuaHelper::register_userdata(L, runnable);
     lua_pushvalue(L, 2);
     lua_setfield(L, -2, "run");
     result = runnable;
  } else {
    luaL_error(L, "Argument must be a function.");
  }
  return result;
}

int LuaMachineBind::settimeout(lua_State* L) {
   LuaMachine::Ptr machine = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
   int interval = luaL_checkinteger(L, 3);
   LuaRun* runnable = createtimercallback(L);
   int id = machine->proxy()->set_time_out(runnable, interval);
   lua_pushinteger(L, id);
   return 1;
}

int LuaMachineBind::setinterval(lua_State* L) {
   LuaMachine::Ptr machine = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
   int interval = luaL_checkinteger(L, 3);
   LuaRun* runnable = createtimercallback(L);
   int id = machine->proxy()->set_interval(runnable, interval);
   lua_pushinteger(L, id);
   return 1;
}

int LuaMachineBind::cleartimer(lua_State* L) {
  LuaMachine::Ptr machine = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  int id = luaL_checkinteger(L, 2);
  machine->proxy()->clear_timer(id);
  return LuaHelper::chaining(L);
}

int LuaMachineBind::at(lua_State* L) {
  LuaMachine::Ptr p = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  int machine_index = luaL_checkinteger(L, 2);
  Machine* machine = LuaGlobal::GetMachine(machine_index);
  if (machine) {    
    LuaMachine* lua_machine = new LuaMachine(L);
    lua_machine->set_mac(machine);
    LuaHelper::requirenew<LuaMachineBind>(L, "psycle.machine", lua_machine);
	LuaMachineBind::createparams(L, lua_machine);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int LuaMachineBind::muted(lua_State* L) {  
  LuaMachine::Ptr p = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  int machine_index = luaL_checkinteger(L, 2);
  Machine* machine(0);
  if (machine_index >= 0 && machine_index < 256) {
    machine = LuaGlobal::GetMachine(machine_index);
  }
  lua_pushboolean(L, machine ? machine->Mute() : true);
  return 1;
}

int LuaMachineBind::insert(lua_State* L) {
  LuaMachine::Ptr p = LuaHelper::check_sptr<LuaMachine>(L, 1, meta);
  int n = lua_gettop(L);
  int slot = 0;
  if (n == 3) {
    slot = luaL_checkinteger(L, 2);    
  }  
  LuaMachine::Ptr lua_machine = LuaHelper::check_sptr<LuaMachine>(L, n, LuaMachineBind::meta);
  lua_machine->set_shared(true);
  if (n==2) {
    if (lua_machine->mac()->_mode == MACHMODE_FX) {
      slot = Global::song().GetFreeFxBus();
    } else {
      slot = Global::song().GetFreeBus();
    }
  }
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  int mode = (lua_machine->mac()->_mode == MACHMODE_FX) ? 1 : 0;
  view->CreateNewMachine(-1, -1, slot, mode, lua_machine->mac()->_type, lua_machine->mac()->GetDllName(), 0, lua_machine->mac());
  lua_pushinteger(L, slot);
  return 1;
}


// PlayerBind
const char* LuaPlayerBind::meta = "psyplayermeta";

int LuaPlayerBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"samplerate", samplerate},
    {"tpb", tpb},
	{"bpt", bpt},
    {"spt", spt},
    {"rline", rline},
    {"line", line},
    {"playing", playing},
	{"playpattern", playpattern},
	{"playposition", playposition},
	{"setplayline", setplayline},
	{"setplayposition", setplayposition},
	{"playnote", playnote},
	{"stopnote", stopnote},
	{"toggletrackarmed", toggletrackarmed},
	{"istrackarmed", istrackarmed},
	{"toggletrackmuted", toggletrackmuted},
	{"istrackmuted", istrackmuted},
	{"toggletracksoloed", toggletracksoloed},
	{"istracksoloed", istracksoloed},
	{"playorder", playorder},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods, gc);
}

int LuaPlayerBind::create(lua_State* L) {
  LuaHelper::new_shared_userdata<Player>(L, meta, &Global::player(), 1, true);
  return 1;
}

int LuaPlayerBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaPlayerBind>(L, meta);
}

int LuaPlayerBind::samplerate(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int v = p->SampleRate();
  lua_pushinteger(L, v);
  return 1;
}

int LuaPlayerBind::tpb(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_pushinteger(L, p->lpb);
  return 1;
}

int LuaPlayerBind::bpt(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_pushnumber(L, 1.0/p->lpb);
  return 1;
}

int LuaPlayerBind::rline(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  double row = (p->SamplesPerRow()-p->_samplesRemaining)/(double)p->SamplesPerRow() + p->_lineCounter;
  lua_pushnumber(L, row);
  return 1;
}

  int LuaPlayerBind::spt(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_pushnumber(L, p->SamplesPerTick());
  return 1;
}

int LuaPlayerBind::line(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_pushinteger(L, p->_lineCounter);
  return 1;
}

int LuaPlayerBind::setplayline(lua_State* L) {
  boost::shared_ptr<Player> player = LuaHelper::check_sptr<Player>(L, 1, meta);
  int val = luaL_checkinteger(L, 2);
  player->_lineCounter = val;
  return LuaHelper::chaining(L);
}

int LuaPlayerBind::setplayposition(lua_State* L) {
  boost::shared_ptr<Player> player = LuaHelper::check_sptr<Player>(L, 1, meta);
  int val = luaL_checkinteger(L, 2);
  player->_playPosition = val;
  player->_playPattern = Global::song().playOrder[val];
  return LuaHelper::chaining(L);
}

int LuaPlayerBind::playpattern(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_pushinteger(L, p->_playPattern);
  return 1;
}

int LuaPlayerBind::playposition(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_pushinteger(L, p->_playPosition);
  return 1;
}

int LuaPlayerBind::playing(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_pushboolean(L, p->_playing);
  return 1;
}

int LuaPlayerBind::playnote(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int note = luaL_checkinteger(L, 2);
  if (!(note>=0 && note < notecommands::invalid)) {
    return luaL_error(L, "Note is invalid.");
  }
  int track = luaL_checkinteger(L, 3);
  PatternEntry entry = PsycleGlobal::inputHandler().BuildNote(note, 255, 127, false);
  PsycleGlobal::inputHandler().PlayNote(&entry, track); 
  return LuaHelper::chaining(L);
}

int LuaPlayerBind::stopnote(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int note = luaL_checkinteger(L, 2);
  if (!(note>=0 && note < notecommands::invalid)) {
    return luaL_error(L, "Note is invalid.");
  }
  PsycleGlobal::inputHandler().StopNote(note, 255, false); 
  return LuaHelper::chaining(L);
}

int LuaPlayerBind::toggletrackarmed(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int ttm = luaL_checkinteger(L, 2);
  Global::song().ToggleTrackArmed(ttm);
  return LuaHelper::chaining(L);
}

int LuaPlayerBind::istrackarmed(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int ttm = luaL_checkinteger(L, 2);
  if (ttm >=0 && ttm < MAX_TRACKS) {
	lua_pushboolean(L, Global::song()._trackArmed[ttm]);
  } else {
    return luaL_error(L, "Track index out of bounds.");
  }
  return 1;
}

int LuaPlayerBind::toggletrackmuted(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int ttm = luaL_checkinteger(L, 2);
  Global::song().ToggleTrackMuted(ttm);
  return LuaHelper::chaining(L);
}

int LuaPlayerBind::istrackmuted(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int ttm = luaL_checkinteger(L, 2);
  if (ttm >=0 && ttm < MAX_TRACKS) {
	lua_pushboolean(L, Global::song()._trackMuted[ttm]);
  } else {
    return luaL_error(L, "Track index out of bounds.");
  }
  return 1;
}

int LuaPlayerBind::toggletracksoloed(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int ttm = luaL_checkinteger(L, 2);
  Global::song().ToggleTrackSoloed(ttm);
  return LuaHelper::chaining(L);
}

int LuaPlayerBind::istracksoloed(lua_State* L) {
  boost::shared_ptr<Player> p = LuaHelper::check_sptr<Player>(L, 1, meta);
  int ttm = luaL_checkinteger(L, 2);
  if (ttm >=0 && ttm < MAX_TRACKS) {
	lua_pushboolean(L, ttm == Global::song()._trackSoloed);
  } else {
    return luaL_error(L, "Track index out of bounds.");
  }
  return 1;
}

int LuaPlayerBind::playorder(lua_State* L) {
  boost::shared_ptr<Player> pattern = LuaHelper::check_sptr<Player>(L, 1, meta);
  lua_newtable(L);
  for (int i = 0; i < Global:: song().playLength; ++i) {
     lua_pushinteger(L, Global::song().playOrder[i]);
	 lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

const char* LuaSequenceBarBind::meta = "psysequencebarmeta";

int LuaSequenceBarBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"currpattern", currpattern},
	{"editposition", editposition},
	{"followsong", followsong},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods, gc);
}

int LuaSequenceBarBind::create(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  LuaHelper::new_shared_userdata<CChildView>(L, meta, view, 1, true);
  return 1;
}

int  LuaSequenceBarBind::gc(lua_State* L) {
   return LuaHelper::delete_shared_userdata<CChildView>(L, meta); 
}

int LuaSequenceBarBind::currpattern(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<CChildView> view = LuaHelper::check_sptr<CChildView>(L, 1, meta);
  int pos = Global::song().playOrder[view->editPosition];
  lua_pushinteger(L, pos);
  return 1;
}

int LuaSequenceBarBind::editposition(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<CChildView> view = LuaHelper::check_sptr<CChildView>(L, 1, meta);
  lua_pushinteger(L, view->editPosition);
  return 1;
}

int LuaSequenceBarBind::followsong(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<CChildView> view = LuaHelper::check_sptr<CChildView>(L, 1, meta);
  lua_pushboolean(L, PsycleGlobal::conf()._followSong);
  return 1;
}

const char* LuaMachineBarBind::meta = "psymachinebarmeta";

int LuaMachineBarBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"currmachine", currmachine},
	{"currinst", currinst},
	{"curraux", curraux},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods, gc);
}

int LuaMachineBarBind::create(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  LuaHelper::new_shared_userdata<CChildView>(L, meta, view, 1, true);
  return 1;
}

int LuaMachineBarBind::gc(lua_State* L) {
   return LuaHelper::delete_shared_userdata<CChildView>(L, meta); 
}

int LuaMachineBarBind::currmachine(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<CChildView> view = LuaHelper::check_sptr<CChildView>(L, 1, meta);
  int mach = Global::song().seqBus;
  lua_pushinteger(L, mach);
  return 1;
}

int LuaMachineBarBind::currinst(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<CChildView> view = LuaHelper::check_sptr<CChildView>(L, 1, meta);
  int inst = Global::song().instSelected;
  lua_pushinteger(L, inst);
  return 1;
}

int LuaMachineBarBind::curraux(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<CChildView> view = LuaHelper::check_sptr<CChildView>(L, 1, meta);
  int aux =  Global::song().auxcolSelected;
  lua_pushinteger(L, aux);
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// LuaPatternData+Bind
///////////////////////////////////////////////////////////////////////////////

  int LuaPatternData::numlines(int ps) const {
    return song_->patternLines[ps];
  }

  int LuaPatternData::numtracks() const {
    return song_->SONGTRACKS;
  }

  bool LuaPatternData::has_pattern(int ps) const {    
    return song_->ppPatternData[ps] != 0;	
  }

  bool LuaPatternData::IsTrackEmpty(int ps, int trk) const {
    bool result(true);
	if (song_->ppPatternData[ps] != 0) {
		for (size_t i = 0; i < song_->patternLines[ps]; ++i) {
			unsigned char* e = song_->_ptrackline(ps, trk, i);
			PatternEntry* entry = (PatternEntry*)(e);
			if (entry->_note != notecommands::empty ||
			    entry->_inst != 0xFF || entry->_cmd != 0 || entry->_parameter != 0
			) {
			  result = false;
			  break;
			}
		}
	}    
    return result;
  }


const char* LuaPatternDataBind::meta = "psypatterndatameta";

int LuaPatternDataBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"setevent", setevent},
    {"buildevent", buildevent},
    {"eventat", eventat},
    {"settrack", settrack},
    {"track", track},
    {"pattern", create},
    {"numtracks", numtracks},
    {"numlines", numlines},
	{"istrackempty", istrackempty},
	{"line", line},
	{"patstep", patstep},
	{"editquantizechange", editquantizechange},
	{"deleterow", deleterow},
	{"clearrow", clearrow},
	{"insertrow", insertrow},
	{"interpolate", interpolate},
	{"interpolatecurve", interpolatecurve},
	{"loadblock", loadblock},
	{"addundo", addundo},
	{"undo", undo},
	{"redo", redo},
	{"movecursorpaste", movecursorpaste},
	{"settrackedit", settrackedit},
	{"settrackline", settrackline},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods, gc);
}

int LuaPatternDataBind::createevent(lua_State* L, LuaPatternEvent& ev) {
  lua_createtable(L, 0, 8);
  LuaHelper::setfield(L, "pos", ev.pos);
  LuaHelper::setfield(L, "len", ev.len);
  LuaHelper::setfield(L, "hasoff", ev.has_off);
  LuaHelper::setfield(L, "val", static_cast<int>(ev.entry._note));
  LuaHelper::setfield(L, "inst", static_cast<int>(ev.entry._inst));
  LuaHelper::setfield(L, "mach", static_cast<int>(ev.entry._mach));
  LuaHelper::setfield(L, "cmd", static_cast<int>(ev.entry._cmd));
  LuaHelper::setfield(L, "parameter", static_cast<int>(ev.entry._parameter)); 
  LuaHelper::setfield(L, "dummy", 0);   
  return 1;
}

int LuaPatternDataBind::getevent(lua_State* L, int idx, LuaPatternEvent& ev) {  
  lua_getfield(L, idx, "pos");  
  lua_getfield(L, idx, "len");  
  lua_getfield(L, idx, "hasoff");  
  lua_getfield(L, idx, "val");  
  lua_getfield(L, idx, "inst");  
  lua_getfield(L, idx, "mach");  
  lua_getfield(L, idx, "cmd");
  lua_getfield(L, idx, "parameter");
  ev.entry._parameter = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  ev.entry._cmd = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  ev.entry._mach = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  ev.entry._inst = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  ev.entry._note = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  ev.has_off = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  ev.len = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  ev.pos = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  return 0;
}

int LuaPatternDataBind::create(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  LuaPatternData* pattern = new LuaPatternData(L, Global::song().ppPatternData, &Global::song());
  LuaHelper::new_shared_userdata<LuaPatternData>(L, meta, pattern);
  return 1;
}

int LuaPatternDataBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaPatternData>(L, meta);
}

int LuaPatternDataBind::eventat(lua_State* L) {
	try {
		boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
		int ps = luaL_checknumber(L, 2);  
		int trk = luaL_checknumber(L, 3);
		int pos = luaL_checknumber(L, 4);
		unsigned char* e = pattern->ptrackline(ps, trk, pos);
		LuaPatternEvent ev;
		ev.entry._note = *e++;
		ev.entry._inst = *e++;
		ev.entry._mach = *e++;
		ev.entry._cmd = *e++;
		ev.entry._parameter = *e++;
		return createevent(L, ev);
	} catch (std::exception& e) {
		luaL_error(L, e.what());
		return 0;
	}
}

int LuaPatternDataBind::line(lua_State* L) {
	boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta); 
	int ps = luaL_checknumber(L, 2); 
	int line = luaL_checknumber(L, 3);
	lua_newtable(L);
	int n = 1;
	for (int track = 0; track < pattern->numtracks() ; ++track) {
		unsigned char* e = pattern->ptrackline(ps, track, line);
		LuaPatternEvent ev;
		ev.entry._note = *e++;
		ev.entry._inst = *e++;
		ev.entry._mach = *e++;
		ev.entry._cmd = *e++;
		ev.entry._parameter = *e++;
		if (ev.entry._note != notecommands::empty ||
			ev.entry._inst != 0xFF || ev.entry._cmd != 0 || ev.entry._parameter != 0) {
				createevent(L, ev);
				LuaHelper::setfield(L, "track", track);   
				lua_rawseti(L, 4, n++);
		}
	}
    return 1;
}

int LuaPatternDataBind::pattern(lua_State* L) {
  // LuaPatternData* pattern = LuaHelper::check<LuaPatternData>(L, 1, meta);
  lua_newtable(L);
  //const int len = pattern->numtracks();
  /*for (int i=1; i <= len; ++i) {
    lua_pushvalue(L, 1);
    lua_pushnumber
    track(L);
    lua_rawseti(L, i
  }*/
  return 1;
}

int LuaPatternDataBind::settrack(lua_State* L) {
  boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
  int ps = luaL_checknumber(L, 2);
  int trk = luaL_checknumber(L, 3);
  int pos = luaL_checknumber(L, 4);  
  luaL_checktype(L, 5, LUA_TTABLE);  
  size_t len = lua_rawlen(L, 5);
  for (size_t i = 1; i <= len; ++i) {
    lua_rawgeti(L, 5, i);
    LuaPatternEvent ev;    
    getevent(L, 6, ev);
    unsigned char* srce = (unsigned char*) &ev.entry;
    unsigned char* e = pattern->ptrackline(ps, trk, pos);
    for (int i = 0; i < 5; ++i) {
      *e++ = *srce++;      
    }
    lua_pop(L, 1);
  }
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::settrackedit(lua_State* L) {
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  int track = luaL_checkinteger(L, 1);
  view->editcur.track = track;
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::settrackline(lua_State* L) {
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  int line = luaL_checkinteger(L, 1);
  view->editcur.line = line;
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::track(lua_State* L) {
  boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
  int ps = luaL_checknumber(L, 2);
  int trk = luaL_checknumber(L, 3);
  PatternEntry entry;
  std::vector<LuaPatternEvent> events;
  LuaPatternEvent* last = 0;
  int lastpos = 0;
  int pos = 0;
  for (size_t i = 0; i < pattern->numlines(ps); ++i, ++pos) {
    unsigned char* e = pattern->ptrackline(ps, trk, i);
    int note = static_cast<int>(*e);
    if (note == notecommands::empty) continue;
    if (last) {
        int len = i - lastpos;
        last->len = len;
        if (note == notecommands::release) {
          last->has_off = true;
        }
    }
    lastpos = i;
    if (note == notecommands::release) {
      last = 0;
    } else {
      LuaPatternEvent ev;
      ev.len = 1;
      ev.pos = pos;
      ev.has_off = false;
      ev.entry._note = *e++;
      ev.entry._inst = *e++;
      ev.entry._mach = *e++;
      ev.entry._cmd = *e++;
      ev.entry._parameter = *e++;
      events.push_back(ev);
      last = &events.back();
    }
  }
  if (events.size()!=0 && !events.back().has_off && events.back().entry._note!=notecommands::release) {
    events.back().len = pattern->numlines(ps) - events.back().pos;
  }
  lua_createtable(L, events.size(), 0);
  int i=1;
  std::vector<LuaPatternEvent>::iterator it = events.begin();
  for (; it != events.end(); ++it, ++i) {
    LuaPatternEvent& ev = *it;
    createevent(L, ev);
    lua_rawseti(L, -2, i);
  }
  return 1;
}

int LuaPatternDataBind::setevent(lua_State* L) {
  boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
  int ps = luaL_checknumber(L, 2);
  int trk = luaL_checknumber(L, 3);
  int pos = luaL_checknumber(L, 4);
  unsigned char* e = pattern->ptrackline(ps, trk, pos);
  luaL_checktype(L, 5, LUA_TTABLE);
  lua_getfield(L, 5, "parameter");
  lua_getfield(L, 5, "cmd");
  lua_getfield(L, 5, "mach");
  lua_getfield(L, 5, "inst");
  lua_getfield(L, 5, "val");

  for (int i = 0; i < 5; ++i) {
    *e++ = luaL_checknumber(L, -1);
    lua_pop(L, 1);
  } 
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::buildevent(lua_State* L) {
  boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
  int ps = luaL_checknumber(L, 2);
  int trk = luaL_checknumber(L, 3);
  int pos = luaL_checknumber(L, 4);
  int note = luaL_checknumber(L, 5);
  PatternEntry entry = PsycleGlobal::inputHandler().BuildNote(note);
  unsigned char* e = pattern->ptrackline(ps, trk, pos);
  *e = entry._note;
  return LuaHelper::chaining(L);;
}

int LuaPatternDataBind::patstep(lua_State* L) {
  boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  lua_pushinteger(L, view->patStep);
  return 1;
}

int LuaPatternDataBind::editquantizechange(lua_State* L) {
  boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  int diff = luaL_checknumber(L, 2);
  main->EditQuantizeChange(diff);
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::deleterow(lua_State* L) {
	boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
	int ps = luaL_checknumber(L, 2);
	int trk = luaL_checknumber(L, 3);
	int pos = luaL_checknumber(L, 4);
	unsigned char * offset = Global::song()._ptrack(ps, trk);
	int patlines = Global::song().patternLines[ps];
	int i;
	for (i=pos; i < patlines-1; i++)
		memcpy(offset+(i*MULTIPLY), offset+((i+1)*MULTIPLY), EVENT_SIZE);

	PatternEntry blank;
	memcpy(offset+(i*MULTIPLY),&blank,EVENT_SIZE);  			
	return LuaHelper::chaining(L);
}

int LuaPatternDataBind::clearrow(lua_State* L) {
	boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
	ui::alert("clearrow not yet implemented.");		
	return LuaHelper::chaining(L);
}

int LuaPatternDataBind::insertrow(lua_State* L) {
	boost::shared_ptr<LuaPatternData> pattern = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
	int ps = luaL_checknumber(L, 2);
	int trk = luaL_checknumber(L, 3);
	int pos = luaL_checknumber(L, 4);
	unsigned char * offset = Global::song()._ptrack(ps, trk);
	int patlines = Global::song().patternLines[ps];
	int i;
	for (i=patlines-1; i > pos; i--)
		memcpy(offset+(i*MULTIPLY), offset+((i-1)*MULTIPLY), EVENT_SIZE);
	PatternEntry blank;
	memcpy(offset+(pos*MULTIPLY),&blank,EVENT_SIZE);
    return LuaHelper::chaining(L);
}

int LuaPatternDataBind::interpolate(lua_State* L) {
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  int track = luaL_checkinteger(L, 2);
  int startline = luaL_checkinteger(L, 3);
  int endline = luaL_checkinteger(L, 4);  
  CSelection sel;
  sel.start = CCursor(startline, 0, track);
  sel.end = CCursor(endline, 0, track);
  CSelection old_selection = view->selection();
  bool old_sel_status = view->blockSelected;
  view->SetSelection(sel);
  view->blockSelected = true;
  view->BlockParamInterpolate();
  view->SetSelection(old_selection);
  view->blockSelected = old_sel_status;
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::interpolatecurve(lua_State* L) {
  CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
  CChildView* view = &main->m_wndView;
  int track = luaL_checkinteger(L, 2);
  int startline = luaL_checkinteger(L, 3);
  int endline = luaL_checkinteger(L, 4);  
  CSelection sel;
  sel.start = CCursor(startline, 0, track);
  sel.end = CCursor(endline, 0, track);
  CSelection old_selection = view->selection();
  bool old_sel_status = view->blockSelected;
  view->SetSelection(sel);
  view->blockSelected = true;
  view->OnPopInterpolateCurve();
  view->SetSelection(old_selection);
  view->blockSelected = old_sel_status;
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::loadblock(lua_State* L) {
    boost::shared_ptr<LuaPatternData> patterndata = LuaHelper::check_sptr<LuaPatternData>(L, 1, meta);
	const char* filename = luaL_checkstring(L, 2);
	FILE* file = fopen(filename, "rb");
	if (!file) {
	  return luaL_error(L, "Load block file error.");
	}
	int nt, nl;
	int ps = -1;
	fread(&nt, sizeof(int), 1, file);
	fread(&nl, sizeof(int), 1, file);

	if ((nt > 0) && (nl > 0))
	{
		ps =  Global::song().GetBlankPatternUnused();
		Global::song().AllocNewPattern(ps, "",
					Global::configuration().GetDefaultPatLines(),FALSE);

		int nlines = Global::song().patternLines[ps];
		if (nlines != nl)
		{			
			Global::song().patternLines[ps] = nl;
		}

		for (int t=0; t<nt; t++)
		{
			for (int l=0; l<nl; l++)
			{
				if(l<MAX_LINES && t<MAX_TRACKS)
				{
					unsigned char* offset_target = Global::song()._ptrackline(ps, t, l);
					fread(offset_target, sizeof(char), EVENT_SIZE, file);
				}
			}
		}
		PatternEntry blank;

		for (int t = nt; t < MAX_TRACKS;t++)
		{
			for (int l = nl; l < MAX_LINES; l++)
			{
				unsigned char* offset_target =  Global::song()._ptrackline(ps, t, l);
				memcpy(offset_target, &blank, EVENT_SIZE);
			}
		}	
		lua_pushinteger(L, ps);	
    } else {
		lua_pushnil(L);
	}   
	fclose(file);
    return 1;
}

int LuaPatternDataBind::addundo(lua_State* L) {
  int pattern = luaL_checkinteger(L, 1);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  int tracks = luaL_checkinteger(L, 4);
  int lines = luaL_checkinteger(L, 5);
  int edittrack = luaL_checkinteger(L, 6);
  int editline = luaL_checkinteger(L, 7);
  int editcol = luaL_checkinteger(L, 8);
  int seqpos = luaL_checkinteger(L, 9);
  PsycleGlobal::inputHandler().
    AddUndo(pattern, x, y, tracks, lines, edittrack, editline, editcol, seqpos); // , BOOL bWipeRedo=true, int counter=0);
  return LuaHelper::chaining(L);
}

int LuaPatternDataBind::undo(lua_State* L) {
	if (!PsycleGlobal::inputHandler().pUndoList.empty()) {
		int edittrack = luaL_checkinteger(L, 1);
		int editline = luaL_checkinteger(L, 2);
		int editcol = luaL_checkinteger(L, 3);
		int seqpos = luaL_checkinteger(L, 4);
		SPatternUndo& pUndo = PsycleGlobal::inputHandler().pUndoList.back();
		if (pUndo.type == UNDO_PATTERN) {		
			PsycleGlobal::inputHandler().AddRedo(pUndo.pattern,pUndo.x,pUndo.y,pUndo.tracks,pUndo.lines,
			edittrack,
			editline,
			editcol,
			seqpos,
			pUndo.counter);		
			unsigned char* pData = pUndo.pData;
			for (int t=pUndo.x;t<pUndo.x+pUndo.tracks;t++)
			{
				for (int l=pUndo.y;l<pUndo.y+pUndo.lines;l++)
				{
					unsigned char *offset_source= Global::song()._ptrackline(pUndo.pattern,t,l);								
					memcpy(offset_source,pData,EVENT_SIZE);
					pData+=EVENT_SIZE;
				}
			}
			PsycleGlobal::inputHandler().pUndoList.pop_back();
		}
	}
	return LuaHelper::chaining(L);
}

int LuaPatternDataBind::redo(lua_State* L) {
	if (!PsycleGlobal::inputHandler().pRedoList.empty()) {
	    int edittrack = luaL_checkinteger(L, 1);
		int editline = luaL_checkinteger(L, 2);
		int editcol = luaL_checkinteger(L, 3);		
		SPatternUndo& pRedo = PsycleGlobal::inputHandler().pRedoList.back();
		if (pRedo.type == UNDO_PATTERN) {
			PsycleGlobal::inputHandler().AddUndo(
			    pRedo.pattern,
			    pRedo.x,
			    pRedo.y,
			    pRedo.tracks,
		 	    pRedo.lines,
			    edittrack,
			    editline,
			    editcol,
			    pRedo.seqpos,
			    false,
			    pRedo.counter);
			unsigned char* pData = pRedo.pData;
			for (int t=pRedo.x;t<pRedo.x+pRedo.tracks;t++)
			{
				for (int l=pRedo.y;l<pRedo.y+pRedo.lines;l++)
				{
					unsigned char *offset_source=Global::song()._ptrackline(pRedo.pattern,t,l);					
					memcpy(offset_source,pData,EVENT_SIZE);
					pData+=EVENT_SIZE;
				}
			}
		}
		PsycleGlobal::inputHandler().pRedoList.pop_back();
	}
	return LuaHelper::chaining(L);
}

int LuaPatternDataBind::movecursorpaste(lua_State* L) {
  lua_pushboolean(L, PsycleGlobal::conf().inputHandler().bMoveCursorPaste);
  return 1;
}


// Tweak + Bind

void LuaTweak::OnAutomate(int macIdx, int param, int value, bool undo,
    const ParamTranslator& translator, int track, int line) {
	try {
		LuaImport in(L, this, LuaGlobal::proxy(L));
		if (in.open("onautomate")) {			
			in << macIdx << param << value << track << line << pcall(0);
		}
	} catch (std::exception& e) {
	  ui::alert(e.what());
	}
}

const char* LuaTweakBind::meta = "tweakmeta";

int  LuaTweakBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods, gc);
}

int LuaTweakBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected (self)", n);
  } 
  LuaHelper::new_shared_userdata<LuaTweak>(L, meta, new LuaTweak(L));
  return 1;
}

int LuaTweakBind::gc(lua_State *L) {
  return LuaHelper::delete_shared_userdata<LuaTweak>(L, meta);	  
}

///////////////////////////////////////////////////////////////////////////////
// PlotterBind
///////////////////////////////////////////////////////////////////////////////
#if 0//!defined WINAMP_PLUGIN
int LuaPlotterBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"stem", stem },
    { NULL, NULL }
  };
  return LuaHelper::open(L, "psyplottermeta", methods,  gc);
}

int LuaPlotterBind::create(lua_State* L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  LuaHelper::new_shared_userdata<CPlotterDlg>(L, "psyplottermeta",
    new CPlotterDlg(AfxGetMainWnd()));
  return 1;
}

int LuaPlotterBind::gc(lua_State* L) {
  typedef boost::shared_ptr<CPlotterDlg> T;
  T ud = *(T*) luaL_checkudata(L, 1, "psyplottermeta");
  // todo this is all a mess. Dunno how to multithread mfc...
  // Maybe write a pipe in the winmain thread ...
  AfxGetMainWnd()->SendMessage(0x0501, (WPARAM)ud.get(), 0);
  /*if (ud) {
  // ud->DestroyWindow();
  }*/
  return 0;
}

int LuaPlotterBind::stem(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<CPlotterDlg> plotter = LuaHelper::check_sptr<CPlotterDlg>(L, 1,
      "psyplottermeta");
    plotter->ShowWindow(SW_SHOW);
    PSArray* x = *(PSArray **)luaL_checkudata(L, -1, LuaArrayBind::meta);
    plotter->set_data(x->data(), x->len());
    plotter->UpdateWindow();
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, array)", n);
  }
  return 0;
}
#endif // !defined WINAMP_PLUGIN
///////////////////////////////////////////////////////////////////////////////
// Delay
///////////////////////////////////////////////////////////////////////////////
void PSDelay::work(PSArray& x, PSArray& y) {
  int n = x.len();
  int k = mem.len();
  if (n>k) {
    y.copyfrom(x, k);
    y.copyfrom(mem, 0);
    int o = n-k;
    x.offset(o);
    mem.copyfrom(x, 0);
    x.offset(-o);
  } else
  if (n==k) {
    y.copyfrom(mem);
    mem.copyfrom(x);
  } else {
    y.copyfrom(mem, 0);
    PSArray tmp(k, 0);
    mem.offset(n);
    tmp.copyfrom(mem, 0);
    mem.offset(-n);
    tmp.copyfrom(x, k-n);
    mem.copyfrom(tmp);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DelayBind
///////////////////////////////////////////////////////////////////////////////

const char* LuaDelayBind::meta = "psydelaymeta";

int  LuaDelayBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"work", work},
    {"tostring", tostring},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods, gc, tostring);
}

int LuaDelayBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, k)", n);
  }
  int k = luaL_checknumber (L, 2);
  luaL_argcheck(L, k >= 0, 2, "negative index not allowed");
  LuaHelper::new_shared_userdata<PSDelay>(L, meta, new PSDelay(k));
  return 1;
}

int LuaDelayBind::work(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<PSDelay> delay = LuaHelper::check_sptr<PSDelay>(L, 1, meta);
    PSArray* x = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
    PSArray** y = (PSArray **)lua_newuserdata(L, sizeof(PSArray *));
    *y = new PSArray(x->len(), 0);
    luaL_setmetatable(L, LuaArrayBind::meta);
    delay->work(*x, **y);
  }  else {
    luaL_error(L, "Got %d arguments expected 2 (self, arrayinput)", n);
  }
  return 1;
}

int LuaDelayBind::gc (lua_State *L) {
  return LuaHelper::delete_shared_userdata<PSDelay>(L, meta);	  
}

int LuaDelayBind::tostring(lua_State *L) {    
  lua_pushfstring(L, "delay");
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// WaveOscTables (Singleton/Lazy Creation)
///////////////////////////////////////////////////////////////////////////////

WaveOscTables::WaveOscTables() {
  srand((unsigned)time(NULL));
  set_samplerate(Global::player().SampleRate()); // Lazy Creation
}

void WaveOscTables::saw(float* data, int num, int maxharmonic)  {
  double gain = 0.5 / 0.777;
  for  (int h = 1; h <= maxharmonic; ++h) {
    double amplitude = gain / h;
    double to_angle = 2* psycle::helpers::math::pi / num * h;
    for (int i = 0; i < num; ++i) {
      data[i] += ::sin(pow(1.0,h+1)*i*to_angle)*amplitude;
    }
  }
}

void WaveOscTables::sqr(float* data, int num, int maxharmonic)  {
  double gain = 0.5 / 0.777;
  for  (int h = 1; h <= maxharmonic; h=h+2) {
    double amplitude = gain / h;
    double to_angle = 2* psycle::helpers::math::pi / num * h;
    for (int i = 0; i < num; ++i) {
      data[i] += ::sin(i*to_angle)*amplitude;
    }
  }
}

void WaveOscTables::tri(float* data, int num, int maxharmonic)  {
  // double gain = 0.5 / 0.777;
  for (int h = 1; h <= maxharmonic; h=h+2) {
    double to_angle = 2*psycle::helpers::math::pi/num*h;
    for (int i = 0; i < num; ++i) {
      data[i] += pow(-1.0,(h-1)/2.0)/(h*h)*::sin(i*to_angle);
    }
  }
}

void WaveOscTables::sin(float* data, int num, int maxharmonic)  {
  double to_angle = 2*psycle::helpers::math::pi/num;
  for (int i = 0; i < num; ++i) {
    data[i] = ::sin(i*to_angle);
  }
}

void WaveOscTables::ConstructWave(double fh,
  XMInstrument::WaveData<float>& wave,
  void (*func)(float*, int, int),
  int sr) {
    double f = 261.6255653005986346778499935233; // C4
    int num = (int) (sr/f + 0.5);
    int hmax = (int) (sr/2/fh);
    wave.AllocWaveData(num, false);
    wave.WaveSampleRate(sr);
    wave.WaveLoopStart(0);
    wave.WaveLoopEnd(num);
    wave.WaveLoopType(XMInstrument::WaveData<float>::LoopType::NORMAL);
    //Warning!! do not use dsp::Clear. Memory is not aligned in wavedata.
    for (int i = 0; i < num; ++i) wave.pWaveDataL()[i]=0;
    func(wave.pWaveDataL(), num, hmax);
}

void WaveOscTables::set_samplerate(int sr) {
  if (sin_tbl.size() != 0) {
    cleartbl(sin_tbl);
    cleartbl(tri_tbl);
    cleartbl(sqr_tbl);
    cleartbl(saw_tbl);
    cleartbl(rnd_tbl);
  }
  double f_lo = 440*std::pow(2.0, (0-notecommands::middleA)/12.0);
  for (int i = 0; i < 10; ++i) {
    double f_hi = 2 * f_lo;
    if (i==0) {
      f_lo = 0;
    }
    XMInstrument::WaveData<float>* w;
    w =  new XMInstrument::WaveData<float>();
    ConstructWave(f_hi, *w, &sqr, sr);
    sqr_tbl[range<double>(f_lo, f_hi)] = w;
    w =  new XMInstrument::WaveData<float>();
    ConstructWave(f_hi, *w, &saw, sr);
    saw_tbl[range<double>(f_lo, f_hi)] = w;
    w =  new XMInstrument::WaveData<float>();
    ConstructWave(f_hi, *w, &sin, sr);
    sin_tbl[range<double>(f_lo, f_hi)] = w;
    w =  new XMInstrument::WaveData<float>();
    ConstructWave(f_hi, *w, &tri, sr);
    tri_tbl[range<double>(f_lo, f_hi)] = w;
    f_lo = f_hi;
  }
}

void WaveOscTables::cleartbl(WaveList<float>::Type& tbl) {
  WaveList<float>::Type::iterator it;
  for (it = tbl.begin(); it != tbl.end(); ++it) {
    delete it->second;
  }
  tbl.clear();
}

///////////////////////////////////////////////////////////////////////////////
// WaveOsc (delegator to resampler)
///////////////////////////////////////////////////////////////////////////////

WaveOsc::WaveOsc(WaveOscTables::Shape shape) : shape_(shape) {
  if (shape != WaveOscTables::PWM) {
    WaveList<float>::Type tbl = WaveOscTables::getInstance()->tbl(shape);
    resampler_.reset(new ResamplerWrap<float, 1>(tbl));
    resampler_->set_frequency(263);
  } else {
    // pwm subtract two saw's
    resampler_.reset(new PWMWrap<float, 1>());
    resampler_->set_frequency(263);
  }
}

void WaveOsc::set_shape(WaveOscTables::Shape shape) {
  if (shape_ != shape) {
    if (shape != WaveOscTables::PWM) {
      WaveList<float>::Type tbl = WaveOscTables::getInstance()->tbl(shape);
      if (shape_ == WaveOscTables::PWM) {
        ResamplerWrap<float, 1>* rnew = new ResamplerWrap<float, 1>(tbl);
        rnew->set_gain(resampler_->gain());
        rnew->set_frequency(resampler_->frequency());
        rnew->setphase(resampler_->phase());
        if (resampler_->Playing()) {
          rnew->Start(resampler_->phase());
        }
        resampler_.reset(rnew);
      } else {
        resampler_->SetData(tbl);
      }
    } else {
      PWMWrap<float, 1>* rnew = new PWMWrap<float,1>();
      rnew->set_gain(resampler_->gain());
      rnew->set_frequency(resampler_->frequency());
      if (resampler_->Playing()) {
        rnew->Start(resampler_->phase());
      }
      resampler_.reset(rnew);
    }
    shape_ = shape;
  }
}

void WaveOsc::work(int num, float* data, SingleWorkInterface* master) {
    if (shape() != WaveOscTables::RND) {
      resampler_->work(num, data, 0, master);
    } else {
      // todo fm, am, pw
      for (int i = 0; i < num; ++i) {
        float r = ((float) rand() / (RAND_MAX))*2-1;
        *data++ = r;
      }
    }
}

///////////////////////////////////////////////////////////////////////////////
// WaveOscBind
///////////////////////////////////////////////////////////////////////////////
const char* LuaWaveOscBind::meta = "psyoscmeta";

std::map<WaveOsc*, WaveOsc*> LuaWaveOscBind::oscs; // osc map for samplerates
// changes

int LuaWaveOscBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"frequency", get_base_frequency},
    {"setfrequency", set_base_frequency},
    {"setgain", set_gain},
    {"gain", gain},
    {"setpw", setpw},
    {"pw", pw},
    {"work", work},
    {"stop", stop},
    {"start", start},
    {"tostring", tostring},
    {"isplaying", isplaying},
    {"setshape", set_shape},
    {"shape", shape},
    {"setquality", set_quality},
    {"quality", set_quality},
    {"setsync", set_sync},
    {"setsyncfadeout", set_sync_fadeout},
    {"phase", phase},
    {"setphase", setphase},
    {"setpm", setpm},
    {"setam", setam},
    {"setfm", setfm},
    {"setpwm", setpwm},
    { NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc, tostring);
  static const char* const e[] = {"SIN", "SAW", "SQR", "TRI", "PWM", "RND"};
  LuaHelper::buildenum(L, e, sizeof(e)/sizeof(e[0]), 1);
  static const char* const f[] =
      {"ZEROHOLD", "LINEAR", "SPLINE", "SINC", "SOXR"};
  LuaHelper::buildenum(L, f, sizeof(f)/sizeof(f[0]), 1);
  return 1;
}

int LuaWaveOscBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 2 and n!=3) {
    return luaL_error(L,
      "Got %d arguments expected 2[,3] (self, shape [, f])",
      n);
  }
  int type = luaL_checknumber (L, 2);
  // luaL_argcheck(L, f >= 0, 2, "negative frequency is not allowed");
  WaveOsc* osc = new WaveOsc((WaveOscTables::Shape)type);
  LuaHelper::new_shared_userdata<WaveOsc>(L, meta, osc);
  oscs[osc] = osc;
  if (n==3) {
    double f = luaL_checknumber (L, 3);
    osc->set_frequency(f);
  }
  return 1;
}

int LuaWaveOscBind::phase(lua_State* L) {
   LuaHelper::bind(L, meta, &WaveOsc::phase);
   return 1;
}

int LuaWaveOscBind::setphase(lua_State* L) {
  LuaHelper::bind(L, meta, &WaveOsc::setphase);
  return LuaHelper::chaining(L);
}

void LuaWaveOscBind::setsamplerate(double sr) {
  std::map<WaveOsc*, WaveOsc*>::iterator it = oscs.begin();
  for ( ; it != oscs.end(); ++it) {
    it->second->set_shape(it->second->shape()); // reset wave data
  }
}

int LuaWaveOscBind::set_gain(lua_State* L) {
  LuaHelper::bind(L, meta, &WaveOsc::set_gain);
  return LuaHelper::chaining(L);
}

int LuaWaveOscBind::gain(lua_State* L) {
  LuaHelper::bind(L, meta, &WaveOsc::gain);
  return 1;
}

int LuaWaveOscBind::setpw(lua_State* L) {
  LuaHelper::bind(L, meta, &WaveOsc::setpw);
  return LuaHelper::chaining(L);
}

int LuaWaveOscBind::pw(lua_State* L) {
  LuaHelper::bind(L, meta, &WaveOsc::pw);
  return 1;
}

int LuaWaveOscBind::set_shape(lua_State* L) {
  return LuaHelper::callstrict1<WaveOsc, double>(L, meta, &WaveOsc::set_shape);
}

int LuaWaveOscBind::shape(lua_State* L) {
  return LuaHelper::getnumber<WaveOsc, WaveOscTables::Shape>(L, meta, &WaveOsc::shape);
}

int LuaWaveOscBind::stop(lua_State* L) {
  return LuaHelper::callopt1<WaveOsc, double>(L, meta, &WaveOsc::Stop, 0);
}

int LuaWaveOscBind::start(lua_State* L) {
  return LuaHelper::callopt1<WaveOsc, double>(L, meta, &WaveOsc::Start, 0);
}

int LuaWaveOscBind::isplaying(lua_State* L) {
  int n = lua_gettop(L);
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
  lua_pushboolean(L, osc->IsPlaying());
  return 1;
}

int LuaWaveOscBind::get_base_frequency(lua_State* L) {
  LuaHelper::bind(L, meta, &WaveOsc::base_frequency);
  return 1;
}

int LuaWaveOscBind::set_base_frequency(lua_State* L) {
  LuaHelper::bind(L, meta, &WaveOsc::set_frequency);
  return LuaHelper::chaining(L);
}

int LuaWaveOscBind::setpm(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
    if (lua_isnil(L, 2)) {
      osc->setpm(0);
    } else {
      PSArray* v = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
      osc->setpm(v->data());
    }
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

int LuaWaveOscBind::setfm(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
    if (lua_isnil(L, 2)) {
      osc->setfm(0);
    } else {
      PSArray* v = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
      osc->setfm(v->data());
    }
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

int LuaWaveOscBind::setam(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
    if (lua_isnil(L, 2)) {
      osc->setam(0);
    } else {
      PSArray* v = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
      osc->setam(v->data());
    }
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

int LuaWaveOscBind::setpwm(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
    if (lua_isnil(L, 2)) {
      osc->setam(0);
    } else {
      PSArray* v = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
      osc->setpwm(v->data());
    }
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

int LuaWaveOscBind::work(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2 or n==3 or n==4 or n==5) {
    boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
    PSArray* data = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
    float* fm = 0;
    float* env = 0;
    // float* pwm = 0;
    if (n>2 && (!lua_isnil(L,3))) {
      PSArray*arr = *(PSArray **)luaL_checkudata(L, 3, LuaArrayBind::meta);
      fm = arr->data();
    }
    if (n>3 && (!lua_isnil(L,4))) {
      PSArray* arr = *(PSArray **)luaL_checkudata(L, 4, LuaArrayBind::meta);
      env = arr->data();
    }
    // check for master
    std::auto_ptr<LuaSingleWorker> master;
    lua_getfield(L, 1, "sync");
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
    } else {
      lua_pushvalue(L, 1);
      master.reset(new LuaSingleWorker(L));
    }
    osc->work(data->len(), data->data(), master.get());
    lua_pushnumber(L,  data->len()); // return processed samples
  }  else {
    luaL_error(L, "Got %d arguments expected 2 or 3 (self, num, fm)", n);
  }
  return 1;
}

int LuaWaveOscBind::gc(lua_State *L) {
  boost::shared_ptr<WaveOsc> ptr = *(boost::shared_ptr<WaveOsc> *)luaL_checkudata(L, 1, meta);
  std::map<WaveOsc*, WaveOsc*>::iterator it = oscs.find(ptr.get());
  assert(it != oscs.end());
  oscs.erase(it);
  return LuaHelper::delete_shared_userdata<WaveOsc>(L, meta);
}

int LuaWaveOscBind::tostring(lua_State *L) {
  boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
  std::stringstream r;
  r << "osc " << "f=" << osc->base_frequency();
  lua_pushfstring(L, r.str().c_str());
  return 1;
}

int LuaWaveOscBind::set_quality(lua_State* L) {
  int n = lua_gettop(L);
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, frequency)", n);
  }
  boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
  int quality = luaL_checknumber(L, 2)-1;
  osc->set_quality((psycle::helpers::dsp::resampler::quality::type)quality);
  return 0;
}

int LuaWaveOscBind::quality(lua_State* L) {
  return LuaHelper::getnumber<WaveOsc, psycle::helpers::dsp::resampler::quality::type>
    (L, meta, &WaveOsc::quality);
}

int LuaWaveOscBind::set_sync(lua_State* L) {
  LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_setfield(L, -2, "sync");
  lua_getglobal(L, "require");
  lua_pushstring(L, "psycle.array");
  lua_pcall(L, 1, 1, 0);
  lua_getfield(L, -1, "new");
  lua_pushnumber(L, 256);
  lua_pcall(L, 1, 1, 0);
  lua_setfield(L, 1, "syncarray");
  return 0;
}

int LuaWaveOscBind::set_sync_fadeout(lua_State* L) {
  boost::shared_ptr<WaveOsc> osc = LuaHelper::check_sptr<WaveOsc>(L, 1, meta);
  int fadeout = luaL_checknumber (L, 2);
  osc->resampler()->set_sync_fadeout_size(fadeout);
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// DspMathHelperBind
///////////////////////////////////////////////////////////////////////////////
int LuaDspMathHelper::open(lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"notetofreq", notetofreq},
    {"freqtonote", freqtonote},
    {NULL, NULL}
  };
  luaL_newlib(L, funcs);
  return 1;
}

int LuaDspMathHelper::notetofreq(lua_State* L) {
  double note = luaL_checknumber(L, 1);
  int n = lua_gettop(L);
  int base = notecommands::middleA;
  if (n==2) {
    base = luaL_checknumber(L, 2);
  }
  lua_pushnumber(L, 440*std::pow(2.0, (note-base)/12.0));
  return 1;
}

int LuaDspMathHelper::freqtonote(lua_State* L) {
  double f = luaL_checknumber(L, 1);
  int n = lua_gettop(L);
  int base = notecommands::middleA;
  if (n==2) {
    base = luaL_checknumber(L, 2);
  }
  double note = 12*std::log10(f/440.0)/std::log10(2.0)+base;
  lua_pushnumber(L, note);
  return 1;
}


// FileHelperBind
int LuaFileHelper::open(lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"mkdir", mkdir},
    {"isdirectory", isdirectory},
    {"filetree", filetree},
	  {"parentdirectory", parentdirectory},
	  {"directorylist", directorylist},
    {"fileinfo", fileinfo},
    {"remove", remove},
    {"rename", rename},
    {NULL, NULL}
  };
  luaL_newlib(L, funcs);
  return 1;
}

int LuaFileHelper::mkdir(lua_State* L) {
  const char* dir_str = luaL_checkstring(L, 1); 
  try { 
    boost::filesystem::path dir(dir_str);
	boost::filesystem::create_directories(dir);
  } catch (boost::filesystem::filesystem_error& e) {
	lua_pushstring(L, e.what());
	return 1;
  }
  return 0;
}

int LuaFileHelper::isdirectory(lua_State* L) {
  const char* dir_path = luaL_checkstring(L, 1);
  std::string full_path = PsycleGlobal::configuration().GetAbsoluteLuaDir() + "\\" + dir_path;    
  boost::filesystem::path dir(full_path.c_str());
	lua_pushboolean(L, (boost::filesystem::is_directory(dir)));
  return 1;
}

int LuaFileHelper::remove(lua_State* L) {  
  std::string pathstr = luaL_checkstring(L, 1);
  boost::filesystem::path path(pathstr.c_str());
  try {
    boost::filesystem::remove(path);
  } catch (std::exception& e) {
    luaL_error(L, e.what());
  }
  return LuaHelper::chaining(L);
}

int LuaFileHelper::rename(lua_State* L) {  
  std::string path_old_str = luaL_checkstring(L, 1);
  boost::filesystem::path path_old(path_old_str.c_str());
  std::string path_new_str = luaL_checkstring(L, 2);
  boost::filesystem::path path_new(path_new_str.c_str());
  try {
    boost::filesystem::rename(path_old, path_new);
  } catch (std::exception& e) {
    luaL_error(L, e.what());
  }
  return LuaHelper::chaining(L);
}

int LuaFileHelper::fileinfo(lua_State* L) {
  const char* dir_path = luaL_checkstring(L, 1);      
  createfileinfo(L, boost::filesystem::path(dir_path));
  return 1;
}

int LuaFileHelper::parentdirectory(lua_State* L) {
  const char* dir_path = luaL_checkstring(L, 1);  
  using namespace boost::filesystem;    
  path p(dir_path);
  try {
    if (exists(p)) {
      if (is_regular_file(p)) { // is p a regular file?   
		  lua_pushnil(L); // no directory        
	  } else 
      if (is_directory(p)) {        
		    path parent = p.parent_path();
		    createfileinfo(L, parent);
      }
      else {
        lua_pushnil(L); // cout << p << " exists, but is neither a regular file nor a directory\n";
	    }
    } else {
      lua_pushnil(L); // does not exist
	  }
  } catch (const filesystem_error& ex) {
    ex.what();
    lua_pushnil(L);
  }
  return 1;
}

int LuaFileHelper::directorylist(lua_State* L) {
  const char* dir_path = luaL_checkstring(L, 1);  
  using namespace boost::filesystem;    
  path p(dir_path);
  try {
    if (exists(p)) {
      if (is_regular_file(p)) { // is p a regular file?   
		    lua_pushnil(L); // no directory        
	    } else 
      if (is_directory(p)) {        
        typedef std::vector<path> vec;
        vec v;
        std::copy(directory_iterator(p), directory_iterator(), std::back_inserter(v));
        std::sort(v.begin(), v.end());                                   
		    lua_newtable(L);
		    int i = 1;
        for (vec::const_iterator it (v.begin()); it != v.end(); ++it) {		  
		      path curr = *it;		     
		      if (is_directory(curr)) {
			     createfileinfo(L, curr);
			     lua_rawseti(L, -2, i++);
		      }		  
        }		
		    for (vec::const_iterator it (v.begin()); it != v.end(); ++it) {		  
		      path curr = *it;
		      if (is_regular_file(curr)) {
			      createfileinfo(L, curr);
			      lua_rawseti(L, -2, i++);
		      }		  
        }
      } else {
        lua_pushnil(L); // cout << p << " exists, but is neither a regular file nor a directory\n";
	    }
    } else {
      lua_pushnil(L); // does not exist
	  }
  } catch (const filesystem_error& ex) {
    ex.what();
    lua_pushnil(L);
  }
  return 1;
}

int LuaFileHelper::createfileinfo(lua_State* L, boost::filesystem::path curr) {
  lua_newtable(L);
  LuaHelper::setfield(L, "filename", curr.filename().string());
  if (is_directory(curr)) { 	
	LuaHelper::setfield(L, "isdirectory", true);			
    LuaHelper::setfield(L, "extension", "");	
  }	else 
  if (is_regular_file(curr)) {		
	LuaHelper::setfield(L, "isdirectory", false);            
    LuaHelper::setfield(L, "extension", curr.extension().string());	
  }		  
  curr.remove_filename();
  LuaHelper::setfield(L, "path", curr.string());			  
  return 1;
}

int LuaFileHelper::filetree(lua_State* L) {
  const char* dir_path = luaL_checkstring(L, 1);  
  using namespace boost::filesystem;    
  try {
    path dir(dir_path);
    recursive_directory_iterator begin = recursive_directory_iterator(dir);
    recursive_directory_iterator end = recursive_directory_iterator();
    recursive_directory_iterator it = begin;  
    std::string p;
    LuaHelper::new_lua_module(L, "psycle.node");    
    int level = 0;
    for ( ; it != end; ++it) {      
      path curr_path(it->path());      
      if (is_directory(it->status())) {
        level++;
        LuaHelper::new_lua_module(L, "psycle.node");
        lua_getfield(L, -2, "add");
        lua_pushvalue(L, -3);
        lua_pushvalue(L, -3);
        LuaHelper::setfield(L, "isdirectory", true);
        LuaHelper::setfield(L, "filename", "");
        LuaHelper::setfield(L, "extension", "");        
        boost::shared_ptr<ui::Node> node = LuaHelper::check_sptr<ui::Node>(L, -1, LuaNodeBind::meta);
        node->set_text(curr_path.filename().string());        
        LuaHelper::setfield(L, "path", curr_path.string()+"\\");
        int status = lua_pcall(L, 2, 1, 0);
        if (status) {        
          const char* msg = lua_tostring(L, -1);
          throw std::exception(msg);
        } 
        lua_pop(L, 1);
      } else {        
        for ( ; it.level() < level; --level) {          
          lua_pop(L, 1);
        }        
        lua_getfield(L, -1, "add");
        lua_pushvalue(L, -2);        
        LuaHelper::new_lua_module(L, "psycle.node");
        LuaHelper::setfield(L, "isdirectory", false);
        LuaHelper::setfield(L, "filename", curr_path.filename().string());
        LuaHelper::setfield(L, "extension", curr_path.extension().string());
        boost::shared_ptr<ui::Node> node = LuaHelper::check_sptr<ui::Node>(L, -1, LuaNodeBind::meta);
        node->set_text(curr_path.filename().string());
        curr_path.remove_filename();
        LuaHelper::setfield(L, "path", curr_path.string()+"\\");
        int status = lua_pcall(L, 2, 1, 0);
        if (status) {        
          const char* msg = lua_tostring(L, -1);
          throw std::exception(msg);
        }
        lua_pop(L, 1);
      }
    }
    while ((level--) > 0) {
      lua_pop(L, 1);
    }
  } catch (std::exception&) {
    lua_pushnil(L);
  }  
  return 1;
}

// MidiHelperBind
const char* gm_percusssion_names[]=
    {"Acoustic Bass Drum","Bass Drum 1","Side Stick","Acoustic Snare",
      "Hand Clap","Electric Snare","Low Floor Tom","Closed Hi Hat",
      "High Floor Tom","Pedal Hi-Hat","Low Tom","Open Hi-Hat",
      "Low-Mid Tom", "Hi Mid Tom", "Crash Cymbal 1", "High Tom",
      "Ride Cymbal 1", "Chinese Cymbal", "Ride Bell", "Tambourine",
      "Splash Cymbal", "Cowbell", "Crash Cymbal 2", "Vibraslap",
      "Ride Cymbal 2", "Hi Bongo", "Low Bongo", "Mute Hi Conga",
      "Open Hi Conga", "Low Bongo", "Mute Hi Conga", "Open Hi Conga",
      "Low Conga", "High Timbale", "Low Timbale", "High Agogo", "Low Agogo",
      "Cabasa", "Maracas", "Short Whistle", "Long Whistle", "Short Guiro",
      "Long Guiro", "Claves", "Hi Wood Block", "Low Wood Block",
      "Mute Cuica", "Open Cuica", "Mute Triangle", "Open Triangle"
    };

int LuaMidiHelper::open(lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"notename", notename},
	{"gmpercussionname", gmpercussionname},
    {"gmpercussionnames", gmpercussionnames},
    {"combine", combine},
    {NULL, NULL}
  };
  luaL_newlib(L, funcs);
  return 1;
}

int LuaMidiHelper::notename(lua_State* L) {
  const char* notenames[]=
      {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-", "C-"};
  int note = luaL_checknumber(L, 1);
  std::stringstream str;
  if (note >= 0) {
    str << notenames[note%12] << note/12;
  } else {
    str << notenames[12 + note%12] << "(" << note/12-1 << ")";
  }
  lua_pushstring(L, str.str().c_str());
  return 1;
}

int LuaMidiHelper::gmpercussionnames(lua_State* L) {  
  lua_createtable(L, 127, 0);
  for (int i = 1; i < 128; ++i) {
    lua_pushstring(L, i > 34 && i < 82 ? gm_percusssion_names[i - 35] : "");
    lua_rawseti(L, -2, i);
  }
  return 1;
}

int LuaMidiHelper::gmpercussionname(lua_State* L) { 
  int note = luaL_checknumber(L, 1); 
  lua_pushstring(L, note > 34 && note < 82 ? gm_percusssion_names[note - 35] : "");
  return 1;
}

int LuaMidiHelper::combine(lua_State* L) {
  std::uint8_t lsb = luaL_checknumber(L, 1);
  std::uint8_t msb = luaL_checknumber(L, 2);
  std::uint16_t val = (msb << 7) | lsb;
  lua_pushnumber(L, val);
  return 1;
}

// LuaMidiInput
LuaMidiInput::LuaMidiInput() : LuaState(0), imp_(new CMidiInput()) {
	imp_->SetListener(this);
}
LuaMidiInput::LuaMidiInput(lua_State* L) : LuaState(L), imp_(new CMidiInput()) {
	imp_->SetListener(this);
}	

void LuaMidiInput::SetDeviceId(unsigned int driver, int devId) {
	assert(imp_.get());
	return imp_->SetDeviceId(driver, devId);
}

bool LuaMidiInput::Open() {
	assert(imp_.get());
	return imp_->Open();
}

bool LuaMidiInput::Close() {
	assert(imp_.get());
	return imp_->Close();
}

bool LuaMidiInput::Active() {
	assert(imp_.get());
	return imp_->Active();
}

void LuaMidiInput::OnMidiData(uint32_t uMsg, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	try {
		switch(uMsg) {
			// normal data message
			case MIM_OPEN: {
				LuaImport in(L, this, LuaGlobal::proxy(L));
				if (in.open("onopen")) {			
					in << pcall(0);
				}
			}
			break;
			case MIM_CLOSE: {
				LuaImport in(L, this, LuaGlobal::proxy(L));
				if (in.open("onclose")) {			
					in << pcall(0);
				}
			}
			break;
			case MIM_DATA: {// split the first parameter DWORD into bytes
				int p1HiWordHB = (dwParam1 & 0xFF000000) >>24; p1HiWordHB; // not used
				int p1HiWordLB = (dwParam1 & 0xFF0000) >>16;
				int p1LowWordHB = (dwParam1 & 0xFF00) >>8;
				int p1LowWordLB = (dwParam1 & 0xFF);	
				// assign uses
				int note = p1LowWordHB;
				int program = p1LowWordHB;
				int data1 = p1LowWordHB;
				int velocity = p1HiWordLB;
				int data2 = p1HiWordLB;
				//int data = ((data2&0x7f)<<7)|(data1&0x7f);
				int status = p1LowWordLB;
				// and a bit more...
				int statusHN = (status & 0xF0) >> 4;
				int statusLN = (status & 0x0F);
				int channel = statusLN;
				
				LuaImport in(L, this, LuaGlobal::proxy(L));
				if (in.open("onmididata")) {			
					in << note << channel << velocity << program << statusHN << statusLN << data1 << data2 << pcall(0);
				}
			}
			break;
			default:;
		}			
	}  catch (std::exception& e) {
		ui::alert(e.what());   
	}
}

// LuaMidiInputBind
std::string LuaMidiInputBind::meta = "psymidiinput";

int LuaMidiInputBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
	{"numdevices", numdevices},
	{"devicedescription", devicedescription},
	{"finddevicebyname", finddevicebyname},
	{"setdevice", setdevice},
	{"open", openinput},
	{"close", closeinput},
	{"active", active},
    {NULL, NULL}
  }; 
  LuaHelper::open(L, meta, methods, gc);
  LuaHelper::setfield(L, "DRIVER_MIDI", DRIVER_MIDI);
  LuaHelper::setfield(L, "DRIVER_SYNC", DRIVER_SYNC);
  return 1;
}

int LuaMidiInputBind::create(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::new_shared_userdata<LuaMidiInput>(L, meta, new LuaMidiInput(L));
	return 1;
}

int LuaMidiInputBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaMidiInput>(L, meta);
}

int LuaMidiInputBind::numdevices(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::check_sptr<LuaMidiInput>(L, 1,  meta);
	lua_pushinteger(L, CMidiInput::GetNumDevices());
	return 1;
}

int LuaMidiInputBind::devicedescription(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::check_sptr<LuaMidiInput>(L, 1,  meta);
	int index = luaL_checkinteger(L, 2);	
	if (index < 1 || index > CMidiInput::GetNumDevices()) {
	  luaL_error(L, "Device index out of bounds.");
	}
	std::string desc;
	CMidiInput::GetDeviceDesc(index -1, desc);
	lua_pushstring(L, desc.c_str());
	return 1;
}

int LuaMidiInputBind::finddevicebyname(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::check_sptr<LuaMidiInput>(L, 1,  meta);
	const char* name = luaL_checkstring(L, 2);	
	CString nameString(name);
	int device_id = CMidiInput::FindDevByName(nameString);
	lua_pushinteger(L, device_id);
	return 1;
}

int LuaMidiInputBind::setdevice(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::check_sptr<LuaMidiInput>(L, 1,  meta);
	int driver_id = luaL_checkinteger(L, 2);
	int device_id = luaL_checkinteger(L, 3) - 1;
	midi_input->SetDeviceId(driver_id, device_id);
	return LuaHelper::chaining(L);
}

int LuaMidiInputBind::openinput(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::check_sptr<LuaMidiInput>(L, 1, meta);
	midi_input->Open();
	return LuaHelper::chaining(L);
}

int LuaMidiInputBind::closeinput(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::check_sptr<LuaMidiInput>(L, 1,  meta);
	midi_input->Close();
	return LuaHelper::chaining(L);
}

int LuaMidiInputBind::active(lua_State* L) {
	boost::shared_ptr<LuaMidiInput> midi_input = LuaHelper::check_sptr<LuaMidiInput>(L, 1,  meta);
	lua_pushboolean(L, midi_input->Active());
	return 1;
}


///////////////////////////////////////////////////////////////////////////////
// DspFilterBind
///////////////////////////////////////////////////////////////////////////////

const char* LuaDspFilterBind::meta = "psydspfiltermeta";

std::map<LuaDspFilterBind::Filter*, LuaDspFilterBind::Filter*>
  LuaDspFilterBind::filters;   // store map for samplerate change

int LuaDspFilterBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"work", work},
    {"setcutoff", setcutoff},
    {"cutoff", cutoff},
    {"setresonance", setresonance},
    {"resonance", resonance},
    {"settype", setfiltertype},
    {"tostring", tostring},
    { NULL, NULL }
  };
  LuaHelper::open(L, meta, methods, gc, tostring);
  static const char* const e[] =
    {"LOWPASS", "HIGHPASS", "BANDPASS", "BANDREJECT", "NONE", "ITLOWPASS",
      "MPTLOWPASS", "MPTHIGHPASS", "LOWPASS12E", "HIGHPASS12E", "BANDPASS12E",
      "BANDREJECT12E"};
  LuaHelper::buildenum(L, e, sizeof(e)/sizeof(e[0]));
  return 1;
}

int LuaDspFilterBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, f)", n);
  }
  int type = luaL_checknumber (L, 2);
  Filter* ud = new Filter();
  ud->Init(Global::player().SampleRate());
  ud->Type(static_cast<psycle::helpers::dsp::FilterType>(type));
  LuaHelper::new_shared_userdata<Filter>(L, meta, ud);
  filters[ud] = ud;
  return 1;
}

void LuaDspFilterBind::setsamplerate(double sr) {
  std::map<Filter*, Filter*>::iterator it = filters.begin();
  for ( ; it != filters.end(); ++it) {
    it->second->SampleRate(sr);
  }
}

int LuaDspFilterBind::work(lua_State* L) {
  int n = lua_gettop(L);
  if (n ==2 or n==3 or n==4) {
    float* vcfc = 0;
    float* vcfr = 0;
    boost::shared_ptr<Filter> filter = LuaHelper::check_sptr<Filter>(L, 1,  meta);
    PSArray* x_input = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
    if (n>2) {
      PSArray* arr = *(PSArray **)luaL_checkudata(L, 3, LuaArrayBind::meta);
      vcfc = arr->data();
    }
    if (n>3) {
      PSArray* arr = *(PSArray **)luaL_checkudata(L, 3, LuaArrayBind::meta);
      vcfr = arr->data();
    }
    int num = x_input->len();
    float* data = x_input->data();
    for (int i=0; i < num; ++i) {
      if (vcfc) {
        filter->Cutoff(std::min(std::max(0.0f, vcfc[i]), 127.0f));
      }
      if (vcfr) {
        filter->Ressonance(std::min(std::max(0.0f, vcfr[i]), 127.0f));
      }
      data[i] = filter->Work(data[i]);
    }
    lua_pushvalue(L, 2);
  }  else {
    luaL_error(L,
      "Got %d arguments expected 2 or 3(self, array filter input [, voltage control])", n);
  }
  return 1;
}

int LuaDspFilterBind::setcutoff(lua_State* L) {
  LuaHelper::callstrict1<Filter, int>(L, meta, &Filter::Cutoff);
  return LuaHelper::chaining(L);
}

int LuaDspFilterBind::cutoff(lua_State* L) {
  return LuaHelper::getnumber(L, meta, &Filter::Cutoff);
}

int LuaDspFilterBind::setresonance(lua_State* L) {
  LuaHelper::callstrict1<Filter, int>(L, meta, &Filter::Ressonance);
  return LuaHelper::chaining(L);
}

int LuaDspFilterBind::resonance(lua_State* L) {
  return LuaHelper::getnumber(L, meta, &Filter::Ressonance);
}

int LuaDspFilterBind::setfiltertype(lua_State* L) {
  int n = lua_gettop(L);
  if (n ==2) {
    boost::shared_ptr<Filter> filter = LuaHelper::check_sptr<Filter>(L, 1,  meta);
    int filtertype = luaL_checknumber(L, -1);
    filter->Type(static_cast<psycle::helpers::dsp::FilterType>(filtertype));
  }  else {
    luaL_error(L, "Got %d arguments expected 2(self, filtertype)", n);
  }
  return 0;
}

int LuaDspFilterBind::gc (lua_State *L) {
  boost::shared_ptr<Filter> filter = *(boost::shared_ptr<Filter>*) luaL_checkudata(L, 1, meta);  
  std::map<Filter*, Filter*>::iterator it = filters.find(filter.get());
  assert(it != filters.end());
  filters.erase(it);
  return LuaHelper::delete_shared_userdata<Filter>(L, meta);  
}

int LuaDspFilterBind::tostring(lua_State *L) {
  boost::shared_ptr<Filter> filter = LuaHelper::check_sptr<Filter>(L, 1,  meta);
  std::stringstream r;
  r << "filter " << "c=" << filter->Cutoff();
  lua_pushfstring(L, r.str().c_str());
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// WaveDataBind
///////////////////////////////////////////////////////////////////////////////

const char* LuaWaveDataBind::meta = "psywavedatameta";

int LuaWaveDataBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"copy", copy},
    {"setwavesamplerate", set_wave_sample_rate},
    {"setwavetune", set_wave_tune},
    {"setwavefinetune", set_wave_fine_tune},
    {"setloop", set_loop},
    {"copytobank", set_bank},
    { NULL, NULL }
  };
  LuaHelper::open(L, meta, methods, gc);
  static const char* const e[] = {"DO_NOT", "NORMAL", "BIDI"};
  LuaHelper::buildenum(L, e, sizeof(e)/sizeof(e[0]));
  return 1;
}

int LuaWaveDataBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 2 (self)", n);
  }
  LuaHelper::new_shared_userdata<>(L, meta, new XMInstrument::WaveData<float>);
  return 1;
}

int LuaWaveDataBind::copy(lua_State *L) {
  int n = lua_gettop(L);
  if (n!=2 and n!=3) {
    return luaL_error(L,
      "Got %d arguments expected 2 (self, array, array)", n);
  }
  typedef XMInstrument::WaveData<float> T;
  boost::shared_ptr<T> wave = LuaHelper::check_sptr<T>(L, 1, meta);
  PSArray* la = *(PSArray                                                                          **)luaL_checkudata(L, 2, LuaArrayBind::meta);
  PSArray* ra = 0;
  bool is_stereo = (n==3 );
  if (is_stereo) {
    ra = *(PSArray **)luaL_checkudata(L, 3, LuaArrayBind::meta);
  }
  wave->AllocWaveData(la->len(), is_stereo);
  float* l = wave->pWaveDataL();
  float* r = wave->pWaveDataR();
  for (int i = 0; i < la->len(); ++i) {
    l[i] = la->get_val(i);
    if (is_stereo) {
      r[i] = ra->get_val(i);
    }
  }
  return 0;
}

int LuaWaveDataBind::set_bank(lua_State *L) {
  int n = lua_gettop(L);
  if (n!=2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  int index = luaL_checknumber(L, 2);
  typedef XMInstrument::WaveData<float> T;
  boost::shared_ptr<T> wave = LuaHelper::check_sptr<T>(L, 1, meta);
  XMInstrument::WaveData<std::int16_t> wave16;
  int len = wave->WaveLength();
  wave16.AllocWaveData(wave->WaveLength(), wave->IsWaveStereo());
  wave16.WaveName("test");
  std::int16_t* ldest = wave16.pWaveDataL();
  std::int16_t* rdest = wave16.pWaveDataR();
  for (int i = 0; i < len; ++i) {
    ldest[i] = wave->pWaveDataL()[i]*32767;
    if (wave->IsWaveStereo()) {
      rdest[i] = wave->pWaveDataL()[i]*32767;
    }
  }
  wave16.WaveLoopStart(wave->WaveLoopStart());
  wave16.WaveLoopEnd(wave->WaveLoopEnd());
  wave16.WaveLoopType(wave->WaveLoopType());
  wave16.WaveTune(wave->WaveTune());
  wave16.WaveFineTune(wave->WaveFineTune());
  wave16.WaveSampleRate(wave->WaveSampleRate());
  wave16.WaveName("test");
  SampleList& list = Global::song().samples;
  list.SetSample(wave16, index);
  return 0;
}

int LuaWaveDataBind::set_wave_sample_rate(lua_State *L) {
  int n = lua_gettop(L);
  if (n!=2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, samplerate)", n);
  }
  typedef XMInstrument::WaveData<float> T;
  boost::shared_ptr<T> wave = LuaHelper::check_sptr<T>(L, 1, meta);
  int rate = luaL_checknumber(L, 2);
  wave->WaveSampleRate(rate);
  return 0;
}

int LuaWaveDataBind::set_wave_tune(lua_State *L) {
  int n = lua_gettop(L);
  if (n!=2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, tune)", n);
  }
  typedef XMInstrument::WaveData<float> T;
  boost::shared_ptr<T> wave = LuaHelper::check_sptr<T>(L, 1, meta);
  int tune = luaL_checknumber(L, 2);
  wave->WaveTune(tune);
  return 0;
}

int LuaWaveDataBind::set_wave_fine_tune(lua_State *L) {
  int n = lua_gettop(L);
  if (n!=2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, finetune)", n);
  }
  typedef XMInstrument::WaveData<float> T;
  boost::shared_ptr<T> wave = LuaHelper::check_sptr<T>(L, 1, meta);
  int tune = luaL_checknumber(L, 2);
  wave->WaveFineTune(tune);
  return 0;
}

int LuaWaveDataBind::set_loop(lua_State *L) {
  int n = lua_gettop(L);
  if (n!=3 and n!=4) {
    return luaL_error(L,
      "Got %d arguments expected 3 (self, start, end [, looptype])", n);
  }
  typedef XMInstrument::WaveData<float> T;
  boost::shared_ptr<T> wave = LuaHelper::check_sptr<T>(L, 1, meta);
  int loop_start = luaL_checknumber(L, 2);
  int loop_end = luaL_checknumber(L, 3);
  if (n==4) {
    int loop_type = luaL_checknumber(L, 4);
    wave->WaveLoopType(
      static_cast<const XMInstrument::WaveData<float>::LoopType::Type>(loop_type));
  } else {
    wave->WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
  }
  wave->WaveLoopStart(loop_start);
  wave->WaveLoopEnd(loop_end);
  return 0;
}

int LuaWaveDataBind::gc (lua_State *L) {
  return LuaHelper::delete_shared_userdata<XMInstrument::WaveData<float>>(L, meta);
}

///////////////////////////////////////////////////////////////////////////////
// PWM Wrap
///////////////////////////////////////////////////////////////////////////////

template <class T, int VOL>
PWMWrap<T, VOL>::PWMWrap() :
ResamplerWrap<T, VOL>(WaveOscTables::getInstance()->tbl(WaveOscTables::SAW)),
  pw_(0.5), pwm_(0) {
    resampler.quality(helpers::dsp::resampler::quality::linear);
    set_frequency(f_);
    wavectrl.Playing(false);
    wavectrl2.Playing(false);
    wave_it = waves_.find(range<double>(f_));
    last_wave = wave_it != waves_.end() ? wave_it->second : 0;
    wave_it2 = waves_.find(range<double>(f_));
    last_wave2 = wave_it2 != waves_.end() ? wave_it->second : 0;
}

template <class T, int VOL>
void PWMWrap<T, VOL>::Start(double phase) {
  wave_it = waves_.find(range<double>(f_));
  last_wave = wave_it != waves_.end() ? wave_it->second : 0;
  if (wave_it != waves_.end()) {
    wavectrl.Init(last_wave, 0, resampler);
    wavectrl.Position2(phase*last_wave->WaveLength());
    double sr = Global::player().SampleRate();
    basef = 440 *std::pow(2.0,
      (notecommands::middleC-last_wave->WaveTune()-
      last_wave->WaveFineTune()/100.0-notecommands::middleA)/12.0)*
      sr/last_wave->WaveSampleRate();
    speed_ = f_/basef;
    wavectrl.Speed(resampler, speed_);
    wavectrl.Playing(true);
  }
  // phase2
  wave_it2 = waves_.find(range<double>(f_));
  last_wave2 = wave_it2 != waves_.end() ? wave_it->second : 0;
  if (wave_it2 != waves_.end()) {
    wavectrl2.Init(last_wave2, 0, resampler);
    double p2 = (phase+pw_);
    while (p2 >=1) {p2--;}
    while (p2<0) {p2++;}
    wavectrl2.Position2(p2*last_wave2->WaveLength());
    double sr = Global::player().SampleRate();
    basef = 440 *std::pow(2.0,
      (notecommands::middleC-last_wave2->WaveTune()-
      last_wave2->WaveFineTune()/100.0-notecommands::middleA)/12.0)*
      sr/last_wave2->WaveSampleRate();
    speed_ = f_/basef;
    wavectrl2.Speed(resampler, speed_);
    wavectrl2.Playing(true);
  }
  last_sgn = 1;
  oldtrigger = 0;
  dostop_ = false;
}

template <class T, int VOL>
void PWMWrap<T, VOL>::set_frequency(double f) {
  f_ = f;
  speed_ = f_/basef;
  wavectrl.Speed(resampler, speed_);
  wavectrl2.Speed(resampler, speed_);
}

// hardsync work
template <class T, int VOL>
int PWMWrap<T, VOL>::work_trigger(int numSamples,
  float* pSamplesL,
  float* pSamplesR,
  float* fm,
  float* env,
  float* phase,
  SingleWorkInterface* master) {
    if (numSamples==0) return 0;
    int num = 0;
    if (oldtrigger) {
      for (int i=0; i < fsize; ++i) oldtrigger[i] = oldtrigger[n+i];
    }
    n = numSamples + ((oldtrigger) ? 0 : fsize);
    PSArray* arr = master->work(n, fsize);
    float* trigger = arr->data();
    float* samples = (oldtrigger) ? trigger : trigger+fsize;
    while (numSamples) {
      if (startfadeout) {
        int i = std::min(fsize-fadeout_sc, numSamples);
        if (i > 0) {
          num += work_samples(i, pSamplesL, pSamplesR, fm, env, phase);
          for (int k = 0; k < i; ++k) {
            float fader =  (fsize-fadeout_sc) / (float)fsize;
            if (pSamplesL) pSamplesL[0] *= fader;
            if (pSamplesR) pSamplesR[0] *= fader;
            numSamples--;
            samples++;
            if (pSamplesL) pSamplesL++;
            if (pSamplesR) pSamplesR++;
            if (fadeout_sc == (fsize-1)) {
              wavectrl.Position(0);
              wavectrl2.Position(pw_*last_wave2->WaveLength());
              startfadeout = false;
              fadeout_sc = 0;
              break;
            }
            ++fadeout_sc;
          }
        } else {
          wavectrl.Position(0);
          wavectrl2.Position(pw_*last_wave2->WaveLength());
          startfadeout = false;
          fadeout_sc = 0;
        }
      } else {
        int i = next_trigger(numSamples+fsize, samples)-fsize;
        if (i>=0) {
          num += work_samples(i, pSamplesL, pSamplesR, fm, env, phase);
          if (i != numSamples) {
            startfadeout = true;
            samples += i;
          }
          if (pSamplesL) pSamplesL += i;
          if (pSamplesR) pSamplesR += i;
          numSamples -= i;
        } else {
          int k = std::min(i+fsize, numSamples);
          num += work_samples(k, pSamplesL, pSamplesR, fm, env, phase);
          samples += i+fsize;
          if (pSamplesL) pSamplesL += k;
          if (pSamplesR) pSamplesR += k;
          numSamples -= k;
          if (numSamples < 0 ) {
            numSamples = 0;
          }
        }
      }
    }
    assert(numSamples==0);
    // adjust the master array to do slave/osc mix
    arr->margin(oldtrigger ? 0 : fsize, n);
    oldtrigger = trigger;
    return num;
}

template <class T, int VOL>
int PWMWrap<T, VOL>::work_samples(int numSamples,
  float* pSamplesL,
  float* pSamplesR,
  float* fm,
  float* env,
  float* ph) {
    if (!wavectrl.Playing())
      return 0;
    int num = numSamples;
    float left_output = 0.0f;
    float right_output = 0.0f;
    float left_output2 = 0.0f;
    float right_output2 = 0.0f;
    double f = f_;
    while(numSamples) {
      XMSampler::WaveDataController<T>::WorkFunction pWork;
      int pos = wavectrl.Position();
      if (wave_it != waves_.end() && speed_ > 0) {
        int step = 1;
        if (ph==NULL) {
          step = numSamples;
        }
        for (int z = 0; z < num; z+=step) {
        if(ph!=0) {
            double phase = wavectrl.Position2() / last_wave->WaveLength();
            phase += (*ph++);
            while (phase >=1) {--phase;}
            while (phase<0) {++phase;}
            wavectrl.Position2(phase*last_wave->WaveLength());
            double p2 = (phase+pw_);
            while (p2 >=1) {p2--;}
            while (p2<0) {p2++;}
            wavectrl2.Position2(p2*last_wave2->WaveLength());
        }
          int nextsamples = std::min(wavectrl.PreWork(
            ph!=0 ? 1 : numSamples, &pWork, false),
            ph!=0 ? 1 : numSamples);
        numSamples-=nextsamples;
        while (nextsamples) {
          if (pwm_!=0) {
            double phase = wavectrl.Position2() / last_wave->WaveLength();
            double pw = pw_+(*pwm_++)*0.5;
            pw = std::min(1.0, std::max(0.0, pw));
            double p2 = (phase+pw);
            while (p2 >=1) {p2--;}
            while (p2<0) {p2++;}
            wavectrl2.Position2(p2*last_wave2->WaveLength());
          }
          wavectrl2.PreWork(1, &pWork, false);
          pWork(wavectrl2, &left_output2, &right_output2);
          wavectrl2.PostWork();
          ++pos;
          if (dostop_) {
            double phase = pos / (double) last_wave->WaveLength();
            while (phase >= 1) {
              phase -= 1;
            }
            if (abs(stopphase_ - phase) <= speed_/ (double) last_wave->WaveLength()) {
              wavectrl.NoteOff();
              wavectrl.Playing(false);
              dostop_ = false;
            }
          }
          if (fm != 0) {
            f = (f_+*(fm++));
            speed_ = f/basef;
            wavectrl.Speed(resampler, speed_);
          }
          pWork(wavectrl, &left_output, &right_output);
          if (!wavectrl.IsStereo()) {
            right_output = left_output;
          }
          if (env != 0) {
            float v = (*env++);
            left_output *= v;
            right_output *= v;
            left_output2 *= v;
            right_output2 *= v;
          }
          if (pSamplesL) *pSamplesL++ += (left_output2-left_output)*adjust_vol*gain_;
          if (pSamplesR) *pSamplesR++ += (right_output2-right_output)*adjust_vol*gain_;
          nextsamples--;
        }
        wavectrl.PostWork();
        }
      } else {
        numSamples--;
        if (wave_it == waves_.end()) {
          check_wave(f);
        } else
          if (fm != 0) {
            f = (f_+*(fm++));
            speed_ = f/basef;
            wavectrl.Speed(resampler, speed_);
          }
      }
      if (!wavectrl.Playing()) {
        return num - numSamples;
      }
      check_wave(f);
    }
    return num;
}

template <class T, int VOL>
void PWMWrap<T, VOL>::check_wave(double f) {
  if (wave_it == waves_.end()) {
    wave_it = waves_.find(range<double>(f_));
  } else {
    while (wave_it != waves_.begin() && wave_it->first.min() > f) {
      wave_it--;
    }
    while (wave_it != waves_.end() && wave_it->first.max() < f) {
      wave_it++;
    }
  }
  if (wave_it != waves_.end()) {
    const XMInstrument::WaveData<T>* wave = wave_it->second;
    if (last_wave!=wave) {
      int oldpos = wavectrl.Position();
      wavectrl.Init(wave, 0, resampler);
      basef = 440 *std::pow(2.0,
        (notecommands::middleC-wave->WaveTune()-
        wave->WaveFineTune()/100-notecommands::middleA)/12.0)*
        Global::player().SampleRate() / wave->WaveSampleRate();
      wavectrl.Speed(resampler, f/basef);
      wavectrl.Playing(true);
      double phase = oldpos / (double) last_wave->WaveLength();
      int newpos = phase * wave->WaveLength();
      wavectrl.Position(newpos);
      last_wave = wave;
    }
  } else {
    last_wave = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Resampler Wrap
///////////////////////////////////////////////////////////////////////////////
template <class T, int VOL>
void ResamplerWrap<T, VOL>::Start(double phase) {
  wave_it = waves_.find(range<double>(f_));
  last_wave = wave_it != waves_.end() ? wave_it->second : 0;
  if (wave_it != waves_.end()) {
    wavectrl.Init(last_wave, 0, resampler);
    wavectrl.Position(phase*last_wave->WaveLength());
    double sr = Global::player().SampleRate();
    double l = last_wave->WaveSampleRate();
    basef = 440*std::pow(2.0,
      (notecommands::middleC-last_wave->WaveTune()-
      last_wave->WaveFineTune()/100.0-notecommands::middleA)/12.0)*sr/l;
    speed_ = f_/basef;
    wavectrl.Speed(resampler, speed_);
    wavectrl.Playing(true);
  }
  last_sgn = 1;
  oldtrigger = 0;
  dostop_ = false;
  lph_ = 0;
  lpos_ = 0;
}

template <class T, int VOL>
void ResamplerWrap<T, VOL>::set_frequency(double f) {
  f_ = f;
  speed_ = f_/basef;
  wavectrl.Speed(resampler, speed_);
}

// compute hardsync trigger points
template <class T, int VOL>
int ResamplerWrap<T, VOL>::next_trigger(int numSamples, float* trigger) {
  int res = numSamples;
  int tmp = 1;
  for (int i = 0; i < numSamples; ++i) {
    // check conversion from negative to positive
    if (i == numSamples-fsize-1) {
      tmp = last_sgn;
    }
    int sgn = (trigger[i] >= 0) ? 1 : -1;
    if (last_sgn < sgn) {
      last_sgn = sgn;
      res = i;
      break;
    }
    last_sgn = sgn;
  }
  if (res == numSamples) {
    last_sgn = tmp;
  }
  return res;
}

template <class T, int VOL>
int ResamplerWrap<T, VOL>::work(int num, float* left, float* right,
    SingleWorkInterface* master) {
  if (!master) {
    return work_samples(num, left, right, fm_, am_, pm_);
  } else {
    return work_trigger(num, left, right, fm_, am_, pm_, master);
  }
}

// hardsync work
template <class T, int VOL>
int ResamplerWrap<T, VOL>::work_trigger(int numSamples,
  float* pSamplesL,
  float* pSamplesR,
  float* fm,
  float* env,
  float* phase,
  SingleWorkInterface* master) {
    if (numSamples==0) return 0;
    int num = 0;
    if (oldtrigger) {
      for (int i=0; i < fsize; ++i) oldtrigger[i] = oldtrigger[n+i];
    }
    n = numSamples + ((oldtrigger) ? 0 : fsize);
    PSArray* arr = master->work(n, fsize);
    float* trigger = arr->data();
    float* samples = (oldtrigger) ? trigger : trigger+fsize;
    while (numSamples) {
      if (startfadeout) {
        int i = std::min(fsize-fadeout_sc, numSamples);
        if (i > 0) {
          num += work_samples(i, pSamplesL, pSamplesR, fm, env, phase);
          for (int k = 0; k < i; ++k) {
            float fader =  (fsize-fadeout_sc) / (float)fsize;
            if (pSamplesL) pSamplesL[0] *= fader;
            if (pSamplesR) pSamplesR[0] *= fader;
            numSamples--;
            samples++;
            if (pSamplesL) pSamplesL++;
            if (pSamplesR) pSamplesR++;
            if (fadeout_sc == (fsize-1)) {
              wavectrl.Position(0);
              startfadeout = false;
              fadeout_sc = 0;
              break;
            }
            ++fadeout_sc;
          }
        } else {
          wavectrl.Position(0);
          startfadeout = false;
          fadeout_sc = 0;
        }
      } else {
        int i = next_trigger(numSamples+fsize, samples)-fsize;
        if (i>=0) {
          num += work_samples(i, pSamplesL, pSamplesR, fm, env, phase);
          if (i != numSamples) {
            startfadeout = true;
            samples += i;
          }
          if (pSamplesL) pSamplesL += i;
          if (pSamplesR) pSamplesR += i;
          numSamples -= i;
        } else {
          int k = std::min(i+fsize, numSamples);
          num += work_samples(k, pSamplesL, pSamplesR, fm, env, phase);
          samples += i+fsize;
          if (pSamplesL) pSamplesL += k;
          if (pSamplesR) pSamplesR += k;
          numSamples -= k;
          if (numSamples < 0 ) {
            numSamples = 0;
          }
        }
      }
    }
    assert(numSamples==0);
    // adjust the master array to do slave/osc mix
    arr->margin(oldtrigger ? 0 : fsize, n);
    oldtrigger = trigger;
    return num;
}

template <class T, int VOL>
int ResamplerWrap<T, VOL>::work_samples(int numSamples,
  float* pSamplesL,
  float* pSamplesR,
  float* fm,
  float* env,
  float* ph) {
    if (!wavectrl.Playing())
      return 0;
    int num = numSamples;
    float left_output = 0.0f;
    float right_output = 0.0f;
    double f = f_;

    while(numSamples) {
      XMSampler::WaveDataController<T>::WorkFunction pWork;
      int pos = wavectrl.Position();
      if (wave_it != waves_.end() && speed_ > 0) {
        int step = 1;
        if (ph==NULL) {
          step = numSamples;
        }
        for (int z = 0; z < num; z+=step) {
          if(ph!=0) {
            double phase = wavectrl.Position2() / last_wave->WaveLength();
            phase += (*ph++);
            while (phase >=1) {--phase;}
            while (phase<0) {++phase;}
            wavectrl.Position2(phase*last_wave->WaveLength());
          }
          int nextsamples = std::min(wavectrl.PreWork(
            ph!=0 ? 1 : numSamples, &pWork, false),
            ph!=0 ? 1 : numSamples);
          numSamples-=nextsamples;
          while (nextsamples) {
            ++pos;
            if (dostop_) {
              double phase = pos / (double) last_wave->WaveLength();
              while (phase >= 1) {
                phase -= 1;
              }
              if (abs(stopphase_ - phase) <= speed_/ (double) last_wave->WaveLength()) {
                wavectrl.NoteOff();
                wavectrl.Playing(false);
                dostop_ = false;
              }
            }
            pWork(wavectrl, &left_output, &right_output);
            if (fm != 0) {
              f = (f_+*(fm++));
              speed_ = f/basef;
              wavectrl.Speed(resampler, speed_);
            }
            if (env != 0) {
              float v = (*env++);
              left_output *= v;
              right_output *= v;
            }
            if (!wavectrl.IsStereo()) {
              right_output = left_output;
            }
            if (pSamplesL) *pSamplesL++ += left_output*adjust_vol*gain_;
            if (pSamplesR) *pSamplesR++ += right_output*adjust_vol*gain_;
            nextsamples--;
          }
          wavectrl.PostWork();
        }
      } else {
        numSamples--;
        if (wave_it == waves_.end()) {
          check_wave(f);
        } else
          if (fm != 0) {
            f = (f_+*(fm++));
            speed_ = f/basef;
            wavectrl.Speed(resampler, speed_);
          }
      }
      if (!wavectrl.Playing()) {
        return num - numSamples;
      }
      check_wave(f);
    }
    return num;
}

template <class T, int VOL>
void ResamplerWrap<T, VOL>::check_wave(double f) {
  if (wave_it == waves_.end()) {
    wave_it = waves_.find(range<double>(f_));
  } else {
    while (wave_it != waves_.begin() && wave_it->first.min() > f) {
      wave_it--;
    }
    while (wave_it != waves_.end() && wave_it->first.max() < f) {
      wave_it++;
    }
  }
  if (wave_it != waves_.end()) {
    const XMInstrument::WaveData<T>* wave = wave_it->second;
    if (last_wave!=wave) {
      int oldpos = wavectrl.Position();
      wavectrl.Init(wave, 0, resampler);
      basef = 440 *std::pow(2.0,
        (notecommands::middleC-wave->WaveTune()-wave->WaveFineTune()/100-
        notecommands::middleA)/12.0) *
        Global::player().SampleRate() / wave->WaveSampleRate();
      wavectrl.Speed(resampler, f/basef);
      wavectrl.Playing(true);
      double phase = oldpos / (double) last_wave->WaveLength();
      int newpos = phase * wave->WaveLength();
      wavectrl.Position(newpos);
      last_wave = wave;
    }
  } else {
    last_wave = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Resampler Bind
///////////////////////////////////////////////////////////////////////////////

const char* LuaResamplerBind::meta = "psyresamplermeta";

PSArray* LuaSingleWorker::work(int numSamples, int val) {
  lua_getfield(L, -1, "syncarray");
  PSArray* arr = *(PSArray **)luaL_checkudata(L, -1, LuaArrayBind::meta);
  arr->resize(numSamples+val);
  arr->margin(val, val+numSamples);
  arr->clear();
  lua_getfield(L, -2, "sync");
  lua_getfield(L, -1, "work");
  lua_pushvalue(L, -2);
  lua_pushvalue(L, -4);
  int status = lua_pcall(L, 2, 1 ,0);
  if (status) {
    const char* msg =lua_tostring(L, -1);
    std::ostringstream s; s << "Failed: " << msg << std::endl;
    luaL_error(L, "single work error", 0);
  }
  // int num = luaL_checknumber(L, -1);
  lua_pop(L, 3);
  arr->clearmargin();
  return arr;
}

int LuaResamplerBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"newwavetable", createwavetable},
    {"work", work},
    {"noteoff", noteoff},
    {"setfrequency", set_frequency},
    {"frequency", frequency},
    {"start", start},
    {"stop", stop },
    {"phase", phase},
    {"setphase", setphase},
    {"isplaying", isplaying},
    {"setwavedata", set_wave_data},
    {"setquality", set_quality},
    {"quality", set_quality},
    {"setsync", set_sync},
    {"setsyncfadeout", set_sync_fadeout},
    {"setpm", setpm},
    {"setam", setam},
    {"setfm", setfm},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);
  lua_pushnumber(L, 1);
  lua_setfield(L, -2, "ZEROHOLD");
  lua_pushnumber(L, 2);
  lua_setfield(L, -2, "LINEAR");
  lua_pushnumber(L, 3);
  lua_setfield(L, -2, "SINC");
  return 1;
}

int LuaResamplerBind::createwavetable(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, wavetable)", n);
  }
  LuaHelper::new_shared_userdata<>(L, meta,
    new ResamplerWrap<float, 1>(check_wavelist(L)));
  return 1;
}

int LuaResamplerBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, wave)", n);
  }
  RWInterface* rwrap = 0;
  boost::shared_ptr<XMInstrument::WaveData<float> > wave =
    LuaHelper::check_sptr<XMInstrument::WaveData<float> >(L, 4,
    LuaWaveDataBind::meta);
  WaveList<float>::Type waves;
  waves[range<double>(0, 96000)] = wave.get();
  rwrap = new ResamplerWrap<float, 1>(waves);
  LuaHelper::new_shared_userdata<>(L, meta, rwrap);
  lua_pushnil(L);
  lua_setfield(L, -2, "sync");
  return 1;
}

WaveList<float>::Type LuaResamplerBind::check_wavelist(lua_State* L) {
  WaveList<float>::Type waves;
  if (lua_istable(L, 2)) {
    size_t len = lua_rawlen(L, 2);
    for (size_t i = 1; i <= len; ++i) {
      lua_rawgeti(L, 2, i); // get triple {Wave, flo, fhi}
      lua_rawgeti(L, 3, 1); // GetWaveData
      boost::shared_ptr<XMInstrument::WaveData<float> > wave =
    LuaHelper::check_sptr<XMInstrument::WaveData<float> >(L, 4,
    LuaWaveDataBind::meta);
      lua_pop(L,1);
      lua_rawgeti(L, 3, 2); // GetFreqLo
      double lo = luaL_checknumber(L, 4);
      lua_pop(L,1);
      lua_rawgeti(L, 3, 3); // GetFreqHi
      double hi = luaL_checknumber(L, 4);
      lua_pop(L,1);
      lua_pop(L,1);
      waves[range<double>( lo, hi )] = wave.get();
    }
  }
  return waves;
}

int LuaResamplerBind::set_wave_data(lua_State* L) {
  int n = lua_gettop(L);
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, wavedata)", n);
  }
  boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
  WaveList<float>::Type waves = check_wavelist(L);
  rwrap->SetData(waves);
  return 0;
}

int LuaResamplerBind::set_sync(lua_State* L) {
  boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_setfield(L, -2, "sync");
  lua_getglobal(L, "require");
  lua_pushstring(L, "psycle.array");
  lua_pcall(L, 1, 1, 0);
  lua_getfield(L, -1, "new");
  lua_pushnumber(L, 256);
  lua_pcall(L, 1, 1, 0);
  lua_setfield(L, 1, "syncarray");
  return 0;
}

int LuaResamplerBind::work(lua_State* L) {
  int n = lua_gettop(L);
  if (n!=3 and n != 4 and n!=5 and n!=6) {
    return luaL_error(L, "Got %d arguments expected 2 (self, left, right [,fm])", n);
  }
  boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
  if (!rwrap->Playing()) {
    lua_pushnumber(L, 0);
    return 1;
  }
  PSArray* l = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
  PSArray* r = *(PSArray **)luaL_checkudata(L, 3, LuaArrayBind::meta);

  float* fm = 0;
  float* env = 0;
  // frequence modulation -- optional
  if (n>3 && (!lua_isnil(L, 4))) {
    PSArray*arr = *(PSArray **)luaL_checkudata(L, 4, LuaArrayBind::meta);
    fm = arr->data();
  }
  // volume envelope -- optional
  if (n>4 && !lua_isnil(L, 5)) {
    PSArray* arr = *(PSArray **)luaL_checkudata(L, 5, LuaArrayBind::meta);
    env = arr->data();
  }
  // check for master
  std::auto_ptr<LuaSingleWorker> master;
  lua_getfield(L, 1, "sync");
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
  } else {
    lua_pushvalue(L, 1);
    master.reset(new LuaSingleWorker(L));
  }
  int processed = rwrap->work(l->len(), l->data(), r->data(), master.get());
  lua_pushnumber(L, processed); // return total number of resampled samples
  return 1;
}

int LuaResamplerBind::start(lua_State* L) {
  return LuaHelper::callopt1<RWInterface, double>(L, meta, &RWInterface::Start, 0);
}

int LuaResamplerBind::stop(lua_State* L) {
  return LuaHelper::callopt1<RWInterface, double>(L, meta, &RWInterface::Stop, 0);
}

int LuaResamplerBind::set_quality(lua_State* L) {
  int n = lua_gettop(L);
  if (n != 2) {
    return luaL_error(L, "Got %d arguments expected 2 (self, frequency)", n);
  }
  boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
  int quality = luaL_checknumber(L, 2);
  rwrap->set_quality(
    (psycle::helpers::dsp::resampler::quality::type)(quality-1));
  return 0;
}

int LuaResamplerBind::quality(lua_State* L) {
  int n = lua_gettop(L);
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }
  boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
  lua_pushnumber(L, (int)(rwrap->quality()+1));
  return 1;
}

int LuaResamplerBind::gc (lua_State *L) {
  return LuaHelper::delete_shared_userdata<RWInterface>(L, meta);
}

int LuaResamplerBind::setpm(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
    if (lua_isnil(L, 2)) {
      rwrap->setpm(0);
    } else {
      PSArray* v = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
      rwrap->setpm(v->data());
    }
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

int LuaResamplerBind::setfm(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
    if (lua_isnil(L, 2)) {
      rwrap->setfm(0);
    } else {
      PSArray* v = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
      rwrap->setfm(v->data());
    }
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

int LuaResamplerBind::setam(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 2) {
    boost::shared_ptr<RWInterface> rwrap = LuaHelper::check_sptr<RWInterface>(L, 1, meta);
    if (lua_isnil(L, 2)) {
      rwrap->setam(0);
    } else {
      PSArray* v = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);
      rwrap->setam(v->data());
    }
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return LuaHelper::chaining(L);
}

///////////////////////////////////////////////////////////////////////////////
// Envelope
///////////////////////////////////////////////////////////////////////////////
void LEnvelope::work(int num) {
  out_.resize(num);
  if (!is_playing() || (stage_ == sus_ && !susdone_)) {
    out_.fill(lv_);
    return;
  }
  float* data = out_.data();
  for (int k = 0; k < num; ++k) {
    if (nexttime_ == sc_) {
      ++stage_;
      if (stage_ > peaks_.size()-1 or (!susdone_ && stage_ == sus_)) {
        lv_ = peaks_[stage_-1];
        m_ = 0;
        for (int j = k; j < num; ++j) out_.set_val(j, lv_);
        break;
      }
      calcstage(peaks_[stage_-1], peaks_[stage_], times_[stage_],
        types_[stage_]);
    }
    if (types_[stage_]==0) {
      *(data++) = lv_;
      lv_ += m_;
    } else {
      // *(data++) = (up_) ? peaks_[stage_]-lv_ : lv_;
      *(data++) = lv_;
      lv_ *= m_;
    }
    ++sc_;
  }
}

void LEnvelope::release() {
  if (is_playing()) {
    // int old = stage_;
    susdone_ = true;
    if (stage_ != sus_) {
      stage_ = peaks_.size()-1;
    }
    //calcstage((up_) ?  peaks_[old]-lv_
    //	            : lv_, peaks_[stage_], times_[stage_], types_[stage_]);
    calcstage(lv_, peaks_[stage_], times_[stage_], types_[stage_]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// LuaEnvelopeBind
///////////////////////////////////////////////////////////////////////////////

const char* LuaEnvelopeBind::meta = "psyenvelopemeta";

int LuaEnvelopeBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"work", work },
    {"release", release},
    {"isplaying", isplaying},
    {"start", start },
    {"setpeak", setpeak},
    {"setstartvalue", setstartvalue},
    {"lastvalue", lastvalue},
    {"peak", peak},
    {"settime", setstagetime},
    {"time", time},
    {"tostring", tostring},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc, tostring);
  LuaHelper::setfield(L, "LIN", 0);
  LuaHelper::setfield(L, "EXP", 1);
  return 1;
}

int LuaEnvelopeBind::create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments
  if (n != 3) {
    return luaL_error(L,
      "Got %d arguments expected 3 (self, points, sustainpos", n);
  }
  std::vector<double> times;
  std::vector<double> peaks;
  std::vector<int> types;
  if (lua_istable(L, 2)) {
    size_t len = lua_rawlen(L, 2);
    for (size_t i = 1; i <= len; ++i) {
      lua_rawgeti(L, 2, i); // get point {time, peak}
      size_t argnum = lua_rawlen(L, 4);
      lua_rawgeti(L, 4, 1); // get time
      double t = luaL_checknumber(L, 5);
      times.push_back(t);
      lua_pop(L,1);
      lua_rawgeti(L, 4, 2); // get peak
      peaks.push_back(luaL_checknumber(L, 5));
      lua_pop(L,1);
      if (argnum == 3) {
        lua_rawgeti(L, 4, 3); // get type
        if (!lua_isnil(L, 5)) {
          types.push_back(luaL_checknumber(L, 5));
        }
        lua_pop(L,1);
      } else types.push_back(0);
      lua_pop(L,1);
    }
  }
  int suspos = luaL_checknumber(L, 3)-1;
  double startpeak = 0;
  if (n==4) {
    startpeak = luaL_checknumber(L, 4);
  }
  LuaHelper::new_shared_userdata<LEnvelope>(L, meta,
    new LEnvelope(times,
    peaks,
    types,
    suspos,
    startpeak,
    Global::player().SampleRate()
    ));
  return 1;
}

int LuaEnvelopeBind::gc(lua_State *L) {
  return LuaHelper::delete_shared_userdata<LEnvelope>(L, meta);
}

int LuaEnvelopeBind::work(lua_State* L) {
  int n = lua_gettop(L);
  if (n ==2) {
    boost::shared_ptr<LEnvelope> env = LuaHelper::check_sptr<LEnvelope>(L, 1, meta);
    int num = luaL_checknumber(L, 2);
    env->work(num);
    PSArray ** rv = (PSArray **)lua_newuserdata(L, sizeof(PSArray *));
    luaL_setmetatable(L, LuaArrayBind::meta);
    *rv = new PSArray(env->out().data(), num);
  }  else {
    luaL_error(L, "Got %d arguments expected 2 (self, num)", n);
  }
  return 1;
}

int LuaEnvelopeBind::setpeak(lua_State* L) {
  LuaHelper::callstrict2<LEnvelope>(L, meta, &LEnvelope::setstagepeak, true);  
  return LuaHelper::chaining(L);
}

int LuaEnvelopeBind::peak(lua_State* L) {
  int n = lua_gettop(L);
  if (n ==2) {
    boost::shared_ptr<LEnvelope> env = LuaHelper::check_sptr<LEnvelope>(L, 1, meta);
    int idx = luaL_checknumber(L, 2)-1;
    lua_pushnumber(L, env->peak(idx));
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return 1;
}

int LuaEnvelopeBind::time(lua_State* L) {
  //LuaHelper::bind(L, meta, &LEnvelope::time);
  int n = lua_gettop(L);
  if (n ==2) {
    boost::shared_ptr<LEnvelope> env = LuaHelper::check_sptr<LEnvelope>(L, 1, meta);
    int idx = luaL_checknumber(L, 2)-1;
    lua_pushnumber(L, env->time(idx));
  } else {
    luaL_error(L, "Got %d arguments expected 2 (self, index)", n);
  }
  return 1;
}

int LuaEnvelopeBind::setstagetime(lua_State* L) {
  LuaHelper::callstrict2<LEnvelope>(L, meta, &LEnvelope::setstagetime, true);
  return LuaHelper::chaining(L);
}

int LuaEnvelopeBind::tostring(lua_State *L) {    
  lua_pushfstring(L,"env");
  return 1;
}
} // namespace
} // namespace