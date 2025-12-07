#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#include "cartridge.h"
#include "ppu.h"
#include "cpu.h"
#include "bus.h"

#define NINTENDO_RAM_SZ         0x800
#define NINTENDO_PRG_RAM_SZ     0x2000
#define NINTENDO_PRG_ROM_SZ     0x4000
#define NINTENDO_CHR_ROM_SZ     0x2000

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

void nes_init(struct nes_emu *nes);
void nes_ppu_init(struct nes_emu *nes);
void nes_init_bus(struct nes_emu *nes);


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

    nes->ppu.cycle = 0;
    nes->ppu.scanline = 0;
    nes->ppu.palette_table = nes_palette32;
}

int main(int argc, char *argv[])
{
    struct nes_emu nes;
    struct nes_cart cart;
    uint8_t running, frame_count, ret;
    SDL_Event event;

    nes_init(&nes);

    ret = nes_load_catridge(&nes, &cart, "roms/donkey_kong.nes");
    if (ret < 0)
        goto cleanup;

#define SCALE 3

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "NES Emulator",
        SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED,
        256 * SCALE, 
        240 * SCALE, 
        SDL_WINDOW_SHOWN
    );
    
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, 
        -1, 
        SDL_RENDERER_ACCELERATED
    );
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888, 
        SDL_TEXTUREACCESS_STREAMING,
        256, 
        240
    );

    running = 1;
    frame_count = 0;

    while(running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        SDL_UpdateTexture(texture, NULL, nes.ppu.frame_buffer, 256 * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

cleanup:
    nes_eject_catridge(NULL, &cart);

    return 0;
}
