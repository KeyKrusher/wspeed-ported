/****************************************************************************
 *
 * Intgen - Originally the S3M to MidiS3M subsystem internalformat converter
 *          Contains also routines for handling the NES-S3M files
 *
 *          Copyright (C) 2000 Bisqwit (http://iki.fi/bisqwit/)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  This source code is still a real horrible spaget mess,
 *  although it has been lately a bit functionalized.
 *  You may lose your mental health while browsing it.
 *  You have been warned.
 *
 *
 *****************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __GNUC__
#include <unistd.h>
#include <fcntl.h>
#endif

#include "music.h"
#include "m_midi.h"
#include "adldata.h"
#include "argh.h"

#ifdef __MSDOS__
static const char NULFILE[] = "NUL";
#else
static const char NULFILE[] = "/dev/null";
#endif

static int Verbose=0;

/* Going to convert the volumes for adlib? */
static int FMVolConv = 0;

/* Making linear volumeformat? */
static int MakeLinear = 0;

/* Disable name output? */
static int noname = 0;

/* Include instrument names in internalformat file? */
static int inames = 0;

/* Include also unused instruments? */
static int allins = 0;

/* Include also unused patterns? */
static int allpat = 0;

/* Job is to NESify? */
static int Nesify = 0;

/* NESify only data, not program? */
static int NesifyDataOnly=0;

/* No far, use near */
static int NoFar=0;

/* Verify only, no conversion */
static int Verify=0;

/* 0=No interlacing (for Nesify==2, this is assumed as 0) */
static int NESInterlace=1;
/* 256=No compressing */
static int NESCompress=127;
/* 10 bits for depth, 6 for length */
static int NESDepf=10;
/* Or instead of 6, this */
static int NESLength=6;
/* 0=No xorring */
static int NESXOR=255;
/* For bit engine, char count */
static int NESCount=64;
/* For bit engine, char add */
static int NESRoll=154;
/* 0=-sc, 1=-sC */
static int NESFullFeatured;
/* 1=Assume readonly nesmusa.$$$ is ok already */
static int InternalFB=0;
/* Some description to put between comment marks in C file. */
static char MFName[64];

static int MayInstruOpt = 1;

static const BYTE NESvol[65] =
{    0,29,33,35,37,39,40,42,
    43,44,45,45,46,47,48,48,
    49,49,50,51,51,52,52,52,
    53,53,54,54,54,55,55,56,
    56,56,57,57,57,58,58,58,
    58,59,59,59,60,60,60,60,
    61,61,61,61,61,62,62,62,
    62,63,63,63,63,63,64,64,
    64
};

/* Channel conversion table */
static BYTE ChanConv[32];

/* Instrument conversion table */
static BYTE InstConv[MAXINST+1];
static BYTE InstCon2[MAXINST+1];

/* Visited rows */
static BYTE There[MAXPATN][MAXROW];

/* Possible join points */
static BYTE Join[MAXPATN][MAXROW];

/* Real order to intgen order conversion table */
static BYTE OrdCvt[MAXORD];

/* Orders */
static BYTE Ords[MAXORD];

/* Instrument pointers in S3M file */
static WORD InstPtr[MAXINST+1];

/* Pattern pointers in S3M file */
static WORD PatnPtr[MAXPATN];

/* Used instruments */
static char InstUsed[MAXINST+1]; /* booleans */

static S3MHdr Hdr;
static ADLSample *Instr[MAXINST+1];

static long StartPtr;

/******** Start NES analyzer engine ********/
static unsigned Axx;
static BYTE NESNote[4] = {1,1,1,1};
static BYTE NESVol[4]  = {0,0,0,0};
static long NESPos=0, NESLoop=0;
static long *NESAt=NULL; /* gcc likes initialized variables */
#define NESOut(n) {register int a=(n)*Axx;for(;a>0;a--){ \
    fwrite(&NESNote,1,4,fo); fwrite(&NESVol, 1,4,fo); }}
#define NESRemember() (NESPos=ftell(fo))
/* Hope the fread() in NESRepeat works... */
#define NESRepeat(count) \
    {int a; size_t c = (size_t)(ftell(fo)-NESPos); \
     char *s = (char *)malloc(c); \
     if(!s)MemoryTrouble("NESRepeat"); \
     fseek(fo, NESPos, SEEK_SET); \
     fread(s, 1,c, fo); \
     fseek(fo, NESPos+c, SEEK_SET); \
     for(a=count;a;a--)fwrite(s, 1,c, fo); \
     free(s); }
#define NESHere(ord,row) (NESAt[(ord)*64+(row)]=ftell(fo)/8)
#define NESLoopStart(ord,row) (NESLoop=NESAt[(ord)*64+(row)])
/******** End NES analyzer engine ********/

static void MemoryTrouble(const char *Where)
{
    fprintf(stderr, "Memory allocation error at %s, intgen halted\n", Where);
    exit(errno);
}

static void AllocateInstrus(void)
{
    unsigned a;
    for(a=0; a<=MAXINST; a++)
    {
        Instr[a] = (ADLSample *)malloc(sizeof(ADLSample));
        if(!Instr[a])MemoryTrouble("AllocateInstrus");
    }
}

static void ReadInstruments(FILE *const f)
{
    unsigned a, SMPcount, AMEcount;
    
    /****** SCAN THE INSTRUMENTS ******/
    if(!allins)
        for(;;)
        {
            fseek(f, StartPtr+InstPtr[Hdr.InsNum-1]*16, SEEK_SET);
            fread(Instr[0], sizeof(ADLSample), 1, f);
            if(Instr[0]->Typi != 0)break;
            if(!--Hdr.InsNum)break;
        }
     
    SMPcount = 0;
    AMEcount = 0;
    
    /****** READ THE INSTRUMENTS ******/
    for(a=0; a<Hdr.InsNum; a++)
    {
        fseek(f, StartPtr+InstPtr[a]*16, SEEK_SET);
        fread(Instr[a+1], sizeof(ADLSample), 1, f);
        /* FIXME: Verify? samples... */

        /* Calculate SMP count (A) and AME count (B) */
        if(Instr[a+1]->Typi==1)++SMPcount;
        if(Instr[a+1]->Typi==2)++AMEcount;
    }

    Hdr.InSpeed &= 127;
    if(SMPcount > AMEcount)
    {
        if(!MakeLinear)
        {
            if(Verbose)printf("Warning: Converting all linear volumes to adlib volumes.\n");
            FMVolConv = 1;
        }
    }
    else if(MakeLinear)
    {
        if(Verbose)printf("Warning: Converting all adlib volumes to linear volumes.\n");
        FMVolConv = 1;
    }

    if(MakeLinear)Hdr.InSpeed |= 128; /* Force linearity */
}

static void ConvertInstruments(FILE *const f)
{
    unsigned a;
    unsigned A=0; /* Count of samples with GM/GP        */
    unsigned B=0; /* Count of instruments without GM/GP */
    
    /****** IF NECESSARY, CONVERT THE INSTRUMENTS ******/
    for(a=0; a<Hdr.InsNum; a++)
    {
        BYTE GM = 0;
        int scale=63, Addi=0, Bank=0;
        int ft=0; /* Finetune */

        fseek(f, StartPtr+InstPtr[a]*16, SEEK_SET);
        fread(Instr[a+1], sizeof(ADLSample), 1, f);

        Instr[a+1]->ManualScale = 100;

        if(Instr[a+1]->Name[0]=='G')    /* GM=General MIDI */
        {
            int c = Instr[a+1]->Name[1];
            if(c=='M'    /* GM; M stands also for "melodic" */
             ||c=='P')   /* GP; P stands for "percussion"   */
            {
                char *s = Instr[a+1]->Name+2;
                for(; isdigit(*s); GM = GM*10 + *s++ - '0');

                for(;;)
                {
                    int sign=0;
                    if(*s=='-')sign=1;
                    if(sign || *s=='+')
                    {
                        for(ft=0; isdigit(*++s); ft = ft*10 + *s - '0');
                        if(sign)ft=-ft;
                        if(ft>127 || ft<-128)
                        {
                            printf(
                                "Warning: Instrument %d:\n"
                                "\t Finetune value out of ranges; using %u instead of %d.\n",
                                a, (BYTE)ft, ft);
                        }
                        continue;
                    }
                    if(*s == '/')
                    {
                        for(scale=0; isdigit(*++s); scale=scale*10 + *s-'0');
                        if(scale > 63)
                        {
                            Instr[a+1]->ManualScale = scale*100/63;
                            if(Verbose > 1)
                                printf(
                                    "Warning: Instrument %d: Scaling level too big; using 63 instead of %d\n"
                                    "\t and performing manual scaling of %d%% during the conversion.\n",
                                    a, scale,
                                    Instr[a+1]->ManualScale);
                            scale=63;
                        }
                        continue;
                    }
                    if(*s == '&')
                    {
                        for(Addi=0; isdigit(*++s); Addi=Addi*10 + *s-'0');
                        if(Addi > 15)
                        {
                            printf(
                                "Warning: Instrument %d:\n"
                                "\t Auto-SDx value out of ranges; using %d instead of %d.\n",
                                a, Addi&15, Addi);
                            Addi &= 15;
                        }
                        continue;
                    }
                    if(*s == '%')
                    {
                        for(Bank=0; isdigit(*++s); Bank=Bank*10 + *s-'0');
                        if(Bank > 127)
                        {
                            printf(
                                "Warning: Instrument %d:\n"
                                "\t Bank can not be >127; using %d instead of %d.\n",
                                a, Bank&127, Bank);
                            Bank &= 127;
                        }
                        continue;
                    }
                    if(*s == '*')
                    {
                        s++; /* Doublechannel mode - NOT SUPPORTED */
                        printf("Warning: Instrument %d is using obsolete doublechannel mode!\n", a);
                        continue;
                    }
                    break;
                }
                if(c=='P')GM+=128;
            }
            else
            {
                ++B;
            }
        }
        else if(Instr[a+1]->Filler2[2]) /* Löytyy tästäkin nuo tarvittaessa. */
        {                               /* Lähinnä NES-S3M:issä siis.        */
            GM = Instr[a+1]->Filler2[2];
            ft = (SBYTE)Instr[a+1]->Filler2[3];
        }
        else
            ++B;

        if(Instr[a+1]->Typi==1 || (!Instr[a+1]->Typi && allins))
        {
            int i = GM - 1;

            if(i >= 128)i -= 35;

            ++A;
            Instr[a+1]->Typi = 2;

            Instr[a+1]->D[0] = adl[i*14 + 3];
            Instr[a+1]->D[1] = adl[i*14 + 9];
            Instr[a+1]->D[2] = adl[i*14 + 4];
            Instr[a+1]->D[3] = adl[i*14 +10];
            Instr[a+1]->D[4] = adl[i*14 + 5];
            Instr[a+1]->D[5] = adl[i*14 +11];
            Instr[a+1]->D[6] = adl[i*14 + 6];
            Instr[a+1]->D[7] = adl[i*14 +12];
            Instr[a+1]->D[8] = adl[i*14 + 7] & 3;
            Instr[a+1]->D[9] = adl[i*14 +13] & 3;
            Instr[a+1]->D[10]= adl[i*14 + 8];

            Instr[a+1]->C4Spd=8363;

            /* Converted. */
            Bank |= 128;
        }

        /* Parempaakaan paikkaa tolle... */
        Instr[a+1]->DosName[0]=GM;
        /* -"-                           */
        Instr[a+1]->DosName[1]=ft&255;
        Instr[a+1]->DosName[2]=Bank;    /* b7 is adlib finetuning checker  */
        Instr[a+1]->D[8] |= (scale<<2); /* b1 and b0 select the waveform;  */
                                        /* b7 to b2 have the scaling level.*/
        Instr[a+1]->D[9] |= (Addi<<2);  /* b7 to b6 are free.              */
    }

    if(Verbose && B)
    {
        printf("Warning: %d instruments with no GM/GP header.\n",B);
        if(B*100/Hdr.InsNum > 80 && !Verify) //Yli 80%
            printf("That's quite much.\n");
    }
    if(A && !Verify && (Verbose + !Nesify) > 1)
        printf("Warning: %d samples converted to adlib instruments.\n", A);
}

/* gcc bugs, can't make this const */
static void OrderScan(BYTE (*const Visited)[MAXROW])
{
    unsigned CurOrd=0;
    unsigned OrderCount=0;
    
    /* Rescan orders to determine which ones were not visited */
    
    for(CurOrd=0; CurOrd<Hdr.OrdNum; ++CurOrd)
    {
        unsigned i;
        
        /* Map the order */
        OrdCvt[CurOrd] = OrderCount;
        
        for(i=0; i<MAXROW; i++)
            if(Visited[CurOrd][i])
                break;
        
        /* If order is never used */
        if(i==MAXROW || Ords[CurOrd]==254)
        {
            for(i=CurOrd; i<Hdr.OrdNum; i++)Ords[i] = Ords[i+1];
            Hdr.OrdNum--; /* One byte less left */
        }
        else
            ++OrderCount;
    }
    if(OrderCount != Hdr.OrdNum)
    {
        printf("intgen: Wonders, OrderCount did not match!\n");
    }
}

static void ScanPatterns(FILE *const f, FILE *const fo)
{
    int a, i;
    int Investigating = 0;
    int Surprise = 0;
    
    unsigned CurOrd, CurPat, CurRow;
    
    int SBNext = 0; /* gcc likes initialized variables */
    
    /* Pattern analyzer variables */
    BYTE *TmpPat, *pp;

    /* Visited rows per order */
    BYTE Visited[MAXORD][MAXROW];

    /****** DETERMINE WHICH PATTERNS AND ROWS BE NEVER VISITED ******/
    memset(There,   0, sizeof There);
    memset(Visited, 0, sizeof Visited);
    memset(Join,    0, sizeof Join);

    for(a=0; a<Hdr.OrdNum; a++)
    {
        if(Ords[a] == 255) //end
        {
            Hdr.OrdNum = a;
            break;
        }
        
        if(Ords[a]<254 && Ords[a] >= Hdr.PatNum)
        {
            printf(
                "intgen: Fatal: Your song file refers to missing patterns.\n"
                "Empty patterns are not allowed as a part of song.\n"
                "The dummy pattern number is %d.\n", Ords[a]);
            exit(1);
        }
    }

    CurOrd = 0;
    
    Axx = Hdr.InSpeed;

    if(Nesify)NESAt = (long *)calloc(Hdr.OrdNum, 64*sizeof(long));
    
    for(;;)
    {
        for(a=0; CurOrd < Hdr.OrdNum; TmpPat ? free(TmpPat) : 0)
        {
            CurRow = 0;
            CurPat = Ords[CurOrd];

            if(CurPat==0xFE)
            {
                ++CurOrd;
                continue;
            }

            /* Fetch the pattern */
            fseek(f, StartPtr+((long)(PatnPtr[CurPat]) << 4), SEEK_SET);
            i = fgetc(f);
            i = (fgetc(f) << 8) | i;

            TmpPat = (BYTE *)malloc(i-=2);
            if(!TmpPat)MemoryTrouble("allocating TmpPat");
            pp = TmpPat;
            fread(pp, i, 1, f);

            if(a>0 && Visited[CurOrd][a-1] && !Visited[CurOrd][a] && Verbose)
            {
                printf(
                    "Surprising jump to pattern %02d, row %02d...\n"
                    "This kind of things are usually Bisqwit's handwriting!\n",
                    CurPat, a);
                /* Surprising jumps are for example as follows:
                    31 C-4 01 .. B05 | - goes to somewhere
                    32 D-4 01 .. .00 | - arrives from somewhere - surprise
                */
            }

            for(SBNext=1; ; )
            {
                int Bxx, Cxx, SBx,SEx, SBStart;

                if(!a)
                {
                    if(Visited[CurOrd][CurRow])
                    {
                        if(Nesify)NESLoopStart(CurOrd, CurRow);
                        CurOrd = Hdr.OrdNum;
                        break;
                    }
                    if(Nesify)NESHere(CurOrd, CurRow);

                    if(Verbose>1)
                    {
                        printf("Ord %02X Pat %02d Row %02d\r", CurOrd, CurPat, CurRow);
                        fflush(stdout);
                    }
                    
                    if(Investigating)
                    {
                        for(a=0; a<MAXROW; a++)if(Visited[CurOrd][a])break;
                        if(a==MAXROW)
                        {
                            if(Verbose > 2)
                                printf("Surprise at order %02X (%02X%-40c\n", CurOrd, Ords[CurOrd], ')');
                            ++Surprise;
                        }
                        a=0;
                    }
                    if(!CurRow || There[CurPat][CurRow])
                        Join[CurPat][CurRow] = 1;
                    
                    Visited[CurOrd][CurRow] = 1;
                    There[CurPat][CurRow] = 1;
                }

                Bxx=Cxx=-1;
                SBStart=SBNext;
                SBNext=0;
                SEx=SBx=0;

                for(;;)
                {
                    int Command, InfoByte;
                    BYTE b = *pp++;

                    if(!b)break;

                    Command=InfoByte=0;

                    if(b<32)continue; /* Turha tavu */

                    if(b&32)
                    {
                        register int d, c=*pp++;
                        if(c>=126 && c<254)MayInstruOpt = 0;
                        d = *pp++;
                        if(!a)
                        {
                            if(c<254)NESNote[b&3]=(c&15) + 12*(c>>4);
                            else if(c==254)NESVol[b&3]=0;
                            if(d)
                            {
                                InstUsed[d]=1;
                                if(c!=254)
                                {
                                    NESVol[b&3]=Instr[d]->Volume%65;
                                    if(Nesify==1)NESVol[b&3]=NESvol[NESVol[b&3]];
                                }
                        }    }
                    }
                    if(b&64)
                    {
                        register int v = *pp++;
                        if(!a)
                        {
                            NESVol[b&3]=v%65;
                            if(Nesify==1)NESVol[b&3]=NESvol[NESVol[b&3]];
                        }
                    }
                    if(b&128)
                    {
                        Command = *pp++;
                        InfoByte= *pp++;
                    }
                    if(Command=='A'-64)if(!a)Axx=InfoByte;
                    if(Command=='B'-64)Bxx=InfoByte;
                    if(Command=='C'-64)Cxx=InfoByte;
                    if(Command=='D'-64)SBNext=1;
                    if(Command=='S'-64)
                    {
                        if((InfoByte>>4) == 0xB)
                        {
                            SBx = InfoByte&15;
                            if(!SBx)
                                SBStart=1;
                            else
                                SBNext=1;
                        }
                        if((InfoByte>>4) == 0xE)SEx = InfoByte&15;
                    }
                }

                /* a on tässä skippicountteri Bxx+Cxx -hommia varten. */
                if(a)
                    --a;
                else
                {
                    if(SBStart)
                    {
                        Join[CurPat][CurRow]=1;
                        NESRemember();
                    }
                    if(Nesify)NESOut(SEx+1);
                    if(SBx && Nesify)NESRepeat(SBx);
                    if(Bxx>=0)
                    {
                        CurOrd = Bxx;
                        if(Cxx>=0)a=(Cxx&15) + 10*(Cxx>>4);
                        break;
                    }
                    if(Cxx>=0)
                    {
                        CurOrd++;
                        a = (Cxx&15) + 10*(Cxx>>4);
                        break;
                    }
                }
                // FIXME: Verify? Pattern row count
                if(++CurRow >= 64)
                {
                    ++CurOrd;
                    break;
                }
            }
        }
        
        if(!Hdr.OrdNum && !allpat)
        {
            printf("intgen: Fatal: File does not contain any orders.\n");
            exit(1);
        }
        
        for(a=1; a<Hdr.OrdNum; a++)
            if(Ords[a-1]==254 && Ords[a] < 254)
            {
                for(i=0; i<MAXROW; i++)if(Visited[a][i])break;
                if(i==MAXROW && !Nesify)    /* Never visited, valid order. */
                {
                    /* Investigating a possible music archive. */
                    CurOrd = a;
                    ++Investigating;
                    break;
                }
            }
        if(a == Hdr.OrdNum)break;
    }

    if(Verbose && Investigating)
    {
        printf("Needed to re-investigate the file ");
        printf(Investigating>1?"%d times.\n":"once.\n", Investigating);
        if(Surprise)
        {
            if(!Verify && Verbose>1)
            {
                printf("Suppose the extra investigations weren't done, ");
                printf(Surprise>1?"%d orders":"one order", Surprise);
                printf(" would've been cleaned.\n");
            }
        }
        else
        {
            /* Can this happen? */
            printf("For a surprise, nothing extra was found anyway.\n");
        }
        fflush(stdout);
    }
    
    OrderScan(Visited);

    if(Nesify)free(NESAt);
}

static void SortInstruments(void)
{
    unsigned a;
    unsigned InsCount;
    
    /* Not used instruments */
    char NotUsed[MAXINST+1]; /* booleans */

    /****** SORT THE INSTRUMENTS, REMOVE UNUSABLE ONES ******/
    memset(InstConv, 255, sizeof InstConv);
    memset(NotUsed, 0, sizeof NotUsed);
    for(InsCount=a=0; a<Hdr.InsNum; a++)
        if(Instr[a+1]->Typi==2)
        {
            if(InstUsed[a+1] || allins)
            {
                InstCon2[InsCount] = a+1;
                InstConv[a+1] = InsCount++;
            }
            else
                NotUsed[a+1]=1;
        }

    /****** WRITE THE HEADER AFTER WE HAVE THE USABLE INSTRUMENT COUNT ******/
    Hdr.InsNum = InsCount;

    if(Verbose && !Nesify)
    {
        unsigned i = 0;
        for(a=1; a <= Hdr.InsNum; a++)
            if(NotUsed[a])
                i++;

        if(i==1)
        {
            printf("Instrument ");
            for(a=1; a <= Hdr.InsNum; a++)
                if(NotUsed[a])
                    printf("%d was never used, though it was usable.\n", a);
        }
        else if(i)
        {
            char Scales[MAXINST][8];
            unsigned ScaleCount;

            if(i > 32)
                printf("Over 32 instruments");
            else
            {
                printf("Instruments ");

                for(ScaleCount=0, a=1; a<=Hdr.InsNum; a++)
                    if(NotUsed[a])
                    {
                        for(i=1; (a+i+1<=Hdr.InsNum)&&NotUsed[a+i+1]; i++);
                        if(i)
                        {
                            sprintf(Scales[ScaleCount++], "%d-%d", a, a+i);
                            a += i;
                        }
                        else
                            sprintf(Scales[ScaleCount++], "%d", a);
                    }

                for(i=ScaleCount, a=0; a<ScaleCount; a++)
                    printf("%s%s", Scales[a], --i==1?" and ":i?", ":"");
            }
            printf(" were never used, though they were usable.\n");
        }
    }
}

static void WriteHeader(FILE *const fo)
{
    unsigned a;
    unsigned ChanCount;

    /* Not used instruments */
    char NotUsed[MAXINST+1]; /* booleans */

    /****** CONVERT THE CHANNEL LIST ******/
    memset(NotUsed, 1, sizeof NotUsed);
    
    if(Verify)printf("Channels: ");
    for(a=0; a<32; a++)
    {
        Hdr.Channels[a] &= 127;
        if(Verify)
        {
            if(Hdr.Channels[a]<8)printf("L%c ", Hdr.Channels[a]+49);
            else if(Hdr.Channels[a]<16)printf("R%c ", Hdr.Channels[a]+41);
            else if(Hdr.Channels[a]<32)printf("A%c ", Hdr.Channels[a]+33);
            else putchar('.');
        }
        if(Hdr.Channels[a]>=16 && Hdr.Channels[a]<32)
            NotUsed[Hdr.Channels[a]&15]=0;
    }
    if(Verify)printf("\n");

    for(a=0; a<32; a++)
        if(Hdr.Channels[a] < 16)
        {
            unsigned i;
            for(i=0; i<16; i++)
                if(NotUsed[i])
                {
                    Hdr.Channels[a] = i|16;
                    NotUsed[i] = 0;
                    break;
                }
        }
    
    memset(ChanConv, 255, sizeof ChanConv);
    for(ChanCount=a=0; a<32; a++)
        if(Hdr.Channels[a] >= 16 && Hdr.Channels[a] < 32)
            ChanConv[a] = ChanCount++;

    if(ChanCount > MAXCHN)
    {
        printf("intgen: ERROR: Channel count exceeds 15. 15 supported.\n");
        ChanCount = MAXCHN;
    }
    
    if(!Verify && !Nesify)
    {
        WORD patnum = Hdr.PatNum;
        
        /****** WRITE THE HEADER ******/
        if(Hdr.Name[0] && Hdr.Name[1] > 2 && !noname)
            fwrite(&Hdr.Name, 1, strlen(Hdr.Name)+1, fo);
        else if(((BYTE *)&Hdr.OrdNum)[1] > 2)
        {
            printf("intgen: ERROR: intgen has been cracked (%04X)\n", Hdr.OrdNum);
            exit(EXIT_FAILURE);
        }
        
        if(inames)
            patnum |= 512;
            
        if(MayInstruOpt)
            patnum |= 1024;
            
        fwrite(&Hdr.OrdNum, 2, 1, fo);
        fwrite(&Hdr.InsNum, 2, 1, fo);
        fwrite(&patnum,     2, 1, fo);
        fwrite(&Hdr.InsNum, 2, 1, fo);
        fwrite(&Hdr.InSpeed, 1, 1, fo);
        fwrite(&Hdr.InTempo, 1, 1, fo);
        if(inames)fputc(ChanCount, fo);
        fwrite(&Ords, 1, Hdr.OrdNum, fo);
    }
}

static void WriteInstruments(FILE *const fo)
{
    unsigned A;
    
    /****** WRITE THE INSTRUMENTS ******/
    for(A=0; A<Hdr.InsNum; ++A)
    {
        BYTE a = InstCon2[A];
        BYTE Tag128 = Instr[a]->DosName[2] & 128; /* Was converted SMP->AME? */

        Instr[a]->Volume %= 65;

        if(FMVolConv)
        {
            if(Tag128 && !MakeLinear)
                Instr[a]->C4Spd = 8363; //Standardize C2SPD
            Instr[a]->Volume = (
                MakeLinear
                ?   GMVol[(int)Instr[a]->Volume*63/64]
                :   FMVol[(int)Instr[a]->Volume] ) >> 1;
        }

        // Convert volumes in range 0..64 to 0..63 and do manual scaling
        Instr[a]->Volume = (int)Instr[a]->Volume * Instr[a]->ManualScale / 100;
        Instr[a]->Volume = (int)Instr[a]->Volume * 63 / 64;

        /* ManualScale may overflow, so this check */
        if(Instr[a]->Volume > 63)Instr[a]->Volume=63;

        /* Volume bits 6 and 7 are unused */

        if(!Verify && !Nesify)
        {
            fwrite(&Instr[a]->DosName[1], 1, 1, fo); //finetune
            fwrite(&Instr[a]->D,         11, 1, fo); //adlib data
            fwrite(&Instr[a]->Volume,     1, 1, fo); //default volume
            fwrite(&Instr[a]->C4Spd,      2, 1, fo); //c2spd (adlib only)
            fwrite(&Instr[a]->DosName[0], 1, 1, fo); //patch
            fwrite(&Instr[a]->DosName[2], 1, 1, fo); //bank
            
            if(inames)
            {
                char *s = strdup(Instr[a]->Name);
                int a = strlen(s);
                while(a>0 && s[a-1]==' ')s[--a]=0;
                fprintf(fo, "%.27s", s);//name
                fputc(0, fo);
                free(s);
            }
        }
    }
}

/* Return value: Count of bytes added */
static unsigned FlushWait(unsigned const Wait, BYTE **const q)
{
    if(Wait)
    {
        if(Wait <= 15)
        {
            *(*q)++ = Wait - 1;
            return 1;
        }
        *(*q)++ = 15;
        *(*q)++ = Wait - 16;
          
        return 2;
    }    
    return 0;
}

static void WriteConvertPattern(unsigned patnum, const BYTE *p, unsigned PatLen, FILE *const fo)
{
    /* The pattern can not be bigger than this */
    BYTE PatBuf[MAXROW*18*5], *q = PatBuf;
    BYTE OldInstru[MAXCHN];
    
    unsigned Length_so_far=0;
    unsigned Wait=0;
    unsigned Row;

    for(Row=0; PatLen>0; ++Row, ++Wait)
        for(;;)
        {
            BYTE Note=0;
            BYTE Instru=0; //gcc happy
            BYTE Volume=0; //gcc happy
            BYTE Command=0; //gcc happy
            BYTE InfoByte=0;
            BYTE b;
            char ok;
            
            if(!PatLen)
                printf("intgen: Premature end of data in S3M pattern %u!\n", patnum);

            b = *p++;
            ok = There[patnum][Row] || allpat;
            --PatLen;
            
            if(!b)break;
            if(b < 0x20)
            {
                printf("intgen: Irrelevant byte %02X in pattern %u at row %u\n", b, patnum, Row);
                continue;
            }
            
            if(ChanConv[b&31] == 127)ok = 0; /* Hylätään rivi.       */
            if((b&31) >= MAXCHN)ok = 0;      /* Hylätään tällöinkin. */

            b = (b & 0xE0) | ChanConv[b & 0x1F];
            
            if(b & 0x20)
            {
                Note   = *p++;
                Instru = *p++;
                
                PatLen -= 2;
                
                /* 00 = no instrument */
                if(Instru)
                {
                    if((Instru=InstConv[Instru]) == 255)
                    {
                        /* Hylätään instrumentti ja nuotti. */
                        b &= ~32;
                    }
                    ++Instru;
                }
            }
            if(b & 0x40)
            {
                Volume = *p++ % 65;
                
                --PatLen;
                
                if(FMVolConv)
                    Volume = (MakeLinear
                        ? GMVol[Volume]
                        : FMVol[Volume]
                        ) >> 1;
                
                if(b&32)
                    Volume = (int)Volume
                             * Instr[InstCon2[Instru-1]]->ManualScale
                             / 100;
                
                Volume = Volume * 64 / 65; /* Convert volume */
                /*
                    *64/65 is correct.
                    *63/64 would make the scale harsh.
                */
                
                /* May overflow because of manualscale, so ensure */
                if(Volume > 63)Volume=63;
            }
            
            if(b & 0x80)
            {
                Command = *p++;
                InfoByte= *p++;
                
                PatLen -= 2;
                
                if(Command=='B'-64 && InfoByte!=OrdCvt[InfoByte])
                {
                    if(Verbose && !Verify)
                        printf("Pattern %u row %u: B%02X converted to B%02X  \n",
                            patnum, Row, InfoByte, OrdCvt[InfoByte]);
                    
                    InfoByte = OrdCvt[InfoByte];
                }
            }

            if(b >= 32 && ok) /* Acceptable? */
            {
                int PutInstru=0;
                
                if(InfoByte)
                {
                    if(Command)
                        b |= 16;
                    else
                    {
                        if(Verbose)
                            printf("Warning: Infobyte %02X without command in pattern %u row %u\n", 
                                InfoByte, patnum, Row);
                    }
                }

                /* Kaikissa mahdollisissa epäsuorissa saapumispisteissä
                 * resetoidaan instru-cache. Näitä pisteitä ovat patternien
                 * alut (ordereita saatetaan hyppiä käsin), sekä ne kohdat
                 * mihin tullaan Cxx:n kautta.
                 */
                if(Join[patnum][Row])
                {
                    memset(OldInstru, 255, sizeof OldInstru);
                }
                
                Length_so_far += FlushWait(Wait, &q);
                Wait = 0;

                Length_so_far++, *q++ = b;

                if((b&32) && (OldInstru[b&15] != Instru) && Instru)
                    PutInstru = 1;
                
                if(MayInstruOpt && PutInstru)
                    Note |= 128;
                else if(!MayInstruOpt)
                {
                /*  if(!Instru)Instru = OldInstru[b&15]; */
                    PutInstru = 1;
                }

                if(b&16)
                    Length_so_far++, *q++ = InfoByte;
                if(b&32)
                {
                    Length_so_far++, *q++ = Note;
                    if(PutInstru)
                        Length_so_far++, *q++ = OldInstru[b&15] = Instru;
                }
                if(b&64)
                {
                    Length_so_far++, *q++ = Volume; /* Convert volume */
                    if(!(b&32))OldInstru[b&15] = 255;
                }
                if(b&128)
                    Length_so_far++, *q++ = Command;
            }
        }
    
    Length_so_far += FlushWait(Wait, &q);
    
    fputc(Length_so_far & 255, fo);
    fputc(Length_so_far >> 8,  fo);
    
    fwrite(PatBuf, Length_so_far, 1, fo);
}

static void ReadConvertWritePats(FILE *const f, FILE *const fo)
{
    unsigned a;

    /****** READ, CONVERT AND WRITE THE PATTERNS ******/
    for(a=0; a<Hdr.PatNum; a++)
    {
        BYTE *p;
        unsigned PatLen;
        
        if(!PatnPtr[a])
            PatLen = 0;
        else
        {
            BYTE tmp;
            fseek(f, StartPtr + ((long)(PatnPtr[a]) << 4), SEEK_SET);
            tmp = fgetc(f);
            PatLen = (fgetc(f) << 8) | tmp;
        }

        if(!PatLen)
        {
            /* Empty pattern is 64 rows long by default */
            
            p = malloc(PatLen = 64);
            if(!p)MemoryTrouble("ReadConvertWritePats");
            memset(p, 0, PatLen);
        }
        else
        {
            p = malloc(PatLen -= 2);
            if(!p)MemoryTrouble("ReadConvertWritePats");
            fread(p, 1, PatLen, f);
        }
        
        WriteConvertPattern(a, p, PatLen, fo);
        
        free(p);
    }
}

static void CWrite(FILE *const f, FILE *const fo, const char *Name)
{
    unsigned i, l, A;
    int a;
    
    if(MFName[0])fprintf(f, "/* %s */\n", MFName);
    fprintf(f,
        "#ifdef __DJ_size_t\n"
        "#define _SIZE_T\n"
        "#endif\n"
        "#ifdef _SIZE_T\t\t/* Not quite portable test... */\n"
        "extern \n"
        "#endif\n");
    if(!NoFar)
        fprintf(f, "#ifdef __GNUC__\n");
    fprintf(f, "unsigned char %s[]\n", Name);
    if(!NoFar)
        fprintf(f,
            "#else\n"
            " #ifdef __STDC__\n"
            "  #define far /* undefined */\n"
            " #endif\n"
            "unsigned char far %s[]\n"
            "#endif\n", Name);
    fprintf(f,
        "#ifndef _SIZE_T\n"
        "=\n{\n");

    fwrite(fo, 0, 0, fo); //Truncate

    fseek(fo, 1, SEEK_SET);
    A=fgetc(fo);

    rewind(fo);

    if(A>2)
    {
        fprintf(f, "\t");
        for(i=l=0; (a=fgetc(fo))!=0; i++)
        {
            fprintf(f, "%d,", a);
            l+=2;if(a>9){l++;if(a>99)l++;}
        }
        fprintf(f, "0,");
        fprintf(f, l+i>=66?"\n\t":" ");
        fprintf(f, "/* ");
        for(rewind(fo); (a=fgetc(fo))!=0; )fputc(a, f);
        fprintf(f, " */\n");
    }

    // l=line length yet, i=line position
    for(i=l=0, a=fgetc(fo); ; i++)
    {
        if(i==0)fprintf(f, "\t");

        fprintf(f, "%d", a);
        l+=2;if(a>9){l++;if(a>99)l++;}

        a = fgetc(fo);
        if(a == EOF)break;

        fputc(',', f);

        if(l > 246){fprintf(f, "\n");l=0;i=-1;}
    }
    fclose(fo);

    fprintf(f, "\n}\n#endif\n;\n");
}

static void ASMWrite(FILE *const f, FILE *const fo,
                     unsigned RowCount, long CPos, long Newpos)
{
    int a, y, Len;
    
    if(MFName[0])fprintf(f, "; %s\n;\n", MFName);
    fprintf(f,
        ".386c\n"
        "jumps\n"
        "code segment byte use16\n"
        "org 100h\n"
        "assume cs:code,ds:code,es:nothing,ss:code\n"
        "start:"
        "call CPU\n"
        "Msg\tdb 'This executable has been assembled from a '\n"
        "\tdb 'file created with intgen.',13,10,'Playing \"'\n"
        "\tdb \"%s\",0\n"
        "Ms2 db '\"...',13,10,0\n"
        "cld\n"
        "mov ax,cs\n"
        "mov ds,ax\n"
        "mov es,ax"
        "\n" /* Memory size fix */
        "mov ah,4Ah\n"
        "mov bx,offset Loppu+31\n"
        "int 21h"
        "\n" /* Detection */
        "call Tu\n"
        "in al,dx\nmov si,ax\n"
        "mov ax,0FF02h%s"
        "mov ax,2104h%s"
        "xor cx,cx\n@@D:loop @@D\n"
        "in al,dx\nmov di,ax\n"
        "call Tu\n"
        "and si,224\njnz N\n"
        "and di,224\n"
        "cmp di,192\nje Do\n"
        "N:" /* Error message - not found OPL */
        "lea edx,CT\n"
        "mov[edx],' LPO'\n" /* `OPL ' */
        "Q:call F9\n"
        "mov ax,4C01h\nint 21h\n"
        "Tu:mov ax,6004h%smov ax,8004h\njmp OB\n",
        Hdr.Name,
        "\ncall OB\n",
        "\ncall OB\n",
        "\ncall OB\n");

    fprintf(f,
        "Do:" /* Found, muistin varaus */
        "call ID\n" /* delayloop init ensin */
        "call RS\n" /* opl:n resetointi my”s */
        "%s2,ax\n"
        "%s6,ax\n"
        "%s10,ax\n"
        "%s14,ax\n",

        "call @@A\nmov L.word ptr ",
        "call @@A\nmov L.word ptr ",
        "call @@A\nmov L.word ptr ",
        "call @@A\nmov L.word ptr ");

    fprintf(f,
        /* Purku */
        "mov bp,3\n"                    /* bp = Chan */
        "@@P3:"
        "xor dx,dx\n"                  /* dx = ThisRow */
        "mov bx,dx\n"                  /* bx = ThisRow*2 */
        "mov si,bp\n"
        "add si,si\n"
        "add si,bp\n"                  /* si=bp*3, bp+si=bp*4 */
        "les di,dword ptr L+bp+si\n"  /* es:di = Laulu[Chan] */
        "@@P4:"
        "call @@R\nnot al\nxchg cx,ax\n"  /* cx */
        "call @@R\n");                    /* al */
    if(NESCompress<256)
        fprintf(f,
            "cmp cl,%u\n"
            "je @@P1\n", NESCompress);
    fprintf(f,
        "mov ah,cl\n"          /* al=volume, ah=note */
        "mov es:[bx+di],ax\n"
        "db 'BCC'\n");  /* dx++, bx+=2 */
    if(NESCompress<256)
    {
        fprintf(f,
            "jmp @@P2\n"
            "@@P1:");
        if(NESXOR)fprintf(f, "xor al,%u\n", NESXOR);
        fprintf(f,
            "xchg cx,ax\n" /* cx<-al */
            "call @@R\n");/* al */
        if(NESXOR)fprintf(f, "xor al,%u\n", NESXOR);
        fprintf(f,
        #if 0
            /* FIXME: NESDepf mukaan */
            "mov ah,cl\n" /* al = juuri luettu */
            "shr ah,6\n"  /* ah = cx\64 */
            "and cx,63\n" /* cx &= 63 */
        #else
            "mov ah,cl\n"
            "mov cx,ax\n"
            "and cx,%d\n"
            "shr ax,%d\n",
                (1<<NESLength)-1,
                16-NESDepf);
        fprintf(f,                        
        #endif                    
            "add ax,ax\n"
            "@@L:"
            "mov si,di\n"
            "sub si,ax\n"
            "mov si,es:[bx+si]\n"
            "mov es:[bx+di],si\n"
            "db 'BCC'\n"  /* dx++, bx+=2 */
            "loop @@L\n"
            "@@P2:");
    }
    fprintf(f,
        "cmp dx,%u\n"
        "jb @@P4\n"
        "dec bp\n"
        "jns @@P3\n",
        RowCount
    );

    fprintf(f,
        /* Soitto */
        "lea edx,Msg\ncall F9\n"
        "lea edx,Ms2\ncall F9\n"
        "Replay:"
        "call Delay\n");
    fprintf(f,
        "les si,L.dword ptr %d\nlea edi,S%d\n"
        "mov bp,%d\nmov si,%d\ncall @@W\n",0,  1,0, 0);
    fprintf(f,
        "les si,L.dword ptr %d\nlea edi,S%d\n"
        "mov bp,%d\nmov si,%d\ncall @@W\n",4,  1,1, 1);
    fprintf(f,
        "les si,L.dword ptr %d\nlea edi,S%d\n"
        "mov bp,%d\nmov si,%d\ncall @@W\n",8,  2,2, 2);
    fprintf(f,
        "les si,L.dword ptr %d\nlea edi,S%d\n"
        "mov bp,%d\nmov si,%d\ncall @@W\n",12, 3,8, 3);
    fprintf(f,
        "db 184\nr dw 0\n" /* 184 = 0B8h = mov ax,imm16 */
        "inc ax\n"
        "cmp ax,%u\n"
        "jb @@LO\n"
        "mov ax,%u\n"
        "@@LO:mov r,ax\n"
        "mov ah,11h\n"
        "int 16h\n"
        "jz Replay\n"

        "call RS\n" /* opl:n resetointi */
        "mov ax,4C00h\n"
        "int 21h\n",
        RowCount,
        (unsigned)NESLoop
    );

    fprintf(f, "@@R:");
    if(NESInterlace)
        fprintf(f,
            "db 190,0,0\n" /* mov si,0 */
            "shr si,1\n"
            "jnc @@R1\n"
            "add si,%u\n"
            "@@R1:"
            "movzx ax,Musa[si]\n"
            "inc word ptr @@R+1\n"
            "ret\n",
            (int)((Newpos+1) >> 1));
    else
        fprintf(f,
            "mov si,Data\n"
            "xor ax,ax\n"
            "lodsb\n"
            "mov Data,si\n"
            "ret\n");

    fprintf(f,
        "@@W:" /* Yksi nuotti - input: bp=port index, es:[si]=patndata */
        "mov bx,r\n" /* di = offset of adlib data */
        "add bx,bx\n"
        "mov al,es:[bx]\n"
        "or al,al\n"
        "jnz @@w1\n"
        "lea ax,[si+176]\ncall OB\n"  /* 176 = 0xB0 */
        "ret\n"
        "@@w1:"
        "push si\n");
    fprintf(f, "lea ax,[bp%+d]\nmov ah,[di%+d]\ncall OB\n", 0x20,0);
    fprintf(f, "lea ax,[bp%+d]\nmov ah,[di%+d]\ncall OB\n", 0x23,1);
    fprintf(f, "lea ax,[bp%+d]\ncall OB\n", 0xF060);
    fprintf(f, "lea ax,[bp%+d]\ncall OB\n", 0xF063);
    fprintf(f, "lea ax,[bp%+d]\ncall OB\n", 0xF080);
    fprintf(f, "lea ax,[bp%+d]\ncall OB\n", 0xF083);
    fprintf(f, "lea ax,[bp%+d]\nmov ah,[di%+d]\ncall OB\n",0xE0,4);
    fprintf(f, "lea ax,[bp%+d]\nmov ah,[di%+d]\ncall OB\n",0xE3,5);
    fprintf(f, "mov si,%d\nmov dl,[di%+d]\ncall VL\n", 0x40,2);
    fprintf(f, "mov si,%d\nmov dl,[di%+d]\ncall VL\n", 0x43,3);
    fprintf(f, "pop bp\n");
    fprintf(f, "lea ax,[bp%+d]\nmov ah,[di%+d]\ncall OB\n",0xC0,6);
    fprintf(f,
        "mov al,es:[bx+1]\n"
        "aam 12\n"
        "movzx cx,ah\n"
        "mov si,ax\n"
        "and si,15\n"
        "add si,si\n"
        "mov ax,P[si]\n"
        "jcxz @@f3\n"
        "@@f:"
        "add ax,ax\n"
        "loop @@f\n"
        "@@f3:" /* Nyt ax=Herz, cx=0 */
        "cmp ax,512\n"
        "jb @@f2\n"
        "inc cx\n"
        "shr ax,1\n"
        "jmp @@f3\n"
        /* Nyt cx=Oct, ax=Herz=0..511 */
        "@@f2:\n"
        "xchg bx,ax\n"
        /* cx=Oct (0..7), bx=Herz (0..3FFh) */
        "and cx,7\n"
        "and bh,3\n"
        "lea ax,[bp+160]\n"
        "mov ah,bl\n"
        "call OB\n" /* OB s„„st„„ bx,cx,si,di,bp */
        /* OPL_Byte(0xA0+c, Herz&255); // F-Number low 8 bits */
        "lea ax,[bp+176]\n"
        "mov ah,cl\n"
        "or ah,8\n" /* 20h after shl 2 */
        "shl ah,2\n"
        "or ah,bh\n"
        "call OB\n"
        /* OPL_Byte(0xB0+c, 0x20       //Key on
                  | ((Herz>>8)&3)      //F-number high 2 bits
                  | ((Oct&7)<<2)
        */
        "ret\n"

        "VL:" /*  WriteR si+bp, (63 - ((63-D&63) * Vol / 64)) | (D&0xC0)*/

        "mov al,dl\n"
        "and al,63\n"
        "sub al,63\n"
        "mov cl,63\n"
        "neg al\n"

        "mul es:byte ptr[bx]\n"
        "shr ax,6\n"
        "sub cl,al\n"

        "and dl,192\n"
        "or cl,dl\n"

        "lea ax,[bp+si]\n"
        "mov ah,cl\n"
        "jmp OB\n");

    if(!NESInterlace)fprintf(f, "Data dw Musa\n");
    
    fprintf(f, "Musa");
    fseek(fo, CPos, SEEK_SET);
    
    for(y=a=Len=0; Newpos>0; Len=1)
    {
        int Y;
        BYTE b;
        char Buf[4];
        b = fgetc(fo);
        #define QUOTABLE(b) (((b>=32&&b<127)||(b>=128&&b<255))&&b!='\'')
        if(QUOTABLE(b))
        {
            if(!y)
            {
                BYTE n = fgetc(fo);
                fseek(fo, -1, SEEK_CUR);
                if(!QUOTABLE(n))goto Eips;
            }
            sprintf(Buf, "%c", b);
            Y=1;
        }
        else
        {
        Eips:
            sprintf(Buf, "%u", b);
            Y=0;
        }
        #undef QUOTABLE
        if(a+strlen(Buf) > 66)
        {
            if(y)fprintf(f, "'");
            fprintf(f, "\n");
            a=0;
            y=0;
        }
        if(!a)fprintf(f, Len?"%7s ":" %s ","db");
        if(y != Y)
        {
            if(a>0 && Y)a++, fprintf(f, ",");
            a++, fprintf(f, "'");
               y=Y;
        }
        if(a>0 && !y)
            a++, fprintf(f, ",");

        a += fprintf(f, "%s", Buf);

        Newpos--;
    }
    if(y)fputc('\'', f);
    
    fprintf(f,
        "\n\n"
        "DC\tdd 0\n"                /* Delaycount             */
        "D55\tdd 55\n"              /* Constant: 55L          */
        "S40\tdw 40h\n"             /* Constant: Seg0040      */
        "L\tdd 0,0,0,0\n"           /* Song pointers, 4*dword */
        "CT\tdb '386 not found.',13,10,0\n"
        /*   D[  0, 1,  2,3,   8,9,10 ] */
        "S1\tdb 33, 1, 14,8,   3,0,2\n"
        "S2\tdb 34,33,192,192, 0,0,5\n"
        "S3\tdb 15, 0,  0,0,   0,1,14\n"
        "P\tdw "); /* periods: */

    for(a=0; a<12; a++)
        fprintf(f, "%ld%s", Period[a]*10000L/22050,a<11?",":"");

    fprintf(f,
        "\n"
        "Delay:"
        "mov cx,%u\n"
        "mov es,S40\n"
        "mov di,0\n"
        "mov bl,es:[di]\n"
        "DC1:"
        "mov eax,DC\n"
        "call DLo\n"
        "loop DC1\n"
        "ret\n"

        "DLo:"
        "sub eax,1\njc @q\ncmp bl,es:[di]\nje DLo\nret\n"

        "ID:"
        "mov es,S40\n"
        "mov di,6Ch\n"
        "mov bl,es:[di]\n"
        "ID2:cmp bl,es:[di]\n"
        "je ID2\n"
        "ID3:mov bl,es:[di]\n"
        "mov eax,-28\n"
        "call DLo\n"
        "not eax\n"
        "cdq\n"
        "idiv D55\n"   /* 1 tick is 54.925493219 ms */
        "mov DC,eax\n"
        "ret\n"

        "F9:"
        "mov bx,dx\n"
        "mov dl,[bx]\n"
        "or dl,dl\n"
        "jz @q\n"
        "mov ah,2\n"
        "int 21h\n"
        "inc bx\n"
        "jmp F9+2\n"

        "@@A:\n" /* Allocates memory */
        "mov ah,48h\n"
        "mov bx,%u\n"
        "int 21h\n"
        "ret\n",
        2200 / Hdr.InTempo,
        ((RowCount*2+255) >> 4)
        /* 255 instead of 15 = leave space for bugs... */
    );

    /* Write OB() */
    fprintf(f,    /* al=index, ah=data */
        "OB:"
        "push cx\n"
        "mov dx,388h\n"
        "out dx,al\n"
        "mov cx,6\n"
        "o1%s1\n"
        "inc dx\n"
        "mov al,ah\n"
        "out dx,al\n"
        "mov cx,35\n"
        "o2%s2\n"
        "dec dx\n"
        "pop cx\n"
        "ret\n",
        ":in al,dx\nloop o",
        ":in al,dx\nloop o");

    fprintf(f,
        "CI:"
        "xor cx,cx\n"
        "mov bp,sp\n"
        "add word ptr[bp],3\n"
        "iret\n"

        "CPU:"
        "push sp\n"
        "pop bp\n"
        "cmp sp,bp\n"
        "jne CF\n" /* 8088, not good */
        "mov es,r\n"
        "push word ptr es:26\n"
        "push word ptr es:24\n"
        "mov es:word ptr 24,offset CI\n"
        "mov es:word ptr 26,cs\n"
        "mov cx,1\n"
        "db 15,32,194\n" /* mov edx, cr0 */
        "jcxz CF\n"
        "pop dword ptr es:24\n"
        "push offset S[9]\n" /* Hypätään kahden sanan yli */
        "S:"
        "mov bx,[bp]\n"
        "cmp byte ptr[bx-1],0\n"
        "je @q\n" /* Tän täytyy pysyä shorttina, huom */
        "inc word ptr[bp]\n"
        "jmp S\n"

        "CF:"
        "mov dx,offset CT\n"
        "jmp Q\n"

        "RS:"
        "xor ax,ax\n"
        "push ax\ncall OB\npop ax\n"
        "inc ax\n"
        "cmp ax,244\n"
        "jbe RS+2\n"
        "@q:ret\n"

        "Loppu label byte\n"
        "code ends\n"
        "end start\n");
}

static void NESWrite(const char *DestFile, const char *Ext, FILE *const f, FILE *const fo)
{
    long Newpos = ftell(fo);
    long RowCount = Newpos/8;
    long RowPos;
    long CPos;
    int a, Max=4;    /* Channel count */

    WORD *Laulu[4];

    int Desperate=0;
    
    for(a=0; a<Max; a++)
    {
        Laulu[a] = (WORD *)malloc((int)(RowCount*2));
        if(!Laulu[a])MemoryTrouble("NESWrite");
    }
    
    if(InternalFB==1)
    {
        /* This might be a bit faster */
        rewind(fo);
        for(a=0; a<Max; a++)fread(Laulu[a], 2, (unsigned)RowCount, fo);
    }
    else
    {
        if(Verbose)printf("Reordering bytes...\n");

        /* This first reorders the bytes */
        for(a=0; a<Max; a++)
            for(RowPos=0; RowPos<RowCount; RowPos++)
            {
                BYTE N, V;
                fseek(fo, a+RowPos*8, SEEK_SET);
                N = fgetc(fo);
                fseek(fo, 3, SEEK_CUR);
                V = fgetc(fo);
                if(!N)N=1;
                Laulu[a][(int)RowPos] = (((WORD)V) << 8) | N;
            }
    }
        
    if(InternalFB==2)
    {
        rewind(fo);
        fwrite(&NESLoop,  1, sizeof(NESLoop), fo);
        fwrite(&Hdr,      1, sizeof(Hdr),     fo);
        for(a=0; a<Max; a++)fwrite(Laulu[a], 2, (unsigned)RowCount, fo);
        return;
    }

    if(RowCount > INT_MAX)
    {
        printf("intgen: Warning: Excess rows %ld - %u is max\n",
            RowCount, INT_MAX);
        RowCount = INT_MAX;
    }

#ifdef __BORLANDC__
Retry:    
#endif
    fseek(fo, CPos=0, SEEK_SET);

    if(Verbose&&NESCompress&&!Desperate)printf("Compressing...\n");
    
    {    /* Stream-osuus alkaa (sisentämättömänä, pah) */
    
    const int MaxLen = (1<<NESLength) - 1;
    long Bits=0; /* Helpers for the Stream macro  */
    long bk=1;   /* Bits in buffer (kerroin) */
    int y;       /* Currently compressing channel */
    
    #define StreamPut fputc(NESXOR^((NESRoll+(int)(Bits%NESCount))&255),fo)
    #define Stream(x, bits) \
    {   Bits = Bits + (x)*bk; \
        bk <<= (bits); \
        while(bk >= (long)NESCount) { StreamPut;Bits/=NESCount,bk/=NESCount; } \
    }
    #define StreamFlush() if(bk)StreamPut
    
    for(y=Max; --y>=0; )
    {
        register int w;        /* Currently compressing row */
        for(w=0; w<RowCount; w++)
        {
            int BLen=2;        /* Best found length            */
            short BPos=0;      /* Relative position /gcc happy */

            if(NESCompress<256)
            {
                register int d, b;   /* Helper position counters */
                register int a = (int)(w - ((1<<NESDepf) - 1));
                if(a<0)a=0;

                for(d=a; d<w; d++)
                    if(Laulu[y][d] == Laulu[y][w])
                    {
                        register int Len=1;    /* Current length */
                        a = d;
                        b = w;
                        for(;;)
                        {
                            a++;
                            if(a >= RowCount)break; /* a>=w also possible? */
                            if(++b >= RowCount)break;

                            if(Laulu[y][b] != Laulu[y][a])break;

                            if(++Len == MaxLen)break;
                        }
                        if(Len > BLen)
                        {
                            BLen = Len;
                            BPos = w-d;
                            if(Len == MaxLen)break;
            }       }   }
            /* Previously this was BLen>3, but since there's a possibility of failure   *
             * in the else-part (about invalid NESCompress value), this is better idea. *
             * This means that for Nesify==1, BLen>=3 (>2) and for Nesify==2, Blen>3    */
            if(BLen > 1+Nesify)
            {
                w += BLen-1;
                
                if(Nesify==2) /* C, not asm? */
                {
                    Stream(NESCompress, 7);
                    Stream(BPos /* ^((1<<NESDepf  )-1) */ , NESDepf);
                    Stream(BLen /* ^((1<<NESLength)-1) */ , NESLength);
                }
                else          /* asm */
                {
                    register short Tmp = BLen | (BPos << NESLength);
                
                    fputc(~NESCompress,       fo);
                    fputc(NESXOR^(Tmp >> 8),  fo);
                    fputc(NESXOR^(Tmp & 255), fo);
                }
            }
            else
            {
                register unsigned short W = Laulu[y][(int)w];
                if(Nesify==2)
                {
                    if((W&127) == NESCompress)
                    {
                        printf("intgen: Fatal: NESCompress value 0x%02X can not be used.\nTry using the -nc option instead.\n", NESCompress);
                        exit(1);
                    }
                    Stream((W&255),   7); /* Note   */
                    Stream((W>>8)^63, 6); /* Volume */
                }
                else
                {
                    if((W&255) == NESCompress)
                    {
                        printf("intgen: Fatal: NESCompress value 0x%02X can not be used.\nTry using the -nc option instead.\n", NESCompress);
                        exit(1);
                    }
                    fputc(~(W&255), fo);
                    fputc(W>>8,     fo);
    }   }   }   }
    
    if(Nesify==2)
        StreamFlush();
        
    #undef Stream
    #undef StreamPut
    #undef StreamFlush
    
    }    /* Stream-osuus päättyi */

    Newpos = ftell(fo)-CPos; /* Length of compressed data */

    #ifdef __BORLANDC__
    if(Newpos > 40000U && Nesify==1)
    {
        if(!Desperate)
        {
            printf(
                "intgen: Panic: Too big data (%ld > 40000)\n"
                "Trying something desperate, this may take a while...\n",
                Newpos);
            Desperate=1;
        }
        RowCount -= 8;
        goto Retry;
    }
    if(Desperate)
        printf("Data size is now %ld.\n", Newpos);
    #endif
    
    if(NESInterlace && Nesify!=2)
    {
        BYTE *Tmp;
        long Pv, w;
        
        if(Verbose)printf("Interlacing...\n");

        Tmp = (BYTE *)malloc((size_t)Newpos);
        if(!Tmp)MemoryTrouble("interlacing");

        fseek(fo, CPos, SEEK_SET);
        fread(Tmp, 1, (size_t)Newpos, fo);

        fseek(fo, CPos+=Newpos, SEEK_SET);
        Pv = (Newpos+1) >> 1;
        for(w=0; w<Newpos; w++)
        {
            long Pos;
            Pos = (w < Pv)
                ? w+w
                : 1 + ((w-Pv) << 1);
            fputc(Tmp[(WORD)Pos], fo);
        }

        free(Tmp);
    }

    if(Verbose)printf("Writing...\n");

    switch(Nesify)
    {
        /* The following case has been written in text mode of 160x64. *
         * The readability on terminal which of width is 80 characters *
         * may not be maximal. I am sorry, but it would be an annoying *
         * job to split all the lines. // Bisqwit                      */
        case 2:
        {
            static int Fla;
            int TriGraphs=0;
            
            fseek(fo, CPos, SEEK_SET);
            if(NesifyDataOnly)
            {
                fprintf(f,
                    "int Values[]={%ld /* size */, %ld, /* rows */\n"
                    "\t%ld /* loop */, %d /* xor */, %d, /* compr */\n"
                    "\t%d  /* depf */, %d /* len */,\n"
                    "\t%d /* count */, %d /* roll */,\n"
                    "\t%d /* tempo */\n"
                    "};typedef unsigned char by;\n",
                    Newpos,RowCount,NESLoop,NESXOR,NESCompress,
                    NESDepf,NESLength,NESCount,NESRoll,
                    Hdr.InTempo*2);
                if(MFName[0])fprintf(f, "/* %s */\n", MFName);
            }
            else
            {
                fprintf(f,
                    "#define _POSIX_SOURCE 1 /* Song length: %ld seconds (%ld seconds of loop). */\n",
                    RowCount * 5 / Hdr.InTempo / 2,
                    (RowCount-NESLoop) * 5 / Hdr.InTempo / 2);
                if(MFName[0])fprintf(f, "/* %s */\n", MFName);
                fprintf(f,
                    "/*/echo \"$0 is not an executable but a C source file. Compiling it now...\"\n"
                    "n=`basename $0`;gcc -O2 -Wall -W -pedantic -o /tmp/$n \"$0\";/tmp/$n $*;exit; */\n"
                    "static char*Title=\"%splayer for /dev/audio (8-bit, µ-law)\\n\"\n"
                    "\"This file has been compiled from a file created with intgen.\";\n"
                    "typedef unsigned char by;char*A,%s*tmp=\"nesmusa.$$$\",*a0;\n",
                    NESFullFeatured?"NES-S3M ":"Self",
                    NESFullFeatured?"*In,":"");
            }
            a=fprintf(f, "static by*z=(by*)\"");
            for(Fla=0; Newpos>0; )
            {
                static int Fla;
                char Buf[4];
                BYTE b = fgetc(fo);
                #define QUOTABLE(b) ((b>=32 && b<127)&&!Fla)
                if(b=='\\')
                    strcpy(Buf, "\\\\");
                else if(b=='\"')
                    strcpy(Buf, "\\\"");
                else if(b=='?')
                {
                    /* Special case for ANSI trigraphs */
                    b = fgetc(fo);
                    ungetc(b, fo);
                    
                    if(b=='?') /* strchr("=(/)'<!>-?", b)) */
                    {
                        /* Todo: '?' as last char on line */
                        strcpy(Buf, "?\"\"");
                        TriGraphs++;
                    }
                    else
                        strcpy(Buf, "?");
                }
                else if(QUOTABLE(b))
                    sprintf(Buf, "%c", b);
                else
                {
                    sprintf(Buf, "\\%o", b);
                    b = fgetc(fo);
                    Fla = (b>='0' && b<='9');
                    ungetc(b, fo);
                }                        
                #undef QUOTABLE
                if(a+strlen(Buf) > 77)
                {
                    fprintf(f, "\"\n");
                    Fla=a=0;
                }
                if(!a)a=fprintf(f, "\"");
                a += fprintf(f, "%s", Buf);

                Newpos--;
            }
            if(TriGraphs && Verbose)
                fprintf(stderr, "Warning: %d ANSI trigraph%s detected\n",
                    TriGraphs, TriGraphs>1?"s":"");
            
            if(NesifyDataOnly)
                fprintf(f, "\";\n");
            else
            {
                fprintf(f, "\",\n"
                    "%ss[5][65536],l[299999],%ssn[32]=\"%s\";\n"
                    "#include <stdio.h>\n#include <unistd.h>\n"
                    "static int %s,h,OF,PN,d,p=1,r,k,i,a,c,CO,j,x,T=%d,f=8000,LS=0,\nP[12],"
                    "H[99],IL[99];FILE*fo%s\n"
                    "static int Out(int v){int e=0,s=1,f=0;if(OF)v=128+v/30;else for((s=(v<0))?v=-v\n"
                    ":0;e<8;e++)if((f=((v+32)>>(e+1))-16)<16)break;return fputc(OF?v:~((s<<7)|(e<<4\n"
                    ")|f),fo);}void gn(void){for(f=0;A[1]>='0'&&A[1]<='9';f=f*10+*++A-'0');}int\n"
                    "G(int n){while((k=1<<n)>p)d+=p*",
                    NESFullFeatured ? "o[256],w[256][64],NN[8]," : "",
                    NESFullFeatured ? "\n" : "",
                    Hdr.Name,
                    NESFullFeatured
                        ?   "ss,m,IN,IV[99],IP[256],PP[100],b,M,I,D[99],"
                            "Bxx,Cxx,B,Ss,CP,\nAt[256][64],Np=0,S=SEEK_SET"
                        :   "S",
                    Hdr.InTempo*2,
                    NESFullFeatured ? ",*fi;\n#define rw(a) a=fgetc(fi),a|=fgetc(fi)<<8" : ";");
                        
                if(NESRoll)fprintf(f, "(by)(");
                if(NESXOR)fprintf(f, "(");
                fprintf(f, "*z++");
                if(NESXOR)fprintf(f, "^%d)", NESXOR);
                if(NESRoll)fprintf(f, "%+d)", NESXOR?(SBYTE)(-NESRoll):-NESRoll); /* Avoid +++ */
                fprintf(f,
                    ",p*=%d;PN=d&(k-1);d/=k,p%s/=k;%sreturn PN;}",
                    NESCount,
                    NESFullFeatured ? "" : "\n",
                    NESFullFeatured ? "\n" : "");
                fprintf(f, 
                    "int main(int C,char**V){%s"
                        "if(V)fi=stdin,a0=*V;for(P[CO=%s]=907;++i<12;P[i\n]=P[i-1]*89/84);",
                            NESFullFeatured?"\n":"",
                            NESFullFeatured
                            ?   "a=i=C?0:(fseek(fo,0,SEEK_END),r=ftell(fo)/8,rewind(fo),\n"
                                "fread(l,8,r,fo),fclose(fo),remove(tmp),z=(by*)In,main(-1,0))"
                            :   "i=0");
                            
                /* Auts, nyt tuli tosissaan spagettia.                                    *
                 * Tämä pitäisi varmasti tehdä aliohjelmilla, mutta voi miten olenkaan    *
                 * pahatapainen. Toinen vaihtoehto olisi käyttää sprintf:ää, mutta        *
                 * kello on vähän päälle viisi aamulla, joten käytän spagettia.           */
                 
                /* Selkeyden vuoksi nämä spagettilabelit *
                 * on numeroitu hyppyjärjestyksessä.     */
                 
                if(!NESFullFeatured)goto SPAGETTI_1;
                
                fprintf(f, "if(C<0){");
                
            SPAGETTI_2:
                fprintf(f,
                    /* FIXME: Please invent here something that checks for the *
                     *        existance of the sound device before writing!    */
                    "if(isatty(fileno(fo=stdout)))fo=fopen(A=OF?\"/dev/dsp\":\n"
                    "\"/dev/audio\",\"wb\");if(!fo){perror(A);exit(-1);}a=c=i=sn[29]=0;\n"
                    "for(fprintf(stderr,\"Playing %s (%%s)...\\n\",%ssn);;\n"
                    "i<3?i++:((c?c--^Out(S/9):++a>=r?a=LS:(c=f*5/T)),i=S=0)){\n"
                        "x=l[a*8+4+i],j=i?i-1:0;"
                        /* CHECKME: Does division really have   *
                         *          bigger priority than shift? */
                        "H[i] += %s<<x/12;\n"
                        "if(IL[j])S += (s[j][((unsigned)H[i]/f)%%IL[j]]-128)*l[a*8+i];"
                    "}",
                    NESFullFeatured?"%s":"built-in song",
                    NESFullFeatured?"*z-'*'?z:z+1,":"",
                    NESFullFeatured?"(D[j]?D[j]*P[x%12]/14500:P[x%12])":"P[x%12]");
            
                if(!NESFullFeatured)goto SPAGETTI_3;
                fprintf(f, "}\n");
                
            SPAGETTI_1:
                fprintf(f,
                        "while(--C)if(*(A=*++V)%s)while(*A&&A[1])\n"
                        "*++A=='r'?gn(),0:*A=='d'?OF=1:*A=='h'?h=1:0;%s"
                        "fprintf(stderr,"
                            "\"%%s\\n"
                            "\\nUsage:\\t %%s [options]%s\\n\"\n"
                            "\"Example: %%s%s\\n"
                            "\\t %%s -dr22050%s|esdcat %s-b -m -r 22050\\nOptions:\\n"
                            "\"\n"
                            "\"\\t -d\\tPlay in linear format instead & use /dev/dsp if not piped\\n\"\n"
                            "\"\\t -r#\\tSelect sampling rate in Hz\\n"
                            "\\t -h\\tThis help\\n\\n\","
                            "Title,a0,a0,a0);\n"
                        "if(h)exit(0);"
                        "for(r=%ld,LS=%ld,c=4;c--;)for(a=0;a<r;)if((S=G(7))-%d)\n"
                            "l[c+8*a+4]=S,l[c+8*a++]=G(6)^63;else for(i=G(%d),S=G(%d);S--;a++)\n"
                                "l[a*8+4+c]=l[(a-i)*8+4+c],l[a*8+c]=l[(a-i)*8+c];\n"
                        "for(i=IL[0]=IL[1]=32;--i>=0;s[0][i]=(i&24)?170:20)s[1][i]=16*(i<16?i:31-i);\n"
                        "for(i=IL[2]=999;--i>=0;s[2][i]=(a=a*999+1)%%200);%s",
                    NESFullFeatured?"-'-')In=A;else if(A[1]":"=='-'",
                    NESFullFeatured?"else In=\"*stdin\";\nif(!In){":"\n",
                    NESFullFeatured?" nesfile.s3m":"",
                    NESFullFeatured?" mman3_d2.s3m":"",
                    NESFullFeatured?" cv2b.s3m":"",
                    NESFullFeatured?"\"\n\"":"",
                    RowCount,NESLoop,NESCompress,NESDepf,NESLength,
                    NESFullFeatured?"z=(by*)\"built-in song\";\nmain(i,0);}":"\n");
                
                if(!NESFullFeatured)goto SPAGETTI_2;
                
                fprintf(f,
                    "if(*In-'*')fi=fopen(In,\"rb\");if(!fi||!(fo=fopen(tmp,\"wb+\")))\n"
                    "{perror(fi?tmp:In);return-1;}fread(sn,1,32,fi);rw(m);rw(IN);\n"
                    "rw(PN);fseek(fi,49,S);x=fgetc(fi);T=fgetc(fi)*2;fseek(fi,0x60,S);\n"
                    "fread(o,m,1,fi);for(i=0;i<IN;i++)rw(IP[i]);for(i=0;i<PN;i++)rw(PP[i]);\n"
                    "for(i=0;i<IN;i++)"
                        "fseek(fi,IP[i]*16+13,S),fgetc(fi),rw(LS),rw(IL[i]),rw(j),\n"
                        "fseek(fi,8,SEEK_CUR),IV[i]=fgetc(fi),fseek(fi,3,SEEK_CUR),\n"
                        "rw(D[i]),fseek(fi,LS*16,S),fread(s[i],1,IL[i],fi);\n"
                    "for(;;){if(CO>=m)main(0,V);if((CP=o[CO])-254){fseek(fi,PP[CP]*16,S);\n"
                    "rw(i);if(!i)PP[CP]=0;if(PP[CP])fread(z=s[4],i-=2,1,fi);"
                    "for(r=0;r<64;r++){if(!r)ss=1;\n"
                        "a?0:w[CO][r]?(LS=At[CO][r]),main(0,V):(At[CO][r]=ftell(fo)/8,(w[CO][r]=1));\n"
                        "Bxx=Cxx=-1,Ss=ss,j=B=ss=0;"
                        "if(PP[CP])while((b=*z++)-(M=I=0)){\n"
                            "if(b&32){if(!a){if(*z<254)NN[(b&3)+4]=*z%%16+12*(*z/16);else\n"
                            "if(*z==254)NN[b&3]=0;if(z[1]&&*z!=254)NN[b&3]=IV[z[1]-1];}z+=2;}\n"
                            "if(b&64){if(!a)NN[b&3]=*z;z++;}if(b&128){M=*z++;I=*z++;}\n"
                            "if(M==1&&!a)x=I;M==2?Bxx=I:M==3?Cxx=I:M==4?ss=1:0;\n"
                            "if(M==19){"
                                "if(I/16==11)*((B=I&15)?&ss:&Ss)=1;"
                                "if(I/16==14)j=I&15;"
                        "}}"
                        "if(a)\na--;else{"
                            "if(Ss)Np=ftell(fo);for(i=(j+1)*x;i;i--)fwrite(NN,1,8,fo);\n"
                            "if(B){c=ftell(fo)-Np;fseek(fo,Np,S);fread(l,1,c,fo);\n"
                            "fseek(fo,Np+c,S);for(i=B;i;i--)fwrite(l,1,c,fo);}\n"
                            "if(Bxx>=0){CO=Bxx-1;if(Cxx<0)break;}\n"
                            "if(Cxx>=0){a=(Cxx&15)+10*(Cxx/16);break;}"
                        "}"
                    "}}CO++;}");
            
            SPAGETTI_3:                    
                fprintf(f, "}\n");
            } /* nesifydataonly */
            
            if(Verbose)
                printf(
                    "Done. You may want to run \""
                    "indent -nbad -ts0 -i4 -bap -nbc -bli0 -c33 -cd33 "
                    "-ncdb -ci4 -cli0 -cp33 -d0 -di1 -nfc1 -nfca -i4 "
                    "-ip0 -l60 -lp -npsl -nsc -nsob -nss -npcs -nce "
                    "%s%s\" to enhance the readability of the "
                    "resulting program. :)\n", DestFile,Ext);
            break;
        }
        case 1:
            ASMWrite(f, fo, RowCount, CPos, Newpos);
            break;
     }
}

/* This routine does everything. */
static int MullistaMaailma(const char *s, const char *DestFile, const char *Name)
{
    /* Other variables. */
    int a;
    long InfSize;
    FILE *f, *fo;

    int crpending;

    const char *Ext = Nesify==1?".asm":".c";

    if(InternalFB)
        if(*DestFile != '*' && *DestFile != '$')
        {
            long Size, Size1;
            fo = fopen("nesmusa.$$$", "rb");
            if(!fo)
            {
                perror("nesmusa.$$$");
                return -errno;
            }
            fread(&NESLoop,  1, sizeof(NESLoop), fo);
            fread(&Hdr,      1, sizeof(Hdr),     fo);
            Size1 = ftell(fo);
            
            fseek(fo, 0, SEEK_END);
            Size = ftell(fo) - Size1;
            
            f = fopen("intgen.$$$", "wb+");
            if(!f)
            {
                perror("intgen.$$$");
                return -errno;
            }
            fseek(fo, Size1, SEEK_SET);
            while(Size>0)
            {
                char Diu[8192];
                
                int b = sizeof(Diu); /* Transfer buffer */
                if((long)b > Size)b = (int)Size;
                
                fread(Diu, 1, b, fo);
                fwrite(Diu, 1, b, f);
                
                Size -= b;
            }
            fclose(fo);
            fo = f;
            goto FBSkip;
        }

    if(!((f = fopen(s, "rb")) != (FILE *)(StartPtr=0)))
    {
        perror(s);
        return -errno;
    }
    fseek(f, 0, SEEK_END);
    InfSize = ftell(f);
    rewind(f);

    if(strcasecmp(s, "nesmusa.$$$"))
        fo = fopen("nesmusa.$$$", "wb+");
    else
    {
        fo = fopen("nesmusa.$$$", "rb+");
        if(Verbose)printf("Warning: Temporary file name is the same as input file name...\n");
    }

    if(!fo)
    {
        fclose(f);
        perror("nesmusa.$$$");
        return -errno;
    }
    rewind(fo);

    crpending = 0;

    if(Verbose>1)
    {
        printf("------------------- Selected options: -----------------------\n"
            "Source file:        %s\n", s);
        if(!Verify)
        {
            if(DestFile[0]!='*' && DestFile[0]!='$')
            {
                printf("Destination file:   %s%s\n", DestFile, Ext);
                if(!Nesify)
                    printf("Interface variable: char %s[]\n", Name);
            }
            if(!Nesify)
                printf("Array data format:  InternalHdr + Ords + InternalSamples\n");
            printf(
                "------------------------------------------------------------\n"
                "Creating temporary file using %s\n",
                s);
        }
    }
    else if(Verbose)
    {
        printf("InFile: %s", s);
        if(DestFile[0]!='*' && DestFile[0]!='$' && !Verify)
        {
            printf(" OutFile: %s%s", DestFile, Ext);
            if(!Nesify)printf(" VarName: %s[]", Name);
        }
        crpending = 1;
    }

    for(;;)
    {
        WORD w1, w2;
        
        StartPtr = ftell(f);
        if(StartPtr)printf("at 0x%08lX\n", StartPtr);
        fread(&Hdr, sizeof Hdr, 1, f);

        if(strncmp(Hdr.Name, "MZ", 2))break;

        if(Verbose)
        {
            if(crpending){crpending=0;printf("\n");}
            printf("Loading overlay ");
        }
        
        w1 = (((BYTE *)&Hdr)[2]) | (((BYTE *)&Hdr)[3] << 8);
        w2 = (((BYTE *)&Hdr)[4]) | (((BYTE *)&Hdr)[5] << 8);

        fseek(f, StartPtr + w1 + 512 * (w2 - 1), SEEK_SET);
    }

    if(strncmp(Hdr.Ident, "SCRM", 4))
    {
        if(crpending){crpending=0;printf("\n");}
        if(Verbose)
        {
            printf("Not a S3M file.\n");
        }
        fclose(f);
        return -255;
    }

    for(a=strlen(Hdr.Name); a; Hdr.Name[a]=0)
        if(Hdr.Name[--a] != ' ')break;

    if(crpending)printf(" (%s)", Hdr.Name);
    if(Verbose)
    {
        if(!crpending)printf("Song name: %s", Hdr.Name);
        printf(" (%ld bytes)", InfSize);
        crpending=1;
    }
    if(crpending){crpending=0;printf("\n");}

    fread(&Ords,    Hdr.OrdNum, 1, f);
    fread(&InstPtr, Hdr.InsNum, 2, f);
    fread(&PatnPtr, Hdr.PatNum, 2, f);

    if(Verify)
        printf("%u orders, %u instruments, %u patterns\n",
            Hdr.OrdNum, Hdr.InsNum, Hdr.PatNum);

    AllocateInstrus();
    ReadInstruments(f);
    ConvertInstruments(f);
    ScanPatterns(f, fo);
    SortInstruments();
    WriteHeader(fo);
    WriteInstruments(fo);

    if(!Nesify)
        ReadConvertWritePats(f, fo);
    
    fclose(f);
    
    /****** SAFELY CAN UNALLOCATE THE INSTRUMENT MEMORY ******/
    for(a=0; a<=MAXINST; a++)free(Instr[a]);

    if(Verify)
    {
        printf("Ok.\n");
        return 0;
    }

    if(*DestFile=='*' || *DestFile=='$' || (Name && (*Name == '*' || *Name =='$')))
    {
        if(Verbose)
            printf("We have now the temporary file nesmusa.$$$ containing the %s data.\n",
                Nesify
                    ? "selfplayer"
                    : "internal");

        if(!InternalFB)
        {
            fclose(fo);
            return 0;
        }
        Nesify    =1;
        InternalFB=2;
    }
    else
    {
        char Buf2[64];
        
FBSkip:
        sprintf(Buf2, "%s%s", DestFile, Ext);
        if(Verbose)printf("Writing from temporary file to %s\n", Buf2);

        /* BINARY to save space. Complain this if it makes you fun. */
        f = fopen(Buf2, "wb");
        if(!f)
        {
            perror(Buf2);
            return -1;
        }
    }

    if(Nesify)
        NESWrite(DestFile, Ext, f, fo);
    else
        CWrite(f, fo, Name);
    
    if(InternalFB)
        remove("intgen.$$$");

    fclose(f);
    
    return 0;
}

static const char *ifn = NULL;
static const char *ofn = NULL;
static const char *vn  = NULL;
static int Quiet = 0;

const char ProgName[] = "intgen";

void PrintVersionInfo(void)
{
    printf("%s - S3M data conversion tool v"VERSION" (C) 1992,2000 Bisqwit\n", ProgName);
}

void PrintDescription(void)
{
    printf("\nConverts MidiS3M files to the MidiS3M system internal format."
           "\nConverts NES-S3M files to many formats.\n");
}

void Case(int id, const char **dummy)
{
    switch(id)
    {
        case 'v':
            ++Verbose;
            break;
        case 'q':
            dup2(open(NULFILE, O_WRONLY), ++Quiet);
            break;
        case 'V':
            PrintVersionInfo();
            exit(EXIT_SUCCESS);
        case '?': Heelp=1;         break;
        case 'n': noname=1;        break;
        case 'f': NoFar=1;         break;
        case 'r': Verify=1;        break;
        case 'a': allins=1;        break;
        case 'p': allpat=1;        break;
        case 'o': MayInstruOpt=0;  break;
        case 'i': inames=1;        break;
        case 'l': MakeLinear=1;    break;
        case 'A': Nesify=1;        break;
        case 'I': NESInterlace=0;  break;
        case 'F': InternalFB=1;    break;
        case '2': NesifyDataOnly=1;break;
        case 2000: NESFullFeatured=0; Nesify=2; break;
        case 2001: NESFullFeatured=1; Nesify=2; break;
        case 'C': NESCompress = (int)ParamRange(0,255, dummy);   break;
        case 'X': NESXOR      = (int)ParamRange(0,255, dummy);   break;
        case 'L': NESLength   = (int)ParamRange(2,10,  dummy);   break;
        case 'O': NESCount    = (int)ParamRange(1,256, dummy);   break;
        case 'R': NESRoll     = (int)ParamRange(0,255, dummy);   break;
        case 'D':
            NESDepf = (int)ParamRange(2,14, dummy);
            NESLength = 16-NESDepf;
            break;
        case -1:
            /* Finish args, set default values if necessary */
            
            /* FIXME: Vulnerable code (scanf) */
            
            if(!ifn)
            {
                char *s = (char *)malloc(512);
                if(!access("intgen.msg", 4))
                {
                    /* Undocumented feature. Used in WSpeed. */
                    FILE *fp = fopen("intgen.msg", "rt");
                    for(;fgets(s,511,fp);)printf("** %s\r", s);
                    fclose(fp);
                }
                printf("Input file name? "); scanf("%s", s);
                ifn = s;
            }
            if(Verify)
            {
                Verbose=1;
                if(ofn)
                    printf("Warning: Output file name superfluous with --verify\n");
            }
            else
            {
                if(!ofn)
                {
	                char *s = (char *)malloc(512);
                    printf("Output file name? "); scanf("%s", s);
                    ofn = s;
                }
                if(!vn && !Nesify)
                {
                    char *s = (char *)malloc(512);
                    printf("Public variable name? "); scanf("%s", s);
                    vn = s;
                }
                if(vn && Nesify)
                    printf("Warning: Public variable name superfluous with --nes\n");
            }
            break;
    }
}

void DefArg(const char *arg)
{
    if(*arg == '(')
    {
        unsigned len = strlen(arg) - 2;
        strncpy(MFName, arg+1, len);
        MFName[len] = 0;

        /* #fb is a macro for Findbest to save command line space in dos */
        if(!strcmp(MFName, "#fb"))
        strcpy(MFName, 
            "This source was optimized with "
            "Findbest of the Bisqwit's MidS3Mtools pack");
        return;
    }

    if(!ifn)
        ifn = arg;
    else if(!ofn)
        ofn = arg;
    else if(!vn)
        vn = arg;
    else
        ArgError(2, arg);
}

struct Option Options[] =
{
    {'v', 'v',  0,"verbose",   "Verbose++"},
    {'q', 'q',  0,"quiet",     "Quiet++"},
    {'f', 'f',  0,"no-far",    "Make near data, not far"},
    {'l', 'l',  0,"linear",    "Make file volumes linear"},
    {'r', 'r',  0,"verify",    "Verifies source .S3M, implies -v"},
    {'n', 'n',  0,"noname",    "Disables song name from output file"},
    {'i', 'i',  0,"inames",    "Includes instrument names in internalformat file"},
    {'a', 'a',  0,"allins",    "Include also unused instruments"},
    {'p', 'p',  0,"allpat",    "Include also unused patterns"},
    {'o', 'o',  0,"nopt",      "Disables instrument number optimization"},
    {'A', 's','a',"nesa",      "Converts NES-S3M file into selfplaying .asm file (OPL)"},
    {2000,'s','c',"nesc",      "Converts NES-S3M file into selfplaying .c file (/dev/audio)"},
    {2001,'s','C',"nesC",      "Makes a full featured /dev/audio selfplayer (see intgen.txt)"},
    {'F', 'f','b',"internal1", "Internally used by 'findbest' program"},
    {'2', 'f','2',"internal2", "Make only the data in -sc"},
    {'I', 'n','i',"nesil",     "Disables interlaced format (-sa)"},
    {'C', 'n','c',"nesco",     "Sets the compression datavalue (-sa:0..256 -sc:0..127,256)"},
    {'O', 'n','o',"nescount",  "Sets the character count (1..256) (-sc)"},
    {'R', 'n','r',"nesroll",   "Sets the character roll (0..255) (-sc)"},
    {'X', 'n','x',"nesxor",    "Sets the compression xorvalue (0..255) (-sa)"},
    {'D', 'n','d',"nesdepf",   "Sets the compression depthbits (2..14) (-sa,-sc)"},
    {'L', 'n','l',"neslength", "Sets the compression lengthbits (2..10) (-sc)"},
    {'?', '?',  0,"help",      "Help"},
    {'V', 'V',  0,"version",   "Version information"},
    {0,0,0,0,0}
};

int main(int argc, char const *const *const argv)
{
    ReadArgs(argc, argv,
        "[\\(infotext\\)] [srcfile [destfile [pubname]]]\n"
        "\n"
        "Examples:\n"
        "  intgen -sa mman3_e2.s3m mman3_e2\n"
        "  intgen -b (Official WSpeed musicfile) \\mod\\tcsmods\\jhmadl52.s3m wspeed dah\n"
        "  intgen -vsCnd11nx0nc12nr40nl8 nes/dtales1q.s3m dtales1q\n"
        "  intgen music.s3m musicinc music_table",
        
        "\n"
        "Note: You must not supply extension for destfile.\n"
        "This program may be distributed under the terms of General Public License.\n"
        "See the file COPYING for more details. No warranty.\n"
        "This program is invoked and required by some other programs.\n");
    
    return MullistaMaailma(ifn, ofn, vn);
}
