#ifndef bqMusicStruct_h
#define bqMusicStruct_h

#ifdef __GNUC__
 #define EMPTYARRAY 0
 #define pk __attribute__((packed))
#else
 #define EMPTYARRAY
 #define pk
#endif

/*
        IMPORTANT NOTICE:
                Word alignment is our enemy
*/

/* Internal constants - should not be modified much */
#define MAXINST 100 /* ~70 practically (ST3 bugs)                           */
#define MAXPATN 256 /* 100 practically, 256 technically. Bxx makes limit.   */
#define MAXORD  256 /* Unused in music.c                                    */
#define MAXROW  100 /* 64 practically, 100 technically :) (Cxx makes limit) */
#define MAXCHN  15  /* For MIDI convenience                                 */

#define CLKMUL 0x100000 /* This too. We're using 32-bit signed fixed point. */

#if 1
#define PERIODCONST 9437400   /* Period[11]*22000/4, Bisqwit's const */
#else
#define PERIODCONST 14317056  /* Period[11]*8363, as in ST3 document */
/* This doesn't produce correct sound, so we use Bisqwit's version */
#endif

typedef struct
{
    char Name[28] pk;
    BYTE CtrlZ pk;
    BYTE FileType pk;  /* 16 = S3M */
    WORD Filler1 pk;
    WORD OrdNum pk;    /* Number of orders, is even                         */
    WORD InsNum pk;    /* Number of instruments                             */
    WORD PatNum pk;    /* Number of patterns                                */
    WORD Flags pk;     /* Don't care, ei mit„„n t„rke„„                     */
    WORD Version pk;   /* ST3.20 = 0x1320, ST3.01 = 0x1301                  */
    WORD SmpForm pk;   /* 2 = Unsigned samples, don't care, ei adlibin asia */
    char Ident[4] pk;  /*"SCRM"                                             */
    BYTE GVol pk;      /* Global volume, don't care, ei vaikuta adlibbeihin */
    BYTE InSpeed pk;   /* Initial speed                                     */
                       /*   &128 = linearmidivol                            */
    BYTE InTempo pk;   /* Initial tempo                                     */
    BYTE MVol pk;      /* Master volume, don't care, ei vaikuta adlibbeihin */
    BYTE UC pk;        /* Ultraclick removal, don't care, gus-homma         */
    BYTE DP pk;        /* Default channel pan, don't care, gussiin sekin    */
    BYTE Filler2[10] pk;
    BYTE Channels[32] pk; /* Channel settings, 16..31 = adl channels */
} S3MHdr;

typedef struct
{
    BYTE Typi pk;           /*2 = AMel           1           */
    char DosName[13] pk;    /*AsciiZ file name   13          */
                            /* Temporary used by intgen:     */
                            /*  [0]=GM (unsigned)            */
                            /*  [1]=ft (signed)              */
                            /*  [2]=doublechannel flag       */
    SWORD ManualScale pk;   /* This is actually part of the  */
                            /* filename, but this is now for */
                            /* intgen temporary.             */
    BYTE D[12] pk;          /*Identificators     12          */
    BYTE Volume pk;         /*                   1           */
    BYTE Filler1[3] pk;     /*                   3           */
    WORD C4Spd pk;          /*8363 = normal..?   2           */
    BYTE Filler2[14] pk;    /* May contain:      14          */
                            /*  [2]=GM (unsigned)            */
                            /*  [3]=ft (signed)              */
    char Name[28] pk;       /*AsciiZ sample name 28          */
    char Ident[4] pk;       /*"SCRI"             4           */
} ADLSample;                /*                   80 = 0x50   */

typedef struct
{
    WORD OrdNum pk;      /* Number of orders, is even                */
    WORD InsNum pk;      /* Number of instruments                    */
    WORD PatNum pk;      /* Number of patterns                       */
                         /* If bit 9 set, all instruments have       */
                         /* the insname[] filled with AsciiZ name.   */
                         /* If bit 10 is set, there exist no         */
                         /* notes between 126..253 and the 7th bit   */
                         /* of note value can be used to tell if     */
                         /* there is instrument number.              */
    WORD Ins2Num pk;     /* Number of instruments - for checking!    */
    BYTE InSpeed pk;     /* Initial speed in 7 lowermost bits        */
                         /* If highest bit (&128) set, no logaritmic */
                         /* adlib volume conversion needs to be done */
                         /* when playing with midi device.           */
    BYTE InTempo pk;     /* Initial tempo                            */
    BYTE ChanCount[EMPTYARRAY] pk;
                      /* ChanCount[0] present if PatNum bit 9 set */
} InternalHdr; /* Size: 2+2+2+2+1+1 = 8+2 = 10 */

typedef struct
{
    SBYTE FT pk;           /*finetune value         1         */
                           /*  signed seminotes.              */
    BYTE D[11] pk;         /*Identificators         11        */
                           /* - 12nd BYTE not needed.         */
                           /* six high bits of D[8]           */
                           /* define the midi volume          */
                           /* scaling level                   */
                           /* - default is 63.                */
                           /* D[9] bits 5..2 have             */
                           /* the automatical SDx             */
                           /* adjust value.                   */
                           /* - default is 0.                 */
                           /* D[9] bits 6-7 are free.         */
    BYTE Volume pk;        /*0..63                  1         */
                           /* Bits 6-7 are free.              */
    WORD C4Spd pk;         /*8363 = normal..?       2         */
    BYTE GM pk;            /*GM program number      1         */
    BYTE Bank pk;          /*Bank number, 0=normal. 1         */
                           /* If bit 7 set,                   */
                           /*  the finetune value             */
                           /*  affects also FM.               */
                           /*  (SMP->AME conversion)          */
                           /* Bit 6 free                      */
    char insname[EMPTYARRAY] pk;
                           /*Only if PatNum&512 set           */
} InternalSample;          /*             total:17 = 0x11     */

typedef struct Pattern
{
    WORD Len;
    const BYTE *Ptr;
} Pattern;

struct MusDriver
{
    const char *name;
    const char *longdescr;
    
    /* Key used for finding. Also useful for command line option char. */
    int optchar;
    
    int (*detect)(void); /* Returns 0 for ok, -1 for not */
    void (*reset)(void);
    void (*close)(void);
    
    void (*midibyte)(BYTE data);             /* Don't call this directly. */
    void (*oplbyte)(BYTE index, BYTE data);  /* This neither.             */
    void (*midictrl)(int ch, BYTE ctrl, BYTE value);
    /* Controlling (0,255,255) disables the     *
     * following functions for any midi device! */
    
    void (*setpan)(int ch, SBYTE value);
    
    /* Bend limits: 0x0000 - 0x3FFF */
    void (*bend)(int ch, WORD val);
    
    void (*noteoff)(int ch);
    void (*patch)(int ch, const BYTE *D, BYTE GM, BYTE bank);
    void (*noteon)(int ch, BYTE note, int hz, BYTE vol, BYTE adlvol);
    void (*touch)(int ch, BYTE vol, BYTE adlvol);
    void (*hztouch)(int ch, int hz);

    /* tick() is used by some drivers to get a good timing.
     * Each tick is 1/Rate seconds long.
     */
    void (*tick)(unsigned numticks);
};

/* MUSIC UNIT GLOBAL DATA */
struct MusPlayerPos
{
    unsigned Ord, Pat, Row;
    const BYTE *Pos;
    unsigned Wait;
};

struct MusData
{
    const BYTE *Playing; /* Currently playing music from the buffer
                          * pointed by this pointer.
                          * posi contains the position that is being
                          * played inside this buffer.
                          */
                          
    struct MusDriver *driver;
    
    struct MusPlayerPos posi, saved;
    BYTE LoopCount;
    char PendingSB0; /* boolean */
    
    /* Paused flag.
     * Pausing doesn't stop interrupts. */
    char Paused;
    
    /* This variable is Rate times per second increased by 1        *
     * You can poll and modify it for your pleasure.                */
    
    unsigned NextTick;
    
    /* This flag is increased by 1 every time a frame is played.    *
     * You can poll and modify it to synchronize screen with music. */
    
    unsigned NextFrame;
    
    /* This variable is increased by 1 every time a row is played.  *
     * You can poll and modify it to synchronize screen with music. */
    
    unsigned NextRow;

    const InternalHdr *Hdr;                 /* Points inside Playing. */
    const InternalSample *Instr[MAXINST+1]; /* These too.             */
    const BYTE *Orders;                     /* As well.               */
    Pattern Patn[MAXPATN];                  /* Yep, you guessed.      */
    /* FIXME: Patn[] should be dynamically allocated */
    
    char HaveChanInfo;       /* boolean */
    char InstruOptim;        /* boolean */
    char LinearMidiVol;      /* boolean */
    BYTE GlobalVolume;       /* 0..255 */
    
    BYTE CurVol[MAXCHN];
    BYTE NNum[MAXCHN];
    BYTE CurPuh[MAXCHN];
    BYTE NoteCut[MAXCHN];
    BYTE NoteDelay[MAXCHN];
    BYTE CurInst[MAXCHN];
    BYTE PatternDelay;

    BYTE DxxDefault[MAXCHN];
    BYTE ExxDefault[MAXCHN]; /* FxxDefault = ExxDefault */
    BYTE HxxDefault[MAXCHN];
    BYTE IxxDefault[MAXCHN];
    BYTE JxxDefault[MAXCHN];
    BYTE GxxDefault[MAXCHN];
    BYTE MxxDefault[MAXCHN];
    BYTE RxxDefault[MAXCHN];
    BYTE SxxDefault[MAXCHN];

    unsigned ST3Period[MAXCHN];
    unsigned NextPeriod[MAXCHN]; /* For slides */

    SBYTE VolSlide[MAXCHN];    /* Range: -15..15 */
    char FineVolSlide[MAXCHN]; /* 0: Normal, 1: Fine */
    
    SBYTE PerSlide[MAXCHN];    /* Range: -15..15 */
    char FinePerSlide[MAXCHN]; /* 0: Normal, 1: Fine, 2: Extra fine */
    
    BYTE Portamento[MAXCHN];
    BYTE Retrig[MAXCHN];

    BYTE ArpegCnt[MAXCHN];  /* This counter runs 0,1,2,0,1,2,..., */
    BYTE ArpegSpd;          /* changing each 1/ArpegSpd second.   */
    BYTE Arpeggio[MAXCHN];  /* Note displacemenents for arpeggio. */
    SDWORD ArpegClk[MAXCHN];/* Don't touch */

    BYTE VibDepth[MAXCHN]; /* Hxy x */
    BYTE VibSpd  [MAXCHN]; /* Hxy y */
    SDWORD VibClk[MAXCHN]; /* Don't touch */
    BYTE VibPos  [MAXCHN]; /* This runs 0..255, don't touch          */
    char FineVibra[MAXCHN];/* 0: Normal, 1: Fine                     */
    char VibraForm[MAXCHN];/* 0:Sine, 1:Rampdown, 2:Square, 3:Random */
    
    BYTE Tremor[MAXCHN];   /* This is Ixy */

    BYTE TremoDepth[MAXCHN]; /* Rxy x */
    BYTE TremoSpd  [MAXCHN]; /* Rxy y */
    SDWORD TremoClk[MAXCHN]; /* Don't touch */
    BYTE TremoPos  [MAXCHN]; /* This runs 0..255, don't touch     */
    char TremoForm[MAXCHN];  /* 0:Sine, 1:Rampdown, 2:Square, 3:Random */

    BYTE FrameCount;     /* This is Axx */
    BYTE Tempo;          /* This is Txx */
    BYTE Frame;          /* For A06, this gets values 0..5       */
    DWORD FrameSpd;      /* Don't touch */
    SDWORD FrameClk;     /* Don't touch */
};

#undef pk

#endif
