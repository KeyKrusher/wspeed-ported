#ifndef __USECURSES_H
#define __USECURSES_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <netdb.h>   
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#define Random(val) (((val)<2)?0:(random()%(val)))
#define Randomize() srand(time(NULL)+getppid());

#define SH_DENYALL (0600 + 0060 + 0006)
#define SH_DENYNONE 0000
#define SH_DENYWRITE (S_IWUSR|S_IWGRP|S_IWOTH)
#define SH_DENYREAD (S_IRUSR|S_IRGRP|S_IROTH)
#define fsopen _fsopen

#define SH_DENYRW SH_DENYALL

void DoneCons(void);
void InitCons(void);

char *strlwr(char *string);
char *strupr(char *string);
int strnicmp(const char *eka, const char *toka, long Max);

#define stricmp(a,b) strnicmp((a),(b), 0x7FFFFFFFL)

const char *strscan(const char *str, char Ch);
char *strccat(char *Dest, char Ch);

void Error(char *Where);

int kbhit(void);
int getch(void);

void GotoXY(int x, int y);
void ClrScr(void);
void ClrEOL(void);

#endif
