#GCC=1

MODEL=c

commonfilepath=..
!include $(commonfilepath)\makefile.src

all: intgen.exe

omabin: \omabin\intgen.exe

clean:
  del intgen.exe
  del intgen.obj

!if $d(GCC)

\omabin\intgen.exe: intgen.exe
  strip intgen.exe
  strip intgen.exe
  copy /Y intgen.exe \omabin

intgen.obj: intgen.c $(adl)\music.h $(DEFdep)
  $(CC) $(CFLAGS) -I$(adl) -c -o$@ $*.c

intgen.exe: intgen.obj $(adl)\music.lib $(DEFdep)
  $(LINK) $*.obj $(adl)\music.lib $(LFLAGS) -o$@ 

!else

\omabin\intgen.exe: intgen.exe
  pakkaa intgen -eauro \omabin

intgen.obj: intgen.c $(adl)\music.h $(DEFdep)
  $(CC) -I$(inc);$(adl) $(CFLAGS) -c $*.c

intgen.exe: intgen.obj $(adl)\music$(MODEL).lib $(DEFdep)
  $(LINK) $(runobj),$@,,$*.obj $(adl)\music$(MODEL).lib $(runlibs)

!endif
