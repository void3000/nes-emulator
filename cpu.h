#ifndef NES_CPU_HEADER
#define NES_CPU_HEADER

#include <stdint.h>

struct cpu_6502 {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t s;
    uint8_t p;
    uint16_t pc;
};

#endif
