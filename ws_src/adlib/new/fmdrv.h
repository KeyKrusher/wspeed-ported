#ifndef _fmdrv_FMDRV_H
#define _fmdrv_FMDRV_H

/* This is _very_ crappy because of unplannedly built unix support... */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#ifndef __BORLANDC__
#include "music.h"
#endif
#ifdef __BORLANDC__
#include <io.h>
#include <dir.h>
#include <conio.h>
#include <dos.h>
#include "music.h"
#define setxpos(x) gotoxy((x),wherey())
#else
#define farmalloc(x) calloc(x,1)
#define farfree free
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef DJGPP
#include <dir.h>
#include <conio.h>
#include <dos.h>
#define setxpos(x) gotoxy((x),wherey())
#else
#ifdef linux
#include "io.h"
#endif

#define searchpath(s) s
#define EINVFMT EINVAL
#define textattr SetAttr
#define cprintf Gprintf
static int TextAttr=7;
static int OldAttr=7;
static void FlushSetAttr(void);
static void SetAttr(int newattr)
{
	TextAttr = newattr;
}
int WhereX=1,WhereY=1;
#define wherex() WhereX
#define setxpos(x) {printf("\r");if((WhereX=(x))>1)printf("\33[%dC",WhereX-1);fflush(stdout);}
#ifndef AnsiOpt
#define AnsiOpt 1
#endif
struct MusData MusData;
static void FlushSetAttr(void)
{
    if(TextAttr == OldAttr)return;

    printf("\33[");

    if(TextAttr != 7)
    {
    	static char Swap[] = "04261537";
    	
    	if(AnsiOpt)
    	{
	    	int pp=0;
	    	
	    	if((OldAttr&0x80) > (TextAttr&0x80)
	    	|| (OldAttr&0x08) > (TextAttr&0x08))
	    	{
	    		putchar('0'); pp=1;
	    		OldAttr = 7;
	    	}    	
	    	
	    	if((TextAttr&0x08) && !(OldAttr&0x08)){if(pp)putchar(';');putchar('1');pp=1;}
	    	if((TextAttr&0x80) && !(OldAttr&0x80)){if(pp)putchar(';');putchar('5');pp=1;}
	       	
	       	if((TextAttr&0x70) != (OldAttr&0x70))
	       	{
	   	   		if(pp)putchar(';');
	       		putchar('4');
	       		putchar(Swap[(TextAttr>>4)&7]);
	       		pp=1;
	       	}
	       	
	      	if((TextAttr&7) != (OldAttr&7))
	      	{
	       		if(pp)putchar(';');
	       		putchar('3');
	       		putchar(Swap[TextAttr&7]);
	       	}
       	}
       	else
	    	printf("0%s%s;4%c;3%c",
    			(TextAttr&8)?";1":"",
    			(TextAttr&0x80)?";5":"",
    			Swap[(TextAttr>>4)&7],
	    		Swap[TextAttr&7]);
    }

    putchar('m');
	OldAttr = TextAttr;
}
int Gprintf(const char *fmt, ...) __attribute__((format(printf,1,2)));
static int Gputch(int x)
{
	static int Mask[132]={0};
	int TmpAttr = TextAttr;
    if(x=='\n')SetAttr(7);
    FlushSetAttr();
	if(x==7)
	{
		putchar(x);
		return x;
	}
	if(AnsiOpt)
	{
		static int Spaces=0;
		if(x==' ')
		{
			Spaces++;
			return x;
		}
		while(Spaces)
		{
			int a=WhereX-1, Now, mask=Mask[a];
			
			for(Now=0; Now<Spaces && Mask[a]==mask; Now++, a++);
			
			Spaces -= Now;
			if(mask)
			{
			Fill:
				while(Now>0){Now--;putchar(' ');}
			}
			else if(Spaces || !(x=='\r' || x=='\n'))
			{
				if(Now<=4)goto Fill;
				printf("\33[%dC", Now);
			}
			WhereX += Now;
		}
	}
	putchar(x);	
	if(x=='\n')
		SetAttr(TmpAttr);
	if(x=='\r')
		WhereX=1;
	else if(x!='\b')
    {
		if(x >= ' ')Mask[WhereX++ - 1] = 1;
		else WhereX=1, memset(Mask, 0, sizeof Mask);
    }
	else if(WhereX>1)
		WhereX--;
		
	return x;
}
int ColorNums = -1;
int Gprintf(const char *fmt, ...)
{
    char *s, Buf[2048];
    int a;

    va_list ap;
    va_start(ap, fmt);
    a=vsprintf(Buf, fmt, ap);
    va_end(ap);
    
    for(s=Buf; *s; s++)
    {
        if(*s=='\t')Gprintf("   ");
        else
        {
#ifdef DJGPP
            if(*s=='\n')Gputch('\r');
#endif
            if((*s=='.'||isdigit((int)*s)) && ColorNums >= 0)
            {
            	int ta = TextAttr;
            	SetAttr(ColorNums);
            	Gputch(*s);
            	SetAttr(ta);
            }
            else
            	Gputch(*s);
	}	}
    return a;
}
#include "cons.h"
#define random(x) (rand()%(x))
#endif
#endif

#endif
