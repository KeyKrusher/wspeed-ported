#include <stdio.h>
#include <alloc.h> /* farmalloc() */
#include <string.h>

#define SBKLIB

#include "music.h"
#include "m_mpu.h"

#ifdef __GNUC__
 #include <allegro.h>
#else
 #if SUPPORT_AWE
  #include "ctaweapi.h"
  static SOUND_PACKET spSound     = {0};
  static long BankSize            = 0;
  #ifndef SBKLIB
  static char Packet[PACKETSIZE]  = {0};
  static char huge *Preset = NULL;
  #endif
 #endif
#endif

#if SUPPORT_AWE
unsigned short AWEBase;

#ifndef ALLEGRO_VERSION
static void SendMidi(byte *MidiBuf)
{
    switch (MidiBuf[0] >> 4)
    {
        case 0x8:
            awe32NoteOff((word) MidiBuf[0]&15, MidiBuf[1], MidiBuf[2]);
            break;
        case 0x9:
            awe32NoteOn((word) MidiBuf[0]&15, MidiBuf[1], MidiBuf[2]);
            break;
        case 0xA:
            awe32PolyKeyPressure((word) MidiBuf[0]&15, MidiBuf[1], MidiBuf[2]);
            break;
        case 0xB:
            awe32Controller((word) MidiBuf[0]&15, MidiBuf[1], MidiBuf[2]);
            break;
        case 0xC:
            awe32ProgramChange((word) MidiBuf[0]&15, MidiBuf[1]);
            break;
        case 0xD:
            awe32ChannelPressure((word) MidiBuf[0]&15, MidiBuf[1]);
            break;
        default:
            awe32PitchBend((word) MidiBuf[0]&15, MidiBuf[1], MidiBuf[2]);
            break;
    }
}

#endif
static void ProcessMidi(byte a)
{
    #if SUPPORT_AWE
	#ifdef ALLEGRO_VERSION
	if(!IsAWEThere)return;
	midi_awe32.raw_midi(a);
	#else
	static byte MidiMsgLen[]  = {0,0,0,0,0,0,0,0, 3,3,3,3,2,2,3, 0};
	static byte MidiRecv      = 0;     /* Number of MIDI bytes received */
	static byte MidiBuf[8];            /* MIDI read buffer */

	if(!IsAWEThere)return;

	if(!MidiRecv && !MidiMsgLen[a >> 4])return;
    MidiBuf[MidiRecv++] = a;

    if(MidiRecv == MidiMsgLen[MidiBuf[0] >> 4])
    {
        SendMidi(MidiBuf);
        MidiRecv = 0;
    }
    #endif
    #endif
}
#endif

/************************************************/

int AWE_Found(void)
{
#if SUPPORT_AWE
 #ifndef ALLEGRO_VERSION
  #ifndef SBKLIB
    FILE *fp;
    long l, l2;
    int i;
  #endif
 #endif
	MPU_Byte = ProcessMidi;
 #ifdef ALLEGRO_VERSION
	return 0;
 #else // Not Allegro
    if(sbenv.set)
    {
        AWEBase = 0;
        if(!sbenv.E)
            sprintf(SoundError, "BLASTER environment variable has no %c section", 'E');
        else
        {
            AWEBase = sbenv.E;
            sprintf(SoundError, "AWE32/AWE64 not detected at 0x%04X", AWEBase);
        }
    }
    else
        strcpy(SoundError, "BLASTER environment variable not set");

    IsAWEThere = 0;
    if(awe32Detect(AWEBase))return 0;
    if(awe32InitHardware())
    {
        strcpy(SoundError, "AWE32/AWE64 initialization failed");
        return 0;
    }

  #ifdef SBKLIB
    awe32TotalPatchRam(&spSound);
    spSound.bank_no = 0;                    /* load as Bank 0 */
    spSound.total_banks = 1;                /* use 1 bank first */
    /* BankSize = 0;     */                 /* ram is not needed */
    spSound.banksizes = &BankSize;
    awe32DefineBankSizes(&spSound);
    awe32SoundPad.SPad1 = awe32SPad1Obj;
    awe32SoundPad.SPad2 = awe32SPad2Obj;
    awe32SoundPad.SPad3 = awe32SPad3Obj;
	awe32SoundPad.SPad4 = awe32SPad4Obj;
    awe32SoundPad.SPad5 = awe32SPad5Obj;
    awe32SoundPad.SPad6 = awe32SPad6Obj;
    awe32SoundPad.SPad7 = awe32SPad7Obj;
  #else /* not SBKLIB */

    fp = fopen("c:\\sb16\\2gmgsmt.sf2", "rb");
//  fp = fopen("h:synthgm.sf2", "rb");

    /* Allocate ram */
    awe32TotalPatchRam(&spSound);
    spSound.bank_no = 0;                        /* load as Bank 0 */
    spSound.total_banks = 1;                    /* use 1 bank first */
    BankSize = spSound.total_patch_ram;         /* use all available ram */
    spSound.banksizes = &BankSize;
    awe32DefineBankSizes(&spSound);

    /* request to load */
    spSound.data = Packet;
    fread(Packet, 1, PACKETSIZE, fp);
    if((i=awe32SFontLoadRequest(&spSound)) != 0)
    {
        fclose(fp);
        sprintf(SoundError, "Cannot load SF2 file (%d).", i);
        return 0;
    }

    /* stream samples */
    fseek(fp, spSound.sample_seek, SEEK_SET);
    for(i=0; i<spSound.no_sample_packets; i++)
    {
        fread(Packet, 1, PACKETSIZE, fp);
        awe32StreamSample(&spSound);
    }

	/* setup SoundFont preset objects */
	fseek(fp, spSound.preset_seek, SEEK_SET);
    printf("preset size=%ld\n", spSound.preset_read_size);

    l = spSound.preset_read_size;
    Preset = (char huge *)farmalloc(l);
    if(!Preset)
    {
        printf("Out of memory!\n");
        return 0;
    }

    for(l2=0; l; )
	{
        size_t tmp;
        printf("left=%ld, index=%ld, ", l, l2);
        if((tmp = (size_t)-2) > l)tmp = (size_t)l;
        tmp = fread(Preset+l2, 1, tmp, fp);
        printf("read %u\n", tmp);
		l -= tmp;
		l2 += tmp;
	}
    fclose(fp);
    printf("setting presets\n");
    spSound.presets = (char _FAR_ *)Preset;
    if((i=awe32SetPresets(&spSound)) != 0)
	{
		fclose(fp);
		sprintf(SoundError, "Invalid SF2 file (%d).", i);
        return 0;
    }
    printf("done init\n");

    #if 0    
    /* calculate actual ram used */
    if(spSound.no_sample_packets)
    {
        BankSize = spSound.preset_seek - spSound.sample_seek + 160;
        spSound.total_patch_ram -= BankSize;
    }
    else
        BankSize = 0;          /* no sample in SBK file */
    #endif

  #endif /* SBKLIB */

    awe32InitMIDI();
 #endif /* Not Allegro */
    IsAWEThere=1;
    IsAWEThere=AWE_Reset();
    if(IsAWEThere)
        sprintf(SoundError, "AWE32/AWE64 was detected at 0x%04X", AWEBase);
    return IsAWEThere;
#else   /* AWE not supported */
    return 0;
#endif
}

int AWE_Reset(void)
{
#if SUPPORT_AWE
 #ifdef ALLEGRO_VERSION
    return IsAWEThere;
 #else
    int a;
    if(!IsAWEThere)return 0;
    MPU_Byte = ProcessMidi;
	for(a=0; a<16; a++)
    {
        MPU_Byte(0xB0+a); MPU_Byte(0x07); MPU_Byte(127); // set channel volume
        MPU_Byte(0xB0+a); MPU_Byte(0x7B); MPU_Byte(0);   // turn off all notes
        MPU_Byte(0xE0+a); MPU_Byte(0x00); MPU_Byte(0x40);// reset pitch bends
    }
    return 1;
 #endif
#else
    strcpy(SoundError, "AWE support was not compiled in.");
    return 0;
#endif
}

void AWE_Done(void)
{
    /* free allocated memory */
    #if SUPPORT_AWE
    if(!IsAWEThere)return;
    #ifndef ALLEGRO_VERSION
    awe32ReleaseAllBanks(&spSound);
    awe32Terminate();
    #endif
    #endif
}
