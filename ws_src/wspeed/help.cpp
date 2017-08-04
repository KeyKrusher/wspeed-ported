#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "wspeed.h"

void TxtViiva(int x1,int y1, int x2,int y2, int terminator)
{
	unsigned short w;
	int y;
	// y1 on aina alempana kuin y2. T„m„ oletetaan.
	w=x1 + y1*80;

	#ifdef __GNUC__
	unsigned short *Vid;
	__djgpp_nearptr_enable();
	Vid = (unsigned short *)(__djgpp_conventional_base + 0xB8000);
	#else
	#define Vid SCREEN
	#endif

	for(y=y1; y>=y2; y--, w-=80)
	{
		unsigned short a = '³';
		if(y==y2)a = terminator;
		BParm [w] = 0x0C00+a;
		Vid   [w] = 0x1C00+a;
	}
	for(w=x1+ ++y*80; x1!=x2; )
	{
		if(x2<x1)x1--,w--;else x1++,w++;
		BParm [w] = 0x0C00+'Ä';
		Vid   [w] = 0x1C00+'Ä';
	}

	#ifdef __GNUC__
	__djgpp_nearptr_disable();
	#else
	#undef Vid
	#endif

}

void HelpItem(int x, int y, int len, char *s)
{
	long bn = beenrunning;
	int sv = SANAVIIVE;
	int Len=len, c, X, pisin, Y,Y2,Y3, x2;
	char *t;

	updstatus();

	for(X=pisin=Y2=0, t=s; *t; t++)
	{
		if(*t!='\n')X++;
		if(*t=='\n' || !t[1])
		{
			Y2++;
			if(X>pisin)pisin=X;
			X=0;
		}
	}

	#ifdef __GNUC__
	if(__djgpp_nearptr_enable())
	{
		unsigned short *Vid = (unsigned short *)(__djgpp_conventional_base + 0xB8000);
		for(X=x; len; len--, X++)Vid[y*80+X] = (Vid[y*80+X]&0xFF)|0x4E00;
		__djgpp_nearptr_disable();
	}
	#else
	for(X=x; len; len--, X++)SCREEN[y*80+X] = (SCREEN[y*80+X]&0xFF)|0x4E00;
	#endif

	X = random(80-pisin);
	Y = Riveja - Y2 - 1 - random(random(Riveja-Y2));
	Y3=Y;
	for(x2=X, t=s; *t; t++)
	{
		if(*t!='\n')
			Buffer[Y][x2++] = 0x3000+*t;
		if(*t=='\n' || !t[1])
		{
			while(x2-X<pisin)Buffer[Y][x2++]=0x3020;
			x2=X;
			Y++;
		}
	}

	if(Len)
	{
		x+=Len/2;
		if(X>x){X--;Y=(Y+Y3)/2;c='Ú';}
		else if(X+pisin<=x){Y=(Y+Y3)/2;X+=pisin;c='¿';}
		else {c='³';X=x;}
		TxtViiva(x,y-1, X,Y, c);
	}

	SANAVIIVE=0;
    while(!WS_kbhit())GenIdle;
	SANAVIIVE=sv;

	memset(Buffer, 0, sizeof Buffer);

    do WS_getch();while(WS_kbhit());

	beenrunning=bn;
}

void Help(void)
{
	unsigned char far *a;
	int x=0, len=0;

	a = ((unsigned char far *)SCREEN) + 23*160;

	HelpItem(0,14,0,
		"                ** The idea of this game **\n\n"
		"       There go words on the playing screen and you\n"
		"      need to type them as fastly as you are able to\n"
		"       type them. The words disappear when you have\n"
		"           correctly wrote them and press enter.\n\n"
		"   This great game has been made by Bisqwit (bisqwit@iki.fi).\n"
		" The idea and some words of word libraries have been borrowed\n"
		" from the Zorlim's ztspeed game, which is also very cool, but\n"
		" not as cool as this ;)");

	for(x+=len; *a!=':'; x++)a+=2; x++,a+=2; for(len=0; *a!='/'; len++)a+=2;
	HelpItem(x,23, len,
		"Your score. It increases at the\n"
		"same time you do good in the game.");

	for(x+=len; *a!='/'; x++)a+=2; x++,a+=2; for(len=0; a[2]!='S'; len++)a+=2;
	HelpItem(x,23, len,
		"How many letters have you typed.\n"
		"Only the characters that were right,\n"
		"are counted.");

	for(x+=len; *a!=':'; x++)a+=2; x++,a+=2; for(len=0; a[2]!='('; len++)a+=2;
	HelpItem(x,23, len,
		"Your skill level. Units: How many\n"
		"alphabets do you seem to be able\n"
		"to handle at time on screen.");

	for(x+=len; *a!='y'; x++)a+=2; x++,a+=2; for(len=0; *a!=')'; len++)a+=2;
	HelpItem(x,23, len,
		"The game wants your skill level\n"
		"to be at least the one mentioned\n"
		"here.\n"
		"Also this tells you approximately\n"
		"how much stuff there's on screen\n"
		"now, in alphabets.");

	for(x+=len; *a!='('; x++)a+=2; x++,a+=2; for(len=0; *a!=')'; len++)a+=2;
	HelpItem(x,23, len,
		"Your skill level in human language,\n"
		"based on your 10 minute relative speed.");

	for(x+=len; *a!=':'; x++)a+=2; x++,a+=2; for(len=0; *a!='/'; len++)a+=2;
	HelpItem(x,23, len,
		"Your ten minute relative speed.\n"
		"Calculations based on how have\n"
		"you played. Think about this:\n"
		"You could write this much text in\n"
		"ten minutes. One A4-page contains\n"
		"less than 2000 alphabets.");

	for(x+=len; *a!='/'; x++)a+=2; x++,a+=2; for(len=0; isdigit(*a); len++)a+=2;
	HelpItem(x,23, len,
		"Your ten minute relative speed.\n"
		"Calculations based on how quickly\n"
        "did you type the previous word.");

	a = ((unsigned char far *)SCREEN) + 24*160;
	x=0, len=0;

	for(x+=len; *a!='['; x++)a+=2; x++,a+=2; for(len=0; *a!=']'; len++)a+=2;
	HelpItem(x,24, len,
		"You type here.");

	for(x+=len; *a!=':'; x++)a+=2; x++,a+=2; for(len=0; *a!='/'; len++)a+=2;
	HelpItem(x,24, len,
		"This tells how many words there\n"
		"are on screen pending now...");

	for(x+=len; *a!='/'; x++)a+=2; x++,a+=2; for(len=0; *a!=' '; len++)a+=2;
	HelpItem(x,24, len,
		"This is the count of words in the selected word libraries.");

	for(x+=len; *a!=':'; x++)a+=2; x++,a+=4; for(len=0; a[2]!=' '; len++)a+=2;
	HelpItem(x,24, len,
		"How many words have you lost so\n"
		"far. Only 9 misses are accepted:\n"
		"Tenth makes game-over.");
}
