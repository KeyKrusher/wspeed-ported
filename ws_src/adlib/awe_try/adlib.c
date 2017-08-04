#include <dos.h>
#include <stdlib.h>

#include <math.h>

#include "adlib.h"

#define VIBRATO

/* IMPORTANT NOTE: You can load the data only from memory.        */
/*                 To create the data buffer, use INTGEN program. */

/* 11.8.1998 16:37 Bisqwit announces:

	 Supported commandinfobytes currently include:
		Axx (set speed (ticks))
		Bxx
		Cxx
		Dx0 (volume slide up   ticks*x)
		D0y (volume slide down ticks*y)
		DxF (slide volume up   by y)
		DFy (slide volume down by y)
		DFF (maximizes volume)	  (actually not yet :) I'm lazy, that is)
		Exx                       (incorrect yet, difficult to emulate)

		Hxx (if VIBRATO #defined) (incorrect yet, difficult to emulate)
		Kxx (if VIBRATO #defined) (incorrect yet, difficult to emulate)

		Fxx                       (incorrect yet, difficult to emulate)
		SBx (patternloop, x>0?count:start)
		SCx (notecut in   x ticks)
		SDx (notedelay    x ticks)
		SEx (patterndelay x lines)
		Txx (set tempo)

	 By T7D(tempo) A06(ticks), we play 8 notes in second.
	   Rows played in second equals tempo * 93.75 / ticks.
	   Actually it SHOULD be as follows:
	   Rows played in minute equals tempo * 24 / ticks.
	   Conclusion: This unit plays with minorly wrong speed,
				   but since it hasn't yet been detected,
				   it is acceptable :)

	 Exx and Fxx:

	   For Exx, FreqSlide =  xx
	   For Fxx, FreqSlide = -xx

	   In each frame (there are Axx frames in a row)
		the Period of note will be increased by FreqSlide.
	   The output Herz is Period * C2SPD of instrument / 22000.

	 Hxy and Kxy:
	   For Kxy: first do Dxy and then set xy to be the last Hxy xy.

	   Vibrato amplitude: y/16       seminotes
	   Vibrato speed:     x*ticks/64 cycles in second
	   Ticks is the value set by Axx -command.
*/

Pattern Patn[MAXPATN];

/* These variables tell which cards have been detected */
int IsMPUThere=0;
int IsOPLThere=0;
int IsAWEThere=0;
word OPLBase=0x388;
word MPUBase=0x330;
word AWEBase=0x640;

/* Constants for IRQ subsystem */
unsigned int Rate = 1000;
char IRQActive = 0;

/* Variables for IRQ subsystem */
typedef void interrupt (*IRQType)(...);
signed long DelayCount;
char PlayNext = 0;
IRQType OldI08;
long RowTimer;
word Counter;
word Clock;
int Paused; // If paused, interrupts normally but does not play

char *Playing; /* The filename/buffer currently playing */

/* Modified by player */
char CurOrd;
char CurPat;
char CurRow;
char *PatPos;
char *ToSavePos;
int ToSaveOrd;
int ToSaveRow;
int ToSavePat;
byte PatternDelay;
long CurVol[MAXCHN];
char CurInst[MAXCHN];
byte CurPuh[MAXCHN];
int NoteDelay[MAXCHN];
int NoteCut  [MAXCHN];
unsigned long Herz[MAXCHN];
byte NNum[MAXCHN];

/* For effects */
signed char  VolSlide[MAXCHN];
signed char FreqSlide[MAXCHN];
char FineVolSlide[MAXCHN];
#ifdef VIBRATO
byte VibDepth[MAXCHN];
byte VibSpd  [MAXCHN];
byte VibPos  [MAXCHN];
#endif

/* Player: For SBx */
int Restored;
int LoopCount;
char *SavedPtr;
int SavedRow, SavedPat, SavedOrd;

/* Needed by player */
char Orders[MAXORD];
word InstPtr[MAXINST+1];
word PatnPtr[MAXPATN];

InternalHdr *Hdr;
InternalSample *Instr[MAXINST+1];

#ifdef VIBRATO
byte VibratoTable[32] =
{   0,24,49,74,97,120,141,161,
	180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,
	180,161,141,120,97,74,49,24
};

byte avibtab[128] =
{   0,1,3,4,6,7,9,10,12,14,15,17,18,20,21,23,
	24,25,27,28,30,31,32,34,35,36,38,39,40,41,42,44,
	45,46,47,48,49,50,51,52,53,54,54,55,56,57,57,58,
	59,59,60,60,61,61,62,62,62,63,63,63,63,63,63,63,
	64,63,63,63,63,63,63,63,62,62,62,61,61,60,60,59,
	59,58,57,57,56,55,54,54,53,52,51,50,49,48,47,46,
	45,44,42,41,40,39,38,36,35,34,32,31,30,28,27,25,
	24,23,21,20,18,17,15,14,12,10,9,7,6,4,3,1
};
#endif

/****                                   ****/
/****                                   ****/
/**** These routines belong only to MPU ****/
/****                                   ****/
/****                                   ****/

void GM_Byte(byte a)
{
//	((byte far *)0xA0000000UL)[*((byte far *)0x46CUL)] = 100;
//	Just debugging

	if(!MPUFound)return;

	_asm mov dx, MPUBase
	_asm inc dx
Busy:
	_asm in al, dx
	_asm test al, 0x40
	_asm jnz Busy

	_asm mov al, a
	_asm dec dx
	_asm out dx, al
}

/* GENERAL MIDI (GM) COMMANDS:
8x       1000xxxx     nn vv         Note off (key is released)
									nn=note number
									vv=velocity

9x       1001xxxx     nn vv         Note on (key is pressed)
									nn=note number
									vv=velocity

Ax       1010xxxx     nn vv         Key after-touch
									nn=note number
									vv=velocity

Bx       1011xxxx     cc vv         Control Change
									cc=controller number
									vv=new value

Cx       1100xxxx     pp            Program (patch) change
									pp=new program number

Dx       1101xxxx     cc            Channel after-touch
									cc=channel number

Ex       1110xxxx     bb tt         Pitch wheel change (2000H is normal
														or no change)
									bb=bottom (least sig) 7 bits of value
									tt=top (most sig) 7 bits of value
*/

byte GM_Percu[MAXCHN]={0,0,0,0,0,0,0,0,0};
byte GM_Notes[32]={0,0,0,0,0,0,0,0,0};
byte GM_Pats[32];
byte GM_Vol[32];

void GM_Patch(int c, byte p)
{
//	if(c==3)p=128+70;

	if(p<129) // Simple case, melodic instrument.
	{
		GM_Percu[c] = 0;
		GM_Pats[c] = p;
		GM_Byte(0xC0+c);
		GM_Byte(p-1);
	}
	else	  // Not so simple case; percussion.
	{
		GM_Notes[31] = p-128;
		GM_Percu[c] = 1;
		// Yep, this is definitely unsimpler
		// case than the above one. :)
	}
}
byte GM_Volume(byte Vol) // Converts the volume
{
	byte vol[65] =
	{
  /*00*/0x00,0x01,0x01,0x01, 0x01,0x01,0x01,0x01,
  /*08*/0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01,
  /*16*/0x02,0x03,0x04,0x05, 0x06,0x07,0x08,0x09,
  /*24*/0x0A,0x0B,0x0D,0x0F, 0x11,0x13,0x15,0x17,
  /*32*/0x20,0x22,0x24,0x26, 0x28,0x2A,0x2C,0x2E,
  /*40*/0x30,0x32,0x34,0x36, 0x38,0x3A,0x3C,0x3E,
  /*48*/0x40,0x43,0x46,0x4A, 0x4E,0x52,0x56,0x5E,
  /*56*/0x66,0x6C,0x70,0x73, 0x76,0x79,0x7B,0x7E,
  /*64*/0x7F
	};
	if(Vol>64)Vol=64; /* Prevent overflows */
	return vol[Vol];
}
void GM_KeyOn(int c, byte key, byte Vol)
{
//	if(c>1)return;
	key += 12;
	//Korkeus 12, niin kuulostaa fmdrv:ll„ konvertoidut
	//t„sm„lleen samoilta mpu:lla kuin opl:lla korkeuden
	//suhteen.

	if(GM_Percu[c]) // Prepare for worst; percussion on multiple channels
	{
		GM_Byte(0x99);
		GM_Byte(GM_Notes[31]);    // Skip key
		GM_Byte(GM_Vol[31]=GM_Volume(Vol));
	}
	else
	{
		if(GM_Pats[c]==79)key+=36; //Whistle

		GM_Byte(0x90+c);
		GM_Byte(GM_Notes[c]=key);
		GM_Byte(GM_Vol[c]=GM_Volume(Vol));
	}
}

void GM_NoteOff(int c)
{
	if(GM_Percu[c])
	{
		GM_Byte(0x89);
		GM_Byte(GM_Notes[31]);
		GM_Byte(GM_Vol[31]);
		GM_Notes[31]=0;
	}
	else
	{
		GM_Byte(0x80+c);
		GM_Byte(GM_Notes[c]);
		GM_Byte(GM_Vol[c]);
		GM_Notes[c]=0;
	}
}
void GM_Touch(int c, byte Vol)
{
	if(GM_Percu[c])
	{
		if(!GM_Notes[31])return;
		GM_Byte(0xA9);
		GM_Byte(GM_Notes[31]);
		GM_Byte(GM_Vol[31]=GM_Volume(Vol));
	}
	else
	{
		if(!GM_Notes[c])return;
		GM_Byte(0xA0+c);
		GM_Byte(GM_Notes[c]);
		GM_Byte(GM_Vol[c]=GM_Volume(Vol));
	}
}

void GM_Bend(int c, word Count)
{
	// Toivottavasti joku ei yrit„ bendata hihattia tms :-)
	if(!GM_Notes[c])return;

	GM_Byte(0xE0+c);

	GM_Byte((Count     ) & 127);
	GM_Byte((Count >> 7) & 127);

	/* *** Don't try to run this code :-)
		if(Count >= 0x4000)
		{
			fprintf(stderr,
				"Fatal exception [log(14i)] at %04X:%04X\n"
				"Divided buffer overflow, system halted\n", _CS,
				(unsigned short)(&GM_Bend));
			for(Count=1; Count; *((char *)Count++)=Count<<14);
			assert(Count);
		}
	*/
}

int MPUFound(void)
{
	IsMPUThere = 1;
	IsMPUThere = MPUReset();

	return IsMPUThere;
}

int MPUReset(void)
{
	int a;

	if(!IsMPUThere)return 0;

	_asm mov dx, MPUBase
	_asm inc dx
Busy:
	_asm in al, dx
	_asm test al, 0x40
	_asm jnz Busy

	_asm mov al, 0xFF
	_asm out dx, al

	_asm xor cx, cx
Empty:
	_asm in al, dx
	_asm test al, 0x80
	_asm jnz NextLoop

	_asm dec dx
	_asm in al, dx
	_asm cmp al, 0xFE
	_asm je DetectOk
	_asm inc dx

NextLoop:
	_asm loop Empty
	return 0;

DetectOk:
	// Enter UART mode by the way...
	_asm mov dx, MPUBase
	_asm inc dx

Busy2:
	_asm in al, dx
	_asm test al, 0x40
	_asm jnz Busy2

	_asm mov al, 0x3F
	_asm out dx, al

	_asm xor cx, cx
Empty2:
	_asm in al, dx
	_asm test al, 0x80
	_asm jnz Next2

	_asm dec dx
	_asm in al, dx
	_asm cmp al, 0xFE
	_asm je ResetOk
	_asm inc dx

Next2:
	_asm loop Empty2
	// Error: UART mode was not entered.
	return 0;

ResetOk:
	for(a=0; a<32; a++)GM_Notes[a]=0;
	for(a=0; a<MAXCHN; a++)GM_Percu[a]=0;

	return 1;
}

/****                                   ****/
/****                                   ****/
/**** End of MPU-only section           ****/
/****                                   ****/
/****                                   ****/

/****                                   ****/
/****                                   ****/
/**** These routines belong only to OPL ****/
/****                                   ****/
/****                                   ****/

long Period[12] = {907,960,1016,1076,1140,1208,1280,1356,1440,1524,1616,1712};
char Portit[9] = {0,1,2, 8,9,10, 16,17,18};
char AdlStatus[244];

#define AdlNoteOff(Channel) WriteR(0xB0+(Channel), 0)

void WriteR(char Index, char Data)
{
	if(!IsOPLThere)return;

	AdlStatus[Index] = Data;

	asm {
		MOV		AL,Index
		MOV		DX,OPLBase
		OUT		DX,AL
		MOV		CX,6
	}
P1:	asm {
		IN		AL,DX
		LOOP	P1
		inc dx
		MOV		AL,Data
		OUT		DX,AL
		MOV		CX,35
	}
P2:	asm {
		IN		 AL,DX
		LOOP	 P2
}	}

void UpdHerz(int CurChn, unsigned long Herz)
{
	int Oct;

	for(Oct=0; Herz>0x1FF; Oct++)Herz >>= 1;

	WriteR(0xA0+CurChn, Herz&255); //F-Number lsb 8 bits

	WriteR(0xB0+CurChn, (0x20       ) //Key on
					  | ((Herz>>8)&3) //F-number, hsb 2 bits
					  | ((Oct&7)<<2)
		  );
}

void SetVolume(int Ope, int Instru, word Vol)
{
	WriteR(0x40+Ope,
		(Instr[Instru]->D[2]&0xC0) |
		(63-((63-(Instr[Instru]->D[2]&63)) * Vol / 64))
	);

	WriteR(0x43+Ope,
		(Instr[Instru]->D[3]&0xC0) |
		(63-((63-(Instr[Instru]->D[3]&63)) * Vol / 64))
	);
}

int OPLReset(void)
{
	int a;

	if(!IsOPLThere)return 0;

	for(a=0; a<244; WriteR(a++, 0));

	return 1;
}

int OPLFound(void)
{
	register char ST1, ST2;

	IsOPLThere = 1;

	WriteR(4,0x60); WriteR(4,0x80);
	_asm{mov dx,OPLBase;in al,dx;mov ST1,al}
	WriteR(2,0xFF); WriteR(4,0x21);

	_asm xor cx,cx;P1:_asm loop P1
	_asm{mov dx,OPLBase;in al,dx;mov ST2,al}
	WriteR(4,0x60); WriteR(4,0x80);
	return (IsOPLThere = (((ST1&0xE0)==0)&&((ST2&0xE0)==0xC0)?1:0));
}

/****                                   ****/
/****                                   ****/
/**** End of OPL-only section           ****/
/****                                   ****/
/****                                   ****/

/****                                   ****/
/****                                   ****/
/**** These routines belong only to AWE ****/
/****                                   ****/
/****                                   ****/

#include "awedata.h"

int _emu8k_numchannels=32;

static struct midi_preset_t {   /* struct to hold envelope data for each preset */
 int num_splits;                /* number of splits in this preset */
 struct envparms_t **split;     /* array of num_splits pointers to envelope data */
} *midi_preset;                 /* global variable to hold the data */

static struct envparms_t **voice_envelope;      /* array of pointers pointing at the envelope playing on each voice */
static int *exclusive_class_info;               /* exclusive class information */


typedef struct envparms_t {
int envvol,  envval,  dcysus,  atkhldv,
	 lfo1val, atkhld,  lfo2val, ip,
	 ifatn,   pefe,    fmmod,   tremfrq,
	 fm2frq2, dcysusv, ptrx,    psst,
	 csl,     ccca,    rootkey, ipbase,
	 ipscale, minkey,  maxkey,  minvel,
	 maxvel,  key,     vel,     exc,
	 keyMEH,  keyMED,  keyVEH,  keyVED,
	 filter,  atten,   smpmode, volrel,
	 modrel,  pan,     loopst,  reverb,
	 chorus,  volsust, modsust, pitch,
	 sampend;
} envparms_t;

enum {
	sfgen_startAddrsOffset=0,		sfgen_endAddrsOffset,
	sfgen_startloopAddrsOffset,		sfgen_endloopAddrsOffset,
	sfgen_startAddrsCoarseOffset,	sfgen_modLfoToPitch,
	sfgen_vibLfoToPitch,			sfgen_modEnvToPitch,
	sfgen_initialFilterFc,			sfgen_initialFilterQ,
	sfgen_modLfoToFilterFc,			sfgen_modEnvToFilterFc,
	sfgen_endAddrsCoarseOffset, 	sfgen_modLfoToVolume,
	sfgen_chorusEffectsSend=15,		sfgen_reverbEffectsSend,
	sfgen_pan,						sfgen_delayModLFO=21,
	sfgen_freqModLFO,	sfgen_delayVibLFO,	sfgen_freqVibLFO,
	sfgen_delayModEnv,	sfgen_attackModEnv,	sfgen_holdModEnv,
	sfgen_decayModEnv,	sfgen_sustainModEnv,sfgen_releaseModEnv,
	sfgen_keynumToModEnvHold,	sfgen_keynumToModEnvDecay,
	sfgen_delayVolEnv,	sfgen_attackVolEnv,	sfgen_holdVolEnv,
	sfgen_decayVolEnv,	sfgen_sustainVolEnv,sfgen_releaseVolEnv,
	sfgen_keynumToVolEnvHold,	sfgen_keynumToVolEnvDecay,
	sfgen_instrument,	sfgen_keyRange=43,	sfgen_velRange,
	sfgen_startloopAddrsCoarseOffset,		sfgen_keynum,
	sfgen_velocity,		sfgen_initialAttenuation,
	sfgen_endloopAddrsCoarseOffset=50,		sfgen_coarseTune,
	sfgen_fineTune,		sfgen_sampleID,		sfgen_sampleModes,
	sfgen_initialPitch,	sfgen_scaleTuning,	sfgen_exclusiveClass,
	sfgen_overridingRootKey,	gfgen_startAddrs,
	gfgen_startloopAddrs,		gfgen_endloopAddrs,
	gfgen_endAddrs,
	SOUNDFONT_NUM_GENERATORS=63
};

typedef long generators_t[SOUNDFONT_NUM_GENERATORS];

static void write_word(int reg,int channel,int port,word data)
{
	int out_port=AWEBase;
	if(port==1)out_port+=0x400;
	else if(port==2)out_port+=0x402;
	else if(port==3)out_port+=0x800;
	else if(port!=0)return;
	outport(AWEBase+0x802,(reg<<5)+(channel&0x1f));
	outport(out_port,data);
}

static word read_word(int reg,int channel,int port)
{
	int in_port=AWEBase;
	if(port==1)in_port+=0x400;
	else if(port==2)in_port+=0x402;
	else if(port==3)in_port+=0x800;
	else if(port!=0)return 0;
	outport(AWEBase+0x802,(reg<<5)+(channel&0x1f));
	return inport(in_port);
}

static void write_dword(int reg,int channel,int port,dword data)
{
	int out_port=AWEBase;
	if(port==1)out_port+=0x400;
	else if(port==2)out_port+=0x402;
	else if(port==3)out_port+=0x800;
	else if(port!=0)return;
	outport(AWEBase+0x802,(reg<<5)+(channel&0x1f));
	outport(out_port,(word)(data));
	outport(out_port+2,(word)(data>>16));
}

static dword read_dword(int reg,int channel,int port) {
	int in_port;
	word a,b;
	in_port=AWEBase;
	if(port==1)in_port+=0x400;
	else if(port==2)in_port+=0x402;
	else if(port==3)in_port+=0x800;
	else if(port!=0)return 0;
	outport(AWEBase+0x802,(reg<<5)+(channel&0x1f));
	a=inport(in_port);
	b=inport(in_port+2);
	return ((((dword)b)<<16)+a);
}

/* write_*:
 *  Functions to write information to the AWE32's registers
 *  (abbreviated as in the AEPG).
 */
static inline void write_CPF(int channel,dword i)     { write_dword(0,channel,0,i); }
static inline void write_PTRX(int channel,dword i)    { write_dword(1,channel,0,i); }
static inline void write_CVCF(int channel,dword i)    { write_dword(2,channel,0,i); }
static inline void write_VTFT(int channel,dword i)    { write_dword(3,channel,0,i); }
static inline void write_PSST(int channel,dword i)    { write_dword(6,channel,0,i); }
static inline void write_CSL(int channel,dword i)     { write_dword(7,channel,0,i); }
static inline void write_CCCA(int channel,dword i)    { write_dword(0,channel,1,i); }
static inline void write_HWCF4(dword i)               { write_dword(1,      9,1,i); }
static inline void write_HWCF5(dword i)               { write_dword(1,     10,1,i); }
static inline void write_HWCF6(dword i)               { write_dword(1,     13,1,i); }
static inline void write_SMALR(dword i)               { write_dword(1,     20,1,i); }
static inline void write_SMARR(dword i)               { write_dword(1,     21,1,i); }
static inline void write_SMALW(dword i)               { write_dword(1,     22,1,i); }
static inline void write_SMARW(dword i)               { write_dword(1,     23,1,i); }
static inline void write_SMLD(word i)                { write_word (1,     26,1,i); }
static inline void write_SMRD(word i)                { write_word (1,     26,2,i); }
static inline void write_WC(word i)                  { write_word (1,     27,2,i); }
static inline void write_HWCF1(word i)               { write_word (1,     29,1,i); }
static inline void write_HWCF2(word i)               { write_word (1,     30,1,i); }
static inline void write_HWCF3(word i)               { write_word (1,     31,1,i); }
static inline void write_INIT1(int channel,word i)   { write_word (2,channel,1,i); } /* `channel' is really `element' here */
static inline void write_INIT2(int channel,word i)   { write_word (2,channel,2,i); }
static inline void write_INIT3(int channel,word i)   { write_word (3,channel,1,i); }
static inline void write_INIT4(int channel,word i)   { write_word (3,channel,2,i); }
static inline void write_ENVVOL(int channel,word i)  { write_word (4,channel,1,i); }
static inline void write_DCYSUSV(int channel,word i) { write_word (5,channel,1,i); }
static inline void write_ENVVAL(int channel,word i)  { write_word (6,channel,1,i); }
static inline void write_DCYSUS(int channel,word i)  { write_word (7,channel,1,i); }
static inline void write_ATKHLDV(int channel,word i) { write_word (4,channel,2,i); }
static inline void write_LFO1VAL(int channel,word i) { write_word (5,channel,2,i); }
static inline void write_ATKHLD(int channel,word i)  { write_word (6,channel,2,i); }
static inline void write_LFO2VAL(int channel,word i) { write_word (7,channel,2,i); }
static inline void write_IP(int channel,word i)      { write_word (0,channel,3,i); }
static inline void write_IFATN(int channel,word i)   { write_word (1,channel,3,i); }
static inline void write_PEFE(int channel,word i)    { write_word (2,channel,3,i); }
static inline void write_FMMOD(int channel,word i)   { write_word (3,channel,3,i); }
static inline void write_TREMFRQ(int channel,word i) { write_word (4,channel,3,i); }
static inline void write_FM2FRQ2(int channel,word i) { write_word (5,channel,3,i); }

/* read_*:
 *  Functions to read information from the AWE32's registers
 *  (abbreviated as in the AEPG).
 */
static inline dword read_CPF(int channel)     { return read_dword(0,channel,0); }
static inline dword read_PTRX(int channel)    { return read_dword(1,channel,0); }
static inline dword read_CVCF(int channel)    { return read_dword(2,channel,0); }
static inline dword read_VTFT(int channel)    { return read_dword(3,channel,0); }
static inline dword read_PSST(int channel)    { return read_dword(6,channel,0); }
static inline dword read_CSL(int channel)     { return read_dword(7,channel,0); }
static inline dword read_CCCA(int channel)    { return read_dword(0,channel,1); }
static inline dword read_HWCF4()              { return read_dword(1,      9,1); }
static inline dword read_HWCF5()              { return read_dword(1,     10,1); }
static inline dword read_HWCF6()              { return read_dword(1,     13,1); }
static inline dword read_SMALR()              { return read_dword(1,     20,1); }
static inline dword read_SMARR()              { return read_dword(1,     21,1); }
static inline dword read_SMALW()              { return read_dword(1,     22,1); }
static inline dword read_SMARW()              { return read_dword(1,     23,1); }
static inline word read_SMLD()               { return read_word (1,     26,1); }
static inline word read_SMRD()               { return read_word (1,     26,2); }
static inline word read_WC()                 { return read_word (1,     27,2); }
static inline word read_HWCF1()              { return read_word (1,     29,1); }
static inline word read_HWCF2()              { return read_word (1,     30,1); }
static inline word read_HWCF3()              { return read_word (1,     31,1); }
static inline word read_INIT1()              { return read_word (2,      0,1); }
static inline word read_INIT2()              { return read_word (2,      0,2); }
static inline word read_INIT3()              { return read_word (3,      0,1); }
static inline word read_INIT4()              { return read_word (3,      0,2); }
static inline word read_ENVVOL(int channel)  { return read_word (4,channel,1); }
static inline word read_DCYSUSV(int channel) { return read_word (5,channel,1); }
static inline word read_ENVVAL(int channel)  { return read_word (6,channel,1); }
static inline word read_DCYSUS(int channel)  { return read_word (7,channel,1); }
static inline word read_ATKHLDV(int channel) { return read_word (4,channel,2); }
static inline word read_LFO1VAL(int channel) { return read_word (5,channel,2); }
static inline word read_ATKHLD(int channel)  { return read_word (6,channel,2); }
static inline word read_LFO2VAL(int channel) { return read_word (7,channel,2); }
static inline word read_IP(int channel)      { return read_word (0,channel,3); }
static inline word read_IFATN(int channel)   { return read_word (1,channel,3); }
static inline word read_PEFE(int channel)    { return read_word (2,channel,3); }
static inline word read_FMMOD(int channel)   { return read_word (3,channel,3); }
static inline word read_TREMFRQ(int channel) { return read_word (4,channel,3); }
static inline word read_FM2FRQ2(int channel) { return read_word (5,channel,3); }

/* write_init_arrays:
 *  Writes the given set of initialisation arrays to the card.
 */
static void write_init_arrays(word *init1,word *init2,word *init3,word *init4)
{
	int i;
	for(i=0;i<32;i++)write_INIT1(i,init1[i]);
	for(i=0;i<32;i++)write_INIT2(i,init2[i]);
	for(i=0;i<32;i++)write_INIT3(i,init3[i]);
	for(i=0;i<32;i++)write_INIT4(i,init4[i]);
}

/* emu8k_init:
 *  Initialise the synthesiser. See AEPG chapter 4.
 */
void emu8k_init(void)
{
	int channel;
	write_HWCF1(0x0059);
	write_HWCF2(0x0020);
	for(channel=0;channel<_emu8k_numchannels;channel++)write_DCYSUSV(channel, 128);
	for(channel=0;channel<_emu8k_numchannels;channel++)
	{
		write_ENVVOL(channel,0);
		write_ENVVAL(channel,0);
		write_DCYSUS(channel,0);
		write_ATKHLDV(channel,0);
		write_LFO1VAL(channel,0);
		write_ATKHLD(channel,0);
		write_LFO2VAL(channel,0);
		write_IP(channel,0);
		write_IFATN(channel,0);
		write_PEFE(channel,0);
		write_FMMOD(channel,0);
		write_TREMFRQ(channel,0);
		write_FM2FRQ2(channel,0);
		write_PTRX(channel,0);
		write_VTFT(channel,0);
		write_PSST(channel,0);
		write_CSL(channel,0);
		write_CCCA(channel,0);
	}
	for(channel=0;channel<_emu8k_numchannels;channel++)
	{
		write_CPF(channel,0);
		write_CVCF(channel,0);
	}
	write_SMALR(0);
	write_SMARR(0);
	write_SMALW(0);
	write_SMARW(0);
	write_init_arrays(init1_1,init1_2,init1_3,init1_4);

	_asm xor cx,cx;P1:_asm loop P1 //Wait at least 24 msec...

	write_init_arrays(init2_1,init2_2,init2_3,init2_4);
	write_init_arrays(init3_1,init3_2,init3_3,init3_4);
	write_HWCF4(0);
	write_HWCF5(0x00000083);
	write_HWCF6(0x00008000);
	write_init_arrays(init4_1,init4_2,init4_3,init4_4);
	write_HWCF3(0x0004);
}

/* emu8k_detect:
 *  Locate the EMU8000. This tries to extract the base port from the E section
 *  of the BLASTER environment variable, and then does some test reads to check
 *  that there is an EMU8000 there.
 */
int AWEFound(void)
{
	char *envvar;

	IsAWEThere = 0;

	if(!(envvar=getenv("BLASTER")))
	{
		// BLASTER environment variable not set
		return 0;
	}

	AWEBase=0;
	while(*envvar)
	{
		if(*envvar=='E')AWEBase=strtol(envvar+1,NULL,16);
		while((*envvar!=' ')&&(*envvar!=0))envvar++;
		if(*envvar)envvar++;
	}
	if(!AWEBase)
	{
		// BLASTER environment variable has no E section
		return 0;
	}

	if(((read_word(7, 0,3)&0x000f)==0x000c)
	 &&((read_word(1,29,1)&0x007e)==0x0058)
	 &&((read_word(1,30,1)&0x0003)==0x0003))
		return (IsAWEThere=1);

	// AWE32 detection failed.
	return 0;
}

int AWEReset(void)
{
	if(!IsOPLThere)return 0;

	emu8k_init();

	return 1;
}

/* emu_*:
 *  Functions to convert SoundFont information to EMU8000 parameters.
 */
static inline unsigned short emu_delay(int x) {
 int a=0x8000-pow(2,x/1200.0)/0.000725;
 return (a<0)?0:((a>0x8000)?0x8000:a);
}

static inline unsigned char emu_attack(int x) {
/*
 * AEPG doesn't specify exact conversion here; I'm using what Takashi Iwai used for
 * his Linux driver
 */
 int a,msec;
 msec=pow(2,x/1200.0)*1000;
 if (msec==0) return 0x7f;
// if (msec>=360)
  a=11878/msec;
// else
//  a=32+53.426*log10(360.0/msec);

 return (a<1)?1:((a>0x7f)?0x7f:a);
}

static inline unsigned char emu_hold(int x) {
 int a=0x7f-(int)(pow(2,x/1200.0)*0x7f/11.68);
 return (a<0)?0:((a>0x7f)?0x7f:a);
}

static inline unsigned char emu_decay(int x) {
/*
 * AEPG doesn't specify exact conversion here; I'm using an adaptation of
 * what Takashi Iwai used for his Linux driver
 */
 int a,msec;
 msec=pow(2,x/1200.0)*1000;
 if (msec==0) return 0x7f;
 a=0x7f-54.8*log10(msec/23.04);               /* Takashi's */
// a = 47349/(msec+349);                          /* mine */
 return (a<1)?1:((a>0x7f)?0x7f:a);
}

static inline unsigned char emu_sustain(int x) {
/* This is not the same as in Takashi Iwai's Linux driver, but I think I'm right */
 int a=0x7f-x/7.5;
 return (a<0)?0:((a>0x7f)?0x7f:a);
}

static inline unsigned char emu_release(int x) {
 return emu_decay(x);
}

static inline unsigned char emu_mod(int x,int peak_deviation) {
 int a=x*0x80/peak_deviation;
 return (a<-0x80)?0x80:((a>0x7f)?0x7f:a);
}

static inline unsigned char emu_freq(int x) {
 int a=(8.176*pow(2,x/1200.0)+0.032)/0.042;
 return (a<0)?0:((a>0xff)?0xff:a);
}

static inline unsigned char emu_reverb(int x) {
 int a=(x*0xff)/1000;
 return (a<0)?0:((a>0xff)?0xff:a);
}

static inline unsigned char emu_chorus(int x) {
 int a=(x*0xff)/1000;
 return (a<0)?0:((a>0xff)?0xff:a);
}

static inline unsigned char emu_pan(int x) {
 int a=0x7f-(x*0xff)/1000;
 return (a<8)?8:((a>0xf8)?0xf8:a);
}

static inline unsigned char emu_filterQ(int x) {
 int a=(x/15);
 return (a<0)?0:((a>15)?15:a);
}

static inline unsigned int emu_address(int base,int offset,int coarse_offset) {
 return base+offset+coarse_offset*32*1024;
}

static inline unsigned short emu_pitch(int key,int rootkey,int scale,int coarse,int fine,int initial) {
 int a=(((key-rootkey)*scale+coarse*100+fine-initial)*0x1000)/1200+0xE000;
 return (a<0)?0:((a>0xFFFF)?0xFFFF:a);
}

static inline unsigned char emu_filter(int x) {
 int a = (x-4721)/25;
 return (a<0)?0:((a>0xff)?0xff:a);
}

static inline unsigned char emu_atten(int x) {
 int a = x/3.75;
 return (a<0)?0:((a>0xff)?0xff:a);
}

/* emu8k_createenvelope:
 *  Converts a set of SoundFont generators to equivalent EMU8000 register
 *  settings, for passing to the sound playing functions above.
 */
envparms_t *emu8k_createenvelope(generators_t sfgen) {
 envparms_t *envelope=(envparms_t *)malloc(sizeof(envparms_t));
 if (envelope) {
  envelope->envvol  = emu_delay(sfgen[sfgen_delayModEnv]);
  envelope->envval  = emu_delay(sfgen[sfgen_delayVolEnv]);
  envelope->modsust = (emu_sustain(sfgen[sfgen_sustainModEnv])<<8);
  envelope->modrel  = (emu_release(sfgen[sfgen_releaseModEnv])<<0);
  envelope->dcysus  = (0<<15) // we're programming the decay, not the release
		    + envelope->modsust
		    + (0<<7)  // unused
		    + (emu_decay(sfgen[sfgen_decayModEnv])<<0);
  envelope->atkhldv = (0<<15) // 0 otherwise it won't attack
		    + (emu_hold(sfgen[sfgen_holdVolEnv])<<8)
		    + (0<<7)  // unused
		    + (emu_attack(sfgen[sfgen_attackVolEnv])<<0);
  envelope->lfo1val = emu_delay(sfgen[sfgen_delayModLFO]);
  envelope->atkhld  = (0<<15) // 0 otherwise it won't attack
		    + (emu_hold(sfgen[sfgen_holdModEnv])<<8)
		    + (0<<7)  // unused
		    + (emu_attack(sfgen[sfgen_attackModEnv])<<0);
  envelope->lfo2val = emu_delay(sfgen[sfgen_delayVibLFO]);
  envelope->pitch   = 0xe000;
  envelope->ip      = envelope->pitch;
  envelope->filter  = (emu_filter(sfgen[sfgen_initialFilterFc])<<8);
  envelope->atten   = (emu_atten(sfgen[sfgen_initialAttenuation])<<0);
  envelope->ifatn   = envelope->filter+envelope->atten;
  envelope->pefe    = (emu_mod(sfgen[sfgen_modEnvToPitch],1200)<<8)
		    + (emu_mod(sfgen[sfgen_modEnvToFilterFc],7200)<<0);
  envelope->fmmod   = (emu_mod(sfgen[sfgen_modLfoToPitch],1200)<<8)
		    + (emu_mod(sfgen[sfgen_modLfoToFilterFc],3600)<<0);
  envelope->tremfrq = (emu_mod(sfgen[sfgen_modLfoToVolume],120)<<8)
		    + (emu_freq(sfgen[sfgen_freqModLFO])<<0);
  envelope->fm2frq2 = (emu_mod(sfgen[sfgen_vibLfoToPitch],1200)<<8)
		    + (emu_freq(sfgen[sfgen_freqVibLFO])<<0);
  envelope->volsust = (emu_sustain(sfgen[sfgen_sustainVolEnv])<<8);
  envelope->volrel  = (emu_release(sfgen[sfgen_releaseVolEnv])<<0);
  envelope->dcysusv = (0<<15) // we're programming decay not release
		    + envelope->volsust
		    + (0<<7) // turn on envelope engine
		    + (emu_decay(sfgen[sfgen_decayVolEnv])<<0);
  envelope->ptrx    = (0x4000<<16) // ??? I thought the top word wasn't for me to use
		    + (emu_reverb(sfgen[sfgen_reverbEffectsSend])<<8)
		    + (0<<0);      // unused
  envelope->pan     = emu_pan(sfgen[sfgen_pan]);
  envelope->loopst  = emu_address(sfgen[gfgen_startloopAddrs],sfgen[sfgen_startloopAddrsOffset],sfgen[sfgen_startloopAddrsCoarseOffset]);
  envelope->sampend = emu_address(sfgen[gfgen_endAddrs],sfgen[sfgen_endAddrsOffset],sfgen[sfgen_endAddrsCoarseOffset]);
  envelope->psst    = (envelope->pan<<24)
		    + (envelope->loopst);
  envelope->chorus  = emu_chorus(sfgen[sfgen_chorusEffectsSend]);
  envelope->csl     = (envelope->chorus<<24)
		    + (emu_address(sfgen[gfgen_endloopAddrs],sfgen[sfgen_endloopAddrsOffset],sfgen[sfgen_endloopAddrsCoarseOffset])<<0);
  envelope->ccca    = (emu_filterQ(sfgen[sfgen_initialFilterQ])<<28)
		    + (0<<24)     // DMA control bits
		    + (emu_address(sfgen[gfgen_startAddrs],sfgen[sfgen_startAddrsOffset],sfgen[sfgen_startAddrsCoarseOffset])<<0);

  envelope->rootkey = (sfgen[sfgen_overridingRootKey]!=-1)?sfgen[sfgen_overridingRootKey]:69;
  envelope->ipbase  = ((sfgen[sfgen_coarseTune]*100+sfgen[sfgen_fineTune]-envelope->rootkey*sfgen[sfgen_scaleTuning])*0x1000)/1200+0xE000;
  envelope->ipscale = sfgen[sfgen_scaleTuning];

  envelope->minkey  = sfgen[sfgen_keyRange]&0xff;
  envelope->maxkey  = (sfgen[sfgen_keyRange]>>8)&0xff;
  envelope->minvel  = sfgen[sfgen_velRange]&0xff;
  envelope->maxvel  = (sfgen[sfgen_velRange]>>8)&0xff;
  envelope->key     = -1;
  envelope->vel     = -1;
  envelope->exc     = sfgen[sfgen_exclusiveClass];
  envelope->keyMEH  = 0;
  envelope->keyMED  = 0;
  envelope->keyVEH  = 0;
  envelope->keyVED  = 0;
  envelope->smpmode = sfgen[sfgen_sampleModes];
 }
 return envelope;
}

/* translate_soundfont_into_something_useful:
 *  Like it says, translate the soundfont data into something we can use
 *  when playing notes.
 */
static void translate_soundfont_into_something_useful(void)
{
	int p,s,gen,weirdo;
	struct midi_preset_t *thing_to_write=NULL;
	generators_t temp_gens;
	int global_split=0,global_weirdo=0,num_weirdos;

	midi_preset = (struct midi_preset_t *)malloc(129*sizeof(struct midi_preset_t));
	for (p=0;p<_awe_sf_num_presets;p++)
	{
		if (_awe_sf_presets[p*3+1]==0)
			thing_to_write = &midi_preset[_awe_sf_presets[p*3+0]];
		else if (_awe_sf_presets[p*3+1]==128)
			thing_to_write = &midi_preset[128];
		else
			thing_to_write = NULL;
		if(thing_to_write)
		{
			thing_to_write->num_splits=_awe_sf_presets[p*3+2];
			thing_to_write->split=(struct envparms_t **)malloc(thing_to_write->num_splits*sizeof(struct envparms_t *));
			for(s=0;s<thing_to_write->num_splits;s++)
			{
				for(gen=0;gen<SOUNDFONT_NUM_GENERATORS-4;gen++)temp_gens[gen] = _awe_sf_defaults[gen];
				num_weirdos = _awe_sf_splits[global_split];
				for(weirdo=global_weirdo;weirdo<global_weirdo+num_weirdos;weirdo++) temp_gens[_awe_sf_gens[weirdo*2]] = _awe_sf_gens[weirdo*2+1];
				global_weirdo+=num_weirdos;
				for(gen=0;gen<4;gen++)temp_gens[gfgen_startAddrs+gen] = _awe_sf_sample_data[global_split*4+gen];
				global_split++;
				thing_to_write->split[s]=emu8k_createenvelope(temp_gens);
			}
		}
		else;
			// AWE32 driver: had trouble with the embedded data
	}
}

/****                                   ****/
/****                                   ****/
/**** End of AWE-only section           ****/
/****                                   ****/
/****                                   ****/

void SavePos(char *ToSavePos, int ToSaveOrd, int ToSaveRow, int ToSavePat)
{						//Saves the playing position	(SB0)
	SavedPtr = ToSavePos;
	SavedRow = ToSaveRow;
	SavedOrd = ToSaveOrd;
	SavedPat = ToSavePat;
}

void RestorePos(void)	//Restores the playing position (SBx, x!=0)
{
	PatPos = SavedPtr;
	CurRow = SavedRow;
	CurOrd = SavedOrd;
	CurPat = SavedPat;
}

void PlayLine(void)
{
		byte ByteRead, Cmd, Puh, Note, Instru, Info, Oct;
	int Restore, Save, Cut, Break, CutLine, BreakOrder, CurChn;

	if(PatternDelay)
	{
		PatternDelay--;
		return;
	}

	ToSavePos = PatPos;
	ToSaveOrd = CurOrd;
	ToSaveRow = CurRow;
	ToSavePat = CurPat;

	for(Restore=Save=Cut=Break=0 ; (ByteRead = *PatPos++) != 0; )
	{
		CurChn = Hdr->Channels[ByteRead & 31] & 15;
		Puh    = 255;
		Instru = CurInst[CurChn];
		if(ByteRead & 32)
		{
			int a;
			Puh = *PatPos++;

			Oct = Puh>>4;

			Note= Puh&15;

			a = *PatPos++;

			if(a)CurInst[CurChn] = Instru = a;

			if(CurChn < MAXCHN)
				CurVol[CurChn] = (long)Instr[Instru]->Volume << 16;
		}

		if(ByteRead & 64)
		{
			int a = *PatPos++;
			if(CurChn < MAXCHN)CurVol[CurChn] = (long)a << 16;
			if(!a && Puh==255)Puh = 254;
			NoteDelay[CurChn] = 0;
		}

		Cmd = '-';
		if(ByteRead & 128)
		{
			Cmd = (*PatPos++) | 64;
			Info = *PatPos++;
		}

		if(CurChn < MAXCHN)
			if((CurPuh[CurChn]=Puh) < 254)
			{
				Herz[CurChn] = (unsigned long)(Period[Note] * Instr[Instru]->C2Spd / 22000L) << Oct;
				NNum[CurChn] = (12*Oct) + Note;
			}

		if(Cmd != 'D' && Cmd != 'K' && CurChn<MAXCHN)
			VolSlide[CurChn] = 0;

		if(Cmd != 'E' && Cmd != 'F' && CurChn<MAXCHN)
			FreqSlide[CurChn] = 0;

#ifdef VIBRATO
		if(Cmd != 'H' && Cmd != 'K' && CurChn<MAXCHN)
		{
			VibDepth[CurChn] =
			VibSpd  [CurChn] =
			VibPos  [CurChn] = 0;
		}
#endif
		if(Puh!=255 && CurChn<MAXCHN)
			NoteCut[CurChn] = NoteDelay[CurChn] = 0;

		switch(Cmd)
		{
			case 'A':
				Hdr->InSpeed = Info;
				break;
			case 'T':
				Hdr->InTempo = Info;
				break;
			case 'B':
				Break = 1;
				BreakOrder = Info;
				break;
			case 'D':
			case 'K':
				if(Info==0xFF)CurVol[CurChn]=63UL<<16;
				else if((Info&0xF0)==0xF0)
				{
					VolSlide[CurChn] = -(Info&0x0F);
					FineVolSlide[CurChn] = 1;
				}
				else if((Info&0x0F)==0x0F)
				{
					VolSlide[CurChn] = (Info&0x0F);
					FineVolSlide[CurChn] = 1;
				}
				else if(!Info);
				else if(!(Info&0xF0))
				{
					VolSlide[CurChn] = -(Info&0x0F);
					FineVolSlide[CurChn] = 0;
				}
				else if(!(Info&0x0F))
				{
					VolSlide[CurChn] =  (Info&0xF0)>>4;
					FineVolSlide[CurChn] = 0;
				}
#ifdef VIBRATO
				if(Cmd=='K'){Info=0; goto CmdH;}
#endif
				break;
			case 'E':
				if(Info && CurChn<MAXCHN)FreqSlide[CurChn] = -Info;
				break;
			case 'F':
				if(Info && CurChn<MAXCHN)FreqSlide[CurChn] = Info;
				break;
#ifdef VIBRATO
			case 'H':
			{
CmdH:			//int q, Temp;
				if(Info & 0x0F && CurChn<MAXCHN)VibDepth[CurChn] = Info & 0x0F;
				if(Info & 0xF0 && CurChn<MAXCHN)VibSpd  [CurChn] =(Info & 0xF0) >> 2;
				//q = (VibPos[CurChn] >> 2) & 0x1F;
				//Temp = ((VibratoTable[q] * VibDepth[CurChn]) >> 7) << 2;
				if(CurChn<MAXCHN)VibPos[CurChn] += VibSpd[CurChn];
				break;
			}
#endif
			case 'C':
				Cut = 1;
				CutLine = (Info>>4)*10 + (Info&15);
				break;
			case 'S':
				if(Info == 0xB0)Save = 1;
				else if((Info > 0xB0)&&(Info <= 0xBF))
				{
					if(LoopCount == 0)
						LoopCount = Info&0x0F;
					else
						LoopCount--;

					if(LoopCount)Restore = 1;
				}
				else if((Info >= 0xD0)&&(Info <= 0xDF))
				{
					if(CurChn<MAXCHN)
						NoteDelay[CurChn] =
							(Info & 15) * (long)Rate * SPEEDCONST / Hdr->InTempo;
				}
				else if((Info >= 0xC0)&&(Info <= 0xCF))
				{
					if(CurChn<MAXCHN)
						NoteCut[CurChn] =
							(Info & 15) * (long)Rate * SPEEDCONST / Hdr->InTempo;
				}
				else if((Info >= 0xE0)&&(Info <= 0xEF))
					PatternDelay = Info - 0xE0;
		}
	}
	if(Restore)
	{
		RestorePos();
		Restored = 1;
		return;
	}
	CurRow++;
	if((word)PatPos >= (word)Patn[CurPat].Ptr+Patn[CurPat].Len)
	{
		CurOrd++;
		goto P3;
	}
	if(Save)
	{
		if(Restored == 0)
		{
			SavePos(ToSavePos, ToSaveOrd, ToSaveRow, ToSavePat);
			LoopCount = 0;
		}
	}
	Restored = 0;
	if(Break)
	{
		CurOrd = BreakOrder;
P3:		if(!Cut)CutLine = 0;
		goto P2;
	}
	if(Cut)
	{
P1:		do {
			CurOrd = (CurOrd+1)%Hdr->OrdNum;
P2:			CurPat = Orders[CurOrd];
		} while(CurPat >= 0xFE);
		PatPos = Patn[CurPat].Ptr;
		CurRow = CutLine + 1;
		while(CutLine)
		{
			for(;;)
			{
				ByteRead = *PatPos++;
				if(!ByteRead)break;
				if(ByteRead&32)PatPos+=2;
				if(ByteRead&64)PatPos++;
				if(ByteRead&128)PatPos+=2;
			}
			CutLine--;
}	}	}

void interrupt NewI08(...)
{
    int Limit;

	_asm { db 0x66; pusha; sti }

	/* ST32 does this. Don't ask me why. Wants a quiet beeper? */
/*	_asm {
		cli
		mov al, 0x0B6
		out 0x43, al
		mov ax, 0xFFFF
		out 0x42, al
		out 0x42, al
		sti
	}
*/
	/* Check if we can play next row... */
	if(!Paused)
	{
		if(RowTimer == 0)
		{
			PlayLine();
			PlayNext = 1;
		}

		Limit = (long)Rate * (long)Hdr->InSpeed * SPEEDCONST / Hdr->InTempo;

		if(!RowTimer)RowTimer = Limit + 1;

		for(int CurChn=0; CurChn<9; CurChn++)
		{
			int Ope = Portit[CurChn];
			long Vol = CurVol[CurChn];
			int Instru = CurInst[CurChn];

			if(NoteCut[CurChn] > 0)NoteCut[CurChn]--;
			else if(!NoteCut[CurChn])
			{
				NoteCut[CurChn] = -1;
				AdlNoteOff(CurChn);
				GM_NoteOff(CurChn);
			}

			if(NoteDelay[CurChn] > 0)NoteDelay[CurChn]--;
			else if(!NoteDelay[CurChn])
			{
				NoteDelay[CurChn] = -1;
				switch(CurPuh[CurChn])
				{
					case 255:
						GM_Touch(CurChn, Vol>>16);
						break;
					case 254:
						AdlNoteOff(CurChn);
						GM_NoteOff(CurChn);
						break;
					default:
						if(Instr[Instru]->Typi == 2)
						{
							AdlNoteOff(CurChn);
							GM_NoteOff(CurChn);

							WriteR(0x20+Ope, Instr[Instru]->D[0]);
							WriteR(0x60+Ope, Instr[Instru]->D[4]);
							WriteR(0x80+Ope, Instr[Instru]->D[6]);
							WriteR(0xE0+Ope, Instr[Instru]->D[8]);

							WriteR(0x23+Ope, Instr[Instru]->D[1]);
							WriteR(0x63+Ope, Instr[Instru]->D[5]);
							WriteR(0x83+Ope, Instr[Instru]->D[7]);
							WriteR(0xE3+Ope, Instr[Instru]->D[9]);

							WriteR(0xC0+CurChn, Instr[Instru]->D[10]);

							GM_Patch(CurChn, Instr[Instru]->GM);
							GM_KeyOn(CurChn, NNum[CurChn], Vol>>16);
							// GM_Touch(CurChn, Vol>>16);

							SetVolume(Ope, Instru, Vol >> 16);
							UpdHerz(CurChn, Herz[CurChn]);
						}
				}
			}

			if(VolSlide[CurChn] && FineVolSlide[CurChn]<2)
			{
				Vol += ((long)(VolSlide[CurChn] * (Hdr->InSpeed-1)) << 16) / Limit;

				if(Vol < (        0))Vol = (        0);
				if(Vol > (64L << 16))Vol = (64L << 16);

				SetVolume(Ope, Instru, (CurVol[CurChn] = Vol) >> 16);

				GM_Touch(CurChn, Vol>>16);

				if(FineVolSlide[CurChn])FineVolSlide[CurChn] = 2;
			}

			//GM_Touch(CurChn, Vol>>16);

			if(FreqSlide[CurChn])
			{
				UpdHerz(CurChn,
					Herz[CurChn] +=
					(((long)(FreqSlide[CurChn] * (Hdr->InSpeed-1)) << 16) / Limit) >> 12
					);

				GM_Bend(CurChn, 0x2000); // Not implemented yet!!
			}

#ifdef VIBRATO
			// Vibrato - not correctly working yet
			if(Herz[CurChn] && VibDepth[CurChn])
			{
				int VibVal = avibtab[VibPos[CurChn] & 127];
				if(VibPos[CurChn] & 0x80)VibVal = -VibVal;
				int VibDpt = VibDepth[CurChn] << 8;
				VibVal = (VibVal*VibDpt) >> 12;

				UpdHerz(CurChn, Herz[CurChn] - VibVal*8);

				GM_Bend(CurChn, 0x2000); // Not implemented yet!!

				VibPos[CurChn]++;
			}
#endif
		}
		RowTimer--;
	}

	DelayCount++;

	_asm { db 0x66; popa }

	/* Check if we have now time to call the old slow timer IRQ */
	_asm {
		mov ax, Counter
		add Clock, ax
		jnc P1
		pushf
		call dword ptr OldI08
		jmp P2
	}

	/* Then tell IRQ controller that we've finished. */
P1:	_asm mov al, 0x20
	_asm out 0x20, al
P2:
}

void StartS3M(char *s)
{
	byte a;

	StopS3M();

	Paused = 0;

	// I love pointers :) N„in p„„stiin eroon yhdest„ memmovesta.
	Hdr = (InternalHdr *)(Playing = s);
	s += sizeof *Hdr;

	// T„ll„ tyhm„ll„ loopilla p„„stiin eroon toisesta memmovesta.
	for(a=0; a<Hdr->OrdNum; Orders[a++] = *s++);

	for(a=0; a<Hdr->InsNum; a++)
	{
		Instr[a+1] = (InternalSample *)s;
		s += sizeof(InternalSample);
	}

	for(a=0; a<Hdr->PatNum; a++)
	{
		Patn[a].Len = *(word *)s;
		Patn[a].Ptr = (s += 2);
		s += Patn[a].Len;
	}

	for(a=0; a<9; a++)
	{
		WriteR(0x40+Portit[a], CurVol[a]=0);  //Scaling levels...
		WriteR(0x43+Portit[a], CurInst[a]=0);
		NoteDelay[a] = -1;
	}

	CurPat = Orders[CurOrd = 0];
	PatPos = Patn[CurPat].Ptr;
	CurRow = 1;

	if(!IRQActive)
	{
		/* If timer was not active, start it */
		IRQActive = 1;
		OldI08 = GetIntVec(0x08);
		_asm cli
		SetIntVec(0x08, NewI08);
		Counter = 0x1234DCLU / Rate;
		_asm {
			mov al, 0x34
			out 0x43, al
			mov ax, Counter
			out 0x40, al
			mov al, ah
			out 0x40, al
		}
		_asm sti
}	}

void StopS3M(void)
{
	if(IRQActive)
	{
		IRQActive = 0;
		SetIntVec(0x08, OldI08);
		_asm {
			mov al, 0x34
			out 0x43, al
			xor al, al
			out 0x40, al
			out 0x40, al
		}
		OPLReset();
	}
}

void SaveAdlStatus(char *Buf)
{
	int a;
	for(a=0; a<244; a++)Buf[a] = AdlStatus[a];
	Buf[244] = CurRow;
	Buf[245] = CurOrd;
	Buf[246] = CurPat;
	Buf[247] = PatternDelay;
	*(char **)&Buf[248] = Playing;
	*(char **)&Buf[252] = PatPos;
	*(InternalHdr **)&Buf[256] = Hdr;

	for(a=0; a<32; a++)
	{
		Buf[260+a   ] = GM_Notes[a];
		Buf[260+a+32] = GM_Pats [a];
		Buf[260+a+64] = GM_Vol  [a];
	}

	for(a=0; a<MAXCHN; a++)
	{
				  Buf[260+96+a           ] = VolSlide[a];
#ifdef VIBRATO
				  Buf[260+96+a  +MAXCHN*1] = VibDepth[a];
				  Buf[260+96+a  +MAXCHN*2] = VibSpd[a];
				  Buf[260+96+a  +MAXCHN*3] = VibPos[a];
#endif
				  Buf[260+96+a  +MAXCHN*4] = CurInst[a];
		*(long *)&Buf[260+96+a*2+MAXCHN*5] = Herz[a];
		*(long *)&Buf[260+96+a*4+MAXCHN*9] = CurVol[a];
				  Buf[260+96+a  +MAXCHN*13]= FreqSlide[a];
				  Buf[260+96+a  +MAXCHN*14]= NoteDelay[a];
				  Buf[260+96+a  +MAXCHN*15]= CurPuh[a];
				  Buf[260+96+a  +MAXCHN*16]= NoteCut[a];
				  Buf[260+96+a  +MAXCHN*17]= NNum[a];
				  Buf[260+96+a  +MAXCHN*18]= GM_Percu[a];
	}
}

void PauseS3M(void)
{
	Paused = !Paused;
	if(Paused)
	{
		int a;
		OPLReset();
		for(a=0; a<9; GM_NoteOff(a++));
	}
}

void RestoreAdlStatus(char *Buf)
{
	int a;

	StartS3M(*(char **)&Buf[256]);

	for(a=0; a<9; GM_NoteOff(a++));
	for(a=0; a<244; a++)WriteR(a, Buf[a]);

	CurRow = Buf[244];
	CurOrd = Buf[245];
	CurPat = Buf[246];
	PatternDelay = Buf[247];
	Playing= *(char **)&Buf[248];
	PatPos = *(char **)&Buf[252];

	for(a=0; a<32; a++)
	{
		GM_Notes[a] = Buf[260+a   ];
		GM_Pats [a] = Buf[260+a+32];
		GM_Vol  [a] = Buf[260+a+64];
	}

	for(a=0; a<MAXCHN; a++)
	{
		VolSlide[a] = 		    Buf[260+96+a           ];
#ifdef VIBRATO
		VibDepth[a] = 		    Buf[260+96+a  +MAXCHN*1];
		VibSpd[a]   = 		    Buf[260+96+a  +MAXCHN*2];
		VibPos[a]   = 		    Buf[260+96+a  +MAXCHN*3];
#endif
		CurInst[a]  = 		    Buf[260+96+a  +MAXCHN*4];
		Herz[a]     = *(long *)&Buf[260+96+a*2+MAXCHN*5];
		CurVol[a]   = *(long *)&Buf[260+96+a*4+MAXCHN*9];
		FreqSlide[a]= 		    Buf[260+96+a  +MAXCHN*13];
		NoteDelay[a]= 		    Buf[260+96+a  +MAXCHN*14];
		CurPuh[a]   = 		    Buf[260+96+a  +MAXCHN*15];
		NoteCut[a]  = 		    Buf[260+96+a  +MAXCHN*16];
		NNum[a]     = 		    Buf[260+96+a  +MAXCHN*17];
		GM_Percu[a] =           Buf[260+96+a  +MAXCHN*18];
	}
}

void InitADLIB(int Autodetect)
{
	if(Autodetect)
	{
		OPLFound();
		MPUFound();
	}
}

int SelectADLDev(int Selection)
{
	IsOPLThere=IsMPUThere=1;
	if(Selection != SelectOPL)IsOPLThere=0;
	if(Selection != SelectMPU)IsMPUThere=0;

	if(IsMPUThere)if(!MPUFound())return -1;
	if(IsOPLThere)if(!OPLFound())return -1;

	return 0;
}

void ExitADLIB(void)
{
	StopS3M();
	OPLReset();
	MPUReset();
}
