VERSION=3.0.4

CC=gcc

LDLIBS=-lm
# Use -lalleg for DJGPP platform

CPPFLAGS=-Wall -W -g -pipe -Wtraditional -Wundef -Wbad-function-cast \
    -Wcast-qual -Wcast-align -Wwrite-strings -Wstrict-prototypes \
    -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
    -Wnested-externs -Winline -DVERSION=\"$(VERSION)\"

# Add USETHREADS=1 if you want to use pthreads
# implementation instead of timer implementation
# on Linux. You need to add -lpthread to LDLIBS
# then too. Timer implementation is recommended,
# so don't do anything.

CFLAGS=-g
LDFLAGS=-g

ARCHDIR=archives/
ARCHNAME=mids3mtools-$(VERSION)
ARCHFILES=cons.c fmdrv.c music.c m_opl.c m_mpu.c m_awe.c m_sbenv.c \
          cons.h fmdrv.h music.h m_opl.h m_mpu.h m_awe.h m_sbenv.h \
          m_midi.c m_sr.c m_path.c m_irq.c m_arch.c m_dsp.c \
          m_midi.h m_sr.h m_path.h m_irq.h m_arch.h m_dsp.h \
          intgen.c sizes.h m_struct.h adldata.c adldata.h io.h \
          findbest.c midins.c cardcs.inc devopts.inc testi.c \
          hspc_mid.s3m megamix.mid NEWS \
          awemods.tar.gz

all: musiclib.a intgen fmdrv midins testi findbest

testi: testi.o musiclib.a hspc.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

findbest: findbest.o	
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

intgen: intgen.o argh.o musiclib.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

fmdrv: fmdrv.o musiclib.a cons.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

midins: midins.o musiclib.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

musiclib.a: music.o m_arch.o m_dsp.o m_irq.o m_mpu.o m_opl.o m_path.o m_sbenv.o m_sr.o m_midi.o adldata.o
	ar -rc $@ $^

clean:
	rm -f testi midins intgen musiclib.a *.o fmdrv
	
distclean: clean
	rm -f core *~

include depfun.mak

demo: demoplayer megamix.s3m

demoplayer: testi
	ln -f testi demoplayer

hspc.o: hspc.c
hspc.c: hspc_mid.s3m intgen
	./intgen hspc_mid.s3m hspc hspc

megamix.s3m: megamix.mid fmdrv
	./fmdrv -q megamix.mid megamix.s3m

#awemods.tar.gz: c/* m/* s/* l/*
#	tar cf - $^|gzip -9 >$@
