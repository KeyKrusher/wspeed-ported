#ifdef __BORLANDC__
#include <io.h>
#include <alloc.h>
#include <process.h>
#include <conio.h>
#else
#include <unistd.h>
#ifdef DJGPP
#include <process.h>
#include <conio.h>
#endif
#endif
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "music.h"

#ifdef __MSDOS__
static const char NULFILE[] = "NUL";
#else
static const char NULFILE[] = "/dev/null";
#endif

#if defined(__BORLANDC__)
#define WINDOWS_WORKAROUND_SUPPORT
#endif

#if defined(__BORLANDC__) && defined(PLAYSELF)
/* Disable some linker warnings...*/
#define getch Gitch
#define kbhit KbHit
#endif

static void DoResume(void)
{
    MusData.driver->reset();
}

#ifdef WINDOWS_WORKAROUND_SUPPORT
static int windowsfix = 0;
static void _fastcall _OldI21(void) {__emit__(0,0,0,0);}
static void _fastcall _I21RFlag(void) {__emit__(0U);}
static void _fastcall _I21Count(void) {__emit__(0U);}

extern unsigned _stklen;
#define _SaveReg() _asm {db 0x66;pusha;push ds;push es;mov ax,seg _stklen;mov ds,ax}
#define _RestReg() _asm {pop es;pop ds;db 0x66;popa}

static void _fastcall NewI21(void)
{
    #define Count (*((short *)_I21Count))
    #define RFlag (*((short *)_I21RFlag))
    _asm pop si //FIXME
    if(!Count)
    {
        Count++;
        // if(_AX == 0xE00F)_AX = 0x4C31; /* Hihih - SeeAlso: AX=E00Fh */
        if(_AH == 0x4C || _AX == 0xE00F)
        {
            if(!RFlag)
            {
                _SaveReg();
                RFlag = 100;
                _RestReg();
            }
        }
        else if(RFlag)
        {
            _SaveReg();
            RFlag--;
            if(!RFlag)DoResume();
            _RestReg();
        }
        Count--;
    }
    _asm jmp dword ptr _OldI21
}
#endif

/* Path variables */
static char Intgen[256];
static char TempBuf[256];
extern char Trig[18];

static void DOS(void)
{
#ifdef linux
    system("");
#else
    char *s = getenv("COMSPEC");

#ifdef WINDOWS_WORKAROUND_SUPPORT
    if(windowsfix)
    {
        _asm { xor ax, ax; mov es, ax }
        _asm mov eax, dword ptr es:[0x21*4]
        _asm mov dword ptr _OldI21, eax
        _asm { mov ax, seg NewI21; shl eax, 16; mov ax, offset NewI21 }
        _asm mov dword ptr es:[0x21*4], eax
    }
#endif

    printf("\rType EXIT to return to MIDINS%49s", "\n");
    spawnl(P_WAIT, s, "command", NULL);

#ifdef WINDOWS_WORKAROUND_SUPPORT
    if(windowsfix)
    {
        _asm { xor ax, ax; mov es, ax }
        _asm mov eax, dword ptr _OldI21
        _asm mov dword ptr es:[0x21*4], eax
    }
#endif
#endif
}

#ifdef __BORLANDC__
unsigned char _Cdecl __inportb__(unsigned __portid);
#define inportb(__portid) __inportb__(__portid)

#ifdef PLAYSELF
static int kbhit(void)
{
    _asm mov ah, 1
    _asm int 0x16
    _asm jz P1
    return 1;
P1:    return 0;
}

int getch(void)
{
    _asm xor ax, ax
    _asm int 0x16
    return _AX&255;
}
#endif
#endif

#ifdef PLAYSELF
static int Verbose = 1;
#else
static int Verbose = -1;
#endif
static int Quiet   = 0;

#if 0
/* GOTO-hirvitys */
int main(int argc, char **argv)
{
    long size;
    FILE *fp;
    char far *a;
    int b=0;
    char m[512];
#ifdef PLAYSELF
    char *nms = PLAYSELF;
    long Start;
#else
    char *nms = "nesmusa.$$$";
    char *fn;
    int fnMode=0;
    int inames=0;
#endif
    int devtold=0;
    enum{mdS3M,mdC};

    int Hack=0;

    Pathi(Intgen, 255, "PATH", "intgen.exe");

#ifdef PLAYSELF
    printf("This selfplayer uses The MidiS3M subsystem (C) 1992,2000 Bisqwit\n"
           "Usage: %s [-Vdomalqw] [-r#]\n", nms);
#else
    printf("Usage: MIDINS [-vVdomalqw] [-r#] (ginstru | (s3m | c))\n");
    fn=NULL;

    MusData.LinearMidiVol = 0;
#endif

    for(Rate=1000; --argc; )
    {
        a = *++argv;
        if(*a=='-' || *a=='/')
        {
            while(*++a)
                switch(*a)
                {
                    case 'q': Verbose=0; break;
                    case 'r':
#ifndef PLAYSELF
                        if(a[1]=='r')
                            a++, inames=1;
                        else
#endif
                            Rate=(int)strtol(a+1, &a, 10), a--;
                        break;
                    case 'v':
#ifdef PLAYSELF
//fall-through
#else
                        Verbose=1;
                        break;
#endif
                    case 'V': Verbose=2; break;
                    case 'o': devtold++, dev=SelectOPL; break;
#if SUPPORT_AWE
                    case 'a': devtold++, dev=SelectAWE; break;
#endif
#if SUPPORT_MIDI
#if SUPPORT_DSP
                    case 'd': devtold++, dev=SelectDSP; break;
#endif
                    case 'm': devtold++, dev=SelectMPU; break;
#endif
                    case 'w':
#ifdef WINDOWS_WORKAROUND_SUPPORT
                        windowsfix=1;
#endif
                        break;
                    case 'u': Hack=1; break;
#ifndef PLAYSELF
                    case 'l': MusData.LinearMidiVol=1; break;
#endif
                    case '\n': break;
                    default:
                    {
                        printf("Invalid switch: -%c'\n", *a);
                        goto Err;
                    }
                }
            continue;
        }
        #ifdef PLAYSELF
        goto Err;
        #else
        if(strstr(a, ".exe")||strstr(a, ".EXE"))
        {
            printf("How about trying to run the program (%s) itself?\n", a);
            return -1;
        }
        if(strstr(a, ".s3m")||strstr(a, ".S3M")
        || strstr(a, ".mod")||strstr(a, ".MOD")){if(fn)goto Err;fn=a;fnMode=mdS3M;}
/*      else if(strstr(a, ".mid")||strstr(a, ".MID")||
                strstr(a, ".rmi")||strstr(a, ".RMI")){if(fn)goto Err;fn=a;fnMode=mdMID;}
*/      else if(strstr(a, ".c")||strstr(a, ".C")){if(fn)goto Err;fn=a;fnMode=mdC;}
        else
        {
            if(b)goto Err;
            b = atoi(a + 2);
            if(toupper(*a) != 'G')goto Err;
            if(toupper(a[1])=='P')b += 128;
            else if(toupper(a[1])!='M')goto Err;
            if(b<1||(b>128&&b<128+35)||(b>80+128))
            {
                printf("Invalid instrument number.\n");
                return -1;
            }
        }
        #endif
    }

    #ifndef PLAYSELF
    if(!b && !fn)goto Err;
    #endif
    if(!devtold)goto Err;

    if(Verbose==2)
    {
        /* Large message - let an external program print it. (Hack!) */
        b = spawnl(P_WAIT, Intgen, "midins", "<0", NULL);
        if(b < 0)
        {
IntGenSpawnError:
            printf("spawn(%s)) error: %s", Intgen, strerror(errno));
            return -1;
        }
    }

    if(Rate<19 || Rate>32766)
    {
        printf("Rate (-r) must be 19..32766\n");
        return -1;
    }

#ifndef PLAYSELF
    if(fn)
    {
        a=Pathi(TempBuf,255,"MOD", fn);
        if(fnMode==mdS3M)goto S3M;
        if(fnMode==mdC)goto C;
        printf("Internal error\n");
        if(Verbose==2)printf("*** The program has been cracked.\n");
        return -2;
    }

    /* This is really a hack. Why to waste memory? */
    sprintf(m, "%d:%d:%d:%d", Verbose, b, dev, devtold);
    b = spawnl(P_WAIT,
        Intgen,
        "midins", "<1", a,
        Pathi(TempBuf,255,"PATH","fmdrv.dat"),
        m, NULL);
    if(b == 2)goto Err;
    if(b < 0)goto IntGenSpawnError;

    return 0;

C:    /* Another hack... */
    sprintf(m, "%d", Verbose);
    b = spawnl(P_WAIT,
        Intgen,
        "midins", "<2", a, m, NULL);
    if(b < 0)goto IntGenSpawnError;
    if(b)return b;

    a = "you have dropped coffee into your computer.";

    goto S3MSkip;

S3M:if(access(a, 4))
    {
        printf("File not found (%s).\n", a);
        return -1;
    }

    {
        char opt[16] = "-";
        if(inames)strcat(opt, "i");
        if(Verbose > 0)strcat(opt, "vb");
        if(MusData.LinearMidiVol)strcat(opt, "l");
        strcat(opt, "n");
        b =    spawnl(P_WAIT, Intgen, "midins", opt, a, "$", "$", NULL);
    }

    if(b < 0)goto IntGenSpawnError;
    a = "intgen error.";
#endif /* ndef PLAYSELF */
S3MSkip:
    fp = fopen(nms, "rb");
    if(!fp)
    {
        printf("%s\n", a);
        return -1;
    }
#ifdef PLAYSELF
    for(;;)
    {
        Start = ftell(fp);
        if(fread(m, 1, 6, fp) < 6){fclose(fp);goto FmtError;}
        if(strncmp(m, "MZ", 2))break;
        fseek(fp, 0L, SEEK_END);
        size = ftell(fp) - Start;
        fseek(fp, Start +
            (((word *)m)[1] + 512UL * (((word *)m)[2] - 1)),
            SEEK_SET);
    }
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp) - Start;
    fseek(fp, Start, SEEK_SET);
#else
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);
#endif
    if(!size)
    {
        fclose(fp);
        unlink(nms);
        goto FmtError;
    }
    a = farmalloc(size);
    if(!a)
    {
        fclose(fp);
        printf("Out of memory.\n");
        return -1;
    }
    if(fread(a, (size_t)size, 1, fp) < 1)
    {
        printf("fread(%s) error: %s", nms, strerror(errno));
        fclose(fp);
        return -1;
    }
    fclose(fp);
#ifndef PLAYSELF
    unlink(nms);
    if(Verbose>0)printf("Read and deleted %s (%ld bytes)\n", nms, size);
#endif

    if(a[1] > 2)
    {
#ifdef PLAYSELF
        if(Verbose>0)printf("Song name: %Fs (%ld bytes)\n", a, size);
#endif
        a += _fstrlen(a)+1;
    }
#ifdef PLAYSELF
    else if(Verbose>0)
        printf("Song size: %ld bytes\n", size);
#endif

    if(!(
         ((InternalHdr far *)a)->InsNum == ((InternalHdr far *)a)->Ins2Num
      && ((InternalHdr far *)a)->InsNum < MAXINST
      ) )
    {
FmtError:
        printf("File format error.\n");
        return -1;
    }

    InitMusic(NO_AUTODETECT);

    if(Verbose>0)
    {
        printf("Device: ");
        fflush(stdout);
    }

    if(SelectMusDev(dev) < 0)
    {
        printf("Problem: %s\n", SoundError);
        goto DonePlay;
    }

    if(Verbose>0)
        printf("%s",
            dev==SelectOPL?"OPL":
#if SUPPORT_AWE
            dev==SelectAWE?"AWE":
#endif
#if SUPPORT_MIDI
            dev==SelectMPU?"MPU":
#if SUPPORT_DSP
            dev==SelectDSP?"DSP":
#endif
#endif
            "Unknown");
    fflush(stdout);

    if(Hack)
    {
        Rate = 32767;
        /* To not start playing immediately.
         * Sets rate=1000 in StartS3M call.
         */
    }
    StartS3M(a);

    printf("\n");

#ifndef PLAYSELF
    if(Verbose)
        printf("Assuming volumes: %s\n",
            MusData.LinearMidiVol?"linear":"adlib");
#endif

    if(Hack)
    {
        StopS3M();
        IRQActive = 2;
    }

    if(Verbose)
    {
#ifndef PLAYSELF
        char *idata[2] = {NULL,NULL};
#endif
        int cr=MusData.posi.Row, co=MusData.posi.Ord, cp=MusData.posi.Pat;

#ifndef PLAYSELF
        if(inames)
        {
            register size_t size = MusData.Hdr->ChanCount[0] * 40 + 1;
            idata[0] = (char *)malloc(size);
            idata[1] = (char *)malloc(size);
            if(!idata[1] || !idata[0])
            {
                if(idata[0])free(idata[0]);
                if(idata[1])free(idata[1]);
                idata[0] = idata[1] = NULL;
            }
        }
#endif

Redraw: printf("(Q)uit, (P)ause, (D)os, +-Position\n");

        printf("\rCurRow: %02X CurOrd: %02d CurPat: %02d - Press esc",
                MusData.posi.Row, MusData.posi.Ord, MusData.posi.Pat);

#ifndef PLAYSELF
        if(idata[0])
        {
            int a;
            for(a=0; a <= MusData.Hdr->ChanCount[0]; a++)putchar('\n');
            gotoxy(1, wherey() - a);
            memset(idata[0], ' ', (a-1)*40);
            memset(idata[1], ' ', (a-1)*40);
        }
#endif

        for(;;)
        {
            #ifdef __BORLANDC__
            /* Not in DOSEMU */
            /*if(_fmemcmp((char far *)0xF000FFF5UL, "02/25/93", 8))_asm hlt*/
            #endif

            #if 1
            if(MusData.posi.Row!=cr || MusData.posi.Ord!=co || MusData.posi.Pat!=cp)
            {
                printf("\rCurRow: %02d ", cr=MusData.posi.Row);
            }
            if(MusData.posi.Ord!=co || MusData.posi.Pat!=cp)
                printf("CurOrd: %02X ", co=MusData.posi.Ord);
            if(MusData.posi.Pat!=cp)
                printf("CurPat: %02d ", cp=MusData.posi.Pat);
            #ifndef PLAYSELF
            if(idata[0])
            {
                int a, b, x, y;
                for(a=0; a<MusData.Hdr->ChanCount[0]; a++)
                {
                    char *t = idata[0] + a*40;
                    b = MusData.CurInst[a];
                    sprintf(t+11,
                        "[%-27s]", b
                            ?    MusData.Instr[b]->insname
                            :    "");

                    if(t[5]!=' ' || Trig[a]==126)
                    {
                        for(b=0; b<10; b++)t[b]=t[b+1];
                        t[b]=' ';
                    }
                    else if(Trig[a])
                    {
                        memcpy(t, "ðððððð===--", 11);
                        Trig[a] = 126;
                    }
                }
                x = wherex();
                y = -1;
                for(b=0; b<a*40; b++)
                {
                    if(idata[1][b] != idata[0][b])
                    {
                        int X = b%40 + 1;
                        int Y = b/40;

                        if(x!=X || y!=Y)
                        {
                            gotoxy(X, wherey() + Y - y);
                            /* if(X < x) putch('\r'), x=1; */
                            x=X, y=Y;
                        }

                        putch(idata[1][b] = idata[0][b]);
                        x++;
                    }
                }
                gotoxy(1, wherey() - y - 1);
            }
            #endif
            #endif

            if(Hack)
                if(!MusData.Paused)
                    TimerHandler();

            fflush(stdout);

            if(kbhit())
            {
                switch(getch())
                {
                    case 'D':
                    case 'd':
                        #ifndef PLAYSELF
                        if(inames)
                            gotoxy(1, wherey() + MusData.Hdr->ChanCount[0]);
                        #endif
                        DOS();
                        __ResumeS3M(); /* Fix */
                        goto Redraw;
                    case 'Q':
                    case 'q':
                    case 27:
                        #ifndef PLAYSELF
                        if(inames)
                            gotoxy(1, wherey() + MusData.Hdr->ChanCount[0]);
                        #endif
                        goto DonePlay;
                    case 'P':
                    case 'p':
                        PauseS3M();
                        break;
                    case '+':
                        MusSeekNextOrd();
                        break;
                    case '-':
                        MusSeekPrevOrd();
                }
            }
            /*
            if(!Hack)
            {
                if(!MusData.Paused)
                {
                    delay(30);
                }
            }
            else
            {
                #undef delay
                delay(1);
            }

                This delay-thingie seems to _need_ _more_
                processing power than just running

            */
        }
    }
    else
    {
        printf("Playing, press any key to end...");
        fflush(stdout);
        if(!getch())getch();
    }

DonePlay:
    farfree(a);

    ExitMusic();

    printf("\n");

    return 0;

Err:
#ifdef PLAYSELF
    printf(
#if SUPPORT_AWE
        "\t-a for AWE play (EMU8000 chip)\n"
#endif
#ifdef WINDOWS_WORKAROUND_SUPPORT
        "\t-w for Windows sound bug workaround\n"
#endif
        "\t-o for AdLib play (OPL2 chip, FM)\n"
        "\t-v for verbose\n"
        "\t-q for mostly quiet\n"
        "\t-r# to set timer irq rate (default: 1000 (Hz))\n"
        "\t-u for software timer (disables -r#)\n"
#if SUPPORT_MIDI
        "\t-m for General MIDI (MPU-401 standard)\n"
#endif
    );
#else /* not playself: */
    printf(
        "\n"
        "Required files:"
#ifdef WINDOWS_WORKAROUND_SUPPORT
        "\t\t\t-w for Windows sound bug workaround"
#endif
        "\n"
        "ÀÄmidins.exe\t\t\t-o for AdLib play (OPL2 chip, FM)\n"
        "   ÃÄfmdrv.dat\t\t\t-v for verbose (excludes -q)\n"
        "   ÀÄintgen.exe\t\t\t-l is for no adlib volume conversion\n"
#if SUPPORT_AWE
        "\t\t\t\t-a for AWE play (EMU8000 chip)\n"
#endif
        "\t\t\t\t-m for General MIDI (MPU-401 standard)\n"
        "\t\t\t\t-q for minimum text output (excludes -v,-V)\n"
        "\t\t\t\t-rr for even more verbose than -vvv\n"
        "\t\t\t\t-r# to set timer irq rate (default: 1000 (Hz))\n"
        "\t\t\t\t-u to set software timer\n"
        "\nginstru may be either GM## (melodic) or GP## (percussion)"
        "\nUse uppercase -V for troubleshooting verbosity."
        "\n");
#endif /* playself or not */

    return -1;
}
#else

#include "devopts.inc"
int main(int argc, const char *const *argv)
{
    int dev = 'o';
    const char *infn = NULL, *outfn = NULL;
    const char *A0 = *argv;
    
    for(; --argc; )
    {
        const char *a = *++argv;
        if(*a=='-'
        #if defined(__BORLANDC__)||defined(DJGPP)
        || *a=='/'
        #endif
        )
        {
            while(*++a)
            {
                const char *t = a;
                if(finddriver(*a))
                {
                    dev = *a;
                    continue;
                }
                switch(*a)
                {
                    case 'v': Verbose++; break;
                    case 'q':
						dup2(open(NULFILE, O_WRONLY), ++Quiet);
                        break;
                    case 'w':
#ifdef WINDOWS_WORKAROUND_SUPPORT
                        windowsfix=1;
#endif
                    	break;
                    case '?':
                    case 'h':
Help:                        printf(
                            "MIDINS v"VERSION" - Copyright (C) 1992,2000 Bisqwit (http://iki.fi/bisqwit/)\n"
                            "A player.\n\n"
                            "Something that is not yet quite nothing.\n"
                            "This program is under construction.\n"
                            "\n"
                            "Usage:\t%s [<options>] <musicfile>\n"
                            "\t-v\tVerbosity++\n"
#ifdef WINDOWS_WORKAROUND_SUPPORT
				        	"\t-w\tfor Windows sound bug workaround\n"
#endif
							"\t-q\tDecrease text output (-qq = not even error messages)\n",
                            A0);
                        devoptlist();
                        return 0;
                    default:
                        printf("%s: Skipping unknown switch `-", A0);
                        while(t != a)printf("%c", *t++);
                        printf("'\n");
                }
            }
        }
        else if(!infn)infn = a;
        else if(!outfn)outfn = a;
        else
            printf("%s: %sthird filename, '%s'\n",
                A0,
                "I don't know what to do with the ",
                a);
    }
    if(!outfn)outfn = "nesmusa.$$$";
    if(!infn)goto Help;
    
    /* FIXME: Put the player here! :) */
    
    return 0;
}

#endif
