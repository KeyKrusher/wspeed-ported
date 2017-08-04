#ifndef __ADL_MPU_H
#define __ADL_MPU_H

#ifdef __cplusplus
extern "C" {
#endif

#if SUPPORT_MIDI
void GM_Controller(int c, byte i, byte d);
void GM_Patch(int c, byte p);
void GM_Bank(int c, byte b);
byte GM_Volume(byte Vol); // Converts the volume
void GM_KeyOn(int c, byte key, byte Vol);
void GM_KeyOff(int c);
void GM_Touch(int c, byte Vol);
void GM_Bend(int c, word Count);
void GM_Reset(void);
#else
 #define GM_Controller(c,i,d)
 #define GM_Patch(c,p)
 #define GM_Volume(Vol)
 #define GM_KeyOn(c,key,Vol)
 #define GM_KeyOff(c)
 #define GM_Touch(c,Vol)
 #define GM_Bend(c,Count)
 #define GM_Reset()
#endif

#ifdef __cplusplus
}
#endif

#define DSP_RST      0x06                 // DSP reset port
#define DSP_RD_ST    0x0E                 // DSP read buffer status port
#define DSP_RD       0x0A                 // DSP read port
#define DSP_WR_ST    0x0C                 // DSP write buffer status port
#define DSP_WR       0x0C                 // DSP write port
#define DSP_RST_OK   0xaa                 // DSP reset OK

// MPU-401 definitions
#define MPU401_RESET    0xFF
#define MPU401_UART     0x3F
#define MPU401_CMDOK    0xFE
#define MPU401_OK2WR    0x40
#define MPU401_OK2RD    0x80

// SB-MIDI definitions
#define MIDI_IN_P       0x30              // MIDI read (polling mode)
#define MIDI_IN_I       0x31              // MIDI read (interrupt mode)
#define MIDI_UART_P     0x34              // MIDI UART mode (polling mode)
#define MIDI_UART_I     0x35              // MIDI UART mode (interrupt mode)
#define MIDI_UART_TS_P  0x36              // same as 0x34 with timestamp
#define MIDI_UART_TS_I  0x37              // same as 0x35 with timestamp
#define MIDI_OUT_P      0x38              // MIDI write (polling mode)

#endif
