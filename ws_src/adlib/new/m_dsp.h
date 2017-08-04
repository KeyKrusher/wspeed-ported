#ifndef bqMusicDSP_H
#define bqMusicDSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "music.h"

extern struct MusDriver DSPDriver;

#ifdef __cplusplus
}
#endif

#define DSP_RST      0x06                 // DSP reset port
#define DSP_RD_ST    0x0E                 // DSP read buffer status port
#define DSP_RD       0x0A                 // DSP read port
#define DSP_WR_ST    0x0C                 // DSP write buffer status port
#define DSP_WR       0x0C                 // DSP write port
#define DSP_RST_OK   0xaa                 // DSP reset OK

/* SB-MIDI definitions */
#define MIDI_IN_P       0x30              // MIDI read (polling mode)
#define MIDI_IN_I       0x31              // MIDI read (interrupt mode)
#define MIDI_UART_P     0x34              // MIDI UART mode (polling mode)
#define MIDI_UART_I     0x35              // MIDI UART mode (interrupt mode)
#define MIDI_UART_TS_P  0x36              // same as 0x34 with timestamp
#define MIDI_UART_TS_I  0x37              // same as 0x35 with timestamp
#define MIDI_OUT_P      0x38              // MIDI write (polling mode)

#endif
