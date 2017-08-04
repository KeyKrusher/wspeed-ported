#ifndef bqMusicIRQ_h
#define bqMusicIRQ_h

extern unsigned Rate;

/* Install the timer handler, which will call TimerHandler(). */
extern void InstallINT(void);

/* This will block until the timer routine has been released. */
extern void ReleaseINT(void);

/* This is called by the timer routines
 * of m_irq.c, and not defined there.
 */
extern void TimerHandler(void);

#endif
