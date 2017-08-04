#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>

static char ProgName[]="intgen";

char Parm[8] = "-sc";
char Verbose[8] = "-v ";

#if defined(__MSDOS__) || defined(DJGPP)
static char NulName[]  = "NUL";
static char CopyProg[] = "copy /Y";
static char MoveProg[] = "move /Y";
static char DelProg[]  = "del";
static char CatProg[]  = "type";
static char Quote[]    = "";
#else
static char NulName[]  = "/dev/null";
static char CopyProg[] = "cp";
static char MoveProg[] = "mv";
static char DelProg[]  = "rm -f";
static char CatProg[]  = "cat";
static char Quote[]    = "\\";
#endif

int main(int argc, const char *const *argv)
{
	unsigned long a,nx,nc,nd,no,nl,nr;
	unsigned long B,NX,NC,ND,NO,NL,NR;
	unsigned int Max;
	const char *in, *out;
	char ofn[1024], Opt[1024], ohjaus[64]="";
	int i, Try=0, NumTries=0, add=0;
	char *FBtemp = tmpnam(NULL);
	int temp=-1;
		
	i = 3;
	while(argc > 3)
	{
		in = argv[i++];
		argc--;
		while(*in)
		{
			switch(*in)
			{
				case 'd':
					Parm[3] = 'f';
					Parm[4] = '2';
					Parm[5] = 0;
					break;
				case 'c':
				case 'C':
					Parm[2] = *in;
					break;
				case 'q':
					Verbose[0] = 0;
					break;
				case 'D':
					add=atoi(in+1);
					goto ThisDone;
					break;
				default:
					NumTries = atoi(in);
					goto ThisDone;
			}
			in++;
		}
	ThisDone:;
	}
	if(argc != 3)
	{
		printf(	
			"This program runs intgen multiple times to create the\n"
			"shortest possible selfplayer using the %s parameter.\n"
			"\n"
			"Usage: %s input-nes-s3m-file out-c-file [options]\n\n"
			"Outfile is assumed as .c, so don't specify .c\n\n"
			"Options: (d)ataonly (C)omplete (c)ompact #count (q)uiet ad(D)\n"
			"\n"
			"You probably don't know why and how to use this program.\n",
			Parm, argv[0]);
			
		return 0;
	}
	
	in = argv[1];
	out = argv[2];
	
	if(out[0]=='-' && !out[1])
	{
		temp = dup(1);
		/* FIXME: This isn't rational... */
		tmpnam(out);
		fclose(stdout);
	}
	sprintf(ofn, "%s.c", out);
	
	B=1000000;
	ND=NX=NC=NO=NL=NR=0;
	
	a=0;
	
Fixme:	
	sprintf(Opt, "%s %s-fb %s %s %s* %s", ProgName, Verbose, Parm, in, Quote, ohjaus);
	printf("Trying '%s'\n", Opt);	
	if((i=(char)system(Opt)) != 0)
	{
		fprintf(stderr, "We have the problem #%d.\n", i);
		return i;
	}	
	
	for(Max = (1<<28); a<Max; a++)
	{
		int d, A;
		struct stat Stat;
		
		if(NumTries > 0 && Try >= NumTries)break;
		Try++;
		
		A = a*1000 / Max;
		
		d = ((a+add)*4603691)%Max;
		
		nx = d&255;				/* Xorring value    */
		no = 64;            	/* Charcount (2^x)  */
		nc =      (d>>8)&127;	/* Compressor value */
		nl = 5 + ((d>>15)&3);	/* Length bits      */
		nd = 4 + ((d>>17)&7);	/* Distance bits    */
		nr =      (d>>20)&255;	/* Roll count       */
		
		if(NumTries > 0)
			fprintf(stderr, "%4d ", Try);
		else
	    	fprintf(stderr, "%3d.%d%% ", A/10,A%10);
    	fprintf(stderr, "Trying -nd%ld -nx%ld -nc%ld -no%ld -nr%ld -nl%ld%16s", nd,nx,nc,no,nr,nl, "\b\b\b\b\b\b\b\b");
    	fflush(stderr);
    	
    	sprintf(Opt, 
    		/* #fb is a macro to save command line space in dos */
    		"%s \"(#fb)\" -fb %snind%ldnx%ldnc%ldno%ldnr%ldnl%ld %s %s >%s",
    		ProgName, Parm, nd,nx,nc,no,nr,nl, in, out, NulName);
    	
    	if((i=(char)system(Opt))!=0)
    		if(i != 256) /* EXIT_FAILURE */
    		{
	    		fprintf(stderr, " - intgen returned error code %d, exiting\n", i);
	    		break;
    		}
    	
    	if(stat(ofn, &Stat) >= 0)
    		if(Stat.st_size > 0 && B > (unsigned)Stat.st_size)
    		{
    			FILE *fp;
    			int n;
    			char *s;
    			fp = fopen(ofn, "rt");
    			if(!fp)
    				fprintf(stderr, " - can't open %s\n", ofn);
    			else
    			{
	    			fgets(Opt, sizeof(Opt)-1, fp);
	   	 			fclose(fp);
   		 			for(s=Opt;*s;s++)if(isdigit(*s))break;
   		 			while(isdigit(*s))s++;
	    			for(;*s;s++)if(isdigit(*s))break;
    				n=atoi(s);
    				
	    			if(!n)
	    			{
	    				printf(" - fixme - size %ld\n", (long)Stat.st_size);
	    				sprintf(ohjaus, "> %s", NulName);
	    				goto Fixme;
	    			}
	    			
		    		sprintf(Opt, "%s %s %s >%s", CopyProg, ofn, FBtemp, NulName);
		    		system(Opt);
	    			ND=nd,NX=nx,NC=nc,NO=no,NR=nr,NL=nl, B=(long)Stat.st_size;
   					fprintf(stderr, " - size %ld\n", B);
    			}
    		}
    	fprintf(stderr, "         \r");
	}

	fprintf(stderr, "\nBest is %snd%ldnx%ldnc%ldno%ldnr%ldnl%ld -- %ld bytes - in %s\n", Parm, ND,NX,NC,NO,NR,NL, B, ofn);
	
	if(temp>=0)
	{
		dup2(temp, 1);
		sprintf(Opt, "%s %s",               CatProg, FBtemp);        system(Opt);
		sprintf(Opt, "%s %s",               DelProg, ofn);           system(Opt);
		sprintf(Opt, "%s %s",               DelProg, out);           system(Opt);
	}
	else
	{
		sprintf(Opt, "%s %s",               DelProg, ofn);			 system(Opt);
		sprintf(Opt, "%s %s %s",            MoveProg, FBtemp, ofn);  system(Opt);
	}
	sprintf(Opt, "%s intgen.%s$%s$%s$ >%s", DelProg, Quote,Quote,Quote, NulName); system(Opt);
	sprintf(Opt, "%s nesmusa.%s$%s$%s$",    DelProg, Quote,Quote,Quote);          system(Opt);
	
	return 0;
}
