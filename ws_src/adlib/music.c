/* Some necessary stack options */
#ifdef __BORLANDC__
 #pragma option -N-
 #pragma option -k-
#endif

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "music.h"
#include "m_opl.h"
#include "m_mpu.h"
#if SUPPORT_AWE
#include "m_awe.h"
#endif

#ifdef __GNUC__
 #include <allegro.h>
#endif

#if SUPPORT_MIDI
int ChorusLevel=30*127/100;
int ReverbLevel=40*127/100;
#endif

#ifndef MIDISlideFIX
 #define MIDISlideFIX 1
 /* Changes midi volumes between 1..11 to be 11 */
#endif

/* IMPORTANT NOTE: You can load the data only from memory.          *
 *                 To create the data buffer, use INTGEN program.   */

/* Important note 2, 26.9.1998 by Bisqwit:                          *
 *   The AWE support is experimental. It has not been tested yet.   *
 *   Also, the library file is only for Large model. To use another *
 *   model, you need another library file. Get it from Bisqwit.     */

/* Important note 3, 31.12.1998 (2:17) by Bisqwit:                  *
 *   See note about SEx command in "To fix" section of announcement */

/* 13.6.1999 14:03 - Added auto-SDX (GM1&xx) */

/* 1.10.1998 2:15 Bisqwit announces:  (wake-up is 6:30)

     Supported commandinfobytes currently include:
        Axx (set speed (ticks))
        Bxx
        Cxx
        Dx0 (volume slide up   ticks*x)
        D0y (volume slide down ticks*y)
        DxF (increase volume by y)
        DFy (decrease volume by y)
        DFF (maximizes volume)

        EFx (freqdec)           ("jotain sinne p„in", difficult to emulate)
        Exx (freqslide)         ("jotain sinne p„in", difficult to emulate)

        FFx (freqinc)           ("jotain sinne p„in", difficult to emulate)
        Fxx (freqslide)         ("jotain sinne p„in", difficult to emulate)

        Hxx (if VIBRATO #defined) (incorrect yet, difficult to emulate)
        Kxx (if VIBRATO #defined) (incorrect yet, difficult to emulate)

        Jxx (arpeggio)
        Pxx (NON-STANDARD! Specify arpeggio speed - default=32h (50Hz))
            (If bit 7 set, then arpeggio is only with two notes, not three.)

        Xxx (panning, not fully supported for FM. 00..FF, 80h=middle)

        Qxy (under development - seems not to be working correctly.)

		SBx (patternloop, x>0?count:start)
		SCx (notecut in   x ticks)
		SDx (notedelay    x ticks)
		SEx (patterndelay x lines)

		MBx (NON-STANDARD: reverb)
		MCx (NON-STANDARD: chorus)

		Txx (set tempo)


     By definition, rows played in minute is 24 * tempo / ticks.
     That means, in a second, it is 2/5*tempo/ticks.
     Example: T7D, A06: tick length is 5 / (125+125) = 1 / 50 seconds,
                        six ticks in row, means row is 6 / 50 seconds,
                                       25/3 = 8.333.. rows per second.
            timer ticks per row = rate * speed * 5 / (tempo << 1)

     Exx and Fxx:

       For Exx, FreqSlide =  xx
       For Fxx, FreqSlide = -xx

       In each frame (there are Axx frames in a row)
        the Period of note will be increased by FreqSlide.
       The output Herz is Period * C2SPD of instrument / 22000.

     Hxy and Kxy:
       For Kxy: first do Dxy and then set xy to be the last Hxy xy.

       Vibrato amplitude: y/16       semitones
       Vibrato speed:     x*ticks/64 cycles in second
       Ticks is the value set by Axx -command.

     To make:

        Rxy
        Gxx
        Lxy

     To fix:

        DFx (somehow... grain? karkea)
        DxF (this too)
		EFx
		EEx
		Exx
		FFx
		FEx
		Fxx
		Hxx
		Kxx
		Qxy
		SEx (does not appear to handle command repeatings correctly)
		Uxx
*/

char *MusVersion = __FILE__" "__TIME__" "__DATE__;
char SoundError[128];

/* These variables tell which cards have been detected */
struct sbenv sbenv;
int IsOPLThere=0;
#if SUPPORT_MIDI
int IsMPUThere=0;
#if SUPPORT_DSP
int IsDSPThere=0;
#endif
#endif
#if SUPPORT_AWE
int IsAWEThere=0;
#endif

struct MusData MusData;

int IRQActive = 0;

/* Constants for IRQ subsystem */
unsigned int Rate = 1000;
long DelayCount = 0;

/* Variables for IRQ subsystem */
#ifndef ALLEGRO_VERSION
typedef void interrupt (*IRQType)(void);
IRQType OldI08;
static word Clock;
static word Counter;
#endif

#if VIBRATO
static const byte avibtab[128] =
{   0,1,3,4,6,7,9,10,12,14,15,17,18,20,21,23,
    24,25,27,28,30,31,32,34,35,36,38,39,40,41,42,44,
    45,46,47,48,49,50,51,52,53,54,54,55,56,57,57,58,
    59,59,60,60,61,61,62,62,62,63,63,63,63,63,63,63,
    64,63,63,63,63,63,63,63,62,62,62,61,61,60,60,59,
	59,58,57,57,56,55,54,54,53,52,51,50,49,48,47,46,
    45,44,42,41,40,39,38,36,35,34,32,31,30,28,27,25,
    24,23,21,20,18,17,15,14,12,10,9,7,6,4,3,1
};
#endif

#if SUPPORT_MIDI
static void DeMidiDouble(int c)
{
    GM_KeyOff(c);
    MusData.Active[c] = 0;
    if(MusData.Doubles[c])
    {
        GM_KeyOff(MusData.Doubles[c]-1);
        MusData.Active[MusData.Doubles[c]-1] = 0;
        MusData.Doubles[c] = 0;
    }
}

static void MidiDouble(int c, int NNum, int Instru, byte Vol)
{
    if(MusData.Active[c])DeMidiDouble(c);

    GM_Bank(c, MusData.Instr[Instru]->Bank & 127);
	GM_Patch(c, MusData.Instr[Instru]->GM);
    GM_KeyOn(c, NNum, Vol);

    MusData.Active[c] = 1;
    if(MusData.Instr[Instru]->Volume&64) /* Double channel mode? */
    {
        int d;
        /* Double-hommaan olisi hyv„ k„ytt„„ niit„         *
         * kanavia, joita ei muuten k„ytet„ musiikissa...  *
         * FIXME.                                          */
        for(d=MAXCHN; --d>=0; )if(!MusData.Active[d])break;
        if(d>=0)
        {
            GM_Bank(d, MusData.Instr[Instru]->Bank & 127);
			GM_Patch(d, MusData.Instr[Instru]->GM);
            GM_KeyOn(d, NNum, Vol);
            MusData.Doubles[c]=d+1;
            MusData.Active[d]=1;
}   }   }

static void ControlMidiDouble(int c, int a, int b)
{
    GM_Controller(c, a, b);
    if(MusData.Doubles[c])GM_Controller(MusData.Doubles[c]-1, a, b);
}

static void BendMidiDouble(int c, word Count)
{
    GM_Bend(c, Count);
    if(MusData.Doubles[c])GM_Bend(MusData.Doubles[c]-1, Count);
}

static void TouchMidiDouble(int c, byte Vol)
{
    GM_Touch(c, Vol);
    if(MusData.Doubles[c])GM_Touch(MusData.Doubles[c]-1, Vol);
}
#else /* No midi support */
 #define DeMidiDouble(c)
 #define MidiDouble(c,NNum,Instru,Vol)
 #define ControlMidiDouble(c,a,b)
 #define BendMidiDouble(c,Count)
 #define TouchMidiDouble(c,Vol)
#endif

void PlayLine(void)
{
    struct MusPlayerPos ToSave;
	byte ByteRead, Cmd, Puh, Note, Instru, Info, Oct;
    int Restore, Cut, Break;
	int CutLine, BreakOrder, CurChn;
	byte GotCmd[MAXCHN];

    if(MusData.PatternDelay)
	{
        for(Cmd=0; Cmd<MAXCHN; MusData.CurPuh[Cmd++]=255);
        MusData.PatternDelay--;
		return;
	}

    ToSave = MusData.posi;

    for(Cmd=Restore=Cut=Break=0;
		Cmd<MAXCHN; GotCmd[Cmd++]=0);

    if(MusData.PendingSB0)
    {
        /* Do saving at the pattern first line! */
        MusData.saved = ToSave;
        MusData.PendingSB0 = 0;
    }

    if(MusData.posi.Wait)
	{
        MusData.posi.Wait--;
	}

    if(!MusData.posi.Wait)
    {
        /* Iso for-lause, y„k */
        for(;;)
        {
            int a;
    
            ByteRead = *MusData.posi.Pos++;
            if(ByteRead < 16)break;
    
            Puh    = 255;
            CurChn = ByteRead&15; /* There are no hdr.channels anymore */
            Instru = MusData.CurInst[CurChn];
            Info = 0;
    
            if(ByteRead & 16)
                Info = *MusData.posi.Pos++;
    
            if(ByteRead & 32) /* Note(+instru) specified? */
            {
                a = MusData.InstNum[CurChn];
                Puh = *MusData.posi.Pos++;
    
                if(!(MusData.Hdr->OrdNum&256)) /* What does this mean? */
                {
    GetInsNum:      MusData.InstNum[CurChn] = a = *MusData.posi.Pos++;
                    /* Normaali tilanne */
                }
                else if(Puh&128)
                {
                    if(Puh<254)Puh&=127;
                    goto GetInsNum;
				}
                if(a)
                {
                    MusData.CurInst[CurChn] = Instru = a;
                    MusData.CurVol[CurChn] =
                        (long)(MusData.Instr[Instru]->Volume&63)<<volshift;
                }
                Oct = Puh>>4;
                Note= Puh&15;
            }
    
            if(ByteRead & 64) /* Volume specified? */
            {
                a = *MusData.posi.Pos++;
                MusData.CurVol[CurChn] = (long)a<<volshift;
                MusData.NoteDelay[CurChn] = 0;
            }
    
            Cmd = '-';
            if(ByteRead & 128) /* Cmd(+infobyte) specified? */
                GotCmd[CurChn] = Cmd = (*MusData.posi.Pos++) | 64;
    
            if(Puh != 255)
            {
                MusData.NoteCut[CurChn] = 0;
                MusData.NoteDelay[CurChn] = 0;
                #if 0
                if(Cmd != 'X')
                {
                    ControlMidiDouble(CurChn, 10, 64); /* Middle, that is. */
                    OPL_Pan(CurChn, 64);
                }
                #endif
            }
    
            if((MusData.CurPuh[CurChn]=Puh) < 254)
            {
                int r;
    
                MusData.NNum[CurChn] = (12*Oct) + Note;
                MusData.NNum[CurChn] += MusData.Instr[Instru]->FT - 2;
                /* MIDI finetuning. -2 is a hack. */
    
                r = ((MusData.Instr[Instru]->D[9] >> 2) & 15);
    
				if(r)
                {
                    r = rand() % (r + 1);
                    MusData.NoteDelay[CurChn] =
                        (int)(r * (long)Rate * 5 / (MusData.Tempo<<1));
                }
    
                if(MusData.Instr[Instru]->Volume&128) /* AdLib finetuning also? */
                {
                    Note = MusData.NNum[CurChn]%12;
                    Oct  = MusData.NNum[CurChn]/12;
                }
                MusData.Herz[CurChn] = /* Period is defined in m_opl.c */
                    (unsigned long)(Period[Note] * MusData.Instr[Instru]->C2Spd
                                                 / 22000L) << Oct;
            }
    
            switch(Cmd)
            {
                case 'A':
                    MusData.Speed = Info;
                    break;
                case 'T':
                    MusData.Tempo = Info;
                    break;
                case 'J':
                    if(Info)MusData.Arpeggio[CurChn] = Info;
                    break;
                case 'P':
                    if(Info)
                        MusData.ArpegSpd = Info;
                    break;
    #if RETRIG
                case 'Q':
                    if(Info)MusData.Retrig[CurChn] = Info;
                    break;
    #endif
                case 'B':
                    Break = 1;
                    BreakOrder = Info;
                    break;
                case 'X':
                {
                    OPL_Pan(CurChn, Info/2);
                    ControlMidiDouble(CurChn, 10, Info/2);
					break;
                }
    
                case 'D':
                case 'K':
                    if((ByteRead&64)
                    && !MusData.CurVol[CurChn])
                        MusData.CurVol[CurChn]=1L<<volshift;
    
                    if(!Info)Info = MusData.DxxDefault[CurChn];
                    MusData.DxxDefault[CurChn] = Info;
    
                    if(Info==0xFF)MusData.CurVol[CurChn]=63UL<<volshift;
                                                             /* DFF, maximize */
                    else if(Info > 0xF0)                     /* DFx           */
                    {
                        MusData.VolSlide[CurChn] = -(int)(Info&0x0F);
                        MusData.FineVolSlide[CurChn] = 1;
                    }
                    else if((Info&0x0F)==0x0F && (Info&0xF0)) /* DxF */
                    {
                        MusData.VolSlide[CurChn] = Info>>4;
                        MusData.FineVolSlide[CurChn] = 1;
                    }
                    else if(!Info);                           /* D00 */
                    else if(!(Info&0xF0))                     /* D0x */
                    {
                        MusData.VolSlide[CurChn] = -(Info&0x0F);
                        MusData.FineVolSlide[CurChn] = 0;
                    }
                    else if(!(Info&0x0F))                     /* Dx0 */
                    {
                        MusData.VolSlide[CurChn] = Info>>4;
                        MusData.FineVolSlide[CurChn] = 0;
                    }
    #if VIBRATO
                    if(Cmd=='K'){Info=0; goto CmdH;}
    #endif
                    break;
                case 'E':
                    MusData.FineFrqSlide[CurChn] = 0;
                    if(!Info)Info=MusData.ExxDefault[CurChn];
                    MusData.ExxDefault[CurChn]=Info;
                    if(Info>=0xF0)MusData.FineFrqSlide[CurChn]=1, Info&=15;
                    MusData.FreqSlide[CurChn] = -Info;
					break;
				case 'N':
                    // Bend limits: 0x0000 - 0x3FFF
                    // 0x80 << 6 = 0x2000
                    // 0x00 << 6 = 0x0000
                    // 0xFF << 6 = 0x3FE0 or similar
                    BendMidiDouble(CurChn, (unsigned)Info << 6);
					break;
                case 'F':
                    if(!Info)Info=MusData.FxxDefault[CurChn];
                    MusData.FxxDefault[CurChn]=Info;
                    MusData.FineFrqSlide[CurChn] = 0;
                    if(Info>=0xF0)MusData.FineFrqSlide[CurChn]=1, Info&=15;
                    MusData.FreqSlide[CurChn] = Info;
                    break;
    #if VIBRATO
                case 'H':
                {
    CmdH:           if(!Info)Info=MusData.HxxDefault[CurChn];
                    MusData.HxxDefault[CurChn]=Info;
                    if(Info & 0x0F)MusData.VibDepth[CurChn] = Info & 0x0F;
                    if(Info & 0xF0)MusData.VibSpd  [CurChn] =(Info & 0xF0) >> 2;
                    MusData.VibPos[CurChn] += MusData.VibSpd[CurChn];

                    ControlMidiDouble(CurChn, 1,
                        (MusData.VibDepth[CurChn]+
                         MusData.VibSpd  [CurChn]) << 2);

                    break;
                }
    #endif
                case 'C':
                    Cut = 1;
					CutLine = (Info>>4)*10 + (Info&15);
					break;
				case 'M':
					if(!Info)Info = MusData.MxxDefault[CurChn];
					MusData.MxxDefault[CurChn] = Info;
                    if((Info>>4)==0xB) /* reverb */
					{
						ControlMidiDouble(CurChn, 91, (Info&15)<<3);
					}
                    else if((Info>>4)==0xC) /* chorus */
					{
						ControlMidiDouble(CurChn, 93, (Info&15)<<3);
					}
					break;
				case 'S':
					if(!Info)Info = MusData.SxxDefault[CurChn]; /* Notice */
					MusData.SxxDefault[CurChn] = Info;          /* Notice */

					if(Info == 0xB0)
					{
						/* ToSave is what we had on the beginning of line */
						MusData.saved = ToSave;
					}
					else if((Info > 0xB0)&&(Info <= 0xBF))
					{
						if(MusData.LoopCount == 0)
							MusData.LoopCount = Info&0x0F;
						else
							MusData.LoopCount--;

						if(MusData.LoopCount)
						{
							/* RestorePos() immediately after this line */
							Restore = 1;
						}
						else
						{
							/* SavePos() at next line */
							MusData.PendingSB0 = 1;
						}
					}
					else if((Info>>4)==0xC)
                    {
                        /* Mit„k”h„n ideaa olisi komennossa SC0... */
                        MusData.NoteCut[CurChn] = (int)(
                            (Info & 15) * (long)Rate * 5L / (MusData.Tempo<<1));
                    }
                    else if((Info>>4)==0xD)
                    {
                        /* Mit„k”h„n ideaa olisi komennossa SD0... */
                        MusData.NoteDelay[CurChn] = (int)(
                            (Info & 15) * (long)Rate * 5 / (MusData.Tempo<<1));
					}
					else if((Info>>4)==0xE)
                    {
                        /* Mit„k”h„n ideaa olisi komennossa SE0... */
                        MusData.PatternDelay = Info - 0xE0;
                    }
            }
        }
        if(ByteRead==15)MusData.posi.Wait = *MusData.posi.Pos++;
        else if(ByteRead>0)MusData.posi.Wait=ByteRead+1;
    }

	for(CurChn=0; CurChn<MAXCHN; CurChn++)
	{
		Cmd = GotCmd[CurChn];

        if(Cmd != 'D' && Cmd != 'K')MusData.VolSlide[CurChn] = 0;
        if(Cmd != 'E' && Cmd != 'F')MusData.FreqSlide[CurChn] = 0;
        if(Cmd != 'J')MusData.Arpeggio[CurChn]=0;
#if RETRIG
        if(Cmd != 'Q')MusData.Retrig[CurChn]=0;
#endif
#if VIBRATO
        if(Cmd != 'H' && Cmd != 'K')
        {
            if(MusData.VibDepth[CurChn]
            || MusData.VibSpd[CurChn])
                ControlMidiDouble(CurChn, 1, 0);

            MusData.VibDepth[CurChn] =
            MusData.VibSpd  [CurChn] =
            MusData.VibPos  [CurChn] = 0;
        }
#endif
    }
    if(Restore)
    {
        /* If got SBx _now_ */
        MusData.posi = MusData.saved;
        return;
    }
    if(!MusData.PatternDelay)MusData.posi.Row++;

    if(!Cut && !Break
    && MusData.posi.Pos >= MusData.Patn[MusData.posi.Pat].Ptr
                         + MusData.Patn[MusData.posi.Pat].Len)
    {
        MusData.posi.Ord = (MusData.posi.Ord+1) % (byte)MusData.Hdr->OrdNum;
        goto P3;
    }

    if(Break)
    {
        MusData.posi.Ord = BreakOrder;
P3:     if(!Cut)CutLine = 0;
        goto P2;
    }
    if(Cut)
    {
P1:     MusData.posi.Ord = (MusData.posi.Ord+1) % (byte)MusData.Hdr->OrdNum;
P2:     MusData.posi.Pat = MusData.Orders[MusData.posi.Ord];
        MusData.posi.Pos = MusData.Patn[MusData.posi.Pat].Ptr;
        MusData.posi.Row = CutLine;
        while(CutLine)
        {
            if(MusData.posi.Wait)MusData.posi.Wait--;
            if(!MusData.posi.Wait)
            {
                for(;;)
                {
                    ByteRead = *MusData.posi.Pos++;
                    if(ByteRead<16)break;
                    if(ByteRead&16)MusData.posi.Pos++;
					if(ByteRead&32)
					{
                        byte a = *MusData.posi.Pos++;
                        if((!(MusData.Hdr->OrdNum&256)) || (a&128))
                            MusData.posi.Pos++;
					}
                    if(ByteRead&64)MusData.posi.Pos++;
                    if(ByteRead&128)MusData.posi.Pos++;
                }
                if(ByteRead==15)MusData.posi.Wait = *MusData.posi.Pos++;
                else if(ByteRead>0)MusData.posi.Wait=ByteRead+1;
            }
            CutLine--;
        }
        /* At this point, we are always on a new order. */
        MusData.PendingSB0 = 1;
    }
}

#ifdef ALLEGRO_VERSION
#define irqtype void
#else
#define irqtype void interrupt
#endif

void TimerHandler(void)
{
	static int Frame=0;
	static int FCount;
	int Limit, CurChn;
    char Gaah[MAXCHN];

    if(!MusData.RowTimer)
	{
		PlayLine();
		Frame=-1;
		FCount=0;
        MusData.PlayNext = 1;
	}

    Limit = (int)((long)Rate * (long)(MusData.Speed)
                             * 5L / (MusData.Tempo<<1));
    /* <<1 ja *5 tekev„t *2.5 joka on se oikea mitoitus */

	if(!FCount)
	{
		Frame++;
        FCount = (int)((long)Rate * 5L / (MusData.Tempo<<1));
        for(CurChn=0; CurChn<MAXCHN; CurChn++)Gaah[CurChn]=1;
	}
	FCount--;

    /* RowTimer kertoo kuinka monta irq:ta j„ljell„
     * ennen kuin seuraava PlayLine kutsutaan.
     * Limit kertoo t„m„nhetkisen pituuden samassa
     * yksik”ss„. Limitti„ k„ytt„„ esim. Dxx:n mitoitus.
     */

    if(!MusData.RowTimer)MusData.RowTimer = Limit + 1;
    /* +1: Sit„ v„hennet„„n tuolla alempana */

	for(CurChn=0; CurChn<MAXCHN; CurChn++)
	{
        long Vol = MusData.CurVol[CurChn];
        int Instru = MusData.CurInst[CurChn];

        if(MusData.NoteCut[CurChn] > 0)MusData.NoteCut[CurChn]--;
        else if(!MusData.NoteCut[CurChn])
		{
            MusData.NoteCut[CurChn] = -1;
			OPL_NoteOff(CurChn);
			DeMidiDouble(CurChn);
		}

		if(MusData.NoteDelay[CurChn] > 0)MusData.NoteDelay[CurChn]--;
		else if(!MusData.NoteDelay[CurChn])
		{
			MusData.NoteDelay[CurChn] = -1;
			switch(MusData.CurPuh[CurChn])
			{
				case 255:   /* Tyhj„ rivi */
					TouchMidiDouble(CurChn, (Vol>>volshift) * (MusData.Instr[Instru]->D[8]>>2) / 63);
					OPL_Touch(CurChn, Instru, Vol>>volshift);
					break;
                case 254:   /* ^^ -rivi */
					OPL_NoteOff(CurChn);
					DeMidiDouble(CurChn);
					break;
                default:    /* Rivi jolla nuotti */
					OPL_NoteOff(CurChn);
                    /* DeMidiDouble(CurChn); Not required because */
                    /*                       MidiDouble() does it */
                    MidiDouble(CurChn, MusData.NNum[CurChn], Instru,
						(Vol>>volshift)
                        * (MusData.Instr[Instru]->D[8]>>2) / 63);

					OPL_Patch(CurChn, Instru);
					OPL_Touch(CurChn, Instru, Vol>>volshift);
                    OPL_NoteOn(CurChn, MusData.Herz[CurChn]);

                    Gaah[CurChn] = 0;
			}
		}
#if RETRIG  /* FIXME */
		if(Frame)
		{
            if(MusData.Retrig[CurChn])
                if(!(Frame%MusData.Retrig[CurChn]))
				{
					OPL_NoteOff(CurChn);
                    /* DeMidiDouble(CurChn); Not required because */
                    /*                       MidiDouble() does it */

                    MidiDouble(CurChn, MusData.NNum[CurChn], Instru,
						(Vol>>volshift)
                        * (MusData.Instr[Instru]->D[8]>>2) / 63);

				/*	OPL_Patch(CurChn, Instru); * Necessary? */
					OPL_Touch(CurChn, Instru, Vol>>volshift);
                    OPL_NoteOn(CurChn, MusData.Herz[CurChn]);
				}
		}
#endif
        if(MusData.Arpeggio[CurChn])
		{
			unsigned long Herz;
			static word ArpClock=0;
			register int Note, Oct;
            word ArpCounter = (((long)(MusData.ArpegSpd&127))<<16)/Rate;
			Oct=0;
	#ifdef ALLEGRO_VERSION
			if(ArpClock+ArpCounter < ArpClock)Oct++;
			ArpClock += ArpCounter;
	#else
			_asm mov ax, ArpCounter
			_asm add ArpClock, ax
			_asm adc word ptr Oct, 0
	#endif
			if(Oct)
			{
                switch((MusData.ArpegCnt[CurChn]++)%(3-(MusData.ArpegSpd>>7)))
				{
					case 0:
                        Note = 0;
						break;
					case 1:
                        Note = MusData.Arpeggio[CurChn] & 15;
						break;
					case 2:
                        Note = MusData.Arpeggio[CurChn] >> 4;
				}

                /*
                BendMidiDouble(CurChn, 0x2000 + Note * 0x1000);
                */

                Note += MusData.NNum[CurChn];

                
                DeMidiDouble(CurChn);
                MidiDouble(CurChn, Note, Instru,
						(Vol>>volshift)
                        * (MusData.Instr[Instru]->D[8]>>2) / 63);


                if(!(MusData.Instr[Instru]->Volume&128))
                    Note -= MusData.Instr[Instru]->FT;
					/* Remove finetuning effect for AdLib temporarily */

				Herz = (unsigned long)(Period[Note%12]
                      * MusData.Instr[Instru]->C2Spd / 22000L)
					  << (Note/12);

				OPL_Touch(CurChn, Instru, Vol>>volshift);
				OPL_NoteOn(CurChn, Herz);
		}   }

        if(MusData.VolSlide[CurChn] && (Gaah[CurChn]||MusData.FineVolSlide[CurChn]))
		{
            if(MusData.FineVolSlide[CurChn])
                Vol += ((long)MusData.VolSlide[CurChn])<<volshift;
			else
                Vol += ((long)(MusData.VolSlide[CurChn] * (long)((MusData.Speed)-1))<<volshift) / Limit;

			if(Vol < (            0))Vol = (            0);
			if(Vol > (64L<<volshift))Vol = (64L<<volshift);

            MusData.CurVol[CurChn] = Vol;

			OPL_Touch(CurChn, Instru, Vol>>volshift);

			TouchMidiDouble(CurChn,
                (Vol>>volshift)
                * (MusData.Instr[Instru]->D[8]>>2) / 63);

            if(MusData.FineVolSlide[CurChn] && !MusData.PatternDelay)
                MusData.VolSlide[CurChn] = 0;
		}

        if(MusData.FreqSlide[CurChn])
		{
			OPL_NoteOn(CurChn,
                MusData.Herz[CurChn] +=
                (((long)(MusData.FreqSlide[CurChn]
                     * ((MusData.Speed)-1))<<14) / Limit) >> 12
			);

            /* BendMidiDouble(CurChn, 0x1000); FIXME - not implemented. */

            if(MusData.FineFrqSlide[CurChn] && !MusData.PatternDelay)
                MusData.FreqSlide[CurChn]=0;
		}

#if VIBRATO
        /* Vibrato - not correctly working yet */
        if(MusData.Herz[CurChn])
        {
			if(MusData.VibDepth[CurChn])
            {
                int VibDpt, VibVal = avibtab[MusData.VibPos[CurChn] & 127];
    
				if(MusData.VibPos[CurChn] & 0x80)VibVal = -VibVal;
                VibDpt = MusData.VibDepth[CurChn] << 8;
                VibVal = (VibVal*VibDpt) >> 12;
    
                OPL_NoteOn(CurChn, MusData.Herz[CurChn] - VibVal*8);
    
                MusData.VibPos[CurChn]++;
            }
        }
#endif
	}
    MusData.RowTimer--;
}

irqtype NewI08(void)
{
#ifndef ALLEGRO_VERSION
    _asm { db 0x66; pusha } /* ASSUME 386+! */
#endif

	/* Check if we can play next row... */
    if(!MusData.Paused)TimerHandler();

    DelayCount++;

#ifndef ALLEGRO_VERSION
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
#endif
}

void StartS3M(const char far *s)
{
	byte a;
	int NotStart=0;

	StopS3M();

	/* I love pointers :) N„in p„„stiin eroon yhdest„ memmovesta. */
    MusData.Hdr = (InternalHdr far *)__FILE__" (C) 1992,99 Bisqwit (bisqwit@iki.fi)";
	/* Please don't modify the text above. It would be nice to   *
	 * see my name inside games made by people unknown to me :-) */

	MusData.Playing = s;
	if(s[1]>2)for(;*s++;); /* Skip possible song name */
    if(Rate == 32767)
	{
        /* midins -u */
		NotStart=1;
        Rate = 1000;
	}

    MusData.Hdr = (const InternalHdr far *)s;
    s += sizeof(InternalHdr);
    if(MusData.Hdr->PatNum & 512)
        s++; /* ChanCount present */

    /* Samalla tapaan t„st„ my”s. */
    MusData.Orders = s;
    s += (byte)MusData.Hdr->OrdNum;

    for(a=0; a<MusData.Hdr->InsNum; a++)
    {
		InternalSample far *tmp = (InternalSample far *)s;
		if(MusData.Hdr->PatNum & 512)
            s += 1 + _fstrlen(tmp->insname);
		s += sizeof(*tmp);
		MusData.Instr[a+1] = tmp;
	}
    ((InternalHdr far *)MusData.Hdr)->PatNum &= ~512;

    for(a=0; a<MusData.Hdr->PatNum; a++)
    {
        MusData.Patn[a].Len = ((unsigned char)s[0])
                            | (((unsigned char)s[1]) << 8);
        MusData.Patn[a].Ptr = (s += 2);
        s += MusData.Patn[a].Len;
    }

    GM_Reset();
    for(a=0; a<MAXCHN; a++)
    {
        MusData.Doubles[a]=
        MusData.Active[a]=
		MusData.CurVol[a]=
		MusData.CurInst[a]=0;

		MusData.NoteDelay[a] = -1;

		GM_Controller(a, 91, ReverbLevel); /* Reverbbi„ peliin!!   */
		GM_Controller(a, 93, ChorusLevel); /* Chorusta peliin!! :) */
	}

	MusData.Paused = 1;
	MusSeekOrd(0);
	MusData.LinearMidiVol = (MusData.Hdr->InSpeed&128) == 128;
	MusData.Tempo = MusData.Hdr->InTempo;
	MusData.Speed = MusData.Hdr->InSpeed & 127;
    MusData.ArpegSpd = 50;

	if(!NotStart)
	{
		__ResumeS3M();
		PauseS3M(); /* unpause. */
	}
}

int FromArchive=0;
char *Archived = NULL;
void StopS3M(void)
{
    if(Archived && !FromArchive)
    {
        free(Archived);
        Archived = NULL;
	}
	MusData.Paused = 0;
	/* It will release IRQ */
	PauseS3M();
	MusData.Paused = 0;
}

void __ResumeS3M(void)
{
    if(IRQActive == 0)
	{
		/* If timer was not active, start it */
        IRQActive = 1;
#ifdef ALLEGRO_VERSION
		install_timer();
		install_int(NewI08, BPS_TO_TIMER(Rate));
#else
		OldI08 = GetIntVec(0x08);
		_asm cli
		SetIntVec(0x08, NewI08);
#endif
	}
#ifndef ALLEGRO_VERSION
	if(IRQActive < 2)
	{
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
#endif
}

void PauseS3M(void)
{
	MusData.Paused = !MusData.Paused;
	if(MusData.Paused)
	{
        if(IRQActive == 1)
		{
		#ifdef ALLEGRO_VERSION
			remove_int(NewI08);
		#else
            IRQActive = 0;
			SetIntVec(0x08, OldI08);
			_asm {
				mov al, 0x34
				out 0x43, al
				xor al, al
				out 0x40, al
				out 0x40, al
			}
		#endif
			OPL_Reset();
		#if SUPPORT_MIDI
			GM_Reset();
        #if SUPPORT_DSP
			DSP_Reset();
        #endif
		#endif
		#if SUPPORT_MPU
			MPU_Reset();
		#endif
		#if SUPPORT_AWE
			AWE_Reset();
		#endif
		}
	}
	else
	{
		__ResumeS3M();
	}
}

void MusSeekOrd(int Num)
{
    int p = MusData.Paused;
    if(!p)PauseS3M();
    /* The player must not modify these at same time */
    MusData.PatternDelay = 0;
    MusData.RowTimer = 0;
    MusData.posi.Wait = 0;
    MusData.posi.Row = 0;
    MusData.posi.Ord = Num;
    MusData.posi.Pat = MusData.Orders[Num];
    MusData.posi.Pos = MusData.Patn[MusData.posi.Pat].Ptr;
    MusData.LoopCount = 0;
    MusData.PendingSB0 = 1;
    if(!p)PauseS3M();
}

void MusSeekNextOrd(void)
{
    if(MusData.posi.Ord<MusData.Hdr->OrdNum-1)MusSeekOrd(MusData.posi.Ord+1);
}

void MusSeekPrevOrd(void)
{
    if(MusData.posi.Ord>0)MusSeekOrd(MusData.posi.Ord-1);
}

void InitMusic(int Autodetect)
{
    IsOPLThere=0;
    #if SUPPORT_MIDI
    IsMPUThere=0;
    #if SUPPORT_DSP
    IsDSPThere=0;
    #endif
    #endif
    #if SUPPORT_AWE
    IsAWEThere=0;
    #endif
    if(Autodetect)
    {
        OPL_Found();
    #if SUPPORT_MIDI
    #if SUPPORT_DSP
        DSP_Found();
    #endif
        MPU_Found();
    #endif
    #if SUPPORT_AWE
        AWE_Found();
    #endif
    }
}

void ExitMusic(void)
{
    StopS3M();
    OPL_Reset();
    #if SUPPORT_MIDI
	MPU_Reset();
    #if SUPPORT_DSP
	DSP_Reset();
    #endif
    #endif
  	#if SUPPORT_AWE
    AWE_Done();
    #endif
}
