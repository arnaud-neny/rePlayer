#pragma once

#include "int_types.h"
#include <Containers/Array.h>

namespace pfc
{
    bool is_valid_utf8(const char* param, t_size max = SIZE_MAX);
    core::Array<char> string_ansi_to_utf8(const char* buf, size_t size);
}
// namespace pfc