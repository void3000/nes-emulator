#ifndef NES_BUS_HEADER
#define NES_BUS_HEADER

#include <stdint.h>

// NES memory map
//
// +-----------------+ 0x0000
// | Zero Page       |  256 bytes ($00-$FF)
// +-----------------+ 0x0100
// | Stack           |  256 bytes ($0100-$01FF)
// +-----------------+ 0x0200
// | RAM             |  2 KB internal RAM ($0200-$07FF)
// +-----------------+ 0x0800
// | RAM Mirrors     |  Mirrors of $0000-$07FF ($0800-$1FFF)
// +-----------------+ 0x2000
// | PPU Registers   |  8 bytes ($2000-$2007)
// +-----------------+ 0x2008
// | PPU Reg Mirrors |  Mirrors of $2000-$2007 ($2008-$3FFF)
// +-----------------+ 0x4000
// | APU & I/O       | Audio, controller ports ($4000-$4017)
// +-----------------+ 0x4018
// | APU/IO Mirrors  | ($4018-$401F)
// +-----------------+ 0x4020
// | Cartridge Space | PRG-ROM, PRG-RAM, mapper registers ($4020-$FFFF)
// +-----------------+ 0xFFFF
struct nes_bus {
    struct nes_emu *nes;
    struct cpu_6502 *cpu;
    struct nes_ppu  *ppu;
    struct nes_cart *cart;

    uint8_t *ram;
};

uint8_t nes_bus_read(struct nes_bus *bus, uint16_t addr);

void nes_bus_write(struct nes_bus *bus, uint16_t addr, uint8_t data);
void nes_oam_dma_transfer(struct nes_bus *bus, uint8_t data);

#endif
