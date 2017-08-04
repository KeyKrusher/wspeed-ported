omabin: all

clean:
  if exist m?.obj del m?.obj
  if exist m?_*.obj del m?_*.obj
  if exist *.bak del *.bak
  if exist music?.lib del music?.lib

!if !$d(MODEL) && !$d(GCC)
all: c l t
 # Specify here all the models you need to build.

l:
 @$(MAKE) -DMODEL=l -DBUILD=l

c:
 @$(MAKE) -DMODEL=c -DBUILD=c

t:
 @$(MAKE) -DMODEL=t -DBUILD=t -DDEFS="-p -DSUPPORT_MIDI=0 -DVIBRATO=0"

!else

commonfilepath=..
!include $(commonfilepath)\makefile.src

!if $d(GCC)
all: music.lib
!else
all: music$(BUILD).lib
!endif

m$(BUILD).obj: music.c music.h m_awe.h m_opl.h m_mpu.h $(DEFdep)
  $(CC) -I$(inc) $(CFLAGS) $(DEFS) -c -o$@ music.c
                           
m$(BUILD)_sr.obj: m_sr.c music.h m_mpu.h $(DEFdep)
  $(CC) -I$(inc) $(CFLAGS) $(DEFS) -c -o$@ m_sr.c

m$(BUILD)_arch.obj: m_arch.c music.h $(DEFdep)
  $(CC) -I$(inc) $(CFLAGS) $(DEFS) -c -o$@ m_arch.c

m$(BUILD)_path.obj: m_path.c $(DEFdep)
  $(CC) -I$(inc) $(CFLAGS) $(DEFS) -c -o$@ m_path.c

m$(BUILD)_mpu.obj: m_mpu.c music.h m_mpu.h $(DEFdep)
  $(CC) -I$(inc) $(CFLAGS) $(DEFS) -c -o$@ m_mpu.c

m$(BUILD)_opl.obj: m_opl.c music.h m_opl.h $(DEFdep)
  $(CC) -I$(inc) $(CFLAGS) $(DEFS) -c -o$@ m_opl.c

!if !$d(GCC)
m$(BUILD)_awe.obj: m_awe.c music.h m_awe.h m_mpu.h ctaweapi.h $(DEFdep)
  $(CC) -I$(inc) $(CFLAGS) $(DEFS) -c -o$@ m_awe.c
!endif

!if $d(GCC)

music.lib: m$(BUILD).obj m$(BUILD)_opl.obj m$(BUILD)_mpu.obj m$(BUILD)_sr.obj m$(BUILD)_arch.obj m$(BUILD)_path.obj $(DEFdep)
  if exist $@ del $@
  $(LIB) $@ m$(BUILD).obj m$(BUILD)_opl.obj m$(BUILD)_mpu.obj m$(BUILD)_sr.obj m$(BUILD)_arch.obj m$(BUILD)_path.obj

!else

rawe32t.c:
  echo. >rawe32t.c
rawe32t.lib: rawe32t.c
  $(CC) $(CFLAGS) $(DEFS) -c -o$*.obj rawe32t.c
  if exist $@ del $@
  $(LIB) $@ +$*.obj;

music$(BUILD).lib: m$(BUILD).obj m$(BUILD)_opl.obj m$(BUILD)_mpu.obj m$(BUILD)_awe.obj m$(BUILD)_sr.obj m$(BUILD)_arch.obj m$(BUILD)_path.obj rawe32$(MODEL).lib $(DEFdep)
  if exist $@ del $@
  $(LIB) $@ +m$(BUILD) +m$(BUILD)_opl +m$(BUILD)_mpu +m$(BUILD)_awe +m$(BUILD)_sr +m$(BUILD)_arch +m$(BUILD)_path +rawe32$(MODEL).lib;

!endif

!endif
