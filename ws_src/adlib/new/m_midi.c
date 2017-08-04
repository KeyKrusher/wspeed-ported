/* This is not a driver of any kind.
   This is just a wrapper which converts S3M style
   thinking into MIDI style thinking, used by
   MPU and AWE modules.
*/   

#include "music.h"
#include "m_midi.h"

#ifndef MIDISlideFIX
 #define MIDISlideFIX 1
 /* Changes midi volumes between 1..11 to be 11 */
#endif

const unsigned char GMVol[64] =
{
#if MIDISlideFIX
    /*00*/0x00,0x0B,0x0B,0x0B, 0x0B,0x0B,0x0B,0x0B,
    /*08*/0x0B,0x0B,0x0B,0x0B, 0x0B,0x0B,0x0B,0x0B,
    /*16*/0x0B,0x0B,0x0B,0x0C, 0x0D,0x0F,0x10,0x11,
#else
    /*00*/0x00,0x01,0x01,0x01, 0x01,0x01,0x01,0x02,
    /*08*/0x02,0x03,0x04,0x04, 0x05,0x06,0x07,0x08,
    /*16*/0x09,0x0A,0x0B,0x0C, 0x0D,0x0F,0x10,0x11,
#endif
    /*24*/0x13,0x15,0x16,0x18, 0x1A,0x1C,0x1E,0x1F,
    /*32*/0x22,0x24,0x26,0x28, 0x2A,0x2D,0x2F,0x31,
    /*40*/0x34,0x37,0x39,0x3C, 0x3F,0x42,0x45,0x47,
    /*48*/0x4B,0x4E,0x51,0x54, 0x57,0x5B,0x5E,0x61,
    /*56*/0x64,0x69,0x6C,0x70, 0x74,0x78,0x7C,0x7F
};

static void MPU_Byte(BYTE data)
{
	MusData.driver->midibyte(data);
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

Ex       1110xxxx     bb tt         Pitch wheel change (2000h is normal
                                                        or no change)
                                    bb=bottom (least sig) 7 bits of value
                                    tt=top (most sig) 7 bits of value

About the controllers... In AWE32 they are:
    0=Bank select               7=Master Volume     11=Expression(volume?)
    1=Modulation Wheel(Vibrato)10=Pan Position      64=Sustain Pedal
    6=Data Entry MSB           38=Data Entry LSB    91=Effects Depth(Reverb)
  120=All Sound Off           123=All Notes Off     93=Chorus Depth
  100=reg'd param # LSB       101=reg'd param # LSB
   98=!reg'd param # LSB       99=!reg'd param # LSB

    1=Vibrato, 121=reset vibrato,bend
*/

static BYTE GM_Percu[MAXCHN]={0};
static BYTE GM_Notes[16]={0};
static BYTE GM_Chans[16]={0};
static BYTE GM_Vol[17];
static BYTE GM_Opa[16]; /* For traffic optimization (patch) */
static BYTE GM_Oba[16]; /* For traffic optimization (bank) */
static char GM_Non[16];
static int disabled = 0;

BYTE GM_Volume(BYTE Vol) // Converts the volume
{
	if(MusData.LinearMidiVol)return Vol>=63?127:127*Vol/63;
	return GMVol[Vol>63?63:Vol];
	/* Prevent overflows */
}

void GM_Controller(int c, BYTE i, BYTE d)
{
	if(!c && i==255 && d== 255)disabled = 1;
	if(!c && i==255 && d== 254)disabled = 0;
	if(!disabled)
	{
		MPU_Byte(0xB0+GM_Chans[c]);
		MPU_Byte(i);
		MPU_Byte(d);
	}
}

void GM_Patch(int c, BYTE p)
{
	if(disabled)return;
	if(!p) p = 30;
	if(p <= 128) // Simple case, melodic instrument.
	{
		int d = c>8 ? c+1 : c; /* Disallow channel 9 (10). */
		GM_Percu[c] = 0;
		if(GM_Opa[c] != p)
		{
			MPU_Byte(0xC0+d);
			MPU_Byte((GM_Opa[c] = p)-1);
		}
	}
	else      // Percussion.
	{
		GM_Percu[c] = p-128;
	}
}

void GM_Bank(int c, BYTE b)
{
	int d;
	if(disabled)return;
	if(GM_Percu[c])
		d = 9;
	else
		d = c>8 ? c+1 : c; /* Disallow channel 9 (10). */

	if(GM_Oba[c] != b)
	{
		/* controller 0 */
		MPU_Byte(0xB0+d);
		MPU_Byte(0);
		MPU_Byte(GM_Oba[c] = b);
		GM_Opa[c] = 255;
	}
}

void GM_Touch(int c, BYTE Vol)
{
	if(disabled)return;
    if(GM_Percu[c])
	{
		MPU_Byte(0xB9);
		MPU_Byte(7);
		MPU_Byte(GM_Vol[c] = Vol);
	}
    else
	{
        Vol = GM_Volume(Vol);
		if(Vol != GM_Vol[c])
            GM_Controller(GM_Chans[c], 7, GM_Vol[c] = Vol);
	}
}

void GM_KeyOn(const int c, BYTE key, BYTE Vol)
{
	if(disabled)return;
//  if(c>1)return;
	key += 12;
	//Korkeus 12, niin kuulostaa fmdrv:llä konvertoidut
	//täsmälleen samoilta mpu:lla kuin opl:lla korkeuden
	//suhteen.
	
	GM_KeyOff(c);

	if(GM_Percu[c]) // Prepare for worst; percussion on multiple channels
	{
		MPU_Byte(0x90 + (GM_Chans[c] = 9));
		MPU_Byte(GM_Notes[c] = GM_Percu[c]);    // Skip key
		MPU_Byte(GM_Vol[c]=GM_Volume(Vol));
	}
	else
	{
		register int d = c>8 ? c+1 : c; /* Disallow channel 9 (10). */
		GM_Touch(c, Vol);
		MPU_Byte(0x90+(GM_Chans[c] = d));
		MPU_Byte(GM_Notes[c] = key+2);
		MPU_Byte(127);
	}

	GM_Non[c] = 1;	
}

void GM_KeyOff(int c)
{
	if(disabled)return;
	if(!GM_Non[c])return;
	
	GM_Non[c] = 0;
	
	MPU_Byte(0x80 + GM_Chans[c]);
	MPU_Byte(GM_Notes[c]);
	MPU_Byte(0);

	if(GM_Percu[c])
	{
		MPU_Byte(0xB9);
		MPU_Byte(7);
		MPU_Byte(127);
	}

	GM_Notes[c] = 0;
}

void GM_Bend(int c, WORD Count)
{
	/* Toivottavasti joku ei yrit„ bendata hihattia tms :-)
	   3.10.1998 01:50 Sellaistakin voi tapahtua...
	   Esim. urq.mod:ssa on vikassa patternissa isku
	   raskaaseen lautaseen, ja puolen sekunnin perästä
	   siitä tulee J0A samaan kanavaan yhden rivin ajaksi.
	   Valitettavasti MIDI ei tue tätä kuitenkaan.
	   Rumpulautasten yms koko harvemmin
	   kun on lennosta säädettävä...
	*/

	if(disabled)return;
    MPU_Byte(0xE0 + GM_Chans[c]);

	MPU_Byte((Count     ) & 127);
	MPU_Byte((Count >> 7) & 127);

	/* *** Don't try to run this code :-)
		if(Count >= 0x4000)
		{
			fprintf(stderr,
				"Fatal exception [log(14i)] at %04X:%04X\n"
				"Divided buffer overflow, system halted\n", _CS,
				(unsigned short)(&GM_Bend));
			for(Count=1; Count; *((char *)Count++)=Count<<16);
			assert(Count);
		}
	*/
}

void GM_Reset(void)
{
	register int a;
	for(a=0; a<MAXCHN; a++)
	{
		GM_Chans[a] = a;
		GM_KeyOff(a);
		GM_Controller(a, 7, GM_Vol[a]=GM_Volume(127));
		GM_Opa[a] = 255;
	}
    for(a=0; a<16; a++)
    {
        MPU_Byte(0xB0+a); MPU_Byte(0x7B); MPU_Byte(0);   // turn off all notes
        MPU_Byte(0xB0+a); MPU_Byte(0x07); MPU_Byte(127); // set channel volume
        MPU_Byte(0xE0+a); MPU_Byte(0x00); MPU_Byte(0x40);// reset pitch bends
	}
    for(a=0; a<16; a++)
    {
        GM_Notes[a]=0;
		GM_Opa[a]=255;
		GM_Oba[a]=255;
		GM_Vol[a]=255;
    }
	for(a=0; a<MAXCHN; a++)
		GM_Percu[a]=0;

    GM_Vol[16] = 255;
}

void GM_DPatch(int ch, const BYTE *D, BYTE GM, BYTE bank)
{
	D = D;
	GM_Bank(ch, bank);
	GM_Patch(ch, GM);
}
void GM_DNoteOn(int ch, BYTE note, int hz, BYTE vol, BYTE adlvol)
{
	hz = hz;
	adlvol = adlvol;
	GM_KeyOn(ch, note, vol);
}
void GM_DTouch(int ch, BYTE vol, BYTE adlvol)
{
	adlvol = adlvol;
	GM_Touch(ch, vol);
}
void GM_HzTouch(int ch, int hz)
{
    ch = ch;
    hz = hz;
}
void GM_OPLByte(BYTE index, BYTE data)
{
	index = index;
	data = data;
}
void GM_DPan(int ch, SBYTE val)
{
	GM_Controller(ch, 10, (BYTE)(val+128) / 2);
}
