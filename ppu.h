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
    
    // The palette stores indeces into the NES color palette table defined
    // by the hardware. The pogram can change these palette entries to
    // change the colors displayed on screen.
    uint8_t palette[0x020];

    uint32_t frame_buffer[256 * 240];

    // Predefined NES color palette in 32-bit RGB format.
    uint32_t *palette_table;

    struct nes_cart *cart;
};

/* NES 64-color 32-bit colors RGB palette */
static uint32_t nes_canonical_palette[64] = {
    0xff757575, 0xff271b8f, 
    0xff0000ab, 0xff47009f,
    0xff8f0077, 0xffa7004e, 
    0xffb7001e, 0xffb00000,
    0xffa70000, 0xff7f0b00, 
    0xff432f00, 0xff004700,
    0xff005100, 0xff003f17, 
    0xff1b3f5f, 0xff000000,
    0xffbcbcbc, 0xff0073ef, 
    0xff233bef, 0xff8300f3,
    0xffbf00bf, 0xffe7005b, 
    0xfff30017, 0xffef2b00,
    0xffcb4f0f, 0xff8b7300, 
    0xff009700, 0xff00ab00,
    0xff00933b, 0xff00838b, 
    0xff000000, 0xff000000,
    0xffffffff, 0xff3fbfff, 
    0xff5f73ff, 0xff9f3fff,
    0xffbf3fbf, 0xffff3f8f, 
    0xffff5f3f, 0xffff7b0f,
    0xffef9f0f, 0xffbfbf00, 
    0xff5fdf00, 0xff3fef5f,
    0xff3fef9f, 0xff3fcfcf, 
    0xff000000, 0xff000000,
    0xffffffff, 0xffabe7ff, 
    0xffc7d7ff, 0xffd7c7ff,
    0xffe7c7e7, 0xffffc7cf, 
    0xffffd7c7, 0xffffe7b7,
    0xfffff7a3, 0xffe3ffa3, 
    0xffc3ffb3, 0xffb3ffcf,
    0xffb3fff3, 0xffb3e3ff, 
    0xff000000, 0xff000000
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
