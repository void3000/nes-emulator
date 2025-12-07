## PPU

### NES Background Memory Layout

#### 1. Nametable

- **Location in PPU VRAM:** 0x2000–0x2FFF (2 KB physically, mirrored to fill 4 KB)
- **Purpose:** Stores **tile indices** for the background.
- **Structure:**
  - Each byte represents **one 8×8 tile**.
  - Total size per nametable: 1 KB → 1024 bytes → 32×30 tiles (width×height of screen in tiles)
- **Attribute table:**
  - The last 64 bytes of each 1 KB nametable store **palette attributes**.
  - Each 2×2 tile block uses 2 bits to select a background palette (4 possible palettes).

#### 2. Pattern Table

- **Location:** Either in **CHR ROM** or **CHR RAM** on the cartridge
- **Purpose:** Stores the **pixel data** of tiles.
- **Structure of a tile:**
  - 8×8 pixels, 2 bits per pixel → 16 bytes per tile
- **Size:**
  - 1 pattern table = 4 KB = 4096 bytes → holds 256 tiles
  - NES has 2 pattern tables → total 512 tiles
- **Usage:**
  - Pattern Table 0 → often background tiles
  - Pattern Table 1 → can be background or sprites depending on `PPUCTRL` settings

### Rendering Background

Alrogithm:

1. Confirm which Nametable bank should be used by reading the base nametable address (bit 0-1) in the PPUCTRL register.
2. Confirm which CHR ROM/RAM bank should be used by reading the background pattern table address (bit 4) in the PPUCTRL register.
3. 