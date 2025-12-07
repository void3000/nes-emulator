```
gcc nes.c cpu.c ppu.c cartridge.c bus.c -o nes $(sdl2-config --cflags --libs)
```