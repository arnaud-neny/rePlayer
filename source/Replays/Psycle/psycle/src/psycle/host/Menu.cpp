#pragma once

// #include "stdafx.h"

#include "Menu.hpp"

namespace psycle {
namespace host {
namespace ui {

class {
 public:
  template<typename T>
  operator boost::shared_ptr<T>() { return boost::shared_ptr<T>(); }
  template<typename T>
  operator boost::weak_ptr<T>() { return boost::weak_ptr<T>(); }
} nullpointer;


} // namespace ui
} // namespace host
} // namespace psycle