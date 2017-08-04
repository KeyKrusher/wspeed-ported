commonfilepath=..
!include $(commonfilepath)\makefile.src

all: midins.exe midspl.dat

omabin: \omabin\midins.exe \omabin\which.exe \omabin\midspl.dat

\omabin\midins.exe: midins.exe
  pakkaa midins -eauro $@

\omabin\midspl.dat: midspl.dat
  if exist midsptmp.exe del midsptmp.exe
  ren midspl.dat midsptmp.exe
  pakkaa midsptmp.exe -eauro $@
  ren midsptmp.exe midspl.dat

\omabin\which.exe: which.exe
  pakkaa which -auro $@

clean:
  del midins.exe
  del midins.asm
  del which.exe
  del which.asm
  del *.obj
  del *.map

midins.obj: midins.c $(adl)\music.h $(DEFdep)
  !if $d(GCC)
  $(CC) $(CFLAGS) -I$(adl) -c -o$@ $*.c
  !else
  $(CC) -S -I$(inc);$(adl) $(CFLAGS) -c $*.c
  tasm -t -ml -w2 -w-res -m9 $*.asm
  del $*.asm
  !endif

midspl.obj: midins.c $(adl)\music.h $(DEFdep)
  !if $d(GCC)
  $(CC) $(CFLAGS) -I$(adl) -c -o$@ midins.c
  !else
  $(CC) -S -DPLAYSELF=argv[0] -I$(inc) -I$(adl) $(CFLAGS) -c -o$*.asm midins.c
  tasm -t -ml -w2 -w-res -m9 $*.asm
  del $*.asm
  del mtmp
  !endif

which.obj: which.c $(DEFdep)
  !if $d(GCC)
  $(CC) $(CFLAGS) -I$(adl) -c -o$@ $*.c
  !else
  $(COMSPEC) /C echo -I$(inc);$(adl) $(CFLAGS) >mtmp
  $(CC) -S @mtmp -c $*.c
  tasm -t -ml -w2 -w-res -m9 $*.asm
  del $*.asm
  del mtmp
  !endif

midins.exe: midins.obj $(adl)\music$(MODEL).lib $(DEFdep)
  !if $d(GCC)
  $(LINK) $*.obj $(adl)\music.lib $(LFLAGS) -o$@
  !else
  $(LINK) $(runobj),$@,,$*.obj $(adl)\music$(MODEL).lib $(runlibs)
  !endif

midspl.dat: midspl.obj $(adl)\music$(MODEL).lib $(DEFdep)
  !if $d(GCC)
  $(LINK) $*.obj $(adl)\music.lib $(LFLAGS) -o$@
  !else
  $(LINK) $(runobj),$@,,$*.obj $(adl)\music$(MODEL).lib $(runlibs)
  !endif

which.exe: which.obj $(DEFdep)
  !if $d(GCC)
  $(LINK) $*.obj $(LFLAGS) -o$@
  !else
  $(LINK) $(runobj),$@,,$*.obj $(runlibs)
  !endif
