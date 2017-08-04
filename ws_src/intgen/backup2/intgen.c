/*****************************************************************************
 *
 * Intgen - Originally the S3M to MidiS3M subsystem internalformat converter
 *          Contains also routines for handling the NES-S3M files
 *
 *          Copyright (C) 1999 Bisqwit (http://www.icon.fi/~bisqwit/)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 *****************************************************************************/

extern unsigned char adl[];     //Later
extern unsigned char FMVol[65]; //This too
extern unsigned char GMVol[64]; //This is in m_mpu.c

#ifndef UnderFMDRV

/* If 1, -sc makes only the data; not player. Set to 0. */
#define SC_MAKES_ONLY_DATA 0

#pragma option -K	//Unsigned chars
#include <io.h>
#if defined(__BORLANDC__)||defined(DJGPP)
 #include <conio.h>
#else
 #include "../fmdrv/fmdrv.h"
 #define unixversion
 #include "m_mpu.c"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "music.h"
#include "m_opl.h"

static int Batch=0;
static int Verbose=0;

static int FMVolConv = 0; // Going to convert the volumes for adlib?
static int MakeLinear = 0;// Making linear volumeformat?
static int noname = 0;    // Disable name output?
static int nopat = 0;     // Write no patterns?
static int Nesify = 0;    // Job is to NESify?

static int Hxx=1;         // Process infobytes
static int Qxx=1;
static int Verify=0;      // Verify only, no conversion

static int NESInterlace=1;	// 0=No interlacing (for Nesify==2, this is assumed as 0)
static int NESCompress=127;	// 256=No compressing
static int NESDepf=10;      // 10 bits for depth, 6 for length
static int NESLength=6;		// Or instead of 6, this
static int NESXOR=255;		// 0=No xorring
static int NESCount=64;		// For bit engine, char count
static int NESRoll=154;		// For bit engine, char add
static int NESFullFeatured; // 0=-sc, 1=-sC
static int InternalFB=0;	// 1=Assume readonly nesmusa.$$$ and ok already
static char MFName[64];   // Some description to put between /* */ in C file.

static int NoteHighBitNotUsed=1;

void S3MtoInternal(char *s, char *DestFile, char *Name)
{
    static far byte ChanConv[32];        //Channel conversion table
    static far byte InstConv[MAXINST+1]; //Instrument conversion table
    static far byte InstCon2[MAXINST+1];
	static far char There[MAXPATN][64];	 //Visited rows
	static far char Ortog[MAXORD][64];	 //Visited rows per order
	static far char Join[MAXPATN][64];   //Possible join points
    static far byte Patns[MAXPATN];      //Pattern conversion table
    static far byte OrdCvt[MAXORD];      //Order conversion table
    static far byte Ords[MAXORD];
	static far word InstPtr[MAXINST+1];
	static far word PatnPtr[MAXPATN];
	static far char InstUsed[MAXINST+1]; //Used instruments
	static far char NotUsed[MAXINST+1];	 //Not used instruments

	static far char Cut[64][MAXPATN];//for beautiful output of pattern
	static far int Gain[64][MAXPATN];//gut chains... I mean, cut gains...

	byte *TmpPat, *pp; //Pattern analyzer variables
	int Axx;

	extern unsigned char NESvol[65];
	static byte NESNote[4] = {1,1,1,1};
	static byte NESVol[4]  = {0,0,0,0};
    long NESPos=0, NESLoop=0;
    long *NESAt=NULL; // gcc happy

	#define NESOut(n) {register int a=(n)*Axx;for(;a>0;a--){ \
		fwrite(&NESNote,1,4,fo); fwrite(&NESVol, 1,4,fo); }}
	#define NESRemember() (NESPos=ftell(fo))
	// Hope the fread() in NESRepeat works...
	#define NESRepeat(count) \
		{int a; size_t c = (size_t)(ftell(fo)-NESPos); \
		 char *s = (char *)malloc(c); \
		 if(!s)goto MemoryTrouble; \
		 fseek(fo, NESPos, SEEK_SET); \
		 fread(s, 1,c, fo); \
		 fseek(fo, NESPos+c, SEEK_SET); \
		 for(a=count;a;a--)fwrite(s, 1,c, fo); \
		 free(s); }
	#define NESHere(ord,row) (NESAt[(ord)*64+(row)]=ftell(fo)/8)
	#define NESLoopStart(ord,row) (NESLoop=NESAt[(ord)*64+(row)])

	int Surprise, Investigating, Row, a, i, l, A, B, SkipSmp; //Other vars
	char ChanCount;
	char Buf2[64];
	long StartPtr;
	long InfSize;
	FILE *f, *fo;
	char OldInstru[16];

	int crpending;
    int SBNext=0; // gcc happy

	S3MHdr Hdr;
	ADLSample *Instr[MAXINST+1];

	char *Ext = Nesify==1?".asm":".c";

	#if defined(__BORLANDC__)||defined(DJGPP)
	 directvideo=0;
	#endif
	
	if(InternalFB)
		if(*DestFile != '*' && *DestFile != '$')
			goto FBSkip;

	if(!((f = fopen(s, "rb")) != (FILE *)(StartPtr=0)))
	{
		printf("Error %d (%s) opening file %s.\n",
			errno, sys_errlist[errno], s);
		return;
	}
	fseek(f, 0, SEEK_END);
	InfSize = ftell(f);
	rewind(f);

	if(stricmp(s, "nesmusa.$$$"))
		fo = fopen("nesmusa.$$$", "wb+");
	else
	{
		fo = fopen("nesmusa.$$$", "rb+");
		if(Verbose)printf("Warning: Temporary file name is the same as input file name...\n");
	}

	if(!fo)
	{
		fclose(f);
		printf("Error %d (%s) opening output file nesmusa.$$$.\n",
			errno, sys_errlist[errno]);
		return;
	}
	rewind(fo);

	crpending = 0;

	if(Verbose&&!Batch)
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
	else
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
		StartPtr = ftell(f);
	    if(StartPtr)printf("at 0x%08lX\n", StartPtr);
		fread(&Hdr, sizeof Hdr, 1, f);

		if(strncmp(Hdr.Name, "MZ", 2))break;

		if(Verbose)
		{
			if(crpending){crpending=0;printf("\n");}
			printf("Loading overlay ");
		}

		fseek(f,
			StartPtr +
			(((word *)&Hdr)[1] + 512 * (((word *)&Hdr)[2] - 1)),
			SEEK_SET);
	}

	if(strncmp(Hdr.Ident, "SCRM", 4))
	{
		if(crpending){crpending=0;printf("\n");}
		if(Verbose)
		{
            printf("Not a S3M file.\n");
		}
		fclose(f);
		return;
	}

	for(a=strlen(Hdr.Name); a; Hdr.Name[a]=0)if(Hdr.Name[--a] != ' ')break;

	if(crpending)printf(" (%s)", Hdr.Name);
	if(Verbose)
	{
		if(!crpending)printf("Song name: %s", Hdr.Name);
		printf(" (%ld bytes)", InfSize);
		crpending=1;
	}
	if(crpending){crpending=0;printf("\n");}

	fread(&Ords, Hdr.OrdNum, 1, f);
	fread(&InstPtr,Hdr.InsNum, 2, f);
	fread(&PatnPtr,Hdr.PatNum, 2, f);

	if(nopat)Hdr.OrdNum=Hdr.PatNum=0;

	if(Verify)
		printf("%u orders, %u instruments, %u patterns\n",
			Hdr.OrdNum, Hdr.InsNum, Hdr.PatNum);

	/****** ALLOCATE MEMORY FOR INSTRUMENTS ******/
	for(a=0; a<=MAXINST; a++)
	{
		Instr[a] = (ADLSample *)malloc(sizeof(ADLSample));
		if(!Instr[a])goto MemoryTrouble;
	}

	/****** SCAN THE INSTRUMENTS ******/
	for(SkipSmp=0; ;SkipSmp++)
	{
		fseek(f, StartPtr+InstPtr[Hdr.InsNum-1]*16, SEEK_SET);
		fread(Instr[0], sizeof(ADLSample), 1, f);
		if(Instr[0]->Typi != 0)break;
		if(!--Hdr.InsNum)break;
	}

	/****** READ THE INSTRUMENTS ******/
	for(A=B=a=0; a<Hdr.InsNum; a++)
	{
		fseek(f, StartPtr+InstPtr[a]*16, SEEK_SET);
		fread(Instr[a+1], sizeof(ADLSample), 1, f);
		// FIXME: Verify? samples...

		/* Calculate SMP count (A) and AME count (B) */
		if(Instr[a+1]->Typi==1)A++;
		if(Instr[a+1]->Typi==2)B++;
	}

	Hdr.InSpeed &= 127;
	if(A > B)/*Enemm„n SMP:it„ kuin AME-ja */
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

	/****** IF NECESSARY, CONVERT THE INSTRUMENTS ******/
	for(A=B=a=0; a<Hdr.InsNum; a++)
	{
		byte GM = 0;
        int scale=63;
		int ft=0; //Finetune

		fseek(f, StartPtr+InstPtr[a]*16, SEEK_SET);
		fread(Instr[a+1], sizeof(ADLSample), 1, f);

		Instr[a+1]->DosName[2] = 0; //No doublechannel mode
        Instr[a+1]->ManualScale = 100;

		if(Instr[a+1]->Name[0]=='G')    //GM=General MIDI
		{
			int c = Instr[a+1]->Name[1];
            if(c=='M'    // GM; M stands also for "melodic"
             ||c=='P')   // GP; P stands for "percussion"
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
                                "\t Finetune value out of ranges; using %d instead of %d.\n",
                                a, (char)ft, ft);
						}
						continue;
                    }
                    if(*s == '/')
                    {
						for(scale=0; isdigit(*++s); scale=scale*10 + *s-'0');
                        if(scale > 63)
                        {
                            Instr[a+1]->ManualScale = scale*100/63;
                            printf(
                                "Warning: Instrument %d: Scaling level too big; using 63 instead of %d\n"
                                "\t and performing manual scaling of %d%% during the conversion.\n",
                                a, scale,
                                Instr[a+1]->ManualScale);
                            scale=63;
                        }
                        continue;
					}
                    if(*s == '*')
					{
						s++, Instr[a+1]->DosName[2] = 1; //Doublechannel mode
						continue;
					}
                    break;
                }
				if(c=='P')GM+=128;
			}
			else
				B++;
		}
		else if(Instr[a+1]->Filler2[2]) //L”ytyy t„st„kin nuo tarvittaessa.
		{
			GM = Instr[a+1]->Filler2[2];
			ft = (signed char)Instr[a+1]->Filler2[3];
		}
		else
			B++;

		if(Instr[a+1]->Typi==1)
		{
			i=GM-1;

			if(i>=128)i-=35;

			A++;
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

			Instr[a+1]->Volume |= 128;		   //Converted.
		}

		Instr[a+1]->DosName[0]=GM;             //Parempaakaan paikkaa tolle...
		Instr[a+1]->DosName[1]=*((char *)&ft); //-"-
		Instr[a+1]->D[8] |= (scale<<2); //b1 and b0 select the waveform;
										//b7 to b2 have the scaling level.
	}

	if(Verbose&&B)
	{
		printf("Warning: %d instruments with no GM/GP header.\n",B);
		if(B*100/Hdr.InsNum > 80 && !Verify) //Yli 80%
			printf("That's quite much.\n");
	}
    if(A && !Verify && (Verbose || !Nesify))
		printf("Warning: %d samples converted to adlib instruments.\n", A);

	/****** DETERMINE, WHICH PATTERNS AND ROWS BE NEVER VISITED ******/
	memset(There, 0, sizeof There);
	memset(Patns, 0, sizeof Patns);
	memset(Ortog, 0, sizeof Ortog);
	memset(Join,  0, sizeof Join);

	for(a=0; a<Hdr.OrdNum; a++)
	{
		if(Ords[a]==255) //end
		{
			Hdr.OrdNum=a;
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

    MusData.CurOrd=0;
	Investigating=Surprise=a=0;

	Axx = Hdr.InSpeed;

	if(Nesify)NESAt = (long *)calloc(Hdr.OrdNum, 64*sizeof(long));

DetPat:
	Row=0;
    if(MusData.CurOrd >= Hdr.OrdNum)goto DonePatDet;

    MusData.CurPat=Ords[MusData.CurOrd];

    if(MusData.CurPat==0xFE)
	{
        MusData.CurOrd++;
		goto DetPat;
	}

    fseek(f, StartPtr+((long)(PatnPtr[MusData.CurPat])<<4), SEEK_SET);
	i=0, fread(&i, 2, 1, f);

	TmpPat = (char *)malloc(i-=2);
	if(!TmpPat)goto MemoryTrouble;
	pp=TmpPat;
	fread(pp, i, 1, f);

    if(a>0 && Ortog[MusData.CurOrd][a-1] && !Ortog[MusData.CurOrd][a] && Verbose)
	{
		printf(
			"Surprising jump to pattern %02d, row %02d...\n"
			"This kind of things are usually Bisqwit's handwriting!\n",
            MusData.CurPat, a);
		/* Surprising jumps are for example as follows:
			31 C-4 01 .. B05 | - goes to somewhere
			32 D-4 01 .. .00 | - arrives from somewhere - surprise
		*/
	}

	for(;;)
	{
		int Bxx, Cxx, SBx,SEx, SBStart;

		if(!Row)SBNext=1;

		if(!a)
		{
            if(Ortog[MusData.CurOrd][Row])
			{
                if(Nesify)NESLoopStart(MusData.CurOrd, Row);
				break;
			}
            if(Nesify)NESHere(MusData.CurOrd, Row);

			if(Verbose)
                printf("Ord %02X Pat %02d Row %02d\r", MusData.CurOrd, MusData.CurPat, Row);
			if(Investigating)
			{
                for(a=0; a<64; a++)if(Ortog[MusData.CurOrd][a])break;
				if(a==64)
				{
/*                  printf("Surprise at order %02X (%02X%-40c\n",
                           MusData.CurOrd, Ords[MusData.CurOrd], ')');
*/					Surprise++;
				}
				a=0;
			}
            if(!Row||There[MusData.CurPat][Row])Join[MusData.CurPat][Row]=1;
            Ortog[MusData.CurOrd][Row]=There[MusData.CurPat][Row]=1;
		}

		Bxx=Cxx=-1;
		SBStart=SBNext;
		SBNext=0;
		SEx=SBx=0;

		for(;;)
		{
			int Command, InfoByte;
			char b = *pp++;

			if(!b)break;

			Command=InfoByte=0;

			if(b<32)continue; //Turha tavu

			if(b&32)
			{
				register int d, c=*pp++;
				if(c>=126 && c<254)NoteHighBitNotUsed=0;
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
				}	}
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
					if(!SBx)SBStart=1; else SBNext=1;
				}
				if((InfoByte>>4) == 0xE)SEx = InfoByte&15;
			}
		}

		/* a on t„ss„ skippicountteri Bxx+Cxx -hommia varten. */
		if(a)
			a--;
		else
		{
			if(SBStart)
			{
                Join[MusData.CurPat][Row]=1;
				NESRemember();
			}
			if(Nesify)NESOut(SEx+1);
			if(SBx&&Nesify)NESRepeat(SBx);
			if(Bxx>=0)
			{
                MusData.CurOrd=Bxx;
				if(Cxx>=0)a=(Cxx&15) + 10*(Cxx>>4);
				free(TmpPat);
				goto DetPat;
			}
			if(Cxx>=0)
			{
                MusData.CurOrd++;
				a=(Cxx&15) + 10*(Cxx>>4);
				free(TmpPat);
				goto DetPat;
			}
		}
		// FIXME: Verify? Pattern row count
		if(++Row>=64)
		{
            MusData.CurOrd++;
			free(TmpPat);
			goto DetPat;
		}
	}
	free(TmpPat);

DonePatDet:
	if(!Hdr.OrdNum && !nopat)
	{
		printf("intgen: Fatal: File does not contain any orders.\nUse -p to force (writes not patterns).\n");
		exit(1);
	}

	for(a=0; a<Hdr.OrdNum-1; a++)
		if(Ords[a]==254 && Ords[a+1]<254) //The what if -case
		{
			for(i=0; i<64; i++)if(Ortog[a+1][i])break;
			if(i==64 && !Nesify)	//Very ortog case.
			{
				// Investigating a possible music archive.
                MusData.CurOrd=a+1;
				Investigating++;
				a=0;
				goto DetPat;
			}
		}

	if(Verbose && Investigating)
	{
		printf("Needed to re-investigate the file ");
		printf(Investigating>1?"%d times.\n":"once.\n", Investigating);
		if(Surprise)
		{
			if(!Verify)
			{
				printf("Suppose the extra investigations weren't done, ");
				printf(Surprise>1?"%d orders":"one order", Surprise);
				printf(" would've been cleaned.\n");
			}
		}
		else
			printf("For a surprise, nothing extra was found anyway.\n");
	}

	//Rescan orders to determine which ones were not visited
	for(A=a=0; a<Hdr.OrdNum; )
	{
		OrdCvt[A]=a;
		for(i=0; i<64; i++)if(Ortog[A][i])break;
		if(i==64 || Ords[a]==254)
		{
			for(i=a; i<Hdr.OrdNum; i++)Ords[i]=Ords[i+1];
			Hdr.OrdNum--; //One byte less left
		}
		else
			a++; //Next generatable order

		A++; //Next handled order
	}

	if(Nesify)free(NESAt);

	/****** SORT THE INSTRUMENTS, REMOVE UNUSABLE ONES ******/
	memset(InstConv, 255, sizeof InstConv);
	memset(NotUsed, 0, sizeof NotUsed);
	for(A=a=0; a<Hdr.InsNum; a++)
		if(Instr[a+1]->Typi==2)
		{
			if(InstUsed[a+1] || nopat)
			{
				InstCon2[A] = a+1;
				InstConv[a+1] = A++;
			}
			else
				NotUsed[a+1]=1;
		}

	if(Verbose && !Nesify)
	{
		int i=0;
		for(a=1; a<=Hdr.InsNum; a++)
			if(NotUsed[a])
				i++;

		if(i==1)
		{
			printf("Instrument ");
			for(a=1; a<=Hdr.InsNum; a++)
				if(NotUsed[a])
					printf("%d was never used, though it was usable.\n", a);
		}
		else if(i)
		{
			char Scales[MAXINST][8];
			int ScaleCount;

			if(i>32)
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

	SkipSmp += Hdr.InsNum-A;

	/****** WRITE THE HEADER AFTER WE HAVE THE USABLE INSTRUMENT COUNT ******/
	Hdr.InsNum = A;

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
			else if(Hdr.Channels[a]<25)printf("A%c ", Hdr.Channels[a]+33);
			else putchar('.');
		}
		if(Hdr.Channels[a]&16 && Hdr.Channels[a]<32)
			NotUsed[Hdr.Channels[a]&15]=0;
	}
	if(Verify)printf("\n");

	for(a=0; a<32; a++)
	{
		if(Hdr.Channels[a]<16)
			for(i=0; i<16; i++)
				if(NotUsed[i])
				{
					Hdr.Channels[a]=i|16;
					NotUsed[i]=0;
					break;
				}
	}
    memset(ChanConv, 255, sizeof ChanConv);
	for(ChanCount=a=0; a<32; a++)
		if(Hdr.Channels[a] >= 16 && Hdr.Channels[a] < 16+MAXCHN)
			ChanConv[a] = ChanCount++;

	if(ChanCount > 16)
	{
		printf("intgen: ERROR: Channel count exceeds 16. 16 supported.\n");
		ChanCount=16;
	}
	if(!Verify && !Nesify)
	{
		/****** WRITE THE HEADER ******/
		if(Hdr.Name[0] && Hdr.Name[1]>2 && !noname)
			fwrite(&Hdr.Name, 1, strlen(Hdr.Name)+1, fo);
		else if(((char *)&Hdr.OrdNum)[1]>2)
		{
			fwrite("rb", 3, 1, fo); // n, >2, 0
			if(Verbose)
				printf("Warning: Order count is %d - unoptimal amount (>511).\n", Hdr.OrdNum);
		}

		Hdr.OrdNum |= (NoteHighBitNotUsed<<8);
		fwrite(&Hdr.OrdNum, 2, 1, fo);
		fwrite(&Hdr.InsNum, 2, 1, fo);
		fwrite(&Hdr.PatNum, 2, 1, fo);
		fwrite(&Hdr.InsNum, 2, 1, fo);
		fwrite(&Hdr.InSpeed, 1, 1, fo);
		fwrite(&Hdr.InTempo, 1, 1, fo);
		fwrite(&Ords, 1, (char)Hdr.OrdNum, fo);
	}

	/****** WRITE THE INSTRUMENTS ******/
	for(A=0; A<Hdr.InsNum; A++)
	{
        int Tag128;
		a = InstCon2[A];

        Tag128 = Instr[a]->Volume&128; /* Was converted SMP->AME? */

        Instr[a]->Volume = (Instr[a]->Volume&127) % 65;

		if(FMVolConv)
		{
			if(Tag128 && !MakeLinear)
				Instr[a]->C2Spd = 8363; //Standardize C2SPD
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

        Instr[a]->Volume |= Tag128;                     //Converted SMP->AME
		if(Instr[a]->DosName[2])Instr[a]->Volume |= 64; //Doublechannel mode

		if(!Verify && !Nesify)
		{
			fwrite(&Instr[a]->DosName[1], 1, 1, fo); //finetune
			fwrite(&Instr[a]->D,         11, 1, fo); //adlib data
			fwrite(&Instr[a]->Volume,     1, 1, fo); //default volume
			fwrite(&Instr[a]->C2Spd,      2, 1, fo); //c2spd (adlib only)
			fwrite(&Instr[a]->DosName[0], 1, 1, fo); //patch
	}	}

	if(Nesify)
	{
		for(a=0; a<Hdr.PatNum; a++)
            free(MusData.Patn[a].Ptr);
	}
	else
	{
		/****** READ, CONVERT AND WRITE THE PATTERNS ******/
		memset(Cut, 0, sizeof Cut);
		memset(Gain,0, sizeof Gain);

		for(a=0; a<Hdr.PatNum; a++)
		{
			char *p, *q;

			int Length;

			int Buffered;

			if(!PatnPtr[a])
                MusData.Patn[a].Len=0;
			else
			{
				fseek(f, StartPtr+((long)(PatnPtr[a])<<4), SEEK_SET);
                fread(&MusData.Patn[a].Len, 2, 1, f);
			}

            MusData.Patn[a].Ptr = (char *)malloc(((1+2+1+2)*32U+1)*64U); // Max alloc

            if(!MusData.Patn[a].Ptr)
			{
MemoryTrouble:	fprintf(stderr, "Memory allocation error, intgen halted\n");
				exit(errno);
			}

            if(MusData.Patn[a].Len==0)
			{
                MusData.Patn[a].Len = 64;
                memset(MusData.Patn[a].Ptr, 0, 64);
				/* Ugliful empty pattern!! */
			}
			else
			{
                MusData.Patn[a].Len -= 2;
                fread(MusData.Patn[a].Ptr, 1, MusData.Patn[a].Len, f);
			}

            p = MusData.Patn[a].Ptr; //Input
            q = MusData.Patn[a].Ptr; //Output (which will be shorter)

			#define FINISH Length++, *q++ = 0; // <-- Notice: This must be 0
			#define ADDIDLE Buffered++;
			#define FLUSH \
				if(Buffered) \
				{ \
					if(Buffered==1) FINISH \
					else if(Buffered<16) Length++, *q++ = Buffered-1; \
					else { *q++ = 15; *q++ = Buffered; Length += 2; } \
					Buffered = 0; \
				}

			/* Aina patternin alussa resetoidaan vanhat instrut */
			memset(OldInstru, 255, sizeof OldInstru);
			for(Length=Buffered=Row=0; Row<64; Row++)
			{
				for(;;)
				{
                    int Note=0;
                    int Instru=0; //gcc happy
                    int Volume=0; //gcc happy
                    int Command=0; //gcc happy
                    int InfoByte=0;

					char b = *p++;
					int w = There[a][Row];

					if(!b)break;
					if(b<32)continue; //Turha tavu

					if(ChanConv[b&31]==127)w=0; //Hyl„t„„n rivi.
					if((b&31) >= MAXCHN)w=0;    //Hyl„t„„n t„ll”inkin.

					b = (b &~31) | ChanConv[b&31];

					if(b&32)
					{
						Note   = *p++;
						Instru = *p++;
						if(Instru)
						{
							if((Instru=InstConv[Instru])==255)
							{
								//Hyl„t„„n instrumentti ja nuotti.
								b &= ~32;
							}
							Instru++;
						}
					}
					if(b&64)
					{
                        Volume = *p++ % 65;
						if(FMVolConv)
							Volume = (MakeLinear
								? GMVol[Volume]
								: FMVol[Volume]
								) >> 1;
						if(b&32)
							Volume = (int)Volume
									 * Instr[(int)InstCon2[Instru-1]]->ManualScale
									 / 100;
						Volume = Volume * 63 / 64; /* Convert volume */
                        /* May overflow because of manualscale, so ensure */
                        if(Volume > 63)Volume=63;
					}
					if(b&128)
					{
						Command = *p++;
						InfoByte= *p++;
						if(Command=='B'-64 && InfoByte!=OrdCvt[InfoByte])
						{
							if(Verbose && !Verify)
								printf("Pattern %d row %d: B%02X converted to ",
									a, Row, InfoByte);
							InfoByte=OrdCvt[InfoByte];
							if(Verbose && !Verify)
								printf("B%02X  \n", InfoByte);
						}
						if((Command=='H'-64 && !Hxx)
						 ||(Command=='Q'-64 && !Qxx))
							b &= ~ 128;
					}

					if(b<32)w=0;

					if(w) //Kelpaako
					{
						int PutInstru=0;
						b = InfoByte ? b|16 : b&~16;

						FLUSH

						Length++, *q++ = b;

						if((b&32) && (OldInstru[b&15] != Instru))PutInstru = 1;
						if(NoteHighBitNotUsed)
							if(PutInstru || Join[a][Row])Note |= 128;

						if(b&16)
							Length++, *q++=InfoByte;
						if(b&32)
						{
							Length++, *q++=Note;
							if(Note&128 || !NoteHighBitNotUsed)
								Length++, *q++=OldInstru[b&15]=Instru;
						}
						if(b&64)
						{
                            Length++, *q++=Volume; //Convert volume
	//						if(!(b&32))OldInstru[b&15] = 255;
						}
						if(b&128)
							Length++, *q++=Command;
					}
				}

				if(Row < 63)
				{
					for(i=Row+1; i<64; i++)if(There[a][i])break;
					if(i == 64) /* Patternin loppuosassa ei ollut ket„„n */
					{
						Cut[Row][a] = 1;
                        Gain[Row][a] = MusData.Patn[a].Len - (int)(p-MusData.Patn[a].Ptr);
						if(Buffered)
							printf("intgen Internal error - buffering is disworking\a\n");
						FINISH     //End of pattern
						break;
					}
				}
				ADDIDLE
			}
			FLUSH

			fwrite(&Length,                  2, 1, fo);
            fwrite(MusData.Patn[a].Ptr, Length, 1, fo);

            free(MusData.Patn[a].Ptr);
		}

		#undef FINISH
		#undef ADDIDLE
		#undef FLUSH

		if(Verbose)
			for(l=Row=0; Row<64; Row++)
			{
				long A=0;
				for(i=a=0; a<Hdr.PatNum; a++)
					if(Cut[Row][a])
					{
						i++;
						A += Gain[Row][a];
					}
				if(!i)continue;
				if(i==1)
				{
					printf("Pattern ");
					for(a=0; a<MAXPATN; a++)
						if(Cut[Row][a])
							printf("%d", a);
				}
				else
				{
					char Scales[MAXPATN][8];
					int ScaleCount;
					printf("Patterns ");

					for(ScaleCount=a=0; a<Hdr.PatNum; a++)
						if(Cut[Row][a])
						{
							for(i=0; (a+i+1<Hdr.PatNum)&&Cut[Row][a+i+1]; i++);
							if(i)
							{
								sprintf(Scales[ScaleCount++], "%d-%d", a, a+i);
								a += i;
							}
							else
								sprintf(Scales[ScaleCount++], "%d", a);
						}

					i=ScaleCount;
					for(a=0; a<ScaleCount; a++)
					{
						printf("%s", Scales[a]);
						printf(--i==1?" and ":i?", ":"");
					}
				}

				if(Verify)
				{
					if(Row==62)
						printf(" had one empty row at end...\n");
					else
						printf(" had %d empty rows at end...\n", 63-Row);
				}
				else
				{
					printf(" %sat %s%d - gained %ld byte%s.\n",
						l<2?"truncated ":"", l?"":"row ",
						Row, A, A==1?"\b\b\b\b\b\bone byte":"s");
				}
				l++;
			}
	}
	fclose(f);

	/****** SAFELY CAN UNALLOCATE THE INSTRUMENT MEMORY ******/
	for(a=0; a<=MAXINST; a++)free(Instr[a]);

	if(Verify)
	{
		printf("Ok.\n");
		return;
	}

	if(*DestFile=='*' || *DestFile=='$' || (Name && (*Name =='*' || *Name =='$')))
	{
		if(Verbose)
			printf("We have now the temporary file nesmusa.$$$ containing the %s data.\n",
				Nesify
					?"selfplayer"
					:"internal");
		if(InternalFB)
		{
			Nesify=1;
			InternalFB=2;
			goto FBSkip2;
		}
	FBSkip3:
		fclose(fo);
		return;
	}
	
	if(0)	/* This is a dirty goto trick -- I know I shouldn't do this */
FBSkip:{	/* But it works, and that is the most important. // Bisqwit */
		long Size, Size1;
		fo = fopen("nesmusa.$$$", "rb");
		if(!fo)
		{
			printf("Error %d (%s) opening output file nesmusa.$$$.\n",
				errno, sys_errlist[errno]);
			return;
		}
		fread(&NESLoop,  1, sizeof(NESLoop), fo);
		fread(&Hdr,      1, sizeof(Hdr),     fo);
		Size1 = ftell(fo);
		
		fseek(fo, 0, SEEK_END);
		Size = ftell(fo) - Size1;
		
		f = fopen("intgen.$$$", "wb+");
		fseek(fo, Size1, SEEK_SET);
		while(Size>0)
		{
			int b = sizeof(Gain); /* Transfer buffer */
			if((long)b > Size)b = (int)Size;
			fread(&Gain, 1, b, fo);
			fwrite(&Gain, 1, b, f);
			Size -= b;
		}		
		fclose(fo);
		fo = f;
	}
	
	sprintf(Buf2, "%s%s", DestFile, Ext);
	if(Verbose)printf("Writing from temporary file to %s\n", Buf2);

	f = fopen(Buf2, "wt");

FBSkip2:
	if(Nesify)
	{
		long Newpos = ftell(fo);
		long RowCount = Newpos/8;
		long RowPos;
		long CPos;
		int Max=4;    /* Channel count */

		unsigned short *Laulu[4];

		int Desperate=0;
		
		for(a=0; a<Max; a++)
		{
			Laulu[a] = (unsigned short *)malloc((int)(RowCount*2));
			if(!Laulu[a])goto MemoryTrouble;
		}
		
		if(InternalFB==1)
		{
			/* This might be a bit faster */
			rewind(fo);
			for(a=0; a<Max; a++)fread(Laulu[a], 2, RowCount, fo);
		}
		else
		{
			if(Verbose)printf("Reordering bytes...\n");
	
			/* This first reorders the bytes */
			for(a=0; a<Max; a++)
				for(RowPos=0; RowPos<RowCount; RowPos++)
				{
					byte N, V;
					fseek(fo, a+RowPos*8, SEEK_SET);
					N = fgetc(fo);
					fseek(fo, 3, SEEK_CUR);
					V = fgetc(fo);
					if(!N)N=1;
					Laulu[a][(int)RowPos] = (((unsigned short)V)<<8)|N;
		}		}
			
		if(InternalFB==2)
		{
			rewind(fo);
			fwrite(&NESLoop,  1, sizeof(NESLoop), fo);
			fwrite(&Hdr,      1, sizeof(Hdr),     fo);
			for(a=0; a<Max; a++)fwrite(Laulu[a], 2, RowCount, fo);
			goto FBSkip3;
		}

		if(RowCount > 32000U)
		{
			printf("intgen: Warning: Excess rows %ld - %u is max\n",
				RowCount, 32000U);
			RowCount = 32000U;
		}

Retry:	fseek(fo, CPos=0, SEEK_SET);

		if(Verbose&&NESCompress&&!Desperate)printf("Compressing...\n");
		
		{	/* Stream-osuus alkaa (sisentämättömänä, pah) */
		
		const int MaxLen = (1<<NESLength) - 1;
		int Bits=0;  /* Helpers for the Stream macro  */
		int bk=1;    /* Bits in buffer */
		int y;       /* Currently compressing channel */
		
		#define StreamPut fputc(NESXOR^((NESRoll+(Bits%NESCount))&255),fo)
		#define Stream(x, bits) \
		{	Bits = Bits + (x)*bk; \
			bk <<= (bits); \
			while(bk >= NESCount) {	StreamPut;Bits/=NESCount,bk/=NESCount; } \
		}
		#define StreamFlush() if(bk)StreamPut
			
		for(y=Max; --y>=0; )
		{
			register int w;		/* Currently compressing row */
			for(w=0; w<RowCount; w++)
			{
				int BLen=2;		/* Best found length            */
				short BPos=0;	/* Relative position /gcc happy */

				if(NESCompress<256)
				{
					register int d, b;	/* Helper position counters */
					register int a = (int)(w - ((1<<NESDepf) - 1));
					if(a<0)a=0;

					for(d=a; d<w; d++)
						if(Laulu[y][(int)d] == Laulu[y][(int)w])
						{
							register int Len=1;	/* Current length */
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
				}		}	}
				/* Previously this was BLen>3, but since there's a possibility of failure   *
				 * in the else-part (about invalid NESCompress value), this is better idea. *
				 * This means that for Nesify==1, BLen>=3 (>2) and for Nesify==2, Blen>3    */
				if(BLen > 1+Nesify)
				{
					w += BLen-1;
					
					if(Nesify==2)
					{
						Stream(NESCompress, 7);
						Stream(BPos /* ^((1<<NESDepf  )-1) */ , NESDepf);
						Stream(BLen /* ^((1<<NESLength)-1) */ , NESLength);
					}
					else
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
		}	}	}	}
		
		if(Nesify==2)
			StreamFlush();
		
		#undef StreamFlush
		#undef StreamPut
		#undef Stream
		}	/* Stream-osuus päättyi */

		Newpos = ftell(fo)-CPos; /* Length of compressed data */

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

		if(NESInterlace && Nesify!=2)
		{
			char *Tmp;
			long Pv, w;
			
			if(Verbose)printf("Interlacing...\n");

			Tmp = (char *)malloc((size_t)Newpos);
			if(!Tmp)goto MemoryTrouble;

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
				fputc(Tmp[(unsigned short)Pos], fo);
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
				int y;
				static int Fla;
        		fseek(fo, CPos, SEEK_SET);
        		#if SC_MAKES_ONLY_DATA
        		fprintf(f, "/* size=%5ld,rows=%ld,loop=%ld,xor=%3d,compr=%3d,depf=%2d,len=%2d,count=%2d,roll=%3d */\n",
        			Newpos,RowCount,NESLoop,NESXOR,NESCompress,NESDepf,NESLength,NESCount,NESRoll);
				if(MFName[0])fprintf(f, "/* %s */\n", MFName);
        		#else
        		fprintf(f,
        			"#define _POSIX_SOURCE 1 /* Song length: %ld seconds (%ld seconds of loop). */\n",
        			RowCount * 5 / Hdr.InTempo / 2,
        			(RowCount-NESLoop) * 5 / Hdr.InTempo / 2);     			
				if(MFName[0])fprintf(f, "/* %s */\n", MFName);
        		fprintf(f,
        			"static char*Title=\"%splayer for /dev/audio (8-bit, µ-law)\\n\"\n"
        			"\"This file has been compiled from a file created with intgen.\";\n"
        			"typedef unsigned char by;char*A,%s*tmp=\"nesmusa.$$$\",*a0;\n",
        			NESFullFeatured?"NES-S3M ":"Self",
        			NESFullFeatured?"*In,":"");
        		#endif        		
        		a=fprintf(f, "static by*z=(by*)\"");
        		for(y=Fla=0; Newpos>0; )
        		{
        			static int Fla;
        			char Buf[4];
        			byte b = fgetc(fo);
        			#define QUOTABLE(b) ((b>=32 && b<127)&&!Fla)
        			if(b=='\\')
        				strcpy(Buf, "\\\\");
					else if(b=='\"')
        				strcpy(Buf, "\\\"");
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
        		#if SC_MAKES_ONLY_DATA
        		fprintf(f, "\";\n");
        		#else
        		fprintf(f, "\",\n"
        			"%ss[5][65536],l[299999],%ssn[32]=\"%s\";\n"
        			"#include <stdio.h>\n#include <unistd.h>\n"
					"static int %s,h,OF,PN,d,p=1,r,k,i,a,c,CO,j,x,T=%d,f=8000,LS=0,\nP[12],"
					"H[99],IL[99];FILE*fo%s\n"
					"static int Out(int v){int e=0,s=1,f=0;if(OF)v=128+v/30;else for((s=(v<0))?v=-v\n"
					":0;e<8;e++)if((f=((v+32)>>(e+1))-16)<16)break;return fputc(OF?v:255^((s<<7)|(e\n"
					"<<4)|f),fo);}void gn(void){for(f=0;A[1]>='0'&&A[1]<='9';f=f*10+*++A-'0');}int\n"
					"G(int n){while((k=1<<n)>p)d+=p*",
					NESFullFeatured?"o[256],w[256][64],NN[8],":"",
					NESFullFeatured?"\n":"",
					Hdr.Name,
					NESFullFeatured
						?	"ss,m,IN,IV[99],IP[256],PP[100],b,M,I,D[99],"
							"Bxx,Cxx,B,Ss,CP,\nAt[256][64],Np=0,S=SEEK_SET"
						:	"S",
					Hdr.InTempo*2,
					NESFullFeatured
						?	",*fi=stdin;\n#define rw(a) a=fgetc(fi),a|=fgetc(fi)<<8"
						:	";");
						
				if(NESXOR||NESRoll)fprintf(f, "(by)");
				if(NESRoll)fprintf(f, "(");
				if(NESXOR)fprintf(f, "(");
				fprintf(f, "*z++");
				if(NESXOR)fprintf(f, "^%d)", NESXOR);
				if(NESRoll)fprintf(f, "%+d)", NESXOR?(signed char)(-NESRoll):-NESRoll);	/* Avoid +++ */
				fprintf(f,
					",p*=%d;PN=d&(k-1);d/=k,p%s/=k;%sreturn PN;}",
					NESCount,
					NESFullFeatured?"":"\n",
					NESFullFeatured?"\n":"");
				fprintf(f, 
					"int main(int C,char**V){%s"
						"if(V)a0=*V;for(P[CO=%s]=907;++i<12;P[i\n]=P[i-1]*89/84);",
							NESFullFeatured?"\n":"",
							NESFullFeatured
							?	"a=i=C?0:(fseek(fo,0,SEEK_END),r=ftell(fo)/8,rewind(fo),\n"
								"fread(l,8,r,fo),fclose(fo),remove(tmp),z=(by*)In,main(-1,0))"
							:	"i=0");
							
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
						"for(i=IL[0]=IL[1]=32;--i>=0;s[0][i]=(i&24)?170:20)s[1][i]=i<16?i*16:(31-i)*16;\n"
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
				#endif
				
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
        					NESDepf);
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
        			(unsigned)RowCount
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
        			(unsigned)RowCount,
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
        		{
        			int y, Len;
        			for(y=a=Len=0; Newpos>0; Len=1)
    	    		{
	        			int Y;
        				byte b;
        				char Buf[4];
        				b = fgetc(fo);
	        			#define QUOTABLE(b) (((b>=32&&b<127)||(b>=128&&b<255))&&b!='\'')
        				if(QUOTABLE(b))
        				{
        					if(!y)
    	    				{
	        					byte n = fgetc(fo);
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
        		}
        		fprintf(f,
        			"\n\n"
        			"DC\tdd 0\n"				/* Delaycount        	  */
        			"D55\tdd 55\n"				/* Constant: 55L     	  */
        			"S40\tdw 40h\n"		        /* Constant: Seg0040      */
        			"L\tdd 0,0,0,0\n"           /* Song pointers, 4*dword */
        			"CT\tdb '386 not found.',13,10,0\n"
        			/*   D[  0, 1,  2,3,   8,9,10 ] */
        			"S1\tdb 33, 1, 14,8,   3,0,2\n"
        			"S2\tdb 34,33,192,192, 0,0,5\n"
        			"S3\tdb 15, 0,  0,0,   0,1,14\n"
        			"P\tdw "); /* periods: */

        		{
        			int y;
	        		for(y=0; y<12; y++)
    	    			fprintf(f, "%ld%s",Period[y]*10000/22050,y<11?",":"");
    	    	}

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
                    (unsigned)((RowCount*2+255) >> 4)
                    /* 255 instead of 15 = leave space for bugs... */
        		);

        		/* Write OB() */
        		fprintf(f,	/* al=index, ah=data */
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
                    "push offset S[9]\n" /* Hyp„t„„n kahden sanan yli */
                    "S:"
        			"mov bx,[bp]\n"
                    "cmp byte ptr[bx-1],0\n"
                    "je @q\n" /* T„n t„ytyy pysy„ shorttina, huom */
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
 			break;	/* case Nesify==1 */
 		}
	}
	else
	{
		if(MFName[0])fprintf(f, "/* %s */\n", MFName);
		fprintf(f,
			"#ifdef __DJ_size_t\n"
			"#define _SIZE_T\n"
			"#endif\n"
			"#ifdef _SIZE_T\t\t// Kind of bad test, but... :)\n"
			"extern \n"
			"#endif\n"
			"#ifdef __GNUC__\n"
			"char %s[]\n"
			"#else\n"
			"char far %s[]\n"
			"#endif\n"
			"#ifndef _SIZE_T\n"
			" =\n{\n", Name, Name);

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
	
	if(InternalFB)
		remove("intgen.$$$");

	fclose(f);
}

#endif // UnderFMDRV

/* Patches */
unsigned char adl[] =
{
	14,0,0,1,143,242,244,0,8,1,6,242,247,0,14,0,0,1,75,242,244,0,8,1,0,242,247,0,14,0,0,1,73,242,244,0,8,1,0,242,246,0,14,0,0,129,18,242,247,0,6,65,0,242,247,0,14,0,0,1,87,241,247,0,0,1,0,242,247,0,14,0,0,1,147,241,247,0,0,1,0,242,247,0,14,0,0,1,128,161,
	242,0,8,22,14,242,245,0,14,0,0,1,146,194,248,0,10,1,0,194,248,0,14,0,0,12,92,246,244,0,0,129,0,243,245,0,14,0,0,7,151,243,242,0,2,17,128,242,241,0,14,0,0,23,33,84,244,0,2,1,0,244,244,0,14,0,0,152,98,243,246,0,0,129,0,242,246,0,14,0,0,24,35,246,246,
	0,0,1,0,231,247,0,14,0,0,21,145,246,246,0,4,1,0,246,246,0,14,0,0,69,89,211,243,0,12,129,128,163,243,0,14,0,0,3,73,117,245,1,4,129,128,181,245,0,14,0,0,113,146,246,20,0,2,49,0,241,7,0,14,0,0,114,20,199,88,0,2,48,0,199,8,0,14,0,0,112,68,170,24,0,4,177,
	0,138,8,0,14,0,0,35,147,151,35,1,4,177,0,85,20,0,14,0,0,97,19,151,4,1,0,177,128,85,4,0,14,0,0,36,72,152,42,1,12,177,0,70,26,0,14,0,0,97,19,145,6,1,10,33,0,97,7,0,14,0,0,33,19,113,6,0,6,161,137,97,7,0,14,0,0,2,156,243,148,1,12,65,128,243,200,0,14,0,
	0,3,84,243,154,1,12,17,0,241,231,0,14,0,0,35,95,241,58,0,0,33,0,242,248,0,14,0,0,3,135,246,34,1,6,33,128,243,248,0,14,0,0,3,71,249,84,0,0,33,0,246,58,0,14,0,0,35,74,145,65,1,8,33,5,132,25,0,14,0,0,35,74,149,25,1,8,33,0,148,25,0,14,0,0,9,161,32,79,
	0,8,132,128,209,248,0,14,0,0,33,30,148,6,0,2,162,0,195,166,0,14,0,0,49,18,241,40,0,10,49,0,241,24,0,14,0,0,49,141,241,232,0,10,49,0,241,120,0,14,0,0,49,91,81,40,0,12,50,0,113,72,0,14,0,0,1,139,161,154,0,8,33,64,242,223,0,14,0,0,33,139,162,22,0,8,33,
	8,161,223,0,14,0,0,49,139,244,232,0,10,49,0,241,120,0,14,0,0,49,18,241,40,0,10,49,0,241,24,0,14,0,0,49,21,221,19,1,8,33,0,86,38,0,14,0,0,49,22,221,19,1,8,33,0,102,6,0,14,0,0,113,73,209,28,1,8,49,0,97,12,0,14,0,0,33,77,113,18,1,2,35,128,114,6,0,14,
	0,0,241,64,241,33,1,2,225,0,111,22,0,14,0,0,2,26,245,117,1,0,1,128,133,53,0,14,0,0,2,29,245,117,1,0,1,128,243,244,0,14,0,0,16,65,245,5,1,2,17,0,242,195,0,14,0,0,33,155,177,37,1,14,162,1,114,8,0,14,0,0,161,152,127,3,1,0,33,0,63,7,1,14,0,0,161,147,193,
	18,0,10,97,0,79,5,0,14,0,0,33,24,193,34,0,12,97,0,79,5,0,14,0,0,49,91,244,21,0,0,114,131,138,5,0,14,0,0,161,144,116,57,0,0,97,0,113,103,0,14,0,0,113,87,84,5,0,12,114,0,122,5,0,14,0,0,144,0,84,99,0,8,65,0,165,69,0,14,0,0,33,146,133,23,0,12,33,1,143,
	9,0,14,0,0,33,148,117,23,0,12,33,5,143,9,0,14,0,0,33,148,118,21,0,12,97,0,130,55,0,14,0,0,49,67,158,23,1,2,33,0,98,44,1,14,0,0,33,155,97,106,0,2,33,0,127,10,0,14,0,0,97,138,117,31,0,8,34,6,116,15,0,14,0,0,161,134,114,85,1,0,33,131,113,24,0,14,0,0,
	33,77,84,60,0,8,33,0,166,28,0,14,0,0,49,143,147,2,1,8,97,0,114,11,0,14,0,0,49,142,147,3,1,8,97,0,114,9,0,14,0,0,49,145,147,3,1,10,97,0,130,9,0,14,0,0,49,142,147,15,1,10,97,0,114,15,0,14,0,0,33,75,170,22,1,8,33,0,143,10,0,14,0,0,49,144,126,23,1,6,33,
	0,139,12,1,14,0,0,49,129,117,25,1,0,50,0,97,25,0,14,0,0,50,144,155,33,0,4,33,0,114,23,0,14,0,0,225,31,133,95,0,0,225,0,101,26,0,14,0,0,225,70,136,95,0,0,225,0,101,26,0,14,0,0,161,156,117,31,0,2,33,0,117,10,0,14,0,0,49,139,132,88,0,0,33,0,101,26,0,
	14,0,0,225,76,102,86,0,0,161,0,101,38,0,14,0,0,98,203,118,70,0,0,161,0,85,54,0,14,0,0,98,153,87,7,0,11,161,0,86,7,0,14,0,0,98,147,119,7,0,11,161,0,118,7,0,14,0,0,34,89,255,3,2,0,33,0,255,15,0,14,0,0,33,14,255,15,1,0,33,0,255,15,1,14,0,0,34,70,134,
	85,0,0,33,128,100,24,0,14,0,0,33,69,102,18,0,0,161,0,150,10,0,14,0,0,33,139,146,42,1,0,34,0,145,42,0,14,0,0,162,158,223,5,0,2,97,64,111,7,0,14,0,0,32,26,239,1,0,0,96,0,143,6,2,14,0,0,33,143,241,41,0,10,33,128,244,9,0,14,0,0,119,165,83,148,0,2,161,
	0,160,5,0,14,0,0,97,31,168,17,0,10,177,128,37,3,0,14,0,0,97,23,145,52,0,12,97,0,85,22,0,14,0,0,113,93,84,1,0,0,114,0,106,3,0,14,0,0,33,151,33,67,0,8,162,0,66,53,0,14,0,0,161,28,161,119,1,0,33,0,49,71,1,14,0,0,33,137,17,51,0,10,97,3,66,37,0,14,0,0,
	161,21,17,71,1,0,33,0,207,7,0,14,0,0,58,206,248,246,0,2,81,0,134,2,0,14,0,0,33,21,33,35,1,0,33,0,65,19,0,14,0,0,6,91,116,149,0,0,1,0,165,114,0,14,0,0,34,146,177,129,0,12,97,131,242,38,0,14,0,0,65,77,241,81,1,0,66,0,242,245,0,14,0,0,97,148,17,81,1,
	6,163,128,17,19,0,14,0,0,97,140,17,49,0,6,161,128,29,3,0,14,0,0,164,76,243,115,1,4,97,0,129,35,0,14,0,0,2,133,210,83,0,0,7,3,242,246,1,14,0,0,17,12,163,17,1,0,19,128,162,229,0,14,0,0,17,6,246,65,1,4,17,0,242,230,2,14,0,0,147,145,212,50,0,8,145,0,235,
	17,1,14,0,0,4,79,250,86,0,12,1,0,194,5,0,14,0,0,33,73,124,32,0,6,34,0,111,12,1,14,0,0,49,133,221,51,1,10,33,0,86,22,0,14,0,0,32,4,218,5,2,6,33,129,143,11,0,14,0,0,5,106,241,229,0,6,3,128,195,229,0,14,0,0,7,21,236,38,0,10,2,0,248,22,0,14,0,0,5,157,
	103,53,0,8,1,0,223,5,0,14,0,0,24,150,250,40,0,10,18,0,248,229,0,14,0,0,16,134,168,7,0,6,0,3,250,3,0,14,0,0,17,65,248,71,2,4,16,3,243,3,0,14,0,0,1,142,241,6,2,14,16,0,243,2,0,14,0,0,14,0,31,0,0,14,192,0,31,255,3,14,0,0,6,128,248,36,0,14,3,136,86,132,
	2,14,0,0,14,0,248,0,0,14,208,5,52,4,3,14,0,0,14,0,246,0,0,14,192,0,31,2,3,14,0,0,213,149,55,163,0,0,218,64,86,55,0,14,0,0,53,92,178,97,2,10,20,8,244,21,0,14,0,0,14,0,246,0,0,14,208,0,79,245,3,14,0,0,38,0,255,1,0,14,228,0,18,22,1,14,0,0,0,0,243,240,
	0,14,0,0,246,201,2,14,0,35,16,68,248,119,2,8,17,0,243,6,0,14,0,35,16,68,248,119,2,8,17,0,243,6,0,14,0,52,2,7,249,255,0,8,17,0,248,255,0,14,0,48,0,0,252,5,2,14,0,0,250,23,0,14,0,58,0,2,255,7,0,0,1,0,255,8,0,14,0,60,0,0,252,5,2,14,0,0,250,23,0,14,0,
	47,0,0,246,12,0,4,0,0,246,6,0,14,0,43,12,0,246,8,0,10,18,0,251,71,2,14,0,49,0,0,246,12,0,4,0,0,246,6,0,14,0,43,12,0,246,8,0,10,18,5,123,71,2,14,0,51,0,0,246,12,0,4,0,0,246,6,0,14,0,43,12,0,246,2,0,10,18,0,203,67,2,14,0,54,0,0,246,12,0,4,0,0,246,6,
	0,14,0,57,0,0,246,12,0,4,0,0,246,6,0,14,0,72,14,0,246,0,0,14,208,0,159,2,3,14,0,60,0,0,246,12,0,4,0,0,246,6,0,14,0,76,14,8,248,66,0,14,7,74,244,228,3,14,0,84,14,0,245,48,0,14,208,10,159,2,0,14,0,36,14,10,228,228,3,6,7,93,245,229,1,14,0,65,2,3,180,
	4,0,14,5,10,151,247,0,14,0,84,78,0,246,0,0,14,158,0,159,2,3,14,0,83,17,69,248,55,2,8,16,8,243,5,0,14,0,84,14,0,246,0,0,14,208,0,159,2,3,14,0,24,128,0,255,3,3,12,16,13,255,20,0,14,0,77,14,8,248,66,0,14,7,74,244,228,3,14,0,60,6,11,245,12,0,6,2,0,245,
	8,0,14,0,65,1,0,250,191,0,7,2,0,200,151,0,14,0,59,1,81,250,135,0,6,1,0,250,183,0,14,0,51,1,84,250,141,0,6,2,0,248,184,0,14,0,45,1,89,250,136,0,6,2,0,248,182,0,14,0,71,1,0,249,10,3,14,0,0,250,6,0,14,0,60,0,128,249,137,3,14,0,0,246,108,0,14,0,58,3,128,
	248,136,3,15,12,8,246,182,0,14,0,53,3,133,248,136,3,15,12,0,246,182,0,14,0,64,14,64,118,79,0,14,0,8,119,24,2,14,0,71,14,64,200,73,0,14,3,0,155,105,2,14,0,61,215,220,173,5,3,14,199,0,141,5,0,14,0,61,215,220,168,4,3,14,199,0,136,4,0,14,0,44,128,0,246,
	6,3,14,17,0,103,23,3,14,0,40,128,0,245,5,2,14,17,9,70,22,3,14,0,69,6,63,0,244,0,1,21,0,247,245,0,14,0,68,6,63,0,244,3,0,18,0,247,245,0,14,0,63,6,63,0,244,0,1,18,0,247,245,0,14,0,74,1,88,103,231,0,0,2,0,117,7,0,14,0,60,65,69,248,72,0,0,66,8,117,5,0,
	14,0,80,10,64,224,240,3,8,30,78,255,5,0,14,0,64,10,124,224,240,3,8,30,82,255,2,0,14,0,72,14,64,122,74,0,14,0,8,123,27,2,14,0,73,14,10,228,228,3,6,7,64,85,57,1,14,0,70,5,5,249,50,3,14,4,64,214,165,0,14,0,68,2,63,0,243,3,8,21,0,247,245,0,14,0,48,1,79,
	250,141,0,7,2,0,248,181,0,14,0,53,0,0,246,12,0,4,0,0,246,6,0
};

unsigned char FMVol[65] =
{	 0, 16, 22, 27,  32, 35, 39, 42,
	45, 48, 50, 53,  55, 57, 59, 61,
	64, 65, 67, 69,  71, 73, 75, 76,
	78, 80, 81, 83,  84, 86, 87, 89,
	90, 91, 93, 94,  96, 97, 98, 99,
   101,102,103,104, 106,107,108,109,
   110,112,113,114, 115,116,117,118,
   119,120,121,122, 123,124,125,126,
   128
};
unsigned char NESvol[65] =
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

#ifndef UnderFMDRV

int SubMidIns(char far *a, char *fdat, char *m)
{
	extern void GM_Patch(int, byte);
	extern void GM_KeyOn(int, byte, byte);
	extern void GM_KeyOff(int);

	long size;
	int b, dev, devtold;
	FILE *fp;

	sscanf(m, "%d:%d:%d:%d", &Verbose, &b, &dev, &devtold);

    InitMusic(NO_AUTODETECT);

	if(dev==SelectOPL)
    {
		if(!devtold)
			dev=SelectMPU;
		else
		{
			printf("Sorry, GM/GP tests can not be done with OPL in this program.\n");
			return 1;
		}
    }

	if(Verbose>0)
	{
		printf("Device: ");
		fflush(stdout);
	}

    if(SelectMusDev(dev) < 0)
	{
		printf("Problem: %s\n", SoundError);
        return 2;
	}

    if(Verbose>0)
        printf("%s\n",
			dev==SelectOPL?"OPL":
#if SUPPORT_AWE
			dev==SelectAWE?"AWE":
#endif
			dev==SelectMPU?"MPU":
			dev==SelectDSP?"DSP":
			"Unknown");

    fp = fopen(fdat, "rt");
	if(fp)
	{
		while(!feof(fp))
		{
			char Buf[64];
			fgets(Buf, 63, fp);
			if(!strnicmp(Buf, a, strlen(a))
			&& Buf[strlen(a)]==':')
				printf("%s\n", Buf);
		}
		fclose(fp);
	}
	else
		if(Verbose>0)
            printf("Did not find %s\n", fdat);

	printf("(Raise Ù)"); fflush(stdout);
	GM_Patch(0, b);
	GM_Patch(1, b);
	GM_Patch(2, b);
	GM_Patch(3, b);
	GM_KeyOn(3, 54, 127);
	for(b=0; ; b++)
	{
		for(size=0; size<10; size++)
			if(inportb(0x60)==0x9C)goto Key;

		if(b==4000)GM_KeyOn(1, 51, 127);
		if(b==8000)GM_KeyOn(2, 47, 127);
		if(b==12000)
			GM_KeyOn(0, 35, 127);
		if(b==16000)
		{
			GM_KeyOn(0, 39, 127);
			GM_KeyOff(1);
			GM_KeyOff(2);
		}
		if(b==20000)GM_KeyOn(0, 42, 127);
		if(b>20000)b--;

		if(kbhit())getch();
	}

Key:GM_KeyOff(0);
	GM_KeyOff(1);
	GM_KeyOff(2);
	GM_KeyOff(3);

    ExitMusic();

	while(kbhit())getch();

	printf("\n");
	if(Verbose>0)puts("\nDone");

	return 0;
}

int SubMidIns2(char *a, int Verbose)
{
	char m[512];
	FILE *fp, *fp2;
	int ark;

	if(access(a, 4))
	{
		printf("File not found (%s).\n", a);
		return 1;
	}

	fp2 = fopen("nesmusa.$$$", "wb");
	if(!fp2)
	{
		printf("Can't open %s for reading (error %d (%s))\n",
				"nesmusa.$$$", errno, sys_errlist[errno]);
		return 1;
	}

	fp = fopen(a, "rb");
	if(!fp)
	{
		printf("Can't open %s for reading (error %d (%s))\n",
				a, errno, sys_errlist[errno]);
		fclose(fp2);
		return 1;
	}

	for(ark=0; !feof(fp); )
	{
		fgets(m, 511, fp);
		fp=fp;
		if(!strcmp(m, "#ifndef _SIZE_T\r\n"))
		{
			int a;
			char *EndPtr;

			if(Verbose)printf("Found marker. Creating nesmusa.$$$...\n");
			ark++;

			for(;!feof(fp);)
			{
				fgets(m, 511, fp);
				if(m[0]=='}')break;
				if(m[0]=='\t')
				{
					for(EndPtr=m;;)
					{
						a = (int)strtol(EndPtr, &EndPtr, 10);
						EndPtr++;             // Skip comma ','
						if(*EndPtr == '/')break;
						putc(a, fp2);
						if(*EndPtr<'0')break; // Ends at \r, \n and /**/
					}
				}
			}
		}
	}
	fclose(fp);
	fclose(fp2);
	return !ark;
}

void SubMidIns0(void)
{
	printf(
		"*** Troubleshooting information\n"
		"***\n"
		"*** This program may crash. If it does, Bisqwit is in no way\n"
		"*** responsible of any damage the program may do to anything.\n"
		"*** You are free to use or not to use this program.\n"
		"***\n"
		"*** If the program crashes, you may tell the crash\n"
		"*** information to Bisqwit. Three crash output cases:\n"
		"***\n"
		"***    Device: (no linefeed)             Device detection crashed\n"
		"***    Device: something (no linefeed)   Sound file loading crashed\n"
		"***    Device: something (linefeed)      Player crashed.\n"
		"***\n"
		"*** Send the file you was trying to play, the command line you\n"
		"*** invoked this program with and tell what is your soundcard.\n"
		"*** The e-mail address is bisqwit@nettipaja.fi.\n"
	);
}

static int Heelp=0;
static int Version=0;

static void V(char **s)
{
	Verbose++;
	s=s;
}
static void B(char **s)
{
	Batch=1;
	s=s;
}
static void He(char **s)
{
	Heelp=1;
	s=s;
}
static void H(char **s)
{
	Hxx=0;
	s=s;
}
static void N(char **s)
{
	noname=1;
	s=s;
}
static void Q(char **s)
{
	Qxx=0;
	s=s;
}
static void R(char **s)
{
	Verify=1;
	s=s;
}
static void P(char **s)
{
	nopat=1;
	s=s;
}
static void O(char **s)
{
	NoteHighBitNotUsed=0;
	s=s;
}
static void Ve(char **s)
{
	Version=1;
	s=s;
}
static void L(char **s)
{
    MakeLinear=1;
	s=s;
}
static void SA(char **s)
{
	Nesify=1;
	s=s;
}
static void Sc(char **s)
{
	NESFullFeatured=0;
	Nesify=2;
	s=s;
}
static void SC(char **s)
{
	NESFullFeatured=1;
	Nesify=2;
	s=s;
}
static void NI(char **s)
{
	NESInterlace=0;
	s=s;
}
static void FB(char **s)
{
	InternalFB=1;
	s=s;
}
static void NC(char **s)
{
	long tmp = strtol(*s, s, 10);
	s--;
	if(tmp<0 || tmp>256)
	{
		printf("intgen: parameter value out of range `%ld'", tmp);
		exit(1);
	}
	NESCompress = (int)tmp;
}
static void NX(char **s)
{
	long tmp = strtol(*s, s, 10);
	s--;
	if(tmp<0 || tmp>255)
	{
		printf("intgen: parameter value out of range `%ld'", tmp);
		exit(1);
	}
	NESXOR = (int)tmp;
}
static void ND(char **s)
{
	long tmp = strtol(*s, s, 10);
	s--;
	if(tmp<2 || tmp>14)
	{
		printf("intgen: parameter value out of range `%ld'", tmp);
		exit(1);
	}
	NESDepf = (int)tmp;
	NESLength = 16-NESDepf;
}
static void NL(char **s)
{
	long tmp = strtol(*s, s, 10);
	s--;
	if(tmp<2 || tmp>10)
	{
		printf("intgen: parameter value out of range `%ld'", tmp);
		exit(1);
	}
	NESLength = (int)tmp;
}
static void NO(char **s)
{
	long tmp = strtol(*s, s, 10);
	s--;
	if(tmp<1 || tmp>256)
	{
		printf("intgen: parameter value out of range `%ld'", tmp);
		exit(1);
	}
	NESCount = (int)tmp;
}
static void NR(char **s)
{
	long tmp = strtol(*s, s, 10);
	s--;
	if(tmp<0 || tmp>255)
	{
		printf("intgen: parameter value out of range `%ld'", tmp);
		exit(1);
	}
	NESRoll = (int)tmp;
}
static struct
{
	void (*Func)(char **);
	char c, c2;
	char *txt;
	char *descr;
} Options[] =
{
	{V, 'v',  0,"verbose",   "Verbose"},
	{B, 'b',  0,"batch",     "Batch working mode"},
	{H, 'h',  0,"no-vibrato","Strip Hxx"},
	{Q, 'q',  0,"no-retrig", "Strip Qxx"},
    {L, 'l',  0,"linear",    "Make file volumes linear"},
	{R, 'r',  0,"verify",    "Verifies source .S3M, implies -v"},
	{N, 'n',  0,"noname",    "Disables song name from output file"},
	{O, 'o',  0,"nopt",      "Disables instrument number optimization"},
	{SA,'s','a',"nesa",      "Converts NES-S3M file into selfplaying .asm file (OPL)"},
	{Sc,'s','c',"nesc",		 "Converts NES-S3M file into selfplaying .c file (/dev/audio)"},
	{SC,'s','C',"nesC",      "Makes a full featured /dev/audio selfplayer (see intgen.txt)"},
	{FB,'f','b',"internal1", "Internally used by 'findbest' program"},
	{NI,'n','i',"nesil",     "Disables interlaced format (-sa)"},
	{NC,'n','c',"nesco",     "Sets the compression datavalue (-sa:0..256 -sc:0..127,256)"},
	{NO,'n','o',"nescount",  "Sets the character count (1..256) (-sc)"},
	{NR,'n','r',"nesroll",   "Sets the character roll (0..255) (-sc)"},
	{NX,'n','x',"nesxor",    "Sets the compression xorvalue (0..255) (-sa)"},
	{ND,'n','d',"nesdepf",   "Sets the compression depthbits (2..14) (-sa,-sc)"},
	{NL,'n','l',"neslength", "Sets the compression lengthbits (2..10) (-sc)"},
	{P, 'p',  0,"nopat",     "Writes no patterns."},
	{He,'?',  0,"help",      "Help"},
	{Ve,'V',  0,"version",   "Version information"},
	{NULL,0,0,NULL,NULL}
};
static void ArgError(int a, char *b)
{
	if(a)printf("intgen: unrecognized option `--%s'", b);
	else
	{
		printf("intgen: illegal option -- %c", *b);
		for(a=0; Options[a].c; a++)
			if(*b == Options[a].c)
			{
				putchar(b[1]);
				break;
			}
	}
	printf("\nTry `intgen --help' for more information.\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int a;

	char *ifn, *ofn, *vn;
	ifn=ofn=vn=NULL;

	if(argv[1][0]=='<')
		switch(argv[1][1])
		{
			case '0': {SubMidIns0();return 0;}
			case '1': exit(SubMidIns(argv[2], argv[3], argv[4]));
			case '2': exit(SubMidIns2(argv[2], atoi(argv[3])));
		}

	for(a=1; a<argc; a++)
	{
		char *s = argv[a];

		if(*s == '(')
		{
			char *b;
			for(s++, MFName[0]=0; ; s=argv[a])
			{
				char Tmp[128];
				strncpy(Tmp, s, (sizeof(Tmp))-1);
				b = strchr(Tmp, ')');
				if(b)*b=0;
				strcat(MFName, Tmp);
				if(b){s=strchr(s, ')')+1;break;}
				strcat(MFName, " ");
				if(++a >= argc)
				{
					printf("intgen: Unterminated description - ')' missing.\n");
					return 1;
				}
			}
    		/* #fb is a macro for Findbest to save command line space in dos */
			if(!strcmp(MFName, "#fb"))
				strcpy(MFName, 
					"This source was optimized with "
					"Findbest of the Bisqwit's Midtools pack");
		}

ReArg:	while(*s==' ' || *s=='\t')s++;
		if(!*s)continue;

		if(*s == '-' || (*s=='/'&&s[1]=='?'))
		{
			s++;
			if(*s == '-')
			{
				int b=0;
				for(s++; Options[b].c; b++)
					if(!strcmp(s, Options[b].txt))
					{
						s = argv[++a];
						Options[b].Func(&s);
						goto ReArg;
					}
				ArgError(1, s);
			}
			for(; s && *s; s++)
			{
				int b,c;
				for(b=c=0; Options[b].c; b++)
					if(s[0]==Options[b].c
					&&(s[1]==Options[b].c2
						  ||!Options[b].c2))
					{
						c=1;
						s++;
						if(Options[b].c2)s++;
						Options[b].Func(&s);
						s--;
					}
				if(!c)ArgError(0, s);
			}
		}
		else
		{
			if(!ifn)ifn=s;
			else if(!ofn)ofn=s;
			else if(!vn)vn=s;
			else
			{
				printf("intgen: too many parameters -- `%s'\n", s);
				return 1;
			}
		}
	}

	if(Verbose || Version || Heelp)
	{
        printf("intgen - S3M data conversion tool v2.87 (C) 1992,1999 Bisqwit\n");
		if(Version)return 0;
		if(!Batch)printf("\n");
	}

	if(Heelp)
	{
		printf(
            "Usage: intgen [-bovlhqrpnV] [\\(infotext\\)] [srcfile [destfile [pubname]]]\n"
			"\n"
			"Examples:\n"
			"  intgen -sa mman3_e2.s3m mman3_e2\n"
			"  intgen -b (Official WSpeed musicfile) \\mod\\tcsmods\\jhmadl52.s3m wspeed dah\n"
			"  intgen -vsCnd11nx0nc12nr40nl8 nes\\dtales1q.s3m dtales1q\n"
			"  intgen music.s3m musicinc music_table\n"
            "\n");

		for(a=0; Options[a].c; a++)
			printf("  -%c%c%c--%-14s%s\n",
				Options[a].c,
				Options[a].c2?Options[a].c2:',',
				Options[a].c2?',':' ',
				Options[a].txt,
				Options[a].descr);

		printf("\n"
			"Note: You must not supply extension for destfile.\n"
			"This program may be distributed under the terms of General Public License.\n"
			"See the file COPYING for more details. No warranty.\n"
			"This program is invoked and required by some other programs.\n");
		return 1;
	}

	if(!ifn)
	{
		ifn=(char *)malloc(512);
		if(!access("intgen.msg", 4))
		{
			FILE *fp = fopen("intgen.msg", "rt");
			for(;fgets(ifn,511,fp);)printf("** %s\r", ifn);
			fclose(fp);
		}
		printf("Input file name? "); scanf("%s", ifn);
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
			ofn=(char *)malloc(512);
			printf("Output file name? "); scanf("%s", ofn);
		}
		if(!vn && !Nesify)
		{
			vn=(char *)malloc(512);
			printf("Public variable name? "); scanf("%s", vn);
		}
		if(vn && Nesify)
			printf("Warning: Public variable name superfluous with --nes\n");
	}

	S3MtoInternal(ifn, ofn, vn);

	return 0;
}

#endif // UnderFMDRV
