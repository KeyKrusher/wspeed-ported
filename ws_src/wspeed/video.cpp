#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef __GNUC__
 #include <sys/movedata.h>
 #include <go32.h>
#endif

#include "wspeed.h"

item items[maxitem];

unsigned short Buffer[25][80];

int StarDir = -1;

int pascal spawnstar(void)
{
	int a;
	for(a=0; a<maxitem; a++)if(!items[a].typi)break;
	if(a-maxitem)
	{
		items[a].typi = tahti;
		items[a].param = random(3);
		items[a].x = StarDir>0?-1:80;
		items[a].y = random(Riveja);
	}
	return 1;
}

/* Two exceptions for misc.cpp */
int XLIMIT = 80;
int x36Attr = -1;

int gprintf(
	int x, int y, unsigned char attr,
	unsigned short far *vid, char *fmt, ...)
{
	static char buf[256];
	char *s = buf;

	va_list ap;
	va_start(ap, fmt);

	vsprintf(buf, fmt, ap);

	va_end(ap);

	#ifdef __GNUC__
	 int scrflag = (vid==SCREEN);
	 if(scrflag && __djgpp_nearptr_enable())
	 {
     	vid = (unsigned short *)(__djgpp_conventional_base + 0xB8000);
	 }
	#endif

	for(vid += y*80+x, y=attr; *s; vid++,s++,x++)
		if(x>=0 && x<XLIMIT)
		{
			if(x36Attr>=0)
			{
				if(x==36 || x==116)attr=x36Attr;
				if(x==80)attr=y;
			}
			if(attr == 0x1F && isdigit(*s))
				*vid = 0x1E00 + *s;
			else
				*vid = (attr<<8) + *s;
        }
	#ifdef __GNUC__
	if(scrflag)__djgpp_nearptr_disable();
	#endif

	return strlen(buf);
}

void updstatus(void)
{
	int a;
	char Buf[32];

#if 1
	a = gprintf(0, 23, 0x1F, SCREEN,
		"Score:%8lu/%-5dSkill: %d (try %d) (%s)",
		score, kirjaimia,
        CurrentSkill(), (int)Paljonkoruudulla(),
		Selita(last10mrs)
	);

	if(Training && (beenrunning%100)<40)
		gprintf(0, 23, 0x13, SCREEN, "** Training mode **");

	gprintf(a, 23, 0x17, SCREEN, "%*s%5d/%-4d",
		70-a, "10mrs:",
		last10mrs, curr10mrs);

	gprintf(0, 24, 0x1F, SCREEN,
		"[%16s]  Traffic: %d/%d   Missed: %d           ", "",
		used, sanac,
		(int)mokia
	);
#else
	a = gprintf(0, 23, 0x1F, SCREEN,
		"Score:%8lu/%-5dMissed: %d Skill: %d (%s)",
		score, kirjaimia,
		(int)mokia,
		CurrentSkill(),
		Selita(last10mrs)
	);

	if(Training && (beenrunning%100)<40)
		gprintf(0, 23, 0x13, SCREEN, "** Training mode **");

	gprintf(a, 23, 0x17, SCREEN, "%*s%5d/%-4d",
		70-a, "10mrs:",
		last10mrs, curr10mrs);

	gprintf(0, 24, 0x1F, SCREEN,
		"[%16s]  Traffic: %d/%d   Throttled skill: %-7d", "",
		used, sanac,
		(int)Paljonkoruudulla()
	);
#endif

	gprintf(1, 24, 0x17, SCREEN, "%-16s", inputline);

	if(NoDelay)
		strcpy(Buf, "Warp mode ('!')");
	else
		sprintf(Buf, "Time used: %ld.%02ds", beenrunning/100, (int)(beenrunning%100));

	gprintf(55, 24, 0x17, SCREEN, "%*s", 25, Buf);
}

/*	Ottaa parametrin„ bufferin, jossa on lent„vi„	*
 *	sanoja. Piirt„„ sen bufferin n„yt”lle, ja		*
 *	bufferin taakse piirt„„ t„hti„.					*/
int pascal drawscreen(unsigned short *vidbuf2)
{
	#ifdef __GNUC__
	 #define near
	#endif
	static unsigned short near vidbuf[Riveja][80];
	int x, y, a, c, wc;
	int scounter = (int)beenrunning;

	/* Clear temp. videobuffer */
	memset(vidbuf, 0, sizeof vidbuf);

	/* Stars */
	for(a=0; a<maxitem; a++)
		if(items[a].typi == 1)
		{
			x = items[a].x;
			y = items[a].y;
			if(x >= 0 && x < 80)
			{
				/* Three different star types */
				char sc[3] = {8,7,15};
				/* Draw star */
				vidbuf[y][x] = (sc[items[a].param]<<8) + 'ú';
			}
			if(!(scounter%TAHTIVIIVE))
				items[a].x += StarDir * (items[a].param+1);
			if(items[a].x < -1 || items[a].x > 80)items[a].typi = tyhjyys;
		}

	/* Words */
	for(a=wc=0; a<maxitem; a++)
		if(items[a].typi == 2)
		{
			wc++;
			x = items[a].x;
			y = items[a].y;

            GetWord(items[a].param, sb);

			c = x<50?10:x<65?14:12;

			/* Draw word */
			gprintf(x, y, c, (unsigned short far *)&vidbuf, sb);

			if(x >= 79)
			{
                int c = MusData.Tempo;

				mokia++;
				if(mokia >= 2)
				{
                    c = MusData.Tempo + 10;
					if(c>254)c=254;
				}
				if(mokia == 9)c = 254;
                MusData.Tempo = c;

				items[a].typi = tyhjyys;
				sanau[items[a].param]--;
				used--;
			}
			else
				!SANAVIIVE || scounter%SANAVIIVE || items[a].x++;
		}

	/* Draw */
	unsigned short near *dest = (unsigned short near *)vidbuf;
	for(y=0; y<Riveja; y++)
		for(x=0; x<80; x++)
		{
			if(*vidbuf2)*dest = *vidbuf2;
			vidbuf2++;
			dest++;
		}

	#ifdef __GNUC__
	movedata(_my_ds(), (int)&vidbuf, _dos_ds, 0xB8000, sizeof(vidbuf));
	#else
	_fmemcpy(SCREEN, vidbuf, sizeof vidbuf);
	#endif

	return wc;
}
