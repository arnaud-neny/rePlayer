#include "scriptasm.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace V2
{
    ScriptAsm::ScriptAsm()
    {
        rsp.data = reinterpret_cast<int64_t>(m_stack + 32768);
    }

    void ScriptAsm::RDTSC()
    {
        auto rdtsc = __rdtsc();
        eax.data = static_cast<int32_t>(rdtsc);
        edx.data = static_cast<int32_t>(rdtsc >> 32);
    }
}
//namespace V2