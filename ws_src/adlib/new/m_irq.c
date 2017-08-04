#include "m_irq.h"

/*
  Supported ways:
    __BORLANDC__           uses Timer IRQ
    DJGPP                  uses Allegro
    linux                  uses Timer (recommended)
    linux, USETHREADS      uses Thread
*/

/* Variables for IRQ subsystem */
unsigned Rate = 100;

/* Notes about Rate:
 *
 *   In DOS, it shouldn't be less than 19 or greater than 65535.
 *   In Linux, 1000000 should be divisible with it.
 */

static int IRQActive = 0;

/*************************************************************/

#ifdef __BORLANDC__

/* Useful macros for system functions. Only for real/v86 mode, though. */
#define GetIntVec(a) (IRQType)(*(long far *)((a)*4))
#define SetIntVec(a,b) (*(long far *)((a)*4))=(long)((void far *)(b))

typedef void interrupt (*IRQType)(void);
static IRQType OldI08;
static volatile word Clock;
static volatile word Counter;

static void interrupt NewI08(void)
{
    _asm { db 0x66; pusha } /* ASSUME 386+! */

	/* Check if we can play next row... */
    TimerHandler();

    _asm { db 0x66; popa } /* ASSUME 386+! */

    /* Check if we have now time to call the old slow timer IRQ */
    _asm {
        mov ax, Counter
        add Clock, ax
        jnc P1
        pushf
        call dword ptr OldI08
        jmp P2
    }
    /* Then tell IRQ controller that we've finished. */
P1: _asm mov al, 0x20
	_asm out 0x20, al
P2:
}

static void InstallINT(void)
{
	OldI08 = GetIntVec(0x08);
	_asm cli
	SetIntVec(0x08, NewI08);
	Counter = 0x1234DCLU / Rate;
	_asm {
		cli
		mov al, 0x34
		out 0x43, al
		mov ax, Counter
		out 0x40, al
		mov al, ah
		out 0x40, al
		sti
	}
}

static void ReleaseINT(void)
{
	SetIntVec(0x08, OldI08);
	_asm {
		mov al, 0x34
		out 0x43, al
		xor al, al
		out 0x40, al
		out 0x40, al
	}
}

#endif

/*************************************************************/

#ifdef DJGPP
#include "allegro.h"

static void Tickker(void)
{
    TimerHandler();
}

static void Install(void)
{
	install_timer();
	install_int(Ticker, BPS_TO_TIMER(Rate));	
}

static void Release(void)
{
	remove_int(Ticker);	
}

#endif

/*************************************************************/

#ifdef linux

static volatile int mayend;

#ifdef USETHREADS

#include <pthread.h>
#include <unistd.h>

#include "music.h"

static const unsigned delaylen = 100000;

static pthread_t thread;
static void *TickThread(void *dami)
{
	unsigned r = 0;
	
	/* This must be done. It hopefully ensures
	 * the IO permissions. Hopefully we don't
	 * reset anywhere where we shouldn't.
	 */	 
	MusData.driver->reset();
	
	while(!mayend)
	{
		while(r < delaylen)
		{
			r += 1000000 / Rate;
		    TimerHandler();
		}	
		while(r >= delaylen)
		{
			usleep(delaylen);
			r -= delaylen;
		}
	}
	
	return dami;
}

static void Install(void)
{
    pthread_attr_t attr;
	
	mayend = 0;
	
#ifdef PTHREAD_CANCEL_ENABLE
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
#endif

	pthread_attr_init(&attr);
	pthread_create(&thread, &attr, TickThread, NULL);
	pthread_attr_destroy(&attr);
	
	pthread_detach(thread);
}

static void Release(void)
{
	mayend = 1;
	pthread_join(thread, NULL);
}

#else

#include <sys/time.h>
#include <signal.h>

static void EnsureTicker(int yes);
static void Ticker(int dummy)
{
	dummy = dummy;
    if(!mayend)
    {
    	EnsureTicker(1);
	    TimerHandler();
	}
}

static void EnsureTicker(int yes)
{
	struct itimerval tr;
	tr.it_value.tv_sec = 0;
	tr.it_value.tv_usec = yes ? 1000000 / Rate : 0;
	tr.it_interval.tv_sec = 0;
	tr.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &tr, NULL);
	signal(SIGALRM, Ticker);
}

static void Install(void)
{
	EnsureTicker(1);
	mayend = 0;
}

static void Release(void)
{
	mayend = 1;
	/* We don't want false alarms */
	EnsureTicker(0);
	signal(SIGALRM, SIG_DFL);
}
#endif
#endif

/*************************************************************/

void InstallINT(void)
{
	if(IRQActive)return;
	IRQActive = 1;
	Install();
}

void ReleaseINT(void)
{
	if(!IRQActive)return;
	IRQActive = 0;
	
	Release();
}
