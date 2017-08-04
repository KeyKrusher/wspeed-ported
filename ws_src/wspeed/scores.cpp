#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h> // for MK_FP

/* SCORE FILE I/O */

char Version[] = "1.13";

#ifdef MAIN
#include <ctype.h>
#include <stdio.h>
#include <conio.h>
#include "scores.h"
static int Verbose = 0;
static int NoCollision;
static int RequireBetter;
static int SANAVIIVE;
static char DictFiles['z'-'a'+1];
static int DictSel[sizeof DictFiles];
static int DictCount;
static int ScoreWasFound;
#define scorenamelen 128
#else
#include "wspeed.h"
#define scorenamelen
#endif

ScoreItem Scores[ScoreDepth];
static unsigned long CkSum;

char scorename[scorenamelen] = "wspeed.rec";
char mixscorename[] = "wspeed.mix";
static char Header[128];
#ifdef MAIN
static int Original=0;
#else
static char tmpname[scorenamelen] = "wspeed.reo";
#endif
static char ckhdr[] = "Password";

#ifdef MAIN
static void CleanUp(int value)
{
    textattr(7);
    exit(value);
}
static void Understand(char *s)
{
    int a;
    s += 6;
    for(NoCollision=0;isdigit(*s);)NoCollision = NoCollision*10 + *s++ - '0';
    s++;
    for(RequireBetter=0;isdigit(*s);)RequireBetter=RequireBetter*10+*s++-'0';
    s++;
    for(SANAVIIVE = 0;isdigit(*s);)SANAVIIVE   = SANAVIIVE * 10 + *s++ - '0';
    DictCount = strlen(s);
    for(a=0; *s; a++)
    {
        DictSel[a]=1;
        DictFiles[a] = *s++;
    }
}
static int IsModel(char *s)
{
    return !strncmp(s, "Model ", 6);
}
#endif

char *GetModel(void)
{
    unsigned a, b;
	sprintf(Header, "Model %u-%u-%u", NoCollision, RequireBetter, SANAVIIVE);
	b = strlen(Header);
	for(a=0; a<DictCount; a++)
		if(DictSel[a])
		{
			Header[b++] = DictFiles[a];
			Header[b] = 0;
		}
#ifndef MAIN
	if(Dvorak)
	{
		Header[b++] = '/';
		Header[b++] = 'd';
		Header[b] = 0;
	}
#endif
	return Header;
}

#ifdef MAIN
static int Kaikkimatch = 0;
#define HELPVERTAA || Kaikkimatch
#else
#define HELPVERTAA
#endif

#define VertaaHeader(s) (!strcmp((s), Header) HELPVERTAA)

static const char ScoreFormat[] = "%d*%ld*%d*%d*%d*%d*%ld";

static void AddCheckSum(char *s)
{
    #ifndef __BORLANDC__
     register unsigned long _EAX,_EDX;
     #define _DL ((unsigned char)_EDX)
    #endif
    while(*s)
    {
        /* This works if it works. For me, it works. */
        _EDX = CkSum;
        _EAX = ((unsigned long)(*s++) << (_DL&15)) ^ _EDX;
        #ifdef __GNUC__
        asm ( "rorl $11, %%eax"
            : /* Out  */ "=a" (_EAX)
            : /* In   */
            : /* Uses */ "%eax" );
        #else
        _asm { db 0x66; rol ax, 11 }
        #endif
        CkSum = _EAX;
    }
}

#define Fail(s) {printf("\n"s); goto FAIL;}

void ReadScores(void)
{
    FILE *fp = fopen(scorename, "rt");

    char Buf[256];
    char SectionName[sizeof Header];
	char Summa[32];
	int Summattu;

	memset(Scores, 0, sizeof Scores);

	if(!fp)
	{
		if(errno!=ENOENT
		#ifdef __BORLANDC__
		&& errno!=ENOFILE
		#endif
		#ifdef MAIN
		|| Kaikkimatch
        #endif
        )
        {
FERR:       perror(scorename);
ERR:        CleanUp(errno);
        }
        return;
FAIL:   fclose(fp);
        errno = EINVFMT;
        goto FERR;
	}

	GetModel();

    Summa[0] = 0;

    #ifdef MAIN
    ScoreWasFound=0;
    #endif
    for(Summattu=0, CkSum=0;;)
    {
        int i;
        if(!fgets(Buf, sizeof(Buf), fp))break;
        if(Buf[0]=='#' || Buf[0]==';')continue; // Remark

        i = strlen(Buf);
        while(i && ((Buf[i-1]=='\n')||(Buf[i-1]=='\r')))Buf[--i]=0;

        if(!i)continue;

        if(Buf[0] == '[')
        {
            if(Buf[--i] != ']')Fail("ReadScores: Missing ']'");
            Buf[i] = 0;
            strncpy(SectionName, Buf+1, sizeof(SectionName)-1);
            continue;
        }
        if(strcmp(SectionName, ckhdr))
        {
            AddCheckSum(Buf);

			if(VertaaHeader(SectionName))
            {
                int pos;
                long Score;
                int Mokia, kirjaimia, last10mrs, Sanoja;
                long When;
                char *s;
                sscanf(Buf, ScoreFormat,
                    &pos,&Score,&Mokia,&kirjaimia,
                    &last10mrs,&Sanoja,&When);
                if(pos >= ScoreDepth || pos<0)Fail("ReadScores: Invalid score index");
                Scores[pos].Score = Score;
                Scores[pos].Mokia = Mokia;
                Scores[pos].kirjaimia = kirjaimia;
                Scores[pos].last10mrs = last10mrs;
                Scores[pos].Sanoja = Sanoja;
                Scores[pos].When = When;
                s = strchr(Buf, '&');
                if(!s || strlen(s)>MaxScoreNameLen+1)Fail("ReadScores: Invalid name length");
                // +1 because & should not be counted.
                strcpy(Scores[pos].Name, s+1);
                #ifdef MAIN
                ScoreWasFound=1;
                #endif
            }
        }
        else
        {
            strncpy(Summa, Buf, sizeof(Summa));
            Summattu = 1;
        }
    }
    if(strcmp(Summa, ScoreFormat)) /* Provide a backdoor */
        if(!Summattu || atol(Summa) != CkSum)
        {
            /* DO NOT LEAVE THIS HERE!! */
//          printf("\nReadScores: Invalid password (was %s, should %ld)", Summa, CkSum);
            goto FAIL;
        }
    fclose(fp);
}

#ifdef MAIN
static char JoinInfo[256];
#define SCOREINFO "# File is %s.\n"
#define SCOREINFO2 JoinInfo,
#else
#define SCOREINFO
#define SCOREINFO2
#endif

#define Alkusanat \
    "# WSPEED state file\n" \
    "#\n" \
    "# Generated by "__FILE__", compiled "__DATE__" "__TIME__".\n" \
    "# Latest modification of this file: %s.\n"SCOREINFO \
    "#\n" \
    "# Short explanation of file format:\n" \
    "#   Lines starting with # are comments - not processed\n" \
    "#   [] starts block\n" \
    "#   [Model] starts score block for specified model\n" \
    "#   [%s] block contains value for checking that\n" \
    "#%*sthe file has not been modified by user.\n" \
    "#   Score records contain the following:\n" \
    "#     Index, score, misses, total alphabets typed,\n" \
    "#     ten minute relative speed, total words typed,\n" \
    "#     time() at gameover and player name.\n" \
    "#     Fields are separated by *, except the player name.\n" \
    "#   Dates in the game and in comments below are\n" \
    "#   in mmddyy format and in mm.dd.yyyy format.\n" \
    "# Generally, consider this file as read-only.\n" \
    "# The game itself may do modifications here, not users.\n", \
    Buf, SCOREINFO2 ckhdr, 6+strlen(ckhdr), ""

void WriteOutScores(FILE *fo)
{
	int a, b;
	struct tm *TM;
	char Buf[256];

	for(b=0; b<ScoreDepth; b++)
		if(!Scores[b].Score)break;

	if(b)
	{
		time_t First=0x7FFFFFFFLU, Last=0;
		long Score=0;
		long Mokia=0;
		long kirjaimia=0;
		long last10mrs=0;
		long sanoja=0;
		#ifdef MAIN
		if(Verbose)printf("Writing %d entr%s.\n", b, b==1?"y":"ies");
		#endif
		fprintf(fo, "\n[%s]\n", Header);
        for(a=0; a<b; a++)
        {
            sprintf(Buf, ScoreFormat, a,
                Scores[a].Score,
                Scores[a].Mokia,
                Scores[a].kirjaimia,
                Scores[a].last10mrs,
                Scores[a].Sanoja,
                Scores[a].When);
            strcat(Buf, "&");
            strcat(Buf, Scores[a].Name);
            AddCheckSum(Buf);
            fprintf(fo, "%s\n", Buf);
            Score += Scores[a].Score;
            Mokia += Scores[a].Mokia;
            kirjaimia += Scores[a].kirjaimia;
            last10mrs += Scores[a].last10mrs;
            sanoja += Scores[a].Sanoja;
            if(Scores[a].When>Last)Last=Scores[a].When;
            if(Scores[a].When<First)First=Scores[a].When;
        }
        fprintf(fo, "# %ld*%ld*%ld*%ld*%ld  -- averages\n",
            Score/b, Mokia/b, kirjaimia/b,
			last10mrs/b, sanoja/b);

        TM = localtime(&First);
        if(First==Last)
            strftime(Buf, sizeof Buf, "# Entry time: %d.%m.%Y %H:%M:%S\n", TM);
        else
        {
			strftime(Buf, sizeof Buf, "# First entry:  %d.%m.%Y %H:%M:%S\n", TM);
            fprintf(fo, Buf);
            TM = localtime(&Last);
            strftime(Buf, sizeof Buf, "# Latest entry: %d.%m.%Y %H:%M:%S\n", TM);
        }
        fprintf(fo, Buf);
    }
}

#ifndef MAIN
void FindAverage10mrs(const char *Who, int *Local, int *Global)
{
    FILE *fp = fopen(scorename, "rt");

	char Buf[256];
	char SectionName[sizeof Header];

    int LocalCount=0;
	int GlobalCount=0;
    long LocalSum = 0;
    long GlobalSum= 0;

    if(!fp)
    {
        if(errno!=ENOENT
        #ifdef __BORLANDC__
        && errno!=ENOFILE
        #endif
        )
        {
FERR:       perror(scorename);
ERR:        CleanUp(errno);
        }
        return;
	}

	GetModel();

    for(;;)
    {
        int i;
        if(!fgets(Buf, sizeof(Buf), fp))break;
        if(Buf[0]=='#' || Buf[0]==';')continue; // Remark

        i = strlen(Buf);
        while(i && ((Buf[i-1]=='\n')||(Buf[i-1]=='\r')))Buf[--i]=0;

        if(!i)continue;

        if(Buf[0] == '[')
        {
            Buf[--i] = 0; // Remove ']' (should exist)
            strncpy(SectionName, Buf+1, sizeof(SectionName)-1);
            continue;
        }
        if(strcmp(SectionName, ckhdr))
        {
            long l;
            int a, Ten;
            sscanf(Buf, ScoreFormat, &a,&l,&a,&a,&Ten,&a,&l);
            char *s = strchr(Buf, '&');
            if(s && strlen(s)<=MaxScoreNameLen+1 && !strcmp(s+1, Who))
            {
				if(VertaaHeader(SectionName))
                {
                    LocalSum += Ten;
                    LocalCount++;
                }
                else
                {
                    GlobalSum += Ten;
                    GlobalCount++;
                }
            }
        }
    }
    fclose(fp);

    *Local = (int)(LocalCount ? LocalSum / LocalCount : 0);
    *Global= (int)(GlobalCount ? GlobalSum / GlobalCount : 0);
}

void WriteScores(void)
{
    char Buf[256];
    char SectionName[64];
    time_t now;
    struct tm *TM;

    FILE *fp=NULL, *fo;

    time(&now);
    TM = localtime(&now);
	strftime(Buf, sizeof Buf, "%b %d %Y %H:%M:%S", TM);

    GetModel();
    CkSum=0;

    if(rename(scorename, tmpname))
    {
        if(errno==ENOENT
        #ifdef __BORLANDC__
        || errno==ENOFILE
        #endif
        )
        {
            if(!(fo=fopen(scorename, "wt")))goto FERR;
            fprintf(fo, Alkusanat);
            goto SkipReading;
        }
        fclose(fo);
        printf("For %s: ", tmpname);
        goto FERR;
    }

    fp = fopen(tmpname, "rt");

    if(!fp)
    {
FERR:   perror(scorename);
ERR:    CleanUp(errno);
        return;
FAIL:   if(fp)fclose(fp);
        errno = EINVFMT;
        goto FERR;
    }

    if(!(fo=fopen(scorename, "wt")))goto FERR;
    fprintf(fo, Alkusanat);

	strcpy(SectionName, Header); /* For comments to work correctly */
    for(;;)
    {
        int i;
        if(!fgets(Buf, sizeof(Buf), fp))break;
        if(Buf[0]=='#' || Buf[0]==';')
        {
			if(strcmp(SectionName, ckhdr) && !VertaaHeader(SectionName))
                fprintf(fo, "%s", Buf);
            continue; // Remark
        }

        i = strlen(Buf);
        while(i && ((Buf[i-1]=='\n')||(Buf[i-1]=='\r')))Buf[--i]=0;

        if(!i)continue;

        if(Buf[0] == '[')
        {
            if(Buf[--i] != ']')Fail("WriteScores: Missing ']'");
            Buf[i] = 0;
            strncpy(SectionName, Buf+1, sizeof(SectionName)-1);
            if(strcmp(SectionName, ckhdr) && !VertaaHeader(SectionName))
                fprintf(fo, "\n[%s]\n", SectionName);
            continue;
        }
        if(strcmp(SectionName, ckhdr) && !VertaaHeader(SectionName))
        {
            /* Jos tietue menee ulos */
            AddCheckSum(Buf);
            fprintf(fo, "%s\n", Buf);
        }
    }
    fclose(fp);

SkipReading:
    WriteOutScores(fo);
    fprintf(fo, "\n[%s]\n%lu\n# End of WSPEED state file.\n", ckhdr, CkSum);

    fclose(fo);
    remove(tmpname);
}

#else // MAIN defined

#define SafeReadScores() {unsigned long Bup=CkSum;ReadScores();CkSum=Bup;}

static void JoinScores(char *Dest, char *f1, char *f2)
{
    FILE *fp, *fo=NULL;
    time_t now;
    struct tm *TM;
    char Buf[256];
    char SectionName[sizeof Header];

    time(&now);
    TM = localtime(&now);
    strftime(Buf, sizeof Buf, "%b %d %Y %H:%M:%S", TM);

    int Modeli=0;

    switch(Original)
    {
        case 2:
            sprintf(JoinInfo, "the original WSpeed score file (distribution version %s)", Version);
            break;
        case 1:
            sprintf(JoinInfo, "updated WSpeed score file (distribution version %s)", Version);
            break;
        default:
            sprintf(JoinInfo, "result from combining %s and %s", f1, f2);
    }

    printf("Combining %s and %s to %s...\n", f1, f2, Dest);

    strcpy(scorename, f2);

    if(Verbose)printf("First pass\n");

    fp = fopen(f2, "rt");
    if(!fp)goto FERR;
    fclose(fp);

    fp = fopen(f1, "rt");
    if(!fp)
    {
FERR:   perror(scorename);
ERR:    if(fo)fclose(fo);
        CleanUp(errno);
        return;
FAIL:   fclose(fp);
        errno = EINVFMT;
        goto FERR;
    }

    if(!(fo=fopen(Dest, "wt")))goto FERR;
    fprintf(fo, Alkusanat);

    for(CkSum=0;;)
    {
        int i;
        if(!fgets(Buf, sizeof(Buf), fp))break;
        if(Buf[0]=='#' || Buf[0]==';')continue; // Remark

        i = strlen(Buf);
        while(i && ((Buf[i-1]=='\n')||(Buf[i-1]=='\r')))Buf[--i]=0;

        if(!i)continue;

        if(Buf[0] == '[')
        {
            if(Modeli)WriteOutScores(fo);
            Buf[--i] = 0; // Remove ']' (should exist)
            strncpy(SectionName, Buf+1, sizeof(SectionName)-1);
            if((Modeli=IsModel(SectionName))!=0)
            {
                if(Verbose)printf("Combining '%s'... ", SectionName);
                Understand(SectionName);
                SafeReadScores();
            }
            continue;
        }
        if(Modeli)
        {
            int x;
            long Score;
            int Mokia, kirjaimia, last10mrs, Sanoja;
            long When;
            char *s;
            sscanf(Buf, ScoreFormat,
                &x,&Score,&Mokia,&kirjaimia,
                &last10mrs,&Sanoja,&When);
            s = strchr(Buf, '&');
            if(!s || strlen(s)>MaxScoreNameLen+1)Fail("JoinScores: Invalid name length");
            // +1 because & should not be counted.
            for(x=0; x<ScoreDepth; x++)
            {
                if(Scores[x].Score == Score
                && Scores[x].Mokia == Mokia
                && Scores[x].kirjaimia == kirjaimia
                && Scores[x].last10mrs == last10mrs
                && Scores[x].Sanoja == Sanoja
                && Scores[x].When == When
                && !strcmp(Scores[x].Name, s+1))
                    break; /* Ei tehd„ duplikaatteja. */
                if(Score > Scores[x].Score || !Scores[x].Score)
                {
                    int y;
                    for(y=ScoreDepth-1; y>x; y--)Scores[y] = Scores[y-1];
                    Scores[x].Score     = Score;
                    Scores[x].Mokia     = Mokia;
                    Scores[x].kirjaimia = kirjaimia;
                    Scores[x].last10mrs = last10mrs;
                    Scores[x].Sanoja    = Sanoja;
                    Scores[x].When      = When;
                    // +1 because & should not be counted.
                    strcpy(Scores[x].Name, s+1);
                    break;
                }
            }
        }
    }
    fclose(fp);

    if(Verbose)printf("Second pass\n");

    fp = fopen(f2, "rt");
    if(!fp)goto FERR;

    strcpy(scorename, f1);

    for(;;)
    {
        int i;
        if(!fgets(Buf, sizeof(Buf), fp))break;
        if(Buf[0]=='#' || Buf[0]==';')continue; // Remark

        i = strlen(Buf);
        while(i && ((Buf[i-1]=='\n')||(Buf[i-1]=='\r')))Buf[--i]=0;

        if(!i)continue;

        if(Buf[0] == '[')
        {
            if(Modeli)WriteOutScores(fo);
            Buf[--i] = 0; // Remove ']' (should exist)
            strncpy(SectionName, Buf+1, sizeof(SectionName)-1);
            if((Modeli=IsModel(SectionName))!=0)
            {
                Understand(SectionName);
                SafeReadScores();
                if(ScoreWasFound)
                    Modeli=0;
                else
                    if(Verbose)printf("Combining '%s'... ", SectionName);
            }
            continue;
        }
        if(Modeli)
        {
            int x;
            long Score;
            int Mokia, kirjaimia, last10mrs, Sanoja;
            long When;
            char *s;
            sscanf(Buf, ScoreFormat,
                &x,&Score,&Mokia,&kirjaimia,
                &last10mrs,&Sanoja,&When);
            s = strchr(Buf, '&');
            if(!s || strlen(s)>MaxScoreNameLen+1)Fail("JoinScores: Invalid name length");
            // +1 because & should not be counted.
            for(x=0; x<ScoreDepth; x++)
            {
                if(Scores[x].Score == Score
                && Scores[x].Mokia == Mokia
                && Scores[x].kirjaimia == kirjaimia
                && Scores[x].last10mrs == last10mrs
                && Scores[x].Sanoja == Sanoja
                && Scores[x].When == When
                && !strcmp(Scores[x].Name, s+1))
                    break; /* Ei tehd„ duplikaatteja. */
                if(Score > Scores[x].Score || !Scores[x].Score)
                {
                    int y;
                    for(y=ScoreDepth-1; y>x; y--)Scores[y] = Scores[y-1];
                    Scores[x].Score     = Score;
                    Scores[x].Mokia     = Mokia;
                    Scores[x].kirjaimia = kirjaimia;
                    Scores[x].last10mrs = last10mrs;
                    Scores[x].Sanoja    = Sanoja;
                    Scores[x].When      = When;
                    // +1 because & should not be counted.
                    strcpy(Scores[x].Name, s+1);
                    break;
                }
            }
        }
    }
    fclose(fp);

    if(Modeli)WriteOutScores(fo);
    fprintf(fo, "\n[%s]\n%lu\n# End of WSPEED state file.\n", ckhdr, CkSum);

    if(!Original)printf("Done.\n");
    fclose(fo);
}

int main(int argc, const char *const *argv)
{
    static char File1[128] = "";
    static char File2[128] = "";

    int a;
    const char *s;
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
P1:                     printf(
                            "ScoreCom version %s - written by Bisqwit - see wspeed.txt.\n"
                            "\nDescription #1\n"
                            "  Verifies WSpeed score file\n\n"
                            "  Usage: ScoreCom [-v] <file.rec>\n"
                            "\n"
                            "  Verifies that the score file is ok.\n"
                            "\nDescription #2\n"
                            "  Conjoins two WSpeed score files\n\n"
                            "  Usage: ScoreCom [-v] <file1.rec> <file2.rec>\n"
                            "\n"
                            "  The program creates %s as result.\n"
                            "  You need to manually rename it as wspeed.rec\n"
                            "  to use it in the game.\n",
                            Version, mixscorename
                        );
                        CleanUp(0);
                    case 'V':
                        Verbose++;
                        break;
                }
                if(*s)s++;
            }
        }
        else if(!strcmp(s, "(C)"))Original++;
        else
        {
            while(*s==' ' || *s=='\t')s++;
            if(*s)
                if(!File1[0])strcpy(File1, s);
                else if(!File2[0])strcpy(File2, s);
                else goto P1;
        }
    }

    if(!File1[0])goto P1;

    if(!File2[0])
    {
        strcpy(scorename, File1);
        printf("Checking '%s'... ", File1);
        Kaikkimatch = 1;
        ReadScores();
        printf("Ok.\n");
    }
    else
        JoinScores(mixscorename, File1, File2);

    CleanUp(0);
    return 0;
}

#endif
