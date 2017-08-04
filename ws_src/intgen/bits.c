#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Värikoodeja tulostusta varten */
#define B "\33[1;34m"
#define S "\33[1;31m"
#define C "\33[1;30m"
#define N "\33[m"

const int nmerk    = 10;
const int eka_arvo = 0;

static char Jono[512];
char *Jonoloppu = Jono+511;
char *Jonohead  = Jono+511;

static int Buf=0;	/* Bufferi */
static int bk=1;	/* Kerroin bufferin huippua varten */
	
void PrintVirta(char *term)
{
	char *s;
	for(s=Jonohead; s<=Jonoloppu; s++)printf("%X", *s);
	printf("%s", term);
}

void Putbits(int value, int nbits)
{
	char *s;
	int tmp, carry, n1=1<<nbits;
	
	printf("%d * ", n1);
	PrintVirta(" + ");
	printf("%d = ", value);
	
	carry=0;
	for(s=Jonoloppu; s>=Jonohead; s--)
	{
		tmp = carry + *s * n1;
		*s  = tmp % nmerk;
		carry=tmp / nmerk;
	}
	while(carry > 0)
	{
		*--Jonohead = carry % nmerk;
		carry /= nmerk;
	}
	
	PrintVirta(" + ");
	printf("%d = ", value);
	
	s = Jonoloppu;
	for(tmp = *s + value; ; )
	{
		*s = tmp % nmerk;
		carry = tmp / nmerk;
		if(!carry)break;
		tmp = *--s + carry;
		if(s<Jonohead)Jonohead=s;
	}	
	
	PrintVirta("\n");
}

int Getbits(int nbits)
{
	char *s;
	int a,b, k, luku;
	
	k = 1<<nbits;	
	luku = 0;
	
	PrintVirta(" / ");
	printf("%d = ", k);

	for(; !*Jonohead && Jonohead<Jonoloppu; Jonohead++);
	
	for(s=Jonohead; s<=Jonoloppu; s++)
	{
		luku = luku*nmerk + *s;
		
		/* Olisiko tämä nopeampi, jos tän ottais pois? */		
		#if 1
		if(luku < k)
			*s = 0;
		else
		#endif		
		{
			*s = luku / k;
			luku %= k;
		}
	}
	
	PrintVirta("; left ");
	printf("%d\n", luku);
	
	return luku;
}

#define NSAMP 18

int main(void)
{
	int a;
	int bits[NSAMP]; /* data */
	int pit[NSAMP];	 /* datan bittimäärät */
	
	printf(B"Tehdään data joka halutaan tallentaa...\n\n"N);
	
	srand(time(0));
	
	for(a=0; a<NSAMP; a++)
	{
		pit[a]  = 1 + (a%5);
		bits[a] = rand() % (1 << pit[a]);
	}
	
	printf(B"Syötetään bittivirtaan seuraavat bitit:\n"N);
	
	for(a=0; a<NSAMP; a++)
	{
		int b;
		for(b=pit[a]; --b>=0; printf("%d", (bits[a]>>b)&1));
		printf(S"|"N);
	}
	
	printf("\n");
	
	for(a=NSAMP; --a>=0; )
		Putbits(bits[a], pit[a]);
	
	printf(
		B"\n\nBittivirta näyttää nyt tältä "
		"(merkkejä väliltä %02X..%02X ('%c'..'%c')):\n\33[m",
		eka_arvo, eka_arvo+nmerk-1,
		eka_arvo, eka_arvo+nmerk-1);
		
	PrintVirta("\n");
	
	printf(B"\nLuetaan bittivirta:\n"N);
	
	for(a=0; a<NSAMP; a++)
	{
		int b;
		bits[a] = Getbits(pit[a]);
	}
	
	for(a=0; a<NSAMP; a++)
	{
		int b;
		for(b=pit[a]; --b>=0; printf("%d", (bits[a]>>b)&1));
		printf(S"|"N);						
	}
	
	printf(B"\n\nMeniköhän nyt ihan oikein?\n\33[m");
	
	return 0;
}
