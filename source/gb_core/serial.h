// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (c) 2011-2015, 2019, Antonio Niño Díaz
//
// GiiBiiAdvance - GBA/GB emulator

#ifndef __GB_SERIAL__
#define __GB_SERIAL__

//--------------------------------------------------------------------------------

void GB_SerialClockCounterReset(void);
void GB_SerialUpdateClocksCounterReference(int reference_clocks);
int GB_SerialGetClocksToNextEvent(void);

//--------------------------------------------------------------------------------

void GB_SerialWriteSB(int reference_clocks, int value);
void GB_SerialWriteSC(int reference_clocks, int value);

//--------------------------------------------------------------------------------

void GB_SerialInit(void);
void GB_SerialEnd(void);

//--------------------------------------------------------------------------------

void GB_SerialPlug(int device);

//--------------------------------------------------------------------------------

#endif //__GB_SERIAL__
