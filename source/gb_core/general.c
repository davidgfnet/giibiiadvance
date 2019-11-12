// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (c) 2011-2015, 2019, Antonio Niño Díaz
//
// GiiBiiAdvance - GBA/GB emulator

#include <string.h>

#include "../build_options.h"
#include "../general_utils.h"
#include "../debug_utils.h"
#include "../config.h"

#include "gameboy.h"
#include "cpu.h"
#include "memory.h"
#include "sound.h"
#include "interrupts.h"
#include "sgb.h"
#include "serial.h"
#include "video.h"
#include "gb_main.h"
#include "camera.h"
#include "ppu.h"
#include "dma.h"
#include "mbc.h"

_GB_CONTEXT_ GameBoy;

void GB_PowerOn(void)
{
    GB_CPUInit(); // This goes first - It resets the clock counters of all subsystems
    GB_MemInit(); // This prepares the write/read function pointers - It must be called second
    GB_InterruptsInit();
    GB_SoundInit();
    GB_Screen_Init();
    GB_PPUInit();
    GB_SerialInit();
    GB_DMAInit();
    GB_MapperInit();

    if(GameBoy.Emulator.SGBEnabled) SGB_Init();
}

void GB_PowerOff(void)
{
    if(GameBoy.Emulator.SGBEnabled) SGB_End();

    GB_MapperEnd();
    GB_DMAEnd();
    GB_SerialEnd();
    GB_PPUEnd();
    GB_SoundEnd();
    GB_InterruptsEnd();
    GB_MemEnd();
    GB_CPUEnd();
}

void GB_HardReset(void)
{
    GB_PowerOff();

    GameBoy.Emulator.FrameDrawn = 1;

    GB_Screen_Init();

    if(GameBoy.Emulator.boot_rom_loaded)
    {
        GameBoy.Emulator.enable_boot_rom = 1;

        if(GameBoy.Emulator.gbc_in_gb_mode)
        {
            GameBoy.Emulator.gbc_in_gb_mode = 0;
            GameBoy.Emulator.CGBEnabled = 1;
            GameBoy.Emulator.DrawScanlineFn = &GBC_ScreenDrawScanline;
        }
    }

    GB_PowerOn();
}

int GB_EmulatorIsEnabledSGB(void)
{
    return GameBoy.Emulator.SGBEnabled;
}

int GB_RumbleEnabled(void)
{
    return GameBoy.Emulator.rumble;
}