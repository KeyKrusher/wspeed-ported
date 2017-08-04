#GCC=1

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
  copy /Y intgen.exe \omabin

# FIXME!
intgen.exe: intgen.c $(adl)\adlib.h $(DEFdep)
  $(CC) $(CFLAGS) -I$(adl) -c -o$*.obj $*.c $(runobj)
  $(LINK) $(LDFLAGS) -o$@ $*.obj $(runlibs)

!else

\omabin\intgen.exe: intgen.exe
  pakkaa intgen -auro \omabin

intgen.obj: intgen.c $(adl)\adlib.h $(DEFdep)
  $(COMSPEC) /C echo -I$(inc);$(adl) $(CFLAGS) >mtmp
  $(CC) @mtmp -c $*.c
  del mtmp

intgen.exe: intgen.obj $(adl)\adlib.lib $(DEFdep)
  $(LINK) $(runobj),$@,,$*.obj $(adl)\adlib.lib $(runlibs)

!endif
