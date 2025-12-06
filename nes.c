#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NINTENDO_RAM_SZ         0x800
#define NINTENDO_PRG_RAM_SZ     0x2000
#define NINTENDO_PRG_ROM_SZ     0x4000
#define NINTENDO_CHR_ROM_SZ     0x2000


struct cpu_6502 {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t s;
    uint8_t p;
    uint16_t pc;
};

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

// PPU memory map
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

    struct nes_cart *cart;

    uint8_t oam[0x0100];
    uint8_t vram[0x0800];
    uint8_t palette[0x020];
    uint32_t frame_buffer[256 * 240];
};

struct nes_emu {
    struct cpu_6502 cpu;
    struct nes_ppu  ppu;
    struct nes_cart cart;
    struct nes_bus bus;

    uint8_t ram[NINTENDO_RAM_SZ];
};

int nes_load_ines_header(FILE *fp, struct nes_cart *cart);
int nes_prg_ram_alloc(struct nes_cart *cart);
void nes_trainer_set(FILE *fp, struct nes_cart *cart);
int nes_prg_rom_load(FILE *fp, struct nes_cart *cart);
int nes_chr_rom_load(FILE *fp, struct nes_cart *cart);
int nes_load_catridge(struct nes_emu *nes,
                      struct nes_cart *cart,
                      const char *name);

int nes_cart_read(struct nes_cart *cart, uint16_t addr);
void nes_cart_write(struct nes_cart *cart, uint16_t addr, uint8_t data);

void nes_init(struct nes_emu *nes);
void nes_ppu_init(struct nes_emu *nes);
void nes_init_bus(struct nes_emu *nes);

uint8_t nes_bus_read(struct nes_bus *bus, uint16_t addr);
void nes_bus_write(struct nes_bus *bus, uint16_t addr, uint8_t data);

uint8_t nes_ppu_reg_read(struct nes_ppu *ppu, uint16_t addr);
uint8_t nes_ppu_read(struct nes_ppu *ppu, uint16_t addr);


uint16_t nes_nametable_addr_get(struct nes_ppu *ppu,  uint16_t addr);
uint8_t nes_palette_addr_get(struct nes_ppu *ppu,  uint16_t addr);

void nes_ppu_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data);

void nes_ppu_reg_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data);

void nes_oam_dma_transfer(struct nes_bus *bus, uint8_t data);

int nes_load_catridge(struct nes_emu *nes,
                      struct nes_cart *cart,
                      const char *name)
{
    FILE *fp;
    size_t ret;

    if (!name ||name[0] == '\0')
        return -1;

    fp = fopen(name, "rb");
    if (!fp)
        return -1;

    ret = nes_load_ines_header(fp, cart);
    if (ret)
        goto cleanup;

    nes_trainer_set(fp, cart);

    ret = nes_prg_rom_load(fp, cart);
    if (ret)
        goto cleanup;

    ret = nes_chr_rom_load(fp, cart);
    if (ret)
        goto cleanup;

    ret = nes_prg_ram_alloc(cart);

    cart->mirroring = cart->header.flags6 & 0x01;
    cart->battery = cart->header.flags6 & 0x02;

    nes->cart = *cart;

cleanup:
    fclose(fp);

    return ret;
}

int nes_load_ines_header(FILE *fp, struct nes_cart *cart)
{
    size_t ret;

    ret = fread(&cart->header, 1, sizeof(struct ines_header), fp);
    if (ret != sizeof(struct ines_header))
        return -1;

    return 0;
}

void nes_trainer_set(FILE *fp, struct nes_cart *cart)
{
    cart->trainer_present = cart->header.flags6 & 0x04;

    if (cart->trainer_present)
        fseek(fp, 512, SEEK_CUR);
}

int nes_prg_ram_alloc(struct nes_cart *cart)
{
    cart->prg_ram = NULL;

    if (cart->header.flags6 & 0x02) {
        // Assuming NES format, and no NES v2 support.
        cart->prg_ram = malloc(NINTENDO_PRG_RAM_SZ);
        if (!cart->prg_ram)
            return -1;
    }

    return 0;
}

int nes_prg_rom_load(FILE *fp, struct nes_cart *cart)
{
    size_t prg_bytes, ret;

    cart->prg_rom = NULL;

    if (cart->header.prg_rom_size == 0)
        return 0;

    prg_bytes = cart->header.prg_rom_size * NINTENDO_PRG_ROM_SZ;

    cart->prg_rom = malloc(prg_bytes);

    if (!cart->prg_rom)
        return -1;

    ret = fread(cart->prg_rom, 1, prg_bytes, fp);
    if (ret != prg_bytes) {
        free(cart->prg_rom);
        cart->prg_rom = NULL;
        return -1;
    }

    return 0;
}

int nes_chr_rom_load(FILE *fp, struct nes_cart *cart)
{
    size_t chr_bytes, ret;

    cart->chr_rom = NULL;

    if (cart->header.chr_rom_size == 0)
        return 0;

    chr_bytes = cart->header.chr_rom_size * NINTENDO_CHR_ROM_SZ;

    cart->chr_rom = malloc(chr_bytes);
    if (!cart->chr_rom)
        return -1;

    ret = fread(cart->chr_rom, 1, chr_bytes, fp);
    if (ret != chr_bytes) {
        free(cart->chr_rom);
        cart->chr_rom = NULL;
        return -1;
    }

    return 0;
}

int nes_eject_catridge(struct nes_emu *nes, struct nes_cart *cart)
{
    if (cart->prg_rom != NULL)
        free(cart->prg_rom);

    if (cart->chr_rom != NULL)
        free(cart->chr_rom);

    if (cart->prg_ram != NULL)
        free(cart->prg_ram);

    return 0;
}

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
        if (cart->prg_ram != NULL)
            cart->prg_ram[addr - 0x6000] = data;
}

void nes_init_bus(struct nes_emu *nes)
{
    nes->bus.cpu = &nes->cpu;
    nes->bus.ppu = &nes->ppu;
    nes->bus.cart = &nes->cart;
    nes->bus.ram = nes->ram;
}

void nes_init(struct nes_emu *nes)
{
    memset(nes->ram, 0, sizeof(nes->ram));

    nes_ppu_init(nes);
    nes_init_bus(nes);
}

void nes_ppu_init(struct nes_emu *nes)
{
    nes->ppu.cart = &nes->cart;
}

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

int main(int argc, char *argv[])
{
    struct nes_emu nes;
    struct nes_cart cart;

    nes_init(&nes);
    nes_load_catridge(&nes, &cart, "roms/donkey_kong.nes");

clanup:
    nes_eject_catridge(NULL, &cart);

    return 0;
}
