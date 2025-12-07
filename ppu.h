#ifndef NES_PPU_HEADER
#define NES_PPU_HEADER

#include <stdint.h>

#include "cartridge.h"

struct nes_cart;

struct nes_ppu_internal_reg {
    // During rendering, used for the scroll position.
    // Outside of rendering, used as the current VRAM 
    // address.
    uint16_t v;

    // During rendering, specifies the starting coarse-x
    // scroll for the next scanline and the starting y 
    // scroll for the screen. Outside of rendering, holds
    // the scroll or VRAM address before transferring it
    // to v.
    uint16_t t;

    // The fine-x position of the current scroll, used
    // during rendering alongside v.
    uint16_t x;

    // Toggles on each write to either PPUSCROLL or PPUADDR,
    // indicating whether this is the first or second write.
    // Clears on reads of PPUSTATUS. Sometimes called the 
    // 'write latch' or 'write toggle'.
    uint8_t w;
};

struct nes_ppu {
    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    uint8_t oam_addr;
    uint16_t scroll;
    uint16_t vram_addr;
    uint8_t vram_data_latch;
    uint8_t oam_dma;

    struct nes_ppu_internal_reg reg;


    uint8_t oam[0x0100];

    uint32_t frame_buffer[256 * 240];

    // Memory map
    // +---------------------------+ 0x0000
    // | Pattern Table 0           | 4 KB  (from cartridge CHR ROM/RAM)
    // | (tiles)                   |
    // +---------------------------+ 0x1000
    // | Pattern Table 1           | 4 KB  (from cartridge CHR ROM/RAM)
    // +---------------------------+ 0x2000
    // | Nametable 0               | 1 KB  \
    // +---------------------------+        \
    // | Nametable 1               | 1 KB   |--> 2 KB INTERNAL PPU VRAM
    // +---------------------------+        |
    // | Nametable 2 (mirror)      | 1 KB   |
    // +---------------------------+        |
    // | Nametable 3 (mirror)      | 1 KB  /
    // +---------------------------+ 0x3F00
    // | Palette RAM               | 32 bytes
    // +---------------------------+ 0x3F20
    // | Palette Mirroring         | mirrors every 32 bytes
    // +---------------------------+ 0x3FFF
    uint8_t vram[0x0800];
    uint8_t palette[0x020];

    struct nes_cart *cart;
};


uint8_t nes_ppu_reg_read(struct nes_ppu *ppu, uint16_t addr);
uint8_t nes_ppu_read(struct nes_ppu *ppu, uint16_t addr);
uint8_t nes_palette_addr_get(struct nes_ppu *ppu,  uint16_t addr);

uint16_t nes_nametable_addr_get(struct nes_ppu *ppu,  uint16_t addr);

void nes_ppu_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data);
void nes_ppu_reg_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data);

#endif
