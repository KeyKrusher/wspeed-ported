#include <ctype.h>
#include <stdlib.h>
#include <process.h>
#include <dos.h>
#include <io.h>

#ifdef __GNUC__
#include <signal.h>
#include <crt0.h>
#endif

#include "wspeed.h"
#include "musa04.c"

extern char Version[];

int SANAVIIVE = 30;
int TAHTIVIIVE = 4;

int Dvorak=0;
int NoDelay=0;     		// Viiveet”n peli (huh)
int NoCollision=1;		// Ei samoja sanoja
int RequireBetter=20;	// Peli vaikeutuu koko ajan, paljonko
int Verbose=0;
int Training=0;         // Training mode -> no score.

char far *GameMusic;

void InitVideo(void)
{
#ifdef   __BORLANDC__
	if(*((unsigned char *)0x484) != 25-1 /* Jos ei 25-rivinen tila... */
	|| *((unsigned char *)0x449) != 3)   /* Tai ei edes tekstitila */
	{
		/* Niin aseta sellainen. */
		textmode(C80);
	}
	/*_asm {
		// set blink-attr off
		mov ax, 0x1003
		xor bx, bx
		int 0x10
	}*/
#endif
	putch('\r');
}

void CleanUp(int err)
{
	/*
	_asm {
		// re-enable blink-attr
		mov ax, 0x1003
		mov bx, 1
		int 0x10
	}
	*/
	textattr(7);
    ExitMusic();
	exit(err);
}

#ifdef __BORLANDC__
static int CtrlBreakHandler(void)
{
	return 1; /* Nonzero */
}
#else
void CtrlCHandler(int dummy)
{
	signal(SIGINT, CtrlCHandler);
}
#endif

int main(int argc, const char *const *argv)
{
	int a, CardSelected;
	const char *s;
	static const char *MusicFile = "wspeed.c";
	static const char *scorecom = "SCORECOM.EXE";

	extern char scorename[];
	extern char mixscorename[];
	static char newscorename[] = "wspeed.ret";

    scorecom = Pathi("PATH", scorecom);

    #ifdef __GNUC__
    if(!(_crt0_startup_flags & _CRT0_FLAG_NEARPTR))
        if(__djgpp_nearptr_enable() == 0)
        {
            printf("Fatal error\n");
        }
    #endif

	for(a=1; a<argc; a++)
	{
		s = argv[a];
		if(*s=='-' || *s=='/')
		{
			while(*s=='-' || *s=='/')s++;
			while(*s)
			{
				switch(toupper(*s))
				{
					/* The rest of parameters are being handled *
					 * at below  -  this is just for -V flag.   */
					case 'V': Verbose++; break;
					case 'M':
						do s++; while(*s==' ' || *s=='\t');
						if(!*s){if(++a==argc)goto P1;s=argv[a];}
						s=""; // -> break
						break;
					case 'Q':
					case 'W':
						do s++; while(*s==' ' || *s=='\t');
						if(!*s){if(++a==argc)goto P1;s=argv[a];}
						while(isdigit(*s))*s++;
				}
				if(*s)s++;
			}
		}
	}

	if(!access(newscorename, 4))
	{
		printf("Preparing to run WSpeed the first time...\n");
		if(access(scorename, 4))
			rename(newscorename, scorename);
		else if(access("scorecom.exe", 5))
		{
			printf("WSpeed: Error: ScoreCom not found. Stop.\n");
			exit(-1);
		}
		else
		{
			if(spawnl(P_WAIT,
				(char *)scorecom, "wspeed", Verbose?"-v":"(C)",
				newscorename, scorename, NULL))
			{
				perror(scorecom);
				exit(errno);
			}
			if(access(mixscorename, 4))
			{
				printf("WSpeed: Error: ScoreCom failed. Stop.\n");
				exit(-1);
			}
			if(unlink(scorename))
			{
				perror(scorename);
				exit(errno);
			}
			printf("Replacing %s with %s...\n", scorename, mixscorename);
			rename(mixscorename, scorename);
			if(unlink(newscorename))
			{
				perror(newscorename);
				exit(errno);
			}
		}
		printf("Done.\n");
	}

	InitVideo();

    InitMusic(NO_AUTODETECT);
	Rate = 200;
	/* Rate 1000 is best for accurate delays,
	 * Rate 200 may be better for slower computers... */

    MusData.Paused=1;
	ChorusLevel = 30*127/100;
	ReverbLevel = 40*127/100;
	__ResumeS3M(); // We need a working Delay() from beginning

	#ifdef __BORLANDC__
	ctrlbrk(CtrlBreakHandler); /* ctrlbrk() is in dos.h */
	#else
	signal(SIGINT, CtrlCHandler);
	#endif

    srand((unsigned)time(NULL));

	for(a=1; a<argc; a++)
	{
		s = argv[a];

		if(*s=='-' || *s=='/')
		{
			while(*s=='-' || *s=='/')s++;
			while(*s)
			{
				switch(toupper(*s))
				{
					case '?':
					case 'H':
P1:						printf(
							"WSPEED version %s\n\n"
							"Plays the WSpeed game - see wspeed.txt.\n\n"
							"Usage: WSpeed [options]\n"
							"\nOptions:\n"
							"   -c\t\tEnables same words to appear twice at same time.\n"
							"\t\tThis option makes the game a lot easier... I did not invent a\n"
							"\t\tgood way to grade the users of -c... So now it gives no score.\n"
							"   -d\t\tDvorak mode. (For Finnish keyboards).\n"
							"   -m file\tSelects music file (MidiS3M,C) - requires intgen.exe\n"
							"\t\tDefault: %s\n"
							"   -q num\tSpecify difficulty level.\n"
							"\t\tDefault: Level %d. Notice: Specifying 0 here may make you able\n"
							"\t\tto play this forever.., which would not be very developing.\n"
							"   -w num\tInitial brakes. Recommended value 3..50\n"
							"\t\tDefault: %d. Smaller number -> words move faster.\n"
							"   -v\t\tLess unverbose initialization\n",
							Version, MusicFile,
							RequireBetter, SANAVIIVE
						);
						CleanUp(0);
					/* Handled in the beginning
					case 'V':
						Verbose++;
						break;
					*/
					case 'D':
						Dvorak=1;
						break;
					case 'C':
						NoCollision=0;
						Training=1;
						break;
					case 'Q':
						do s++; while(*s==' ' || *s=='\t');
						if(!*s){if(++a==argc)goto P1;s=argv[a];}
						RequireBetter=0;
						while(isdigit(*s))RequireBetter=RequireBetter*10+*s++-'0';
						if(RequireBetter>99)RequireBetter=99;
						if(RequireBetter<0)RequireBetter=0;
						break;
					case 'M':
						do s++; while(*s==' ' || *s=='\t');
						if(!*s){if(++a==argc)goto P1;s=argv[a];}
						MusicFile = s;
						s=""; // -> break
						break;
					case 'W':
						do s++; while(*s==' ' || *s=='\t');
						if(!*s){if(++a==argc)goto P1;s=argv[a];}
						SANAVIIVE=0;
						while(isdigit(*s))SANAVIIVE=SANAVIIVE*10+*s++-'0';
						if(SANAVIIVE>999)SANAVIIVE=999;
						if(SANAVIIVE<0)SANAVIIVE=0;
					case 'V': // Already handled.
						break;
					default:
						goto P1;
				}
				if(*s)s++;
			}
		}
		else
			goto P1;
	}

	for(CardSelected=0, a=1; a; CardSelected=1)
	{
		StarDir = -1;
		if(!CardSelected)
			if(DoSelectCard() < 0)
			{
				gotoxy(1, 25);
				CleanUp(0);
			}

		StartS3M(adl_start);
		StarDir = 1;
		SelectDicts();

		if(Verbose)
			for(a=0; a<80; a++)putch('-');

		PrepareDicts();

		if(Verbose)printf("%s\n", SoundError);

		GameMusic = LoadMusicFile(MusicFile);
		if(!GameMusic)CleanUp(errno);

		if(Verbose)
		{
			for(a=0; a<80; a++)putch('-');
			printf("Press any key to start...");
            if(!WS_getch())WS_getch();
		}

		a = GameLoop();
		ReleaseDicts();
	}
	CleanUp(0);
	return 0;
}
