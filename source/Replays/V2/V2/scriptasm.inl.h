#include "scriptasm.h"

#include "int128_t.h"

#include <cmath>
#include <numbers>
#include <utility>

namespace V2
{
    inline void ScriptAsm::ADD(Reg64 op0, int64_t op1)
    {
        // todo flags
        op0.data += op1;
    }

    inline void ScriptAsm::ADD(Reg64 op0, int32_t* op1)
    {
        ADD(op0, *op1);
    }

    inline void ScriptAsm::ADD(Reg32 op0, int32_t op1)
    {
        // todo AF
        uint32_t dst = static_cast<uint32_t>(op0.data);
        int64_t iDst = op0.data;
        int64_t iSrc = op1;
        uint32_t add = dst + static_cast<uint32_t>(op1);
        int64_t iAdd = iDst + iSrc;
        op0.data = add;
        //m_flags.AF = ?;
        m_flags.CF = (uint64_t(dst) + uint64_t(op1)) > 0xffFFffFFull;
        m_flags.OF = (iAdd < static_cast<int32_t>(0x80000000)) || (iAdd > static_cast<int32_t>(0x7fFFffFF));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (add >> 31) & 1;
        m_flags.PF = (add & 1) ^ 1;
    }

    inline void ScriptAsm::ADD(void* op0, Reg32 op1)
    {
        // todo AF
        int32_t src = op1.data;
        uint32_t dst = *reinterpret_cast<uint32_t*>(op0);
        int64_t iDst = static_cast<int32_t>(dst);
        int64_t iSrc = src;
        uint32_t add = dst + static_cast<uint32_t>(src);
        int64_t iAdd = iDst + iSrc;
        *reinterpret_cast<uint32_t*>(op0) = add;
        //m_flags.AF = ?;
        m_flags.CF = (uint64_t(dst) + uint64_t(src)) > 0xffFFffFFull;
        m_flags.OF = (iAdd < static_cast<int32_t>(0x80000000)) || (iAdd > static_cast<int32_t>(0x7fFFffFF));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (add >> 31) & 1;
        m_flags.PF = (add & 1) ^ 1;
    }

    inline void ScriptAsm::ADD(Reg32 op0, void* op1)
    {
        ADD(op0, *reinterpret_cast<int32_t*>(op1));
    }

    inline void ScriptAsm::ADD(Reg32 op0, Reg32 op1)
    {
        ADD(op0, op1.data);
    }

    inline void ScriptAsm::AND(Reg8 op0, int8_t op1)
    {
        int8_t data = op0.data & op1;
        op0.data = data;
        m_flags.CF = m_flags.OF = 0;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 7) & 1;
        m_flags.PF = (data & 1) ^ 1;
    }

    inline void ScriptAsm::AND(Reg32 op0, int32_t op1)
    {
        int32_t data = op0.data & op1;
        op0.data = data;
        m_flags.CF = m_flags.OF = 0;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 31) & 1;
        m_flags.PF = (data & 1) ^ 1;
    }

    inline void ScriptAsm::AND(Reg32 op0, void* op1)
    {
        AND(op0, *reinterpret_cast<int32_t*>(op1));
    }

    inline void ScriptAsm::CMOVC(Reg32 op0, void* op1)
    {
        if (m_flags.CF == 1)
            op0.data = *reinterpret_cast<int32_t*>(op1);
    }

    inline void ScriptAsm::CMOVE(Reg32 op0, Reg32 op1)
    {
        if (m_flags.ZF == 1)
            op0.data = op1.data;
    }

    inline void ScriptAsm::CMOVGE(Reg32 op0, Reg32 op1)
    {
        if (m_flags.SF == m_flags.OF)
            op0.data = op1.data;
    }

    inline void ScriptAsm::CMOVLE(Reg32 op0, Reg32 op1)
    {
        if (m_flags.ZF == 1 || m_flags.SF != m_flags.OF)
            op0.data = op1.data;
    }

    inline void ScriptAsm::CMP(Reg64 op0, Reg64 op1)
    {
        // todo AF
        int64_t src = op1.data;
        uint64_t dst = static_cast<uint64_t>(op0.data);
        int128_t iDst = static_cast<int64_t>(dst);
        int128_t iSrc = src;
        uint64_t sub = dst - static_cast<uint64_t>(src);
        int128_t iSub = iDst - iSrc;
        //m_flags.AF = ?;
        m_flags.CF = dst < static_cast<uint64_t>(src);
        m_flags.OF = (iSub < int128_t(0xffFFffFFffFFffFF, 0x8000000000000000) || iSub > int128_t(0, 0x7fFFffFFffFFffFF));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (sub >> 63) & 1;
        m_flags.PF = (sub & 1) ^ 1;
    }

    inline void ScriptAsm::CMP(Reg32 op0, int32_t op1)
    {
        // todo AF
        uint32_t dst = static_cast<uint32_t>(op0.data);
        int64_t iDst = static_cast<int32_t>(dst);
        int64_t iSrc = op1;
        uint32_t sub = dst - static_cast<uint32_t>(op1);
        int64_t iSub = iDst - iSrc;
        //m_flags.AF = ?;
        m_flags.CF = dst < static_cast<uint32_t>(op1);
        m_flags.OF = (iSub < static_cast<int32_t>(0x80000000) || iSub > 0x7fFFffFF);
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (sub >> 31) & 1;
        m_flags.PF = (sub & 1) ^ 1;
    }

    inline void ScriptAsm::CMP(Reg32 op0, void* op1)
    {
        CMP(op0, *reinterpret_cast<int32_t*>(op1));
    }

    inline void ScriptAsm::CMP(Reg32 op0, Reg32 op1)
    {
        CMP(op0, op1.data);
    }

    inline void ScriptAsm::CMP(Reg16 op0, int16_t op1)
    {
        uint16_t dst = static_cast<uint16_t>(op0.data);
        int32_t iDst = static_cast<int16_t>(dst);
        int32_t iSrc = op1;
        uint16_t sub = dst - static_cast<uint16_t>(op1);
        int32_t iSub = iDst - iSrc;
        //m_flags.AF = ?;
        m_flags.CF = dst < static_cast<uint16_t>(op1);
        m_flags.OF = (iSub < static_cast<int16_t>(0x8000) || iSub > static_cast<int16_t>(0x7fFF));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (sub >> 15) & 1;
        m_flags.PF = (sub & 1) ^ 1;
    }

    inline void ScriptAsm::CMP(int8_t* op0, int8_t op1)
    {
        // todo AF
        uint8_t dst = static_cast<uint8_t>(*op0);
        int16_t iDst = static_cast<int8_t>(dst);
        int16_t iSrc = op1;
        uint8_t sub = dst - static_cast<uint8_t>(op1);
        int16_t iSub = iDst - iSrc;
        //m_flags.AF = ?;
        m_flags.CF = dst < static_cast<uint8_t>(op1);
        m_flags.OF = (iSub < static_cast<int8_t>(0x80)) || (iSub > static_cast<int8_t>(0x7f));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (sub >> 7) & 1;
        m_flags.PF = (sub & 1) ^ 1;
    }

    inline void ScriptAsm::CMP(int8_t* op0, Reg8 op1)
    {
        CMP(op0, op1.data);
    }

    inline void ScriptAsm::CMP(Reg8 op0, int8_t op1)
    {
        // todo AF
        uint8_t dst = static_cast<uint8_t>(op0.data);
        int16_t iDst = static_cast<int8_t>(dst);
        int16_t iSrc = op1;
        uint8_t sub = dst - static_cast<uint8_t>(op1);
        int16_t iSub = iDst - iSrc;
        //m_flags.AF = ?;
        m_flags.CF = dst < static_cast<uint8_t>(op1);
        m_flags.OF = (iSub < static_cast<int8_t>(0x80)) || (iSub > static_cast<int8_t>(0x7f));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (sub >> 7) & 1;
        m_flags.PF = (sub & 1) ^ 1;
    }

    inline void ScriptAsm::CMP(Reg8 op0, void* op1)
    {
        CMP(op0, *reinterpret_cast<int8_t*>(op1));
    }

    inline void ScriptAsm::DEC(Reg32 op0)
    {
        uint32_t data = static_cast<uint32_t>(op0.data);
        int64_t iData = static_cast<int32_t>(data) - 1;
        data--;
        //m_flags.AF = ?;
        m_flags.OF = (iData < static_cast<int32_t>(0x80000000)) || (iData > static_cast<int32_t>(0x7fFFffFF));
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 31) & 1;
        m_flags.PF = (data & 1) ^ 1;
        op0.data = static_cast<int32_t>(data);
    }

    inline void ScriptAsm::IMUL(Reg32 op0, int32_t op1)
    {
        int64_t data = op0.data;
        data *= op1;
        op0.data = static_cast<int32_t>(data);
        m_flags.CF = m_flags.OF = (static_cast<int32_t>(data) & 0x80000000) != ((data >> 32) & 0x80000000);
    }

    inline void ScriptAsm::IMUL(Reg32 op0, void* op1)
    {
        IMUL(op0, *reinterpret_cast<int32_t*>(op1));
    }

    inline void ScriptAsm::INC(Reg64 op0)
    {
        int64_t dst = op0.data;
        uint64_t data = static_cast<uint64_t>(dst);
        int128_t iData = dst;
        iData++;
        data++;
        //m_flags.AF = ?;
        m_flags.OF = (iData < int128_t(0xffFFffFFffFFffFF, 0x8000000000000000) || iData > int128_t(0, 0x7fFFffFFffFFffFF));
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 63) & 1;
        m_flags.PF = (data & 1) ^ 1;
        op0.data = static_cast<int64_t>(data);
    }

    inline void ScriptAsm::INC(int32_t* op0)
    {
        int32_t dst = *op0;
        uint32_t data = static_cast<uint32_t>(dst);
        int64_t iData = dst;
        iData++;
        data++;
        //m_flags.AF = ?;
        m_flags.OF = (iData < static_cast<int32_t>(0x80000000)) || (iData > static_cast<int32_t>(0x7fFFffFF));
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 31) & 1;
        m_flags.PF = (data & 1) ^ 1;
        *op0 = static_cast<int32_t>(data);
    }

    inline void ScriptAsm::INC(Reg32 op0)
    {
        int32_t dst = op0.data;
        uint32_t data = static_cast<uint32_t>(dst);
        int64_t iData = dst;
        iData++;
        data++;
        //m_flags.AF = ?;
        m_flags.OF = (iData < static_cast<int32_t>(0x80000000)) || (iData > static_cast<int32_t>(0x7fFFffFF));
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 31) & 1;
        m_flags.PF = (data & 1) ^ 1;
        op0.data = static_cast<uint32_t>(data);
    }

    inline void ScriptAsm::INC(int8_t* op0)
    {
        int8_t dst = *op0;
        uint8_t data = static_cast<uint8_t>(dst);
        int16_t iData = dst;
        iData++;
        data++;
        //m_flags.AF = ?;
        m_flags.OF = (iData < static_cast<int8_t>(0x80)) || (iData > static_cast<int8_t>(0x7f));
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 7) & 1;
        m_flags.PF = (data & 1) ^ 1;
        *op0 = static_cast<uint8_t>(data);
    }

    inline void ScriptAsm::INC(Reg8 op0)
    {
        int8_t dst = op0.data;
        uint8_t data = static_cast<uint8_t>(dst);
        int16_t iData = dst;
        iData++;
        data++;
        //m_flags.AF = ?;
        m_flags.OF = (iData < static_cast<int8_t>(0x80)) || (iData > static_cast<int8_t>(0x7f));
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 7) & 1;
        m_flags.PF = (data & 1) ^ 1;
        op0.data = static_cast<int8_t>(data);
    }

    inline void ScriptAsm::LODSB()
    {
        auto src = reinterpret_cast<int8_t*>(rsi.data);
        if (m_flags.DF)
        {
            rsi.data -= 1;
            al.data = *src;
        }
        else
        {
            rsi.data += 1;
            al.data = *src;
        }
    }

    inline void ScriptAsm::MOV(void* op0, Reg64 op1)
    {
        *reinterpret_cast<int64_t*>(op0) = op1.data;
    }

    inline void ScriptAsm::MOV(Reg64 op0, void* op1)
    {
        op0.data = *reinterpret_cast<int64_t*>(op1);
    }

    inline void ScriptAsm::MOV(Reg64 op0, Reg64 op1)
    {
        op0.data = op1.data;
    }

    inline void ScriptAsm::MOV(void* op0, Reg32 op1)
    {
        *reinterpret_cast<int32_t*>(op0) = op1.data;
    }

    inline void ScriptAsm::MOV(int32_t* op0, int32_t op1)
    {
        *op0 = op1;
    }

    inline void ScriptAsm::MOV(Reg32 op0, int32_t op1)
    {
        op0.data = op1;
    }

    inline void ScriptAsm::MOV(Reg32 op0, void* op1)
    {
        MOV(op0, *reinterpret_cast<int32_t*>(op1));
    }

    inline void ScriptAsm::MOV(Reg32 op0, Reg32 op1)
    {
        MOV(op0, op1.data);
    }

    inline void ScriptAsm::MOV(Reg16 op0, void* op1)
    {
        op0.data = *reinterpret_cast<int16_t*>(op1);
    }

    inline void ScriptAsm::MOV(int8_t* op0, int8_t op1)
    {
        *op0 = op1;
    }

    inline void ScriptAsm::MOV(Reg8 op0, int8_t op1)
    {
        op0.data = op1;
    }

    inline void ScriptAsm::MOV(Reg8 op0, void* op1)
    {
        op0.data = *reinterpret_cast<int8_t*>(op1);
    }

    inline void ScriptAsm::MOV(void* op0, Reg8 op1)
    {
        *reinterpret_cast<int8_t*>(op0) = op1.data;
    }

    inline void ScriptAsm::MOVZX(Reg32 op0, int8_t* op1)
    {
        reinterpret_cast<uint32_t&>(op0.data) = static_cast<uint8_t>(*op1);
    }

    inline void ScriptAsm::MUL(Reg32 op0)
    {
        uint64_t data = static_cast<uint64_t>(eax.data);
        data *= static_cast<uint32_t>(op0.data);
        eax.data = static_cast<int32_t>(data);
        edx.data = static_cast<int32_t>(data >> 32);
        m_flags.CF = m_flags.OF = edx.data != 0;
    }

    inline void ScriptAsm::NEG(Reg32 op0)
    {
        m_flags.CF = op0.data != 0;
        op0.data = -op0.data;
    }

    inline void ScriptAsm::NOT(Reg32 op0)
    {
        op0.data = ~op0.data;
    }

    inline void ScriptAsm::OR(Reg32 op0, int32_t op1)
    {
        auto data = op0.data | op1;
        m_flags.CF = m_flags.OF = 0;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 31) & 1;
        m_flags.PF = (data & 1) ^ 1;
        op0.data = data;
    }

    inline void ScriptAsm::OR(Reg32 op0, Reg32 op1)
    {
        OR(op0, op1.data);
    }

    inline void ScriptAsm::OR(Reg8 op0, int8_t op1)
    {
        int8_t data = op0.data | op1;
        m_flags.CF = m_flags.OF = 0;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 7) & 1;
        m_flags.PF = (data & 1) ^ 1;
        op0.data = data;
    }

    inline void ScriptAsm::OR(Reg8 op0, Reg8 op1)
    {
        OR(op0, op1.data);
    }

    inline void ScriptAsm::POP(Reg32 op0)
    {
        auto data = reinterpret_cast<int32_t*>(rsp.data);
        auto dataHigh = reinterpret_cast<int32_t*>(rsp.data + 32768);
        op0.data = *data++;
        *(&op0.data + 1) = *dataHigh++;
        rsp.data += 4;
    }

    inline void ScriptAsm::POPAD()
    {
        auto data = reinterpret_cast<int32_t*>(rsp.data);
        auto dataHigh = reinterpret_cast<int32_t*>(rsp.data + 32768);
        edi.data = *data++;
        esi.data = *data++;
        ebp.data = *data++;
        *data++;
        edx.data = *data++;
        ecx.data = *data++;
        ebx.data = *data++;
        eax.data = *data++;
        *(&edi.data + 1) = *dataHigh++;
        *(&esi.data + 1) = *dataHigh++;
        *(&ebp.data + 1) = *dataHigh++;
        *dataHigh++;
        *(&edx.data + 1) = *dataHigh++;
        *(&ecx.data + 1) = *dataHigh++;
        *(&ebx.data + 1) = *dataHigh++;
        *(&eax.data + 1) = *dataHigh++;
        rsp.data += 32;
    }

    inline void ScriptAsm::PUSH(Reg32 op0)
    {
        auto data = reinterpret_cast<int32_t*>(rsp.data);
        auto dataHigh = reinterpret_cast<int32_t*>(rsp.data + 32768);
        *--data = op0.data;
        *--dataHigh = *(&op0.data + 1);
        rsp.data -= 4;
    }

    inline void ScriptAsm::PUSHAD()
    {
        auto data = reinterpret_cast<int32_t*>(rsp.data);
        auto dataHigh = reinterpret_cast<int32_t*>(rsp.data + 32768);
        *--data = eax.data;
        *--data = ebx.data;
        *--data = ecx.data;
        *--data = edx.data;
        *--data = esp.data;
        *--data = ebp.data;
        *--data = esi.data;
        *--data = edi.data;
        *--dataHigh = *(&eax.data + 1);
        *--dataHigh = *(&ebx.data + 1);
        *--dataHigh = *(&ecx.data + 1);
        *--dataHigh = *(&edx.data + 1);
        *--dataHigh = *(&esp.data + 1);
        *--dataHigh = *(&ebp.data + 1);
        *--dataHigh = *(&esi.data + 1);
        *--dataHigh = *(&edi.data + 1);
        rsp.data -= 32;
    }

    inline void ScriptAsm::RCL(Reg32 op0, int8_t op1)
    {
        uint64_t data = static_cast<uint32_t>(op0.data);
        data = (data << 32) | (static_cast<uint64_t>(m_flags.CF) << 31) | (data >> 1);
        data <<= op1;
        auto res = static_cast<int32_t>(data >> 32);
        op0.data = res;
        m_flags.CF = (data >> 31) & 1;
        if (op1 == 1)
            m_flags.OF = ((res >> 31) & 1) ^ m_flags.CF;
    }

    inline void ScriptAsm::RCR(Reg32 op0, int8_t op1)
    {
        uint64_t data = static_cast<uint32_t>(op0.data);
        data = data | (static_cast<uint64_t>(m_flags.CF) << 32) | (data << 33);
        data >>= op1;
        auto res = static_cast<int32_t>(data);
        op0.data = res;
        m_flags.CF = (data >> 32) & 1;
        if (op1 == 1)
            m_flags.OF = ((res >> 31) & 1) ^ ((res >> 30) & 1);
    }

/*
    inline void ScriptAsm::RDTSC()
    {
        auto rdtsc = __rdtsc();
        eax.data = static_cast<int32_t>(rdtsc);
        edx.data = static_cast<int32_t>(rdtsc >> 32);
    }
*/

    inline void ScriptAsm::REP_MOVSD()
    {
        auto src = reinterpret_cast<int32_t*>(rsi.data);
        auto dst = reinterpret_cast<int32_t*>(rdi.data);
        auto n = ecx.data;
        if (m_flags.DF)
        {
            rsi.data -= n * 4;
            rdi.data -= n * 4;
            for (; n; --n)
                *dst-- = *src--;
        }
        else
        {
            rsi.data += n * 4;
            rdi.data += n * 4;
            for (; n; --n)
                *dst++ = *src++;
        }
        ecx.data = 0;
    }

    inline void ScriptAsm::REP_STOSB()
    {
        auto dst = reinterpret_cast<int8_t*>(rdi.data);
        auto data = al.data;
        auto n = ecx.data;
        if (m_flags.DF)
        {
            rdi.data -= n;
            for (; n; --n)
                *dst-- = data;
        }
        else
        {
            rdi.data += n;
            for (; n; --n)
                *dst++ = data;
        }
        ecx.data = 0;
    }

    inline void ScriptAsm::REP_STOSD()
    {
        auto dst = reinterpret_cast<int32_t*>(rdi.data);
        auto data = eax.data;
        auto n = ecx.data;
        if (m_flags.DF)
        {
            rdi.data -= n * 4;
            for (; n; --n)
                *dst-- = data;
        }
        else
        {
            rdi.data += n * 4;
            for (; n; --n)
                *dst++ = data;
        }
        ecx.data = 0;
    }

    inline void ScriptAsm::SAR(Reg32 op0, int8_t op1)
    {
        int64_t v = (static_cast<int64_t>(op0.data) << 32) >> op1;
        int32_t data = static_cast<int32_t>(v >> 32);
        op0.data = data;
        m_flags.CF = (v >> 31) & 1;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 31) & 1;
        m_flags.PF = (data & 1) ^ 1;
        if (op1 == 1)
            m_flags.OF = 0;
    }

    inline void ScriptAsm::SBB(Reg32 op0, Reg32 op1)
    {
        // todo AF
        int32_t src = op1.data;
        uint32_t dst = static_cast<uint32_t>(op0.data);
        int64_t iDst = dst;
        int64_t iSrc = src + m_flags.CF;
        uint32_t sub = dst - (static_cast<uint32_t>(src) + m_flags.CF);
        int64_t iSub = iDst - iSrc;
        //m_flags.AF = ?;
        m_flags.CF = dst < (static_cast<uint64_t>(src) + m_flags.CF);
        m_flags.OF = (iSub < static_cast<int32_t>(0x80000000)) || (iSub > static_cast<int32_t>(0x7fFFffFF));
        m_flags.ZF = sub == 0;
        m_flags.SF = (sub >> 31) & 1;
        m_flags.PF = (sub & 1) ^ 1;
        op0.data = static_cast<int32_t>(sub);
    }

    inline void ScriptAsm::SHL(int32_t* op0, int8_t op1)
    {
        uint64_t v = static_cast<uint64_t>(*op0) << op1;
        *op0 = static_cast<int32_t>(v);
        m_flags.CF = (v >> 32) & 1;
        m_flags.ZF = *op0 == 0;
        m_flags.SF = (v >> 31) & 1;
        m_flags.PF = (v & 1) ^ 1;
        if (op1 == 1)
            m_flags.OF = m_flags.CF ^ m_flags.SF;
    }

    inline void ScriptAsm::SHL(Reg32 op0, int8_t op1)
    {
        uint64_t v = static_cast<uint64_t>(op0.data) << op1;
        int32_t data = static_cast<int32_t>(v);
        op0.data = data;
        m_flags.CF = (v >> 32) & 1;
        m_flags.ZF = data == 0;
        m_flags.SF = (v >> 31) & 1;
        m_flags.PF = (v & 1) ^ 1;
        if (op1 == 1)
            m_flags.OF = m_flags.CF ^ m_flags.SF;
    }

    inline void ScriptAsm::SHL(Reg16 op0, int8_t op1)
    {
        uint32_t v = static_cast<uint32_t>(op0.data) << op1;
        int16_t data = static_cast<int16_t>(v);
        op0.data = data;
        m_flags.CF = (v >> 16) & 1;
        m_flags.ZF = data == 0;
        m_flags.SF = (v >> 15) & 1;
        m_flags.PF = (v & 1) ^ 1;
        if (op1 == 1)
            m_flags.OF = m_flags.CF ^ m_flags.SF;
    }

    inline void ScriptAsm::SHR(Reg32 op0, int8_t op1)
    {
        uint64_t v = static_cast<uint32_t>(op0.data);
        v = (v << 32) >> op1;
        int32_t data = static_cast<int32_t>(v >> 32);
        op0.data = data;
        m_flags.CF = (v >> 31) & 1;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 31) & 1;
        m_flags.PF = (data & 1) ^ 1;
        if (op1 == 1)
            m_flags.OF = m_flags.SF;
    }

    inline void ScriptAsm::STOSD()
    {
        auto dst = reinterpret_cast<int32_t*>(rdi.data);
        if (m_flags.DF)
        {
            rdi.data -= 4;
            *dst = eax.data;
        }
        else
        {
            rdi.data += 4;
            *dst = eax.data;
        }
    }

    inline void ScriptAsm::SUB(Reg32 op0, Reg32 op1)
    {
        // todo AF
        uint32_t src = static_cast<uint32_t>(op1.data);
        uint32_t dst = static_cast<uint32_t>(op0.data);
        int64_t iDst = static_cast<int32_t>(dst);
        int64_t iSrc = op1.data;
        uint32_t sub = dst - src;
        int64_t iSub = iDst - iSrc;
        op0.data = static_cast<int32_t>(sub);
        //m_flags.AF = ?;
        m_flags.CF = dst < src;
        m_flags.OF = (iSub < static_cast<int32_t>(0x80000000)) || (iSub > static_cast<int32_t>(0x7fFFffFF));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (sub >> 31) & 1;
        m_flags.PF = (sub & 1) ^ 1;
    }

    inline void ScriptAsm::SUB(Reg8 op0, int8_t op1)
    {
        // todo AF
        uint8_t dst = static_cast<uint8_t>(op0.data);
        int16_t iDst = static_cast<int8_t>(dst);
        int16_t iSrc = op1;
        uint8_t sub = dst - static_cast<uint8_t>(op1);
        int16_t iSub = iDst - iSrc;
        op0.data = sub;
        //m_flags.AF = ?;
        m_flags.CF = dst < static_cast<uint8_t>(op1);
        m_flags.OF = (iSub < static_cast<int8_t>(0x80)) || (iSub > static_cast<int8_t>(0x7f));
        m_flags.ZF = iSrc == iDst;
        m_flags.SF = (sub >> 7) & 1;
        m_flags.PF = (sub & 1) ^ 1;
    }

    inline void ScriptAsm::TEST(int8_t* op0, uint8_t op1)
    {
        uint8_t data = static_cast<uint8_t>(*op0) & op1;
        m_flags.CF = m_flags.OF = 0;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 7) & 1;
        m_flags.PF = (data & 1) ^ 1;
    }

    inline void ScriptAsm::TEST(Reg8 op0, uint8_t op1)
    {
        uint8_t data = static_cast<uint8_t>(op0.data) & op1;
        m_flags.CF = m_flags.OF = 0;
        m_flags.ZF = data == 0;
        m_flags.SF = (data >> 7) & 1;
        m_flags.PF = (data & 1) ^ 1;
    }

    inline void ScriptAsm::XCHG(Reg32 op0, Reg32 op1)
    {
        std::swap(op0.data, op1.data);
    }

    inline void ScriptAsm::XOR(Reg32 op0, int32_t op1)
    {
        int32_t v = op0.data ^ op1;
        op0.data = v;
        m_flags.SF = (v >> 31) & 1;
        m_flags.ZF = v == 0;
        m_flags.PF = (v & 1) ^ 1;
        m_flags.CF = m_flags.OF = 0;
    }

    inline void ScriptAsm::XOR(Reg32 op0, void* op1)
    {
        XOR(op0, *reinterpret_cast<int32_t*>(op1));
    }

    inline void ScriptAsm::XOR(Reg32 op0, Reg32 op1)
    {
        XOR(op0, op1.data);
    }

    inline void ScriptAsm::F2XM1()
    {
        m_st[m_stPos] = pow(2.0, m_st[m_stPos]) - 1.0;
    }

    inline void ScriptAsm::FABS()
    {
        reinterpret_cast<uint64_t&>(m_st[m_stPos]) &= 0x7fFFffFFffFFffFF;
        m_sw.C1 = 0;
    }

    inline void ScriptAsm::FADD(int32_t* op0)
    {
        auto dst = m_stPos;
        m_st[dst] += *reinterpret_cast<float*>(op0);
    }

    inline void ScriptAsm::FADD(int64_t* op0)
    {
        auto dst = m_stPos;
        m_st[dst] += *reinterpret_cast<double*>(op0);
    }

    inline void ScriptAsm::FADD(FloatId op0)
    {
        auto dst = m_stPos;
        auto src = m_stPos + op0;
        m_st[dst] += m_st[src];
    }

    inline void ScriptAsm::FADD(FloatId op0, FloatId op1)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos + op1;
        m_st[dst] += m_st[src];
    }

    inline void ScriptAsm::FADDP(FloatId op0, FloatId /*op1*/)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos++;
        m_st[dst] += m_st[src];
    }

    inline void ScriptAsm::FCHS()
    {
        reinterpret_cast<uint64_t&>(m_st[m_stPos]) ^= 0x8000000000000000;
    }

    inline void ScriptAsm::FCLEX()
    {
        m_sw.PE = m_sw.UE = m_sw.OE = m_sw.ZE = m_sw.DE = m_sw.IE = 0;
    }

    inline void ScriptAsm::FCMOVB(FloatId /*op0*/, FloatId op1)
    {
        auto dst = m_stPos;
        auto src = m_stPos + op1;
        if (m_flags.CF)
            m_st[dst] = m_st[src];
    }

    inline void ScriptAsm::FCMOVNB(FloatId /*op0*/, FloatId op1)
    {
        auto dst = m_stPos;
        auto src = m_stPos + op1;
        if (!m_flags.CF)
            m_st[dst] = m_st[src];
    }

    inline void ScriptAsm::FCOMI(FloatId /*op0*/, FloatId op1)
    {
        auto dst = m_st[m_stPos];
        auto src = m_st[m_stPos + op1];
        m_flags.PF = 0;
        // todo NAN or invalid value
        if (dst == src)
        {
            m_flags.ZF = 1;
            m_flags.CF = 0;
        }
        else
        {
            m_flags.ZF = 0;
            m_flags.CF = dst < src;
        }
    }

    inline void ScriptAsm::FDIV(int32_t* op0)
    {
        auto dst = m_stPos;
        m_st[dst] /= *reinterpret_cast<float*>(op0);
    }

    inline void ScriptAsm::FDIV(FloatId op0, FloatId op1)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos + op1;
        m_st[dst] /= m_st[src];
    }

    inline void ScriptAsm::FDIVP(FloatId op0, FloatId /*op1*/)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos++;
        m_st[dst] /= m_st[src];
    }

    inline void ScriptAsm::FDIVR(FloatId op0, FloatId op1)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos + op1;
        m_st[dst] = m_st[src] / m_st[dst];
    }

    inline void ScriptAsm::FDIVRP(FloatId op0, FloatId /*op1*/)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos++;
        m_st[dst] = m_st[src] / m_st[dst];
    }

    inline void ScriptAsm::FIADD(int32_t* op0)
    {
        m_st[m_stPos] += *op0;
    }

    inline void ScriptAsm::FIDIV(int32_t* op0)
    {
        m_st[m_stPos] /= *op0;
    }

    inline void ScriptAsm::FILD(int32_t* op0)
    {
        m_st[--m_stPos] = *op0;
    }

    inline void ScriptAsm::FIST(int32_t* op0)
    {
        *op0 = static_cast<int32_t>(m_st[m_stPos]);
    }

    inline void ScriptAsm::FISTP(int32_t* op0)
    {
        *op0 = static_cast<int32_t>(m_st[m_stPos++]);
    }

    inline void ScriptAsm::FLD(int64_t* op0)
    {
        m_st[--m_stPos] = *reinterpret_cast<double*>(op0);
    }

    inline void ScriptAsm::FLD(int32_t* op0)
    {
        m_st[--m_stPos] = *reinterpret_cast<float*>(op0);
    }

    inline void ScriptAsm::FLD(FloatId op0)
    {
        auto src = m_stPos + op0;
        auto value = m_st[src];
        m_st[--m_stPos] = value;
    }

    inline void ScriptAsm::FLD1()
    {
        m_st[--m_stPos] = 1.0;
    }

    inline void ScriptAsm::FLDPI()
    {
        m_st[--m_stPos] = std::numbers::pi;
    }

    inline void ScriptAsm::FLDZ()
    {
        m_st[--m_stPos] = 0.0;
    }

    inline void ScriptAsm::FMUL(int32_t* op0)
    {
        auto dst = m_stPos;
        m_st[dst] *= *reinterpret_cast<float*>(op0);
    }

    inline void ScriptAsm::FMUL(int64_t* op0)
    {
        auto dst = m_stPos;
        m_st[dst] *= *reinterpret_cast<double*>(op0);
    }

    inline void ScriptAsm::FMUL(FloatId op0)
    {
        auto dst = m_stPos;
        auto src = m_stPos + op0;
        m_st[dst] *= m_st[src];
    }

    inline void ScriptAsm::FMUL(FloatId op0, FloatId op1)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos + op1;
        m_st[dst] *= m_st[src];
    }

    inline void ScriptAsm::FMULP(FloatId op0, FloatId /*op1*/)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos++;
        m_st[dst] *= m_st[src];
    }

    inline void ScriptAsm::FPATAN()
    {
        m_st[m_stPos + 1] = atan2(m_st[m_stPos + 1], m_st[m_stPos]);
        m_stPos++;
    }

    inline void ScriptAsm::FPREM()
    {
        m_st[m_stPos] = fmod(m_st[m_stPos], m_st[m_stPos + 1]);
    }

    inline void ScriptAsm::FSCALE()
    {
        m_st[m_stPos] *= pow(2.0, std::trunc(m_st[m_stPos + 1]));
    }

    inline void ScriptAsm::FSINCOS()
    {
        --m_stPos;
        m_st[m_stPos] = cos(m_st[m_stPos + 1]);
        m_st[m_stPos + 1] = sin(m_st[m_stPos + 1]);
    }

    inline void ScriptAsm::FSQRT()
    {
        m_st[m_stPos] = sqrt(m_st[m_stPos]);
    }

    inline void ScriptAsm::FST(int32_t* op0)
    {
        *reinterpret_cast<float*>(op0) = static_cast<float>(m_st[m_stPos]);
    }

    inline void ScriptAsm::FSTP(int32_t* op0)
    {
        *reinterpret_cast<float*>(op0) = static_cast<float>(m_st[m_stPos++]);
    }

    inline void ScriptAsm::FSTP(FloatId op0)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos++;
        m_st[dst] = m_st[src];
    }

    inline void ScriptAsm::FSTSW(Reg16 op0)
    {
        op0.data = *reinterpret_cast<int16_t*>(&m_sw);
    }

    inline void ScriptAsm::FSUB(int32_t* op0)
    {
        m_st[m_stPos] -= *reinterpret_cast<float*>(op0);
    }

    inline void ScriptAsm::FSUB(FloatId op0, FloatId op1)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos + op1;
        m_st[dst] -= m_st[src];
    }

    inline void ScriptAsm::FSUBP(FloatId op0, FloatId /*op1*/)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos++;
        m_st[dst] -= m_st[src];
    }

    inline void ScriptAsm::FSUBR(int32_t* op0)
    {
        m_st[m_stPos] = *reinterpret_cast<float*>(op0) - m_st[m_stPos];
    }

    inline void ScriptAsm::FSUBR(FloatId op0, FloatId op1)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos + op1;
        m_st[dst] = m_st[src] - m_st[dst];
    }

    inline void ScriptAsm::FSUBRP(FloatId op0, FloatId /*op1*/)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos++;
        m_st[dst] = m_st[src] - m_st[dst];
    }

    inline void ScriptAsm::FXCH(FloatId op0)
    {
        auto dst = m_stPos + op0;
        auto src = m_stPos;
        std::swap(m_st[dst], m_st[src]);
    }

    inline void ScriptAsm::FYL2X()
    {
        m_st[m_stPos + 1] *= log2(m_st[m_stPos]);
        m_stPos++;
    }
}
//namespace V2