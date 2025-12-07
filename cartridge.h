#ifndef NES_CARTRIDGE_HEADER
#define NES_CARTRIDGE_HEADER

#include <stdint.h>

struct ines_header {
    uint8_t signature[4];   // "NES\x1A"
    uint8_t prg_rom_size;   // PRG-ROM size in 16 KB units
    uint8_t chr_rom_size;   // CHR-ROM size in 8 KB units

    uint8_t flags6;         // Mapper low nibble, mirroring, battery, trainer, four-screen
    uint8_t flags7;         // Mapper high nibble, NES 2.0 ID bits

    uint8_t prg_ram_size;   // iNES: size in 8 KB units (0 = 8 KB)
                            // NES 2.0: lower 4 bits PRG-RAM, upper 4 bits PRG-NVRAM

    uint8_t tv_system;      // iNES: 0=NTSC,1=PAL
                            // NES 2.0: part of timing mode

    uint8_t flags10;        // NES 2.0: CPU/PPU timing, Vs. system, etc.

    uint8_t unused[5];      // Must be zero in NES 2.0
} __attribute__((packed));

struct nes_cart {
    struct ines_header header;

    // Many old NES games used special cart hardware that
    // required certain RAM values to be preset at 0x7000-
    // 0x71FF before the game runs. Because the original
    // iNES format was created in the early 1990s (before 
    // accurate emulators existed), people needed a way to 
    // make certain ROM dumps work without fully emulating 
    // the hardware. So the Trainer was added as a hack.
    uint8_t trainer_present;
    uint8_t mirroring;
    uint8_t battery;

    // Depending on flags6 bit 1, the cartridge can contain
    // battery-backed PRG RAM mapped at CPU address 0x6000-
    // 0x7fff or other persistent memory.
    uint8_t *prg_ram;

    uint8_t *prg_rom;
    uint8_t *chr_rom;
};

int nes_cart_read(struct nes_cart *cart, uint16_t addr);

void nes_cart_write(struct nes_cart *cart, uint16_t addr, uint8_t data);

#endif
