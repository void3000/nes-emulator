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

struct nes_ppu {
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

void nes_init_bus(struct nes_emu *nes);

uint8_t nes_bus_read(struct nes_bus *bus, uint16_t addr);
void nes_bus_write(struct nes_bus *bus, uint16_t addr, uint8_t data);

uint8_t nes_ppu_read(struct nes_ppu *ppu, uint16_t addr);
void nes_ppu_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data);

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

    if (!cart->header.prg_rom_size) {
        cart->prg_rom = NULL;
        return 0;
    }

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

    if (!cart->header.chr_rom_size) {
        cart->chr_rom = NULL;
        return 0;
    }

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
    if ((addr >= 0x6000) && (addr <= 0x7fff))
        return cart->prg_ram[addr - 0x6000];

    if ((addr >= 0x8000) && (addr <= 0xffff))
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

uint8_t nes_bus_read(struct nes_bus *bus, uint16_t addr)
{
    switch (addr) {
    case 0x0000 ... 0x1fff:
        return bus->ram[addr & 0x07ff];
    case 0x2000 ... 0x3fff:
        return nes_ppu_read(bus->ppu, addr);
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
        nes_ppu_write(bus->ppu, addr, data);
        break;
    case 0x4000 ... 0x401f:
        break;  // TODO: APU + IO
    case 0x4020 ... 0xffff:
        nes_cart_write(bus->cart, addr, data);
        break;
    default:
    }
}

uint8_t nes_ppu_read(struct nes_ppu *ppu, uint16_t addr)
{
    return 0;
}

void nes_ppu_write(struct nes_ppu *ppu, uint16_t addr, uint8_t data)
{
}

int main(int argc, char *argv[])
{
    struct nes_emu nes;
    struct nes_cart cart;

    nes_load_catridge(&nes, &cart, "roms/donkey_kong.nes");
    nes_init_bus(&nes);

clanup:
    nes_eject_catridge(NULL, &cart);

    return 0;
}
