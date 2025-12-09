#ifndef NES_PPU_HEADER
#define NES_PPU_HEADER

#include <stdint.h>

#include "cartridge.h"

#define FRAME_BUFF_OFFSET(x, y)   ((y) * 256 + (x))

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

    uint16_t cycle;
    uint16_t scanline;

    struct nes_ppu_internal_reg reg;

    // The OAM (Object Attribute Memory) is 256 bytes
    // used to hold sprite information (position, tile
    // index, attributes).
    uint8_t oam[0x0100];

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

    uint32_t frame_buffer[256 * 240];

    uint32_t *palette_table;

    struct nes_cart *cart;
};

/* NES 64-color 32-bit colors RGB palette */
static uint32_t nes_palette32[64] = {
	0x7c7c7c, 0x0000fc, 
    0x0000bc, 0x4428bc,
	0x940084, 0xa80020, 
    0xa81000, 0x881400,
	0x503000, 0x007800, 
    0x006800, 0x005800,
	0x004058, 0x000000, 
    0x000000, 0x000000,
	0xbcbcbc, 0x0078f8, 
    0x0058f8, 0x6844fc,
	0xd800cc, 0xe40058, 
    0xf83800, 0xe45c10,
	0xac7c00, 0x00b800, 
    0x00a800, 0x00a844,
	0x008888, 0x000000, 
    0x000000, 0x000000,
	0xf8f8f8, 0x3cbcfc, 
    0x6888fc, 0x9878f8,
	0xf878f8, 0xf85898, 
    0xf87858, 0xfc9844,
	0xf8b800, 0xb8f818, 
    0x58d854, 0x58f898,
	0x00e8d8, 0x787878, 
    0x000000, 0x000000,
	0xfcfcfc, 0xa4e4fc, 
    0xb8b8f8, 0xd8b8f8,
	0xf8b8f8, 0xf8a4c0, 
    0xf0d0b0, 0xfce0a8,
	0xf8d878, 0xd8f878, 
    0xb8f8b8, 0xb8f8d8,
	0x00fcfc, 0xf8d8f8, 
    0x000000, 0x000000
};

uint8_t nes_ppu_reg_read(struct nes_ppu *ppu, uint16_t addr);
uint8_t nes_ppu_read(struct nes_ppu *ppu, uint16_t addr);

uint8_t nes_palette_addr_calc(struct nes_ppu *ppu,  uint16_t addr);
uint8_t nes_attr_palette_calc(struct nes_ppu *ppu, uint8_t attr_byte);

uint16_t nes_nametable_addr_calc(struct nes_ppu *ppu,  uint16_t addr);
uint16_t nes_tile_addr_calc(struct nes_ppu *ppu);
uint16_t nes_tile_attr_addr_calc(struct nes_ppu *ppu);
uint16_t nes_tile_pattern_addr_calc(struct nes_ppu *ppu, uint8_t tile_index);
uint16_t nes_pattern_data_calc(struct nes_ppu *ppu, uint16_t pattern_addr);
uint16_t nes_pattern_addr_calc(struct nes_ppu *ppu, uint8_t tile_index);

void nes_ppu_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data);
void nes_ppu_reg_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data);

void nes_ppu_tick(struct nes_ppu *ppu);
void nes_ppu_pipeline_tick(struct nes_ppu *ppu);
void nes_ppu_visible_scanline_tick(struct nes_ppu *ppu);
void nes_ppu_prerender_scanline_tick(struct nes_ppu *ppu);
void nes_ppu_vblank_scanline_tick(struct nes_ppu *ppu);

void nes_ppu_bkg_render(struct nes_ppu *ppu);
void nes_ppu_sprite_render(struct nes_ppu *ppu);
void nes_ppu_backdrop_render(struct nes_ppu *ppu);

#endif
