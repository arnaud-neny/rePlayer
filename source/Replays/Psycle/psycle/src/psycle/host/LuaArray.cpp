// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#include <psycle/host/detail/project.hpp>
#include "LuaArray.hpp"
#include "LuaHelper.hpp"
#include <lua.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>


namespace psycle {
namespace host {

PSArray::PSArray(int len, float v) :
    len_(len),
    baselen_(len_),
    shared_(0) {
  cap_ = len_ - (len_ % 4) + 4;
  universalis::os::aligned_memory_alloc(16, ptr_, cap_);
  can_aligned_ = is_aligned(ptr_);
  if (can_aligned_ && v==0) {
    psycle::helpers::dsp::Clear(ptr_, len_);
  } else {    
    std::fill_n(ptr_, len_, v);
  }
  base_ = ptr_;
}

PSArray::PSArray(double start, double stop, double step) : shared_(0) {
    baselen_ = len_ = (stop-start+0.5)/step;
    cap_ = len_ - (len_ % 4) + 4;
    universalis::os::aligned_memory_alloc(16, ptr_, cap_); // reserve
    base_ = ptr_;
    int count = 0;
    for (double i=start; i < stop; i+=step, ++count) ptr_[count] = i;
    can_aligned_ = is_aligned(ptr_);
}

PSArray::PSArray(PSArray& a1, PSArray& a2) : shared_(0) {
  baselen_ = len_ = a1.len() + a2.len();
  cap_ = len_ - (len_ % 4) + 4;
  universalis::os::aligned_memory_alloc(16, ptr_, cap_); // reserve
  base_ = ptr_;
  for (int i=0; i < a1.len_; ++i) ptr_[i] = a1.ptr_[i];
  for (int i=0; i < a2.len_; ++i) ptr_[i+a1.len_] = a2.ptr_[i];
  can_aligned_ = is_aligned(ptr_);
}

PSArray::PSArray(const PSArray& other) :
    cap_(other.cap_),
    len_(other.len_),
    baselen_(other.len_),
    shared_(other.shared_),
    can_aligned_(other.can_aligned_) {
  if (!shared_ != 0 && other.base_ != 0) {
    // create new mem and copy data from rhs into it
    universalis::os::aligned_memory_alloc(16, base_, other.cap_);
    if (is_aligned(base_)) {
      psycle::helpers::dsp::Mov(other.base_, base_, other.cap_);
    } else {
      std::copy(other.base_, other.base_ + other.cap_, base_);
    }
    // restore margin position
    ptr_ = base_ + (other.ptr_ - other.base_);
  } else {
    ptr_ = other.ptr_;
    base_ = other.ptr_;
  }
}

void PSArray::swap(PSArray& rhs) {
  using std::swap;
  swap(ptr_, rhs.ptr_);
  swap(base_, rhs.base_);
  swap(cap_, rhs.cap_);
  swap(len_, rhs.len_);
  swap(baselen_, rhs.baselen_);
  swap(can_aligned_, rhs.can_aligned_);
  swap(shared_, rhs.shared_);
}

void PSArray::resize(int newsize) {
  if (!shared_ && cap_ < newsize) {
    std::vector<float> buf(base_, base_+baselen_);
    universalis::os::aligned_memory_dealloc(base_);
    cap_ = newsize - (newsize % 4) + 4;
    universalis::os::aligned_memory_alloc(16, ptr_, cap_);
    for (int i = 0; i < baselen_; ++i) ptr_[i]=buf[i];
    base_ = ptr_;
  }
  baselen_ = len_ = newsize;
  can_aligned_ = is_aligned(ptr_);
}

bool PSArray::copyfrom(PSArray& src) {
  if (src.len() != len_) {
    return false;
  }
  if (src.can_aligned_) {
    psycle::helpers::dsp::Mov(src.ptr_, ptr_, len_);
  } else {
    std::copy(src.ptr_, src.ptr_ + len_, ptr_);
  }
  return true;
}

void PSArray::clear() {
  if (base_ == ptr_ && can_aligned_) {
    psycle::helpers::dsp::Clear(ptr_, len_);
  } else {
    std::memset(ptr_, 0, len_ * sizeof *ptr_);
  }
}

void PSArray::fill(float val) {
  std::fill_n(ptr_, len_, val); // todo sse2
}

void PSArray::fill(float val, int pos) {
  int num = len_ - pos;
  for (int i = pos; i < num; ++i) {
    ptr_[i] = val;
  }
}

void PSArray::mul(float multi) {
  if (can_aligned_) {
    psycle::helpers::dsp::Mul(ptr_, len_, multi);
  } else {
    for (int i = 0; i < len_; ++i) { ptr_[i] *= multi; }
  }
}

void PSArray::mul(PSArray& src) { // todo sse2
  float* s = src.data();
  for (int i = 0; i < len_; ++i) ptr_[i] *= s[i];
}

void PSArray::sqrt(PSArray& src) {
  for (int i = 0; i < len_; ++i) ::sqrt(ptr_[i]);
}

void PSArray::mix(PSArray& src, float multi) {
  float* srcf = src.data();
  if (can_aligned_) {
    psycle::helpers::dsp::Add(srcf, ptr_, len_, multi);
  } else {
    for(int i = 0; i < len_; ++i) ptr_[i] += srcf[i] * multi;
  }
}

void PSArray::add(PSArray& src) {
  float* s = src.data();
  if (can_aligned_) {
    psycle::helpers::dsp::SimpleAdd(s, ptr_, len_);
  } else {
    for (int i = 0; i < len_; ++i) ptr_[i] += s[i];
  }
}

void PSArray::sub(PSArray& src) {
  float* s = src.data();
  if (can_aligned_) {
    psycle::helpers::dsp::SimpleSub(s, ptr_, len_);
  } else {
    for (int i = 0; i < len_; ++i) ptr_[i] -= s[i];
  }
}

void PSArray::add(float addend) { // todo sse2
  for (int i = 0; i < len_; ++i) ptr_[i] += addend;
}

void PSArray::rsum(double lv) {
  double sum = lv;
  for (int i = 0; i < len(); ++i) {
    sum = sum + get_val(i);
    set_val(i, sum);
  }
}

int PSArray::copyfrom(PSArray& src, int pos) {
  if (pos < 0) return 0;
  for (int i = 0; i < src.len_ && i+pos < len_; ++i) {
    ptr_[i+pos] = src.ptr_[i];
  }
  return 1;
}

std::string PSArray::tostring() const {
  std::stringstream res;
  res << "[ ";
  for (int i=0; i < len_; ++i)
    res << ptr_[i] << " ";
  res << "]";
  return res.str();
}

template<class T>
void PSArray::do_op(T& func) {
  if (len_ > 0) {
    func.p = ptr_;
    for (int i=0; i < len_; ++i) ptr_[i] = func(i);
  }
}

// LuaArrayBind

const char* LuaArrayBind::meta = "array_meta";

static void call(lua_State* L, void (PSArray::*pt2Member)()) { // function ptr
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, "array_meta");
	(rv->*pt2Member)();
}

int LuaArrayBind::array_new(lua_State *L) {
  int n = lua_gettop(L);
  PSArray ** udata = (PSArray **)lua_newuserdata(L, sizeof(PSArray *));
  luaL_setmetatable(L, meta);
  int size;
  double val;
  switch (n) {
  case 0 :
    *udata = new PSArray();
    break;
  case 1 : {
    if (lua_istable(L, 1)) {
      size_t len = lua_rawlen(L, 1);
      *udata = new PSArray(len, 0);
      float* ptr = (*udata)->data();
      for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, 1, i);
        *ptr++ = luaL_checknumber(L, -1);
        lua_pop(L,1);
      }
    } else {
      size = luaL_checknumber (L, 1);
      *udata = new PSArray(size, 0);
    }
            }
            break;
  case 2:
    size = luaL_checknumber (L, 1);
    val = luaL_checknumber (L, 2);
    *udata = new PSArray(size, val);
    break;
  default:
    luaL_error(L, "Got %d arguments expected max 2 (none, size, size+val)", n);
    ;
  }
  return 1;
}

int LuaArrayBind::array_copy(lua_State* L) {
  int n = lua_gettop(L);
  PSArray* dest = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (n==3) {
    int pos = luaL_checknumber (L, 2);
    PSArray* src = *(PSArray **)luaL_checkudata(L, 3, meta);
    dest->copyfrom(*src, pos);
  } else {
    PSArray* src = *(PSArray **)luaL_checkudata(L, 2, meta);
    if (!dest->copyfrom(*src)) {
      std::ostringstream o;
      o << "size src:" << src->len() << ", dst:" << dest->len() << "not compatible";
      luaL_error(L, o.str().c_str());
    }
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_arange(lua_State* L) {
  float start = luaL_checknumber (L, 1);
  float stop = luaL_checknumber (L, 2);
  float step = luaL_checknumber (L, 3);
  PSArray ** udata = (PSArray **)lua_newuserdata(L, sizeof(PSArray *));
  *udata = new PSArray(start, stop, step);
  luaL_setmetatable(L, "array_meta");
  return 1;
}

int LuaArrayBind::array_random(lua_State* L) {
  int size = luaL_checknumber (L, 1);
  PSArray ** ud = (PSArray **)lua_newuserdata(L, sizeof(PSArray *));
  *ud = new PSArray(size, 1);
  struct {float* p; float operator()(int y) {
    return p[y] = (lua_Number)(rand()%RAND_MAX) / (lua_Number)RAND_MAX; }
  } f;
  (*ud)->do_op(f);
  luaL_setmetatable(L, meta);
  return 1;
}

int LuaArrayBind::array_gc (lua_State *L) {
  PSArray* ptr = *(PSArray **)luaL_checkudata(L, 1, meta);
  delete ptr;
  return 0;
}

PSArray* LuaArrayBind::create_copy_array(lua_State* L, int idx) {
  PSArray* udata = *(PSArray **)luaL_checkudata(L, idx, meta);
  PSArray** rv = (PSArray **)lua_newuserdata(L, sizeof(PSArray*));
  luaL_setmetatable(L, meta);
  *rv = new PSArray(*udata);
  return *rv;
}

int LuaArrayBind::array_method_from_table(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  size_t len = lua_rawlen(L, 2);
  float* ptr = rv->data();
  for (size_t i = 1; i <= len; ++i) {
    lua_rawgeti(L, 2, i);
    *ptr++ = luaL_checknumber(L, -1);
    lua_pop(L, 1);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_to_table(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  lua_createtable(L, rv->len(), 0);
  for (size_t i = 0; i < rv->len(); ++i) {
    lua_pushnumber(L, rv->get_val(i));
    lua_rawseti(L, 2, i+1);
  }
  return 1;
}

int LuaArrayBind::array_method_add(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    rv->add(*v);
  } else {
    rv->add(luaL_checknumber (L, 2));
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_sub(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    rv->sub(*v);
  } else {
    rv->add(-luaL_checknumber (L, 2));
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_mix(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
  rv->mix(*v, luaL_checknumber (L, 3));
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_random(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  struct {float* p; float operator()(int y) {return (lua_Number)(rand()%RAND_MAX) / (lua_Number)RAND_MAX;;}} f;
  rv->do_op(f);
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_mul(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    rv->mul(*v);
  } else {
    rv->mul(luaL_checknumber (L, 2));
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_min(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return std::min(p[y],v[y]);}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; float c; float operator()(int y) {return std::min(p[y],c);}} f;
    f.c = luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_max(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return std::max(p[y],v[y]);}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; float c; float operator()(int y) {return std::max(p[y],c);}} f;
    f.c = luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_div(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return p[y]/v[y];}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; double c; float operator()(int y) {return p[y]/c;}} f;
    f.c = luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_and(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {
      return static_cast<int>(p[y]) & (int)v[y];}
    } f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; int c; float operator()(int y) {
      return static_cast<int>(p[y]) & c;}
    } f;
    f.c = (int) luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_or(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {
      return static_cast<int>(p[y]) | (int)v[y];}
    } f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; int c; float operator()(int y) {
      return static_cast<int>(p[y]) | c;}
    } f;
    f.c = (int) luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_xor(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {
      return static_cast<int>(p[y]) ^ (int)v[y];}
    } f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; int c; float operator()(int y) {
      return static_cast<int>(p[y]) ^ c;}
    } f;
    f.c = (int) luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_sleft(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {
      return static_cast<int>(p[y]) << (int)v[y];}
    } f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; int c; float operator()(int y) {
      return static_cast<int>(p[y]) << c;}
    } f;
    f.c = (int) luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_sright(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  if (lua_isuserdata(L, 2)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {
      return static_cast<int>(p[y]) >> (int)v[y];}
    } f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    struct {float* p; int c; float operator()(int y) {
      return static_cast<int>(p[y]) >> c;}
    } f;
    f.c = (int) luaL_checknumber (L, 2);
    rv->do_op(f);
  }
  return LuaHelper::chaining(L);
}

int LuaArrayBind::array_method_bnot(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  struct {float* p; float operator()(int y) {
    return ~static_cast<unsigned int>(p[y]);}
  } f;
  rv->do_op(f);
  return 1;
}

// ops
int LuaArrayBind::array_add(lua_State* L) {
  if ((lua_isuserdata(L, 1)) && (lua_isuserdata(L, 2))) {
    PSArray* rv = create_copy_array(L);
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return p[y]+v[y];}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    double param1 = 0;
    PSArray* rv = 0;
    if ((lua_isuserdata(L, 1)) && (lua_isnumber(L, 2))) {
      param1 = luaL_checknumber (L, 2);
      rv = create_copy_array(L);
    } else {
      param1 = luaL_checknumber (L, 1);
      rv = create_copy_array(L, 2);
    }
    struct {float* p; double c; float operator()(int y) {return p[y]+c;}} f;
    f.c = param1;
    rv->do_op(f);
  }
  return 1;
}

int LuaArrayBind::array_sub(lua_State* L) {
  if ((lua_isuserdata(L, 1)) && (lua_isuserdata(L, 2))) {
    PSArray* rv = create_copy_array(L);
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return p[y]-v[y];}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    float param1 = 0;
    PSArray* rv = 0;
    if ((lua_isuserdata(L, 1)) && (lua_isnumber(L, 2))) {
      param1 = luaL_checknumber (L, 2);
      rv = create_copy_array(L);
    } else {
      param1 = luaL_checknumber (L, 1);
      rv = create_copy_array(L, 2);
    }
    struct {float* p; float c; float operator()(int y) {return p[y]-c;}} f;
    f.c = param1;
    rv->do_op(f);
  }
  return 1;
}

int LuaArrayBind::array_mod(lua_State* L) {
  /*
  if ((lua_isuserdata(L, 1)) && (lua_isuserdata(L, 2))) {
    PSArray* rv = create_copy_array(L);
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, "array_meta");
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return p[y]%v[y];}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    float param1 = 0;
    PSArray* rv = 0;
    if ((lua_isuserdata(L, 1)) && (lua_isnumber(L, 2))) {
      param1 = luaL_checknumber (L, 2);
      rv = create_copy_array(L);
    } else {
      param1 = luaL_checknumber (L, 1);
      rv = create_copy_array(L, 2);
    }
    struct {float* p; float c; float operator()(int y) {return p[y]-c;}} f;
    f.c = param1;
    rv->do_op(f);
  }*/
  return 1;
}

int LuaArrayBind::array_op_pow(lua_State* L) {
  if ((lua_isuserdata(L, 1)) && (lua_isuserdata(L, 2))) {
    PSArray* rv = create_copy_array(L);
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return pow(p[y],v[y]);}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    float param1 = 0;
    PSArray* rv = 0;
    if ((lua_isuserdata(L, 1)) && (lua_isnumber(L, 2))) {
      param1 = luaL_checknumber (L, 2);
      rv = create_copy_array(L);
    } else {
      param1 = luaL_checknumber (L, 1);
      rv = create_copy_array(L, 2);
    }
    struct {float* p; float c; float operator()(int y) {return pow(p[y],c);}} f;
    f.c = param1;
    rv->do_op(f);
  }
  return 1;
}

int LuaArrayBind::array_sum(lua_State* L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  struct {float* p; float* v; float operator()(int y) {return p[y]+v[y];}} f;
  PSArray* dest = 0;
  for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
    PSArray* v = *(PSArray **)luaL_checkudata(L, -1, meta);
    if (!dest) {
      dest = new PSArray(v->len(), 0);
    }
    luaL_argcheck(L, dest->len() == v->len(), 2, "size not compatible");
    f.v = v->data();
    dest->do_op(f);
  }
  if (!dest) luaL_error(L, "no input arrays");
  PSArray ** ret_datum = (PSArray **)lua_newuserdata(L, sizeof(PSArray*));
  luaL_setmetatable(L, meta);
  *ret_datum = dest;
  return 1;
}

int LuaArrayBind::array_rsum(lua_State* L) {
  PSArray* v = *(PSArray **)luaL_checkudata(L, 1, meta);
  PSArray ** rv = (PSArray **)lua_newuserdata(L, sizeof(PSArray*));
  *rv = new PSArray(v->len(), 0);
  luaL_setmetatable(L, meta);
  double sum = 0;
  for (int i = 0; i < v->len(); ++i) {
    sum = sum + v->get_val(i);
    (*rv)->set_val(i, sum);
  }
  return 1;
}

int LuaArrayBind::array_method_rsum(lua_State* L) {
  PSArray* rv = *(PSArray **)luaL_checkudata(L, 1, meta);
  int n = lua_gettop(L);
  double sum = 0;
  if (n==2) {
    sum = luaL_checknumber(L, 2);
  }
  for (int i = 0; i < rv->len(); ++i) {
    sum = sum + rv->get_val(i);
    rv->set_val(i, sum);
  }
  return 1;
}

int LuaArrayBind::array_mul(lua_State* L) {
  if ((lua_isuserdata(L, 1)) && (lua_isuserdata(L, 2))) {
    PSArray* rv = create_copy_array(L);
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return p[y]*v[y];}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    float param1 = 0;
    PSArray* rv = 0;
    if ((lua_isuserdata(L, 1)) && (lua_isnumber(L, 2))) {
      param1 = luaL_checknumber (L, 2);
      rv = create_copy_array(L);
    } else {
      param1 = luaL_checknumber (L, 1);
      rv = create_copy_array(L, 2);
    }
    struct {float* p; float c; float operator()(int y) {return p[y]*c;}} f;
    f.c = param1;
    rv->do_op(f);
  }
  return 1;
}

  int LuaArrayBind::array_div(lua_State* L) {
  if ((lua_isuserdata(L, 1)) && (lua_isuserdata(L, 2))) {
    PSArray* rv = create_copy_array(L);
    PSArray* v = *(PSArray **)luaL_checkudata(L, 2, meta);
    luaL_argcheck(L, rv->len() == v->len(), 2, "size not compatible");
    struct {float* p; float* v; float operator()(int y) {return p[y]/v[y];}} f;
    f.v = v->data();
    rv->do_op(f);
  } else {
    float param1 = 0;
    PSArray* rv = 0;
    if ((lua_isuserdata(L, 1)) && (lua_isnumber(L, 2))) {
      param1 = luaL_checknumber (L, 2);
      rv = create_copy_array(L);
    } else {
      param1 = luaL_checknumber (L, 1);
      rv = create_copy_array(L, 2);
    }
    struct {float* p; float c; float operator()(int y) {return p[y]/c;}} f;
    f.c = param1;
    rv->do_op(f);
  }
  return 1;
}

int LuaArrayBind::array_unm(lua_State* L) {
  create_copy_array(L)->mul(-1);
  return 1;
}

int LuaArrayBind::array_sin(lua_State* L) {
  create_copy_array(L)->sin();
  return 1;
}

int LuaArrayBind::array_cos(lua_State* L) {
  create_copy_array(L)->cos();
  return 1;
}

int LuaArrayBind::array_tan(lua_State* L) {
  create_copy_array(L)->tan();
  return 1;
}

int LuaArrayBind::array_pow(lua_State* L) {
  create_copy_array(L)->pow(luaL_checknumber (L, 2));
  return 1;
}

int LuaArrayBind::array_sqrt(lua_State* L) {
  create_copy_array(L)->sqrt();
  return 1;
}

int LuaArrayBind::array_concat(lua_State* L) {
  PSArray* v1 = *(PSArray **)luaL_checkudata(L, 1, meta);
  PSArray* v2 = *(PSArray **)luaL_checkudata(L, 2, meta);
  PSArray ** rv = (PSArray **)lua_newuserdata(L, sizeof(PSArray*));
  *rv = new PSArray(*v1, *v2);
  luaL_setmetatable(L, "array_meta");
  return 1;
}

int LuaArrayBind::array_tostring(lua_State *L) {
  PSArray** ud = (PSArray**) luaL_checkudata(L, 1, meta);
  lua_pushfstring(L, (*ud)->tostring().c_str());
  return 1;
}

int LuaArrayBind::array_index(lua_State *L) {
  if (lua_isnumber(L, 2)) {
    PSArray** ud = (PSArray**) luaL_checkudata(L, 1, meta);
    int index = luaL_checknumber(L, 2);
    if (index <0 && index >= (*ud)->len()) {
      luaL_error(L, "index out of range");
    }
    lua_pushnumber(L, *((*ud)->data()+index));
    return 1;
  } else
    if (lua_istable(L, 2)) {
      PSArray** ud = (PSArray**) luaL_checkudata(L, 1, meta);
      std::vector<int> p;
      for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        p.push_back(luaL_checknumber(L, -1));
      }
      PSArray ** rv = (PSArray **)lua_newuserdata(L, sizeof(PSArray*));
      if (p[1]-p[0] == 0) {
        *rv = new PSArray();
      } else {
        if (!((0 <= p[0] && p[0] <= (*ud)->len()) &&
              (0 <= p[1] && p[1] <= (*ud)->len()) &&
              p[0] < p[1])) {
          std::ostringstream o;
          o << "index" << p[0] << "," << p[1] << "," << " out of range (max)" << (*ud)->len();
          luaL_error(L, o.str().c_str());
        }
        *rv = new PSArray(p[1]-p[0], 0);
        struct {float* p; float* v; int s; float operator()(int y) {return v[y+s];}} f;
        f.s = p[0];
        f.v = (*ud)->data();
        (*rv)->do_op(f);
      }
      luaL_setmetatable(L, meta);
      return 1;
    } else {
      size_t len;
      luaL_checkudata(L, 1, meta);
      const char* key = luaL_checklstring(L, 2, &len);
      lua_getmetatable(L, 1);
      for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
        const char* method = luaL_checklstring(L, -2, &len);
        int result = strcmp(key, method);
        if (result == 0) {
          return 1;
        }
      }
    }
    // no method found
    return 0;
}

int LuaArrayBind::array_new_index(lua_State *L) {
  if (lua_isnumber(L, 2)) {
    PSArray** ud = (PSArray**) luaL_checkudata(L, 1, meta);
    int index = luaL_checknumber(L, 2);
    float value = luaL_checknumber(L, 3);
    if (!(0 <= index && index < (*ud)->len())) {
        luaL_error(L, "index out of range");
    }
    *((*ud)->data()+index) = value;
    return 0;
  } else {
    //error
    lua_error(L);
    return 0;
  }
}

int LuaArrayBind::open(lua_State* L) {
  static const luaL_Reg pm_lib[] = {
    { "new", array_new },
    { "arange", array_arange },
    { "random", array_random },
    { "sin", array_sin },
    { "cos", array_cos },
    { "tan", array_tan },
    { "sqrt", array_sqrt },
    { "sum", array_sum },
    { "rsum", array_rsum },
    { "pow", array_pow },
    { NULL, NULL }
  };
  static const luaL_Reg pm_meta[] = {
    { "random", array_method_random},
    { "sin", array_method_sin},
    { "cos", array_method_cos },
    { "tan", array_method_tan },
    { "sqrt", array_method_sqrt},
    { "add", array_method_add},
    { "sub", array_method_sub},
    { "mix", array_method_mix},
    { "mul", array_method_mul},
    { "div", array_method_div},
    { "rsum", array_method_rsum},
    { "floor", array_method_floor},
    { "abs", array_method_abs},
    { "sgn", array_method_sgn},
    { "ceil", array_method_ceil},
    { "max", array_method_max},
    { "min", array_method_min},
    { "band", array_method_and},
    { "bor", array_method_or},
    { "bxor", array_method_xor},
    { "bleft", array_method_sleft},
    { "bright", array_method_sright},
    { "bnot", array_method_bnot},
    { "size", array_size},
    { "resize", array_resize},
    { "copy", array_copy},
    { "fillzero", array_fillzero},
    { "fill", array_method_fill},
    { "tostring", array_tostring },
    { "margin", array_margin},
    { "fromtable", array_method_from_table},
    { "table", array_method_to_table},
    { "clearmargin", array_clearmargin },
    { "__newindex", array_new_index },
    { "__index", array_index },
    { "__gc", array_gc },
    { "__tostring", array_tostring },
    { "__add", array_add },
    { "__sub", array_sub },
    { "__mul", array_mul },
    { "__div", array_div },
    { "__mod", array_mod },
    { "__pow", array_op_pow },
    { "__unm", array_unm },
    { "__concat", array_concat},
    { NULL, NULL }
  };
  luaL_newmetatable(L, meta);
  luaL_setfuncs(L, pm_meta, 0);
  lua_pop(L, 1);
  luaL_newlib(L, pm_lib);
  return 1;
}
}
}  // namespace