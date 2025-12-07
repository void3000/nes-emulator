#include "ppu.h"

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
        addr = addr & 0x07ff;
    else
        // Horizontal arrangement: 0x2000 and 0x2800 contain 
        // the first nametable, and 0x2400 and 0x2c00 contain 
        // the second nametable accomplished by connecting 
        // CIRAM A10 to PPU A10.
        addr = ((addr >>1 ) & 0x400) | (addr && 0x3ff);

    return addr;
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

void nes_ppu_pipeline_tick(struct nes_ppu *ppu)
{
    switch (ppu->scanline) {
    case 0 ... 239:
        if (ppu->cycle > 0)
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

    if ((ppu->mask & 0x18) == 0x18)
        // At this point, both background and sprites
        // have been diabled for renderinbg,so display
        // the backdrop color as per specification.
        nes_ppu_backdrop_render(ppu);
}

void nes_ppu_bkg_render(struct nes_ppu *ppu)
{
    uint16_t tile_addr, attr_addr;
    uint8_t tile_index, attr_byte;

    // The Nametable holds the tile indices for the
    // current scanline and cycle.
    tile_addr = nes_tile_addr_calc(ppu);
    tile_index = nes_ppu_read(ppu, tile_addr);

    // The attribute value controls which palette is
    // assigned to each part of the background.
    attr_addr = nes_tile_attr_addr_calc(ppu);
    attr_byte = nes_ppu_read(ppu, attr_addr);
}

uint16_t nes_tile_addr_calc(struct nes_ppu *ppu)
{
    uint16_t base_addr, xt, yt;

    // The selected Nametable base address
    base_addr =  0x2000 | (ppu->ctrl & 0x03) << 10;

    // We need to map screen coordinates [x, y] to Nametable
    // coordinates [xt, yt]. We do this because the Nametable
    // has 30 rows of 32 tileseach, and each tile is 8x8 pixels.
    xt = (ppu->cycle - 1) >> 3;
    yt = (ppu->scanline << 2) & 0xc0;       // Same as: (scanline >> 3) << 5

    return base_addr + yt + xt;
}

uint16_t nes_tile_attr_addr_calc(struct nes_ppu *ppu)
{
    uint16_t base_addr, xt, yt;

    // The selected Nametable base address
    base_addr =  0x2000 | (ppu->ctrl & 0x03) << 10;

    // Each attribute byte covers a 32x32 pixel area,
    // or 4x4 tiles. Thus, we need to map screen
    // coordinates [x, y] to attribute table coordinates
    // [xt, yt]. Each attribute byte is located after the
    // 960 bytes of tile indices, so we need to offset by
    // 960 bytes.
    xt = (ppu->cycle - 1) >> 5;
    yt = (ppu->scanline >> 2) & 0xf8;       // Same as: (scanline >> 5) << 3

    return base_addr + 0x03c0 + yt + xt;
}

void nes_ppu_sprite_render(struct nes_ppu *ppu)
{
    uint32_t pixel;

    ppu->frame_buffer[0] = pixel;
}

void nes_ppu_backdrop_render(struct nes_ppu *ppu)
{
    uint16_t x, y;

    x = ppu->cycle - 1;
    y = ppu->scanline;

    if (ppu->palette_table)
        return;

	ppu->frame_buffer[NES_FRAME_BUFF_OFFSET(x, y)] = 
        ppu->palette_table[ppu->palette[0] & 0x3F];
}

void nes_ppu_prerender_scanline_tick(struct nes_ppu *ppu)
{
}

void nes_ppu_vblank_scanline_tick(struct nes_ppu *ppu)
{
}

