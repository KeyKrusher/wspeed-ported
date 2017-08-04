#include <string.h>
#include "wspeed.h"

void Laske10mrs(void)
{
	static int edkirj=0;
	static long edbeen=0;
	last10mrs=(int)(kirjaimia*60000L/beenrunning);
	if(kirjaimia!=edkirj)
	{
		cached2=cached1;
		cached1=curr10mrs;
		curr10mrs=(int)((kirjaimia-edkirj)*60000L/(beenrunning-edbeen));
	}
	edkirj=kirjaimia;
	edbeen=beenrunning;
}

int Paljonkoruudulla(void)
{
	int a, b;
	// Tarkistetaan, pit„isik” antaa lis„„ haastetta.
	for(a=b=0; a<maxitem; a++)
		if(items[a].typi == sana)
		{
			b += WordLen(items[a].param) + 1;
			// +1 = k„ytt„j„n pit„„ painaa enteri„kin
        }
	return b;
}

int CurrentSkill(void)
{
	return (int)((curr10mrs+cached1+cached2+last10mrs*3)*(long)SANAVIIVE/4500);
	// 4500 = 750*6.
	// Mist„ 750 tulee, ks sanatilanne() game.cpp:ss„.
}

char *Selita(int Speed)
{
	if(!Speed)return "F1 for help/pause";
	if(Speed < 30)return "Hibernative";
	if(Speed < 300)return "Very slow";
	if(Speed < 500)return "Slow";
	if(Speed < 600)return "Quite slow";
	if(Speed < 800)return "Needs a map of kbd";
	if(Speed < 1100)return "One finger";
	if(Speed < 1300)return "Ten finger trainer";
	if(Speed < 2000)return "Intermediate";
	if(Speed < 2400)return "Not bad, actually";
	if(Speed < 2700)return "Quite good";
	if(Speed < 3200)return "Good";
	if(Speed < 3400)return "Good+";
	if(Speed < 4200)return "Very good";
	if(Speed < 5100)return "Excellent";
	if(Speed < 6000)return "Fast, quick, or what?";
	if(Speed < 6500)return "Really fast (>10cps)";
	if(Speed < 7000)return "Twelve fingers";
	if(Speed < 8000)return "4 hands & 2 keyboards";
	return "Isn't this impossible?";
}
