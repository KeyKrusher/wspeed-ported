#ifndef bqMUSIC_H
#define bqMUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Some necessary options */
#ifdef __BORLANDC__
 #pragma option -N-
 #pragma option -k-
#endif

#include "sizes.h"

/* StartS3M(s) stars music pointed by s. */
extern void StartS3M(const BYTE *s);

/* StopS3M() ends music and releases resources. */
extern void StopS3M(void);

/* PauseS3M() toggles music pause. */
extern void PauseS3M(void);
extern void SetPause(int state);

#include "m_struct.h"

extern volatile struct MusData MusData;

extern void MusSeekOrd(unsigned Num);
extern void MusSeekRow(unsigned Num);
extern void MusSeekNextOrd(void);
extern void MusSeekPrevOrd(void);

#ifdef __cplusplus
};
#endif

#endif
