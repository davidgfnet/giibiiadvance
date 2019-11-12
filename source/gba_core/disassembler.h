// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (c) 2011-2015, 2019, Antonio Niño Díaz
//
// GiiBiiAdvance - GBA/GB emulator

#ifndef __GBA_DISASSEMBLER__
#define __GBA_DISASSEMBLER__

#include "gba.h"

void GBA_DebugAddBreakpoint(u32 addr);
void GBA_DebugClearBreakpoint(u32 addr);
int GBA_DebugIsBreakpoint(u32 addr); // used in debugger
int GBA_DebugCPUIsBreakpoint(u32 addr); // used in CPU loop
void GBA_DebugClearBreakpointAll(void);

void GBA_DisassembleARM(u32 opcode, u32 address, char * dest, int dest_size);

void GBA_DisassembleTHUMB(u16 opcode, u32 address, char * dest, int dest_size);

#endif //__GBA_DISASSEMBLER__
