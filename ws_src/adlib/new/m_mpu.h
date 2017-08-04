#ifndef bqMusicMPU_H
#define bqMusicMPU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "music.h"

extern struct MusDriver MPUDriver;

#ifdef __cplusplus
}
#endif

/* MPU-401 definitions */
#define MPU401_RESET    0xFF
#define MPU401_UART     0x3F
#define MPU401_CMDOK    0xFE
#define MPU401_OK2WR    0x40
#define MPU401_OK2RD    0x80

#endif
