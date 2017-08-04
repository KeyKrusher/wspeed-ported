#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include "m_path.c"

void Putta(char *s)
{
	if(!access(s, 4))puts(s);
}

char env[64] = "PATH";

int Found=0;

#define PATHI(a) {Pathi(meminuse,255,env,(a));if(!access(meminuse,4))printf("%s\n",meminuse),Found++;}
static char meminuse[256];

char Buf[512];
char *s;

void Ezi(void)
{
    strcpy(Buf, s);            PATHI(Buf);
    sprintf(Buf, "%s.com", s); PATHI(Buf);
    sprintf(Buf, "%s.exe", s); PATHI(Buf);
    sprintf(Buf, "%s.bat", s); PATHI(Buf);
    sprintf(Buf, "%s.btm", s); PATHI(Buf);
    sprintf(Buf, "%s.s3m", s); PATHI(Buf);
    sprintf(Buf, "%s.mod", s); PATHI(Buf);
    sprintf(Buf, "%s.xm", s);  PATHI(Buf);
    sprintf(Buf, "%s.txt", s); PATHI(Buf);
    sprintf(Buf, "%s.doc", s); PATHI(Buf);
}

int main(int argc, char **argv)
{
	if(--argc)
	{
		s = *++argv;
		if(--argc)
			strcpy(env, *++argv);
	}
	else
	{
		printf("Usage: WHICH prog [envvar]\n");
		return 0;
	}

	Ezi();

	strcpy(Buf, env);
	strcpy(env, s);
	s = (char *)malloc(strlen(Buf)+1);
	strcpy(s, Buf);

	Ezi();

    return Found;
}
