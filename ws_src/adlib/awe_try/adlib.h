#ifndef __ADLIB_HPP
#define __ADLIB_HPP

/* Useful macros for system functions. Only for real/v86 mode, though. */
#define GetIntVec(a) (IRQType)(*(long *)((a)*4))
#define SetIntVec(a,b) (*(long *)((a)*4))=(long)((void far *)(b))
#define Timer() (*(unsigned int *)0x0040006CUL)

/* Useful typedefs. Extremely dangerous to modify. */
typedef unsigned long dword;
typedef unsigned short word;
typedef unsigned char byte;

/* Internal constants - should not be modified much */
#define MAXINST 100
#define MAXPATN 100
#define MAXORD  256
#define MAXCHN  9

/* Almost as accurate speed constant as possible. Should not be modified. */
#define SPEEDCONST (0x7D/0x06/8.0) //By T7D A06, we play 8 notes in second

/* Useful constants for InitADLIB() */
#define NO_AUTODETECT 0
#define AUTODETECT 1

/* Useful constants for SelectADLDev() */
#define SelectSilence 0
#define SelectOPL 1
#define SelectMPU 2

/* InitADLIB() probes OPL and MPU if nonzero parameter given */
void InitADLIB(int Autodetect);

/* Selects music playback device. See Selectxxx -defines above.
   If nothing given, selects OPL if exists. If the selected device
   does not exist or value is invalid, autoselects silence.
   Important notice: Selecting something undoes all probing of
   other playback devices. So, if the selection should be changed,
   the devices should be re-probed with InitADLIB() or
   xxxFound()-series functions.
   Another important notice: If multiple playback devices have been
   succesfully probed and no call to SelectADLDev() is issued,
   then the music is played with all detected playback devices.
   Return value nonzero if selection failed.
*/
int SelectADLDev(int Selection);

/* StartS3M(s) stars music pointed by s */
void StartS3M(char *s);

/* PauseS3M() toggles music pause */
void PauseS3M(void);

/* StopS3M() ends music */
void StopS3M(void);

/* ExitADLIB() releases the sound system */
void ExitADLIB(void);

/* PlayLine() plays single line of music */
void PlayLine(void);

/* Constant - don't modify unless you know what are you doing. */
#define STATUSBUFFERSIZE (261+96+MAXCHN*19)

/* SaveAdlStatus(Buf) saves the music status for later restoration
   Buf must point to buffer of STATUSBUFFERSIZE size. */
void SaveAdlStatus(char *Buf);

/* RestoreAdlStatus(Buf) restores the saved music status from
   Buf, which must point to buffer of STATUSBUFFERSIZE size. */
void RestoreAdlStatus(char *Buf);

extern word OPLBase;
int OPLReset(void);
int OPLFound(void);

extern word MPUBase;
int MPUFound(void);
int MPUReset(void);

typedef struct
{
	char Name[28];
	char CtrlZ;
	char FileType;	// 16 = S3M
	word Filler1;
	word OrdNum;	//Number of orders, is even
	word InsNum;	//Number of instruments
	word PatNum;	//Number of patterns
	word Flags;		//Don't care, ei mit„„n t„rke„„
	word Version;	//ST3.20 = 0x1320, ST3.01 = 0x1301
	word SmpForm;	//2 = Unsigned samples, don't care, ei adlibin asia
	char Ident[4]; 	//"SCRM"
	char GVol;		//Global volume, don't care, ei vaikuta adlibbeihin
	char InSpeed;	//Initial speed
	char InTempo;	//Initial tempo
	char MVol;		//Master volume, don't care, ei vaikuta adlibbeihin
	char UC;		//Ultraclick removal, don't care, gus-homma
	char DP;		//Default channel pan, don't care, gussiin sekin
	char Filler2[10];
	char Channels[32];//Channel settings, 16..31 = adl channels
}S3MHdr;

typedef struct
{
	char Typi;			//2 = AMel           1
	char DosName[15];	//AsciiZ file name   15
	char D[12];			//Identificators     12
	char Volume;        //                   1
	char Filler1[3];    //                   3
	word C2Spd;			//8363 = normal..?   2
	char Filler2[14];   //                   14
	char Name[28];		//AsciiZ sample name 28
	char Ident[4];		//"SCRI"             4
}ADLSample;				//                   80 = 0x50

typedef struct
{
	word OrdNum;	//Number of orders, is even
	word InsNum;	//Number of instruments
	word PatNum;	//Number of patterns
	char InSpeed;	//Initial speed
	char InTempo;	//Initial tempo
	char Channels[32];//Channel settings, 16..31 = adl channels
}InternalHdr;

typedef struct
{
	char Typi;			//2 = AMel           1
	char D[12];			//Identificators     12
	char Volume;        //                   1
	word C2Spd;			//8363 = normal..?   2
	byte GM;			//GM program number  1
}InternalSample;		//                   16 = 0x10

typedef struct Pattern
{
	word Len;
	char *Ptr;
}Pattern;

/* For IRQ subsystem */
extern unsigned int Rate;	   //Defaults to 256
							   //If you modify this, you must do it before
							   //calling StartS3M().
extern signed long DelayCount; //Increased by Rate times per second by one

extern char IRQActive; /* If playing something. Don't modify the value! */

extern char *Playing; /* The filename/buffer currently playing */

/* Modified by player */
extern char PlayNext; /* Set to 1 every time next line is played */
extern char *PatPos;
extern char CurOrd;
extern char CurPat;
extern char CurRow;

//extern long CurVol[];
extern char CurInst[];

/* Other data */
extern long Period[];
extern char Orders[];
extern Pattern Patn[];
extern char HC[];

#endif
