// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (c) 2011-2015, 2019, Antonio Niño Díaz
//
// GiiBiiAdvance - GBA/GB emulator

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../build_options.h"
#include "../config.h"
#include "../debug_utils.h"
#include "../file_utils.h"
#include "../general_utils.h"

#include "debug.h"
#include "gameboy.h"
#include "gb_main.h"
#include "general.h"
#include "licensees.h"
#include "mbc.h"
#include "rom.h"
#include "video.h"

// Seen:
//
// 00 - 01 - 02 - 03 - 06 - 0B - 0D - 10 - 11 - 13 - 19 - 1A - 1B - 1C - 1E - 20
// 22 - 97 - 99 - BE - EA - FC - FD - FE - FF
//
// Others are bad dumps. In fact, some of those are bad dumps or strange ROMs...

// If a cart type is in parenthesis, I haven't seen any game that uses it, but
// it should use that controller. Cartridges with "???" have been seen, but
// there is not much documentation about it... Probably just bad dumps.

static const char *gb_memorycontrollers[256] = {
    "ROM ONLY", "MBC1", "MBC1+RAM", "MBC1+RAM+BATTERY",
    "Unknown", "(MBC2)", "MBC2+BATTERY", "Unknown",
    "(ROM+RAM) ", "(ROM+RAM+BATTERY)", "Unknown", "MMM01",
    "(MMM01+RAM)", "MMM01+RAM+BATTERY", "Unknown", "(MBC3+TIMER+BATTERY)",
    "MBC3+TIMER+RAM+BATTERY", "MBC3", "(MBC3+RAM)", "MBC3+RAM+BATTERY",
    "Unknown", "(MBC4)", "(MBC4+RAM)", "(MBC4+RAM+BATTERY)",
    "Unknown", "MBC5", "MBC5+RAM", "MBC5+RAM+BATTERY",
    "MBC5+RUMBLE", "(MBC5+RUMBLE+RAM)", "MBC5+RUMBLE+RAM+BATTERY", "Unknown",
    "MBC6+RAM+BATTERY ???", "Unknown", "MBC7+RAM+BATTERY ???", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",

    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",

    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", " ??? ",
    "Unknown", " ??? ", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", " ??? ", "Unknown",

    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", " ??? ", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "CAMERA", "BANDAI TAMA5", "HuC3", "HuC1+RAM+BATTERY"
};

static const u8 gb_nintendo_logo[48] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

extern _GB_CONTEXT_ GameBoy;

static int showconsole = 0;

int GB_ShowConsoleRequested(void)
{
    int ret = showconsole;
    showconsole = 0;
    return ret;
}

int GB_CartridgeLoad(const u8 *pointer, const u32 rom_size)
{
    showconsole = 0; // If at the end this is 1, show console window

    ConsoleReset();

    _GB_ROM_HEADER_ *GB_Header;

    GB_Header = (void *)pointer;

    ConsolePrint("Checking cartridge...\n");

    int k;
    for (k = 0; k < 11; k++)
        GameBoy.Emulator.Title[k] = GB_Header->title[k];
    for (; k < 15; k++)
        GameBoy.Emulator.Title[k] = GB_Header->manufacturer[k - 11];

    GameBoy.Emulator.Title[15] = GB_Header->cgb_flag;
    GameBoy.Emulator.Title[16] = '\0';

    if (GB_Header->old_licensee == 0x33)
        GameBoy.Emulator.Title[12] = '\0';

    ConsolePrint("Game title: %s\n", GameBoy.Emulator.Title);

    char *dest_name;
    switch (GB_Header->dest_code)
    {
        case 0x00:
            dest_name = "Japan";
            break;
        case 0x01:
            dest_name = "Non-Japan";
            break;
        default:
            dest_name = "Unknown";
            break;
    }
    ConsolePrint("Destination: %s (%02X)\n", dest_name, GB_Header->dest_code);

    if (GB_Header->old_licensee == 0x33)
    {
        char byte1 = isprint(GB_Header->new_licensee[0]) ?
                     GB_Header->new_licensee[0] : '.';
        char byte2 = isprint(GB_Header->new_licensee[1]) ?
                     GB_Header->new_licensee[1] : '.';
        ConsolePrint("Licensee (new): %s (%c%c)\n",
                     GB_GetLicenseeName(GB_Header->new_licensee[0],
                                        GB_Header->new_licensee[1]),
                     byte1, byte2);
    }
    else
    {
        // In order to have a nice list with all licensees (old and new),
        // both bytes are converted from hex (A-F, 0-9) to ASCII ('A'-'F',
        // '0', '9')
        char byte1 = (GB_Header->old_licensee >> 4) & 0x0F;
        byte1 += (byte1 < 10) ? '0' : ('A' - 10);
        char byte2 = GB_Header->old_licensee & 0x0F;
        byte2 += (byte2 < 10) ? '0' : ('A' - 10);
        ConsolePrint("Licensee (old): %s (%02X)\n",
                     GB_GetLicenseeName(byte1, byte2), GB_Header->old_licensee);
    }

    ConsolePrint("Rom version: %02X\n", GB_Header->rom_version);

    // This will check which possible GB models can run the game
    // ---------------------------------------------------------

    ConsolePrint("GBC flag = %02X\n", GB_Header->cgb_flag);

    int enable_gb = 0;
    int enable_sgb = 0;
    int enable_gbc = 0;

    GameBoy.Emulator.game_supports_gbc = 0;

    // Color
    if (GB_Header->cgb_flag & (1 << 7))
    {
        if (GB_Header->cgb_flag == 0xC0) // GBC only
        {
            enable_gbc = 1;
            GameBoy.Emulator.game_supports_gbc = 1;
        }
        else if (GB_Header->cgb_flag == 0x80) // GBC or GB
        {
            enable_gbc = 1;
            enable_gb = 1;
            GameBoy.Emulator.game_supports_gbc = 1;
        }
        else // Unknown
        {
            enable_gb = 1;
            enable_gbc = 1;
            GameBoy.Emulator.game_supports_gbc = 1;

            ConsolePrint("[!]Unknown GBC flag...\n");
            if (EmulatorConfig.debug_msg_enable)
                showconsole = 1;
        }
    }
    else // GB only
    {
        enable_gb = 1;
    }

    // SGB
    if ((GB_Header->sgb_flag == 0x03) && (GB_Header->old_licensee == 0x33))
    {
        enable_sgb = 1;
    }

    GameBoy.Emulator.selected_hardware = EmulatorConfig.hardware_type;

    if (GameBoy.Emulator.selected_hardware == -1) // Auto
    {
        if (enable_gbc == 1)
            GameBoy.Emulator.HardwareType = HW_GBC;
        else if (enable_sgb == 1)
            GameBoy.Emulator.HardwareType = HW_SGB;
        else if (enable_gb == 1)
            GameBoy.Emulator.HardwareType = HW_GB;
        else
            GameBoy.Emulator.HardwareType = HW_GBC; // ?
    }
    else
    {
        // Force mode
        GameBoy.Emulator.HardwareType = GameBoy.Emulator.selected_hardware;
    }

    switch (GameBoy.Emulator.HardwareType)
    {
        case HW_GB:
            ConsolePrint("Loading in GB mode...\n");
            break;
        case HW_GBP:
            ConsolePrint("Loading in GBP mode...\n");
            break;
        case HW_SGB:
            ConsolePrint("Loading in SGB mode...\n");
            break;
        case HW_SGB2:
            ConsolePrint("Loading in SGB2 mode...\n");
            break;
        case HW_GBC:
            ConsolePrint("Loading in GBC mode...\n");
            break;
        case HW_GBA:
            ConsolePrint("Loading in GBA mode...\n");
            break;
        case HW_GBA_SP:
            ConsolePrint("Loading in GBA SP mode...\n");
            break;
        default: // Should never happen
            Debug_ErrorMsgArg("%s(): Trying to load in an undefined mode!",
                              __func__);
            return 0;
    }

    GameBoy.Emulator.gbc_in_gb_mode = 0;

    if ((GameBoy.Emulator.HardwareType == HW_GB)
        || (GameBoy.Emulator.HardwareType == HW_GBP))
    {
        //Video_EnableBlur(true);
        GameBoy.Emulator.CGBEnabled = 0;
        GameBoy.Emulator.SGBEnabled = 0;
        GameBoy.Emulator.DrawScanlineFn = &GB_ScreenDrawScanline;
    }
    else if ((GameBoy.Emulator.HardwareType == HW_SGB)
             || (GameBoy.Emulator.HardwareType == HW_SGB2))
    {
        //Video_EnableBlur(false);
        GameBoy.Emulator.CGBEnabled = 0;
        GameBoy.Emulator.SGBEnabled = 1;
        GameBoy.Emulator.DrawScanlineFn = &SGB_ScreenDrawScanline;
    }
    else if (GameBoy.Emulator.HardwareType == HW_GBC)
    {
        //Video_EnableBlur(true);
        GameBoy.Emulator.CGBEnabled = 1;
        GameBoy.Emulator.SGBEnabled = 0;
        GameBoy.Emulator.DrawScanlineFn = &GBC_ScreenDrawScanline;
    }
    else if ((GameBoy.Emulator.HardwareType == HW_GBA)
             || (GameBoy.Emulator.HardwareType == HW_GBA_SP))
    {
        //Video_EnableBlur(false);
        GameBoy.Emulator.CGBEnabled = 1;
        GameBoy.Emulator.SGBEnabled = 0;
        GameBoy.Emulator.DrawScanlineFn = &GBC_ScreenDrawScanline;
    }

    GameBoy.Emulator.enable_boot_rom = 0;
    GameBoy.Emulator.boot_rom_loaded = 0;

    if (EmulatorConfig.load_from_boot_rom)
    {
        // Load boot rom if any...
        char *boot_rom_filename = NULL;

        switch (GameBoy.Emulator.HardwareType)
        {
            case HW_GB:
                boot_rom_filename = DMG_ROM_FILENAME;
                break;
            case HW_GBP:
                boot_rom_filename = MGB_ROM_FILENAME;
                break;
            case HW_SGB:
                boot_rom_filename = SGB_ROM_FILENAME;
                break;
            case HW_SGB2:
                boot_rom_filename = SGB2_ROM_FILENAME;
                break;
            case HW_GBC:
                boot_rom_filename = CGB_ROM_FILENAME;
                break;
            case HW_GBA:
                boot_rom_filename = AGB_ROM_FILENAME;
                break;
            case HW_GBA_SP:
                boot_rom_filename = AGS_ROM_FILENAME;
                break;
            default:
                break;
        }

        if (boot_rom_filename != NULL)
        {
            if (DirGetBiosFolderPath())
            {
                int size = strlen(DirGetBiosFolderPath())
                           + strlen(boot_rom_filename) + 2;
                char *completepath = malloc(size);
                snprintf(completepath, size, "%s%s", DirGetBiosFolderPath(),
                         boot_rom_filename);
                FILE *test = fopen(completepath, "rb");
                if (test)
                {
                    fclose(test);

                    FileLoad(completepath, (void *)&GameBoy.Emulator.boot_rom,
                             NULL);

                    if (GameBoy.Emulator.boot_rom)
                    {
                        ConsolePrint("Boot ROM loaded from %s!\n",
                                     boot_rom_filename);
                        GameBoy.Emulator.enable_boot_rom = 1;
                        GameBoy.Emulator.boot_rom_loaded = 1;
                    }
                }
                free(completepath);
            }
        }
    }

    GameBoy.Emulator.HasBattery = 0;
    GameBoy.Emulator.HasTimer = 0;

    ConsolePrint("Cartridge type: %02X - %s\n", GB_Header->cartridge_type,
                 gb_memorycontrollers[GB_Header->cartridge_type]);

    GameBoy.Emulator.EnableBank0Switch = 0;
    GameBoy.Memory.mbc_mode = 0;

    // Cartridge type
    switch (GB_Header->cartridge_type)
    {
        case 0x00: // ROM ONLY
            GameBoy.Emulator.MemoryController = MEM_NONE;
            break;
        case 0x01: // MBC1
            GameBoy.Emulator.MemoryController = MEM_MBC1;
            break;
        case 0x02: // MBC1+RAM
            GameBoy.Emulator.MemoryController = MEM_MBC1;
            break;
        case 0x03: // MBC1+RAM+BATTERY
            GameBoy.Emulator.MemoryController = MEM_MBC1;
            GameBoy.Emulator.HasBattery = 1;
            break;
        case 0x05: // MBC2
            GameBoy.Emulator.MemoryController = MEM_MBC2;
            break;
        case 0x06: // MBC2+BATTERY
            GameBoy.Emulator.MemoryController = MEM_MBC2;
            GameBoy.Emulator.HasBattery = 1;
            break;
        case 0x08: // ROM+RAM
            GameBoy.Emulator.MemoryController = MEM_NONE;
            break;
        case 0x09: // ROM+RAM+BATTERY
            GameBoy.Emulator.MemoryController = MEM_NONE;
            break;
        case 0x0B: // MMM01
            GameBoy.Emulator.MemoryController = MEM_MMM01;
            GameBoy.Emulator.EnableBank0Switch = 1;
            break;
        case 0x0C: // MMM01+RAM // I've never seen a game that uses this...
            GameBoy.Emulator.MemoryController = MEM_MMM01;
            GameBoy.Emulator.EnableBank0Switch = 1;
            break;
        case 0x0D: // MMM01+RAM+BATTERY
            GameBoy.Emulator.MemoryController = MEM_MMM01;
            GameBoy.Emulator.HasBattery = 1;
            GameBoy.Emulator.EnableBank0Switch = 1;
            break;
        case 0x0F: // MBC3+TIMER+BATTERY
            GameBoy.Emulator.MemoryController = MEM_MBC3;
            GameBoy.Emulator.HasBattery = 1;
            GameBoy.Emulator.HasTimer = 1;
            break;
        case 0x10: // MBC3+TIMER+RAM+BATTERY
            GameBoy.Emulator.MemoryController = MEM_MBC3;
            GameBoy.Emulator.HasBattery = 1;
            GameBoy.Emulator.HasTimer = 1;
            break;
        case 0x11: // MBC3
            GameBoy.Emulator.MemoryController = MEM_MBC3;
            break;
        case 0x12: // MBC3+RAM
            GameBoy.Emulator.MemoryController = MEM_MBC3;
            break;
        case 0x13: // MBC3+RAM+BATTERY
            GameBoy.Emulator.MemoryController = MEM_MBC3;
            GameBoy.Emulator.HasBattery = 1;
            break;
        case 0x19: // MBC5
            GameBoy.Emulator.MemoryController = MEM_MBC5;
            break;
        case 0x1A: // MBC5+RAM
            GameBoy.Emulator.MemoryController = MEM_MBC5;
            break;
        case 0x1B: // MBC5+RAM+BATTERY
            GameBoy.Emulator.MemoryController = MEM_MBC5;
            GameBoy.Emulator.HasBattery = 1;
            break;
        case 0x1C: // MBC5+RUMBLE
            GameBoy.Emulator.MemoryController = MEM_RUMBLE;
            break;
        case 0x1D: // MBC5+RUMBLE+RAM
            GameBoy.Emulator.MemoryController = MEM_RUMBLE;
            break;
        case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
            GameBoy.Emulator.MemoryController = MEM_RUMBLE;
            GameBoy.Emulator.HasBattery = 1;
            break;
        case 0x20: // MBC6+RAM+BATTERY ???
            GameBoy.Emulator.MemoryController = MEM_MBC6;
            GameBoy.Emulator.HasBattery = 1;
            break;
        case 0x22: // MBC7+RAM+BATTERY ???
            GameBoy.Emulator.MemoryController = MEM_MBC7;
            GameBoy.Emulator.HasBattery = 1;
            break;
        case 0xFC: // POCKET CAMERA
            GameBoy.Emulator.MemoryController = MEM_CAMERA;
            GameBoy.Emulator.HasBattery = 1;
            break;
#if 0
        case 0xFD: // BANDAI TAMA5
            GameBoy.Emulator.MemoryController = MEM_TAMA5;
            break;
        case 0xFE: // HuC3
            GameBoy.Emulator.MemoryController = MEM_HUC3;
            break;
#endif
        case 0xFF: // HuC1+RAM+BATTERY (MBC1-like + IR PORT)
            GameBoy.Emulator.MemoryController = MEM_HUC1;
            GameBoy.Emulator.HasBattery = 1;
            break;
        default:
            ConsolePrint("[!]UNSUPPORTED CARTRIDGE\n");
            showconsole = 1;
            return 0;
    }

    GB_MapperSet(GameBoy.Emulator.MemoryController);

    // RAM
    switch (GB_Header->ram_size)
    {
        case 0x00: // No RAM
            GameBoy.Emulator.RAM_Banks = 0;
            break;
        case 0x01: // 2KB
            GameBoy.Emulator.RAM_Banks = 1;
            break;
        case 0x02: // 8KB
            GameBoy.Emulator.RAM_Banks = 1;
            break;
        case 0x03: // 4 * 8KB
            GameBoy.Emulator.RAM_Banks = 4;
            break;
        case 0x04: // 16 * 8KB
            GameBoy.Emulator.RAM_Banks = 16;
            break;
        case 0x05: // 8 * 8KB -> "Pocket Monsters - Crystal Version (Japan)"
            GameBoy.Emulator.RAM_Banks = 8;
            break;
        default:
            ConsolePrint("[!]RAM SIZE UNKNOWN: %02X\n", GB_Header->ram_size);
            showconsole = 1;
            return 0;
    }

    if (GameBoy.Emulator.MemoryController == MEM_MBC2)
        GameBoy.Emulator.RAM_Banks = 1; // 512 * 4 bits

    if (GameBoy.Emulator.MemoryController == MEM_MBC7)
        GameBoy.Emulator.RAM_Banks = 1;

    if (GameBoy.Emulator.MemoryController == MEM_CAMERA)
    {
        // In case any other software uses GB Camera...
        if (GameBoy.Emulator.RAM_Banks < 1)
            GameBoy.Emulator.RAM_Banks = 1; // This shouldn't be needed.
    }

    ConsolePrint("RAM size %02X -- %d banks\n", GB_Header->ram_size,
                 GameBoy.Emulator.RAM_Banks);

    // ROM
    switch (GB_Header->rom_size)
    {
        case 0x00:
            GameBoy.Emulator.ROM_Banks = 2;
            break;
        case 0x01:
            GameBoy.Emulator.ROM_Banks = 4;
            break;
        case 0x02:
            GameBoy.Emulator.ROM_Banks = 8;
            break;
        case 0x03:
            GameBoy.Emulator.ROM_Banks = 16;
            break;
        case 0x04:
            GameBoy.Emulator.ROM_Banks = 32;
            break;
        case 0x05:
            GameBoy.Emulator.ROM_Banks = 64;
            break;
        case 0x06:
            GameBoy.Emulator.ROM_Banks = 128;
            break;
        case 0x07:
            GameBoy.Emulator.ROM_Banks = 256;
            break;
        case 0x08:
            GameBoy.Emulator.ROM_Banks = 512;
            break;
        default:
            ConsolePrint("[!]ROM SIZE UNKNOWN: %02X\n", GB_Header->rom_size);
            showconsole = 1;
            return 0;
    }

    ConsolePrint("ROM size %02X -- %d banks\n", GB_Header->rom_size,
                 GameBoy.Emulator.ROM_Banks);

    if (rom_size != (GameBoy.Emulator.ROM_Banks * 16 * (1024)))
    {
        if (EmulatorConfig.debug_msg_enable)
            showconsole = 1;

        ConsolePrint("[!]ROM file size incorrect!\n"
                     "File size is %d B (%d KB), header says it is %d KB.\n",
                     rom_size, rom_size / 1024,
                     (GameBoy.Emulator.ROM_Banks * 16));

        if (rom_size < (GameBoy.Emulator.ROM_Banks * 16 * (1024)))
        {
            ConsolePrint("[!]File is smaller than the size the header says.\n"
                         "Aborting...");
            showconsole = 1;
            return 0;
        }
    }

    // Checksums
    // ---------

    // Header checksum
    u32 sum = 0;
    u32 count;
    for (count = 0x0134; count <= 0x014C; count++)
        sum = sum - pointer[count] - 1;

    sum &= 0xFF;

    ConsolePrint("Header checksum: %02X - Obtained: %02X\n",
                 GB_Header->header_checksum, sum);

    if (GB_Header->header_checksum != sum)
    {
        ConsolePrint("[!]INCORRECT! - Maybe a bad dump?\n"
                     "[!]Game wouldn't work in a real GB.\n");
        if (EmulatorConfig.debug_msg_enable)
            showconsole = 1;
    }

    // Global checksum
    u32 size = GameBoy.Emulator.ROM_Banks * 16 * 1024;
    sum = 0;
    for (count = 0; count < size; count++)
        sum += (u32)pointer[count];

    sum -= GB_Header->global_checksum & 0xFF; // Checksum bytes not included
    sum -= (GB_Header->global_checksum >> 8) & 0xFF;

    sum &= 0xFFFF;
    sum = ((sum >> 8) & 0x00FF) | ((sum << 8) & 0xFF00);

    ConsolePrint("Global checksum: %04X - Obtained: %04X\n",
                 GB_Header->global_checksum, sum);

    if (GB_Header->global_checksum != sum)
    {
        ConsolePrint("[!]INCORRECT! - Maybe a bad dump?\n");
        if (EmulatorConfig.debug_msg_enable)
            showconsole = 1;
    }

    ConsolePrint("Checking Nintendo logo... ");

    if (memcmp(GB_Header->nintendologo, gb_nintendo_logo,
               sizeof(gb_nintendo_logo)) == 0)
    {
        ConsolePrint("Correct!\n");
    }
    else
    {
        ConsolePrint("\n"
                     "[!]INCORRECT! - Maybe a bad dump?\n"
                     "[!]Game wouldn't work in a real GB.\n");
        if (EmulatorConfig.debug_msg_enable)
            showconsole = 1;
    }

    // SAVE LOCATION
    GameBoy.Emulator.Rom_Pointer = (void *)pointer;

    // Prepare Game Boy palettes

    if (GameBoy.Emulator.HardwareType == HW_GB)
        GB_ConfigLoadPalette();
    else if (GameBoy.Emulator.HardwareType == HW_GBP)
        GB_SetPalette(0xFF, 0xFF, 0xFF);

    ConsolePrint("Done!\n");

    return 1;
}

void GB_Cartridge_Unload(void)
{
    if (GameBoy.Emulator.boot_rom_loaded)
    {
        free(GameBoy.Emulator.boot_rom);
        GameBoy.Emulator.boot_rom = NULL;
        GameBoy.Emulator.boot_rom_loaded = 0;
        GameBoy.Emulator.enable_boot_rom = 0;
    }

    free(GameBoy.Emulator.Rom_Pointer);
}

void GB_Cardridge_Set_Filename(char *filename)
{
    u32 len = strlen(filename);

    if (GameBoy.Emulator.save_filename)
    {
        s_strncpy(GameBoy.Emulator.save_filename, filename,
                  sizeof(GameBoy.Emulator.save_filename));
    }

    len--;

    while (1)
    {
        if (GameBoy.Emulator.save_filename[len] == '.')
        {
            GameBoy.Emulator.save_filename[len] = '\0';
            return;
        }

        len--;

        if (len == 0)
            break;
    }
}

//--------------------------------------------------------------------------

void GB_RTC_Save(FILE *savefile)
{
    if (GameBoy.Emulator.HasTimer == 0)
        return;

    time_t current_time = time(NULL);

    int error = 0;

    // Time

    if (fwrite(&GameBoy.Emulator.Timer.sec, 1, 4, savefile) != 4)
        error = 1;
    if (fwrite(&GameBoy.Emulator.Timer.min, 1, 4, savefile) != 4)
        error = 1;
    if (fwrite(&GameBoy.Emulator.Timer.hour, 1, 4, savefile) != 4)
        error = 1;

    u32 days_low = GameBoy.Emulator.Timer.days & 0xFF;
    u32 days_hi = (GameBoy.Emulator.Timer.days >> 8)
                  | (GameBoy.Emulator.Timer.halt << 6)
                  | (GameBoy.Emulator.Timer.carry << 7);

    if (fwrite(&days_low, 1, 4, savefile) != 4)
        error = 1;
    if (fwrite(&days_hi, 1, 4, savefile) != 4)
        error = 1;

    // Latched time

    if (fwrite(&GameBoy.Emulator.LatchedTime.sec, 1, 4, savefile) != 4)
        error = 1;
    if (fwrite(&GameBoy.Emulator.LatchedTime.min, 1, 4, savefile) != 4)
        error = 1;
    if (fwrite(&GameBoy.Emulator.LatchedTime.hour, 1, 4, savefile) != 4)
        error = 1;

    days_low = GameBoy.Emulator.LatchedTime.days & 0xFF;
    days_hi = (GameBoy.Emulator.LatchedTime.days >> 8)
              | (GameBoy.Emulator.LatchedTime.halt << 6)
              | (GameBoy.Emulator.LatchedTime.carry << 7);

    if (fwrite(&days_low, 1, 4, savefile) != 4)
        error = 1;
    if (fwrite(&days_hi, 1, 4, savefile) != 4)
        error = 1;

    // Timestamp

    u32 timestamp_low, timestamp_hi;

    if (sizeof(time_t) == 4)
    {
        timestamp_low = current_time;
        timestamp_hi = 0;
    }
    else if (sizeof(time_t) == 8)
    {
        timestamp_low = current_time;
        timestamp_hi = (current_time >> 32); // TODO: Remove warning?
    }
    else // ????
    {
        // TODO: Add assert
        //Debug_ErrorMsg("Invalid size of time_t.");
        timestamp_low = current_time;
        timestamp_hi = (current_time >> 32); // TODO: Remove warning?
    }

    if (fwrite(&timestamp_low, 1, 4, savefile) != 4)
        error = 1;
    if (fwrite(&timestamp_hi, 1, 4, savefile) != 4)
        error = 1;

    if (error)
        Debug_ErrorMsgArg("Error while saving RTC data!");
}

void GB_RTC_Load(FILE *savefile)
{
    if (GameBoy.Emulator.HasTimer == 0)
        return;

    u64 current_time = (u64)time(NULL);
    u64 old_time;

    ConsolePrint("Loading RTC data... ");

    int error = 0;
    u32 days_low, days_hi;

    // Time

    if (fread(&GameBoy.Emulator.Timer.sec, 1, 4, savefile) != 4)
        error = 1;
    if (fread(&GameBoy.Emulator.Timer.min, 1, 4, savefile) != 4)
        error = 1;
    if (fread(&GameBoy.Emulator.Timer.hour, 1, 4, savefile) != 4)
        error = 1;

    if (fread(&days_low, 1, 4, savefile) != 4)
        error = 1;
    if (fread(&days_hi, 1, 4, savefile) != 4)
        error = 1;

    GameBoy.Emulator.Timer.days = (days_low & 0xFF) | ((days_hi & 0x01) << 8);
    GameBoy.Emulator.Timer.halt = (days_hi & (1 << 6)) >> 6;
    GameBoy.Emulator.Timer.carry = (days_hi & (1 << 7)) >> 7;

    // Latched time

    if (fread(&GameBoy.Emulator.LatchedTime.sec, 1, 4, savefile) != 4)
        error = 1;
    if (fread(&GameBoy.Emulator.LatchedTime.min, 1, 4, savefile) != 4)
        error = 1;
    if (fread(&GameBoy.Emulator.LatchedTime.hour, 1, 4, savefile) != 4)
        error = 1;

    if (fread(&days_low, 1, 4, savefile) != 4)
        error = 1;
    if (fread(&days_hi, 1, 4, savefile) != 4)
        error = 1;

    GameBoy.Emulator.LatchedTime.days = (days_low & 0xFF)
                                        | ((days_hi & 0x01) << 8);
    GameBoy.Emulator.LatchedTime.halt = (days_hi & (1 << 6)) >> 6;
    GameBoy.Emulator.LatchedTime.carry = (days_hi & (1 << 7)) >> 7;

    // Timestamp

    u32 timestamp_low, timestamp_hi;

    if (fread(&timestamp_low, 1, 4, savefile) != 4)
        error = 1;
    if (fread(&timestamp_hi, 1, 4, savefile) != 4)
        timestamp_hi = 0;

    old_time = ((u64)timestamp_low) | (((u64)timestamp_hi) << 32);

    if (error)
    {
        Debug_ErrorMsgArg("Error while loading RTC data!");
        old_time = current_time;
    }

    if (GameBoy.Emulator.Timer.halt == 1)
        return; // Nothing else to do...

    u64 delta_time = current_time - old_time;

    GameBoy.Emulator.Timer.sec += delta_time % 60;
    if (GameBoy.Emulator.Timer.sec > 59)
    {
        GameBoy.Emulator.Timer.sec -= 60;
        delta_time += 60;
    }

    GameBoy.Emulator.Timer.min += (delta_time / 60) % 60;
    if (GameBoy.Emulator.Timer.min > 59)
    {
        GameBoy.Emulator.Timer.min -= 60;
        delta_time += 3600;
    }

    GameBoy.Emulator.Timer.hour += (delta_time / 3600) % 24;
    if (GameBoy.Emulator.Timer.hour > 23)
    {
        GameBoy.Emulator.Timer.hour -= 24;
        delta_time += 3600 * 24;
    }

    GameBoy.Emulator.Timer.days += delta_time / (3600 * 24);
    while (GameBoy.Emulator.Timer.days > 511)
    {
        GameBoy.Emulator.Timer.days &= 511;
        GameBoy.Emulator.Timer.carry = 1;
    }

    ConsolePrint("Done!\n");
}

//--------------------------------------------------------------------------

void GB_SRAM_Save(void)
{
    if ((GameBoy.Emulator.RAM_Banks == 0) || (GameBoy.Emulator.HasBattery == 0))
        return;

    int size = strlen(GameBoy.Emulator.save_filename) + 5;
    char *name = malloc(size);
    snprintf(name, size, "%s.sav", GameBoy.Emulator.save_filename);

    FILE *savefile = fopen(name, "wb+");

    if (!savefile)
    {
        Debug_ErrorMsgArg("Couldn't save SRAM.");
        free(name);
        return;
    }

    if (GameBoy.Emulator.MemoryController == MEM_MBC2)
    {
        // 512 * 4 bits
        int n = fwrite(GameBoy.Memory.ExternRAM[0], 1, 512, savefile);
        if (n != 512)
            Debug_ErrorMsgArg("Error while writing SRAM: %d bytes written.", n);
    }
    //else if (((_GB_ROM_HEADER_ *)GameBoy.Emulator.Rom_Pointer)->ram_size == 1)
    //{
    //    // 2 KB
    //    fwrite(GameBoy.Memory.ExternRAM[0], 1, 2 * 1024, savefile);
    //}
    else
    {
        // Complete banks
        for (int a = 0; a < GameBoy.Emulator.RAM_Banks; a++)
        {
            int n = fwrite(GameBoy.Memory.ExternRAM[a], 1, 8 * 1024, savefile);
            if (n != (8 * 1024))
                Debug_ErrorMsgArg("Error while writing SRAM bank %d: %d bytes written",
                                  a, n);
        }
    }

    GB_RTC_Save(savefile);

    fclose(savefile);
    free(name);
}

void GB_SRAM_Load(void)
{
    if ((GameBoy.Emulator.RAM_Banks == 0) || (GameBoy.Emulator.HasBattery == 0))
        return;

    // Reset cartridge RAM in case there is no SAV
    for (int i = 0; i < GameBoy.Emulator.RAM_Banks; i++)
    {
        memset_rand(GameBoy.Memory.ExternRAM[i], 8 * 1024);
    }

    int size = strlen(GameBoy.Emulator.save_filename) + 5;
    char *name = malloc(size);
    snprintf(name, size, "%s.sav", GameBoy.Emulator.save_filename);

    FILE *savefile = fopen(name, "rb");

    if (!savefile) // No save file...
    {
        free(name);
        return;
    }

    ConsolePrint("Loading SRAM... ");

    if (GameBoy.Emulator.MemoryController == MEM_MBC2)
    {
        // 512 * 4 bits
        int n = fread(GameBoy.Memory.ExternRAM[0], 1, 512, savefile);

        if (n != 512)
            ConsolePrint("Error while reading SRAM: %d bytes read", n);
    }
    //else if (((_GB_ROM_HEADER_ *)GameBoy.Emulator.Rom_Pointer)->ram_size == 1)
    //{
    //    //2 KB
    //    fread(GameBoy.Memory.ExternRAM[0], 1, 2 * 1024, savefile);
    //}
    else // Complete banks
    {
        for (int a = 0; a < GameBoy.Emulator.RAM_Banks; a++)
        {
            int n = fread(GameBoy.Memory.ExternRAM[a], 1, 8 * 1024, savefile);
            if (n != 8 * 1024)
                ConsolePrint("Error while reading SRAM bank %d: %d bytes read",
                             a, n);
        }
    }

    GB_RTC_Load(savefile);

    fclose(savefile);
    free(name);

    ConsolePrint("Done!\n");
}
