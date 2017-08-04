#ifndef __WSPEED_H
#define __WSPEED_H

#include <stdio.h>
#include <alloc.h>
#include <conio.h>

#include "music.h"

#ifdef __BORLANDC__
 #include <malloc.h>
#else
 #define random(x) (rand()%(x))
 #define EINVFMT EINVAL
 #define huge
 #ifndef halloc
  #define halloc(num, size) malloc((size_t)((unsigned long)(num)*(size)))
  #define hfree free
 #endif
#endif

/* Screen pointer */
#ifdef __GNUC__
 #include <sys/nearptr.h>
 #define SCREEN	((unsigned short far *)-1)
#else
 #define SCREEN ((unsigned short far *)0xB8000000UL)
#endif

/* General idle procedure - inlined to be generally useful */
#define GenIdle {beenrunning++&3||spawnstar();drawscreen(BParm);delay(10);}

/* Maximum of items on screen */
#define maxitem 256

/* Starfield line count on screen */
#define Riveja 23

extern int SANAVIIVE;
extern int TAHTIVIIVE;

extern long beenrunning;  //peli ollut k„ynniss„, sadasosasekunteina

extern char inputline[17];
extern long score;
extern int mokia;
extern int kirjaimia;     //pelaajan kirjoittamia
extern int last10mrs;     //edellinen 10mrs
extern int curr10mrs;     //t„m„nhetkinen -"-
extern long beenrunning;  //peli ollut k„ynniss„, sadasosasekunteina
extern int sanoja;        //pelaaja kirjoittanut

extern unsigned short Buffer[25][80];
#define BParm ((unsigned short *)&Buffer)

extern char far *GameMusic;

extern char sb[512];  // Yleinen mahd. lyhytaikainen temp. bufferi

enum itemtyyppi {tyhjyys=0, tahti, sana};

typedef struct item
{
	enum itemtyyppi typi;
	int param; //jos typi==sana,  niin on sanan numero
	int x, y;  //jos typi==tahti, niin on nopeus
}item;

extern item items[maxitem];

extern unsigned char huge *sanau; // Sanaa k„ytetty

extern int sanac; // Sanaston koko
extern int used;  // Sanoja sanakirjassa

extern void CleanUp(int err);

/* HELP.CPP */
extern void HelpItem(int x, int y, int len, char *s);
extern void Help(void);

/* VIDEO.CPP */
extern int StarDir;
extern void updstatus(void);
#ifdef __GNUC__
#define pascal
#endif
extern int pascal drawscreen(unsigned short *vidbuf2);
extern int gprintf(
	int x, int y, unsigned char attr,
	unsigned short far *vid, char *fmt, ...);

/* ITEMS.CPP */
extern int pascal spawnstar(void);

/* FUNC.CPP */
extern void Laske10mrs(void);
extern int Paljonkoruudulla(void);
extern char *Selita(int Speed);
extern int CurrentSkill(void);

/* MISC.CPP */
extern const char *pascal Pathi(char *env, const char *a);
extern char *LoadMusicFile(const char *MusicFile);
extern char DictFiles[24-3];
extern int DictSel[sizeof DictFiles];
extern int DictCount;
extern char *GetWord(int i, char *dest);
extern int WordLen(int i);
extern void SelectDicts(void);
extern void PrepareDicts(void);
extern void ReleaseDicts(void);
extern int DoSelectCard(void);

/* GAME.CPP */
extern int GameLoop(void);
extern int cached1;
extern int cached2;

/* SCORE.CPP */
#include "scores.h"

/* KEY.CPP */
extern int WS_kbhit();
extern int WS_getch();

/* Parameters */
extern int Dvorak;
extern int NoDelay;     	// Viiveet”n peli (huh)
extern int NoCollision;		// Ei samoja sanoja
extern int RequireBetter;	// Peli vaikeutuu koko ajan
extern int Verbose;
extern int Training;        // Training mode -> no score.

#endif
