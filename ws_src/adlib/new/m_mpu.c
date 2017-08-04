#include "m_mpu.h"
#include "m_midi.h"
#include "m_sbenv.h"
#include "io.h"

#ifdef __BORLANDC__
#include <dos.h>
#endif

#ifdef linux

#include <unistd.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <stdlib.h>

static int seqfd;
static int mididevno = 1;

SEQ_DEFINEBUF(128);

void seqbuf_dump(void)
{
	if(_seqbufptr && seqfd>0)
	{
		write(seqfd, _seqbuf, _seqbufptr);
		_seqbufptr = 0;
	}                            			
}
static void MPU_Byte(BYTE data)
{
	if(seqfd < 0)return;
	SEQ_MIDIOUT(mididevno, data);
	SEQ_DUMPBUF();
}

#endif

#ifdef DJGPP
#include <allegro.h>

static void MPU_Byte(BYTE data)
{
    midi_mpu401.raw_midi(data);
}


#endif

#ifdef __BORLANDC__

static unsigned MPUBase;

static void MPU_Byte(BYTE data)
{
    int a;
    
    /* check write status */
    for(a=0; inportb(MPUBase + 1) & MPU401_OK2WR; ++a)
        if(a>1000)break;
    
    outportb(MPUBase, data); // offer timeout instead of lockup
}

static void MPU_Cmd(BYTE cmd)
{
	register int a;
	/* check write status */
	for(a=0; inportb(MPUBase + 1) & MPU401_OK2WR; a++)if(a>100)break;
	outportb(MPUBase + 1, cmd);
}

/************************************************************************
 * READ_MPU401_DATA: reads a byte from the MPU401 port.
 * Input: none
 * Output: data byte
 ************************************************************************/
static BYTE Read_MPU401_Data(void)
{
    register int a;
    // check read status
    for(a=0; (inportb(MPUBase + 1) & MPU401_OK2RD); a++)if(a>100)break;
    return(inportb(MPUBase));
}

/************************************************************************
 * RESET_MPU401: resets the MPU401 port.
 * Input: none
 * Output: result
 * note: Error will occur if UART mode is not turned off first.
 ************************************************************************/
static int Reset_MPU401(void)
{
	int i = 100;
	MPU_Cmd(MPU401_RESET);       // send reset command
	while((i > 0) && (Read_MPU401_Data() != MPU401_CMDOK))i--;
	return i;                    // offer timeout instead of lockup
}

/************************************************************************
 * SET_UART_MODE: puts the MPU401 port in UART mode.
 * Input: state (Reset will return error if UART mode is on when reset occurs)
 * Output: result
 ************************************************************************/
static int Set_UART_Mode(int state)
{
	int i = 100;
	if(state)
	{
		MPU_Cmd(MPU401_UART);    // turn on UART mode
		while((i > 0) && (Read_MPU401_Data() != MPU401_CMDOK))i--;
	}                                     // offer timeout instead of lockup
	else
		MPU_Cmd(MPU401_RESET); // turn off UART mode
	return i;
}

static int Init_MPU(void)
{
	if(!Reset_MPU401() || !Set_UART_Mode(1))
		return -1;

	return 0;
}

#endif

static int MPU_Detect(void)
{
#ifdef linux
	const char *s = getenv("MIDEV");
	if(s)mididevno = atoi(s);

	seqfd = open("/dev/sequencer", O_WRONLY, 0);
	if(seqfd < 0)seqfd = open("/dev/sound/sequencer", O_WRONLY, 0);
	if(seqfd < 0)return -1;
	return 0;
	
#endif

#ifdef ALLEGRO_VERSION
	if(!midi_mpu401.detect(FALSE))return -1;
	return 0;
#endif

#ifdef __BORLANDC__
	
	BYTE tmp;
	
	fillsbenv();

	MPUBase = 0x330;
	if(sbenv.set && sbenv.P)MPUBase = sbenv.P;

	tmp = inportb(MPUBase+1);
	if(tmp & MPU401_OK2WR)return -1; /* FIXME: Is this really correct? */
	
	Set_UART_Mode(0);                /* just in case it was left on */
	if(Reset_MPU401() < 0)return -1; /* does reset work?            */
	
	Init_MPU();

	return 0;
#endif
}

static void MPU_Reset(void)
{
#ifndef linux
    Init_MPU();
#endif
    GM_Reset();
}

static void MPU_Close(void)
{
#ifdef linux
	if(seqfd > 0)close(seqfd);
#else
	MPU_Reset();
#endif
}

static void MPU_Tick(unsigned numticks)
{
	numticks = numticks;
	/* MPU is a real time device, does not need timing */
}

struct MusDriver MPUDriver =
{
#ifdef linux
	"Any OSS compatible MIDI device",
	
	"This driver supports at least my AWE32.\n"
	"To select the midi device, use shell variable:\n"
	"   export MIDEV=0\n"
	"Default: 1\n",
	
#else	

    "General MIDI (Roland MPU-401 interface or 100% compatible)",
    
	"Most General MIDI synthesizers support the Roland MPU-401 MIDI\n"
	"hardware interface standard. MPU-401 devices generally offer the\n"
	"highest level of performance and sound quality available, but\n"
	"because it uses the port 330h, users of Adaptec SCSI controllers\n"
	"may have some problems with this. If you are using Creative Labs\n"
    "AWE-32 or AWE-64 and you are not using Windows, you should select\n"
    "\"Creative Labs AWE-32 / AWE-64 General MIDI\" instead, because the\n"
    "aweutil.com seems to be unstable sometimes... If you have AWE-32 or\n"
    "AWE-64 and you are using Windows, you should maybe select this\n"
	"selection instead because this can be more stable under Windows :)\n",
#endif
	
	'm', /* optchar */
	
	MPU_Detect,
	MPU_Reset,
	MPU_Close,
	
	MPU_Byte,
	GM_OPLByte,
	
	GM_Controller,
	GM_DPan,
	GM_Bend,
	GM_KeyOff,
	GM_DPatch,
	GM_DNoteOn,
	GM_DTouch,
	GM_HzTouch,
	MPU_Tick
};
