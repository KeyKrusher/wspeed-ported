#ifndef bqMusicMidi_h
#define bqMusicMidi_h

#include "sizes.h"

/* This is not a driver of any kind.
   This is just a wrapper which converts S3M style
   thinking into MIDI style thinking, used by
   MPU and AWE modules.
*/   

extern void GM_Reset(void);
extern void GM_Bend(int c, WORD Count);
extern void GM_KeyOff(int c);
extern void GM_KeyOn(int c, BYTE key, BYTE Vol);
extern void GM_Touch(int c, BYTE Vol);
extern void GM_Bank(int c, BYTE b);
extern void GM_Patch(int c, BYTE p);
extern void GM_Controller(int c, BYTE i, BYTE d);

extern void GM_DPatch(int ch, const BYTE *D, BYTE GM, BYTE bank);
extern void GM_DNoteOn(int ch, BYTE note, int hz, BYTE vol, BYTE adlvol);
extern void GM_DTouch(int ch, BYTE vol, BYTE adlvol);
extern void GM_HzTouch(int ch, int hz);
extern void GM_OPLByte(BYTE index, BYTE data);
extern void GM_DPan(int ch, SBYTE val);

extern const unsigned char GMVol[64];

#endif
