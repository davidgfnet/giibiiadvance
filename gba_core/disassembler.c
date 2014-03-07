/*
    GiiBiiAdvance - GBA/GB  emulator
    Copyright (C) 2011-2014 Antonio Ni�o D�az (AntonioND)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>

#include "../build_options.h"
#include "../font_utils.h"

#include "gba.h"
#include "cpu.h"
#include "shifts.h"
#include "memory.h"

//----------------------------------------------------------------------------------

#define GBA_MAX_BREAKPOINTS 20

static u32 gba_brkpoint_addrlist[GBA_MAX_BREAKPOINTS];
static int gba_brkpoint_used[GBA_MAX_BREAKPOINTS];
static int gba_any_breakpoint_used = 0;

int GBA_DebugIsBreakpoint(u32 addr)
{
    if(gba_any_breakpoint_used == 0) return 0;

    int i;
    for(i = 0; i < GBA_MAX_BREAKPOINTS; i++)
    {
        if(gba_brkpoint_used[i])
        {
            if(gba_brkpoint_addrlist[i] == addr)
            {
                return 1;
            }
        }
    }
    return 0;
}

static u32 gba_last_executed_opcode = 1;

int GBA_DebugCPUIsBreakpoint(u32 addr)
{
    if(gba_any_breakpoint_used == 0) return 0;

    if(gba_last_executed_opcode == addr)
    {
        gba_last_executed_opcode = 1;
        return 0;
    }

    int i;
    for(i = 0; i < GBA_MAX_BREAKPOINTS; i++)
    {
        if(gba_brkpoint_used[i])
        {
            if(gba_brkpoint_addrlist[i] == addr)
            {
                gba_last_executed_opcode = addr;
                return 1;
            }
        }
    }
    return 0;
}

void GBA_DebugAddBreakpoint(u32 addr)
{
    if(GBA_DebugIsBreakpoint(addr))
        return;

    int i;
    for(i = 0; i < GBA_MAX_BREAKPOINTS; i++)
    {
        if(gba_brkpoint_used[i] == 0)
        {
            gba_brkpoint_addrlist[i] = addr;
            gba_brkpoint_used[i] = 1;
            gba_any_breakpoint_used = 1;
            return;
        }
    }
}

void GBA_DebugClearBreakpoint(u32 addr)
{
    int i;
    for(i = 0; i < GBA_MAX_BREAKPOINTS; i++)
    {
        if(gba_brkpoint_used[i])
        {
            if(gba_brkpoint_addrlist[i] == addr)
            {
                gba_brkpoint_used[i] = 0;
                break;
            }
        }
    }

    for(i = 0; i < GBA_MAX_BREAKPOINTS; i++)
    {
        if(gba_brkpoint_used[i])
        {
            return;
        }
    }

    gba_any_breakpoint_used = 0;
}

void GBA_DebugClearBreakpointAll(void)
{
    int i;
    for(i = 0; i < GBA_MAX_BREAKPOINTS; i++)
    {
        gba_brkpoint_used[i] = 0;
    }

    gba_any_breakpoint_used = 0;
}

//----------------------------------------------------------------------------------

inline u32 arm_check_condition(u32 cond); // in arm.c

//returns 1 if there is a ';' in the line (previous to this or written by this)
static inline int gba_dissasemble_add_condition_met(int cond, u32 address, char * dest, int add_comment)
{
    if(CPU.R[R_PC] == address)
    {
        if(cond == 14) return 0; // always

        if(add_comment)
        {
            if(arm_check_condition(cond)) strcat(dest," ; true");
            else strcat(dest," ; false");
        }
        else
        {
            if(arm_check_condition(cond)) strcat(dest," - true");
            else strcat(dest," - false");
        }
        return 1;
    }
    return 0;
}

//----------------------------------------------------------------------------------

static struct {
    char * name;
    u32 address;
} gba_io_reg_struct[] = {
    //LCD I/O Registers
    {"DISPCNT",  0x04000000}, {"GREENSWAP", 0x04000002}, {"DISPSTAT", 0x04000004}, {"VCOUNT",  0x04000006},
    {"BG0CNT",   0x04000008}, {"BG1CNT",    0x0400000A}, {"BG2CNT",   0x0400000C}, {"BG3CNT",  0x0400000E},
    {"BG0HOFS",  0x04000010}, {"BG0VOFS",   0x04000012}, {"BG1HOFS",  0x04000014}, {"BG1VOFS", 0x04000016},
    {"BG2HOFS",  0x04000018}, {"BG2VOFS",   0x0400001A}, {"BG3HOFS",  0x0400001C}, {"BG3VOFS", 0x0400001E},
    {"BG2PA",    0x04000020}, {"BG2PB",     0x04000022}, {"BG2PC",    0x04000024}, {"BG2PD",   0x04000026},
    {"BG2X_L",   0x04000028}, {"BG2X_H",    0x0400002A}, {"BG2Y_L",   0x0400002C}, {"BG2Y_H",  0x0400002E},
    {"BG3PA",    0x04000030}, {"BG3PB",     0x04000032}, {"BG3PC",    0x04000034}, {"BG3PD",   0x04000036},
    {"BG3X_L",   0x04000038}, {"BG3X_H",    0x0400003A}, {"BG3Y_L",   0x0400003C}, {"BG3Y_H",  0x0400003E},
    {"WIN0H",    0x04000040}, {"WIN1H",     0x04000042}, {"WIN0V",    0x04000044}, {"WIN1V",   0x04000046},
    {"WININ",    0x04000048}, {"WINOUT",    0x0400004A}, {"MOSAIC",   0x0400004C}, {"BLDCNT",  0x04000050},
    {"BLDALPHA", 0x04000052}, {"BLDY",      0x04000054},

    //Sound Registers
    {"SOUND1CNT_L", 0x04000060}, {"SOUND1CNT_H", 0x04000062}, {"SOUND1CNT_X", 0x04000064}, {"SOUND2CNT_L", 0x04000068},
    {"SOUND2CNT_H", 0x0400006C}, {"SOUND3CNT_L", 0x04000070}, {"SOUND3CNT_H", 0x04000072}, {"SOUND3CNT_X", 0x04000074},
    {"SOUND4CNT_L", 0x04000078}, {"SOUND4CNT_H", 0x0400007C}, {"SOUNDCNT_L",  0x04000080}, {"SOUNDCNT_H",  0x04000082},
    {"SOUNDCNT_X",  0x04000084}, {"SOUNDBIAS",   0x04000088}, {"WAVE_RAM",    0x04000090}, {"WAVE_RAM",    0x04000092},
    {"WAVE_RAM",    0x04000094}, {"WAVE_RAM",    0x04000096}, {"WAVE_RAM",    0x04000098}, {"WAVE_RAM",    0x0400009A},
    {"WAVE_RAM",    0x0400009C}, {"WAVE_RAM",    0x0400009E}, {"FIFO_A",      0x040000A0}, {"FIFO_B",      0x040000A4},

    //DMA Transfer Channels
    {"DMA0SAD", 0x040000B0}, {"DMA0DAD", 0x040000B4}, {"DMA0CNT_L", 0x040000B8}, {"DMA0CNT_H", 0x040000BA},
    {"DMA1SAD", 0x040000BC}, {"DMA1DAD", 0x040000C0}, {"DMA1CNT_L", 0x040000C4}, {"DMA1CNT_H", 0x040000C6},
    {"DMA2SAD", 0x040000C8}, {"DMA2DAD", 0x040000CC}, {"DMA2CNT_L", 0x040000D0}, {"DMA2CNT_H", 0x040000D2},
    {"DMA3SAD", 0x040000D4}, {"DMA3DAD", 0x040000D8}, {"DMA3CNT_L", 0x040000DC}, {"DMA3CNT_H", 0x040000DE},

    //Timer Registers
    {"TM0CNT_L", 0x04000100}, {"TM0CNT_H", 0x04000102}, {"TM1CNT_L", 0x04000104}, {"TM1CNT_H", 0x04000106},
    {"TM2CNT_L", 0x04000108}, {"TM2CNT_H", 0x0400010A}, {"TM3CNT_L", 0x0400010C}, {"TM3CNT_H", 0x0400010E},

    //Serial Communication 1},
    {"SIODATA32", 0x04000120}, {"SIOMULTI0", 0x04000120}, {"SIOMULTI1",   0x04000122}, {"SIOMULTI2", 0x04000124},
    {"SIOMULTI3", 0x04000126}, {"SIOCNT",    0x04000128}, {"SIOMLT_SEND", 0x0400012A}, {"SIODATA8",  0x0400012A},

    //Keypad Input
    {"KEYINPUT", 0x04000130}, {"KEYCNT", 0x04000132},

    //Serial Communication 2},
    {"RCNT",      0x04000134},
    // 4000136h - IR,  Ancient - Infrared Register Prototypes only,
    {"JOYCNT", 0x04000140}, {"JOY_RECV", 0x04000150}, {"JOY_TRANS", 0x04000154}, {"JOYSTAT", 0x04000158},

    //Interrupt, Waitstate, and Power-Down Control
    {"IE",      0x04000200}, {"IF",      0x04000202}, {"WAITCNT", 0x04000204}, {"IME", 0x04000208},
    {"POSTFLG", 0x04000300}, {"HALTCNT", 0x04000301},
    // 4000410h  ?    ?     Undocumented - Purpose Unknown / Bug ??? 0FFh
    // 4000800h  4    R/W   Undocumented - Internal Memory Control R/W
    // 4xx0800h  4    R/W   Mirrors of 4000800h repeated each 64K

    // All further addresses at 4XXXXXXh are unused and do not contain mirrors of the
    // I/O area, with the only exception that 4000800h is repeated each 64K ie.
    // mirrored at 4010800h, 4020800h, etc.},

    {"IRQ_HANDLER", 0x03007FFC}, {"IRQ_HANDLER", 0x03FFFFFC}, // Only mirror to these addresses, the ones actually
    {"IRQ_BIOS_FLAGS", 0x03007FF8}, {"IRQ_BIOS_FLAGS", 0x03FFFFF8}, // used in programs

//Default memory usage at 03007FXX (and mirrored to 03FFFFXX)

//  Addr. Size Expl.
//  7FFCh 4    Pointer to user IRQ handler (32bit ARM code)
//  7FF8h 4    Interrupt Check Flag (for IntrWait/VBlankIntrWait functions)
//  7FF4h 4    Allocated Area
//  7FF0h 4    Pointer to Sound Buffer
//  7FE0h 16   Allocated Area
//  7FA0h 64   Default area for SP_svc Supervisor Stack (4 words/time)
//  7F00h 160  Default area for SP_irq Interrupt Stack (6 words/time)

    {NULL, 0}
};

static int gba_dissasemble_add_io_register_name(int reg_address, char * dest, int add_comment)
{
    int i = 0;
    while(1)
    {
        if(gba_io_reg_struct[i].name == NULL) break;
        if(gba_io_reg_struct[i].address == reg_address)
        {
            if(add_comment)
            {
                strcat(dest," ; ");
                strcat(dest,gba_io_reg_struct[i].name);
            }
            else
            {
                strcat(dest," - ");
                strcat(dest,gba_io_reg_struct[i].name);
            }
            return 1;
        }
        i++;
    }
    return 0;
}

static struct {
    int code;
    char * name;
} swi_name_struct[] = {
  {0x00,"SoftReset"}, {0x01,"RegisterRamReset"}, {0x02,"Halt"}, {0x03,"Stop/Sleep"}, {0x04,"IntrWait"},
  {0x05,"VBlankIntrWait"}, {0x06,"Div"}, {0x07,"DivArm"}, {0x08,"Sqrt"}, {0x09,"ArcTan"}, {0x0A,"ArcTan2"},
  {0x0B,"CpuSet"}, {0x0C,"CpuFastSet"}, {0x0D,"GetBiosChecksum"}, {0x0E,"BgAffineSet"}, {0x0F,"ObjAffineSet"},
  {0x10,"BitUnPack"}, {0x11,"LZ77UnCompWram"}, {0x12,"LZ77UnCompVram"}, {0x13,"HuffUnComp"}, {0x14,"RLUnCompWram"},
  {0x15,"RLUnCompVram"}, {0x16,"Diff8bitUnFilterWram"},  {0x17,"Diff8bitUnFilterVram"}, {0x18,"Diff16bitUnFilter"},
  {0x19,"SoundBias"}, {0x1A,"SoundDriverInit"}, {0x1B,"SoundDriverMode"}, {0x1C,"SoundDriverMain"},
  {0x1D,"SoundDriverVSync"}, {0x1E,"SoundChannelClear"}, {0x1F,"MidiKey2Freq"}, {0x20,"SoundWhatever0"},
  {0x21,"SoundWhatever1"}, {0x22,"SoundWhatever2"}, {0x23,"SoundWhatever3"}, {0x24,"SoundWhatever4"},
  {0x25,"MultiBoot"}, {0x26,"HardReset"}, {0x27,"CustomHalt"}, {0x28,"SoundDriverVSyncOff"},
  {0x29,"SoundDriverVSyncOn"}, {0x2A,"SoundGetJumpList"},
  {0,NULL}
};

static int gba_disassemble_swi_name(int swi_code, char * dest, int add_comment) // swi code = 1 byte
{
    int i = 0;
    while(1)
    {
        if(swi_name_struct[i].name == NULL) break;
        if(swi_name_struct[i].code == swi_code)
        {
            if(add_comment)
            {
                strcat(dest," ; ");
                strcat(dest,swi_name_struct[i].name);
            }
            else
            {
                strcat(dest," - ");
                strcat(dest,swi_name_struct[i].name);
            }
            return 1;
        }
        i++;
    }
    return 0;
}

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

// ldr      r0, [r1, r2]    @ Pre-indexed.             r0= *(u32*)(r1+r2)
// ldr      r0, [r1, r2]!   @ Pre-indexed,  writeback. r0= *(u32*)(r1 += r2)
// ldr      r0, [r1], r2    @ Post-indexed, writeback. r0= *(u32*)r1; r1 += r2;

static const char arm_cond[16][6] = {
    "eq","ne","cs","cc","mi","pl","vs","vc","hi","ls","ge","lt","gt","le","","nv[!]"
};

static const char arm_shift_type[4][4] = { "lsl","lsr","asr","ror" };

void GBA_DisassembleARM(u32 opcode, u32 address, char * dest)
{
    int arm_cond_code = (opcode >> 28)&0xF;
    const char * cond = arm_cond[arm_cond_code];
    u32 ident = (opcode >> 25) & 7;
    opcode &= 0x01FFFFFF;

    switch(ident)
    {
        case 0:
        {
            if(opcode & BIT(4))
            {
                if(opcode & BIT(7))
                {
                    if(opcode & (3<<5)) // Halfword, Doubleword, and Signed Data Transfer
                    {
                        u32 Rn = (opcode>>16)&0xF; //(including R15=PC+8)
                        u32 Rd = (opcode>>12)&0xF; //(including R15=PC+12)

                        char * sign = (opcode & BIT(23)) ? "+" : "-";

                        u32 writeresult = (opcode & BIT(22)) && (Rn==R_PC); //Only if "Immediate as offset"
                        u32 addr = CPU.R[Rn];

                        char addr_text[32];
                        if(opcode & BIT(24)) //Pre-indexed
                        {
                            char * writeback = (opcode & BIT(21)) ? "!" : "";

                            if(opcode & BIT(22)) //Immediate as offset
                            {
                                //[Rn, <#{+/-}expression>]{!}
                                u32 offset = ((opcode>>4)&0xF0)|(opcode&0xF);
                                if(offset)
                                    sprintf(addr_text,"[r%d, #%s0x%08X]%s",Rn,sign,offset, writeback);
                                else
                                    sprintf(addr_text,"[r%d]%s",Rn, writeback);
                                addr += (opcode & BIT(23)) ? offset : -offset;
                            }
                            else //Register as offset
                            {
                                //[Rn, {+/-}Rm]{!}
                                //11-8 must be 0000
                                u32 Rm = opcode&0xF; //(not including R15)
                                sprintf(addr_text,"[r%d, %sr%d]%s",Rn,sign,Rm, writeback);
                            }
                        }
                        else //Post-indexed
                        {
                            if(opcode & BIT(22)) //Immediate
                            {
                                //[Rn], <#{+/-}expression>
                                u32 offset = ((opcode>>4)&0xF0)|(opcode&0xF);
                                if(offset)
                                    sprintf(addr_text,"[r%d], #%s0x%08X",Rn,sign,offset);
                                else
                                    sprintf(addr_text,"[r%d]",Rn);
                            }
                            else //Register as offset
                            {
                                //[Rn], {+/-}Rm
                                //11-8 must be 0000
                                u32 Rm = opcode&0xF; //(not including R15)
                                sprintf(addr_text,"[r%d], %sr%d",Rn,sign,Rm);
                            }
                        }

                        if(opcode & BIT(20)) // LDR
                        {
                            u32 op = (opcode>>5)&3;

                            if(op == 1)
                            {
                                // LDR{cond}H  Rd,<Address>  ;Load Unsigned halfword (zero-extended)
                                if(writeresult)
                                {
                                    sprintf(dest,"ldr%sh r%d, =0x%08X",cond,Rd,(u32)GBA_MemoryRead16(addr&~1));
                                    gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                                }
                                else
                                {
                                    sprintf(dest,"ldr%sh r%d, %s",cond,Rd,addr_text);
                                    int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                                    gba_dissasemble_add_io_register_name(addr,dest,!comment);
                                }
                                return;
                            }
                            else if(op == 2)
                            {
                                // LDR{cond}SB Rd,<Address>  ;Load Signed byte (sign extended)
                                if(writeresult)
                                {
                                    sprintf(dest,"ldr%ssb r%d, =0x%08X",cond,Rd,(s32)(s8)GBA_MemoryRead8(address));
                                    gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                                }
                                else
                                {
                                    sprintf(dest,"ldr%ssb r%d, %s",cond,Rd,addr_text);
                                    int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                                    gba_dissasemble_add_io_register_name(addr,dest,!comment);
                                }
                                return;
                            }
                            else //if (op == 3)
                            {
                                // LDR{cond}SH Rd,<Address>  ;Load Signed halfword (sign extended)
                                if(writeresult)
                                {
                                    sprintf(dest,"ldr%ssh r%d, =0x%08X",cond,Rd,(s32)(s16)GBA_MemoryRead16(address&~1));
                                    gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                                }
                                else
                                {
                                    sprintf(dest,"ldr%ssh r%d, %s",cond,Rd,addr_text);
                                    int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                                    gba_dissasemble_add_io_register_name(addr,dest,!comment);
                                }
                                return;
                            }
                        }
                        else // STR
                        {
                            if( ((opcode>>5)&3) > 1 )
                            {
                                strcpy(dest, "[!] Undefined Instruction #0-4");
                                return;
                            }

                            // STR{cond}H  Rd,<Address>  ;Store halfword
                            sprintf(dest,"str%sh r%d, %s",cond,Rd,addr_text);
                            int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                            gba_dissasemble_add_io_register_name(addr,dest,!comment);
                            return;
                        }
                        return;
                    }
                    else //MUL/SWP
                    {
                        if(opcode & BIT(24)) //SWP{cond}{B} Rd,Rm,[Rn]
                        {
                            if( opcode & BIT(23) || (opcode & (0xF00|(3<<20))) )
                            {
                                strcpy(dest, "[!] Undefined Instruction #0-1");
                                return;
                            }

                            char * bytemode = opcode & BIT(22) ? "b" : "";

                            u32 Rn = (opcode>>16)&0xF; // |
                            u32 Rd = (opcode>>12)&0xF; // | r0 - r14
                            u32 Rm = opcode&0xF;       // |

                            sprintf(dest,"swp%s%s r%d, r%d, [r%d]",cond,bytemode,Rd,Rm,Rn);
                            int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                            gba_dissasemble_add_io_register_name(CPU.R[Rn],dest,!comment);
                            return;
                        }
                        else //Multiplication
                        {
                            u32 op = (opcode>>21)&7;

                            char * setcond = ((opcode & BIT(20)) != 0) ? "s" : "";

                            u32 Rd = (opcode>>16)&0xF; // |            (or RdHi)
                            u32 Rn = (opcode>>12)&0xF; // | r0 - r14   (or RdLo)
                            u32 Rs = (opcode>>8)&0xF;  // |
                            u32 Rm = opcode&0xF;       // |

                            switch(op)
                            {
                                case 0: sprintf(dest,"mul%s%s r%d, r%d, r%d",cond,setcond,Rd,Rm,Rs); break;
                                case 1: sprintf(dest,"mla%s%s r%d, r%d, r%d, r%d",cond,setcond,Rd,Rm,Rs,Rn); break;
                                case 2: case 3:
                                    strcpy(dest, "[!] Undefined Instruction #0-2");
                                    return;
                                case 4: sprintf(dest,"umull%s%s r%d, r%d, r%d, r%d",cond,setcond,Rn,Rd,Rm,Rs); break;
                                case 5: sprintf(dest,"umlal%s%s r%d, r%d, r%d, r%d",cond,setcond,Rn,Rd,Rm,Rs); break;
                                case 6: sprintf(dest,"smull%s%s r%d, r%d, r%d, r%d",cond,setcond,Rn,Rd,Rm,Rs); break;
                                case 7: sprintf(dest,"smlal%s%s r%d, r%d, r%d, r%d",cond,setcond,Rn,Rd,Rm,Rs); break;
                            }
                            gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                            return;
                        }
                    }
                }
                else
                {
                    if((opcode & 0x0FFFFFF0) == 0x012FFF10)
                    {
                        //BX{cond}
                        u32 Rn = opcode&0xF;
                        if(CPU.R[Rn] & 1) //Switch to THUMB
                        {
                            sprintf(dest,"bx%s r%d ; Switch to THUMB",cond,Rn); //PC=Rn-1, T=Rn.0
                            gba_dissasemble_add_condition_met(arm_cond_code,address,dest,0);
                        }
                        else
                        {
                            sprintf(dest,"bx%s r%d",cond,Rn); //PC=Rn, T=Rn.0
                            gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                        }
                        return;
                    }

                    //Data Processing, (Register shifted by Register) 2nd Operand

                    u32 op = (opcode >> 21);

                    if( ((op & 0xC) == 0x8) && ((opcode & BIT(20)) == 0) ) //8<=op<=b
                    {
                        strcpy(dest, "[!] Undefined Instruction #0-3");
                        return;
                    }

                    u32 Rn = (opcode>>16)&0xF; //Must be 0000b for MOV/MVN.
                    u32 Rd = (opcode>>12)&0xF; //Must be 0000b {or 1111b) for CMP/CMN/TST/TEQ{P}.

                    char * setcond = ((opcode & BIT(20)) != 0) ? "s" : "";

                    const char * shift = arm_shift_type[(opcode>>5)&3];

                    u32 Rs = (opcode>>8)&0xF; //(R0-R14)
                    u32 Rm = opcode & 0xF;

                    switch(op)
                    {
                        case 0: sprintf(dest,"and%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 1: sprintf(dest,"eor%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 2: sprintf(dest,"sub%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 3: sprintf(dest,"rsb%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 4: sprintf(dest,"add%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 5: sprintf(dest,"adc%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 6: sprintf(dest,"sbc%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 7: sprintf(dest,"rsc%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 8: sprintf(dest,"tst%s r%d, r%d, %s r%d",cond,Rn,Rm,shift,Rs); break;
                        case 9: sprintf(dest,"teq%s r%d, r%d, %s r%d",cond,Rn,Rm,shift,Rs); break;
                        case 0xA: sprintf(dest,"cmp%s r%d, r%d, %s r%d",cond,Rn,Rm,shift,Rs); break;
                        case 0xB: sprintf(dest,"cmn%s r%d, r%d, %s r%d",cond,Rn,Rm,shift,Rs); break;
                        case 0xC: sprintf(dest,"orr%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 0xD: sprintf(dest,"mov%s%s r%d, r%d, %s r%d",cond,setcond,Rd,Rm,shift,Rs); break;
                        case 0xE: sprintf(dest,"bic%s%s r%d, r%d, r%d, %s r%d",cond,setcond,Rd,Rn,Rm,shift,Rs); break;
                        case 0xF: sprintf(dest,"mvn%s%s r%d, r%d, %s r%d",cond,setcond,Rd,Rm,shift,Rs); break;
                    }
                    gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                    return;
                }
            }
            else
            {
                if( (opcode & ((3<<23)|(0x3F<<16)|0xFFF)) == ((2<<23)|(0x0F<<16)) )
                {
                    u32 Rd = (opcode>>12)&0xF;

                    if(opcode & BIT(22)) //MRS{cond} Rd,spsr
                    {
                        sprintf(dest,"mrs%s r%d,spsr",cond,Rd);
                        gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                        return;
                    }
                    else //MRS{cond} Rd,cpsr
                    {
                        sprintf(dest,"mrs%s r%d,cpsr",cond,Rd);
                        gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                        return;
                    }
                }

                u32 val = opcode & (0x1F<<20);
                if( val == (0x12<<20) )
                {
                    // MSR{cond} cpsr{_field},Rm
                    u32 Rm = opcode & 0xF;
                    char fields[5]; int cursor = 0;
                    if(opcode & BIT(19)) fields[cursor++] = 'f'; //31-24
                    if(opcode & BIT(18)) fields[cursor++] = 's';
                    if(opcode & BIT(17)) fields[cursor++] = 'x';
                    if(opcode & BIT(16)) fields[cursor++] = 'c'; //7-0
                    fields[cursor] = '\0';

                    sprintf(dest,"msr%s cpsr_%s, r%d",cond,fields,Rm);
                    gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                    return;
                }
                else if( val == (0x16<<20) )
                {
                    // MSR{cond} spsr{_field},Rm
                    u32 Rm = opcode & 0xF;
                    char fields[5]; int cursor = 0;
                    if(opcode & BIT(19)) fields[cursor++] = 'f'; //31-24
                    if(opcode & BIT(18)) fields[cursor++] = 's';
                    if(opcode & BIT(17)) fields[cursor++] = 'x';
                    if(opcode & BIT(16)) fields[cursor++] = 'c'; //7-0
                    fields[cursor] = '\0';

                    sprintf(dest,"msr%s spsr_%s, r%d",cond,fields,Rm);
                    gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                    return;
                }

                //Data Processing, (Shift by Inmediate) 2nd Operand
                u32 op = (opcode >> 21);

                if( ((op & 0xC) == 0x8) && ((opcode & BIT(20)) == 0) ) //8<=op<=b
                {
                    strcpy(dest, "[!] Undefined Instruction #1");
                    return;
                }

                u32 Rn = (opcode>>16)&0xF; //Must be 0000b for MOV/MVN.
                u32 Rd = (opcode>>12)&0xF; //Must be 0000b {or 1111b) for CMP/CMN/TST/TEQ{P}.

                char * setcond = ((opcode & BIT(20)) != 0) ? "s" : "";

                u32 shiftval = (opcode>>7) & 0x1F;
                u32 Rm = opcode & 0xF;
                char * shift = (char*)arm_shift_type[(opcode>>5)&3];

                int canbenop = 0;

                char temp[40];
                if(shiftval == 0)
                {
                    switch((opcode>>5)&3)
                    {
                        default:
                        case 0: strcpy(temp,""); if((opcode & BIT(20)) == 0) canbenop = 1; break;
                        case 1: case 2: shiftval = 32; sprintf(temp,", %s #0x%02X",shift,shiftval); break;
                        case 3: strcpy(temp,", rrx"); break;
                    }
                }
                else
                {
                    sprintf(temp,", %s #0x%02X",shift,shiftval);
                }

                switch(op)
                {
                    case 0: sprintf(dest,"and%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;;
                    case 1: sprintf(dest,"eor%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 2: sprintf(dest,"sub%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 3: sprintf(dest,"rsb%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 4: sprintf(dest,"add%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 5: sprintf(dest,"adc%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 6: sprintf(dest,"sbc%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 7: sprintf(dest,"rsc%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 8: sprintf(dest,"tst%s r%d, r%d%s",cond,Rn,Rm,temp); break;
                    case 9: sprintf(dest,"teq%s r%d, r%d%s",cond,Rn,Rm,temp); break;
                    case 0xA: sprintf(dest,"cmp%s r%d, r%d%s",cond,Rn,Rm,temp); break;
                    case 0xB: sprintf(dest,"cmn%s r%d, r%d%s",cond,Rn,Rm,temp); break;
                    case 0xC: sprintf(dest,"orr%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 0xD:
                        if( (canbenop) && ((Rd|Rm) == 0) )
                            sprintf(dest,"nop%s",cond);
                        else
                            sprintf(dest,"mov%s%s r%d, r%d%s",cond,setcond,Rd,Rm,temp);
                        break;
                    case 0xE: sprintf(dest,"bic%s%s r%d, r%d, r%d%s",cond,setcond,Rd,Rn,Rm,temp); break;
                    case 0xF: sprintf(dest,"mvn%s%s r%d, r%d%s",cond,setcond,Rd,Rm,temp); break;
                }
                gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                return;
            }
        }
        case 1:
        {
            if( (opcode & ((3<<23)|(3<<20))) == ((2<<23)|(2<<20)) )
            {
                // MSR{cond} Psr{_field},Imm

                //20-17 must be 1111b
                char * dst = opcode & BIT(22) ? "spsr" : "cpsr";
                u32 value = ror_immed_no_carry(opcode&0xFF,((opcode>>8)&0xF)<<1);
                char fields[5]; int cursor = 0;
                if(opcode & BIT(19)) fields[cursor++] = 'f'; //31-24
                if(opcode & BIT(18)) fields[cursor++] = 's';
                if(opcode & BIT(17)) fields[cursor++] = 'x';
                if(opcode & BIT(16)) fields[cursor++] = 'c'; //7-0
                fields[cursor] = '\0';
                sprintf(dest,"msr%s %s_%s, #0x%08X",cond,dst,fields,value);
                gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                return;
            }

            //Data Processing, Immediate 2nd Operand
            u32 op = (opcode >> 21);

            if( ((op & 0xC) == 0x8) && ((opcode & BIT(20)) == 0) ) //8<=op<=b
            {
                strcpy(dest, "[!] Undefined Instruction #1");
                return;
            }

            u32 Rn = (opcode>>16)&0xF; //Must be 0000b for MOV/MVN.
            u32 Rd = (opcode>>12)&0xF; //Must be 0000b {or 1111b) for CMP/CMN/TST/TEQ{P}.

            char * setcond = ((opcode & BIT(20)) != 0) ? "s" : "";

            u32 val = ror_immed_no_carry(opcode&0xFF,((opcode>>8)&0xF)<<1);

            switch(op)
            {
                case 0: sprintf(dest,"and%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 1: sprintf(dest,"eor%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 2: sprintf(dest,"sub%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 3: sprintf(dest,"rsb%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 4: sprintf(dest,"add%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 5: sprintf(dest,"adc%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 6: sprintf(dest,"sbc%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 7: sprintf(dest,"rsc%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 8: sprintf(dest,"tst%s r%d, #0x%08X",cond,Rn,val); break;
                case 9: sprintf(dest,"teq%s r%d, #0x%08X",cond,Rn,val); break;
                case 0xA: sprintf(dest,"cmp%s r%d, #0x%08X",cond,Rn,val); break;
                case 0xB: sprintf(dest,"cmn%s r%d, #0x%08X",cond,Rn,val); break;
                case 0xC: sprintf(dest,"orr%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 0xD: sprintf(dest,"mov%s%s r%d, #0x%08X",cond,setcond,Rd,val); break;
                case 0xE: sprintf(dest,"bic%s%s r%d, r%d, #0x%08X",cond,setcond,Rd,Rn,val); break;
                case 0xF: sprintf(dest,"mvn%s%s r%d, #0x%08X",cond,setcond,Rd,val); break;
                default: break;
            }
            gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
            return;
        }
        case 2:
        {
            // LDR/STR -- INMEDIATE AS OFFSET

            u32 Rn = (opcode>>16)&0xF; //(including R15=PC+8)
            u32 Rd = (opcode>>12)&0xF; //(including R15=PC+12)
            s32 offset = opcode & 0xFFF;
            char * sign = (opcode & BIT(23)) ? "+" : "-";

            char * bytemode = opcode & BIT(22) ? "b" : "";

            char * forceuser = "";

            char addr_text[32];
            if(opcode & BIT(24)) //Pre-indexed
            {
                if(offset)
                    sprintf(addr_text,"[r%d, #%s0x%03X]%s",Rn,sign,offset, opcode & BIT(21) ? "!":"");
                else
                    sprintf(addr_text,"[r%d]%s",Rn, opcode & BIT(21) ? "!":"");
            }
            else //Post-indexed
            {
                forceuser = (opcode & BIT(21)) ? "t" : "";
                if(offset)
                    sprintf(addr_text,"[r%d], #%s0x%03X",Rn,sign,offset);
                else
                    sprintf(addr_text,"[r%d]",Rn);
            }

            u32 addr = address;

            if(opcode & BIT(20)) // LDR{cond}{B}{T} Rd,<Address>
            {
                if(Rn==R_PC)
                {
                    if(opcode & BIT(24)) addr += 8 + ( (opcode & BIT(23)) ? offset : -offset); //Pre-indexed
                    if(opcode & BIT(22)) sprintf(dest,"ldr%s%s%s r%d, =0x%08X",cond,bytemode,forceuser,Rd,GBA_MemoryRead32(addr)&0xFF);
                    else sprintf(dest,"ldr%s%s%s r%d, =0x%08X",cond,bytemode,forceuser,Rd,GBA_MemoryRead32(addr));
                }
                else
                {
                    if(opcode & BIT(24)) addr += ( (opcode & BIT(23)) ? offset : -offset); //Pre-indexed
                    sprintf(dest,"ldr%s%s%s r%d, %s",cond,bytemode,forceuser,Rd,addr_text);
                }
            }
            else // STR{cond}{B}{T} Rd,<Address>
            {
                if(opcode & BIT(24)) addr += ( (opcode & BIT(23)) ? offset : -offset); //Pre-indexed
                sprintf(dest,"str%s%s%s r%d, %s",cond,bytemode,forceuser,Rd,addr_text);
            }
            int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
            gba_dissasemble_add_io_register_name(CPU.R[Rn],dest,!comment);
            return;

        }
        case 3:
        {
            // LDR/STR -- SHIFTED REGISTER AS OFFSET

            if(opcode & BIT(4))
            {
                strcpy(dest, "[!] Undefined Instruction");
                return;
            }

            u32 Rn = (opcode>>16)&0xF; //(including R15=PC+8)
            u32 Rd = (opcode>>12)&0xF; //(including R15=PC+12)
            u32 Rm = opcode&0xF; //(not including PC=R15)

            char * sign = (opcode & BIT(23)) ? "+" : "-";
            char * bytemode = opcode & BIT(22) ? "b" : "";

            char * forceuser = "";

            char shift_text[16];
            u32 shiftval = (opcode>>7) & 0x1F;
            char * shift = (char*)arm_shift_type[(opcode>>5)&3];

            if(shiftval == 0)
            {
                switch((opcode>>5)&3)
                {
                    default:
                    case 0: sprintf(shift_text,"r%d",Rm); break;
                    case 1: case 2: shiftval = 32; sprintf(shift_text,"r%d, %s #%02X",Rm,shift,shiftval); break;
                    case 3: sprintf(shift_text,"r%d, rrx",Rm); break;
                }
            }
            else
            {
                sprintf(shift_text,"r%d, %s #%02X",Rm,shift,shiftval);
            }


            u32 addr = CPU.R[Rn] + (Rn==15?8:0);

            char addr_text[32];
            if(opcode & BIT(24)) //Pre-indexed
            {
                sprintf(addr_text,"[r%d, %s%s]%s",Rn,sign,shift_text,opcode & BIT(21) ? "!":"");
                s32 offset = cpu_shift_by_reg_no_carry_arm_ldr_str(
                                        (opcode>>5)&3,CPU.R[opcode&0xF],(opcode>>7)&0x1F);
                addr += (opcode & BIT(23)) ? offset : -offset;
            }
            else //Post-indexed
            {
                forceuser = (opcode & BIT(21)) ? "t" : "";
                sprintf(addr_text,"[r%d], %s%s",Rn,sign,shift_text);
            }

            if(opcode & BIT(20)) // LDR{cond}{B}{T} Rd,<Address>
            {
                sprintf(dest,"ldr%s%s%s r%d, %s",cond,bytemode,forceuser,Rd,addr_text);
            }
            else // STR{cond}{B}{T} Rd,<Address>
            {
                sprintf(dest,"str%s%s%s r%d, %s",cond,bytemode,forceuser,Rd,addr_text);
            }
            int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
            gba_dissasemble_add_io_register_name(addr,dest,!comment);
            return;
        }
        case 4:
        {
/*
            if( (opcode & (3<<21)) == (3<<21) )
            {
                //Unpredictable
                strcpy(dest, "[!] Undefined Instruction #4");
                return;
            }
*/
            //Block Data Transfer (LDM,STM)

            char * increment_time = (opcode & BIT(24)) ? "b" : "a"; // before : after | pre : post
            char * sign = (opcode & BIT(23)) ? "i" : "d"; //increment : decrement | + : -
            char * usrmod = (opcode & BIT(22)) ? "^" : "";
            char * writeback = (opcode & BIT(21)) ? "!" : "";

            u32 Rn = (opcode>>16)&0xF; //(not including R15)

            u32 load = opcode & BIT(20);

            int is_pop = (opcode & BIT(23)) && (!(opcode & BIT(24))) && (opcode & BIT(21)) && (Rn == R_SP);
            int is_push = (!(opcode & BIT(23))) && (opcode & BIT(24)) && (opcode & BIT(21)) && (Rn == R_SP);

            opcode &= 0xFFFF;

            char reglist[128] = "{";
            int i;
            for(i = 0; i < 16; i++)
            {
                if(opcode & BIT(i))
                {
                    char reg[4]; sprintf(reg,"r%d",i);
                    strcat(reglist,reg);
                    if( (opcode & BIT(i+1)) && (opcode & BIT(i+2)) )
                    {
                        strcat(reglist,"-");
                        while(opcode & BIT(i++));
                        i-=2;
                        sprintf(reg,"r%d",i);
                        strcat(reglist,reg);
                    }

                    int j = i+1;
                    while((opcode & BIT(j))==0) { j++; if(j == 16) break; }
                    if(j < 16) strcat(reglist,",");
                }
            }
            strcat(reglist,"}");

            if(load)
            {
                // LDM{cond}{amod} Rn{!},<Rlist>{^}
                sprintf(dest,"ldm%s%s%s r%d%s, %s%s",cond,sign,increment_time,Rn,writeback,
                        reglist,usrmod);
                if(is_pop)
                    strcat(dest," ; pop"); // ldmia r13! == pop
            }
            else
            {
                // STM{cond}{amod} Rn{!},<Rlist>{^}
                sprintf(dest,"stm%s%s%s r%d%s, %s%s",cond,sign,increment_time,Rn,writeback,
                        reglist,usrmod);
                if(is_push)
                    strcat(dest," ; push"); // stmdb r13! == push
            }
            gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
            return;
        }
        case 5:
        {
            if(opcode & (1<<24)) //BL{cond}
            {
                u32 nn = (opcode & 0x00FFFFFF);
                sprintf(dest,"bl%s #0x%08X",cond,address+8+
                        ( ((nn & BIT(23)) ? (nn|0xFF000000) : nn)*4 ) );
                strcat(dest," ; ->");
                gba_dissasemble_add_condition_met(arm_cond_code,address,dest,0);
                return;
            }
            else //B{cond}
            {
                u32 nn = (opcode & 0x00FFFFFF);
                u32 addr_dest = address+8+ ( ((nn & BIT(23)) ? (nn|0xFF000000) : nn)*4 );
                sprintf(dest,"b%s #0x%08X",cond,addr_dest);
                if(addr_dest > address) strcat(dest," ; " STR_SLIM_ARROW_DOWN);
                else if(addr_dest == address) strcat(dest," ; <-");
                else strcat(dest," ; " STR_SLIM_ARROW_UP);
                gba_dissasemble_add_condition_met(arm_cond_code,address,dest,0);
                return;
            }
        }
        case 6:
        {
            //Coprocessor Data Transfers (LDC,STC)
            //Irrelevant in GBA because no coprocessor exists (except a dummy CP14).

            u32 Rn = (opcode>>16) & 0xF;
            u32 Cd = (opcode>>12) & 0xF;
            u32 Pn = (opcode>>8) & 0xF;
            u32 offset = (opcode & 0xFF)<<2;
            char * sign = (opcode & BIT(23)) ? "+" : "-";
            char * length = (opcode & BIT(22)) ? "l" : "";
            char * writeback = (opcode & BIT(21)) ? "!" : "";

            char addr_text[32];
            if(opcode & BIT(24)) //Pre
            {
                if(offset)
                    sprintf(addr_text,"[r%d, #%s0x%03X]%s",Rn,sign,offset,writeback);
                else
                    sprintf(addr_text,"[r%d]%s",Rn,writeback);
            }
            else //Post
            {
                //Always writeback?
                if(offset)
                    sprintf(addr_text,"[r%d], #%s0x%03X",Rn,sign,offset);
                else
                    sprintf(addr_text,"[r%d]",Rn);
            }

            if(opcode & BIT(20)) // LDC{cond}{L} Pn,Cd,<Address>
            {
                sprintf(dest,"ldc%s%s p%d, c%d, %s ; [!]",cond,length,Pn,Cd,addr_text);
                gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                return;
            }
            else // STC{cond}{L} Pn,Cd,<Address>
            {
                sprintf(dest,"stc%s%s p%d, c%d, %s ; [!]",cond,length,Pn,Cd,addr_text);
                gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                return;
            }
        }
        case 7:
        {
            if(opcode & BIT(24))
            {
                //SWI{cond}
                sprintf(dest,"swi%s #0x%06X",cond,(opcode & 0x00FFFFFF));
                int comment = gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                gba_disassemble_swi_name((opcode & 0x00FFFFFF)>>16,dest,!comment);
                return;
            }
            else
            {
                if(opcode & BIT(4)) //Coprocessor Register Transfers (MRC, MCR)
                {
                    //Irrelevant in GBA because no coprocessor exists (except a dummy CP14).
                    u32 CP_Opc = (opcode>>21) & 7;
                    u32 Cn = (opcode>>16) & 0xF;
                    u32 Rd = (opcode>>12) & 0xF;
                    u32 Pn = (opcode>>8) & 0xF;
                    u32 CP = (opcode>>5) & 7;
                    u32 Cm = opcode & 0xF;

                    if(opcode & BIT(20)) // MRC{cond} Pn,<cpopc>,Rd,Cn,Cm{,<cp>}
                    {
                        sprintf(dest,"mrc%s p%d, #0x%01X, r%d, c%d, c%d, #0x%01X ; [!]",cond,Pn,CP_Opc,Rd,Cn,Cm,CP);
                        gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                        return;
                    }
                    else // MCR{cond} Pn,<cpopc>,Rd,Cn,Cm{,<cp>}
                    {
                        sprintf(dest,"mcr%s p%d, #0x%01X, r%d, c%d, c%d, #0x%01X ; [!]",cond,Pn,CP_Opc,Rd,Cn,Cm,CP);
                        gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                        return;
                    }
                }
                else //Coprocessor Data Operations (CDP)
                {
                    //Irrelevant in GBA because no coprocessor exists (except a dummy CP14).
                    u32 CP_Opc = (opcode>>20) & 0xF;
                    u32 Cn = (opcode>>16) & 0xF;
                    u32 Cd = (opcode>>12) & 0xF;
                    u32 Pn = (opcode>>8) & 0xF;
                    u32 CP = (opcode>>5) & 7;
                    u32 Cm = opcode & 0xF;

                    sprintf(dest,"cdp%s p%d, #0x%01X, c%d, c%d, c%d, #0x%01X ; [!]",cond,Pn,CP_Opc,Cd,Cn,Cm,CP);
                    gba_dissasemble_add_condition_met(arm_cond_code,address,dest,1);
                    return;
                }
            }
        }
    }

    strcpy(dest,"Unknown Opcode.");
    return;
}

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

static const char thumb_alu_operation[16][4] = {
    "and", "eor", "lsl", "lsr", "asr", "adc", "sbc", "ror", "tst", "neg", "cmp", "cmn", "orr", "mul", "bic", "mvn"
};

void GBA_DisassembleTHUMB(u16 opcode, u32 address, char * dest)
{
    u16 ident = opcode >> 12;
    opcode &= 0x0FFF;

    switch(ident)
    {
        case 0:
        {
            u16 Rd = opcode & 7;
            u16 Rs = (opcode>>3) & 7;
            u16 immed = (opcode >> 6) & 0x1F;
            if(opcode & BIT(11))
            {
                //LSR Rd,Rs,#Offset
                if(immed == 0) immed = 32;//LSR#0: Interpreted as LSR#32
                sprintf(dest,"lsr r%d, r%d, #0x%02X",Rd,Rs,immed);
                return;
            }
            else
            {
                //LSL Rd,Rs,#Offset
                sprintf(dest,"lsl r%d, r%d, #0x%02X",Rd,Rs,immed);
                return;
            }
            break;
        }
        case 1:
        {
            if(opcode & BIT(11))
            {
                u16 Rd = opcode & 7;
                u16 Rs = (opcode>>3) & 7;

                switch((opcode>>9) & 3)
                {
                    case 0: //ADD Rd,Rs,Rn
                    {
                        u16 Rn = (opcode>>6) & 7;
                        sprintf(dest,"add r%d, r%d, r%d",Rd,Rs,Rn);
                        return;
                    }
                    case 1: //SUB Rd,Rs,Rn
                    {
                        u16 Rn = (opcode>>6) & 7;
                        sprintf(dest,"sub r%d, r%d, r%d",Rd,Rs,Rn);
                        return;
                    }
                    case 2: //ADD Rd,Rs,#nn
                    {
                        u16 immed = (opcode >> 6) & 0x7;
                        if(immed)
                            sprintf(dest,"add r%d, r%d, #0x%01X",Rd,Rs,immed);
                        else
                            sprintf(dest,"mov r%d, r%d",Rd,Rs);
                        return;
                    }
                    case 3: //SUB Rd,Rs,#nn
                    {
                        u16 immed = (opcode >> 6) & 0x7;
                        sprintf(dest,"sub r%d, r%d, #0x%01X",Rd,Rs,immed);
                        return;
                    }
                }
            }
            else
            {
                //ASR Rd,Rs,#Offset
                u16 Rd = opcode & 7;
                u16 Rs = (opcode>>3) & 7;
                u16 immed = (opcode >> 6) & 0x1F;

                if(immed == 0) immed = 32;//ASR#0: Interpreted as ASR#32
                sprintf(dest,"asr r%d, r%d, #0x%02X",Rd,Rs,immed);
                return;
            }
            break;
        }
        case 2:
        {
            u32 Rd = (opcode >> 8) & 7;
            u32 immed = opcode & 0xFF;

            if(opcode & BIT(11))
            {
                //CMP Rd,#nn
                sprintf(dest,"cmp r%d, #0x%02X",Rd,immed);
                return;
            }
            else
            {
                //MOV Rd,#nn
                sprintf(dest,"mov r%d, #0x%02X",Rd,immed);
                return;
            }
            break;
        }
        case 3:
        {
            u32 Rd = (opcode >> 8) & 7;
            u32 immed = opcode & 0xFF;

            if(opcode & BIT(11))
            {
                //SUB Rd,#nn
                sprintf(dest,"sub r%d, #0x%02X",Rd,immed);
                return;
            }
            else
            {
                //ADD Rd,#nn
                sprintf(dest,"add r%d, #0x%02X",Rd,immed);
                return;
            }
            break;
        }
        case 4:
        {
            if(opcode & BIT(11))
            {
                //LDR Rd,[PC,#nn]
                u16 Rd = (opcode>>8)&7;
                u32 offset = (opcode&0xFF)<<2;
                //sprintf(dest,"ldr r%d, [pc, #%03X]",Rd,offset);
                //sprintf(dest,"ldr r%d, [pc, #0x%03X] =0x%08X",Rd,offset,GBA_MemoryRead32(((address+4)&(~2))+offset));
                sprintf(dest,"ldr r%d, =0x%08X",Rd,GBA_MemoryRead32(((address+4)&(~2))+offset));
                return;
            }
            else
            {
                if(opcode & BIT(10))
                {
                    //Hi register operations/branch exchange
                    switch((opcode>>8)&3)
                    {
                        case 0:
                        {
                            if((opcode>>6)&3)
                            {
                                //ADD Rd,Rs
                                u16 Rd = (opcode&7) | ( (opcode>>4) & 8);
                                u16 Rs = (opcode>>3)&0xF;
                                sprintf(dest,"add r%d, r%d",Rd,Rs);
                                return;
                            }
                            else
                            {
                                u16 Rd = (opcode&7) | ( (opcode>>4) & 8);
                                u16 Rs = (opcode>>3)&0xF;
                                sprintf(dest,"add r%d, r%d ; [!] (Unused Opcode #4-0)",Rd,Rs);
                                return;
                            }
                        }
                        case 1:
                        {
                            if((opcode>>6)&3)
                            {
                                //CMP Rd,Rs
                                u16 Rd = (opcode&7) | ( (opcode>>4) & 8);
                                u16 Rs = (opcode>>3)&0xF;
                                sprintf(dest,"cmp r%d, r%d",Rd,Rs);
                                return;
                            }
                            else
                            {
                                //CMP Rd,Rs
                                u16 Rd = (opcode&7) | ( (opcode>>4) & 8);
                                u16 Rs = (opcode>>3)&0xF;
                                sprintf(dest,"cmp r%d, r%d ; [!] (Unused Opcode #4-1)",Rd,Rs);
                                return;
                            }
                        }
                        case 2:
                        {
                            if((opcode>>6)&3)
                            {
                                //MOV Rd,Rs
                                u16 Rd = (opcode&7) | ( (opcode>>4) & 8);
                                u16 Rs = (opcode>>3)&0xF;
                                if( (Rd == 8) && (Rs == 8) )
                                    strcpy(dest,"nop");
                                else
                                    sprintf(dest,"mov r%d, r%d",Rd,Rs);
                                return;
                            }
                            else //Tested in real hardware
                            {
                                //MOV Rd,Rs
                                u16 Rd = (opcode&7) | ( (opcode>>4) & 8);
                                u16 Rs = (opcode>>3)&0xF;
                                if( (Rd == 8) && (Rs == 8) )
                                    strcpy(dest,"nop ; [!] (Unused Opcode #4-2)");
                                else
                                    sprintf(dest,"mov r%d, r%d ; [!] (Unused Opcode #4-2)",Rd,Rs);
                                return;
                            }
                        }
                        case 3:
                        {
                            if(opcode & 7)
                            {
                                strcpy(dest,"[!] BX/BLX Undefined Opcode #4-3");
                                return;
                            }
                            else
                            {
                                if(opcode & BIT(7))
                                {
                                    //(BLX  Rs), Unpredictable
                                    strcpy(dest,"[!] BLX Rs - Undefined Opcode #4-4");
                                    return;
                                }
                                else
                                {
                                    //BX  Rs
                                    u16 Rs = (opcode>>3)&0xF;
                                    if(CPU.R[Rs]&BIT(0))
                                        sprintf(dest,"bx r%d",Rs);
                                    else
                                        sprintf(dest,"bx r%d ; Switch to ARM",Rs);
                                    return;
                                }
                            }

                        }
                    }
                    break;
                }
                else
                {
                    //ALU operations
                    u16 Rd = opcode&7;
                    u16 Rs = (opcode>>3)&7;
                    u16 op = (opcode>>6)&0xF;
                    sprintf(dest,"%s r%d, r%d",thumb_alu_operation[op],Rd,Rs);
                    return;
                }
            }
            break;
        }
        case 5:
        {
            u16 Rd = opcode&7;
            u16 Rb = (opcode>>3)&7;
            u16 Ro = (opcode>>6)&7;

            u16 op = (opcode>>9)&7;

            static const char thumb_mem_ops[8][6] = {
                "str", "strh", "strb", "ldsb", "ldr", "ldrh", "ldrb", "ldsh"
            };

            sprintf(dest,"%s r%d, [r%d, r%d]",thumb_mem_ops[op],Rd,Rb,Ro);
            gba_dissasemble_add_io_register_name(CPU.R[Rb]+CPU.R[Ro],dest,1);
            return;
        }
        case 6:
        {
            u16 Rb = (opcode>>3)&7;
            u16 Rd = opcode&7;
            u16 offset = (opcode>>4)&(0x1F<<2);
            if(opcode & BIT(11))
            {
                //LDR  Rd,[Rb,#nn]
                if(offset)
                    sprintf(dest,"ldr r%d, [r%d, #0x%02X]",Rd,Rb,offset);
                else
                    sprintf(dest,"ldr r%d, [r%d]",Rd,Rb);
            }
            else
            {
                //STR  Rd,[Rb,#nn]
                if(offset)
                    sprintf(dest,"str r%d, [r%d, #0x%02X]",Rd,Rb,offset);
                else
                    sprintf(dest,"str r%d, [r%d]",Rd,Rb);
            }
            gba_dissasemble_add_io_register_name(CPU.R[Rb]+offset,dest,1);
            return;
        }
        case 7:
        {
            u16 Rb = (opcode>>3)&7;
            u16 Rd = opcode&7;
            u16 offset = (opcode>>6)&0x1F;
            if(opcode & BIT(11))
            {
                //LDRB Rd,[Rb,#nn]
                if(offset)
                    sprintf(dest,"ldrb r%d, [r%d, #0x%02X]",Rd,Rb,offset);
                else
                    sprintf(dest,"ldrb r%d, [r%d]",Rd,Rb);
            }
            else
            {
                //STRB Rd,[Rb,#nn]
                if(offset)
                    sprintf(dest,"strb r%d, [r%d, #0x%02X]",Rd,Rb,offset);
                else
                    sprintf(dest,"strb r%d, [r%d]",Rd,Rb);
            }
            gba_dissasemble_add_io_register_name(CPU.R[Rb]+offset,dest,1);
            return;
        }
        case 8:
        {
            u16 Rd = opcode&7;
            u16 Rb = (opcode>>3)&7;
            u16 offset = (opcode>>5)&(0x1F<<1);
            if(opcode & BIT(11))
            {
                //LDRH Rd,[Rb,#nn]
                if(offset)
                    sprintf(dest,"ldrh r%d, [r%d, #0x%02X]",Rd,Rb,offset);
                else
                    sprintf(dest,"ldrh r%d, [r%d]",Rd,Rb);
            }
            else
            {
                //STRH Rd,[Rb,#nn]
                if(offset)
                    sprintf(dest,"strh r%d, [r%d, #0x%02X]",Rd,Rb,offset);
                else
                    sprintf(dest,"strh r%d, [r%d]",Rd,Rb);
            }
            gba_dissasemble_add_io_register_name(CPU.R[Rb]+offset,dest,1);
            return;
        }
        case 9:
        {
            u16 Rd = (opcode>>8)&7;
            u16 offset = (opcode&0xFF)<<2;
            if(opcode & BIT(11))
            {
                //LDR  Rd,[SP,#nn]
                if(offset)
                    sprintf(dest,"ldr r%d, [sp, #0x%02X]",Rd,offset);
                else
                    sprintf(dest,"ldr r%d, [sp]",Rd);
                return;
            }
            else
            {
                //STR  Rd,[SP,#nn]
                if(offset)
                    sprintf(dest,"str r%d, [sp, #0x%02X]",Rd,offset);
                else
                    sprintf(dest,"str r%d, [sp]",Rd);
                return;
            }
            break;
        }
        case 0xA:
        {
            u16 Rd = (opcode>>8)&7;
            u16 offset = (opcode&0xFF)<<2;
            if(opcode & BIT(11))
            {
                //ADD  Rd,SP,#nn
                if(offset)
                    sprintf(dest,"add r%d, sp, #0x%03X",Rd,offset);
                else
                    sprintf(dest,"mov r%d, sp",Rd);
                return;
            }
            else
            {
                //ADD  Rd,PC,#nn
                if(offset)
                    sprintf(dest,"add r%d, pc, #0x%03X",Rd,offset);
                else
                    sprintf(dest,"mov r%d, pc",Rd);
                return;
            }
            break;
        }
        case 0xB:
        {
            switch((opcode>>9)&7)
            {
                case 0:
                    if(opcode & BIT(8)) break;
                    else
                    {
                        u16 offset = (opcode & 0x7F)<<2;
                        if(opcode & BIT(7))
                        {
                            //ADD  SP,#-nn
                            sprintf(dest,"add sp, #-0x%02X",offset);
                        }
                        else
                        {
                            //ADD  SP,#nn
                            sprintf(dest,"add sp, #0x%02X",offset);
                        }
                        return;
                    }
                case 1: break;
                case 2:
                {
                    //PUSH {Rlist}{LR}
                    u32 registers = (opcode & 0xFF) | (( opcode & BIT(8)) ? BIT(14) : 0 );
                    char reglist[128] = "{";
                    int i;
                    for(i = 0; i < 16; i++)
                    {
                        if(registers & BIT(i))
                        {
                            char reg[4]; sprintf(reg,"r%d",i);
                            strcat(reglist,reg);
                            if( (registers & BIT(i+1)) && (registers & BIT(i+2)) )
                            {
                                strcat(reglist,"-");
                                while(registers & BIT(i++));
                                i-=2;
                                sprintf(reg,"r%d",i);
                                strcat(reglist,reg);
                            }
                            int j = i+1;
                            while((registers & BIT(j))==0) { j++; if(j == 16) break; }
                            if(j < 16) strcat(reglist,",");
                        }
                    }
                    strcat(reglist,"}");
                    sprintf(dest,"push %s",reglist);
                    return;
                }
                case 3: break;
                case 4: break;
                case 5: break;
                case 6:
                {
                    //POP {Rlist}{PC}
                    u32 registers = (opcode & 0xFF) | (( opcode & BIT(8)) ? BIT(15) : 0 );
                    char reglist[128] = "{";
                    int count = 0;
                    int i;
                    for(i = 0; i < 16; i++)
                    {
                        if(registers & BIT(i))
                        {
                            count ++;
                            char reg[4]; sprintf(reg,"r%d",i);
                            strcat(reglist,reg);
                            if( (registers & BIT(i+1)) && (registers & BIT(i+2)) )
                            {
                                strcat(reglist,"-");
                                while(registers & BIT(i++));
                                i-=2;
                                if(i<16) sprintf(reg,"r%d",i);
                                else strcat(reglist,"pc");
                                strcat(reglist,reg);
                            }

                            int j = i+1;
                            while((registers & BIT(j))==0) { j++; if(j == 16) break; }
                            if(j < 16) strcat(reglist,",");
                        }
                    }
                    strcat(reglist,"}");
                    sprintf(dest,"pop %s",reglist);
                    if(count == 0) strcat(dest," ; [!] Undefined opcode!");
                    return;
                }
                case 7:
                    if((opcode & BIT(8)) == 0) { strcpy(dest,"[!] Undefined Opcode #B"); return; }
                    else break;
            }

            strcpy(dest,"[!] Undefined Opcode #B");
            return;
        }
        case 0xC:
        {
            char reglist[128] = "{";
            int i;
            for(i = 0; i < 8; i++)
            {
                if(opcode & BIT(i))
                {
                    char reg[4]; sprintf(reg,"r%d",i);
                    strcat(reglist,reg);
                    if( (opcode & BIT(i+1)) && (opcode & BIT(i+2)) )
                    {
                        strcat(reglist,"-");
                        while(opcode & BIT(i++));
                        i-=2;
                        sprintf(reg,"r%d",i);
                        strcat(reglist,reg);
                    }

                    int j = i+1;
                    while((opcode & BIT(j))==0) { j++; if(j == 8) break; }
                    if(j < 8) strcat(reglist,",");
                }
            }
            strcat(reglist,"}");

            u16 Rb = (opcode>>8)&7;

            if(opcode & BIT(11))
            {
                //LDMIA Rb!,{Rlist}
                sprintf(dest,"ldmia r%d!,%s",Rb,reglist);
                if((opcode&0xFF) == 0) strcat(dest," ; [!] Undefined opcode");
            }
            else
            {
                //STMIA Rb!,{Rlist}
                sprintf(dest,"stmia r%d!,%s",Rb,reglist);
                if((opcode&0xFF) == 0) strcat(dest," ; [!] Rb += 0x40");
            }
            return;
        }
        case 0xD:
        {
            u16 cond = opcode >> 8;
            u16 data = opcode & 0xFF;
            if(cond == 15)
            {
                //SWI nn
                sprintf(dest,"swi #0x%02X",data);
                gba_disassemble_swi_name(data,dest,1);
                return;
            }
            else if(cond == 14)
            {
                //Undefined opcode -- Tested in real hardware
                u32 addr_dest = address+4+ ( ((s16)(s8)data) << 1 );
                sprintf(dest,"b{al} #0x%08X ; [!] (Undefined Opcode #D)", addr_dest);
                if(addr_dest > address) strcat(dest," ; " STR_SLIM_ARROW_DOWN);
                else if(addr_dest == address) strcat(dest," ; <-");
                else strcat(dest," ; " STR_SLIM_ARROW_UP);
                return;
            }
            else
            {
                //B{cond} label
                u32 addr_dest = address+4+ ( ((s16)(s8)data) << 1 );
                sprintf(dest,"b%s #0x%08X",arm_cond[cond], addr_dest);
                if(addr_dest > address) strcat(dest," ; " STR_SLIM_ARROW_DOWN);
                else if(addr_dest == address) strcat(dest," ; <-");
                else strcat(dest," ; " STR_SLIM_ARROW_UP);
                gba_dissasemble_add_condition_met(cond,address,dest,0);
                return;
            }
            break;
        }
        case 0xE:
        {
            if(opcode & BIT(11))
            {
                strcpy(dest,"[!] Undefined Opcode #E");
                return;
            }
            else
            {
                //B label
                s32 offset = (opcode&0x3FF)<<1;
                if(offset & BIT(10)) { offset |= 0xFFFFF800; }
                u32 addr_dest = address+4+offset;
                sprintf(dest,"b #0x%08X",addr_dest);
                if(addr_dest > address) strcat(dest," ; "STR_SLIM_ARROW_DOWN);
                else if(addr_dest == address) strcat(dest," ; <-");
                else strcat(dest," ; "STR_SLIM_ARROW_UP);
                return;
            }
            break;
        }
        case 0xF:
        {
            //BL label
            if(opcode & BIT(11)) //Second part
            {
                u16 prev = GBA_MemoryRead16(address - 2);
                if((prev & 0xF800) == 0xF000)
                {
                    strcpy(dest,"bl (2nd part)");
                    return;
                }
                else
                {
                    strcpy(dest,"bl (2nd part) ; [!] corrupted!");
                    return;
                }
            }
            else //First part
            {
                u16 next = GBA_MemoryRead16(address + 2);
                if((next & 0xF800) == 0xF800)
                {
                    u32 addr = address + 4 + ((((u32)opcode & 0x7FF) << 12) | ((u32)opcode&BIT(10)?0xFF800000:0)) + ((next & 0x7FF) << 1);
                    sprintf(dest,"bl #0x%08X",addr);
                    strcat(dest," ; ->");
                    return;
                }
                else
                {
                    strcpy(dest,"bl (1st part) ; [!] corrupted!");
                    return;
                }
            }
            break;
        }
    }

    strcpy(dest,"Unknown Opcode.");
    return;
}

//--------------------------------------------------------------------------------------------------------
