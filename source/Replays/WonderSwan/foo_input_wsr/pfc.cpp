#include "pfc.h"

#include <guiddef.h>
#include "pfc-lite.h"
#include "string_conv.h"

namespace pfc
{
    core::Array<char> string_ansi_to_utf8(const char* buf, size_t size)
    {
        pfc::stringcvt::string_utf8_from_ansi converter;
        converter.convert(buf, size);
        return { converter.get_ptr(), converter.length() };
    }
}
