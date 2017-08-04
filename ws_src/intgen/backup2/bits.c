#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* V‰rikoodeja tulostusta varten */
#define B "\33[1;34m"
#define S "\33[0;34m"
#define C "\33[1;30m"
#define N "\33[m"

const int nmerk    = 94;
const int eka_arvo = 33;

char Jono[512];
char *Jonoloppu = Jono;	/* Putbitsi‰ varten */
char *Jonohead = Jono;	/* Getbitsi‰ varten */

/* Minusta t‰m‰ on aivan kunnossa */
void Putbits(int value, int nbits)
{
	static int Buf=0;	/* Bufferi */
	static int bk=1;	/* Kerroin bufferin huippua varten */
	
	Buf = Buf + value*bk;	
	
	bk <<= nbits;		/* Eli  bk *= (1 << nbits)  */
	
	while(bk > nmerk)
	{
		int tmp = Buf % nmerk;
		
		printf(C"%02X"N, tmp);
		
		*Jonoloppu++ = (eka_arvo + tmp) &255;
		
		Buf /= nmerk;
		bk /= nmerk;			
	}
}

/* Mutta t‰m‰ toimii oikein vain mik‰li nmerk on joku kahden potenssi. */
int Getbits(int nbits)
{
	static int Buf=0;
	static int bk=1;	/* Vain t‰t‰ pienempi‰ lukuja voidaan hakea Buf:st‰ */
	
	int retval;
	
	while((1 << nbits) > bk)	/* Onko suurempi kuin bk? */
	{
		int tmp = (((*Jonohead++) - eka_arvo) & 255);
		
		printf(C"%02X"N, tmp);
		
		Buf = Buf + bk * tmp;
		bk *= nmerk;
	}
	
	retval = Buf & ((1<<nbits)-1);
	
	bk  >>= nbits;
	Buf >>= nbits;
	
	return retval;		
}

#define NSAMP 16

int main(void)
{
	int a;
	int bits[NSAMP]; /* data */
	int pit[NSAMP];	 /* datan bittim‰‰r‰t */
	
	srand(time(0));
	
	printf(B"Tehd‰‰n data joka halutaan tallentaa...\n\n"N);
	
	for(a=0; a<NSAMP; a++)
	{
		pit[a]  = 1 + (a%5);
		bits[a] = rand() % (1 << pit[a]);
	}
	
	printf(
		C"*"B" T‰ll‰ v‰rill‰ on merkitty bitflush-operaatio...\n"
		"Syˆtet‰‰n bittivirtaan seuraavat bitit:\n"N);
	
	for(a=0; a<NSAMP; a++)
	{
		int b;
		for(b=pit[a]; --b>=0; printf("%d", (bits[a]>>b)&1));
		Putbits(bits[a], pit[a]);
		
		printf(S"|"N);
	}
	
	Putbits(0, 16); /* T‰m‰ loppuun varmuuden vuoksi */
	
	*Jonoloppu = 0;		
	printf(
		B"\n\nBittivirta n‰ytt‰‰ nyt t‰lt‰ "
		"(merkkej‰ v‰lilt‰ %02X..%02X ('%c'..'%c')):\n\33[m%s",
		eka_arvo, eka_arvo+nmerk-1,
		eka_arvo, eka_arvo+nmerk-1,
		Jono);
	
	printf(
		C"\n\n*"B" T‰ll‰ v‰rill‰ on merkitty bitfetch-operaatio...\n"
		"Luetaan bittivirta:\n"N);
	
	for(a=0; a<NSAMP; a++)
	{
		int b;
		bits[a] = Getbits(pit[a]);
		
		for(b=pit[a]; --b>=0; printf("%d", (bits[a]>>b)&1));
		printf(S"|"N);						
	}
	
	for(a=nmerk;!(a&1);a>>=1);
	printf(B"\n\nMenikˆh‰n nyt ihan oikein? %s\n\33[m",
		a-1?"Ei varmaankaan.":"Taisi menn‰.");
	
	return 0;
}
