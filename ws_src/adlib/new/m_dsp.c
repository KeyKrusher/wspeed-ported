#include "m_dsp.h"
#include "m_midi.h"
#include "m_sbenv.h"

#ifdef __BORLANDC__
#include <dos.h>
#endif

#ifdef linux
#include "io.h"
#endif

static unsigned DSPBase;

static void DSP_Byte(BYTE data)
{
    /* Wait for write buffer ready */
    while(inportb(DSPBase + DSP_WR_ST) & 0x80);
    /* Send MIDI write command */
    outportb(DSPBase + DSP_WR, MIDI_OUT_P);
    /* Wait for write buffer ready */
    while(inportb(DSPBase + DSP_WR_ST) & 0x80);
    /* Send MIDI data byte */
    outportb(DSPBase + DSP_WR, data);
}

/************************************************************************
 * DSPReset: Resets the DSP on the Sound Blaster card.
 * Input:  none
 * Output: result
 ************************************************************************/
static int DSPReset(void)
{
    int i, j, result=1;  
    outportb(DSPBase + DSP_RST, 1);
    for(i=0; i<100; i++);   /* delay more than 3us */
    outportb(DSPBase + DSP_RST, 0);  
    for(i = 0; (i < 32) && (result != DSP_RST_OK); i++)
    {
        for(j=0; (j < 512) && (inportb(DSPBase + DSP_RD_ST) < 0x80); j++);
        if(j < 512)result = inportb(DSPBase + DSP_RD);
    }
    if(result == DSP_RST_OK)result = 0;  
    return result;
}

static int DSP_Detect(void)
{
	fillsbenv();
	
    DSPBase = 0x220;
    
    if(sbenv.set && sbenv.A)
    	DSPBase = sbenv.A;

    if(DSPReset())return -1;
    
	return 0;
}

static void DSP_Reset(void)
{
	DSPReset();
}

static void DSP_Close(void)
{
	DSP_Reset();
}

static void DSP_Tick(unsigned numticks)
{
	numticks = numticks;
	/* DSP is a real time device, does not need timing */
}

struct MusDriver DSPDriver =
{
    "SB-MIDI support",
    
	"Chances are that this works, but even bigger\n"
	"chances are that this doesn't work at all.",
	
	'd', /* optchar */
	
	DSP_Detect,
	DSP_Reset,
	DSP_Close,
	
	DSP_Byte,
	GM_OPLByte,
	
	GM_Controller,
	GM_DPan,
	GM_Bend,
	GM_KeyOff,
	GM_DPatch,
	GM_DNoteOn,
	GM_DTouch,
	GM_HzTouch,
	
	DSP_Tick
};
