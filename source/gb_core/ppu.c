// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (c) 2011-2015, 2019, Antonio Niño Díaz
//
// GiiBiiAdvance - GBA/GB emulator

#include "../build_options.h"
#include "../debug_utils.h"

#include "cpu.h"
#include "debug.h"
#include "gameboy.h"
#include "gb_main.h"
#include "general.h"
#include "interrupts.h"
#include "memory.h"
#include "ppu.h"
#include "ppu_dmg.h"
#include "ppu_gbc.h"
#include "video.h"

//----------------------------------------------------------------

extern _GB_CONTEXT_ GameBoy;

//----------------------------------------------------------------

void GB_PPUInit(void)
{
    GameBoy.Emulator.FrameDrawn = 0;
    GameBoy.Emulator.stat_signal = 0;

    if (GameBoy.Emulator.enable_boot_rom)
    {
        GameBoy.Emulator.lcd_on = 0;
        GameBoy.Memory.IO_Ports[LCDC_REG - 0xFF00] = 0x00;

        GameBoy.Memory.IO_Ports[SCY_REG - 0xFF00] = 0x00;
        GameBoy.Memory.IO_Ports[SCX_REG - 0xFF00] = 0x00;

        GameBoy.Memory.IO_Ports[LYC_REG - 0xFF00] = 0x00;

        GameBoy.Memory.IO_Ports[BGP_REG - 0xFF00] = 0x00;
        GameBoy.Memory.IO_Ports[OBP0_REG - 0xFF00] = 0x00;
        GameBoy.Memory.IO_Ports[OBP1_REG - 0xFF00] = 0x00;

        GameBoy.Memory.IO_Ports[WY_REG - 0xFF00] = 0x00;
        GameBoy.Memory.IO_Ports[WX_REG - 0xFF00] = 0x00;

        switch (GameBoy.Emulator.HardwareType)
        {
            case HW_GB:
            case HW_GBP: // Not verified yet
            {
                GameBoy.Emulator.ly_clocks = 0;
                GameBoy.Emulator.ScreenMode = 1;
                GameBoy.Emulator.CurrentScanLine = 0;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode;
                break;
            }
            case HW_SGB:
            case HW_SGB2: // Unknown. Can't test.
            {
                GameBoy.Emulator.ly_clocks = 0;
                GameBoy.Emulator.ScreenMode = 1;
                GameBoy.Emulator.CurrentScanLine = 0;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode;
                break;
            }
            case HW_GBC:
            case HW_GBA:
            case HW_GBA_SP: // Not verified yet
            {
                GameBoy.Emulator.ly_clocks = 0;
                GameBoy.Emulator.ScreenMode = 1;
                GameBoy.Emulator.CurrentScanLine = 0;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode;
                break;
            }
            default:
            {
                GameBoy.Emulator.ly_clocks = 0;
                GameBoy.Emulator.ScreenMode = 1;
                GameBoy.Emulator.CurrentScanLine = 0;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode;
                Debug_ErrorMsg("GB_PPUInit():\n"
                               "Unknown hardware");
            }
        }
    }
    else
    {
        GameBoy.Emulator.lcd_on = 1;
        GameBoy.Memory.IO_Ports[LCDC_REG - 0xFF00] = 0x91;

        GameBoy.Memory.IO_Ports[SCY_REG - 0xFF00] = 0x00;
        GameBoy.Memory.IO_Ports[SCX_REG - 0xFF00] = 0x00;

        // Verified on hardware
        GameBoy.Memory.IO_Ports[LYC_REG - 0xFF00] = 0x00;

        GameBoy.Memory.IO_Ports[BGP_REG - 0xFF00] = 0xFC;
        GameBoy.Memory.IO_Ports[OBP0_REG - 0xFF00] = 0xFF;
        GameBoy.Memory.IO_Ports[OBP1_REG - 0xFF00] = 0xFF;

        GameBoy.Memory.IO_Ports[WY_REG - 0xFF00] = 0x00;
        GameBoy.Memory.IO_Ports[WX_REG - 0xFF00] = 0x00;

        switch (GameBoy.Emulator.HardwareType)
        {
            case HW_GB:
            case HW_GBP:
            {
                GameBoy.Emulator.ly_clocks = (456 - 8); // Not verified yet
                GameBoy.Emulator.ScreenMode = 1;
                GameBoy.Emulator.CurrentScanLine = 0;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode | BIT(2);
                break;
            }
            case HW_SGB:
            case HW_SGB2: // Unknown. Can't test.
            {
                GameBoy.Emulator.ly_clocks = 0;
                GameBoy.Emulator.ScreenMode = 1;
                GameBoy.Emulator.CurrentScanLine = 0;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode;
                break;
            }
            case HW_GBC:
            case HW_GBA:
            case HW_GBA_SP: // Not verified yet
            {
                GameBoy.Emulator.ly_clocks = 0;
                GameBoy.Emulator.ScreenMode = 1;
                GameBoy.Emulator.CurrentScanLine = 0x90;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode;
                break;
            }
            default:
            {
                GameBoy.Emulator.ly_clocks = 0;
                GameBoy.Emulator.ScreenMode = 0;
                GameBoy.Emulator.CurrentScanLine = 0;
                GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] =
                        GameBoy.Emulator.ScreenMode;
                Debug_ErrorMsg("GB_PPUInit():\n"
                               "Unknown hardware");
            }
        }
    }

    GameBoy.Memory.IO_Ports[LY_REG - 0xFF00] = GameBoy.Emulator.CurrentScanLine;

    switch (GameBoy.Emulator.HardwareType)
    {
        case HW_GB:
        case HW_GBP:
        case HW_SGB:
        case HW_SGB2:
        {
            GameBoy.Emulator.PPUUpdate = GB_PPUUpdateClocks_DMG;
            GameBoy.Emulator.PPUClocksToNextEvent =
                    GB_PPUGetClocksToNextEvent_DMG;
            break;
        }
        default:
            // Previous switches will show an error message, don't show it in
            // this switch too, just fall through.
        case HW_GBC:
        case HW_GBA:
        case HW_GBA_SP: // Not verified yet
        {
            GameBoy.Emulator.PPUUpdate = GB_PPUUpdateClocks_GBC;
            GameBoy.Emulator.PPUClocksToNextEvent =
                    GB_PPUGetClocksToNextEvent_GBC;
            break;
        }
    }
}

void GB_PPUEnd(void)
{
    // Nothing to do
}

//----------------------------------------------------------------

static int gb_ppu_clock_counter = 0;

void GB_PPUClockCounterReset(void)
{
    gb_ppu_clock_counter = 0;
}

static int GB_PPUClockCounterGet(void)
{
    return gb_ppu_clock_counter;
}

static void GB_PPUClockCounterSet(int new_reference_clocks)
{
    gb_ppu_clock_counter = new_reference_clocks;
}

void GB_PPUUpdateClocksCounterReference(int reference_clocks)
{
    int increment_clocks = reference_clocks - GB_PPUClockCounterGet();

    if (GameBoy.Emulator.lcd_on)
    {
        GameBoy.Emulator.PPUUpdate(increment_clocks);
    }

    GB_PPUClockCounterSet(reference_clocks);
}

int GB_PPUGetClocksToNextEvent(void)
{
    return GameBoy.Emulator.PPUClocksToNextEvent();
}

//----------------------------------------------------------------

void GB_PPUCheckStatSignal(void)
{
    if (GameBoy.Emulator.lcd_on == 0)
    {
        GameBoy.Emulator.stat_signal = 0;
        return;
    }

    _GB_MEMORY_ *mem = &GameBoy.Memory;
    u32 screenmode = GameBoy.Emulator.ScreenMode;
    int stat = mem->IO_Ports[STAT_REG - 0xFF00];

    u8 ly = mem->IO_Ports[LY_REG - 0xFF00];
    u8 lyc = mem->IO_Ports[LYC_REG - 0xFF00];

    int any_condition_met =
            ((ly == lyc) && (stat & IENABLE_LY_COMPARE))
            || ((screenmode == 0) && (stat & IENABLE_HBL))
            || ((screenmode == 2) && (stat & IENABLE_OAM))
            || ((screenmode == 1) && (stat & (IENABLE_VBL | IENABLE_OAM)));
    // Note: For mode 1, don't just check IENABLE_VBL. Check also IENABLE_OAM.

    if (any_condition_met)
    {
        if (GameBoy.Emulator.stat_signal == 0) // Rising edge
        {
            GB_InterruptsSetFlag(I_STAT);
        }

        GameBoy.Emulator.stat_signal = 1;
    }
    else
    {
        GameBoy.Emulator.stat_signal = 0;
    }
}

void GB_PPUCheckLYC(void)
{
    if (GameBoy.Emulator.lcd_on)
    {
        u8 ly = GameBoy.Memory.IO_Ports[LY_REG - 0xFF00];
        u8 lyc = GameBoy.Memory.IO_Ports[LYC_REG - 0xFF00];
        if (ly == lyc)
            GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] |= I_LY_EQUALS_LYC;
        else
            GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] &= ~I_LY_EQUALS_LYC;
    }
    else
    {
        GameBoy.Memory.IO_Ports[STAT_REG - 0xFF00] &= ~I_LY_EQUALS_LYC;
    }
}
