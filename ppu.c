#include "ppu.h"
#include <stdio.h>

uint8_t nes_ppu_read(struct nes_ppu *ppu, uint16_t addr)
{
    uint8_t pal;
    
    addr &= 0x3fff;

    switch (addr) {
    case 0x0000 ... 0x1fff:
        // No support for CHR RAM for now
        return ppu->cart->chr_rom[addr];
    case 0x2000 ... 0x3eff:
        addr = nes_nametable_addr_calc(ppu, addr);
        return ppu->vram[addr];
    case 0x3f00 ... 0x3fff:
        pal = nes_palette_addr_calc(ppu, addr);
        return ppu->palette[pal];
    default:
        return 0;
    }
}

void nes_ppu_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data)
{
    uint8_t pal;

    addr &= 0x3fff;

    switch (addr) {
    case 0x0000 ... 0x1fff:
        // No support for CHR RAM for now
        break;
    case 0x2000 ... 0x3eff:
        addr = nes_nametable_addr_calc(ppu, addr);
        ppu->vram[addr] = data;
        break;
    case 0x3f00 ... 0x3fff:
        pal = nes_palette_addr_calc(ppu, addr);
        ppu->palette[pal] = data;
    default:
        return;
    }
}

uint8_t nes_ppu_reg_read(struct nes_ppu *ppu, uint16_t addr)
{
    uint8_t data, ret;

    switch(addr) {
    case 0x2002:
        data = ppu->status;

        ppu->status &= ~0x80;
        ppu->reg.w = 0;

        return data;
    case 0x2004:
        return ppu->oam[ppu->oam_addr];
    case 0x2007:
        data = nes_ppu_read(ppu, ppu->vram_addr);
 
        ret = ppu->vram_data_latch;
        if (ppu->vram_addr >= 0x3f00)
            ret = data;

        ppu->vram_data_latch = data;
        ppu->vram_addr += (ppu->ctrl & 0x04) ? 0x20: 0x01;

        return ret;
    default:
        return 0;
    }
}

void nes_ppu_reg_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data)
{
    addr = 0x2000 + (addr & 0x07);

    switch (addr) {
    case 0x2000:
        ppu->ctrl = data;
        break;
    case 0x2001:
        ppu->mask = data;
        break;
    case 0x2003:
        ppu->oam_addr = data;
        break;
    case 0x2004:
        ppu->oam[ppu->oam_addr++] = data;
        break;
    case 0x2005: // TODO: scrolling
        if (ppu->reg.w) {
            ppu->reg.w = 0;
        } else {
            ppu->reg.w = 1;
        }
        break;
    case 0x2006: // TODO: scrolling
        if (ppu->reg.w) {
            ppu->reg.w = 0;
        } else {
            ppu->reg.w = 1;
        }

        break;
    case 0x2007:
        nes_ppu_write(ppu, ppu->vram_addr, data);
        ppu->vram_addr += (ppu->ctrl & 0x04) ? 0x20: 0x01;

        break;
    default:
        return;
    }
}

uint16_t nes_nametable_addr_calc(struct nes_ppu *ppu,  uint16_t addr)
{
    addr &= 0x2fff;             // Mirrored

    if (ppu->cart->mirroring)
        // Vertical arrangement: 0x2000 and 0x2400 contain the
        // first nametable, and 0x2800 and 0x2C00 contain the
        // second nametable, accomplished by connecting CIRAM
        // A10 to PPU A11.
        return addr & 0x07ff;
    else
        // Horizontal arrangement: 0x2000 and 0x2800 contain 
        // the first nametable, and 0x2400 and 0x2c00 contain 
        // the second nametable accomplished by connecting 
        // CIRAM A10 to PPU A10.
        return ((addr >> 1 ) & 0x400) | (addr & 0x3ff);
}

uint8_t nes_palette_addr_calc(struct nes_ppu *ppu,  uint16_t addr)
{
    uint8_t pal;

    pal = addr & 0x01f;

    // Apparently it was expensive to have separate
    // physical memory for both background and sprite
    // palette entry. So, some background colors were
    // reused for sprite colors
    if ((pal & 0x13) == 0x10)
        pal &= 0x0f;

    return pal;
}

void nes_ppu_tick(struct nes_ppu *ppu)
{

    nes_ppu_pipeline_tick(ppu);

    ppu->cycle++;

    if (ppu->cycle > 340) {
        ppu->cycle = 0;
        ppu->scanline++;

        if (ppu->scanline > 261)
            ppu->scanline = 0;
    }
}

void nes_ppu_pipeline_tick(struct nes_ppu *ppu)
{
    switch (ppu->scanline) {
    case 0 ... 239:
        if (ppu->cycle >= 1 && ppu->cycle <= 256)
            nes_ppu_visible_scanline_tick(ppu);
        break;
    case 241 ... 260:
        nes_ppu_vblank_scanline_tick(ppu);
        break;
    case 261:
        nes_ppu_prerender_scanline_tick(ppu);
        break;
    }
}

void nes_ppu_visible_scanline_tick(struct nes_ppu *ppu)
{
    if (ppu->mask & 0x08)
        // Render must happen in the order of background
        // first, then sprites on top.
        nes_ppu_bkg_render(ppu);

    if (ppu->mask & 0x10)
        nes_ppu_sprite_render(ppu);

    if ((ppu->mask & 0x18) != 0x18)
        // At this point, both background and sprites
        // have been diabled for renderinbg,so display
        // the backdrop color as per specification.
        nes_ppu_backdrop_render(ppu);
}

void nes_ppu_bkg_render(struct nes_ppu *ppu)
{
    uint16_t tile_addr, attr_addr, palette_index, pattern_addr, pixel_index, x, y;
    uint8_t tile_indx, attr_byte, rgb_index;

    // The attribute value controls which palette is
    // assigned to each part of the background.
    attr_addr = nes_tile_attr_addr_calc(ppu);
    attr_byte = nes_ppu_read(ppu, attr_addr);
 
    palette_index = nes_attr_palette_calc(ppu, attr_byte);

    // The Nametable holds the tile indices for the
    // current scanline and cycle.
    tile_addr = nes_tile_addr_calc(ppu);
    tile_indx = nes_ppu_read(ppu, tile_addr);

    // The pattern value controls which pixels or colors
    // from the tile are displayed on screen.
    pattern_addr = nes_pattern_addr_calc(ppu, tile_indx);
    pixel_index = nes_pattern_data_calc(ppu, pattern_addr);

    if (pixel_index)
        // Because we are rendering the background, it's fine to use a 4 bit
        // offset into the palette. The last 16 entries are for the sprites.
        rgb_index = ppu->palette[((palette_index << 2) | pixel_index) & 0x3f];
    else
        // Backdrop color (transparent)
        rgb_index = ppu->palette[0];

    x = ppu->cycle - 1;
    y = ppu->scanline;

    ppu->frame_buffer[FRAME_BUFF_OFFSET(x, y)] = ppu->palette_table[rgb_index];
}

uint16_t nes_tile_addr_calc(struct nes_ppu *ppu)
{
    uint16_t base_addr, xt, yt;

    // The selected Nametable base address
    base_addr =  0x2000 | (ppu->ctrl & 0x03) << 0x0a;

    // We need to map screen coordinates [x, y] to Nametable
    // coordinates [xt, yt]. We do this because the Nametable
    // has 30 rows of 32 tileseach, and each tile is 8x8 pixels.
    xt = (ppu->cycle - 1) >> 3;
    yt = (ppu->scanline >> 3) << 5;

    return base_addr + yt + xt;
}

uint16_t nes_tile_attr_addr_calc(struct nes_ppu *ppu)
{
    uint16_t base_addr, xt, yt;

    // The selected Nametable base address
    base_addr =  0x2000 | (ppu->ctrl & 0x03) << 0x0a;

    // Each attribute byte covers a 32x32 pixel area,
    // or 4x4 tiles. Thus, we need to map screen
    // coordinates [x, y] to attribute table coordinates
    // [xt, yt]. Each attribute byte is located after the
    // 960 bytes of tile indices, so we need to offset by
    // 960 bytes.
    xt = (ppu->cycle - 1) >> 5;
    yt = (ppu->scanline >> 5) << 3;

    return base_addr + 0x03c0 + yt + xt;
}

uint8_t nes_attr_palette_calc(struct nes_ppu *ppu, uint8_t attr_byte)
{
    uint8_t xq, yq, quadrant, shift;

    xq = ((ppu->cycle - 1) >> 4) & 0x01;
    yq = (ppu->scanline    >> 4) & 0x01;

    quadrant = xq | yq << 1;

    // Each quadrant uses 2 bits in the attribute byte
    // to select the palette for that quadrant.
    shift = quadrant << 1;

    return (attr_byte >> shift) & 0x03;
}

uint16_t nes_pattern_addr_calc(struct nes_ppu *ppu, uint8_t tile_indx)
{
    uint16_t base_addr;

    if (ppu->ctrl & 0x10) 
        base_addr = 0x1000;
    else
        base_addr = 0x0000;

    return base_addr + (tile_indx << 4);
}

uint16_t nes_pattern_data_calc(struct nes_ppu *ppu, uint16_t pattern_addr)
{
    uint8_t fy, fx, pattern_lo, pattern_hi, shift, bit0, bit1;

    fy = ppu->scanline & 0x07;
    fx = (ppu->cycle - 1) & 0x07;

    pattern_lo = nes_ppu_read(ppu, pattern_addr + fy);
    pattern_hi = nes_ppu_read(ppu, pattern_addr + fy + 8);
 
    shift = 7 - fx;
    
    bit0 = (pattern_lo >> shift) & 0x01;
    bit1 = (pattern_hi >> shift) & 0x01;

    return (bit1 << 1) | bit0;
}

void nes_ppu_sprite_render(struct nes_ppu *ppu)
{
}

void nes_ppu_backdrop_render(struct nes_ppu *ppu)
{
    uint16_t x, y;

    x = ppu->cycle - 1;
    y = ppu->scanline;

	ppu->frame_buffer[FRAME_BUFF_OFFSET(x, y)] = 
        ppu->palette_table[ppu->palette[0] & 0x3f];
}

void nes_ppu_prerender_scanline_tick(struct nes_ppu *ppu)
{
}

void nes_ppu_vblank_scanline_tick(struct nes_ppu *ppu)
{
}

