#ifndef bqMusicSbEnv_h
#define bqMusicSbEnv_h

#include "sizes.h"

extern void fillsbenv();

struct sbenv    /* Data from BLASTER environment variable */
{           
    int set;    /* Has been set? */
    WORD A, /* base port */
         D, /* dma */
         I, /* irq */
         H, /* high dma */
         P, /* midiport */
         E, /* emu8k port */
         T; /* type (?) */
};
extern struct sbenv sbenv;

#endif
