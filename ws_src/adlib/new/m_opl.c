#include "m_opl.h"
#include "m_sbenv.h"

#define SUPPORT_OPL3 1

static const char Portit[9] = {0,1,2, 8,9,10, 16,17,18};
static signed char Pans[18];
static const BYTE *Dtab[18];

/*************************************************************/

#ifdef DJGPP

#include "allegro.h"

/* Requires fm_write() to be public in Allegro
 * It normally is not!
 */
extern void fm_write(int reg, unsigned char data);
static void OPL_Byte(byte Index, byte Data)
{
    if(IsOPLThere)fm_write(Index, Data);
}

/* Lazy... */
#define SetBase(a) a

#endif

/*************************************************************/

#ifdef __BORLANDC__

static WORD OPLBase;
static WORD UseBase;
static int OPLMode;

static int SetBase(int c)
{
    switch(OPLMode)
    {
        case 1:
            UseBase = OPLBase;
            if(c<9)return c;
        case 0:
            break;
#if SUPPORT_OPL3
        default:
        {
            int cnum = c/9;
            UseBase = OPLBase + cnum+cnum;
            return c%9;
        }
#endif
    }
    return 9;
}

static void OPL_Byte(byte Index, byte Data)
{
    if(!OPLMode)return;
    asm {
        MOV     AL,Index
        MOV     DX,UseBase
        OUT     DX,AL
        MOV     CX,6
    }
P1: asm {
        IN      AL,DX
        LOOP    P1
        inc dx
        MOV     AL,Data
		OUT     DX,AL
        DEC     DX
		MOV     CX,35
	}
P2: asm {
		IN       AL,DX
		LOOP     P2
}   }

#endif

/*************************************************************/

#ifdef linux

#include <unistd.h>
#include "io.h"

static int did_ioperm=0;

static WORD OPLBase;
static WORD UseBase;
static int OPLMode;

static int SetBase(int c)
{
    switch(OPLMode)
    {
        case 1:
            UseBase = OPLBase;
            if(c<9)return c;
        case 0:
            break;
#if SUPPORT_OPL3
        default:
        {
            int cnum = c/9;
            UseBase = OPLBase + cnum+cnum;
            return c%9;
        }
#endif
    }
    return 9;
}

static void OPL_Byte(BYTE Index, BYTE Data)
{
	register int a;
	outportb(OPLBase, Index);  for(a=0; a<6;  a++)inportb(OPLBase);
	outportb(OPLBase+1, Data); for(a=0; a<35; a++)inportb(OPLBase);
}

#endif

/*************************************************************/

static void OPL_NoteOff(int c)
{
    c = SetBase(c);
    if(c<9)
	{
		int Ope = Portit[c];
		/* KEYON_BLOCK+c seems to not work alone?? */
		OPL_Byte(KEYON_BLOCK+c, 0);
		OPL_Byte(KSL_LEVEL+  Ope, 0xFF);
		OPL_Byte(KSL_LEVEL+3+Ope, 0xFF);
	}
}

/* OPL_NoteOn changes the frequency on specified
   channel and guarantees the key is on. (Doesn't
   retrig, just turns the note on and sets freq.) */
/* Could be used for pitch bending also. */
static void OPL_NoteOn(int c, int Herz)
{
	int Oct;

    c = SetBase(c);
    if(c >= 9)return;

#if 1
	for(Oct=0; Herz>0x1FF; Oct++)Herz >>= 1;
#else
	for(Oct=-1; Herz>0x1FF; Oct++)Herz >>= 1;
	if(Oct<0)Oct=0;
#endif

/*
    Bytes A0-B8 - Octave / F-Number / Key-On

        7     6     5     4     3     2     1     0
     +-----+-----+-----+-----+-----+-----+-----+-----+
     |        F-Number (least significant byte)      |  (A0-A8)
     +-----+-----+-----+-----+-----+-----+-----+-----+
     |  Unused   | Key |    Octave       | F-Number  |  (B0-B8)
	 |           | On  |                 | most sig. |
     +-----+-----+-----+-----+-----+-----+-----+-----+
*/

    /* Ok - 1.1.1999/Bisqwit */
    OPL_Byte(0xA0+c, Herz&255);  //F-Number low 8 bits
    OPL_Byte(0xB0+c, 0x20        //Key on
                      | ((Herz>>8)&3) //F-number high 2 bits
                      | ((Oct&7)<<2)
          );
}

static void OPL_Touch(int c, const BYTE *D, WORD Vol)
{
    int Ope;
    #if !1
    int level;
    #endif

    c = SetBase(c);
    if(c >= 9)return;

    Ope = Portit[c];

/*
    Bytes 40-55 - Level Key Scaling / Total Level

        7     6     5     4     3     2     1     0
     +-----+-----+-----+-----+-----+-----+-----+-----+
     |  Scaling  |             Total Level           |
     |   Level   | 24    12     6     3    1.5   .75 | <-- dB
     +-----+-----+-----+-----+-----+-----+-----+-----+
          bits 7-6 - causes output levels to decrease as the frequency
                     rises:
                          00   -  no change
                          10   -  1.5 dB/8ve
                          01   -  3 dB/8ve
                          11   -  6 dB/8ve
          bits 5-0 - controls the total output level of the operator.
                     all bits CLEAR is loudest; all bits SET is the
                     softest.  Don't ask me why.
*/
    #if 1
    
    /* Ok - 1.1.1999/Bisqwit */
    OPL_Byte(KSL_LEVEL+  Ope, (D[2]&KSL_MASK) |
    /*  (63 - ((63-(D[2]&63)) * Vol / 63) )
    */  (63 + (D[2]&63) * Vol / 63 - Vol)
    );
    OPL_Byte(KSL_LEVEL+3+Ope, (D[3]&KSL_MASK) |
    /*  (63 - ((63-(D[3]&63)) * Vol / 63) )
    */  (63 + (D[3]&63) * Vol / 63 - Vol)
	);
    /* Molemmat tekevät saman, tarkistettu assembleria myöten.
    
       The later one is clearly shorter, though
       it has two extra (not needed) instructions.
    */
    
	#else
	
	level = (D[2]&63) - (Vol*72-8);
	if(level<0)level=0;
	if(level>63)level=63;

	OPL_Byte(KSL_LEVEL+  Ope, (D[2]&KSL_MASK) | level);

	level = (D[3]&63) - (Vol*72-8);
	if(level<0)level=0;
	if(level>63)level=63;

	OPL_Byte(KSL_LEVEL+3+Ope, (D[3]&KSL_MASK) | level);
	
    #endif
}

static void OPL_Pan(int c, SBYTE val)
{
    Pans[c] = val;
    /* Doesn't happen immediately! */
}

static void OPL_Patch(int c, const BYTE *D)
{
    int Ope;

	c = SetBase(c);
    if(c >= 9)return;

	Ope = Portit[c];

	OPL_Byte(AM_VIB+           Ope, D[0]);
	OPL_Byte(ATTACK_DECAY+     Ope, D[4]);
	OPL_Byte(SUSTAIN_RELEASE+  Ope, D[6]);
	OPL_Byte(WAVE_SELECT+      Ope, D[8]&3);// 6 high bits used elsewhere

	OPL_Byte(AM_VIB+         3+Ope, D[1]);
	OPL_Byte(ATTACK_DECAY+   3+Ope, D[5]);
	OPL_Byte(SUSTAIN_RELEASE+3+Ope, D[7]);
	OPL_Byte(WAVE_SELECT+    3+Ope, D[9]&3);// 6 high bits used elsewhere

	/* Panning... */
    OPL_Byte(FEEDBACK_CONNECTION+c, 
        (D[10] & ~STEREO_BITS)
            | (Pans[c]<-32 ? VOICE_TO_LEFT
                : Pans[c]>32 ? VOICE_TO_RIGHT
                : (VOICE_TO_LEFT | VOICE_TO_RIGHT)
            ));
}

static void OPL_Reset(void)
{
	int a;

	#ifdef linux
	ioperm(OPLBase, 4, 1);
	#endif
	
    #if SUPPORT_OPL3

	for(a=0; a<9; a++)OPL_NoteOff(a);

	/* Base should be set already ok. */
	OPL_Byte(TEST_REGISTER, ENABLE_WAVE_SELECT);
	OPL_Byte(PERCOSSION_REGISTER, 0x00);          /* Melodic mode. */

    #ifndef DJGPP
    if(OPLMode == 2)   /* OPL3 */
	{
		for(a=9; a<18; a++)OPL_NoteOff(a);
		/* Base should be set already ok. */

		OPL_Byte(OPL3_MODE_REGISTER, OPL3_ENABLE);
		OPL_Byte(CONNECTION_SELECT_REGISTER, 0x00);
	}
    #endif

    #else

    for(a=0; a<244; a++)
        OPL_Byte(a, 0);

	#endif
}

int OPL_Detect(void)
{
#ifdef ALLEGRO_VERSION

    if(!midi_adlib.detect(FALSE))return -1;
    
    midi_adlib.init(FALSE, 0);
    
    return 0;
        
#else
	register BYTE ST1;
    #if !SUPPORT_OPL3
    register BYTE ST2;
    #endif
    
    fillsbenv();

	OPLBase = sbenv.A > 0 ? sbenv.A : 0x388;

	SetBase(0);

	#ifdef linux
	if(!did_ioperm)
	{
		if(ioperm(OPLBase, 4, 1) < 0)return -1;
		did_ioperm=1;
	}
	#endif
	
	/* Reset timers 1 and 2 */
	OPL_Byte(TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);

	/* Reset the IRQ of the FM chip */
	OPL_Byte(TIMER_CONTROL_REGISTER, IRQ_RESET);
	
	ST1 = inportb(OPLBase); /* Status register */

    #if SUPPORT_OPL3
    
    OPLMode = 0;

    if(ST1 == 0x02)           /* ? */
    {
    	OPLMode = 1;
    }
    else if(ST1 == 0x06)      /* OPL2 */
	{
		OPLMode = 1;
	}
    else if(ST1 == 0x00 || ST1 == 0x0F)    /* OPL3 or OPL4 */
    {
		SetBase(9); /* Select right op */

		OPL_Byte(OPL3_MODE_REGISTER, 0);
		OPL_Byte(OPL3_MODE_REGISTER, OPL3_ENABLE);
		OPL_Byte(OPL3_MODE_REGISTER, 0);

        SetBase(0);
        
        OPLMode = 2;
    }
    
    #else /* No OPL3 support */
    
    OPL_Byte(TIMER1_REGISTER, 255);
    OPL_Byte(TIMER_CONTROL_REGISTER, TIMER2_MASK | TIMER1_START);
    
	_asm xor cx,cx;P1:_asm loop P1
	ST2 = inportb(OPLBase);
	
	OPL_Byte(TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);
	OPL_Byte(TIMER_CONTROL_REGISTER, IRQ_RESET);
    OPLMode = (ST2&0xE0)==0xC0 && !(ST1&0xE0);
    
    #endif
    
    if(!OPLMode)return -1;
    
    return 0;
#endif
}

static void OPL_Close(void)
{
	OPL_Reset();
}

static void GM_Byte(BYTE b)
{
	b = b;
}
static void OPL_Controller(int ch, BYTE ctrl, BYTE value)
{
	ch = ch;
	ctrl = ctrl;
	value = value;
}
static void OPL_Bend(int ch, WORD val)
{
	ch = ch;
	val = val;
}
static void OPL_DPatch(int ch, const BYTE *D, BYTE GM, BYTE bank)
{
	GM = GM;
	bank = bank;
	OPL_Patch(ch, Dtab[ch] = D);
}
static void OPL_DTouch(int ch, BYTE vol, BYTE adlvol)
{
	if(!Dtab[ch])return;
	vol = vol;
	OPL_Touch(ch, Dtab[ch], adlvol);
}
static void OPL_DNoteOn(int ch, BYTE note, int hz, BYTE vol, BYTE adlvol)
{
	note = note;
	OPL_NoteOn(ch, hz);
	OPL_DTouch(ch, vol, adlvol);
}
static void OPL_Tick(unsigned numticks)
{
	numticks = numticks;
	/* OPL is a real time device, does not need timing */
}

struct MusDriver OPLDriver =
{
    "OPL3/FM synthesis, SB/AdLib -like cards etc...",

    "This driver can be used with any Ad Lib-compatible FM music\n"
    "synthesizer card (including Sound Blaster compatibles) that\n"
    "does not rely on software TSRs for Ad Lib FM compatibility.\n"
    "Users of more sophisticated General MIDI-based cards should\n"
    "select the Ad Lib driver only if problems occur with the card's\n"
    "General MIDI driver, as the Ad Lib OPL chipset does not offer\n"
    "the superior sound quality available with more modern devices.",
	
	'o', /* optchar */
	
	OPL_Detect,
	OPL_Reset,
	OPL_Close,
	
	GM_Byte,
	OPL_Byte,
	
	OPL_Controller,
	OPL_Pan,
	OPL_Bend,
	OPL_NoteOff,
	OPL_DPatch,
	OPL_DNoteOn,
	OPL_DTouch,
	OPL_NoteOn,
	
	OPL_Tick
};
