#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "music.h"
#include "m_opl.h"

const long Period[12] =
{
    907,960,1016,1076,
    1140,1208,1280,1356,
    1440,1524,1616,1712
};

static const char Portit[9] = {0,1,2, 8,9,10, 16,17,18};

#ifdef ALLEGRO_VERSION

/* Requires fm_write() to be public in Allegro */
extern void fm_write(int reg, unsigned char data);
void OPL_Byte(byte Index, byte Data)
{
    if(IsOPLThere)fm_write(Index, Data);
}

/* Lazy... */
#define SetBase(a) a

#else

static signed char Pans[18] = {0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0};

#ifdef SUPPORT_MIDI
signed char Trig[18] = {0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0};
#endif

#ifndef __BORLANDC__
#define SetBase(a) a
#else

static word OPLBase;
static word UseBase;
static int SetBase(int c)
{
    switch(IsOPLThere)
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

void OPL_Byte(byte Index, byte Data)
{
    if(!IsOPLThere)return;
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
#endif

void OPL_NoteOff(int c)
{
    #ifdef SUPPORT_MIDI
    Trig[c] = 0;
    #endif

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
void OPL_NoteOn(int c, unsigned long Herz)
{
	int Oct;

    #ifdef SUPPORT_MIDI
    Trig[c] = 127;
    #endif

    c = SetBase(c);
    if(c >= 9)return;

	for(Oct=0; Herz>0x1FF; Oct++)Herz >>= 1;

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

void OPL_Touch(int c, int Instru, word Vol)
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
    OPL_Byte(KSL_LEVEL+  Ope, (MusData.Instr[Instru]->D[2]&KSL_MASK) |
    /*  (63 - ((63-(MusData.Instr[Instru]->D[2]&63)) * Vol / 63) )
    */  (63 + (MusData.Instr[Instru]->D[2]&63) * Vol / 63 - Vol)
    );
    OPL_Byte(KSL_LEVEL+3+Ope, (MusData.Instr[Instru]->D[3]&KSL_MASK) |
    /*  (63 - ((63-(MusData.Instr[Instru]->D[3]&63)) * Vol / 63) )
    */  (63 + (MusData.Instr[Instru]->D[3]&63) * Vol / 63 - Vol)
	);
    /* Molemmat tekev„t saman.
      Borland C++:lla (63 - ((63-(k&63)) * a / 63)) k„„ntyy n„in:
        mov     al,byte ptr k
        mov     ah,0   <- stupid
        and     ax,63
        push    ax
        mov     ax,63
        pop     dx
        sub     ax,dx
        imul    word ptr a
        mov     bx,63
        xor     dx,dx <- stupid
        div     bx
        push    ax    <- stupid
        mov     ax,63 <- why not sub ax, 63 ?
        pop     dx    <- stupid
        sub     ax,dx <- why not neg ax ?
      Borland C++:lla (63 + (k&63) * a / 63 - a)    k„„ntyy n„in:
        mov     cx,word ptr a
        mov     al,byte ptr k
        mov     ah,0   <- stupid
        and     ax,63
        imul    cx
        mov     bx,63
        xor     dx,dx  <- stupid
        div     bx
        add     ax,63
        sub     ax,cx
      The later one is clearly shorter, though
      it has two extra (not needed) instructions.
    */
	#else
	level = (MusData.Instr[Instru]->D[2]&63) - (Vol*72-8);
	if(level<0)level=0;
	if(level>63)level=63;

	OPL_Byte(KSL_LEVEL+  Ope, (MusData.Instr[Instru]->D[2]&KSL_MASK) | level);

	level = (MusData.Instr[Instru]->D[3]&63) - (Vol*72-8);
	if(level<0)level=0;
	if(level>63)level=63;

	OPL_Byte(KSL_LEVEL+3+Ope, (MusData.Instr[Instru]->D[3]&KSL_MASK) | level);
    #endif
}

void OPL_Pan(int c, byte val)
{
    Pans[c] = val - 128;
}
void OPL_Patch(int c, int Instru)
{
    int Ope;

	c = SetBase(c);
    if(c >= 9)return;

	Ope = Portit[c];

	OPL_Byte(AM_VIB+           Ope, MusData.Instr[Instru]->D[0]);
	OPL_Byte(ATTACK_DECAY+     Ope, MusData.Instr[Instru]->D[4]);
	OPL_Byte(SUSTAIN_RELEASE+  Ope, MusData.Instr[Instru]->D[6]);
	OPL_Byte(WAVE_SELECT+      Ope, MusData.Instr[Instru]->D[8]&3);// 6 high bits used elsewhere

	OPL_Byte(AM_VIB+         3+Ope, MusData.Instr[Instru]->D[1]);
	OPL_Byte(ATTACK_DECAY+   3+Ope, MusData.Instr[Instru]->D[5]);
	OPL_Byte(SUSTAIN_RELEASE+3+Ope, MusData.Instr[Instru]->D[7]);
	OPL_Byte(WAVE_SELECT+    3+Ope, MusData.Instr[Instru]->D[9]&3);// 6 high bits used elsewhere

	/* Panning... */
    OPL_Byte(FEEDBACK_CONNECTION+c, 
        (MusData.Instr[Instru]->D[10] & ~STEREO_BITS)
            | (Pans[c]<-32 ? VOICE_TO_LEFT
                : Pans[c]>32 ? VOICE_TO_RIGHT
                : (VOICE_TO_LEFT | VOICE_TO_RIGHT)
            ) );
}

int OPL_Reset(void)
{
	int a;

	if(!IsOPLThere)return 0;

    #if SUPPORT_OPL3

	for(a=0; a<9; a++)OPL_NoteOff(a);

	/* Base should be set already ok. */
	OPL_Byte(TEST_REGISTER, ENABLE_WAVE_SELECT);
	OPL_Byte(PERCOSSION_REGISTER, 0x00);          /* Melodic mode. */

    #ifdef __BORLANDC__
    if(IsOPLThere==2)   /* OPL3 */
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

    return 1;
}

#if defined(__BORLANDC__)||defined(DJGPP)

int OPL_Found(void)
{
#ifdef ALLEGRO_VERSION
    IsOPLThere = midi_adlib.detect(FALSE);
    midi_adlib.init(FALSE, 0);
    if(IsOPLThere)
        strcpy(SoundError, midi_adlib.desc);
    else
        strcpy(SoundError, allegro_error);
#else
	register char ST1;
    #if !SUPPORT_OPL3
    register char ST2;
    #endif

	OPLBase = sbenv.A > 0 ? sbenv.A : 0x388;

	SetBase(0);

	/* Reset timers 1 and 2 */
	OPL_Byte(TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);

	/* Reset the IRQ of the FM chip */
	OPL_Byte(TIMER_CONTROL_REGISTER, IRQ_RESET);

	_asm {mov dx, OPLBase; in al,dx; mov ST1, al}/* Status register */

    #if 1 /*SUPPORT_OPL3 */
    IsOPLThere = 0;

    if(ST1 == 0x02)           /* ? */
    {
        IsOPLThere = 1;
    }
    else if(ST1 == 0x06)      /* OPL2 */
	{
        IsOPLThere = 1;
	}
    else if(ST1 == 0x00 || ST1 == 0x0F)    /* OPL3 or OPL4 */
    {
		SetBase(9); /* Select right op */

		OPL_Byte(OPL3_MODE_REGISTER, 0);
		OPL_Byte(OPL3_MODE_REGISTER, OPL3_ENABLE);
		OPL_Byte(OPL3_MODE_REGISTER, 0);

        SetBase(0);
        IsOPLThere = 2;
    }
    #else /* No OPL3 support */
    OPL_Byte(TIMER1_REGISTER, 255);
    OPL_Byte(TIMER_CONTROL_REGISTER, TIMER2_MASK | TIMER1_START);
	_asm xor cx,cx;P1:_asm loop P1
    _asm {mov dx, OPLBase; in al,dx; mov ST2, al}
	OPL_Byte(TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);
	OPL_Byte(TIMER_CONTROL_REGISTER, IRQ_RESET);
    IsOPLThere = (ST2&0xE0)==0xC0 && !(ST1&0xE0);
    #endif
    
    sprintf(SoundError,
        IsOPLThere
            ? "OPL2 was detected at 0x%X (sig 0x%X)"
            : "OPL2 not detected at 0x%X (sig 0x%X)",
            OPLBase, ST1);
#endif

    return IsOPLThere;
}

/****                                   ****/
/****                                   ****/
/**** End of OPL-only section           ****/
/****                                   ****/
/****                                   ****/

/* Moved here!! */
int SelectMusDev(enum cardcs Selection)
{
    char *envvar = getenv("BLASTER");
    if(!envvar)
        sbenv.set = 0;
    else
    {
        sbenv.set = 1;
        sbenv.A = sbenv.D = sbenv.I = sbenv.H = sbenv.E = sbenv.T = -1;
        sbenv.P = 0x330;

        while(*envvar)
        {
            if(*envvar=='A')sbenv.A = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='D')sbenv.D = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='I')sbenv.I = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='H')sbenv.H = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='P')sbenv.P = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='E')sbenv.E = (int)strtol(envvar+1, NULL, 16);
            if(*envvar=='T')sbenv.T = (int)strtol(envvar+1, NULL, 16);
            #if SUPPORT_MIDI
            if(*envvar=='R')ReverbLevel = (int)strtol(envvar+1, NULL, 10)*127/100;
            if(*envvar=='C')ChorusLevel = (int)strtol(envvar+1, NULL, 10)*127/100;
            #endif
            if(!sbenv.E && sbenv.A>0)sbenv.E=sbenv.A+0x400;
            while((*envvar!=' ')&&(*envvar!=0))envvar++;
            if(*envvar)envvar++;
        }
    }

    !(IsOPLThere = (Selection == SelectOPL)) || OPL_Found();
    #if SUPPORT_MIDI
    !(IsMPUThere = (Selection == SelectMPU)) || MPU_Found();
    #if SUPPORT_DSP                      
    !(IsDSPThere = (Selection == SelectDSP)) || DSP_Found();
    #endif
    #endif
    #if SUPPORT_AWE
    !(IsAWEThere = (Selection == SelectAWE)) || AWE_Found();
    #endif

    MusData.GlobalVolume = 64;

    return (
        IsOPLThere
    #if SUPPORT_MIDI
      ||IsMPUThere
    #if SUPPORT_DSP
      ||IsDSPThere
    #endif
    #endif
    #if SUPPORT_AWE
      ||IsAWEThere
    #endif
      ||(Selection==SelectNone)) - 1;
}

#endif /* djgpp or borlandc */
