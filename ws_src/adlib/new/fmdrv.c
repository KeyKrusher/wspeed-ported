#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <values.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

#if defined(__BORLANDC__) || defined(DJGPP)

#include <alloc.h>
#include <conio.h>
#include <io.h>

#define MINDELAY 1000
#else

#include <sys/ioctl.h>
#include <termios.h>

#define AnsiOpt 0

#define MINDELAY 20000

#endif

#include <fcntl.h>

#include "music.h"
#include "m_opl.h"
#include "fmdrv.h"
#include "m_path.h"
#include "adldata.h"

#ifdef __MSDOS__
static const char NULFILE[] = "NUL";
#else
static const char NULFILE[] = "/dev/null";
#endif

static int devtold = 0;
static int Verbose = 0;
static int Confuse = 0;
static int Quiet   = 0;
static int ShortFixes=1;

static int COLS;    /* Screen width  */
static int LINES;   /* Screen height */

static const char *infn=NULL, *outfn=NULL;

static const char tempfn[] = "fmdrv.tmp";
static const char tempfn2[] = "fmdrv.tm2";
static const char A0[] = "fmdrv";

FILE *tempf;

#define VERBOSE(x) if(Verbose>=(x))

static DWORD ConvL(const BYTE *s)
{
	return (((DWORD)s[0] << 24) | ((DWORD)s[1] << 16) | ((WORD)s[2] << 8) | s[3]);
}

static WORD ConvI(const BYTE *s)
{
	return (s[0] << 8) | s[1];
}

static void Die(int e)
{
	fprintf(stderr, "%s: Die(%d (\"%s\"))\n", A0, e, strerror(e));
	exit(EXIT_FAILURE);
}

static struct Midi
{
/* Fixed by ReadMIDI() */
	int Fmt;
	int TrackCount;
	int DeltaTicks;
	size_t *TrackLen;
	BYTE **Tracks;
/* Used by PlayMIDI() */
	DWORD  *Waiting, *sWaiting, *SWaiting;
	BYTE   *Running, *sRunning, *SRunning;
	size_t *Posi, *sPosi, *SPosi;
	DWORD  Tempo;
	DWORD  oldtempo;
	DWORD  initempo;
/* Per channel */
	BYTE   Pan[16];
	BYTE   Patch[16];
	BYTE   MainVol[16];
	BYTE   PitchSense[16];
	int    Bend[16], OldBend[16];
	int    Used[16][127]; /* contains references to adlib channels per note */
} MIDI;

/* Borland C++ enum allows only values within ñ32k */
#define snNoteOff    0x7FB1
#define snNoteOn     0x7FB2
#define snNoteModify 0x7FB3
#define snNoteUpdate 0x7FB4

/* Modifiable with -l */
static int MAXS3MCHAN = 9;

typedef struct
{
/* Important - at offset 0 to be saved to file */
	long Age;
	BYTE Note;
	BYTE Instru;
	BYTE Volume;
	BYTE cmd;
	BYTE info;
	BYTE KeyFlag;	/* Required to diff aftertouch and noteon */
					/* Byte to save space */
/* Real volume(0..127) and main volume(0..127) */
	int RealVol,MainVol;
	/* RealVol must be first non-saved */
/* Less important */
	int LastInstru;	/* Needed by SuggestNewChan() */
	int BendFlag;
	BYTE LastPan;
/* To fasten forcement */
	int uChn, uNote;
} S3MChan;

static S3MChan Chan[MAXCHN];
static S3MChan PrevChan[MAXCHN];

#define chansavesize ((int)((long)&Chan[0].RealVol - (long)&Chan[0].Age))

static int InstruUsed[256];
static int Forced=0;
static int Idle=-1;
static const int tempochanged = -5;
static int PrevIdle=0;
static int LinesQ=0;

static int BendMerk(int fl) 
{
	#ifdef __GNUC__
	static const char merks[] = " +-±";
	#else
	static const char merks[] = " ";
	#endif
	return merks[fl];
}

static char *Nuotti(int note)
{
	static char Buf[] = "C-4";

	switch(note)
	{
		case 254:
			strcpy(Buf, "^^^");
			break;
		case 255:
			strcpy(Buf, "...");
			break;
		default:
		{
			static char Notes[] = "C-C#D-D#E-F-F#G-G#A-A#B-";
			int c = note%12;
			sprintf(Buf, "%c%c%d", Notes[c+c],Notes[c+c+1], note/12);
		}
	}
	return Buf;
}

static int ForceIns = 0;
static int Herzit   = 0;
static int OplLog   = 0;
static int OplVol   = 0;
static int PrintPats= 1;
static long OplViive= 0;

/* Force everything to be displayed all the time in AnalyzeRow */
static int Brainless = 0;

static void FlushOplViive(void)
{
	OplViive/=125;
	if(OplViive)
	{
		fprintf(stderr, "d%lX", OplViive);
		OplViive=0;
	}
}
static int Herzi[MAXCHN];
static int OldHerz[MAXCHN];

/* Maximum displayable channels */
static int GetMaxDispChan(void)
{
	return MAXS3MCHAN;
}

enum edmModes {edmNone=0,edmNorm,edmHerz,edmHex,edmShort,edmLong};
enum edmModes
	NoteMode, BendMode,
	InstMode, VolMode, CmdMode,
	AgeMode, IdleMode, ForceMode;

static void InitChanDisp(void)
{
	int b = GetMaxDispChan();
	int m = COLS; /* Screen write width */
	long best;

	enum edmModes nm,bm,sm,vm,cm,am,im,fm;

	nm = edmNone;
	bm = edmNone;
	sm = edmNone;
	vm = edmNone;
	cm = edmNone;
	am = edmNone;
	im = edmNone;
	fm = edmNone;

	for(best=-1; ; )
	{
		int Dah, Len;
		long Score;

		/* This construct uses tab size of two, but  *
		 * it is better than to make long rows...    */
		switch(nm)
		{
		  case edmNone: nm=edmHerz; break;
		  case edmHerz: nm=edmHex;  break;
		  case edmHex:  nm=edmNorm; break;
		  default:      nm=edmNone;
			switch(bm)
			{
			  case edmNone: bm=edmNorm; break;
			  default:      bm=edmNone;
				switch(sm)
				{
				  case edmNone: sm=edmHex; break;
				  default:      sm=edmNone;
					switch(vm)
					{
					  case edmNone: vm=edmNorm; break;
					  default:      vm=edmNone;
						switch(cm)
						{
						  case edmNone: cm=edmNorm; break;
						  default:      cm=edmNone;
							switch(am)
							{
							  case edmNone: am=edmShort; break;
							  case edmShort:am=edmLong; break;
							  default:      am=edmNone;
								switch(im)
								{
								  case edmNone: im=edmNorm; break;
								  default:      im=edmNone;
									switch(fm)
                                    {
									  case edmNone: fm=edmNorm; break;
									  case edmNorm: fm=edmShort; break;
									  case edmShort:fm=edmLong; break;
									  default:      fm=edmNorm;
										goto Done;
	/*  im  nm  bm  sm  vm  cm  am  fm  */
		}   }   }   }   }   }   }   }

		Len=Dah=0;
		switch(nm)
		{
			case edmNone:
				Dah -= 100; /* Strongly discouraged */
				break;
			case edmHerz:
				Len += 4;
				Dah += 10;
				break;
			case edmHex:
				Len += 2;
				Dah += 7;
				break;
			case edmNorm:
				Len += 3;
				Dah += 10;
			default: ;
		}
		switch(bm)
        {
            case edmNone:
                break;
            case edmNorm:
                Len++;
                Dah += 10;
			default: ;
        }
		switch(sm)
        {
            case edmNone: break;
            case edmHex:
				Len += (nm!=edmNone && bm==edmNone) ? 3 : 2;
				Dah += 5;
				/* as instrument number is as long as volume number, make *
				 * difference with Dah; instrument number is more wanted. */
			default: ;
		}
		switch(vm)
		{
			case edmNone: break;
			case edmNorm:
				Len += (sm!=edmNone||bm==edmNone) ? 3 : 2;
				Dah += 2;
				/* as instrument number is as long as volume number, make *
				 * difference with Dah; instrument number is more wanted. */
			default: ;
		}
		switch(cm)
        {
            case edmNone:
                Dah += 3;
                break;
			case edmNorm:
                Len += (vm!=edmNone||sm!=edmNone||bm==edmNone)
                        ? 4 : 3;
				Dah++;
			default: ;
		}
        switch(am)
        {
            case edmNone:
                Dah += 10;
				break;
            case edmShort:
                Len += (vm!=edmNone||vm!=edmNone||sm!=edmNone||bm==edmNone)
						? 5 : 4;
				break;
			case edmLong:
				Dah -= 40; /* Strongly discouraged */
				Len += (cm!=edmNone||vm!=edmNone||sm!=edmNone||bm==edmNone)
						? 9 : 8;
			default: ;
        }
        Len = b*Len + (b-1); /* b-1 times vertical bar */

        switch(im)
		{
			case edmNone:
				Dah -= 500;	/* Strongly discouraged */
                break;
			case edmNorm:
				Len += 5;
				Dah += 5;
			default: ;
		}
        switch(fm)
		{
            case edmNone:
                break;
			case edmNorm:
                Len += 3;
				Dah += 10;
				break;
			case edmShort:
                Len += (AgeMode!=edmNone) ? 2 : 1;
                Dah += 10;
				break;
			case edmLong:
				Len += 4;
				Dah += 10;
			default: ;
		}

		Score = Dah + (Len >= m ? -10 : 500*Len);

		if((ForceIns!=0 && sm==edmNone)
		|| ((!Herzit) ^ (nm!=edmHerz)))Score=0;

		if(Score > best)
		{
			best = Score;
		#if 1
			#define s(x)
		#else
			printf("Score=%ld, Dah=%d, Len=%d, m=%d\n", Score, Dah, Len, m);
			#define s(x) printf(" "#x"=%s", x==edmNone?"none":x==edmShort?"short":\
			             x==edmLong?"long":x==edmHex?"hex":\
			             x==edmNorm?"norm":"herz")
		#endif			             
			NoteMode  = nm; s(nm);
			BendMode  = bm; s(bm);
			InstMode  = sm; s(sm);
			VolMode   = vm; s(vm);
			CmdMode   = cm; s(cm);
			AgeMode   = am; s(am);
			IdleMode  = im; s(im);
			ForceMode = fm; s(fm); /*printf("\n");*/
			
			#undef s
		}
	}
Done:;
}

/* Return value: 1=Yes, mixed. 0=Not mixed */
static int UndoAndMixRow(void)
{
	int a;

	if(devtold && LinesQ > LINES-2)return 0;

	for(a=0; a<MAXS3MCHAN; a++)
		if(!Chan[a].Age && PrevChan[a].Volume && !PrevChan[a].Age)
		{
			/* Can't join, both are new */
			return 0;
		}

	if(!Quiet && devtold && PrintPats)
	{
		#if defined(__BORLANDC__) || defined(DJGPP)
		gotoxy(1, wherey()-
		#else
		WhereX=1;
		printf("\r\33[%dA",
		#endif
		(LinesQ+1)
		);
	}

	if(!devtold)
		if(ftell(tempf))
			fseek(tempf, -(long)(sizeof(Idle)+MAXS3MCHAN*chansavesize), SEEK_CUR);

	Idle += PrevIdle;
	PrevIdle = 0;

	for(a=0; a<MAXS3MCHAN; a++)
		if(Chan[a].Age)
			Chan[a] = PrevChan[a];

	return 1;
}

static void AnalyzeRow(void)
{
	int a, b, Mixed=0;
	
	for(a=0; a<MAXS3MCHAN; a++)
		if(!Chan[a].Age)break;

	Idle++;

	if(a==MAXS3MCHAN)return;

	if(ShortFixes)
	{
Bla:    if(Idle < 4 && PrevIdle)
			Mixed = UndoAndMixRow();
	}
	else if(Herzit)
	{
		int a;
		for(a=0; a<MAXCHN; a++)if(OldHerz[a] != Herzi[a])break;
		if(a==MAXCHN)goto Bla;
		memcpy(&OldHerz, &Herzi, sizeof OldHerz);
	}

	PrevIdle = Idle;
	memcpy(&PrevChan, &Chan, sizeof PrevChan);

	if(devtold && PrintPats)
	{
		b = GetMaxDispChan();

		//#define TRAP break; default:__emit__(0xCC);
		#define TRAP default:;

		switch(IdleMode)
		{
			case edmNone: break;
			case edmNorm: printf("%-5d", Idle); /* Pistää tarpeeksi spacea */
			TRAP
		}

		for(a=0; a<b; a++)
		{
			int chanote = Chan[a].Note;
			/* Show noteoffs anyway */
			if(VolMode==edmNone && !Chan[a].Volume)chanote = 254;

			if(((Chan[a].KeyFlag||chanote==254)&&!Chan[a].Age) || Brainless)
				switch(NoteMode)
				{
					case edmNone: break;
					case edmHerz: printf("%-4d", Herzi[a]); break;
					case edmHex:
						switch(chanote)
						{
							case 255: printf(".."); break;
							case 254: printf("^^"); break;
							default:
							{
								char *s = Nuotti(chanote);
								if(s[1]=='-')s[1]=s[2];
								s[2] = 0;
								printf("%s", s);
							}
						}
						break;
					case edmNorm: printf(Nuotti(chanote));
					TRAP
				}
			else
				switch(NoteMode)
				{
					case edmNone: break;
					case edmHerz: printf("...."); break;
					case edmHex:  printf(".."); break;
					case edmNorm: printf("...");
					TRAP
				}

			switch(BendMode)
			{
				case edmNone: break;
				case edmNorm: printf("%c", BendMerk(Chan[a].BendFlag));
					TRAP
			}

			if((Chan[a].KeyFlag&&!Chan[a].Age) || Brainless)
				switch(InstMode)
				{
					case edmNone: break;
					case edmHex:
						if(NoteMode!=edmNone && BendMode==edmNone)printf(" ");
						printf("%02X", Chan[a].Instru+1);
					TRAP
				}
			else
				switch(InstMode)
				{
					case edmNone: break;
					case edmHex:
						if(NoteMode!=edmNone && BendMode==edmNone)printf(" ");
						printf("..");
					TRAP
				}

			if(!Chan[a].Age || Brainless)
				switch(VolMode)
				{
					case edmNone: break;
					case edmNorm:
						if(InstMode!=edmNone||BendMode==edmNone)printf(" ");
						printf("%02d", Chan[a].Volume);
					TRAP
				}
			else
				switch(VolMode)
				{
					case edmNone: break;
					case edmNorm:
						if(InstMode!=edmNone||BendMode==edmNone)printf(" ");
						printf("..");
					TRAP
				}

			switch(CmdMode)
			{
				case edmNone: break;
				case edmNorm:
					if(VolMode!=edmNone||InstMode!=edmNone||BendMode==edmNone)
						printf(" ");
					printf("%c%02X",
						Chan[a].cmd?Chan[a].cmd|64:'.',
						Chan[a].info);
					TRAP
			}

			switch(AgeMode)
			{
				case edmNone: break;
				case edmShort:
				{
					char Buf[5];
					int b;
					if(CmdMode!=edmNone||VolMode!=edmNone||InstMode!=edmNone
					  ||BendMode==edmNone)
						printf(":");
					sprintf(Buf, "%04X", (int)(Chan[a].Age));
					for(b=0; b<4; Buf[b++]='.')if(Buf[b]!='0')break;
					printf(Buf);
					break;
				}
				case edmLong:
				{
					char Buf[9];
					int b;
					if(CmdMode!=edmNone||VolMode!=edmNone||InstMode!=edmNone
					  ||BendMode==edmNone)
						printf(":");
					sprintf(Buf, "%08lX", Chan[a].Age);
					for(b=0; b<8; Buf[b++]='.')if(Buf[b]!='0')break;
					printf(Buf);
				}
				TRAP
			}

			if(a<b-1)
			{
				#ifdef __BORLANDC__
				printf("³");
				#else
				printf("|");
				#endif
			}
		}

		if(Forced)
			switch(ForceMode)
			{
				case edmNone: break;
				case edmShort:
					if(AgeMode!=edmNone)printf(" ");
					printf("F");
					break;
				case edmNorm:  printf("<F>"); break;
				case edmLong:  printf(" <F>");
					TRAP
			}

		#undef TRAP

		printf("\n");

		//for(a=b; a<MAXS3MCHAN; a++)printf("+");

		if(Mixed)
			for(a=0; a<LinesQ; a++)printf("\n");
		fflush(stdout);
	}

	LinesQ=0;
	Forced=0;

	if(!devtold)
	{
		if(MIDI.Tempo != MIDI.oldtempo)
		{
			fwrite(&tempochanged,    sizeof(Idle),            1, tempf);
			fwrite(&MIDI.Tempo,      sizeof(MIDI.Tempo),      1, tempf);
			MIDI.oldtempo = MIDI.Tempo;
		}
		fwrite(&Idle, sizeof(Idle), 1, tempf);

		for(a=0; a<MAXS3MCHAN; a++)
			fwrite(&Chan[a], 1, chansavesize, tempf);
	}

	for(a=0; a<MAXS3MCHAN; a++)
		Chan[a].KeyFlag=0;

	Idle=0;
}

static void FixSpeed(float *Speed, int *Tempo)
{
	int a;
	float tmp = 1.0;
	*Speed = MIDI.Tempo * 4E-7 / MIDI.DeltaTicks;
	*Tempo = 125;

	VERBOSE(3)
		printf("%s: Midi speed is %lu per %d, finding the integermost 1/Speed...\n",
			A0, (unsigned long)MIDI.Tempo, MIDI.DeltaTicks);

	for(a=0x40; a<=0xFF; a++)
	{
		double Tmp;
		float n = a * *Speed;
		n = modf(1.0/n, &Tmp);
		if(n < tmp)
		{
			*Tempo = a;
			tmp = n;
		}
	}
	*Speed *= *Tempo;

	VERBOSE(3)
		printf("%s: Ok, Speed = Tempo * %02Xh * 4E-7 / DeltaTicks = 1/%.4f\n",
			A0, *Tempo, 1 / *Speed);
}


static void WriteOut(void)
{
	FILE *fp;
	int InSpeed = 6;
	int InTempo; /* Set with FixSpeed() call */
	int InsCnt, OrdCnt, PatCnt;
	float Posi;
	float Speed;
	int Tempo;
	int a,b, Axx, chai, RowCount;
	typedef BYTE VolTable[256][64];
	VolTable *FavouriteVolumes;
	BYTE PatchUsed[256];
	BYTE PatchMap[256];
	BYTE InstMap[256];
	char datfile[256];
	char *instname[256];
	long l;

	/* In MIDI:
	 *
	 * Tempo      = microseconds per 1/4 note
	 * Deltaticks = Number of delta-time ticks per 1/4 note
	 *
	 *                       Tempo/1000000
	 * Length of one tick is -------------, seconds.
	 *                         DeltaTicks
	 *
	 * In S3M:
	 *
	 * Rows played in a second = InTempo*2/InSpeed/5
	 *
	 * InSpeed = number of ticks per row
	 *
	 *                         2.5
	 * Length of one tick is ------- seconds.
	 *                       InTempo
	 *
	 *             5               Tempo
	 *  When ---------------- = ---------- ...
	 *       InTempo*2000000    DeltaTicks
	 *
	 * Anyway, when Speed equals
	 *    InTempo * MIDI.Tempo * 4E-7 / MIDI.DeltaTicks,
	 * then this works ok. :)
	 *
	 * TODO:
	 *   Try to fix the idle++ -thing... engine, whatever, so that
	 *   in no case will notes be skipped in the resulting song.
	 *
	 */

	Pathi(datfile, (sizeof datfile)-1, "PATH", "fmdrv.dat");

	VERBOSE(1)
		printf("%s: Reading '%s'\n", A0, datfile);

	memset(instname, 0, sizeof instname);

    if((fp = fopen(datfile, "rt")) == NULL)
	{
		VERBOSE(1)
			printf("%s: Error in '%s'\n", A0, datfile);
	}
	else
	{
		char Buf[64];
		while(fgets(Buf, 63, fp) != NULL)
		{
			int a;
			if(!strtok(Buf, ":"))continue;

			if(Buf[1]=='M')
				a = (int)strtol(Buf+2, NULL, 10)-1;
			else
			{
				if(Buf[1]!='P')continue;
				a = (int)strtol(Buf+2, NULL, 10) + (128-35);
			}
			instname[a] = strdup(strtok(NULL, "\n"));
		}
		VERBOSE(1)
			printf("%s: Closing '%s'\n", A0, datfile);
		fclose(fp);
	}

	FixSpeed(&Speed, &Tempo);
	InTempo = Tempo;

	if(!(fp = fopen(tempfn2, "wb+")))
	{
		fprintf(stderr, "%s: Could not open the pattern cache file, '%s' for writing.\n", A0, tempfn2);
		Die(errno);
	}
	VERBOSE(1)
		printf("%s: Reading '%s'\n%s: Writing '%s'\n", A0,tempfn, A0,tempfn2);

	Axx = InSpeed;

	for(a=0; a<256; PatchUsed[a++]=0);

	chai = 1; /* idle */

	RowCount = 0;
	Posi     = 0.0;

	FavouriteVolumes = (VolTable *)malloc(sizeof(VolTable));
	if(FavouriteVolumes)
		memset(FavouriteVolumes, 0, sizeof(VolTable));

	rewind(tempf);

	for(;;)
	{
		if(fread(&Idle, sizeof(Idle), 1, tempf) != 1)break;
		if(Idle != tempochanged)break;
		/* Update tempo! */
		fread(&MIDI.Tempo, sizeof(MIDI.Tempo), 1, tempf);
		FixSpeed(&Speed, &Tempo);
	}

	for(;;)
	{
		enum {isIdle,isAdd,isModify} e = isIdle;

		for(a=0; a<MAXS3MCHAN; a++)Chan[a].Age++;

		for(Posi++; Posi >= Speed; Posi -= Speed)
		{
			if(!Idle)
			{
				for(a=0; a<MAXS3MCHAN; a++)
				{
					fread(&Chan[a], 1, chansavesize, tempf);

					if(Chan[a].Age)
					{
						/* Then it is not new */
						#if 0
						if(!PrevChan[a].Age)
							Chan[a] = PrevChan[a];
						#endif
					}
					else if(PrevChan[a].Age)
					{
						/* new note over old note */
						if(e==isIdle)e = isAdd;
					}
					else
						e = isModify;
				}
				for(;;)
				{
					if(fread(&Idle, sizeof(Idle), 1, tempf) != 1)break;
					if(Idle != tempochanged)break;

					/* Update tempo! */
					fread(&MIDI.Tempo, sizeof(MIDI.Tempo), 1, tempf);
					FixSpeed(&Speed, &Tempo);
				}
				if(feof(tempf))break;
				memcpy(&PrevChan, &Chan, sizeof PrevChan);
			}
			else
				Idle--;
		}
		if(Posi >= Speed)break;	/* eof */

		if(e==isIdle && chai < 0x20)
			chai++;
		else
		{
			#define woSeekBack() fseek(fp,-chansavesize*MAXS3MCHAN, SEEK_CUR)
			#define woSeekForw() fseek(fp, chansavesize*MAXS3MCHAN, SEEK_CUR)
			#define woFuncRecord(_pr_func, _fr_src) { register int _pc_fr; \
				for(_pc_fr=0; _pc_fr<MAXS3MCHAN; _pc_fr++) \
					_pr_func(&_fr_src[_pc_fr], 1, chansavesize, fp); }
			#define woGetRecord(_pr_src) woFuncRecord(fread, _pr_src)
			#define woPutRecord(_gr_src) woFuncRecord(fwrite, _gr_src)
			#define woGetPrev(_gp_tmp) woSeekBack();woGetRecord(_gp_tmp)
			#define woPutPrev(_pp_tmp) woSeekBack();woPutRecord(_pp_tmp)
			#define woCmd(_pc_cmd) (_pc_cmd)-64
			#define woPutCommand(_pc_tmp, _pc_chan, _pc_cmd,_pc_info) \
				_pc_tmp[_pc_chan].cmd  = woCmd(_pc_cmd); \
				_pc_tmp[_pc_chan].info = (_pc_info)

			if(ftell(fp))
			{
				S3MChan tmp[MAXCHN];

				/* Todo: Add SCx. */
				#if 0
				if(chai > Axx && chai-Axx < 0x0F && e==isAdd)
				{
					int freec,bc,sc;

					/* Todo: Fix this. */
					woGetPrev(tmp);

					for(a=0; a<MAXS3MCHAN; a++)
						if(tmp[a].Age)
							tmp[a] = PrevChan[a];

					for(bc=-1, freec=a=0; a<MAXS3MCHAN; a++)
						if(!tmp[a].cmd && ((sc=(!tmp[a].Age)*(1+tmp[a].KeyFlag)) > bc))
							{bc=sc, freec=a;}
					woPutCommand(tmp, freec, 'S', 0xD0 | (chai-Axx));
					woPutPrev(tmp);
				}
				else
				#endif
				if(chai>Axx && !(chai%Axx) && chai/Axx < 0x0F
				&& ftell(fp) > chansavesize*MAXS3MCHAN)
				{
					S3MChan tmp2[MAXCHN];
					int bc,sc,freec, info = 0xE0 | ((chai/Axx)-1);

					woSeekBack();
					woGetPrev(tmp2);
					woGetRecord(tmp);

					for(bc=-1, freec=a=0; a<MAXS3MCHAN; a++)
						if(!tmp[a].cmd && ((sc=(!tmp[a].Age)*(1+tmp[a].KeyFlag)) > bc))
							{bc=sc, freec=a;}

					#if 0
					for(freec=0; freec<MAXS3MCHAN; freec++)
						if(!tmp2[freec].cmd
						 || tmp2[freec].cmd==woCmd('S'))
							break;
					if(freec==MAXS3MCHAN)freec=0;

					if(tmp2[freec].cmd == woCmd('S')
					&& tmp2[freec].info== info)
					{
						Axx = chai;
						woPutCommand(tmp2, freec, 'A', chai);
						woSeekBack();
						woPutPrev(tmp2);
						woSeekForw();
					}
					else
					{
					#endif
						for(bc=freec=-1, a=0; a<MAXS3MCHAN; a++)
							if(!tmp[a].cmd && ((sc=(!tmp[a].Age)*(1+tmp[a].KeyFlag)) > bc))
								{bc=sc, freec=a;}
						if(freec < 0)
							for(freec=a=0; a<MAXS3MCHAN; a++)
								if(tmp[a].cmd=='X')
								{
									freec=a;
									break;
								}

						woPutCommand(tmp, freec, 'S', info);
						woPutPrev(tmp);
					#if 0
					}
					#endif
				}
				else if(chai != Axx)
				{
					int freec,bc,sc;

					woGetPrev(tmp);

					for(bc=freec=-1, a=0; a<MAXS3MCHAN; a++)
						if(!tmp[a].cmd && ((sc=(!tmp[a].Age)*(1+tmp[a].KeyFlag)) > bc))
							{bc=sc, freec=a;}
					if(freec < 0)
						for(freec=a=0; a<MAXS3MCHAN; a++)
							if(tmp[a].cmd=='X')
							{
								freec=a;
								break;
							}

					woPutCommand(tmp, freec, 'A', chai);
					woPutPrev(tmp);

					Axx = chai;
				}

				if(Tempo != InTempo)
				{
					int freec,bc,sc;

					woGetPrev(tmp);

					for(bc=freec=-1, a=0; a<MAXS3MCHAN; a++)
						if(!tmp[a].cmd && ((sc=(!tmp[a].Age)*(1+tmp[a].KeyFlag)) > bc))
							{bc=sc, freec=a;}

					if(freec < 0)
						for(freec=a=0; a<MAXS3MCHAN; a++)
							if(tmp[a].cmd=='X')
							{
								freec=a;
								break;
							}

					woPutCommand(tmp, freec, 'T', InTempo=Tempo);
					woPutPrev(tmp);
				}
			}

			for(a=0; a<MAXS3MCHAN; a++)
			{
				if(Chan[a].KeyFlag&&!Chan[a].Age)
				{
					PatchUsed[Chan[a].Instru]=1;
					(*FavouriteVolumes)[Chan[a].Instru][Chan[a].Volume]++;
				}
				#if 0
				/* No need to be done */
				else
				{
					Chan[a].Instru=0;
					Chan[a].Note  =255;
					if(Chan[a].Age)Chan[a].Volume=255;
				}
				#endif
			}

			woPutRecord(Chan);
			RowCount++;

			chai = 1;

			#undef woSeekBack
			#undef woSeekForw
			#undef woFuncRecord
			#undef woGetRecord
			#undef woPutRecord
			#undef woGetPrev
			#undef woPutPrev
			#undef woCmd
			#undef woPutCommand
		}
	}

	VERBOSE(1) printf("%s: Closing '%s'\n", A0, tempfn);
	fclose(tempf);

	VERBOSE(1) printf("%s: Removing '%s'\n", A0, tempfn);
	unlink(tempfn);

	if(!(tempf = fopen(outfn, "wb")))
	{
		fprintf(stderr, "%s: Could not open the outputfile, '%s' for writing.\n", A0, outfn);
		Die(errno);
	}
	VERBOSE(1) printf("%s: Writing '%s'\n", A0, outfn);

	{	char Buf[32];
		memset(Buf, 0, 32);
		strcpy(Buf, "Converted with FMDRV"); //Less than 27
		Buf[28] = 0x1A;	/* ^Z */
		Buf[29] = 16;	/* File type; 16=S3M */
		fwrite(Buf, 1, 32, tempf);
	}

	for(InsCnt=a=0; a<256; a++)
		if(PatchUsed[a])
			InstMap[InsCnt++] = a;

	for(a=0; a<InsCnt; a++)
		for(b=a+1; b<InsCnt; b++)
			if(InstMap[a] > InstMap[b])
			{
				int c = InstMap[a];
				InstMap[a] = InstMap[b];
				InstMap[b] = c;
			}
	for(a=0; a<InsCnt; a++)
		PatchMap[InstMap[a]] = a;

	if(InsCnt>100)	/* = check S3M limit */
	{
		InsCnt=100;
	}

	{	/*			ordnum@0x20, insnum, patnum@0x24 flags version smpform */
		char Buf[32]={0,0,         0,0,      0,0,     0,0, 0x00,0xE0, 2,0,
		/*			Signature    GlobVol, Axx,Txx, MastVol */
				'S','C','R','M', 0x20,      0,0,   0x20,
				0,0,0,0,0,0, 0,0,0,0,0,0}; /* 12 bytes of zero at end */

		PatCnt = (RowCount+63)/64;
		OrdCnt = (PatCnt+3) & ~1;

		Buf[0]    = OrdCnt&255;
		Buf[2]    = InsCnt&255;
		Buf[4]    = PatCnt&255;
		Buf[0x11] = InSpeed;
		Buf[0x12] = InTempo;
		fwrite(Buf, 1, 32, tempf);

		for(a=0; a<32; a++)Buf[a] = a<MAXS3MCHAN ? a|16 : 255;
		fwrite(Buf, 1, 32, tempf);

		for(a=0; a<OrdCnt; a++)
			fputc(a<PatCnt ? a : 255, tempf);
	}

	l = 0x60L + OrdCnt + InsCnt+InsCnt + PatCnt+PatCnt;

	for(a=0; a<InsCnt; a++)
	{
		int k = InstMap[a], c,d;
		char Txt[28], Text[33];
		const char *tmp;
		InternalSample *Tmp = (InternalSample *)MusData.Instr[k];

		l = (l+15) & ~15;
		fseek(tempf, 0x60L + OrdCnt + a+a, SEEK_SET);

		fputc((unsigned)(l >>  4) & 255, tempf);
		fputc((unsigned)(l >> 12) & 255, tempf);

		fseek(tempf, l, SEEK_SET);

		fputc(2, tempf);			// amel

		for(b=0; b<15; b++)
			fputc(0, tempf);		// unused (filename, memseg)

		fwrite(Tmp->D, 1, 11, tempf);

		/* 53 seems to be quite common one. Let's   *
		 * use it if we don't have enough memory :) */
		d = 53;
		if(FavouriteVolumes)
			for(c=b=0; b<64; b++)
				if((*FavouriteVolumes)[k][b] > c)
				{
					c = (*FavouriteVolumes)[k][b];
					d = b;
				}
		Tmp->Volume = d;

		for(b=0; b<5; b++)
			fputc(b==1 ? Tmp->Volume : 0, tempf);

		fputc(8363 & 255, tempf);
		fputc(8363 >> 8,  tempf);

		for(b=0; b<14; b++)
			fputc(0, tempf); // C2SPD hi (word), rest unused

		tmp = instname[k] ? instname[k] : "Unknown";
		if(k <= 128)
			sprintf(Txt, "GM%d:%s", k+1, tmp);
		else
			sprintf(Txt, "GP%d:%s", k+(35-128), tmp);

		sprintf(Text, "%-28.28sSCRI", Txt);
		for(k=28; Text[--k]==' '; Text[k]=0);
		fwrite(Text, 1, 32, tempf);

		l += 0x50; /* Size of one instrument */
	}

	if(FavouriteVolumes)
	{
		free(FavouriteVolumes);
		FavouriteVolumes = NULL;
	}

	rewind(fp);
	for(a=0; a<PatCnt; a++)
	{
		int Len=0;
		l = (l+15) & ~15;

		fseek(tempf, 0x60L + OrdCnt + InsCnt+InsCnt + a+a, SEEK_SET);
        fputc((unsigned)(l >>  4) & 255, tempf);
        fputc((unsigned)(l >> 12) & 255, tempf);

		fseek(tempf, l, SEEK_SET);

		fputc(0, tempf); /* Below fixed. */
		fputc(0, tempf);

		for(b=0; b<64; b++)
		{
			int c;

			for(c=0; c<MAXS3MCHAN; c++)
			{
				int b = c;
				S3MChan tmp;
				fread(&tmp, 1, chansavesize, fp);

				if((tmp.Note != 255 || tmp.Instru != 0)
				 && tmp.KeyFlag
				 && !tmp.Age)
					b |= 32;

				if(!tmp.Age			/* Row with action */
				&& (!tmp.KeyFlag	/* Volume info alone */
				  || tmp.Volume!=MusData.Instr[tmp.Instru]->Volume))
					b |= 64;		/* Or non-default volume */

				if(tmp.cmd != 0)
					b |= 128;

				if(Brainless)
					b |= 128+64;	/* May not do +32 even here */

				if(b != c)
				{
					fputc(b, tempf), Len++;

					if(b&32)
					{
						int note = tmp.Note;
						int inst = PatchMap[tmp.Instru]+1;

						if(note < 254)
						{
							note -= 12; /* Decrease by one octave */
							if(note<0)note=0;
							note = (note%12) + ((note/12)<<4);
						}

						fputc(note, tempf), Len++;
						fputc(inst, tempf), Len++;
					}
					if(b&64)
						fputc(tmp.Volume, tempf), Len++;
					if(b&128)
					{
						fputc(tmp.cmd,  tempf), Len++;
						fputc(tmp.info, tempf), Len++;
			}	}	}
			fputc(0, tempf), Len++;
		}

		fseek(tempf, l, SEEK_SET);

		Len+=2;

		fputc(Len&255, tempf);
		fputc(Len>>8,  tempf);

		l += Len;
	}

	VERBOSE(1) printf("%s: Closing '%s'\n", A0, outfn);
	fclose(tempf);

	VERBOSE(1) printf("%s: Closing '%s'\n", A0, tempfn2);
	fclose(fp);

	VERBOSE(1) printf("%s: Removing '%s'\n", A0, tempfn2);
	unlink(tempfn2);
}

static int MakeVolume(int vol)
{
	return FMVol[vol*64/127]*63/128;
}

static void Bendi(int chn, int a)
{
	long HZ1, HZ2, Herz;

	int bc = MIDI.Bend[chn];
	int nt = Chan[a].Note;

	if(bc)VERBOSE(4) printf("Bender: Channel %d, note=%02Xh, bc=%d ", a, nt, bc);

		 if((int)bc > MIDI.OldBend[chn])Chan[a].BendFlag |= 1;
	else if((int)bc < MIDI.OldBend[chn])Chan[a].BendFlag |= 2;
	else Chan[a].BendFlag = 0;

	MIDI.OldBend[chn] = (int)bc;

	for(; bc < 0;      bc += 0x1000)nt--;
	for(; bc >=0x1000; bc -= 0x1000)nt++;

	if(bc)VERBOSE(4) printf("- became note=%02Xh, bc=%d\n", nt, bc), LinesQ++;

	HZ1 = Period[(nt+1)%12] * (8363L << (nt+1)/12) / 44100U;
	HZ2 = Period[(nt  )%12] * (8363L << (nt  )/12) / 44100U;

	Herz = HZ2 + bc * (HZ1 - HZ2) / 0x1000;

	/* Tän pitäisi olla ainoa kohta, jossa OPL_NoteOn:ia kutsutaan */
	if(OplLog)
	{
		FlushOplViive();
		fprintf(stderr, "b%X%X", a, (int)Herz);
	}
	
	MusData.driver->hztouch(a, Herz);

	/* Need not so much precision. This is only for AnalyzeRow().
	 */
	Herzi[a] = (int)Herz;
}

/* vole = RealVol*MainVol/127 */
static int SuggestNewChan(int instru, int vole)
{
	int a, c=MAXS3MCHAN, f;
    long b;

    for(a=f=0; a<MAXS3MCHAN; a++)
		if(Chan[a].LastInstru==instru)
            f=1;

	/* T„t„ voisi k„ytt„„ siihen, ett„ voimakkaammat    *
	 * „„net saavat suuremman painoarvon t„ss„ testiss„ */
	vole = vole;

	/* Arvostellaan kanavat */
	for(b=a=0; a<MAXS3MCHAN; a++)
	{
		/* Jos kanava on hiljaa, eli tyhj„ t„ll„ hetkell„ */
		if(!Chan[a].Volume)
		{
			long d;
			if(Confuse)
			{
				/* Ei kommentoida */
				d = rand();
				/* random-modessa v„ltet„„n yli 9:n menevi„
				 * kanavia kuitenkin (vaikka olisikin -l)
				 * if(a>8)d=1;
				 */
			}
			else
			{
				/* Pohjapisteet...
				 * Jos instru oli uusi, mieluiten sijoitetaan
				 * sellaiselle kanavalle, joka on pitk„„n ollut hiljaa.
				 * Muuten sille, mill„ se juuri „skenkin soi.
				 */
                d = f?1:Chan[a].Age;

                /* Jos kanavan edellinen instru oli joku
				 * soinniltaan hyvin lyhyt, pisteitä annetaan lisää */
				switch(Chan[a].LastInstru)
				{
					case 0x81: case 0x82: case 0x83: case 0x84:
					case 0x85: case 0x86: case 0x87: case 0x88:
					case 0x89: case 0x8B: case 0x8D: case 0x8E:
					case 0x90: case 0x94: case 0x96: case 0x9A:
					case 0x9B: case 0x9C: case 0x9D: case 0xA4:
					case 0xAA: case 0xAB:
						d += 12;
						break;
					default:
						/* Jos oli pitkäsointinen percussion, *
						 * annetaan pisteitä iän mukaan.      */
						if(Chan[a].LastInstru > 0x80)
    	                    d += Chan[a].Age*2;
                }

                /* Jos oli samaa instrua, pisteit„ tulee paljon lis„„ */
				if(Chan[a].LastInstru == instru)d += 3;

                //d = (d-1)*Chan[a].Age+1;
            }
            if(d > b)
            {
                b = d;
                c = a;
            }
        }
    }
    return c;
}

/* vole = RealVol*MainVol/127 */
static int ForceNewChan(int instru, int vole)
{
	int a, c;
    long b=0;

    vole *= 127;

    Forced=1;
    for(a=c=0; c<MAXS3MCHAN; c++)
        if(Chan[c].Age
          > b
          + (((instru<128 && Chan[c].Instru>128)
             || (vole > Chan[c].RealVol*Chan[c].MainVol)
             ) ? 1:0
          ) )
        {
            a=c;
			b=Chan[c].Age;
        }
	return a;
}

/* Used twice by SetNote. This should be considered as a macro. */
static void SubNoteOff(int a, int chn, int note)
{
	Chan[a].RealVol = 0;
	Chan[a].MainVol = 0;

	Chan[a].Age     = 0;
	Chan[a].Volume  = 0;
	Chan[a].BendFlag= 0;

	MIDI.Used[chn][note] = 0;

	if(OplLog)
	{
		FlushOplViive();
		fprintf(stderr, "o%X", a);
	}

	if(Chan[a].Instru < 0x80)
		MusData.driver->noteoff(a);
}

static void SetNote(int chn, int note, int RealVol,int MainVol, int bend)
{
	int a, vole=RealVol*MainVol/127;
    
	if(!vole && (bend==snNoteOn || bend==snNoteModify))bend=snNoteOff;
	if(bend==snNoteOn && MIDI.Used[chn][note])bend=snNoteModify;

#if 0
	printf("chn=%d, note=%d, RealVol=%d, MainVol=%d, action=", chn,note,RealVol,MainVol);
	if(bend==snNoteOn)printf("NoteOn");
	else if(bend==snNoteOff)printf("NoteOff");
	else if(bend==snNoteModify)printf("NoteModify");
	else printf("Bend %d", bend);
	printf("\n");
#endif

	switch(bend)
	{
		/* snNoteOn:ssa note ei koskaan ole -1 */
		case snNoteOn:
		{
			int p;

			/* Todellinen instru, joka aiotaan soittaa */
			p = chn==9 ? 128+note-35 : MIDI.Patch[chn];

            a = SuggestNewChan(p, vole);

			if(a==MAXS3MCHAN)
			{
				a = ForceNewChan(p, vole);
				MIDI.Used[Chan[a].uChn][Chan[a].uNote] = 0;
			}

			if(a < MAXS3MCHAN)
			{
				if(OplLog)
					FlushOplViive();

				Chan[a].RealVol= RealVol;
				Chan[a].MainVol= MainVol;
				Chan[a].Note   = chn==9 ? 60 : note;
				Chan[a].Volume = MakeVolume(vole);
                Chan[a].Age    = 0;
                Chan[a].Instru = p;

				if(OplLog && p != Chan[a].LastInstru)
				{
					InstruUsed[p]=1;
					fprintf(stderr, "p%X%X", a, p);
				}

				Chan[a].LastInstru = p;
				Chan[a].KeyFlag= 1;
				Chan[a].uChn  = chn;
                Chan[a].uNote = note;

				#if 0
				if(MIDI.Bend[chn] && Chan[a].Volume)
				{
					int adlbend = MIDI.Bend[chn] * 127L / 8000;

					/* TODO: Fix this. It doesn't work at all. */

					Chan[a].cmd = 'N'-64;
					Chan[a].info= adlbend+0x80; /* To make it unsigned */
				}
				else
				#endif
				if(MIDI.Pan[chn] != Chan[a].LastPan && Chan[a].Volume)
				{
					Chan[a].cmd = 'X'-64;
					Chan[a].info= MIDI.Pan[chn]*2;
					Chan[a].LastPan = MIDI.Pan[chn];
				}
				else
				{
					Chan[a].cmd = Chan[a].info = 0;
				}
				
				MusData.driver->noteoff(a);

				if(OplLog)
					fprintf(stderr, "v%X%X", a, OplVol?Chan[a].Volume:vole);
					
				MusData.driver->patch(a,
				  MusData.Instr[Chan[a].Instru]->D,
				  0,
				  0);
				
				if(MIDI.Pan[chn] != 64)
					MusData.driver->setpan(a, Chan[a].info - 128);
				
				MusData.driver->touch(a, 0, Chan[a].Volume);

				Bendi(chn, a);

				MIDI.Used[chn][note] = a+1;
			}
			break;
        }
        /* snNoteOff:ssa note voi olla -1 */
        case snNoteOff:
        {
            int b=note, c=note;
            if(note < 0)b=0, c=127;

			MIDI.Bend[chn] = 0; /* N„in vaikuttaisi olevan hyv„ */

            for(note=b; note<=c; note++)
            {
				a = MIDI.Used[chn][note];
                if(a > 0)
					SubNoteOff(a-1, chn, note);
            }
			break;
        }
		/* snNoteModify:ssa note ei koskaan ole -1 */
        case snNoteModify:
			a = MIDI.Used[chn][note]-1;
            if(a != -1)
			{
                Chan[a].RealVol= RealVol;
                Chan[a].MainVol= MainVol;

                Chan[a].Volume = MakeVolume(vole);
                Chan[a].Age    = 0;

                if(OplLog)
                {
                    FlushOplViive();
					fprintf(stderr, "v%X%X", a, OplVol?Chan[a].Volume:vole);
                }
                
                MusData.driver->touch(a, 0, Chan[a].Volume);
            }
            break;
        /* snNoteUpdate:ssa note on aina -1 */
        case snNoteUpdate:
            /* snNoteUpdatessa RealVol ei muutu, vain MainVol */
            for(note=0; note<=127; note++)
            {
				a = MIDI.Used[chn][note]-1;

				vole = MainVol*Chan[a].RealVol/127;

				if(a >= 0 && Chan[a].Volume != MakeVolume(vole))
				{
                    Chan[a].MainVol= MainVol;

                    if(!vole)
                        SubNoteOff(a, chn, note);
                    else
                    {
                        Chan[a].Volume = MakeVolume(vole);
                        Chan[a].Age    = 0;
                        if(OplLog)
						{
                            FlushOplViive();
							fprintf(stderr, "v%X%X", a, OplVol?Chan[a].Volume:vole);
                        }
                        
                        MusData.driver->touch(a, 0, Chan[a].Volume);
                    }
                }
			}
            break;
		/* Bendiss„ note on aina -1 */
        default:
            MIDI.Bend[chn] = bend;

            for(note=0; note<=127; note++)
            {
				a = MIDI.Used[chn][note]-1;
				if(a >= 0)
					Bendi(chn, a);
            }
    }
}

static DWORD GetVarLen(int Track, size_t *Posi)
{
    DWORD d = 0;
    for(;;)
    {
        BYTE b = MIDI.Tracks[Track][(*Posi)++];
        VERBOSE(4) printf("<%02X>", b);
        d = (d<<7) + (b&127);
        if(b < 128)break;
    }
    return d;
}

/* NOTICE: This copies len-2 bytes */
static const char *Sana(int len, const unsigned char *buf)
{
	static char Buf[128];
	char *s = Buf;
	len -= 2;
	#define add(c) if(s<&Buf[sizeof(Buf)-1])*s++=(c)
	while(len>0)
	{
		if(*buf<32||(*buf>=127&&*buf<160)){add('^');add(*buf + 64);}
		else add(*buf);
		buf++;
		len--;
	}
	#undef add
	*s=0;
	return Buf;
}

static void Tavuja(int Track, BYTE First, size_t posi, size_t Len)
{
	VERBOSE(4)
    {
        size_t a;
        printf("%d: %02X", Track, First);
        for(a=0; a<Len; a++)
			printf(" %02X", MIDI.Tracks[Track][a+posi]);
        printf("\n"), LinesQ++;
	}

    if(First >= 0xF0)
        switch(First)
        {
            case 0xFF:
            {
                BYTE typi = MIDI.Tracks[Track][posi];
                posi += 2;
                switch(typi)
                {
                    case 0:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(": Sequence number: %u\n",
                            	ConvI(MIDI.Tracks[Track]+posi));
                        break;
                    case 1:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(": %s\n", Sana(Len,MIDI.Tracks[Track]+posi));
                        break;
                    case 2:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(" (C): %s\n", Sana(Len,MIDI.Tracks[Track]+posi));
                        break;
                    case 3:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(" text: %s\n", Sana(Len,MIDI.Tracks[Track]+posi));
                        break;
                    case 4:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(" instru: %s\n", Sana(Len,MIDI.Tracks[Track]+posi));
                        break;
                    case 5:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(" lyric: %s\n", Sana(Len,MIDI.Tracks[Track]+posi));
                        break;
                    case 6:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(" marker: %s\n", Sana(Len,MIDI.Tracks[Track]+posi));
                        break;
                    case 0x2F:
                        MIDI.Posi[Track] = MIDI.TrackLen[Track];
						VERBOSE(3) printf("Track %d", Track),
                        	printf(" *END*\n");
                        break;
					case 0x51:
						MIDI.Tempo =
                            MIDI.Tracks[Track][posi+0] * 0x10000L
                          + MIDI.Tracks[Track][posi+1] * 0x100
                          + MIDI.Tracks[Track][posi+2];
						VERBOSE(2) printf("Track %d", Track),
                        	printf(" tempo: %lu\n", (unsigned long)MIDI.Tempo);
                        break;
					case 0x58:
                        VERBOSE(2) printf("Track %d", Track),
                        	printf(" time signature: %d/%d"
								"\nTicks/mn-click: %d "
								"\n32nd-notes per 1/4: %d\n",
                                MIDI.Tracks[Track][posi+0],
                                MIDI.Tracks[Track][posi+1],
                                MIDI.Tracks[Track][posi+2],
                                MIDI.Tracks[Track][posi+3]);
                        break;
                    default:
						VERBOSE(3)
                        {
							printf("Track %d", Track),
                        	printf(" - unknown metadata %02Xh ", typi);
                            for(typi=0; typi<Len-1; typi++)
								printf("%02Xh ", MIDI.Tracks[Track][posi+typi]);
                            printf("\n");
                        }
                }
                LinesQ++;
                break;
            }
        }
    else
    {
		size_t a;
		if(devtold)
		{
			a=0;
			if((First>>4) == 8) /* Note off, modify it a bit */
			{
			#if 1
				MusData.driver->midibyte(First);
				MusData.driver->midibyte(MIDI.Tracks[Track][posi]);
				MusData.driver->midibyte(0);
			#endif
				Len -= 2;
			}
			else
			{
			#if 1
				MusData.driver->midibyte(First);
			#endif
			}
			
			#if 1			
			for(; a<Len; a++)
				MusData.driver->midibyte(MIDI.Tracks[Track][a+posi]);
			#endif
        }
        switch(First >> 4)
        {
            case 0x8: /* Note off */
                SetNote(
					First&15,                   /* chn */
					MIDI.Tracks[Track][posi+0], /* note */
                    MIDI.Tracks[Track][posi+1], /* volume */
					MIDI.MainVol[First&15], /* mainvol */
                    snNoteOff);
                break;
            case 0x9: /* Note on */
                #if 0
				if((First&15) == 9)
                {
					MIDI.Patch[9] = MIDI.Tracks[Track][posi+0] + 128;
                    SetNote(
						First&15,                   /* chn */
                        60,                         /* note */
                        MIDI.Tracks[Track][posi+1], /* volume */
                        snNoteOn);
                }
                else
                #endif
                SetNote(
					First&15,                   /* chn */
                    MIDI.Tracks[Track][posi+0], /* note */
                    MIDI.Tracks[Track][posi+1], /* volume */
					MIDI.MainVol[First&15], /* mainvol */
                    snNoteOn);
                break;
            case 0xA: /* Key after-touch */
                SetNote(
					First&15,                   /* chn */
                    MIDI.Tracks[Track][posi+0], /* note */
                    MIDI.Tracks[Track][posi+1], /* volume */
					MIDI.MainVol[First&15], /* mainvol */
                    snNoteModify);
                break;
            case 0xC: /* Patch change */
				MIDI.Patch[First&15] = MIDI.Tracks[Track][posi];
                break;
			case 0xD: /* Channel after-touch */
                /* ?? */
				break;
            case 0xE: /* Wheel - 0x2000 = no change */
                a = MIDI.Tracks[Track][posi+0] +
                    MIDI.Tracks[Track][posi+1] * 128;
				SetNote(First&15,
                    -1, 0,0,
					(int)((long)a*MIDI.PitchSense[First&15]/2 - 0x2000L));
				break;
            case 0xB: /* Controller change */
                switch(MIDI.Tracks[Track][posi+0])
                {
                    case 123: /* All notes off on channel */
						SetNote(First&15, -1,0,0, snNoteOff);
                        break;
                    case 121: /* Reset vibrato and bending */
						MIDI.PitchSense[First&15] = 2;
						SetNote(First&15, -1,0,0, 0); /* last 0=bend 0 */
                        break;
                    case 7:   /* Main volume, not yet supported */
						MIDI.MainVol[First&15] = MIDI.Tracks[Track][posi+1];

                        /* TOUCH ALL CHANNELS HERE */
                        SetNote(
							First&15,                   /* chn */
                            -1,                         /* note */
                            0,                          /* volume */
							MIDI.MainVol[First&15], /* mainvol */
                            snNoteUpdate);

                        break;
                    case 64:  /* Sustain pedal, not yet supported */
                    case 91:  /* Reverb depth, not yet supported */
                    case 93:  /* Chorus depth, not yet supported */
                    case 1:   /* Vibrato, not yet supported */
                        break;
                    case 6:   /* Pitch bender sensitivity */
						MIDI.PitchSense[First&15] = MIDI.Tracks[Track][posi+1];
                        break;
					case 10:  /* Pan */
						MIDI.Pan[First&15] = MIDI.Tracks[Track][posi+1];
                        break;
                    case 0:
                    	VERBOSE(4) printf("Bank select: %d (unemulated)\n",
                    		MIDI.Tracks[Track][posi+1]), LinesQ++;
                    	break;
                    default:
						VERBOSE(4) printf("Unknown controller #%d new value: %d\n",
							MIDI.Tracks[Track][posi+0],
							MIDI.Tracks[Track][posi+1]), LinesQ++;
                }
				break;

        }
    }
}

/* bugtracking - kolmas calloc hajotti kun oli fclosea tuplasti
#define calloc Calloc
static void *calloc(unsigned a, unsigned b)
{
	void *tmp;
	printf("calloc(%u,%u) -> ", a, b); fflush(stdout);
	tmp = malloc(a*b);
	printf("%p\n", tmp); fflush(stdout);
	if(tmp)memset(tmp, 0, a*b);
	return tmp;
}
*/

/* Return value: 0=ok, -1=user break */
static int PlayMIDI(void)
{
	int a, NotFirst, Userbreak=0;
	long Viivetta;
	int NoDelay=0;

	VERBOSE(3) printf("%s: Initializing player\n", A0);
	
	MusData.driver->midictrl(0,255,255);

    if(!(MIDI.Waiting = (DWORD  *)calloc(MIDI.TrackCount, sizeof(DWORD)))
	|| !(MIDI.sWaiting= (DWORD  *)calloc(MIDI.TrackCount, sizeof(DWORD)))
	|| !(MIDI.SWaiting= (DWORD  *)calloc(MIDI.TrackCount, sizeof(DWORD)))
	|| !(MIDI.Posi    = (size_t *)calloc(MIDI.TrackCount, sizeof(size_t)))
	|| !(MIDI.sPosi   = (size_t *)calloc(MIDI.TrackCount, sizeof(size_t)))
	|| !(MIDI.SPosi   = (size_t *)calloc(MIDI.TrackCount, sizeof(size_t)))
    || !(MIDI.Running = (BYTE   *)calloc(MIDI.TrackCount, sizeof(BYTE)))
    || !(MIDI.sRunning= (BYTE   *)calloc(MIDI.TrackCount, sizeof(BYTE)))
    || !(MIDI.SRunning= (BYTE   *)calloc(MIDI.TrackCount, sizeof(BYTE)))
    )Die(ENOMEM);

	VERBOSE(4) printf("%s: Preparing sWaiting and sPosi\n", A0);

    for(a=0; a<MIDI.TrackCount; a++)
    {
        size_t c = 0;
		MIDI.sWaiting[a]= GetVarLen(a, &c);
		MIDI.Running[a] = 0;
		MIDI.sPosi[a]   = c;
	}

	for(a=0; a<16; a++)
	{
		MIDI.Pan[a]        = 64;   /* Middle      */
		MIDI.Patch[a]      = 1;    /* Piano       */
		MIDI.PitchSense[a] = 2;    /* ñ seminotes */
		MIDI.MainVol[a]    = 127;
		MIDI.Bend[a]       = 0;
	}

	NotFirst = 0;
ReLoop:
	for(a=0; a<MIDI.TrackCount; a++)
	{
		MIDI.Posi[a]    = MIDI.sPosi[a];
		MIDI.Waiting[a] = MIDI.sWaiting[a];
		MIDI.Running[a] = MIDI.sRunning[a];
	}

	MIDI.Tempo = MIDI.initempo;
	
	memset(&Chan, 0, sizeof Chan);

	memset(MIDI.Used,    0, sizeof MIDI.Used);

    Viivetta = 0;

	VERBOSE(1) printf("%s: %sing.\n", A0, devtold?"Play":"Analyz");
 
	for(;;)
    {
        int Fin, Act;
        
        /*
		{
		  int c;
		  printf("POSI\tWAIT\n");
		  for(c=0; c<MIDI.TrackCount; ++c)
		  {
		      printf("%u\t%lu\n", MIDI.Posi[c], (unsigned long)MIDI.Waiting[c]);
		  }
		  printf("\n\n\n\n");
		}
		*/

        if(NotFirst)
        {
			if(devtold)
            {
				long Lisa = MIDI.Tempo/MIDI.DeltaTicks;

                /* tempo      = microseconds per quarter note
                 * deltaticks = number of delta-time ticks per quarter note
                 *
                 * So, when tempo = 200000
                 * and deltaticks = 10,
                 * then 10 ticks have 200000 microseconds.
                 * 20 ticks have 400000 microseconds.
                 * When deltaticks = 5,
                 * then 10 ticks have 40000 microseconds.
                 */
             
				if(OplLog)OplViive+=Lisa;
             
                Viivetta += Lisa;
				if(Viivetta >= MINDELAY)
                {
                    if(!NoDelay)
                        #if defined(__BORLANDC__) || defined(DJGPP)
						delay(Viivetta / MINDELAY);
                        #else
                        usleep(Viivetta * 1000 / MINDELAY);
                        #endif
                    Viivetta %= MINDELAY;
				}
			}
			AnalyzeRow();
		}
        else
        {
            DWORD b = 0xFFFFFFFFUL;
            /* Etsi pienin viive */
            for(a=0; a<MIDI.TrackCount; a++)
				if(MIDI.Waiting[a] < b)
                    b = MIDI.Waiting[a];
            /* Kuluta se viive kaikilta trakeilta */
			for(a=0; a<MIDI.TrackCount; a++)
                MIDI.Waiting[a] -= b;
        }

        /* Let the notes on channels become older (Age++) only if  *
         * something happens. This way, we don't overflow the ages *
         * too easily.                                             */
        for(Act=a=0; a<MAXS3MCHAN; a++)
            if(MIDI.Waiting[a]<=1 && MIDI.Posi[a]<MIDI.TrackLen[a])
                Act++;
		for(a=0; a<MAXS3MCHAN; a++)
			if(!Chan[a].Age||Act!=0)
				Chan[a].Age++;

        for(a=0; a<MIDI.TrackCount; ++a)
        {
        	MIDI.SPosi[a]    = MIDI.Posi[a];
        	MIDI.SWaiting[a] = MIDI.Waiting[a];
        	MIDI.SRunning[a] = MIDI.Running[a];
        }
        for(Fin=1, a=0; a<MIDI.TrackCount; a++)
        {
            if(MIDI.Waiting[a] > 0)MIDI.Waiting[a]--;

            if(MIDI.Posi[a] < MIDI.TrackLen[a])Fin=0;

            while(MIDI.Waiting[a]<=0 && MIDI.Posi[a]<MIDI.TrackLen[a])
            {
                size_t pos;
                BYTE b = MIDI.Tracks[a][MIDI.Posi[a]];
                if(b < 128)
                    b = MIDI.Running[a];
                else
                {
					MIDI.Posi[a]++;
                    if(b < 0xF0)MIDI.Running[a] = b;
                }

                pos = MIDI.Posi[a];

				if(b == 0xFF)
                {
                    int ls=0;
					size_t len;
                    BYTE typi = MIDI.Tracks[a][MIDI.Posi[a]++];
                    len = (size_t)GetVarLen(a, &MIDI.Posi[a]);
                    if(typi == 6) /* marker */
                        if(!strncmp((char *)(MIDI.Tracks[a]+MIDI.Posi[a]),
									"loopStart", len))
                        {
							printf("Found loopStart\n"), LinesQ++;;
							ls=1;
                        }
                    MIDI.Posi[a] += len;
					if(ls)goto SaveLoopStart;
                }
                else if(b==0xF7 || b==0xF0)
                {
                    MIDI.Posi[a] += (size_t)GetVarLen(a, &MIDI.Posi[a]);
                }
                else if(b == 0xF3)MIDI.Posi[a]++;
                else if(b == 0xF2)MIDI.Posi[a]+=2;
                else if(b>=0xC0 && b<=0xDF)MIDI.Posi[a]++;
                else if(b>=0x80 && b<=0xEF)
                {
                    MIDI.Posi[a]+=2;
                    if(b>=0x90 && b<=0x9F && !NotFirst)
                    {
                        int c;
SaveLoopStart:          NotFirst=1;
						/* Save the starting position for looping */
						for(c=0; c<MIDI.TrackCount; c++)
						{
							MIDI.sPosi[c]    = MIDI.SPosi[c];
							MIDI.sWaiting[c] = MIDI.SWaiting[c];
							MIDI.sRunning[c] = MIDI.SRunning[c];
						}
						MIDI.initempo = MIDI.Tempo;
					}
                    #if 0
                    if(b>=0x80 && b<=0x8F)
                    {
                        /* J„yn„ */
                        int c;
						MIDI.Tracks[a][MIDI.Posi[a]-1] = 0;
                    }
                    #endif
                }

				Tavuja(a, b, pos, MIDI.Posi[a]-pos);

                if(MIDI.Posi[a] < MIDI.TrackLen[a])
                    MIDI.Waiting[a] += GetVarLen(a, &MIDI.Posi[a]);
			}
        }
        if(Fin)
        {
			if(devtold)
			{
                int a;
                printf("**Loop**\n"), LinesQ++;;
				if(!OplLog)goto ReLoop;
                FlushOplViive();
				fprintf(stderr, "z\n");
                for(a=0; a<256; a++)
					if(InstruUsed[a])
						fprintf(stderr, "Used instru G%c%d (%Xh)\n", a>127?'P':'M', (a&127)+(a>127?35:0), a);
            }
            break;
        }
        if(kbhit())
        {
            if(getch()=='!')
                NoDelay=!NoDelay;
            else
            {
                while(kbhit())getch();
                printf("Esc=Resume, Q=Quit\n>");
                for(;;)
                {
					a=getch();
					if(a=='Q' || a=='q')
					{
						printf("%c\n", a);
						Userbreak=-1;
						goto Breakki;
					}
					if(a==27)break;
				}
				printf("\n"), LinesQ+=2;
		}   }
	}
Breakki:
	VERBOSE(1) printf("%s: Finished %sing.\n", A0, devtold?"play":"analyz");
	return Userbreak;
}

static void ReadMIDI(void)
{
	BYTE Signa[4];
	int a;

	FILE *fp = fopen(infn, "rb");
	if(!fp)
	{
		fprintf(stderr, "%s: Could not open the input file, '%s' for reading.\n", A0, infn);
		Die(errno);
	}

	for(;;)
	{
		fread(&Signa, 1, 4, fp);
		if(memcmp(Signa, "RIFF", 4))break;
		fseek(fp, 16, SEEK_CUR);
	}
	if(memcmp(Signa, "MThd", 4))
	{
FmtError:
		fclose(fp);
        fprintf(stderr, "%s: Could not read the input file, '%s'.\n", A0, infn);
        Die(EINVFMT);
	}

    fread(&Signa, 1, 4, fp);
	if(ConvL(Signa) != 6)goto FmtError;

    fread(&Signa, 1, 2, fp);
    MIDI.Fmt = ConvI(Signa);

	VERBOSE(2) printf("%s: '%s' has format #%d\n", A0,infn,MIDI.Fmt);

    fread(&Signa, 1, 2, fp);
    MIDI.TrackCount = ConvI(Signa);

    fread(&Signa, 1, 2, fp);
	MIDI.DeltaTicks = ConvI(Signa);

	VERBOSE(2) printf("%s: TrackCount %d, DeltaTicks %d\n",
        A0, MIDI.TrackCount, MIDI.DeltaTicks);

	VERBOSE(1) printf("%s: Reading '%s'\n", A0, infn);

    MIDI.TrackLen = (size_t *)calloc(MIDI.TrackCount, sizeof(size_t));
    if(!MIDI.TrackLen)
    {
MemError:
        fclose(fp);
        Die(ENOMEM);
	}
	MIDI.Tracks = (BYTE **)calloc(MIDI.TrackCount, sizeof(BYTE *));
    if(!MIDI.Tracks)goto MemError;

    for(a=0; a<MIDI.TrackCount; a++)
    {
        DWORD tmp;

        fread(&Signa, 1, 4, fp);
		if(memcmp(Signa, "MTrk", 4))goto FmtError;

		fread(&Signa, 1, 4, fp);
        tmp = ConvL(Signa);
        if(tmp > ((size_t)-1))
        {
            printf("%s: Track %d too long (%lu bytes). Skipping.\n", A0, a+1, (unsigned long)tmp);
            MIDI.TrackLen[a] = 0;
        }
		else
        {
            MIDI.TrackLen[a] = (size_t)tmp;
			VERBOSE(3) printf("%s: Track %d is %lu bytes long\n", A0, a+1, (unsigned long)tmp);
        }

        MIDI.Tracks[a] = (BYTE *)calloc(MIDI.TrackLen[a],1);
        if(!MIDI.Tracks[a])goto MemError;

        fread(MIDI.Tracks[a], 1, MIDI.TrackLen[a], fp);
        fseek(fp, tmp-MIDI.TrackLen[a], SEEK_CUR);
        /* Skip over the not read part */
    }

    VERBOSE(1) printf("%s: Closing '%s'\n", A0, infn);

    fclose(fp);
    
	MIDI.initempo = 150000;

	switch(MIDI.Fmt)
    {
        case 0:
        case 1:
		case 2:
            VERBOSE(3) printf("%s: Tracks will be handled simultaneously.\n", A0);
    }
}

static void ReadInstrus(void)
{
	int m, i;
	InternalSample *t;	

	m = adldatasize / 14;

	VERBOSE(3) printf("%s: Loading adlib patches...\n", A0);
	
	t = calloc(m, sizeof(InternalSample));

	for(i=0; i<m; i++)
	{
		InternalSample *Tmp = t + i;
		if(!Tmp)Die(ENOMEM);

		Tmp->D[0] = adl[i*14 + 3];
		Tmp->D[1] = adl[i*14 + 9];
		Tmp->D[2] = adl[i*14 + 4];
		Tmp->D[3] = adl[i*14 +10];
		Tmp->D[4] = adl[i*14 + 5];
		Tmp->D[5] = adl[i*14 +11];
		Tmp->D[6] = adl[i*14 + 6];
		Tmp->D[7] = adl[i*14 +12];
		Tmp->D[8] = adl[i*14 + 7] & 3;
		Tmp->D[9] = adl[i*14 +13] & 3;
		Tmp->D[10]= adl[i*14 + 8];
		MusData.Instr[i] = Tmp;
	}
}

static void GetScreenWidth(void)
{
#if defined(__BORLANDC__) || defined(DJGPP)
    struct text_info buf;
	gettextinfo(&buf);
	COLS = buf.screenwidth;
	LINES= buf.screenheight;
#else
	struct winsize ws;        /* buffer for TIOCSWINSZ */
	COLS = 80;
	LINES= 24;
	if(!ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
	{
		LINES = ws.ws_row;
		COLS  = ws.ws_col;
	}
#endif
}

#ifdef __BORLANDC__
static void ErrSign(int dummy)
{
	fprintf(stderr, "%s\n",
		dummy==SIGFPE?"FPE":
		dummy==SIGSEGV?"Segmentation fault":
		"Something strange happened. Exit.");
}
#endif

static void ExitFmdrv(void)
{
	MusData.driver->close();
}

#include "devopts.inc"
int main(int argc, const char *const *argv)
{
	int dev = 'o';
	int SkipWidth=0;

	#if !defined(__BORLANDC__) && !defined(DJGPP)
	InitCons();
	atexit(DoneCons);
	#endif

#ifdef __BORLANDC__
	signal(SIGFPE, ErrSign);
	signal(SIGSEGV,ErrSign);
#endif

	for(; --argc; )
	{
		const char *a = *++argv;
		if(*a=='-'
		#if defined(__BORLANDC__)||defined(DJGPP)
		|| *a=='/'
		#endif
		)
		{
			while(*++a)
			{
				const char *t = a;
				if(finddriver(*a))
				{
					++devtold;
					dev = *a;
					continue;
				}
                switch(*a)
                {
                    case 'v': Verbose++; break;
					case 'c': Confuse=1; break;
                    case 's': ShortFixes=0; break;
                    case 'r':
                        switch(*++a)
                        {
							case 'a': Brainless=1; break;
							case 'i': ForceIns=1; break;
							case 's': SkipWidth=1; break;
							case 'h': Herzit=1; break;
							case 'l': OplVol=0; OplLog=1; break;
							case 'L': OplVol=1; OplLog=1; break;
							case 'p': PrintPats=0; break;
							default:
								goto ArgError;
						}
						break;
					case 'q':
						dup2(open(NULFILE, O_WRONLY), ++Quiet);
						break;
					case 'l':
						MAXS3MCHAN = (int)strtol(++a, (char **)&a, 10); a--;
						if(MAXS3MCHAN<1 || MAXS3MCHAN>MAXCHN)
						{
							printf("%s: -l%d out of ranges.\n", A0,MAXS3MCHAN);
							MAXS3MCHAN=MAXS3MCHAN<1?1:MAXCHN;
						}
						break;
					case '?':
					case 'h':
						printf(
							"FMDRV v"VERSION" - Copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/)\n"
							"Plays MIDI files and converts them into the MidiS3M format\n\n"
							"Usage:\t%s [<options>] <midifile> [<s3mfile>]\n"
							"\t-v\tVerbosity++\n"
							"\t-s\tDisable optimizing short retrigs\n"
							"\t-q\tDecrease text output (-qq = not even error messages)\n"
							"\t-ra\tFull patterns without optimizations (brainless mode)\n"
							"\t-ri\tForce instrument number displaying with exceptions\n"
							"\t-rs\tSkip screen width detection - use infinite width\n"
							"\t-rh\tDisplay herz instead of note\n"
							"\t-rl\tLog opl action (coded) to stderr, don't loop\n"
							"\t-rL\tAs -rl, but with opl volumes\n"
							"\t-rp\tNot print patterns\n"
							"\t-l\tSpecify max number of adlib channels (default %d)\n"
							"\t-c\tDon't tidy patterns, mix/garble them instead\n", A0, MAXS3MCHAN);
						devoptlist();
						printf(
							"If none of -o,-d,-m,-a was specified, will create a MidiS3M file\n"
							"named <s3mfile> from the <midifile>.\n");
                        return 0;
                    default:
		ArgError:       printf("%s: Skipping unknown switch `-", A0);
                        while(t!=a)printf("%c", *t++);
                        printf("'\n");
                }
            }
        }
		else if(!infn)infn = a;
		else if(!outfn)outfn = a;
        else
            printf("%s: %sthird filename, '%s'\n",
                A0,
                "I don't know what to do with the ",
                a);
    }

    if(!infn)
	{
        printf("%s: I don't know what to do without the input filename.\nTry %s -?\n", A0,A0);
        return EXIT_FAILURE;
    }

	if(devtold)
    {
		if(outfn)
			printf("%s: %soutput filename, '%s', when the purpose is to play the input file.\n",
                A0,
                "I don't know what to do with the ",
				outfn);
	}
    else
		if(!outfn)
		{
            printf("%s: Output filename was not specified, playing the input file.\n", A0);
			devtold = 1;
        }

	ReadInstrus();

	GetScreenWidth();
	if(SkipWidth)COLS=MAXINT;

	if(devtold)
	{
		struct MusDriver *tmp = finddriver(dev);
		if(!tmp || tmp->detect() < 0)
		{
			fprintf(stderr, "Not found device!\n");
			return 0;
		}
		VERBOSE(3) printf("Found: %s\n%s\n", tmp->name, tmp->longdescr);
		else VERBOSE(2) printf("Found device.\n");
		
		MusData.driver = tmp;
		tmp->reset();

		atexit(ExitFmdrv);

		InitChanDisp(); /* After GetScreenWidth(), thanks */
	}

	ReadMIDI();

	if(!devtold)
	{
		if(!(tempf = fopen(tempfn, "wb+")))
		{
			fprintf(stderr, "%s: Could not open the tempfile, '%s' for writing.\n", A0, tempfn);
			Die(errno);
		}
		VERBOSE(1) printf("%s: Writing '%s'\n", A0, tempfn);
	}

	if(devtold)
		PlayMIDI();
	else
	{
		/* Not writeout if user break */
		if(!PlayMIDI())
			WriteOut();
	}

	return 0;
}
