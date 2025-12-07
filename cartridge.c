#include "cartridge.h"

int nes_cart_read(struct nes_cart *cart, uint16_t addr)
{
    addr &= 0xffff;

    // Not supproted: Expansion + mappers (at 0x4020-0x5fff)
    if (addr < 0x8000)
        return cart->prg_ram[addr - 0x6000];

    if (addr >= 0x8000)
        // Assume flags 8 is always zero
        return cart->prg_rom[addr - 0x8000];

    return 0;
}

void nes_cart_write(struct nes_cart *cart, uint16_t addr, uint8_t data)
{
    if ((addr >= 0x6000) && (addr <= 0x7fff))
        if (cart->prg_ram)
            cart->prg_ram[addr - 0x6000] = data;
}
