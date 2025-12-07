#include "bus.h"
#include "ppu.h"
#include "cartridge.h"

uint8_t nes_bus_read(struct nes_bus *bus, uint16_t addr)
{
    switch (addr) {
    case 0x0000 ... 0x1fff:
        return bus->ram[addr & 0x07ff];
    case 0x2000 ... 0x3fff:
        return nes_ppu_reg_read(bus->ppu, addr);
    case 0x4000 ... 0x401f:
        return 0;   // TODO: APU + IO
    case 0x4020 ... 0xffff:
        return nes_cart_read(bus->cart, addr);
    default:
        return 0;
    }
}

void nes_bus_write(struct nes_bus *bus, uint16_t addr, uint8_t data)
{
    switch (addr) {
    case 0x0000 ... 0x1fff:
        bus->ram[addr & 0x07ff] = data;
        break;
    case 0x2000 ... 0x3fff:
        nes_ppu_reg_write(bus->ppu, addr, data);
        break;
    case 0x4000 ... 0x401f:
        if (addr == 0x4014) {
            nes_oam_dma_transfer(bus, data);
            return;
        }

        /* TODO: APU + IO */
        break;
    case 0x4020 ... 0xffff:
        nes_cart_write(bus->cart, addr, data);
        break;
    default:
    }
}

void nes_oam_dma_transfer(struct nes_bus *bus, uint8_t data)
{
    uint8_t byte;
    uint16_t page;

    page = (uint16_t)(data << 8);

    for (int i = 0; i < 0x100; ++i) {
        byte = nes_bus_read(bus, (page + i));

        bus->ppu->oam[i] = byte;
    }
}
