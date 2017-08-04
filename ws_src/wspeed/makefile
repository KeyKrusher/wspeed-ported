commonfilepath=..
!include $(commonfilepath)\makefile.src

all: versio.$$$ wspeed.c wspeed.exe scorecom.exe chmmr.s3m marker.$$$

versio.$$$: versio.inf dovers.exe
  dovers versio.inf
  echo .|time>versio.$$$

omabin: all
# Not copying to path directory

wspeed.c: \omabin\intgen.exe $(DEFdep) \mod\tcsmods\jhmmid61.s3m
  # intgen -b (Official WSpeed musicfile) \mod\tcsmods\jhmadl52.s3m wspeed dah
  intgen -b (Official WSpeed musicfile) \mod\tcsmods\jhmmid61.s3m wspeed dah
  if exist nesmusa.$$$ del nesmusa.$$$

musa02.c: \omabin\intgen.exe $(DEFdep) \mod\tcsmods\harppu.s3m
  # intgen -bn \mod\tcsmods\jhmadl64.s3m musa02 adl_gameover
  # intgen -bn \mod\tcsmods\jhmadl55.s3m musa02 adl_gameover
  intgen -b \mod\tcsmods\harppu.s3m musa02 adl_gameover
  if exist nesmusa.$$$ del nesmusa.$$$

musa03.c: \omabin\intgen.exe $(DEFdep) \mod\tcsmods\gameov2.s3m
  # intgen -b \mod\tcsmods\gameover.s3m musa03 adl_end
  # intgen -b \mod\tcsmods\jhmmid56.s3m musa03 adl_end
  intgen -b \mod\tcsmods\gameov2.s3m musa03 adl_end
  if exist nesmusa.$$$ del nesmusa.$$$

musa04.c: \omabin\intgen.exe $(DEFdep) \mod\tcsmods\jhmmid60.s3m
  intgen -b \mod\tcsmods\jhmmid60.s3m musa04 adl_start
  if exist nesmusa.$$$ del nesmusa.$$$

chmmr.s3m: \mod\fmdrv\chmmr.s3m
  copy /Y \mod\fmdrv\chmmr.s3m .

clean: 
  if exist wspeed.exe del wspeed.exe
  if exist scorecom.exe del scorecom.exe
  if exist wspeed.c del wspeed.c
  if exist musa0?.c del musa0?.c
  if exist *.bak del *.bak
  if exist *.map del *.map
  if exist wspeed.mix del wspeed.mix
  if exist *.o* del *.o*
  if exist *.$$$ del *.$$$
  if exist chmmr.s3m del chmmr.s3m

.cpp.o:
  $(CC) -I$(inc);$(adl) $(CFLAGS) -o$*.o -c $*.cpp

objs = func.o game.o help.o misc.o scores.o key.o \
       video.o wspeed.o musa02.o musa03.o musa04.o

musa02.o: musa02.c
  $(CC) -I$(inc) $(CFLAGS) -o$*.o -c $*.c

musa03.o: musa03.c
  $(CC) -I$(inc) $(CFLAGS) -o$*.o -c $*.c

musa04.o: musa04.c
  $(CC) -I$(inc) $(CFLAGS) -o$*.o -c $*.c

key.cpp: wspeed.h
wspeed.h: scores.h
func.cpp: wspeed.h
game.cpp: wspeed.h musa02.c musa03.c musa04.c
help.cpp: wspeed.h
misc.cpp: wspeed.h $(adl)\cardcs.inc
scores.cpp: wspeed.h
video.cpp: wspeed.h
wspeed.cpp: wspeed.h

score2.o: scores.cpp scores.h
  $(CC) -DMAIN -I$(inc) $(CFLAGS) -o$*.o -c scores.cpp

# echo.exe tarvitaan siksi, koska tlink on p”lj„, eik„ hyv„ksy sit„,
# ett„ kirjastojen lista tiedostossa jakautuu useammalle riville.
# toisaalta commandin echo-komento on niin p”lj„, ettei se tue
# -n -parametri„, joka est„„ rivinvaihdon. Siksi k„ytet„„n
# ulkoista GNU echo-ohjelmaa.
# Kaikenkaikkiaan t„t„ virityst„ tarvitaan siksi, koska DOS on
# niin p”lj„, ettei se tue yli 128 merkki„ pitki„ parametrilistoja...
wspeed.exe: echo.exe $(adl)\music$(MODEL).lib $(objs) $(DEFdep)
  echo -o mtmp -n $(objs) $(runlibs)
  $(LINK) $(runobj),$@,,$(adl)\music$(MODEL).lib @mtmp
  del mtmp
  !if $d(GCC)
  !else
  pakkaa wspeed.exe -eaur
  !endif

scorecom.exe: echo.exe score2.o $(DEFdep)
  $(LINK) $(runobj),$@,,score2.o $(runlibs)
  !if $d(GCC)
  !else
  pakkaa scorecom.exe -eaur
  !endif

wspeed.rec: alku_rec.inf
  if not exist wspeed.rec copy /Y alku_rec.inf wspeed.rec
  touch wspeed.rec

marker.$$$: wspeed.rec scorecom.exe
  scorecom (C) wspeed.rec (C) wspeed.rec
  move /Y wspeed.mix wspeed.rec
  sleep 2
  echo -o marker.$$$
