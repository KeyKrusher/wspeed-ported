#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

char FBtemp[] = "tmptmp.findbest";
char ProgName[]="intgen";

char Parm[] = "-sC";

#if defined(__MSDOS__) || defined(DJGPP)
char NulName[]  = "NUL";
char CopyProg[] = "copy /Y";
char MoveProg[] = "move /Y";
char DelProg[]  = "del";
char Quote[]    = "";
#else
char NulName[]  = "/dev/null";
char CopyProg[] = "cp";
char MoveProg[] = "mv";
char DelProg[]  = "rm -f";
char Quote[]    = "\\";
#endif

int main(int argc, char **argv)
{
	unsigned long a,nx,nc,nd,no,nl,nr;
	unsigned long B,NX,NC,ND,NO,NL,NR;
	unsigned int Max;
	char *in, *out, ofn[1024], Opt[1024], ohjaus[64]="";
	int i;
		
	if(argc != 3)
	{
		printf(	
			"This program runs intgen multiple times to create the\n"
			"shortest possible selfplayer using the -%s parameter.\n"
			"\n"
			"Usage: %s input-nes-s3m-file out-c-file\n\n"
			"Outfile is assumed as .c, so don't specify .c\n\n",
			argv[0], Parm);
			
		return 0;
	}
	
	in = argv[1];
	out = argv[2];
	
	sprintf(ofn, "%s.c", out);
	
	B=1000000;
	ND=NX=NC=NO=NL=NR=0;
	
	a=0;
	
Fixme:	
	sprintf(Opt, "%s -v -fb %s %s %s* %s* %s", ProgName, Parm, in, Quote,Quote, ohjaus);
	if((i=(char)system(Opt)) != 0)
	{
		printf("We have the problem #%d.\n", i);
		return i;
	}	
	
	for(Max = (1<<28); a<Max; a++)
	{
		int d, A;
		struct stat Stat;
		
		A = a*1000 / Max;
		
		d = (a*4603691)%Max;
		
		nx = d&255;				/* Xorring value    */
		no = 64;            	/* Charcount (2^x)  */
		nc =      (d>>8)&127;	/* Compressor value */
		nl = 5 + ((d>>15)&3);	/* Length bits      */
		nd = 4 + ((d>>17)&7);	/* Distance bits    */
		nr =      (d>>20)&255;	/* Roll count       */
		
    	printf("%3d.%d%% Trying -nd%ld -nx%ld -nc%ld -no%ld -nr%ld -nl%ld%16s", A/10,A%10, nd,nx,nc,no,nr,nl, "\b\b\b\b\b\b\b\b");
    	fflush(stdout);
    	
    	sprintf(Opt, 
    		/* #fb is a macro to save command line space in dos */
    		"%s \"(#fb)\" -fb %snind%ldnx%ldnc%ldno%ldnr%ldnl%ld %s %s>%s",
    		ProgName, Parm, nd,nx,nc,no,nr,nl, in, out, NulName);
    	
    	if((i=(char)system(Opt))!=0)
    		if(i != 256) /* EXIT_FAILURE */
    		{
	    		printf(" - intgen returned error code %d, exiting\n", i);
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
    				printf(" - can't open %s\n", ofn);
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
	    			
		    		sprintf(Opt, "%s %s %s > %s", CopyProg, ofn, FBtemp, NulName); system(Opt);
	    			ND=nd,NX=nx,NC=nc,NO=no,NR=nr,NL=nl, B=(long)Stat.st_size;
   					printf(" - size %ld\n", B);
    			}
    		}
    	printf("         \r");
	}

	printf("\nBest is %snd%ldnx%ldnc%ldno%ldnr%ldnl%ld -- %ld bytes - in %s\n", Parm, ND,NX,NC,NO,NR,NL, B, ofn);
	
	sprintf(Opt, "%s %s %s",                MoveProg, FBtemp, ofn);               system(Opt);
	sprintf(Opt, "%s intgen.%s$%s$%s$ >%s", DelProg, Quote,Quote,Quote, NulName); system(Opt);
	sprintf(Opt, "%s nesmusa.%s$%s$%s$",    DelProg, Quote,Quote,Quote);          system(Opt);
	
	return 0;
}
