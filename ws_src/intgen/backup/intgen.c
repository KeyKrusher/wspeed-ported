#pragma option -K	//Unsigned chars
#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "adlib.h"

int Batch=0, Verbose=0;

extern char adl[]; //my”hemp„n„
extern int FMVol[];//t„m„kin

extern void GM_Patch(int, int);     //From adlib.c
extern void GM_KeyOn(int, int, int);// -"-
extern void GM_KeyOff(int); 		// -"-

char ChanConv[32];			//Channel conversion table
char InstConv[MAXINST+1];	//Instrument conversion table
char InstCon2[MAXINST+1];
char There[MAXPATN][64];	//Visited rows
char Ortog[MAXORD][64];		//Visited rows per order
char Patns[MAXPATN];		//Pattern conversion table
char OrdCvt[MAXORD];
char Ords[MAXORD];
word InstPtr[MAXINST+1];
word PatnPtr[MAXPATN];
char InstUsed[MAXINST+1];	//Used instruments
char NotUsed[MAXINST+1];	//Not used instruments
							//Am I stupid or what... :)

char far Cut[64][MAXPATN]; //for beautiful output of pattern
int far Gain[64][MAXPATN]; //gut chains... I mean, cut gains...

int FMVolConv = 0; // Going to convert the volumes for adlib?
int noname = 0;    // Disable name output?

int Hxx=1, Qxx=1; // Process infobytes
int Verify=0;     // Verify only, no conversion

char MFName[64];

void S3MtoInternal(char *s, char *DestFile, char *Name)
{
	char *TmpPat, *pp; //Pattern analyzer variables
	char Note=0; //gcc happy
	char Instru=0; //gcc happy
	char Volume=0; //gcc happy
	char Command=0; //gcc happy
	char InfoByte=0; //gcc happy

	int Surprise, Investigating, Row, a, i, l, A, B, SkipSmp; //Other vars
	char ChanCount;
	char Buf2[64];
	long StartPtr;
	long InfSize;
	FILE *f, *fo;

	int crpending;

	S3MHdr Hdr;
	ADLSample *Instr[MAXINST+1];

	directvideo=0;

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
				printf(
					"Destination file:   %s.C\n"
					"Interface variable: char %s[]\n",
					DestFile, Name);
			printf(
				"Array data format:  InternalHdr + Ords + InternalSamples\n"
				"------------------------------------------------------------\n"
				"Creating temporary file using %s\n",
				s);
		}
	}
	else
	{
		printf("InFile: %s", s);
		if(DestFile[0]!='*' && DestFile[0]!='$' && !Verify)
			printf(" OutFile: %s.C VarName: %s[]", DestFile, Name);
		crpending = 1;
	}

	for(;;)
	{
		StartPtr = ftell(f);
		if(StartPtr)printf("at 0x%08X\n", StartPtr);
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

	if(Verify)
		printf("%u orders, %u instruments, %u patterns\n",
			Hdr.OrdNum, Hdr.InsNum, Hdr.PatNum);

	/****** ALLOCATE MEMORY FOR INSTRUMENTS ******/
	for(a=0; a<=MAXINST; a++)
		Instr[a] = (ADLSample *)malloc(sizeof(ADLSample));

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

		if(Instr[a+1]->Typi==1)A++;
		if(Instr[a+1]->Typi==2)B++;
	}

	Hdr.InSpeed = Hdr.InSpeed&127;
	if(A>B)/*Enemm„n SMP:it„ kuin AME-ja */
	{
		FMVolConv = 1;
		// Hdr.InSpeed |= 128; /* Force linearity */
	}

	/****** IF NECESSARY, CONVERT THE INSTRUMENTS ******/
	for(A=B=a=0; a<Hdr.InsNum; a++)
	{
		byte GM = 0;
        unsigned int scale=63;
		int ft=0; //Finetune

		fseek(f, StartPtr+InstPtr[a]*16, SEEK_SET);
		fread(Instr[a+1], sizeof(ADLSample), 1, f);

		Instr[a+1]->DosName[2] = 0; //No doublechannel mode

		if(Instr[a+1]->Name[0]=='G')    //GM=General MIDI
		{
			int c = Instr[a+1]->Name[1];
			if(c=='M' 	 //GM; M stands also for "melodic"
			 ||c=='P')	 //GP; P stands for "percussion"
			{
				int sign;
				char *s = Instr[a+1]->Name+2;
				for(sign=0; isdigit(*s); GM = GM*10 + *s++ - '0');
				if(*s=='-')sign=1;
				if(sign || *s=='+')
				{
					for(; isdigit(*++s); ft = ft*10 + *s - '0');
				}

				if(sign)ft=-ft;

				if(ft>127 || ft<-128)
				{
					printf(
						"Warning: Instrument %d:\n"
						"\t Finetune value out of ranges; using %d instead of %d.\n",
						a, (char)ft, ft);
				}

				if(*s == '/')
				{
					for(scale=0; isdigit(*++s); scale=scale*10 + *s-'0');
					if(scale > 63)
					{
						printf(
							"Warning: Instrument %d:\n"
							"\t Scaling level too big; using %d instead of %d.\n",
							a, scale&63, scale);
					}
				}

				if(*s == '*')
					s++, Instr[a+1]->DosName[2] = 1; //Doublechannel mode

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
	if(A && !Verify)
		printf("Warning: %d samples converted to adlib instruments.\n", A);

	/****** DETERMINE, WHICH PATTERNS AND ROWS BE NEVER VISITED ******/
	memset(There, 0, sizeof There);
	memset(Patns, 0, sizeof Patns);
	memset(Ortog, 0, sizeof Ortog);

	B = 0;
/*	printf("Old orders: ");
	for(a=0; a<Hdr.OrdNum; a++)printf("%02X ", Ords[a]);
	printf("\n");
*/
	for(a=0; a<Hdr.OrdNum; a++)
	{
		if(Ords[a]==255) //end
		{
			B+=Hdr.OrdNum-a;
			Hdr.OrdNum=a;
			break;
		}
		if(Ords[a]<254 && Ords[a] >= Hdr.PatNum)
		{
			printf(
				"Fatal: Your song file refers to missing patterns.\n"
				"       Fix this horrible error and try again.\n"
				"       NEVER use EMPTY patterns as a part of a song.\n"
				"       NEVER even include them to the order list.\n"
				"       The dummy pattern number is %02d.\n", Ords[a]);
			exit(0);
		}
	}

    AdlData.CurOrd=0;
	Investigating=Surprise=a=0;

DetPat:
	Row=0;
    if(AdlData.CurOrd >= Hdr.OrdNum)goto DonePatDet;

    AdlData.CurPat=Ords[AdlData.CurOrd];

    if(AdlData.CurPat==0xFE)
	{
        AdlData.CurOrd++;
		goto DetPat;
	}

    fseek(f, StartPtr+((long)(PatnPtr[AdlData.CurPat])<<4), SEEK_SET);
	i=0, fread(&i, 2, 1, f);

	TmpPat = (char *)malloc(i-=2);
	pp=TmpPat;
	fread(pp, i, 1, f);

    if(a>0 && Ortog[AdlData.CurOrd][a-1] && !Ortog[AdlData.CurOrd][a] && Verbose)
		printf(
			"Surprising jump to pattern %02d, row %02d...\n"
			"This kind of things are usually Bisqwit's handwriting!\n",
			AdlData.CurPat, a);

	for(;;)
	{
		int Bxx, Cxx;

		if(!a)
		{
            if(Ortog[AdlData.CurOrd][Row])break;

			if(Verbose)
                printf("Ord %02X Pat %02d Row %02d\r", AdlData.CurOrd, AdlData.CurPat, Row);
			if(Investigating)
			{
                for(a=0; a<64; a++)if(Ortog[AdlData.CurOrd][a])break;
				if(a==64)
				{
/*                  printf("Surprise at order %02X (%02X%-40c\n",
                           AdlData.CurOrd, Ords[AdlData.CurOrd], ')');
*/					Surprise++;
				}
				a=0;
			}
            Ortog[AdlData.CurOrd][Row]=There[AdlData.CurPat][Row]=1;
		}

		Bxx=Cxx=-1;

		for(;;)
		{
			char b = *pp++;
			if(!b)break;
			Command = 0;
			if(b<32)continue; //Turha tavu

			if(b&32)
			{
				int c = *++pp;
				if(c)InstUsed[c]=1;
				pp++;
			}
			if(b&64)pp++;
			if(b&128)
			{
				Command = *pp++;
				InfoByte= *pp++;
			}
			if(Command=='B'-64)Bxx=InfoByte;
			if(Command=='C'-64)Cxx=InfoByte;
		}

		if(a)
			a--;
		else
		{
			if(Bxx>=0)
			{
                AdlData.CurOrd=Bxx;
				if(Cxx>=0)a=(Cxx&15) + 10*(Cxx>>4);
				free(TmpPat);
				goto DetPat;
			}
			if(Cxx>=0)
			{
                AdlData.CurOrd++;
				a=(Cxx&15) + 10*(Cxx>>4);
				free(TmpPat);
				goto DetPat;
			}
		}
		// FIXME: Verify? Pattern row count
		if(++Row>=64)
		{
            AdlData.CurOrd++;
			free(TmpPat);
			goto DetPat;
		}
	}
	free(TmpPat);

DonePatDet:
/*	printf("Temp orders: ");
	for(a=0; a<Hdr.OrdNum; a++)printf("%02X ", Ords[a]);
    printf("\n");
*/
	if(!Hdr.OrdNum)
	{
		printf("Fatal: File does not contain any orders. Stop.\n");
		exit(0);
	}

	for(a=0; a<Hdr.OrdNum-1; a++)
		if(Ords[a]==254 && Ords[a+1]<254) //The what if -case
		{
			for(i=0; i<64; i++)if(Ortog[a+1][i])break;
			if(i==64)	//Very ortog case.
			{
				// Investigating a possible music archive.
                AdlData.CurOrd=a+1;
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
			B++; //Increase score
		}
		else
			a++; //Next generatable order

		A++; //Next handled order
	}

/*	printf("New orders: ");
	for(a=0; a<Hdr.OrdNum; a++)printf("%02X ", Ords[a]);
    printf("\n");
*/
	if(Verbose && !Verify)
		printf("Re-organized orders, gained %d bytes.\n", B);

	/****** SORT THE INSTRUMENTS, REMOVE UNUSABLE ONES ******/
	memset(InstConv, 255, sizeof(InstConv));
	memset(NotUsed, 0, sizeof(NotUsed));
	for(A=a=0; a<Hdr.InsNum; a++)
		if(Instr[a+1]->Typi==2)
		{
			if(InstUsed[a+1])
			{
				InstCon2[A] = a+1;
				InstConv[a+1] = A++;
			}
			else
				NotUsed[a+1]=1;
		}


	if(Verbose)
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
	memset(ChanConv, 127, sizeof ChanConv);
	for(ChanCount=a=0; a<32; a++)
		if(Hdr.Channels[a] >= 16 && Hdr.Channels[a] < 16+MAXCHN)
//		{
			ChanConv[a] = ChanCount++;
/*			Hdr.Channels[ChanCount++] = Hdr.Channels[a]&15;
		}
	if(Verify)
	{
		printf("\nAfter conversion they would be: ");
		for(a=0; a<ChanCount; a++)
		{
			if(Hdr.Channels[a] > MAXCHN)
				printf("?? ");
			else
				printf("M%d ", Hdr.Channels[a]+1);
		}
	}
	if(Verbose)printf(" (gain %d bytes)\n", 31-ChanCount);
	B += 31-ChanCount; //Increase score
*/
	if(!Verify)
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
		fwrite(&Hdr.OrdNum, 2, 1, fo);
		fwrite(&Hdr.InsNum, 2, 1, fo);
		fwrite(&Hdr.PatNum, 2, 1, fo);
		fwrite(&Hdr.InsNum, 2, 1, fo);
		fwrite(&Hdr.InSpeed, 1, 1, fo);
		fwrite(&Hdr.InTempo, 1, 1, fo);
	/*	fwrite(&ChanCount, 1, 1, fo);
		fwrite(&Hdr.Channels, ChanCount, 1, fo);
	*/	fwrite(&Ords, 1, Hdr.OrdNum, fo);
	}

	/****** WRITE THE INSTRUMENTS ******/
	for(A=0; A<Hdr.InsNum; A++)
	{
		a = InstCon2[A];

		if(FMVolConv)
		{
			Instr[a]->Volume =
				(FMVol[Instr[a]->Volume&127]>>1) | (Instr[a]->Volume&128);
			if(Instr[a]->Volume&128)
				Instr[a]->C2Spd = 8363; //Standardize C2SPD
		}

		Instr[a]->Volume = // Convert volumes in range 0..64 to 0..63
		   ((Instr[a]->Volume&127)*63/64) | (Instr[a]->Volume&128);

		if(Instr[a]->DosName[2])Instr[a]->Volume |= 64; //Doublechannel mode

		if(!Verify)
		{
			fwrite(&Instr[a]->DosName[1], 1, 1, fo); //finetune
			fwrite(&Instr[a]->D,         11, 1, fo); //adlib data
			fwrite(&Instr[a]->Volume,     1, 1, fo); //default volume
			fwrite(&Instr[a]->C2Spd,      2, 1, fo); //c2spd (adlib only)
			fwrite(&Instr[a]->DosName[0], 1, 1, fo); //patch
	}	}

	/****** SAFELY CAN UNALLOCATE THE INSTRUMENT MEMORY ******/
	for(a=0; a<=MAXINST; a++)free(Instr[a]);

	B += sizeof(InternalSample)*SkipSmp;

	/****** READ, CONVERT AND WRITE THE PATTERNS ******/
	memset(Cut, 0, sizeof(Cut));
	memset(Gain,0, sizeof(Gain));

	for(a=0; a<Hdr.PatNum; a++)
	{
		char *p, *q;

		int Length;

		if(!PatnPtr[a])
            AdlData.Patn[a].Len=0;
		else
		{
			fseek(f, StartPtr+((long)(PatnPtr[a])<<4), SEEK_SET);
            fread(&AdlData.Patn[a].Len, 2, 1, f);
		}

        AdlData.Patn[a].Ptr = (char *)malloc(((1+2+1+2)*32U+1)*64U); // Max alloc

        if(!AdlData.Patn[a].Ptr)
		{
			fprintf(stderr, "Memory allocation error, INTGEN halted\n");
			exit(errno);
		}

        if(AdlData.Patn[a].Len==0)
		{
            AdlData.Patn[a].Len = 64;
            memset(AdlData.Patn[a].Ptr, 0, 64);
			/* Ugliful empty pattern!! */
		}
		else
		{
            AdlData.Patn[a].Len -= 2;
            fread(AdlData.Patn[a].Ptr, 1, AdlData.Patn[a].Len, f);
		}

		Length = 0;
        p = AdlData.Patn[a].Ptr; //Input
        q = AdlData.Patn[a].Ptr; //Output

		for(Row=0; Row<64; Row++)
		{
			for(;;)
			{
				char b = *p++;
				int w = There[a][Row];

				if(!b)break;
				if(b<32)continue; //Turha tavu

				if(ChanConv[b&31]==127)w=0; //Hyl„t„„n rivi.
				if((b&31) >= MAXCHN)w=0;	//Hyl„t„„n t„ll”inkin.

				b = ChanConv[b&31] | (b&~31);

				if(b&32)
				{
					Note   = *p++;
					Instru = *p++;
					if(Instru)
					{
						if((Instru=InstConv[Instru])==255)
						{
							//Hyl„t„„n instrumentti ja nuotti.
							b&=~32;
						}
						Instru++;
					}
				}
				if(b&64)
				{
					Volume = *p++;
					if(FMVolConv)
						Volume = FMVol[Volume]>>1;
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
						b &= 127;
				}

				if(b<32)w=0;

				if(w) //Kelpaako
				{
					Length++;
					*q++ = b;

					if(b&32)
					{
						Length++, *q++=Note;
						Length++, *q++=Instru;
					}
					if(b&64)
						Length++, *q++=(Volume*63)/64; //Convert volume
					if(b&128)
					{
						Length++, *q++=Command;
						Length++, *q++=InfoByte;
					}
				}
				else if(There[a][Row])
				{
					B++;
					if(b&32)B+=2;
					if(b&128)B+=2;
					if(b&64)B++;
				}
			}
			for(i=Row+1; i<64; i++)
				if(There[a][i])break;

			if(i==64&&Row<63)
			{
				Cut[Row][a]=1;
                Gain[Row][a] = AdlData.Patn[a].Len - (int)(p-AdlData.Patn[a].Ptr);

				Length++;
				*q++ = 1; //End of pattern
				break;
			}

			*q++ = 0;
			Length++;
		}

		B += (AdlData.Patn[a].Len - Length);

        fwrite(&Length,                  2, 1, fo);
        fwrite(AdlData.Patn[a].Ptr, Length, 1, fo);

        free(AdlData.Patn[a].Ptr);
	}

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

	if(Verbose && B && !Verify)
	{
		printf("Total optimization gain: ");
		printf(B>1?"%d bytes":"The dummy order byte",B);
		if(SkipSmp)printf(" (%s%d instrument entries)",
			B>SkipSmp*sizeof(InternalSample)
				?"contains ":"",
			SkipSmp);
        printf(".\n");
	}

	fclose(f);

	if(Verify)
	{
		printf("Ok.\n");
		return;
	}

	if(*DestFile=='*' || *DestFile=='$'
     ||*Name    =='*' || *Name    =='$')
	{
		fclose(fo);
		if(Verbose)printf("We have now the temporary file nesmusa.$$$ containing the internaldata.\n");
		return;
	}

	sprintf(Buf2, "%s.C", DestFile);
	if(Verbose)printf("Writing from temporary file to %s\n", Buf2);
	f = fopen(Buf2, "wt");
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

	fclose(f);
}

/* Patches */
char adl[] =
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

int FMVol[65] =
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

int SubMidIns(char far *a, char *fdat, char *m)
{
	long size;
    int b, dev, devtold;
    FILE *fp;

	sscanf(m, "%d:%d:%d:%d", &Verbose, &b, &dev, &devtold);

	InitADLIB(NO_AUTODETECT);

	if(dev==SelectOPL)
		if(!devtold)
			dev=SelectMPU;
		else
		{
			printf("Sorry, GM/GP tests can not be done with OPL in this program.\n");
            return 1;
		}

	if(Verbose>0)
	{
		printf("Device: ");
		fflush(stdout);
	}

	if(SelectADLDev(dev) < 0)
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
			printf("Did not find fmdrv.dat\n");

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

	ExitADLIB();

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
		"*** If the program crashes, you are obligated to tell the\n"
		"*** crash information to Bisqwit. Three crash output cases:\n"
		"***\n"
		"***    Device: (no linefeed)             Device detection crashed\n"
		"***    Device: something (no linefeed)   Sound file loading crashed\n"
		"***    Device: something (linefeed)      Player crashed.\n"
		"***\n"
		"*** Send the file you was trying to play, the command line\n"
		"*** you invoked this program with and tell what is your soundcard.\n"
        "*** The e-mail address is joelhy@evitech.fi (until June 2002).\n"
	);
}

int main(int argc, char **argv)
{
	int Heelp, Version, a;

	char *ifn, *ofn, *vn;

	ifn=ofn=vn=NULL;
	Heelp=Version=0;

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
					printf("IntGen: Unterminated description - ')' missing.\n");
					return 1;
				}
			}
		}

		while(*s==' ' || *s=='\t')s++;
		if(!*s)continue;

		if(*s == '-')
		{
			s++;
			if(!*s)goto InvOpt;
			if(*s == '-')
			{
				s++;
				if(!strcmp(s, "verbose"   )){s=NULL; goto argV;}
				if(!strcmp(s, "batch"     )){s=NULL; goto argB;}
				if(!strcmp(s, "no-vibrato")){s=NULL; goto argH;}
				if(!strcmp(s, "no-retrig" )){s=NULL; goto argQ;}
				if(!strcmp(s, "verify"    )){s=NULL; goto argR;}
                if(!strcmp(s, "noname"    )){s=NULL; goto argN;}
				if(!strcmp(s, "version"   )){s=NULL; goto ArgV;}
				if(!strcmp(s, "help"      )){s=NULL; goto Arg;}
			InvOpt:
				printf("IntGen: Unrecognized option `%s'\n", argv[a]);
				return 1;
			}
			else
				for(; s && *s; s++)
				{
					switch(*s)
					{
						case 'v': argV: Verbose=1; break;
						case 'b': argB: Batch=1;   break;
						case '?': Arg:  Heelp=1;   break;
						case 'h': argH: Hxx=0;     break;
                        case 'n': argN: noname=1;  break;
						case 'q': argQ: Qxx=0;     break;
						case 'r': argR: Verify=1;  break;
						case 'V': ArgV: Version=1; break;
						default:
							printf("IntGen: Unrecognized option `-%c'\n", *s);
							return 1;
					}
				}
		}
		else
		{
			if(!ifn)ifn=s;
			else if(!ofn)ofn=s;
			else if(!vn)vn=s;
			else
			{
				printf("IntGen: Too many parameters -- `%s'\n", s);
				return 1;
			}
		}
	}

	if(Verbose || Version || Heelp)
	{
		printf("S3M to internalform Optimizing Converter Version 2.82 (C) 1992, 1999 Bisqwit\n");
		if(Version)return 0;
		if(!Batch)printf("\n");
	}

	if(Heelp)
	{
		/*
		int a;
		for(a=0; a<argc; a++)
			printf("argv[%d] = \"%s\"\n", a, argv[a]);
		*/
		printf(
			"Usage:    INTGEN [-bvhqrnV] [\\(infotext\\)] [srcfile [destfile [pubname]]]\n"
			"\n"
			"Examples:\n"
			"  INTGEN -b (Official WSpeed musicfile) \\mod\\tcsmods\\jhmadl52.s3m wspeed dah\n"
			"  INTGEN music.s3m musicinc music_table\n"
			"\n"
			"Options:\n"
			"\t-v, --verbose\t\tVerbose\n"
			"\t-b, --batch\t\tBatch working mode\n"
			"\t-h, --no-vibrato\tStrip Hxx\n"
			"\t-q, --no-retrig\t\tStrip Qxx\n"
			"\t-r, --verify\t\tVerifies source .S3M. Implies -v.\n"
			"\t-n, --noname\t\tDisables song name from output file.\n"
			"\t-?, --help\t\tHelp\n"
			"\t-V, --version\t\tVersion information\n\n"
			"Note: You must not supply extension for destfile. C assumed.\n"
			"\n"
			"If you don't know why to use this program,\n"
			"you obviously don't even need to know.\n");
		return -1;
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
		if(ofn || vn)
			printf("Warning: Output file name superfluous with --verify\n");
	}
	else
	{
		if(!ofn)
		{
			ofn=(char *)malloc(512);
			printf("Output file name? "); scanf("%s", ofn);
		}
		if(!vn)
		{
			vn=(char *)malloc(512);
			printf("Public variable name? "); scanf("%s", vn);
		}
	}

	S3MtoInternal(ifn, ofn, vn);

	return 0;
}
