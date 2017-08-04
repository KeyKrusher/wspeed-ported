#ifndef __MUSIC_H
#define __MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
        IMPORTANT NOTICE:
                Word alignment is our enemy
*/

#ifndef SUPPORT_AWE
 #define SUPPORT_AWE 1
#endif

#ifndef SUPPORT_DSP
 #define SUPPORT_DSP 0
#endif

#ifndef SUPPORT_MIDI
 #define SUPPORT_MIDI 1  /* To do GM_Byte calls or not?... */
#endif

#if defined(__GNUC__) || !SUPPORT_MIDI
 #undef SUPPORT_AWE
 #define SUPPORT_AWE 0
 #undef SUPPORT_DSP
 #define SUPPORT_DSP 0
#endif

#define SUPPORT_OPL3 SUPPORT_MIDI

#ifndef VIBRATO
 #define VIBRATO 1      /* Does not work correctly yet but widely used */
#endif
#ifndef RETRIG
 #define RETRIG 1       /* Not used much, but works goodly enough */
#endif

#ifdef __GNUC__
#define far
#endif

#ifdef DJGPP
#include <allegro.h>
#endif

#ifndef __GNUC__
/* Useful macros for system functions. Only for real/v86 mode, though. */
#define GetIntVec(a) (IRQType)(*(long far *)((a)*4))
#define SetIntVec(a,b) (*(long far *)((a)*4))=(long)((void far *)(b))
#endif

/* Useful typedefs. Extremely dangerous to modify. */
typedef unsigned long dword;
typedef unsigned short word;
typedef unsigned char byte;

/* Internal constants - should not be modified much */
#define MAXINST 100
#define MAXPATN 100
#define MAXORD  256 /* Unused in music.c */
#define MAXCHN  15  /* Remember that the percussion channel is reserved! */

/* Almost as accurate speed constant as possible. Should not be modified. */
/*#define SPEEDCONST (0x7D/0x06/8) //By T7D A06, we play 8 notes in second*/
/* 26.9.1998 Bisqwit removed SPEEDCONST. Reason: Value is 2.5;            */
/* it is better to use (x+(x<<2))/(d+d) than to use x*2.5/d,              */
/* when the x and d are integers.                                         */

/* Useful constants for InitMusic() */
#define NO_AUTODETECT 0
#define AUTODETECT 1

/* Useful constants for SelectMusDev() */
enum cardcs {
    SelectNone=0, SelectSilence=0
    ,SelectOPL
#if SUPPORT_MIDI
    ,SelectMPU
#if SUPPORT_DSP
    ,SelectDSP
#endif
#if SUPPORT_AWE
    ,SelectAWE
#endif
#endif
};

/* Tries to resume stopped music: don't use unless you really need it */
void __ResumeS3M(void);

/* InitMusic() probes cards if nonzero parameter given */
void InitMusic(int Autodetect);

/* Selects music playback device. See Selectxxx -defines above.
   If nothing given, selects OPL if exists. If the selected device
   does not exist or value is invalid, autoselects silence.
   Important notice: Selecting something undoes all probing of
   other playback devices. So, if the selection should be changed,
   the devices should be re-probed with InitMusic() or
   xxxFound()-series functions.
   Another important notice: If multiple playback devices have been
   succesfully probed and no call to SelectMusDev() is issued,
   then the music is played with all detected playback devices.
   Return value nonzero if selection failed.
*/
int SelectMusDev(enum cardcs Selection); /* in m_opl.c to save space */

/* StartS3M(s) stars music pointed by s */
void StartS3M(const char far *s);

/* PauseS3M() toggles music pause */
void PauseS3M(void);

/* StopS3M() ends music */
void StopS3M(void);

/* ExitMusic() releases the sound system */
void ExitMusic(void);

/* TimerHandler() handles one tick (normally automatic) */
void TimerHandler(void);

/*****************************************/

#if SUPPORT_MIDI
#if SUPPORT_DSP
extern word DSPBase;
extern int DSP_Found(void);
extern int DSP_Reset(void);
extern int IsDSPThere;
#endif

extern word MPUBase;
extern int MPU_Found(void);
extern int MPU_Reset(void);
extern int IsMPUThere;

typedef void (*MidiHandler)(byte a);
extern MidiHandler MPU_Byte;
#endif

extern word OPLBase;
extern int OPL_Reset(void);
extern int OPL_Found(void);
extern int IsOPLThere;

extern void OPL_Byte(byte Index, byte Data);

#if SUPPORT_AWE
extern int AWE_Found(void);
extern int AWE_Reset(void);
extern int IsAWEThere;
#endif

/*****************************************/

typedef struct
{
    char Name[28];
    char CtrlZ;
	char FileType;  /* 16 = S3M */
	word Filler1;
	signed short OrdNum;
					/* Number of orders, is even                         */
					/* Signed to "fix" a poor                            */
					/* designation in intgen (-p switch)                 */
	word InsNum;    /* Number of instruments                             */
	word PatNum;    /* Number of patterns                                */
	word Flags;     /* Don't care, ei mit„„n t„rke„„                     */
	word Version;   /* ST3.20 = 0x1320, ST3.01 = 0x1301                  */
	word SmpForm;   /* 2 = Unsigned samples, don't care, ei adlibin asia */
	char Ident[4];  /*"SCRM"                                             */
	char GVol;      /* Global volume, don't care, ei vaikuta adlibbeihin */
	byte InSpeed;   /* Initial speed                                     */
					/*   &128 = linearmidivol                            */
	char InTempo;   /* Initial tempo                                     */
	char MVol;      /* Master volume, don't care, ei vaikuta adlibbeihin */
	char UC;        /* Ultraclick removal, don't care, gus-homma         */
	char DP;        /* Default channel pan, don't care, gussiin sekin    */
	char Filler2[10];
	char Channels[32]; /* Channel settings, 16..31 = adl channels */
}S3MHdr;

typedef struct
{
	char Typi;          /*2 = AMel           1           */
	char DosName[13];   /*AsciiZ file name   13          */
						/* Temporary used by intgen:     */
						/*  [0]=GM (unsigned)            */
						/*  [1]=ft (signed)              */
						/*  [2]=doublechannel flag       */
	short ManualScale;  /* This is actually part of the  */
						/* filename, but this is now for */
						/* intgen temporary.             */
	char D[12];         /*Identificators     12          */
	char Volume;        /*                   1           */
	char Filler1[3];    /*                   3           */
	word C2Spd;         /*8363 = normal..?   2           */
	char Filler2[14];   /* May contain:      14          */
						/*  [2]=GM (unsigned)            */
						/*  [3]=ft (signed)              */
	char Name[28];      /*AsciiZ sample name 28          */
	char Ident[4];      /*"SCRI"             4           */
}ADLSample;             /*                   80 = 0x50   */

#ifdef __GNUC__
 #define EMPTYARRAY 0
#else
 #define EMPTYARRAY
#endif
 
typedef struct
{
    word OrdNum;      /* Number of orders, is even                */
                      /* If bit 8 (&256) is set, there exist no   */
                      /* notes between 126..253 and the 7th bit   */
                      /* of note value can be used to tell if     */
                      /* there is instrument number.              */
    word InsNum;      /* Number of instruments                    */
    word PatNum;      /* Number of patterns                       */
                      /* If bit 9 set, all instruments have       */
                      /* the insname[] filled with AsciiZ name.   */
    word Ins2Num;     /* Number of instruments - for checking!    */
    byte InSpeed;     /* Initial speed in 7 lowermost bits        */
                      /* If highest bit (&128) set, no logaritmic */
                      /* adlib volume conversion needs to be done */
                      /* when playing with midi device.           */
    char InTempo;     /* Initial tempo                            */
    char ChanCount[EMPTYARRAY];
                      /* ChanCount[0] present if PatNum bit 9 set */
}InternalHdr; /* Size: 2+2+2+2+1+1 = 8+2 = 10 */

typedef struct
{
	signed char FT;     /*finetune value         1         */
						/*  signed seminotes.              */
	char D[11];         /*Identificators         11        */
						/* - 12nd byte not needed.         */
						/* six high bits of D[8]           */
						/* define the midi volume          */
						/* scaling level                   */
						/* - default is 63.                */
						/* D[9] bits 5..2 have             */
						/* the automatical SDx             */
						/* adjust value.                   */
						/* - default is 0.                 */
                        /* D[9] bits 6-7 are free.         */
	char Volume;        /*0..63                  1         */
						/* If bit 6 set,                   */
						/*  the instrument will be         */
						/*  played simultaneously          */
						/*  on two channels when           */
						/*  playing with a midi device.    */
						/*  To gain more volume.           */
						/* If bit 7 set,                   */
						/*  the finetune value             */
						/*  affects also FM.               */
						/*  (SMP->AME conversion)          */
	word C2Spd;         /*8363 = normal..?       2         */
	byte GM;            /*GM program number      1         */
    byte Bank;          /*Bank number, 0=normal. 1         */
                        /*Highest bit (&128) is free.      */
    char insname[EMPTYARRAY];
	                    /*Only if PatNum&512 set           */
}InternalSample;        /*             total:17 = 0x11     */

typedef struct Pattern
{
	word Len;
    const char far *Ptr;
}Pattern;

#define volshift 16 /* Internal, don't change */

#ifdef __GNUC__
#include <unistd.h>
#define delay(i) usleep((i)*1000)
#else
/* Huom: while() -lauseessa alla oleva hlt voi aiheuttaa ongelmia Windows -
   ymp„rist”ss„, koska se voi tulkita processor idlen niin ett„ vied„„np„s
   ohjelmalta prosessoritehokakusta kaikki vaan pois. */
/* Dosemun kanssa hlt taas aiheuttaa isoja virheilmoituksia. */
#define initdelay(i) do DelayCount=(long)Rate*(long)(i)/-1000;while(0)
/* T„st„ syyst„ poistin mokoman hlt:in. 3.1.1999/Bisqwit */
#define delay(i) do{initdelay(i);while(DelayCount<0);}while(0)
#endif

extern char SoundError[128];
/* Human-readable explanation of possible error. *
 * Read it only when an error has happened.      */

/* For IRQ subsystem */

extern long DelayCount;
    /* Increased by one, Rate times per second. *
     * See delay() for usage example.           *
     * IMPORTANT:                               *
     *  Modified only when timer is active.     *
     *  Timer is active only between            *
     *  StartS3M() and StopS3M().               */
extern int IRQActive; /* 0=not active, 1=active, 2=midins -u */
extern unsigned int Rate;
    /* Default value: 256.              *        
     * If you modify this, you must     *
     * do it before calling StartS3M(). */
#if SUPPORT_MIDI
extern int ReverbLevel;
extern int ChorusLevel;
    /* Same for these two. Ranges are 0..127, *
     * initial values are R25 and C50.        */
#endif

/* MUSIC UNIT GLOBAL DATA */

struct MusPlayerPos
{
    int Ord, Pat, Row;
    const char far *Pos;
    int Wait;
};

struct MusData
{
    int PlayNext;
        /* Set to 1 every time next line is played.
           Use it to syncronize screen with music. */

    long RowTimer;
        /* You can delay the music with this.
           Or fasten. Units: Rate */

    const char far *Playing; /* Currently playing music from this buffer. */
    int Paused;    /* If paused, interrupts normally but does not play. *
                    * Read only. There's PauseS3M() for this.           */
    const InternalHdr far *Hdr;                
    const InternalSample far *Instr[MAXINST+1];

    int LinearMidiVol;
    int GlobalVolume;

    int Speed;
    int Tempo;

    /* Data from song */
    const char far *Orders;
    Pattern Patn[MAXPATN];

    /* Player: For SBx */
    struct MusPlayerPos posi, saved;
    int LoopCount;
    int PendingSB0;
    
    long CurVol[MAXCHN];
    byte NNum[MAXCHN];
    byte CurPuh[MAXCHN];
    int NoteCut[MAXCHN];
    int NoteDelay[MAXCHN];
    byte CurInst[MAXCHN];
    byte InstNum[MAXCHN]; /* For the instrument number optimizer */
    byte PatternDelay;

    char FineVolSlide[MAXCHN];
    char FineFrqSlide[MAXCHN];
    signed char  VolSlide[MAXCHN];
    signed char FreqSlide[MAXCHN];
    #if VIBRATO
     byte VibDepth[MAXCHN];
     byte VibSpd  [MAXCHN];
     byte VibPos  [MAXCHN];
    #endif
    byte ArpegSpd;
    byte Arpeggio[MAXCHN];
    byte ArpegCnt[MAXCHN];
    #if RETRIG
     byte Retrig[MAXCHN];
    #endif

    char Active[MAXCHN];
    char Doubles[MAXCHN];

    #define FxxDefault ExxDefault /* They really are combined in ST3 */
    byte DxxDefault[MAXCHN];
    byte ExxDefault[MAXCHN];
    byte HxxDefault[MAXCHN];
	byte MxxDefault[MAXCHN];
	byte SxxDefault[MAXCHN];

    unsigned long Herz[MAXCHN];  /* K„ytt„„ m_opl */
};

extern struct MusData MusData;

/* The status buffer for SaveMusStatus and RestoreMusStatus should
   be malloced instead of wasting stack space for it. Reason: Buffer
   would probably cause a stack overflow. */

/* SaveMusStatus(Buf) saves the music status for later restoration. */
void SaveMusStatus(struct MusData *Buf);

/* RestoreMusStatus(Buf) restores the saved music status from Buf. */
void RestoreMusStatus(struct MusData *Buf);

struct sbenv    /* Data from BLASTER environment variable */
{               /* Has been set? */
    int set;
    word A,D,I,H,P,E,T;
};
extern struct sbenv sbenv;

/* m_arch.c */
/* PlayArchive():
     index==0:
        Soittaa Defaultin
     index>0:
        Soittaa biisin numero <index>
     index<0
        Kertoo biisien m„„r„n tiedostossa.
*/
extern int PlayArchive(int index, const char *FileName, const char *Default);

/* m_path.c */
extern char *Pathi(char *dest, int maxlen, const char *env, const char *a);

/* m_seek.c */
extern void MusSeekOrd(int Num);
extern void MusSeekNextOrd(void);
extern void MusSeekPrevOrd(void);

#ifdef __cplusplus
};
#endif

#endif
