#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argh.h"

int Heelp = 0;

long ParamRange(long min, long max, const char **s)
{
	long tmp = strtol(*s, (char **)s, 10);
	if(tmp<min || tmp>max)
	{
		printf("%s: parameter value out of range `%ld'\n", ProgName, tmp);
		exit(EXIT_FAILURE);
	}
	return tmp;
}

void SuggestHelp(void)
{
	printf("\nTry `%s --help' for more information.\n", ProgName);
}
void ArgError(int a, const char *b)
{
	switch(a)
	{
		case 0:
			printf("%s: illegal option -- %c", ProgName, *b);
			for(a=0; Options[a].c; a++)
				if(*b == Options[a].c)
				{
					putchar(b[1]);
					break;
				}
			break;
		case 1:
			printf("%s: unrecognized option `--%s'", ProgName, b);
			break;
		default:
			printf("%s: invalid parameter: `%s'\n", ProgName, b);
	}
	SuggestHelp();
	exit(EXIT_FAILURE);
}
void ReadArgs(int argc, const char *const *argv,
              const char *UsageExtra, const char *UsageEnd)
{
	int a;
	
	Case(0, NULL);

	for(a=1; a<argc; a++)
	{
		const char *s = argv[a];

ReArg:	if(!s)continue;
		while(*s==' ' || *s=='\t')s++;
		if(!*s)continue;
		
		if((*s == '-' || (*s=='/'&&s[1]=='?')) && s[1])
		{
			if(*++s == '-')
			{
				int b=0;
				for(s++; Options[b].c; b++)
				{
					const char *t = Options[b].txt;
					const char *q = strchr(t, ' ');
					if(!q)q = strchr(t, 0);
					if(!strncmp(s, t, q-t) && !s[q-t])
					{
						s = argv[++a];
						Case(Options[b].ID, &s);
						goto ReArg;
					}
				}
				ArgError(1, s);
			}
			while(s && *s)
			{
				int b,c;
				for(b=c=0; Options[b].c; b++)
					if(s[0]==Options[b].c && s[1]==Options[b].c2)
					{
						c=1, s += 2, Case(Options[b].ID, &s), s--;
						if(!*s)break;
					}
				if(!c)
					for(b=0; Options[b].c; b++)
						if(s[0]==Options[b].c && !Options[b].c2)
							c=1, s++, Case(Options[b].ID, &s), s--;
					
				if(!c)ArgError(0, s);
				if(*s)s++;
			}
		}
		else
			DefArg(s);
	}
	
	if(Heelp)
	{
		PrintVersionInfo();
		PrintDescription();
		
		printf("\nUsage: %s [-", ProgName);
		for(a=0; Options[a].c; a++)if(!Options[a].c2)printf("%c", Options[a].c);
		printf("] %s\n\n", UsageExtra);

		for(a=0; Options[a].c; a++)
			printf("  -%c%c%c--%-14s%s\n",
				Options[a].c,
				Options[a].c2?Options[a].c2:',',
				Options[a].c2?',':' ',
				Options[a].txt,
				Options[a].descr);
				
		printf("%s\n", UsageEnd);

		exit(EXIT_SUCCESS);
	}
	Case(-1, NULL);
}
