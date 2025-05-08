// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include <psycle/host/detail/project.hpp>
#include <vector>
#include <map>
#include <string>
#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/value_mapper.hpp>
#include <psycle/helpers/dsp.hpp>
#include "LuaHelper.hpp"

namespace psycle { namespace host {
  // array wrapper (shared : float*,
  //                notshared: universalis::os::aligned_memory_alloc(16,..)

  class PSArray {
  public:
    PSArray()  {
      ptr_ = base_ = 0;
      shared_ = can_aligned_ = false;
      baselen_ = len_ = cap_ =  0;
    }
    PSArray(int len, float v);
    PSArray(double start, double stop, double step);
    PSArray(float* ptr, int len) : ptr_(ptr),
      base_(ptr),
      cap_(0), len_(len),
      baselen_(len),
      shared_(1),
      can_aligned_(is_aligned(ptr)) {}
    PSArray(PSArray& a1, PSArray& a2);
    PSArray(const PSArray& other);
    PSArray& operator=(PSArray rhs) { swap(rhs); return *this; }
#if defined _MSC_VER >= 1600
    PSArray(PSArray&& other) { swap(other); }
#endif
    ~PSArray() {
      if (!shared_ && base_)
        universalis::os::aligned_memory_dealloc(base_);
    }
    void swap(PSArray& rhs);

    void set_val(int i, float val) { ptr_[i] = val; }
    float get_val(int i) const { return ptr_[i]; }
    void set_len(int len) {len_ = len; }
    int len() const { return len_; }
    bool copyfrom(PSArray& src);
    int copyfrom(PSArray& src, int pos);
    void resize(int newsize);
    std::string tostring() const;
    void clear();
    void fillzero(int pos) { fill(0, pos); }
    void fill(float val);
    void fill(float val, int pos);
    void mul(float multi);
    void mul(PSArray& src);
    void mix(PSArray& src, float multi);
    void add(float addend);
    void add(PSArray& src);
    void sub(PSArray& src);
    void sqrt() { for (int i = 0; i < len_; ++i) ptr_[i] = ::sqrt(ptr_[i]); }
    void sqrt(PSArray& src);
    void sin() { for (int i = 0; i < len_; ++i) ptr_[i] = ::sin(ptr_[i]); }
    void cos() { for (int i = 0; i < len_; ++i) ptr_[i] = ::cos(ptr_[i]); }
    void tan() { for (int i = 0; i < len_; ++i) ptr_[i] = ::tan(ptr_[i]); }
    void floor() { for (int i = 0; i < len_; ++i) ptr_[i] = ::floor(ptr_[i]); }
    void ceil() { for (int i = 0; i < len_; ++i) ptr_[i] = ::ceil(ptr_[i]); }
    void abs() { for (int i = 0; i < len_; ++i) ptr_[i] = ::abs(ptr_[i]); }
    void pow(float exp) { for (int i = 0; i < len_; ++i) ptr_[i] = ::pow(ptr_[i], exp); }
    void sgn() {
      for (int i = 0; i < len_; ++i) {
        ptr_[i] = (ptr_[i] > 0) ? 1 : ((ptr_[i] < 0) ? -1 : 0);
      }
    }
    float* data() { return ptr_; }
    template<class T>
    void do_op(T&);
    void rsum(double lv);
    void margin(int start, int end) {
      ptr_ = base_ + start;
      assert(end >= start && start >= 0 && end <=baselen_);
      len_ = end-start;
      can_aligned_ = is_aligned(ptr_);
    }
    void offset(int offset) {
      base_ += offset;
      ptr_ += offset;
      can_aligned_ = is_aligned(ptr_);
    }
    void clearmargin() {
      ptr_ = base_;
      len_ = baselen_;
      can_aligned_ = is_aligned(ptr_);
    }
    inline bool canaligned() const { return can_aligned_; }
  private:
    static inline bool is_aligned(const void * pointer) {
      return (uintptr_t)pointer % 16u == 0;
    }
    float* ptr_, *base_;
    int cap_;
    int len_;
    int baselen_;
    int shared_;
    bool can_aligned_;
  };

  typedef std::vector<PSArray> psybuffer;

  struct LuaArrayBind {
    static int open(lua_State* L);
    static PSArray* create_copy_array(lua_State* L, int idx=1);
    static const char* meta;

    static int array_index(lua_State *L);
    static int array_new_index(lua_State *L);
    static int array_new(lua_State *L);
    static int array_new_from_table(lua_State *L);
    static int array_arange(lua_State *L);
    static int array_copy(lua_State* L);
    static int array_tostring(lua_State *L);
    static int array_gc(lua_State* L);
    // array methods
    static int array_size(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::len); }
    static int array_resize(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::resize); }
    static int array_margin(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::margin); }
    static int array_clearmargin(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::clearmargin); }
    static int array_concat(lua_State* L);
    static int array_fillzero(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::clear); }
    static int array_method_fill(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::fill); }
    static int array_method_add(lua_State* L);
    static int array_method_sub(lua_State* L);
    static int array_method_mix(lua_State* L);
    static int array_method_mul(lua_State* L);
    static int array_method_div(lua_State* L);
    static int array_method_rsum(lua_State* L); // x(n)=x(0)+..+x(n-1)
    static int array_method_and(lua_State* L); // binary and
    static int array_method_or(lua_State* L);
    static int array_method_xor(lua_State* L);
    static int array_method_sleft(lua_State* L);
    static int array_method_sright(lua_State* L);
    static int array_method_bnot(lua_State* L);
    static int array_method_min(lua_State* L);
    static int array_method_max(lua_State* L);
    static int array_method_ceil(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::ceil); }
    static int array_method_floor(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::floor); }
    static int array_method_abs(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::abs); }
    static int array_method_sgn(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::sgn); }
    static int array_method_random(lua_State* L);
    static int array_method_sin(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::sin); }
    static int array_method_cos(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::cos); }
    static int array_method_tan(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::tan); }
    static int array_method_sqrt(lua_State* L) { LUAEXPORTML(L, meta, &PSArray::sqrt); }
    static int array_method_from_table(lua_State* L);
    static int array_method_to_table(lua_State* L);
    // ops
    static int array_add(lua_State* L);
    static int array_sub(lua_State* L);
    static int array_mul(lua_State* L);
    static int array_div(lua_State* L);
    static int array_mod(lua_State* L);
    static int array_op_pow(lua_State* L);
    static int array_unm(lua_State* L);
    static int array_sum(lua_State* L);
    static int array_rsum(lua_State* L); // x(n)=x(0)+..+x(n-1)
    // funcs
    static int array_sin(lua_State* L);
    static int array_cos(lua_State* L);
    static int array_tan(lua_State* L);
    static int array_sqrt(lua_State* L);
    static int array_random(lua_State* L);
    static int array_pow(lua_State* L);
  };
}  // namespace
}  // namespace