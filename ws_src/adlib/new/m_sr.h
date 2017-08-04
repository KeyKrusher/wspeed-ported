#ifndef bqMusicSavRes_h
#define bqMusicSavRes_h

#include "music.h"

/* The status buffer for SaveMusStatus and RestoreMusStatus should
   be malloced instead of wasting stack space for it. Reason: Buffer
   would probably cause a stack overflow. */

/* SaveMusStatus(Buf) saves the music status for later restoration. */
extern void SaveMusStatus(struct MusData *Buf);

/* RestoreMusStatus(Buf) restores the saved music status from Buf. */
extern void RestoreMusStatus(const struct MusData *Buf);

#endif

