#include "m_mpu.h"
#include "m_midi.h"
#include "m_sbenv.h"
#include "io.h"

#include <unistd.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <stdlib.h>

static int seqfd;
static int mididevno = 1;

SEQ_DEFINEBUF(128);

void seqbuf_dump(void)
{
	if(_seqbufptr && seqfd>0)
	{
		write(seqfd, _seqbuf, _seqbufptr);
		_seqbufptr = 0;
	}                            			
}
static void WRITER_Byte(BYTE data)
{
	if(seqfd < 0)return;
	SEQ_MIDIOUT(mididevno, data);
	SEQ_DUMPBUF();
}

#endif

static int WRITER_Detect(void)
{
	const char *s = getenv("MIDEV");
	if(s)mididevno = atoi(s);

	seqfd = open("/dev/sequencer", O_WRONLY, 0);
	if(seqfd < 0)seqfd = open("/dev/sound/sequencer", O_WRONLY, 0);
	if(seqfd < 0)return -1;
	return 0;
}

static void WRITER_Reset(void)
{
    GM_Reset();
}

static void WRITER_Close(void)
{
	if(seqfd > 0)close(seqfd);
}

static void WRITER_Tick(unsigned num)
{
}

struct MusDriver WRITERDriver =
{
	"Any OSS compatible MIDI device",
	
	"This driver supports at least my AWE32.\n"
	"To select the midi device, use shell variable:\n"
	"   export MIDEV=0\n"
	"Default: 1\n",
	
	'w', /* optchar */
	
	WRITER_Detect,
	WRITER_Reset,
	WRITER_Close,
	
	WRITER_Byte,
	GM_OPLByte,
	
	GM_Controller,
	GM_DPan,
	GM_Bend,
	GM_KeyOff,
	GM_DPatch,
	GM_DNoteOn,
	GM_DTouch,
	GM_HzTouch,
	
	WRITER_Tick
};
