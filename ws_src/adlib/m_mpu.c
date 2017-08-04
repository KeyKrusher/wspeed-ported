#ifndef unixversion
 #include <stdio.h>
 #include <string.h>
 #include <dos.h>
#endif

unsigned char GMVol[64] =
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

#ifndef unixversion

#include "music.h"
#include "m_mpu.h"
#include "m_awe.h"

/* GM_* -routines belong to AWE also */

#if SUPPORT_MIDI
void MPUByte(byte data);
#if SUPPORT_DSP
static enum { MPU401, SBMIDI } type;
#endif
MidiHandler MPU_Byte = MPUByte; /* BY DEFAULT */
word MPUBase;
#endif

void MPUByte(byte data)
{
#if SUPPORT_MIDI
#ifdef ALLEGRO_VERSION
    if(!IsMPUThere)return;
    midi_mpu401.raw_midi(data);
#else
	if(!IsMPUThere)return;

/*  printf("%02X ", data); */

#if SUPPORT_DSP
    if(type == MPU401)
#endif
    {
        register int a;
        /* check write status */
        for(a=0; (inportb(MPUBase + 1) & MPU401_OK2WR); a++)if(a>1000)break;
        outportb(MPUBase, data); // offer timeout instead of lockup
    }
#if SUPPORT_DSP
    else
    {
        /* Wait for write buffer ready */
        while(inportb(sbenv.A + DSP_WR_ST) & 0x80);
        /* Send MIDI write command */
        outportb(sbenv.A + DSP_WR, MIDI_OUT_P);
        /* Wait for write buffer ready */
        while(inportb(sbenv.A + DSP_WR_ST) & 0x80);
        /* Send MIDI data byte */
        outportb(sbenv.A + DSP_WR, data);
    }
#endif
#endif
#else
    data=data;
#endif
}

#if SUPPORT_MIDI
static void MPU_Cmd(byte cmd)
{
	register int a;
	/* check write status */
	for(a=0; inportb(MPUBase + 1) & MPU401_OK2WR; a++)if(a>100)break;
	outportb(MPUBase + 1, cmd);
}
/************************************************************************
 * READ_MPU401_DATA: reads a byte from the MPU401 port.
 * Input: none
 * Output: data byte
 ************************************************************************/
static byte Read_MPU401_Data(void)
{
    register int a;
    // check read status
    for(a=0; (inp(MPUBase + 1) & MPU401_OK2RD); a++)if(a>100)break;
    return(inp(MPUBase));
}
#endif

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

About the controllers... In AWE32 they are:
    0=Bank select               7=Master Volume     11=Expression(volume?)
    1=Modulation Wheel(Vibrato)10=Pan Position      64=Sustain Pedal
    6=Data Entry MSB           38=Data Entry LSB    91=Effects Depth(Reverb)
  120=All Sound Off           123=All Notes Off     93=Chorus Depth
  100=reg'd param # LSB       101=reg'd param # LSB
   98=!reg'd param # LSB       99=!reg'd param # LSB

    1=Vibrato, 121=reset vibrato,bend
*/

#endif /* unixversion */

#if SUPPORT_MIDI
static byte GM_Percu[MAXCHN]={0};
static byte GM_Notes[16]={0};
static byte GM_Chans[16]={0};
static byte GM_Vol[17];
static byte GM_Opa[16]; /* For traffic optimization (patch) */
static byte GM_Oba[16]; /* For traffic optimization (bank) */

byte GM_Volume(byte Vol) // Converts the volume
{
	if(MusData.LinearMidiVol)return Vol>=63?127:127*Vol/63;
	return (GMVol[Vol>63?63:Vol] * MusData.GlobalVolume) >> 6;
	/* Prevent overflows */
}
void GM_Controller(int c, byte i, byte d)
{
	MPU_Byte(0xB0+GM_Chans[c]);
	MPU_Byte(i);
	MPU_Byte(d);
}

void GM_Patch(int c, byte p)
{
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

void GM_Bank(int c, byte b)
{
	int d;
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

void GM_Touch(int c, byte Vol)
{
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

void GM_KeyOn(int c, byte key, byte Vol)
{
//  if(c>1)return;
	key += 12;
	//Korkeus 12, niin kuulostaa fmdrv:ll„ konvertoidut
	//t„sm„lleen samoilta mpu:lla kuin opl:lla korkeuden
	//suhteen.

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
}

void GM_KeyOff(int c)
{
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

void GM_Bend(int c, word Count)
{
	/* Toivottavasti joku ei yrit„ bendata hihattia tms :-)
	   3.10.1998 01:50 Sellaistakin voi tapahtua...
	   Esim. urq.mod:ssa on vikassa patternissa isku
	   raskaaseen lautaseen, ja puolen sekunnin per„st„
	   siit„ tulee J0A samaan kanavaan yhden rivin ajaksi.
	   Valitettavasti MIDI ei tue t„t„ kuitenkaan.
	   Rumpulautasten yms koko harvemmin
	   kun on lennosta s„„dett„v„...
	*/

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
}
#endif /* Support midi */

#ifndef unixversion

#if SUPPORT_MIDI
/************************************************************************
 * RESET_MPU401: resets the MPU401 port.
 * Input: none
 * Output: result
 * note: Error will occur if UART mode is not turned off first.
 ************************************************************************/
static int Reset_MPU401(void)
{
	int i = 100;
	MPU_Cmd(MPU401_RESET);       // send reset command
	while((i > 0) && (Read_MPU401_Data() != MPU401_CMDOK))i--;
	return i;                    // offer timeout instead of lockup
}
/************************************************************************
 * SET_UART_MODE: puts the MPU401 port in UART mode.
 * Input: state (Reset will return error if UART mode is on when reset occurs)
 * Output: result
 ************************************************************************/
static int Set_UART_Mode(int state)
{
	int i = 100;
	if(state)
	{
		MPU_Cmd(MPU401_UART);    // turn on UART mode
		while((i > 0) && (Read_MPU401_Data() != MPU401_CMDOK))i--;
	}                                     // offer timeout instead of lockup
	else
		MPU_Cmd(MPU401_RESET); // turn off UART mode
	return i;
}

/************************************************************************
 * INIT_MIDI: Initializes the MIDI port.  If MPU-401 port then reset
 *            the port and select UART mode.  For either MPU-401 or
 *            SBMIDI ports set all master volumes to 100, turn off all
 *            notes, and reset all pitch bends.
 *
 * Input: none
 * Output: result
 ************************************************************************/
static int Init_MIDI(void)
{
#if SUPPORT_DSP
	if(type == MPU401)
	{
#endif
		if(!Reset_MPU401())   // reset port
		{
			strcpy(SoundError, "Reset_MPU401() failed");
			return 1;
		}
		if(!Set_UART_Mode(1)) // set UART mode
		{
			strcpy(SoundError, "Could not enter UART mode");
			return 1;
		}
#if SUPPORT_DSP
	}
#endif
	return 0;
}

int MPU_Found(void)
{
#ifdef ALLEGRO_VERSION
	IsMPUThere=midi_mpu401.detect(FALSE);

	if(IsMPUThere)
		strcpy(SoundError, midi_mpu401.desc);
	else
		strcpy(SoundError, allegro_error);
#else
	if(sbenv.set)
	{
		MPUBase = 0;
        if(!sbenv.P)
			sprintf(SoundError, "BLASTER environment variable has no %c section", 'P');
		else
		{
			MPUBase = sbenv.P;
			sprintf(SoundError, "MPU-401 not detected at 0x%04X", MPUBase);
		}
	}
	else
		strcpy(SoundError, "BLASTER environment variable not set");

#if SUPPORT_DSP
	type=MPU401;
#endif

	IsMPUThere = inportb(MPUBase+1);
	if(IsMPUThere & MPU401_OK2WR)
		IsMPUThere = 0;
	else
	{
		Set_UART_Mode(0);                       // just in case it was left on
		IsMPUThere = Reset_MPU401()>0; // does reset work?
	}
	MPU_Reset();
#endif

	if(IsMPUThere)
		sprintf(SoundError, "MPU-401 was detected at 0x%04X", MPUBase);
	return IsMPUThere;
}

#if SUPPORT_DSP
/************************************************************************
 * DSPReset: Resets the DSP on the Sound Blaster card.
 * Input:  none
 * Output: result
 ************************************************************************/
int DSPReset(void)
{
    int i, j, result=1;  
    outportb(sbenv.A + DSP_RST, 1);
    for(i=0; i<100; i++);   /* delay more than 3us */
    outportb(sbenv.A + DSP_RST, 0);  
    for(i = 0; (i < 32) && (result != DSP_RST_OK); i++)
    {
        for(j=0; (j < 512) && (inportb(sbenv.A + DSP_RD_ST) < 0x80); j++);
        if(j < 512)result = inportb(sbenv.A + DSP_RD);
    }
    if(result == DSP_RST_OK)result = 0;  
    return result;
}
int DSP_Found(void)
{
#ifdef ALLEGRO_VERSION
    IsDSPThere=midi_mpu401.detect(FALSE);
    if(IsDSPThere)
        strcpy(SoundError, midi_mpu401.desc);
    else
        strcpy(SoundError, allegro_error);
#else
    if(sbenv.set)
    {
        MPUBase = 0;
		if(!sbenv.A)
            sprintf(SoundError, "BLASTER environment variable has no %c section", 'A');
        else
        {
			MPUBase = sbenv.A;
            sprintf(SoundError, "MIDI not detected at 0x%04X", MPUBase);
        }
    }
    else
        strcpy(SoundError, "BLASTER environment variable not set");

    type=SBMIDI;

    IsDSPThere = !DSPReset();
    DSP_Reset();
#endif
    if(IsDSPThere)
        sprintf(SoundError, "DSP MIDI was detected at 0x%04X", MPUBase);
    return IsDSPThere;
}
int DSP_Reset(void)
{
    int a;
    register int b;
    if(!IsDSPThere)return 0;
    a = IsMPUThere;
    IsMPUThere=1;
    b = MPU_Reset();
    IsMPUThere=a;
    return b;
}
#endif
int MPU_Reset(void)
{
    int a;
    MPU_Byte = MPUByte;
    if(!(IsMPUThere && Set_UART_Mode(1)))return 0;
    Init_MIDI();
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

	return 1;
}
#endif

#endif /* unixversion */
