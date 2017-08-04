#include "cons.h"
#include <fcntl.h>

struct termios tattr, saved_attributes;
int LastNap = -1;

int getch(void)
{
	int temp = 0;
	if(LastNap >= 0)
	{
		temp = LastNap;
		LastNap = -1;
		return temp;
	}
	while (!fread(&temp, 1, 1, stdin));

	return temp;
}

int kbhit(void) 
{
	unsigned char Byte;
	if(LastNap >= 0)return 1;
	if(fread(&Byte, 1, 1, stdin))
	{
		LastNap = Byte;
		return 1;
	}
	LastNap = -1;
	return 0;
}

void DoneCons(void)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_attributes);
}

void InitCons(void)
{ 
	setvbuf(stdout, NULL, _IONBF, 0);

    tcgetattr(STDIN_FILENO, &saved_attributes);
    tcgetattr(STDIN_FILENO, &tattr);
    tattr.c_lflag &= ~(ICANON|ISIG|ECHO);
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
	fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
}

char *strlwr(char *string)
{
	char *temp = string;
	
	while(*temp)
	{
		*temp = tolower(*temp);
		if(*temp=='Ä')*temp='ä';
		if(*temp=='Ö')*temp='ö';
		if(*temp=='Å')*temp='å';
		temp++;
	}	
	
	return string;
}

char *strupr(char *string)
{
	char *temp = string;
	
	while(*temp)
	{
		*temp = toupper(*temp);
		if(*temp=='ä')*temp='Ä';
		if(*temp=='ö')*temp='Ö';
		if(*temp=='å')*temp='Å';
		temp++;
	}	
	
	return string;
}

int strnicmp(const char *eka, const char *toka, long Max)
{
    while(Max--)
    {
    	char ch1 = toupper(*eka);
    	char ch2 = toupper(*toka);
    	if(ch1 < ch2)return -1;
    	if(ch1 > ch2)return 1;
    	if(!ch1)break;
    	eka++;
    	toka++;
    }
    return 0;
}

#define stricmp(a,b) strnicmp((a),(b), 0x7FFFFFFFL)

const char *strscan(const char *str, char Ch)
{
	while(*str)
	{
		if(Ch == *str)return str;
		str++;
	}
	return NULL;
}

char *strccat(char *Dest, char Ch)
{
	register int d=strlen(Dest);
	
	Dest[d] = Ch;
	Dest[d+1] = '\0';
	
	return Dest;
}

void Error(char *Where)
{
 	printf("\33[0;1;31mProg: %s() error %d `\33[37m%s\33[31m'\33[m\n\n", Where, errno, sys_errlist[errno]);
	exit(errno);
}

void GotoXY(int x, int y)
{
	printf("\33[%d;%dH", y, x);
}

void ClrScr(void)
{
	printf("\33[2J\33[H");
}

void ClrEOL(void)
{
	printf("\33[K");
}

