#pragma once

#include <stdint.h>

namespace V2
{
    class ScriptAsm
    {
        struct Reg32;
        struct Reg64
        {
            int64_t& data;

            void* operator[](int64_t offset) { return reinterpret_cast<void*>(data + offset); }
        };
        struct Reg32
        {
            int32_t& data;
        };
        struct Reg16
        {
            int16_t& data;
        };
        struct Reg8
        {
            int8_t& data;
        };
    public:
        enum FloatId
        {
            st = 0, st0 = 0, st1, st2, st3, st4, st5, st6, st7
        };

    public:
        ScriptAsm();

        void ADD(Reg64 op0, int64_t op1);
        void ADD(Reg64 op0, int32_t* op1);
        void ADD(Reg32 op0, int32_t op1);
        void ADD(void* op0, Reg32 op1);
        void ADD(Reg32 op0, void* op1);
        void ADD(Reg32 op0, Reg32 op1);
        void AND(Reg8 op0, int8_t op1);
        void AND(Reg32 op0, int32_t op1);
        void AND(Reg32 op0, void* op1);
        void CMOVC(Reg32 op0, void* op1);
        void CMOVE(Reg32 op0, Reg32 op1);
        void CMOVGE(Reg32 op0, Reg32 op1);
        void CMOVLE(Reg32 op0, Reg32 op1);
        void CMP(Reg64 op0, Reg64 op1);
        void CMP(Reg32 op0, int32_t op1);
        void CMP(Reg32 op0, void* op1);
        void CMP(Reg32 op0, Reg32 op1);
        void CMP(Reg16 op0, int16_t op1);
        void CMP(int8_t* op0, int8_t op1);
        void CMP(int8_t* op0, Reg8 op1);
        void CMP(Reg8 op0, int8_t op1);
        void CMP(Reg8 op0, void* op1);
        void DEC(Reg32 op0);
        void IMUL(Reg32 op0, int32_t op1);
        void IMUL(Reg32 op0, void* op1);
        void INC(Reg64 op0);
        void INC(int32_t* op0);
        void INC(Reg32 op0);
        void INC(int8_t* op0);
        void INC(Reg8 op0);
        bool JA() { return m_flags.CF == 0 && m_flags.ZF == 0; }
        bool JAE() { return m_flags.CF == 0; }
        bool JB() { return m_flags.CF == 1; }
        bool JBE() { return m_flags.CF == 1 || m_flags.ZF == 1; }
        bool JE() { return m_flags.ZF == 1; }
        bool JGE() { return m_flags.SF == m_flags.OF; }
        bool JNC() { return m_flags.CF == 0; }
        bool JNE() { return m_flags.ZF == 0; }
        bool JNS() { return m_flags.SF == 0; }
        bool JNZ() { return m_flags.ZF == 0; }
        bool JS() { return m_flags.SF == 1; }
        bool JZ() { return m_flags.ZF == 1; }
        void LODSB();
        void MOV(void* op0, Reg64 op1);
        void MOV(Reg64 op0, void* op1);
        void MOV(Reg64 op0, Reg64 op1);
        void MOV(void* op0, Reg32 op1);
        void MOV(int32_t* op0, int32_t op1);
        void MOV(Reg32 op0, int32_t op1);
        void MOV(Reg32 op0, void* op1);
        void MOV(Reg32 op0, Reg32 op1);
        void MOV(Reg16 op0, void* op1);
        void MOV(int8_t* op0, int8_t op1);
        void MOV(Reg8 op0, int8_t op1);
        void MOV(Reg8 op0, void* op1);
        void MOV(void* op0, Reg8 op1);
        void MOVZX(Reg32 op0, int8_t* op1);
        void MUL(Reg32 op0);
        void NEG(Reg32 op0);
        void NOT(Reg32 op0);
        void OR(Reg32 op0, int32_t op1);
        void OR(Reg32 op0, Reg32 op1);
        void OR(Reg8 op0, int8_t op1);
        void OR(Reg8 op0, Reg8 op1);
        void POP(Reg32 op0);
        void POPAD();
        void PUSH(Reg32 op0);
        void PUSHAD();
        void RCL(Reg32 op0, int8_t op1);
        void RCR(Reg32 op0, int8_t op1);
        void RDTSC();
        void REP_MOVSD();
        void REP_STOSB();
        void REP_STOSD();
        void SAR(Reg32 op0, int8_t op1);
        void SBB(Reg32 op0, Reg32 op1);
        void SHL(int32_t* op0, int8_t op1);
        void SHL(Reg32 op0, int8_t op1);
        void SHL(Reg16 op0, int8_t op1);
        void SHR(Reg32 op0, int8_t op1);
        void STOSD();
        void SUB(Reg32 op0, Reg32 op1);
        void SUB(Reg8 op0, int8_t op1);
        void TEST(int8_t* op0, uint8_t op1);
        void TEST(Reg8 op0, uint8_t op1);
        void XCHG(Reg32 op0, Reg32 op1);
        void XOR(Reg32 op0, int32_t op1);
        void XOR(Reg32 op0, void* op1);
        void XOR(Reg32 op0, Reg32 op1);

        void F2XM1();
        void FABS();
        void FADD(int32_t* op0);
        void FADD(int64_t* op0);
        void FADD(FloatId op0);
        void FADD(FloatId op0, FloatId op1);
        void FADDP(FloatId op0, FloatId op1);
        void FCHS();
        void FCLEX();
        void FCMOVB(FloatId op0, FloatId op1);
        void FCMOVNB(FloatId op0, FloatId op1);
        void FCOMI(FloatId op0, FloatId op1);
        void FDIV(int32_t* op0);
        void FDIV(FloatId op0, FloatId op1);
        void FDIVP(FloatId op0, FloatId op1);
        void FDIVR(FloatId op0, FloatId op1);
        void FDIVRP(FloatId op0, FloatId op1);
        void FIADD(int32_t* op0);
        void FIDIV(int32_t* op0);
        void FILD(int32_t* op0);
        void FIST(int32_t* op0);
        void FISTP(int32_t* op0);
        void FLD(int64_t* op0);
        void FLD(int32_t* op0);
        void FLD(FloatId op0);
        void FLD1();
        void FLDPI();
        void FLDZ();
        void FMUL(int32_t* op0);
        void FMUL(int64_t* op0);
        void FMUL(FloatId op0);
        void FMUL(FloatId op0, FloatId op1);
        void FMULP(FloatId op0, FloatId op1);
        void FPATAN();
        void FPREM();
        void FSCALE();
        void FSINCOS();
        void FSQRT();
        void FST(int32_t* op0);
        void FSTP(int32_t* op0);
        void FSTP(FloatId op0);
        void FSTSW(Reg16 op0);
        void FSUB(int32_t* op0);
        void FSUB(FloatId op0, FloatId op1);
        void FSUBP(FloatId op0, FloatId op1);
        void FSUBR(FloatId op0, FloatId op1);
        void FSUBR(int32_t* op0);
        void FSUBRP(FloatId op0, FloatId op1);
        void FXCH(FloatId op0);
        void FYL2X();

        int8_t* byte(void* op0) const { return reinterpret_cast<int8_t*>(op0); }
        int32_t* dword(float& op0) const { return reinterpret_cast<int32_t*>(&op0); }
        int32_t* dword(void* op0) const { return reinterpret_cast<int32_t*>(op0); }
        int32_t* dword(uint32_t& op0) const { return reinterpret_cast<int32_t*>(&op0); }
        int64_t* qword(void* op0) const { return reinterpret_cast<int64_t*>(op0); }
        int64_t* qword(double& op0) const { return reinterpret_cast<int64_t*>(&op0); }

        Reg64 rax{ m_regs[0].reg64 };
        Reg64 rbx{ m_regs[1].reg64 };
        Reg64 rcx{ m_regs[2].reg64 };
        Reg64 rdx{ m_regs[3].reg64 };
        Reg64 rsp{ m_regs[4].reg64 };
        Reg64 rbp{ m_regs[5].reg64 };
        Reg64 rsi{ m_regs[6].reg64 };
        Reg64 rdi{ m_regs[7].reg64 };
        Reg32 eax{ m_regs[0].reg32 };
        Reg32 ebx{ m_regs[1].reg32 };
        Reg32 ecx{ m_regs[2].reg32 };
        Reg32 edx{ m_regs[3].reg32 };
        Reg32 esp{ m_regs[4].reg32 };
        Reg32 ebp{ m_regs[5].reg32 };
        Reg32 esi{ m_regs[6].reg32 };
        Reg32 edi{ m_regs[7].reg32 };
        Reg16 ax{ m_regs[0].reg16 };
        Reg16 bx{ m_regs[1].reg16 };
        Reg16 cx{ m_regs[2].reg16 };
        Reg16 dx{ m_regs[3].reg16 };
        Reg8 al{ m_regs[0].reg8[0] };
        Reg8 bl{ m_regs[1].reg8[0] };
        Reg8 cl{ m_regs[2].reg8[0] };
        Reg8 dl{ m_regs[3].reg8[0] };
        Reg8 ah{ m_regs[0].reg8[1] };
        Reg8 bh{ m_regs[1].reg8[1] };
        Reg8 ch{ m_regs[2].reg8[1] };
        Reg8 dh{ m_regs[3].reg8[1] };

    private:
        union Registers
        {
            int64_t reg64;
            int32_t reg32;
            int16_t reg16;
            int8_t reg8[2];
        };

    private:
        Registers m_regs[8];

        struct Flags
        {
            uint16_t CF : 1, : 1;
            uint16_t PF : 1, : 1;
            uint16_t AF : 1, : 1;
            uint16_t ZF : 1;
            uint16_t SF : 1;
            uint16_t TF : 1;
            uint16_t IF : 1;
            uint16_t DF : 1;
            uint16_t OF : 1;
            uint16_t IOPL : 2;
            uint16_t NT : 1, : 1;

        } m_flags{};
        struct StatusWord
        {
            uint16_t IE : 1;
            uint16_t DE : 1;
            uint16_t ZE : 1;
            uint16_t OE : 1;
            uint16_t UE : 1;
            uint16_t PE : 1;
            uint16_t SF : 1;
            uint16_t ES : 1;
            uint16_t C0 : 1;
            uint16_t C1 : 1;
            uint16_t C2 : 1;
            uint16_t TOS : 3;
            uint16_t C3 : 1;
            uint16_t B : 1;
        } m_sw{};
        struct ControlWord
        {
            uint16_t IM : 1;
            uint16_t DM : 1;
            uint16_t ZM : 1;
            uint16_t OM : 1;
            uint16_t UM : 1;
            uint16_t PM : 1, : 1, : 1;
            uint16_t PC : 2;
            uint16_t RC : 2, : 4;
        } m_cw{};

        double m_st[10]{ 0.0 };
        int32_t m_stPos{ 8 };

        uint64_t m_beginMem;
        uint64_t m_endMem;

        uint8_t m_stack[32768 * 2];
    };
}
//namespace V2

#include "scriptasm.inl.h"