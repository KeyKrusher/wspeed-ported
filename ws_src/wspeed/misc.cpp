#include <io.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#ifdef __GNUC__
#include <go32.h>
#endif

#include "wspeed.h"

// Sanastot
char DictFiles[25-4];
int DictSel[sizeof DictFiles];
int DictCount;

unsigned char huge *sanau; // Sanaa k„ytetty

static char * huge *Sanat; // Sanat.

int sanac; // Sanaston koko
int used;  // Sanoja sanakirjassa

char sb[512];  // Public general mahd. lyhytaikainen temp. bufferi

char *meminuse = NULL;
const char *pascal Pathi(char *env, const char *a)
{
	char buf[512];
	char buf2[128];
	char *p, *s, *path;
	int i;

	strcpy(buf, env);
	strupr(buf);
	path = getenv(buf);

	if(!path
	|| !access(a, 4)
	|| strlen(path) >= sizeof(buf))P1:return a;
	for(strcpy(p=buf, path);;)
	{
		s = strchr(p, ';');
		if(s)*s=0;

		strcpy(buf2, p);
		i=strlen(buf2);
		if(i)
		{
			if(buf2[i-1]!='\\')strcat(buf2, "\\");
			strcat(buf2, a);
			if(!access(buf2, 4))
			{
				s = meminuse = (char *)malloc(strlen(buf2)+1);
				return strcpy(s, buf2);
			}
		}
		if(!s)goto P1;
		p=s+1;
	}
}

char *LoadMusicFile(const char *MusicFile)
{
	static const char *intgen = "INTGEN.EXE";
	char *MainMusic;
	unsigned short size;
	char v[2];
	FILE *fp;
	int a;

	printf("\n");

	MusicFile = Pathi("MOD", MusicFile);
	intgen    = Pathi("PATH",intgen);
	v[0] = Verbose+'0';
	v[1] = 0;
	remove("nesmusa.$$$");
	a = spawnl(P_WAIT, (char *)intgen,
			   "wspeed", "<2",
			   (char *)MusicFile, v, NULL);
	if(a < 0)
	{
IntGenSpawnError:
		printf("spawn(%s)) error: %s", intgen, strerror(errno));
		return NULL;
	}
	if(a) /* Invaliidi formaatti? Kokeillaan toisella tavalla. */
	{
		a = spawnl(P_WAIT, (char *)intgen, "wspeed",
				   Verbose?"-nv":"-nb",
				   (char *)MusicFile, "$", "$", NULL);
		if(a < 0)goto IntGenSpawnError;
	}
	fp = fopen("nesmusa.$$$", "rb");
	if(!fp)
	{
		perror("nesmusa.$$$");
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	if(!size)
	{
		fclose(fp);
		remove("nesmusa.$$$");
		goto FmtError;
	}
	MainMusic = (char *)malloc(size);
	if(!MainMusic)
	{
		fclose(fp);
		perror("Out of memory");
		return NULL;
	}
	rewind(fp);
	if(fread(MainMusic, (size_t)size, 1, fp) < 1)
	{
		printf("fread(nesmusa.$$$) error: %s", strerror(errno));
		fclose(fp);
        return NULL;
	}
	fclose(fp);
	remove("nesmusa.$$$");
	if(MainMusic[1]>2)MainMusic+=strlen(MainMusic)+1;
	if(	(((InternalHdr *)MainMusic)->InsNum !=
		 ((InternalHdr *)MainMusic)->Ins2Num
	) || ((InternalHdr *)MainMusic)->InsNum >= MAXINST
	  )
	{
FmtError:
		printf("File format error.\n");
		return NULL;
	}

	if(Verbose)printf("Deleted nesmusa.$$$ (%ld bytes)\n", size);
	return MainMusic;
}

static void LaskeMaara(int a, FILE *fp)
{
	switch(a)
	{
		case 0:
			sanac=0;
			break;
		case 2:
			if(sb[0]>31)sanac++;
			break;
		case 3:
			fclose(fp);
	}
}

static void LueSisaan(int a, FILE *fp)
{
	switch(a)
	{
		case 0:
			sanac=0;
			break;
		case 1:
		/*	files[sanac]=fp;
			sanat[sanac]=ftell(fp);
		*/	break;
		case 2:
			if(sb[0] > 31)
			{
				int i=strlen(sb);
				while(i)
				{
					if(sb[i-1]!='\n' && sb[i-1]!='\r')break;
					sb[--i]=0;
				}
				Sanat[sanac] = (char *)malloc(strlen(sb)+1);
				strcpy(Sanat[sanac], sb);
				sanau[sanac++]=0;
			}
			break;
		case 3:
			fclose(fp);
	}
}

void Sanastot(char *What, void (*proc)(int, FILE *))
{
	char Buf[13];
	FILE *fp;
	proc(0, NULL);
	for(int c=0; c<DictCount; c++)
	{
		if(!DictSel[c])continue;
		sprintf(Buf, "wspeed.da%c", DictFiles[c]);
		fp = fopen(Buf, "r");
		if(!fp)
		{
			perror(Buf);
			CleanUp(errno);
		}
		if(What && Verbose)printf("%s '%s'...\n", What, Buf);
		fgets(sb,127,fp); // T„t„ ei saa k„sitell„...
		for(; proc(1, fp),fgets(sb,127,fp); proc(2, fp));
		proc(3, fp);
	}
}

#ifdef __GNUC__
 #define SelInitDisp(r) {int q;\
	movedata(_dos_ds, 0xB8000, _my_ds(), (int)Buffer, (r)*160); \
	unsigned short *V=BParm; \
	for(q=0; q<(r)*80; q++, V++)if((((char)*V)==' ')&&((*V)>>8)<16)*V=0;}
#else
 #define SelInitDisp(r) {int q;\
	memcpy(Buffer, SCREEN, (r)*160); \
	unsigned short *V=BParm; \
	for(q=0; q<(r)*80; q++, V++)if((((char)*V)==' ')&&((*V)>>8)<16)*V=0;}
#endif

int Loga(int Num)
{
	int a, b, c;
	for(a=b=c=0; Num; Num--, b?b--:(a++,b=++c));
/*
	Num	Mit„	a b c
	0	0       0 0 0
	1	1       1 1 1
	2	1       1 0 1
	3	2       2 2 2
	4	2       2 1 2
	5	2       2 0 2
	6	3       3 3 3
	7	3
	8	3
	9	3
	10	4
	11	4
	12	4
	13	4
	14	4
*/
	return a;
}

void SelectDicts(void)
{
	extern char Version[];

	int a, b, y, HSel=0;
	FILE *fp;
	char Buf[80+80+1];
	int SelCount;

	DictCount=0;
	int DictSizes[sizeof DictSel];
	for(a='a'; a<='z'; a++)
	{
		sprintf(Buf, "wspeed.da%c", a);
		if(!access(Buf, 4))
		{
			if(DictCount==sizeof DictFiles)
			{
				printf("Error: Too many word library files.\n");
				CleanUp(sizeof DictFiles);
			}
			DictSel[DictCount] = 1;
			DictFiles[DictCount++] = a;
		}
	}

	for(b=0; b<DictCount; b++)
	{
		for(a=0; a<DictCount; a++)DictSel[a] = a==b;
		Sanastot(NULL, LaskeMaara);
		DictSizes[b] = sanac;
	}
	for(a=SelCount=0; a<DictCount; a++)
	{
		DictSel[a] = DictFiles[a]=='t'; //Suomi! :)
		if(DictSel[a])b=a, SelCount++;
	}

	textattr(0x71);
	cprintf("\r%-80s", "Select the word library files (keys: space, tab, arrows, enter, esc):");
	textattr(7);
	for(a=0; a<DictCount+2; a++)cprintf("%80s", "");
	y=wherey()-DictCount-2;
	gotoxy(1, y+DictCount);
	textattr(0x74);
	sprintf(Buf, "WSpeed version %s Copyright (C) 1992, 1999 Bisqwit (bisqwit@iki.fi)", Version);
	cprintf("%-80s", Buf);

	SelInitDisp(Riveja);

	for(; ; )
	{
		for(a=0; a<DictCount; a++)
		{
			char *s;
			sprintf(Buf, "wspeed.da%c", DictFiles[a]);
			fp = fopen(Buf, "r");
			if(!fp)
			{
				perror(Buf);
				CleanUp(errno);
			}
			fgets(sb,127,fp);
			fclose(fp);
			if((s=strchr(sb,'\n'))!=NULL)*s=0;
			textattr(a==b?0x1F:7);

			gprintf(
				0, y+a-1, a==b?0x1F:7,
				BParm,
				"[%c] %s: %s (%d words)",
				DictSel[a]?'X':' ', Buf, sb, DictSizes[a]);
		}

		sprintf(Buf,
			"      Skill controller (Ä> = slower)       Word movement speed (<Ä = faster)     "
			"(%2d) ------------------------------ (%3d) -------------------------------------",
			RequireBetter, SANAVIIVE);
		extern int XLIMIT, x36Attr;
		for(a=Loga(RequireBetter*5)*30/Loga(495); --a>=0; Buf[a+ 85]='=');
		for(a=Loga(SANAVIIVE)      *37/Loga(999); --a>=0; Buf[a+122]='=');
		XLIMIT=160, x36Attr=HSel?14:6;
		gprintf(0, y+DictCount, HSel?6:14, SCREEN, Buf);
		gprintf(0, y+DictCount, HSel?6:14, BParm, Buf);
		XLIMIT=80, x36Attr=-1;

		gotoxy(2, y+b);

        while(!WS_kbhit())GenIdle;

        switch(WS_getch())
		{
			case 0:
                switch(WS_getch())
				{
					case 0x0F: Tab:
						HSel = 1-HSel;
						break;
					case 'K':
						HSel
							? SANAVIIVE    ?SANAVIIVE--:0
							: RequireBetter?RequireBetter--:0;
						break;
					case 'M':
						HSel
							? SANAVIIVE   <999?SANAVIIVE++:0
							: RequireBetter<99?RequireBetter++:0;
						break;

					case 'H': b=(b+DictCount-1)%DictCount; break;
					case 'P': b=(b+          1)%DictCount; break;
					case '-': goto P1;
				}
				break;
			case '\t': goto Tab;
			case ' ':
				DictSel[b] ^= 1;
				if(DictSel[b])SelCount++;
				else SelCount--;
				break;
            case 3:
			case '\33': goto P1;
			case '\n':
			case '\r':
				if(SelCount)
				{
					textattr(7);
					gotoxy(1, y+DictCount+2);
					return;
	}	}		}
P1:	textattr(7);
	gotoxy(1, y+DictCount+2);
	cprintf("\n%80c", '\r');
	CleanUp(0);
}

void PrepareDicts(void)
{
	Sanastot("Analyzing", LaskeMaara);

	sanau = (unsigned char huge *)halloc(sanac, sizeof(unsigned char));
	Sanat = (char * huge *)       halloc(sanac, sizeof(char *));

	Sanastot("Loading", LueSisaan);
}

void ReleaseDicts(void)
{
	if(sanau){hfree(sanau);sanau=NULL;}
	if(Sanat){hfree(Sanat);Sanat=NULL;}
}

/* This was the original little miracle. A bit slow though.
char *GetWord(int i, char *dest)
{
	int j=i;
	char *s=dest;
	for(fseek(files[i],sanat[i],0); j=getc(files[i]), j-'\n' ? *s=j : (*s=0); s++);
	return dest;
}
*/

char *GetWord(int i, char *dest)
{
	return dest?strcpy(dest, Sanat[i]):Sanat[i];
}
int WordLen(int i)
{
	return strlen(Sanat[i]);
}

#include "../adlib/cardcs.inc"

