#ifndef __CARD_CS_INC
#define __CARD_CS_INC

#include "music.h"
#if defined(__BORLANDC__) || defined(DJGPP)
#include <conio.h>
#endif

#include <stdarg.h>

/* Default Card Selection Routines */
int SelectCard(char *dummy, ...)
{
	va_list ap;
	char Viiva[81];
	char *Selections[16];
	char *Descr[16];
	enum cardcs Opt[16];
	char *t;
	int x, a, c, co;

	textattr(7);
	clrscr();

	va_start(ap, dummy);

	for(a=0; ;a++)
	{
		t = va_arg(ap, char *);
		if(!t)break;

		Selections[a] = t;
		Opt[a]        = va_arg(ap, enum cardcs);
		Descr[a]      = va_arg(ap, char *);
	}

	for(x=0; x<80; x++)Viiva[x]='-';
	Viiva[x]=0;
	gotoxy(1, 20-a); cprintf(Viiva);

	#ifdef SelInitDisp
	 /* Hack for wspeed */
	 SelInitDisp(Riveja);
	#endif

	for(co=7;;)
	{
		#ifdef SelInitDisp
		 /* Another hack for wspeed */
		 gprintf(0, 20-a,co, BParm, "%-80s", dummy);
		 gprintf(0, 21-a,co, BParm, "%-80s", SoundError);
		 gprintf(0, 22-a, 7, BParm, Viiva);
		#else
		 gotoxy(1, 21-a); cprintf("%-80s", dummy);
		 gotoxy(1, 22-a); cprintf("%-80s", SoundError);
		 textattr(7);
		 gotoxy(1, 23-a);
		 cprintf(Viiva);
		#endif

		for(c=0; ;)
		{
			for(x=0; x<19-a; x++)
			{
				gotoxy(1, x+1);
				cprintf("%-80s", "");
			}
			gotoxy(1, 1);
			puts(Descr[c]);
			#ifdef SelInitDisp
			 SelInitDisp(19-a);
			#endif

			for(x=0; x<a; x++)
			{
				#ifdef SelInitDisp
				 gprintf(0, x+23-a, c==x?0x1F:7,BParm,"%-80s",Selections[x]);
				#else
				 gotoxy(1, x+24-a);
				 textattr(c==x?0x1F:7);
				 cprintf("%-80s", Selections[x]);
				#endif
			}

			textattr(co=7);

			#ifdef SelInitDisp
			 /* Another hack for wspeed */
             while(!kbhit())GenIdle;
            #endif

			for(;;)
			{
				x=getch();
				if(x != 0xE0 && x != 0x00)break;
			}
			switch(x)
			{
				case 'P'-'@':
				case 'A': case 'D':
				case 'K': case 'H': /*if(c>  0)*/c = (c+a-1)%a; break;
				case 'N'-'@':
				case 'B': case 'C':
				case 'M': case 'P': /*if(c<a-1)*/c = (c+1  )%a; break;
				#if defined (__BORLANDC__) || defined(DJGPP)
				case 27:
				#endif
				case '-': return -1;
				case '\n':
				case '\r':
					goto Breik;
			}
		}
Breik:

		va_end(ap);

        if(SelectMusDev(Opt[c]) >= 0)break;

		textattr(co=4);
		dummy="Sound initialization failed";
	}
	gotoxy(1, 23);
	co=co; /* Disable erronous 'never used' warning */
	return Opt[c];
}

int DoSelectCard(void)
{
    InitMusic(NO_AUTODETECT);
    SelectMusDev(SelectNone);
	return SelectCard(
		"Select the music output device",
        "OPL3/FM synthesis, SB/AdLib -like cards etc... (NOT AudioPCI 128)", SelectOPL,
			"This driver can be used with any Ad Lib-compatible FM music\n"
			"synthesizer card (including Sound Blaster compatibles) that\n"
			"does not rely on software TSRs for Ad Lib FM compatibility.\n"
			"Users of more sophisticated General MIDI-based cards should\n"
			"select the Ad Lib driver only if problems occur with the card's\n"
			"General MIDI driver, as the Ad Lib OPL chipset does not offer\n"
            "the superior sound quality available with more modern devices.",
#if SUPPORT_MIDI
		"General MIDI (Roland MPU-401 interface or 100% compatible)", SelectMPU,
			"Most General MIDI synthesizers support the Roland MPU-401 MIDI\n"
			"hardware interface standard. MPU-401 devices generally offer the\n"
			"highest level of performance and sound quality available, but\n"
			"because it uses the port 330h, users of Adaptec SCSI controllers\n"
			"may have some problems with this. If you are using Creative Labs\n"
            "AWE-32 or AWE-64 and you are not using Windows, you should select\n"
            "\"Creative Labs AWE-32 / AWE-64 General MIDI\" instead, because the\n"
            "aweutil.com seems to be unstable sometimes... If you have AWE-32 or\n"
            "AWE-64 and you are using Windows, you should maybe select this\n"
			"selection instead because this can be more stable under Windows :)\n"
			"This driver is just as stable as your MPU-401 hardware appears as.",
#endif
#if SUPPORT_AWE
		"Creative Labs AWE-32 / AWE-64 General MIDI", SelectAWE,
			"This driver offers wavetable-quality General MIDI music\n"
			"synthesis with the Creative Labs AWE-32 / AWE-64 card.\n"
			"If you have such card and you are using Windows, you maybe\n"
			"would like to select the \"General Midi\" driver instead, if\n"
			"you are using custom soundfonts... This AWE-driver is just\n"
			"as stable as the Creative ADIP rawe32*.lib is...",
#endif
#if SUPPORT_DSP
        "Creative SB MIDI (It probably don't work)", SelectDSP,
			"A strange music device which reference to I found first time from a SB\n"
            "development kit. I wonder if there exists hardware which this works with.",
#endif
		"Silent quietness", SelectSilence,
            "Select this driver, if you don't like any music.",
		NULL);
}

#endif
