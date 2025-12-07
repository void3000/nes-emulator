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
        addr = nes_nametable_addr_get(ppu, addr);

        return ppu->vram[addr];
    case 0x3f00 ... 0x3fff:
        pal = nes_palette_addr_get(ppu, addr);

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
        addr = nes_nametable_addr_get(ppu, addr);

        ppu->vram[addr] = data;
        break;
    case 0x3f00 ... 0x3fff:
        pal = nes_palette_addr_get(ppu, addr);

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

uint16_t nes_nametable_addr_get(struct nes_ppu *ppu,  uint16_t addr)
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

uint8_t nes_palette_addr_get(struct nes_ppu *ppu,  uint16_t addr)
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
