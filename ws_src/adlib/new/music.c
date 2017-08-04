#include <stdio.h>   /* for printf() */
#include <stdlib.h>
#include <string.h>

#include "music.h"
#include "m_irq.h"
#include "adldata.h" /* for Period[] */
#include <math.h>

/* 
 Supported commandinfobytes currently include:
 
  Axx (set speed (ticks))
  Bxx
  Cxx
  Dx0 (volume slide up   ticks*x)
  D0y (volume slide down ticks*y)
  DxF (increase volume by y)
  DFy (decrease volume by y)
  DFF (maximizes volume)

  EFx (freqdec)    (finally correct!! 2000-09-21 / 3.0.2)
  EEx (UNTESTED)   (extra fine slide down)
  Exx (freqslide)  (-"-)

  FFx (freqinc)    (-"-)
  FEx (UNTESTED)   (extra fine slide up)
  Fxx (freqslide)  (-"-)

  Hxx (nearly correct 2000-09-21 - at least it works on adlib)
  Kxx (-"-)
  Uxx (fine vibrato, UNTESTED. Where is this documented?)
  
  Ixy (Tremor, UNTESTED. Ontime x, offtime y)
  
  Rxy (Tremolo, UNTESTED)

  Gxx (Should work on adlib. *UNTESTED*. / 3.0.2)
  Lxx (-"-)

  Jxx (arpeggio)
  Pxx (NON-STANDARD! Specify arpeggio speed - default=32h (50Hz))
      (If bit 7 set, then arpeggio is only with two notes, not three.)

  Xxx (panning, quite grain in FM... 00..FF, 80h=middle)

  Qxy (BETA)

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
  Timer ticks per row = rate * speed * 5 / tempo / 2

  Exx and Fxx:

    For Exx, PeriodSlide =  xx
    For Fxx, PeriodSlide = -xx

    In each frame (there are Axx frames in a row)
      the Period of note will be increased by PeriodSlide.

  Hxy and Kxy:
  
    For Kxy: first do Dxy and then set xy to be the last Hxy xy.

    Vibrato amplitude: y/16       semitones
    Vibrato speed:     x*ticks/64 cycles in second
    Ticks is the value set by Axx -command.
    
    Uxy is finer than Hxy.
  
  Gxx and Lxy:
  
    For Lxy: first do Dxy and then set xx to be the last Gxx xx.
    
    Tune the period by xx in each tick, until the period is correct.

  To fix:

    SEx (does not appear to handle command repeatings correctly)
    SBx (this has broken in some way)

*/

volatile struct MusData MusData;

static void M_NoteOff(int ch)
{
    MusData.driver->noteoff(ch);
}

static void M_MidiCtrl(int ch, int ctrl, int value)
{
    MusData.driver->midictrl(ch, ctrl, value);
}

static void M_Pan(int ch, int value)
{
    /* Pan limits: -128..+127 */
    MusData.driver->setpan(ch, value - 128);
}

static void M_PeriodTouch(int ch, unsigned Period)
{
    MusData.driver->hztouch(ch, Period ? PERIODCONST / Period : 0);
}

static void M_GmBend(int ch, int val)
{
    MusData.driver->bend(ch, val);
}

static void M_NoteOn(int ch, int note, BYTE vol, unsigned Instru, unsigned Period)
{
    /* M_GmBend(ch, 0x2000);  * redundant */
    MusData.driver->patch(
      ch,
      MusData.Instr[Instru]->D,
      MusData.Instr[Instru]->GM,
      MusData.Instr[Instru]->Bank & 127);
    
    MusData.driver->noteon(
      ch,
      note,
      Period ? PERIODCONST / Period : 0,
      vol * MusData.GlobalVolume / 255 * (MusData.Instr[Instru]->D[8] >> 2) / 63,
      vol * MusData.GlobalVolume / 255);
    
}

static void M_Touch(int ch, unsigned Instru, BYTE vol)
{
    MusData.driver->touch(
      ch,
      vol * MusData.GlobalVolume / 255 * (MusData.Instr[Instru]->D[8] >> 2) / 63,
      vol * MusData.GlobalVolume / 255);
}

static unsigned MakePeriod(unsigned Note, unsigned Oct, unsigned C4Spd)
{
    return (unsigned long)8363 * 16 * Period[11-Note] / C4Spd >> Oct;
}

static void CheckLackingCommands(const BYTE *const GotCmd)
{
    unsigned ch;
    for(ch=0; ch<MAXCHN; ch++)
    {
        BYTE Cmd = GotCmd[ch];

        if(Cmd != 'E' && Cmd != 'F' && Cmd != 'G' && Cmd != 'L'
        && Cmd != 'H' && Cmd != 'K' && Cmd != 'U' && Cmd != 'I')
        {
        	if(MusData.VibDepth[ch] 
        	|| MusData.Portamento[ch]
        	|| MusData.PerSlide[ch])
	            M_PeriodTouch(ch, MusData.ST3Period[ch]);
        }
        if(Cmd != 'D' && Cmd != 'K' && Cmd != 'L')MusData.VolSlide[ch] = 0;
        if(Cmd != 'E' && Cmd != 'F')MusData.PerSlide[ch] = 0;
        if(Cmd != 'G' && Cmd != 'L')MusData.Portamento[ch] = 0;
        if(Cmd != 'I')MusData.Tremor[ch]=0;
        if(Cmd != 'J')MusData.Arpeggio[ch]=0;
        if(Cmd != 'Q')MusData.Retrig[ch]  =0;
        if(Cmd != 'H' && Cmd != 'K' && Cmd != 'U')
        {
            MusData.VibDepth[ch] =
            MusData.VibSpd  [ch] = 0;
        }
        if(Cmd != 'R')
        {
            MusData.TremoDepth[ch] =
            MusData.TremoSpd  [ch] =
            MusData.TremoPos  [ch] = 0;
        }
        if(Cmd != 'R' && Cmd != 'I')
        {
            /* This is used by both of them! */
            MusData.TremoClk[ch] = 0;
        }
    }
}

static void MakeWait(BYTE ByteRead)
{
    /* Skip blank lines */
    if(ByteRead == 15)
        MusData.posi.Wait = *MusData.posi.Pos++ + 16;
    else
        MusData.posi.Wait = ByteRead;
}

void MusSeekRow(unsigned CutLine)
{
    MusData.posi.Pat = MusData.Orders[MusData.posi.Ord];
    MusData.posi.Pos = MusData.Patn[MusData.posi.Pat].Ptr;
    MusData.posi.Row = CutLine;
    MusData.posi.Wait = 0;
    /* There can not be any wait at     *
     * the beginning of pattern, right? */
    while(CutLine)
    {
        if(MusData.posi.Wait)
            --MusData.posi.Wait;
        else if(!MusData.posi.Wait)
        {
            BYTE ByteRead;
            for(;;)
            {
            	if(MusData.posi.Pos >= MusData.Patn[MusData.posi.Pat].Ptr
                                     + MusData.Patn[MusData.posi.Pat].Len)
                {
                    /* Patterni loppui kesken. Ei viivettä. */
                    ByteRead = 0;
                    break;
                }
                if((ByteRead = *MusData.posi.Pos++) < 16)
                {
                    /* Rivi loppui tähän. */
                    break;
                }

                if(ByteRead & 16)MusData.posi.Pos++; /* info */
                if(ByteRead & 32)
                {
                    BYTE a = *MusData.posi.Pos++; /* note */
                    if((!(MusData.InstruOptim)) || (a&128))
                        MusData.posi.Pos++; /* instru */
                }
                if(ByteRead & 64)MusData.posi.Pos++; /* volume */
                if(ByteRead & 128)MusData.posi.Pos++; /* cmd */
            }
            
            MakeWait(ByteRead);
        }
        CutLine--;
    }
    
    printf("Position after seek at row %u: %u\n",
      MusData.posi.Row,
      MusData.posi.Pos - MusData.Patn[MusData.posi.Pat].Ptr);
    
    /* At this point, we are always on a new order.             */
    /* SB0 must always be assumed upon incoming to a new order. */
    MusData.PendingSB0 = 1;
    MusData.LoopCount  = 0;    
}

#define DISPLAYROWS       1
#define HEXDUMP           0
#define DISPLAYFRAMETICKS 0

static void PlayLine(void)
{
    struct MusPlayerPos ToSave;
    int Restore;
    int Cut, CutLine;
    int Break, BreakOrder;
    int CurChn;
    BYTE GotCmd[MAXCHN];
    BYTE Cmd;

    if(MusData.PatternDelay)
    {
        unsigned a;
        /* Set empty row, because we don't want *
         * to retrig the notes while doing SEx. */
        for(a=0; a<MAXCHN; MusData.CurPuh[a++] = 255);
        --MusData.PatternDelay;
        return;
    }

    ToSave = MusData.posi;

    memset(&GotCmd, 0, sizeof GotCmd);
    
    Restore = Cut = Break = 0;

    if(MusData.PendingSB0)
    {
        /* Do saving at the pattern first line! */
        MusData.saved = ToSave;
        MusData.PendingSB0 = 0;
    }

    Cmd = 0;

    if(MusData.posi.Wait)
        --MusData.posi.Wait;
    else if(!MusData.posi.Wait)
    {
        BYTE ByteRead;
        
        /* Rivi loppuu tavuun, jonka neljä ylintä bittiä ovat 0. */
        
        printf("o%02X p%02u r%02u|",
	        MusData.posi.Ord,
    	    MusData.posi.Pat,
        	MusData.posi.Row);
        
#if DISPLAYROWS
        for(ByteRead=0; ByteRead<7; ByteRead++)
        	printf("\33[1;30m... .. .. .00\33[34m|\33[37m");
        	      /*         01234567890123 */
#endif
        
        for(;;)
        {
            BYTE Instru, Info;
            
        	if(MusData.posi.Pos >= MusData.Patn[MusData.posi.Pat].Ptr
                                 + MusData.Patn[MusData.posi.Pat].Len)
            {
                /* Patterni loppui kesken. Ei viivettä. */
                ByteRead = 0;
                break;
            }
            if((ByteRead = *MusData.posi.Pos++) < 16)
            {
                /* Rivi loppui tähän. */
                break;
            }

#if HEXDUMP
			printf("%02X", ByteRead);
#endif
    
            CurChn = ByteRead&15; /* There are no hdr.channels anymore */
            Instru = MusData.CurInst[CurChn];
            Info = 0;
    
            if(ByteRead & 16)
                Info = *MusData.posi.Pos++;
    
            MusData.CurPuh[CurChn] = 255;
            
            if(ByteRead & 32) /* Note(+instru) specified? */
            {
                BYTE a = Instru;
                BYTE Puh = *MusData.posi.Pos++;
    
                if(!MusData.InstruOptim)
                {
                    /* If instrument data has not been optimized, instrument
                     * number always follows the note number.
                     * The optimization can not be used if there
                     * exist notes where oct*16+note >= 128.
                     */
                    
                    a = *MusData.posi.Pos++;
#if DISPLAYROWS
	            	printf("\r\33[%uC\33[35m%02u\33[37m", 12+CurChn*14 + 4, a);
#endif
                }
                else if(Puh&128)
                {
                    /* If the optimization is applied, the instrument
                     * number existence is determined by Note&128.
                     * InstruOptim flags this situation.
                     */
                    
                    if(Puh < 254)Puh &= 127;
                    a = *MusData.posi.Pos++;
#if DISPLAYROWS
	            	printf("\r\33[%uC%02u", 12+CurChn*14 + 4, a);
#endif
                }
                if(a)
                {
                    MusData.CurInst[CurChn] = Instru = a;
                    MusData.CurVol[CurChn] = MusData.Instr[Instru]->Volume & 63;
                    /* Bits 6 and 7 unused */
                }
                
                MusData.CurPuh[CurChn] = Puh;
                
                if(Puh != 255)
                {
                    MusData.NoteCut[CurChn] = 0;
                    MusData.NoteDelay[CurChn] = 1;
                
                    if(Puh != 254)
                    {
                        int r;
                        
                        BYTE Oct  = Puh>>4;
                        BYTE Note = Puh&15;
                        
                        MusData.NNum[CurChn] = (12*Oct) + Note;
                        MusData.NNum[CurChn] += MusData.Instr[Instru]->FT;
                        /* MIDI finetuning. */
            
                        r = ((MusData.Instr[Instru]->D[9] >> 2) & 15);
            
                        if(r)
                        {
                            r = rand() % (r + 1);
                            MusData.NoteDelay[CurChn] = r+1;
                        }
                        
                        /* AdLib finetuning also? */
                        if(MusData.Instr[Instru]->Bank & 128)
                        {
                            /* Recalculate these */
                            Note = MusData.NNum[CurChn] % 12;
                            Oct  = MusData.NNum[CurChn] / 12;
                        }
                        
                        MusData.NNum[CurChn] -= 2; /* Some kind of hack. */
                        
                        MusData.NextPeriod[CurChn] =
                            MakePeriod(Note, Oct, MusData.Instr[Instru]->C4Spd);

#if DISPLAYROWS
	            		printf("\r\33[%uC%c%c%u",
	            			12+CurChn*14,
	            			"CCDDEFFGGAAB"[Note],
	            			"-#-#--#-#-#-"[Note],
	            			Oct);
#endif
                    }
                }
            }
    
            if(ByteRead & 64) /* Volume specified? */
            {
#if DISPLAYROWS
	            /* C-4 10 63 A06|
    	           12345678901234
    	           01234567890
        	    */
            
	            printf("\r\33[%uC%02u", 12+CurChn*14 + 7, *MusData.posi.Pos);
#endif
                
                MusData.CurVol[CurChn] = *MusData.posi.Pos++;
                MusData.NoteDelay[CurChn] = 1;
            }
    
            Cmd = '-';
            if(ByteRead & 128) /* Cmd(+infobyte) specified? */
            {
                GotCmd[CurChn] = Cmd = (*MusData.posi.Pos++) | 64;
#if DISPLAYROWS
	            /* C-4 10 63 A06|
    	           12345678901234
    	           01234567890
        	    */
            
	            printf("\r\33[%uC%c%02X", 12+CurChn*14 + 10, Cmd, Info);
#endif
#if HEXDUMP
				printf("(%c%02X)", Cmd, Info);
#endif
            }
    
            switch(Cmd)
            {
                case 'A':
                    MusData.FrameCount = Info;
                    break;
                case 'B':
                    Break = 1;
                    /* The order is in hexadecimal format */
                    BreakOrder = Info;
                    break;
                case 'C':
                    Cut = 1;
                    /* The line is in bcd format */
                    CutLine = (Info>>4)*10 + (Info&15);
                    break;
                case 'D':
                case 'K':
                    if((ByteRead & 64) && !MusData.CurVol[CurChn])
                    {
                        /* FIXME: Any clue why is this here?        */
                        /* Does it have something to do with midi?  */
                        MusData.CurVol[CurChn] = 1;
                    }
    
                    if(!Info)Info = MusData.DxxDefault[CurChn];
                    else MusData.DxxDefault[CurChn] = Info;
    
                    if(Info==0xFF)MusData.CurVol[CurChn] = 63;
                                                             /* DFF, maximize */
                    else if(Info > 0xF0)                     /* DFx           */
                    {
                        MusData.VolSlide[CurChn] = -(Info&0x0F);
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
                    if(Cmd=='K') { Info=0; goto CmdH; }
                    if(Cmd=='L') { Info=0; goto CmdG; }
                    break;
                
                case 'E':
                case 'F':
                    MusData.FinePerSlide[CurChn] = 0;
                    if(!Info)Info = MusData.ExxDefault[CurChn];
                    else MusData.ExxDefault[CurChn] = Info;
                    if(Info >= 0xF0)MusData.FinePerSlide[CurChn]=1, Info&=15;
                    else if(Info >= 0xE0)MusData.FinePerSlide[CurChn]=2, Info&=15;
                    MusData.PerSlide[CurChn] = (Cmd=='F') ? -Info : Info;
                    break;
                
                case 'G':
CmdG:               if(!Info)Info = MusData.GxxDefault[CurChn];
                    else MusData.GxxDefault[CurChn] = Info;
                    MusData.Portamento[CurChn] = Info;
                    break;
                
                case 'H':
                case 'U':
CmdH:               if(!Info)Info=MusData.HxxDefault[CurChn];
                    else MusData.HxxDefault[CurChn]=Info;
                    if(Info & 0xF0)MusData.VibSpd  [CurChn] = (Info & 0xF0) >> 4;
                    if(Info & 0x0F)MusData.VibDepth[CurChn] = Info & 0x0F;
                    MusData.FineVibra[CurChn] = (Cmd == 'U');
                    break;
                
                case 'I':
                    if(!Info)Info=MusData.IxxDefault[CurChn];
                    else MusData.IxxDefault[CurChn]=Info;
                    MusData.Tremor[CurChn] = Info;
                    break;
    
                case 'J':
                    if(!Info)Info=MusData.JxxDefault[CurChn];
                    else MusData.JxxDefault[CurChn]=Info;
                    MusData.Arpeggio[CurChn] = Info;
                    break;
    
                case 'M':
                    if(!Info)Info = MusData.MxxDefault[CurChn];
                    MusData.MxxDefault[CurChn] = Info;
                    
                    /* MBx is reverb.
                     * MCx is chorus.
                     * MAx is supported also, because I've accidentally
                     *        wrote some songs using it... //Bisqwit
                     */
                    
                    if((Info>>4)==0xB
                    || (Info>>4)==0xA)
                    {
                        /* Reverb is sticky. */
                        M_MidiCtrl(CurChn, 91, (Info&15)<<3);
                    }
                    else if((Info>>4)==0xC)
                    {
                        /* Chorus is sticky. */
                        M_MidiCtrl(CurChn, 93, (Info&15)<<3);
                    }
                    break;

                case 'N':
                    /* Bend is not sticky.
                     * It will automatically reset upon next note.
                     * FIXME: This should be done AFTER NoteDelay!
                     */
                    
                    /* Bend limits: 0x0000 - 0x3FFF
                     * 0x80 << 6 = 0x2000
                     * 0x00 << 6 = 0x0000
                     * 0xFF << 6 = 0x3FE0 or similar
                     */
                    M_GmBend(CurChn, (unsigned)Info << 6);
                    break;
                    
                case 'P':
                    if(Info)
                        MusData.ArpegSpd = Info;
                    break;

                case 'Q':
                    if(Info)
                        MusData.Retrig[CurChn] = Info;
                    break;

                case 'R':
                    if(!Info)Info=MusData.RxxDefault[CurChn];
                    else MusData.RxxDefault[CurChn]=Info;
                    if(Info & 0x0F)MusData.TremoDepth[CurChn] = Info & 0x0F;
                    if(Info & 0xF0)MusData.TremoSpd  [CurChn] = Info & 0xF0;
                    break;
                
                case 'S':
                {
                    BYTE a, b;
                 
                    if(!Info)Info = MusData.SxxDefault[CurChn]; /* Notice */
                    MusData.SxxDefault[CurChn] = Info;          /* Notice */
                    
                    a = Info >> 4;
                    b = Info & 15;
                    
                    switch(a)
                    {
                        case 0xB:
                            if(!b)
                            {
                                /* ToSave is what we had on the beginning of line */
                                MusData.saved = ToSave;
                            }
                            else
                            {
                                /* If there was no active loop going on */
                                if(MusData.LoopCount == 0)
                                {
                                    /* Then set it */
                                    MusData.LoopCount = b;
                                }
                                else
                                {
                                    /* Else decrease the ongoing counter */
                                    --MusData.LoopCount;
                                }
                                
                                /* If there is still something to loop */
                                if(MusData.LoopCount)
                                {
                                    /* Then we'll go back when we can */
                                    Restore = 1;
                                }
                                else
                                {
                                    /* Else we're done, and we must do SB0 at next line. */
                                    MusData.PendingSB0 = 1;
                                }
                            }
                            break;
                        case 0xC:
                            /* Mitäköhän ideaa olisi komennossa SC0... */
                            MusData.NoteCut[CurChn] = b + 1;
                            break;
                        case 0xD:
                            /* Mitäköhän ideaa olisi komennossa SD0... */
                            MusData.NoteDelay[CurChn] = b + 1;
                            break;
                        case 0xE:
                            /* Mitäköhän ideaa olisi komennossa SE0... */
                            MusData.PatternDelay = b;
                            break;
                    }
                    break;
                }
                
                case 'T':
                    MusData.Tempo = Info;
                    break;
                
                case 'X':
                    /* Pan is sticky. */
                    M_Pan(CurChn, Info);
                    break;
            }
#if HEXDUMP
			printf(";");
#endif
        }

#if DISPLAYROWS
		printf("\33[m\n");
#endif
#if HEXDUMP
		printf("\n");
#endif
        
        /* Rivin lopettavan tavun (jonka ylimmät 4 bittiä ovat 0)
         * neljä alinta bittiä kertovat, montako tyhjää riviä tämän
         * jälkeen seuraa.
         *    0: 0
         * 1-14: 2-15
         *   15: seuraava tavu kertoo määrän
         */
        
		MakeWait(ByteRead);
    }
    
    /* Puuttuvat komennot käsitellään tässä vaiheessa. */
    CheckLackingCommands(GotCmd);
    
    if(Restore)
    {
        /* If got SBx _now_ */
        MusData.posi = MusData.saved;
        return;
    }
    
    if(!MusData.PatternDelay)
        ++MusData.posi.Row;

    if(Break)
    {
        MusData.posi.Ord = BreakOrder;
        
        /* Bxx, hypätään määrättyyn orderiin.
         * Jos oli myös Cxx, hypätään määrätylle riville.
         */
        
        printf("B%02X %d? C%02u\n", BreakOrder, Cut, CutLine);
        MusSeekRow(Cut ? CutLine : 0);
    }
    else if(Cut)
    {
        /* Cxx, hypätään seuraavaan orderiin määrätylle riville. */
        /* ++ -orderit yms pitäisi loistaa poissaolollaan, joten tämä toimii */
        MusData.posi.Ord = (MusData.posi.Ord+1) % (MusData.Hdr->OrdNum);
        
        printf("C%02u\n", CutLine);
        MusSeekRow(CutLine);
    }
    else if(!MusData.posi.Wait
         && MusData.posi.Pos >= MusData.Patn[MusData.posi.Pat].Ptr
                              + MusData.Patn[MusData.posi.Pat].Len)
    {
        /* Jos patterni loppui kesken, mennään seuraavaan orderiin, riville 0. */
        
        MusData.posi.Ord = (MusData.posi.Ord+1) % (MusData.Hdr->OrdNum);
        
        printf("Out of data (length: %u), going to order %02X\n",
          MusData.Patn[MusData.posi.Pat].Len,
          MusData.posi.Ord);
        MusSeekRow(0);
    }
}

static void UpdateEffects(void)
{
    /* This table was picked up from Mikmod. */
    static const BYTE avibtab[128] =
    {   0,1,3,4,6,7,9,10,12,14,15,17,18,20,21,23,
        24,25,27,28,30,31,32,34,35,36,38,39,40,41,42,44,
        45,46,47,48,49,50,51,52,53,54,54,55,56,57,57,58,
        59,59,60,60,61,61,62,62,62,63,63,63,63,63,63,63,
        64,63,63,63,63,63,63,63,62,62,62,61,61,60,60,59,
        59,58,57,57,56,55,54,54,53,52,51,50,49,48,47,46,
        45,44,42,41,40,39,38,36,35,34,32,31,30,28,27,25,
        24,23,21,20,18,17,15,14,12,10,9,7,6,4,3,1
    };

    unsigned ch;
    
    /* This loop must be called as often as possible.
     * The bigger rate, the nicer results.
     * Rate-variable must be correct, and indicate
     * the amount of times in second this loop is
     * executed.
     */
    
    /* These operations have each their own timer. */
     
    for(ch=0; ch<MAXCHN; ch++)
    {
        /**** Jxy ****/
        
        if(MusData.Arpeggio[ch])
        {
            DWORD ArpegCounter = (unsigned long)(MusData.ArpegSpd & 127) * CLKMUL / Rate;
            
            for(MusData.ArpegClk[ch] += ArpegCounter;
                MusData.ArpegClk[ch] >= CLKMUL; /* was overflow? */
                MusData.ArpegClk[ch] -= CLKMUL)
            {
                int notemp, Note;
                BYTE Instru = MusData.CurInst[ch];
                
                switch((MusData.ArpegCnt[ch]++) % (3-(MusData.ArpegSpd>>7)))
                {
                    case 0:
                        Note = 0;
                        break;
                    case 1:
                        Note = MusData.Arpeggio[ch] & 15;
                        break;
                    default:
                        Note = MusData.Arpeggio[ch] >> 4;
                }

                Note += MusData.NNum[ch];
                
                notemp = Note;

                if(!(MusData.Instr[Instru]->Bank & 128))
                {
                    notemp -= MusData.Instr[Instru]->FT - 2;
                    /* Remove finetuning effect for AdLib */
                }
                
            //  M_NoteOff(ch);
                M_NoteOn(ch, Note, MusData.CurVol[ch], Instru,
                         MakePeriod(notemp%12, notemp/12,
                                    MusData.Instr[Instru]->C4Spd));
            }
        }
        
        /**** Hxy and Kxy and Uxy ****/
        
        if(MusData.ST3Period[ch] && MusData.VibDepth[ch])
        {
            /* Hxy: 
                     Vibrato amplitude: y/16       semitones
                     Vibrato speed:     x/64       cycles per frame
                     Frames is the value set by Axx -command.
                     
                     Full cycle is 256 vibrato ticks.
                     
                     [c] = cycle
                     [t] = tick
                     [f] = frame
                     [s] = second
                     [x] = vibspeed
                     [T] = tempo
                     
                     x/64 c = f            || 1 frame = x/64 cycles
                     2*T/5 f = s           || 1 second = 2*T/5 frames
                     c = 256 t             || cycle = 256 ticks
                     
                     x*256/64 t = f        || 1 frame = x*256/64 ticks
                     2*T/5 f = s
                     
                     x*256/64 t = s*5/2/T  || x*256/64 ticks = 5/(2*T) seconds
                     
                     2*T*x*256/64/5 t = s  || 1 second = 2*T*x*256/64/5 ticks

                     8*T*x/5 t = s         || 1 second = 8*T*x/5 ticks
                     
            */

            DWORD VibCounter = (unsigned long)MusData.Tempo * MusData.VibSpd[ch] * 8 * CLKMUL / 5 / Rate;
            
            for(MusData.VibClk[ch] += VibCounter;
                MusData.VibClk[ch] >= CLKMUL; /* was overflow? */
                MusData.VibClk[ch] -= CLKMUL)
            {
                int VibVal = avibtab[MusData.VibPos[ch] & 127];
                if(!(MusData.VibPos[ch] & 0x80))VibVal = -VibVal;
                
                VibVal *= MusData.VibDepth[ch]; /* Range: -63*15..63*15 */
                
                /* VibVal is now expected to be in -63*15..63*15 scale,
                 * and should be scaled to +-seminote scale.
                 *
                 * One seminote is 1.06 * current period.
                 * 
                 * 63*15 / 1.06 ~= 892
                 * 63*15*12 = 11340
                 * 63*12 = 756
                 *
                 */
                 
                if(MusData.FineVibra[ch])VibVal /= 8;
                 
                #if 0
                 
                /* This is correct by the definition!  */
                M_PeriodTouch(ch, MusData.ST3Period[ch] * pow(2, VibVal / 756.0));
                
                #else
                
                /* Too bad. None of the players follow the definition. */
                
                /* This is quite near what ST3 does.
                 * At least redalert starts exactly correctly.
                 */
                
                M_PeriodTouch(ch, MusData.ST3Period[ch] + VibVal / 8);
                
                #endif
                
                /* Our brand new midi vibrato! :)              */
                /* This is not _exact_, but it is near enough. */
                
                /* VibVal is -63*15 .. 63*15 (+-0x3B1) 
                 * Multiplying by 8 gives the full scale
                 */
                
                M_GmBend(ch, 0x2000 + VibVal*8);
                
                ++MusData.VibPos[ch];
            }
        }
        
        /**** Rxy ****/
        
        if(MusData.CurVol[ch] && MusData.TremoDepth[ch])
        {
            /* Rxy: 
                     Tremolo amplitude: y*(ticks-1) semitones
                     Tremolo speed:     x*ticks/64 cycles in second
                     Ticks is the value set by Axx -command.
            */

            DWORD TremoCounter = (unsigned long)MusData.Tempo * MusData.TremoSpd[ch] * 8 * CLKMUL / 5 / Rate;
            
            for(MusData.TremoClk[ch] += TremoCounter;
                MusData.TremoClk[ch] >= CLKMUL; /* was overflow? */
                MusData.TremoClk[ch] -= CLKMUL)
            {
                /* This code isn't from Mikmod. */
                int TremoVal = avibtab[MusData.TremoPos[ch] & 127];
                int Vol;
                
                if(MusData.TremoPos[ch] & 0x80)TremoVal = -TremoVal;
                
                TremoVal *= MusData.TremoDepth[ch]; /* Range: -63*15..63*15 */
                TremoVal *= MusData.FrameCount-1;   /* Range: Härregud */
                
                Vol = MusData.CurVol[ch] + TremoVal / 63;
                
                if(Vol < 0)Vol = 0;
                if(Vol > 63)Vol = 63;

                M_Touch(ch, MusData.CurInst[ch], Vol);
                
                ++MusData.TremoPos[ch];
            }
        }
    }
}

static void FrameTick(void)
{
    unsigned ch;
    
#if DISPLAYFRAMETICKS
    printf("\33[32mFrame Tick: Ord %02X Pat %02u Row %02u Wait %u\33[m\n",
        MusData.posi.Ord,
        MusData.posi.Pat,
        MusData.posi.Row,
        MusData.posi.Wait);
#endif

    for(ch=0; ch<MAXCHN; ch++)
    {
        if(MusData.NoteCut[ch] > 0)
        {
            if(!--MusData.NoteCut[ch])
                M_NoteOff(ch);
        }

        if(MusData.NoteDelay[ch] > 0)
        {
            /* Vibrato - don't use */
            /* M_MidiCtrl(CurChn, 1, 0); */
            
            if(!--MusData.NoteDelay[ch])
                switch(MusData.CurPuh[ch])
                {
                    case 255:   /* Tyhjä rivi */
                        M_Touch(ch, MusData.CurInst[ch],
                                    MusData.CurVol[ch]);
                        break;
                    case 254:   /* ^^ -rivi */
                        M_NoteOff(ch);
                        break;
                    default:    /* Rivi jolla nuotti */
                        M_NoteOff(ch);
                        
                        if(!MusData.Portamento[ch])
                            MusData.ST3Period[ch] = MusData.NextPeriod[ch];
                        
                        M_NoteOn(ch, MusData.NNum[ch],
                                     MusData.CurVol[ch],
                                     MusData.CurInst[ch],
                                     MusData.ST3Period[ch]);
                }
                /* FIXME:
                   When PatternDelay is used, NoteDelay
                   should retrigger the same note again
                   and again.
                    !MusData.PatternDelay
                */
        }
        
        /**** Qxy ****/
        
        /* Don't retrig on the first frame */            
        if(MusData.Retrig[ch] && MusData.Frame)
        {
            unsigned space = MusData.Retrig[ch] & 15;
            unsigned fade  = MusData.Retrig[ch] >> 4;
            
            if(!(MusData.Frame % space))
            {
            	if(MusData.CurVol[ch] > fade)
	                MusData.CurVol[ch] -= fade;
	            else
	                MusData.CurVol[ch] = 0;
                
                M_NoteOff(ch);
                M_NoteOn(ch, MusData.NNum[ch],
                             MusData.CurVol[ch],
                             MusData.CurInst[ch],
                             MusData.ST3Period[ch]);
            }
        }
        
        /**** Dxy and Kxy and Lxy ****/
        
        if(MusData.VolSlide[ch])
        {
            int Vol = MusData.CurVol[ch];
            
            if(!MusData.Frame || !MusData.FineVolSlide[ch])
                Vol += MusData.VolSlide[ch];
                
            if(Vol < 0)Vol = 0;
            if(Vol > 63)Vol = 63;

            MusData.CurVol[ch] = Vol;
            
            M_Touch(ch,
              MusData.CurInst[ch],
              MusData.CurVol[ch]);

            if(MusData.FineVolSlide[ch] && !MusData.PatternDelay)
                MusData.VolSlide[ch] = 0;
        }
        
        /**** Ixy ****/
        
        if(MusData.Tremor[ch])
        {
            BYTE on = MusData.Tremor[ch]>>4; /* These are 0-based! 0 on means */
            BYTE off= MusData.Tremor[ch]&15; /* that note is on for one tick  */
            
            if(MusData.TremoClk[ch] > on)
            {
                /* Been on enough, now we switch it off */
                M_Touch(ch,
                  MusData.CurInst[ch],
                  0);
            }
            
            if(++MusData.TremoClk[ch] > on+off+1)
            {
                /* Was off enough, we switch it back on */
                MusData.TremoClk[ch] = 0;
                M_Touch(ch,
                  MusData.CurInst[ch],
                  MusData.CurVol[ch]);
            }
        }        
        
        /**** Exy and Fxy ****/

        if(MusData.PerSlide[ch])
        {
            if(!MusData.Frame || !MusData.FinePerSlide[ch])
            {
                int tmp = MusData.PerSlide[ch]
                        * ((MusData.FinePerSlide[ch] == 2) ? 1 : 4);
                MusData.ST3Period[ch] += tmp;
                  
                M_PeriodTouch(ch, MusData.ST3Period[ch]);
            }
            
            /* M_GmBend(ch, 0x2000); FIXME - not implemented. */

            if(MusData.FinePerSlide[ch] && !MusData.PatternDelay)
                MusData.PerSlide[ch] = 0;
        }
        
        /**** Gxx and Lxy ****/

        if(MusData.Portamento[ch])
        {
            int change = MusData.Portamento[ch] * 4, done=0;
            if(MusData.ST3Period[ch] < MusData.NextPeriod[ch])
            {
                MusData.ST3Period[ch] += change;
                if(MusData.ST3Period[ch] >= MusData.NextPeriod[ch])done=1;
            }
            else
            {
                MusData.ST3Period[ch] -= change;
                if(MusData.ST3Period[ch] <= MusData.NextPeriod[ch])done=1;
            }
            if(done)
            {
                MusData.ST3Period[ch] = MusData.NextPeriod[ch];
                MusData.Portamento[ch] = 0;
            }
            
            M_PeriodTouch(ch, MusData.ST3Period[ch]);
            
            /* M_GmBend(ch, 0x2000); FIXME - not implemented. */
        }
    }
}

/* Handles one timer tick */
/* Called from m_irq.c    */
void TimerHandler(void)
{
    if(MusData.Paused || !MusData.driver)return;
    
    for(MusData.FrameClk += MusData.FrameSpd;
        MusData.FrameClk >= CLKMUL;
        MusData.FrameClk -= CLKMUL)
    {
    	/* tick! */
    	
#if 0
        printf("Clock = %08X, Speed = %08X (T%02X A%02X)\n",
          MusData.FrameClk,
          MusData.FrameSpd,
          MusData.Tempo,
          MusData.FrameCount);
#endif

        if(++MusData.Frame >= MusData.FrameCount)
        {
            ++MusData.NextRow;            
            PlayLine();
            
            MusData.Frame  = 0;
            
            /* By definition, rows played in minute is 24 * tempo / ticks.
            
               Therefore frames per minute is 24 * tempo
               
               Frames per second = 24 * tempo / 60
                                 = 2 * tempo / 5
            */
            
            MusData.FrameSpd = (unsigned long)MusData.Tempo * 2 * CLKMUL / 5 / Rate;
            
#if 0
            printf("Rows per second is %.2f\n",
                2 * MusData.Tempo / 5.0 / MusData.FrameCount);
#endif
        }
        
        /* Frame has ticked, let's do something */
        
        ++MusData.NextFrame;        
        FrameTick();
    }
        
    /* Next we do all the frame independent stuff.          */
    /* Currently includes arpeggio (Jxy) and vibrato (Hxy). */
    
    ++MusData.NextTick;
        
    UpdateEffects();
}

void StartS3M(const BYTE *s)
{
    BYTE a;
    struct MusDriver *tmp = MusData.driver;

    StopS3M();
    
    memset((void *)&MusData, 0, sizeof(MusData));
    MusData.driver = tmp;
    
    {
    	int oct, note;
    	
    	for(note=0; note<12; ++note)
    		printf("\t%d", note);
    	printf("\n");
    	for(oct=0; oct<8; ++oct)
    	{
    		printf("%d\t", oct);
    		for(note=0; note<12; ++note)
    			printf("%d\t", MakePeriod(note, oct, 8363));
    		printf("\n");
    	}
    }
    
    /* Pointers are nice :) */
    MusData.Hdr = (InternalHdr *)__FILE__" v"VERSION"(C) 1992,2000 Bisqwit (bisqwit@iki.fi)";
    /* Please don't modify the text above. It would be nice to   *
     * see my name inside games made by people unknown to me :-) */

    MusData.Playing = s;
    if(s[1]>2)for(;*s++;); /* Skip possible song name */

    MusData.Hdr = (const InternalHdr *)s;
    s += sizeof(InternalHdr);
    
    MusData.HaveChanInfo  = (MusData.Hdr->PatNum  & 512) == 512;
    MusData.LinearMidiVol = (MusData.Hdr->InSpeed & 128) == 128;
    MusData.InstruOptim   = (MusData.Hdr->PatNum & 1024) == 1024;
    
    ((InternalHdr *)MusData.Hdr)->PatNum &= 511;
    ((InternalHdr *)MusData.Hdr)->InSpeed &= 127;

    MusData.Tempo      = MusData.Hdr->InTempo;
    MusData.FrameCount = MusData.Hdr->InSpeed & 127;
    MusData.ArpegSpd     = 50;
    MusData.GlobalVolume = 255;
    
    if(MusData.HaveChanInfo)
        s++; /* ChanCount present */

    MusData.Orders = (const BYTE *)s;
    s += MusData.Hdr->OrdNum;
    
    for(a=0; a<MusData.Hdr->InsNum; a++)
    {
        InternalSample *tmp = (InternalSample *)s;
        
        if(MusData.HaveChanInfo)
            s += 1 + strlen(tmp->insname);
       
        s += sizeof(*tmp);
        
        MusData.Instr[a+1] = tmp;
    }

    /* This disables some bugs. */
    MusData.Instr[0] = MusData.Instr[1];

    for(a = 0; a < MusData.Hdr->PatNum; a++)
    {
        MusData.Patn[a].Len = ((BYTE)s[0]) | (((BYTE)s[1]) << 8);
        MusData.Patn[a].Ptr = (s += 2);
        s += MusData.Patn[a].Len;
    }
    
    SetPause(1);

    MusSeekOrd(0);
    
    SetPause(0);
}

/* From m_arch.c */
extern int FromArchive;
extern char *Archived;

void StopS3M(void)
{
    SetPause(1);
    ReleaseINT();
    
    if(Archived && !FromArchive)
    {
        free(Archived);
        Archived = NULL;
    }
}

void SetPause(int state)
{
    if(state == MusData.Paused)return;
    MusData.Paused = state;
    
    if(MusData.driver)
        MusData.driver->reset();

    if(state) /* pause */
    {
        /* actually nothing to do */
    }
    else      /* unpause */
    {
        InstallINT();
    }
}

void PauseS3M(void)
{
    if(MusData.Paused)
        SetPause(0);
    else
        SetPause(1);
}

void MusSeekOrd(unsigned Num)
{    
    int p = MusData.Paused;
    
    SetPause(1);
    
    /* The player must not modify these at same time */
    MusData.PatternDelay = 0;
    MusData.FrameSpd   = CLKMUL-1; /* Hurry first. */
    MusData.Frame      = MusData.FrameCount-1;
    MusData.posi.Ord   = Num;
    
    MusSeekRow(0);
    
    SetPause(p);
}

void MusSeekNextOrd(void)
{
    unsigned limit = MusData.Hdr->OrdNum - 1;
    if(MusData.posi.Ord < limit)
        MusSeekOrd(MusData.posi.Ord + 1);
}

void MusSeekPrevOrd(void)
{
    if(MusData.posi.Ord > 0)MusSeekOrd(MusData.posi.Ord - 1);
}
