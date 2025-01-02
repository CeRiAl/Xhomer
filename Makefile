# Makefile for xhomer

# To build xhomer:
#
#   1. make clean
#   2. make
#
# To make a new archive tarball, perform the following steps:
#
#   1. make clean
#   2. make date
#   3. make archive

#######
# The following must be set to Y

PRO = Y

#######
# DGA auto-detection
#
# DGA is used for full-screen modes on XF86 X-servers.  The following
# should auto-detect DGA1 (older systems) or DGA2 (newer systems).
# In case the auto-detection fails, the USE_DGA options immediately
# below may be used to override the detected settings.  This should
# be done by setting one of "USE_DGA1" and "USE_DGA2" to "N" and the
# other to "Y" or by setting both to "N" (most non-Linux systems).

X11DIR = /usr/X11R6

ifeq ($(X11DIR)/include/X11/extensions/xf86dga.h,$(wildcard $(X11DIR)/include/X11/extensions/xf86dga.h))
  ifeq ($(X11DIR)/include/X11/extensions/xf86dga1.h,$(wildcard $(X11DIR)/include/X11/extensions/xf86dga1.h))
    USE_DGA2 = Y
  else
    ifeq ($(X11DIR)/include/X11/extensions/xf86vmode.h,$(wildcard $(X11DIR)/include/X11/extensions/xf86vmode.h))
      USE_DGA1 = Y
    endif
  endif
endif

#######
# Compile in support for DGA1 full-screen support
# (set at most one DGA option to Y)

# USE_DGA1 = Y
# USE_DGA1 = N

#######
# Compile in support for DGA2 full-screen support
# (set at most one DGA option to Y)

# USE_DGA2 = Y
# USE_DGA2 = N

#######
# Use MIT shared memory in X-windows (for better performance)

USE_SHM = Y

#######
# Curses library (for texthomer only)
#
# "texthomer" works better with ncurses.  This is usually the default
# curses library for Linux, so this often has no effect, but if you
# are using another platform, you may need to set this to the paths
# where you have installed ncurses.
NCURSESINC=/apps/gnu/include/ncurses
NCURSESLIB=/apps/gnu/lib
NCURSESLINK=-lcurses -ltermcap
# NCURSESLINK=-lncurses -ltermcap

#######
# Setting this causes the emulated realtime clock to be
# locked to the realtime clock on the host system.
# Unset this only for testing purposes.

USE_RTC = Y

#######
# Provide extra information in the X-window title bar
# (at this point, this adds TLB hit statistics)

# USE_EXTRA_STATUS = Y

#######
# Build id.rom and pro350.rom into executable
#
# If this option is not defined, then xhomer expects to find
# id.rom and pro350.rom in the directory it was run from.

USE_BUILTIN_ROMS = Y

#######
# Do not set the following feature.  it is not fully
# functional.

# USE_LIMIT = Y

################
# PDP11 features

CCDEFS = -DMM_CACHE=8
CCDEFS += -DPERF_MONITOR
# Skip boundary conditions for shift operations in the fp
# code.  (No check for shift by 0 or >=64 if you define this.)
# CCDEFS += -DF_SH_FAST
# CCDEFS += -DTRACE
# CCDEFS += -DIOTRACE

# Set version variable (for later use by make archive)

PRO_VERSION = `cat VERSION`

##########################
# C-Compiler configuration

# All the rest of the flags in this section assume gcc
CC=gcc -Wall
# -Wchar-subscripts -W -Wshadow -Wconversion
# -Wtraditional -Wstrict-prototypes

# This prevents "undesirable excess precision" on some machines, says gcc
CC += -ffloat-store

# The 1st choice runs about 15% slower than the 2nd (-O3 -fomit-frame-pointer).
# but it (re)compiles faster and unlike the second choice, it is debuggable.
# CC += -g -O -fno-inline
CC += -O3 -Winline -fomit-frame-pointer

# Some older gcc's need this on i386 to work around a bug.  As long as
# omit-frame-pointer is also set, it doesn't seem to hurt performance, so
# uncomment it if you think your gcc might have the bug.
# CC += -fno-strength-reduce
# Here are some more options from the man page but didn't seem to help much.
# CC += -finline-functions -ffast-math -fexpensive-optimizations

CCDEFS += -DLOCAL=static -DGLOBAL=extern

# Used by pdp11_rp.c and term_x11.c
CCLINK = -lm

TARGET = $(shell echo `uname -s`-`uname -r | cut -d . -f 1`)

ifeq ($(PRO),Y)
  CCDEFS += -DPRO
  CCLINK += -lX11

  # Some Pro features require extra libraries and defines

  ifeq ($(USE_RTC),Y)
    CCDEFS += -DRTC
  endif

  ifeq ($(USE_LIMIT),Y)
    CCDEFS += -DLIMIT
  endif

  ifeq ($(USE_SHM),Y)
    CCDEFS += -DSHM
    CCLINK += -lXext
  endif

  ifeq ($(USE_DGA1),Y)
    CCDEFS += -DDGA
    CCLINK += -lXext
    CCLINK += -lXxf86dga
    CCLINK += -lXxf86vm
  endif

  ifeq ($(USE_DGA2),Y)
    CCDEFS += -DDGA
    CCDEFS += -DDGA2
    CCLINK += -lXext
    CCLINK += -lXxf86dga
  endif

  ifeq ($(USE_EXTRA_STATUS),Y)
    CCDEFS += -DEXTRA_STATUS
  endif

  ifeq ($(USE_BUILTIN_ROMS),Y)
    CCDEFS += -DBUILTIN_ROMS
  endif

  # Try this as the default place for X11 stuff and ncurses

  CCINCS = -I$(X11DIR)/include -I/usr/X11/include -I$(NCURSESINC)
  CCLIBS = -L$(X11DIR)/lib     -L/usr/X11/lib     -L$(NCURSESLIB)

  # But some vendors put things in non-standard places

  ifeq ($(TARGET), HP-UX-A)
    # HP-UX 9
    CCINCS+=-I/usr/include/X11R5
    CCLIBS+=-L/usr/lib/X11R5
  else
  ifeq ($(TARGET), HP-UX-B)
    # HP-UX 10
    CCINCS+=-I/usr/include/X11R6
    CCLIBS+=-L/usr/lib/X11R6
  else
  ifeq ($(TARGET), SunOS-5)
    CCINCS+=-I/usr/openwin/include
    CCLIBS+=-L/usr/openwin/lib
    CCLINK+=-lsocket -lnsl
  endif
  endif
  endif
endif

ifeq ($(TARGET), SunOS-4)
  CCDEFS+= -DSUNOS -D__USE_FIXED_PROTOTYPES__ -DEXIT_FAILURE=1 -DEXIT_SUCCESS=0
endif

###################
# Files, Rules, etc

SOURCES=$(shell /bin/ls pdp11*.c scp*.c pro*.c)
SOURCES_XHOMER=$(SOURCES) $(shell /bin/ls term*x11.c)
SOURCES_TEXTHOMER=$(SOURCES) term_curses.c
SOURCES_DEPEND=$(SOURCES) $(shell /bin/ls term*x11.c term_curses.c)
OBJECTS=$(SOURCES_XHOMER:%.c=%.o)
OBJECTS_TEXTHOMER=$(SOURCES_TEXTHOMER:%.c=%.o)
PGOBJECTS=$(SOURCES_XHOMER:%.c=%.pg.o)

%.o:	%.c; $(CC) -DINLINE=inline $(CCINCS) $(CCDEFS) -c $*.c -o $@
%.pg.o:	%.c; $(CC) -DINLINE=   -pg $(CCINCS) $(CCDEFS) -c $*.c -o $@

xhomer: $(OBJECTS)
	$(CC) $^ $(CCLIBS) $(CCLINK) -o $@

xhomer.static: $(OBJECTS)
	$(CC) $^ $(CCLIBS) $(CCLINK) -static -ldl -o $@
	strip $@

texthomer: $(OBJECTS_TEXTHOMER)
	$(CC) $^ $(CCLIBS) $(NCURSESLINK) -lm -o $@

texthomer.static: $(OBJECTS_TEXTHOMER)
	$(CC) $^ $(CCLIBS) $(NCURSESLINK) -lm -static -o $@
	strip $@

pdp11: $(OBJECTS)
	$(CC) $^ $(CCLIBS) $(CCLINK) -o $@

pdp11pg: $(PGOBJECTS)
	$(CC) -pg $^ $(CCLIBS) $(CCLINK) -o $@

saveit: pdp11
	cp pdp11 pdp11-$(TARGET)
	strip pdp11-$(TARGET)

date:
	./make_version
	@echo -n "#define PRO_VERSION \"" > pro_version.h
	@echo -n `cat VERSION` >> pro_version.h
	@echo "\"" >> pro_version.h

archive:
	./make_archive

clean:
	-@rm -f *.o *~ xhomer xhomer.static texthomer texthomer.static convert_roms

dep depend:
	@egrep -e "# Dependencies" Makefile > /dev/null || \
	echo "# Dependencies" >> Makefile
	sed '/^\#\ Dependencies/q' < Makefile > Makefile.New
	$(CC) -MM $(CCINCS) $(CCDEFS) $(SOURCES_DEPEND) >> Makefile.New
	mv Makefile Makefile.bak
	mv Makefile.New Makefile

##############
# Dependencies
pdp11_cpu.o: pdp11_cpu.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pdp11_fp.o: pdp11_fp.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pdp11_sys.o: pdp11_sys.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_2661kb.o: pro_2661kb.c pdp11_defs.h sim_defs.h pro_defs.h \
  pro_version.h pro_config.h
pro_2661ptr.o: pro_2661ptr.c pdp11_defs.h sim_defs.h pro_defs.h \
  pro_version.h pro_config.h
pro_clk.o: pro_clk.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_com.o: pro_com.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_digipad.o: pro_digipad.c pdp11_defs.h sim_defs.h pro_defs.h \
  pro_version.h pro_config.h
pro_eq.o: pro_eq.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_init.o: pro_init.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_int.o: pro_int.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_kb.o: pro_kb.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_la50.o: pro_la50.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_lk201.o: pro_lk201.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h pro_lk201.h
pro_maint.o: pro_maint.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_mem.o: pro_mem.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_menu.o: pro_menu.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h pro_lk201.h
pro_null.o: pro_null.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_ptr.o: pro_ptr.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_rd.o: pro_rd.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_reg.o: pro_reg.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h id.h pro350.h
pro_rx.o: pro_rx.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
pro_serial.o: pro_serial.c pdp11_defs.h sim_defs.h pro_defs.h \
  pro_version.h pro_config.h
pro_vid.o: pro_vid.c pdp11_defs.h sim_defs.h pro_defs.h pro_version.h \
  pro_config.h
scp.o: scp.c sim_defs.h pro_defs.h pro_version.h pro_config.h
scp_tty.o: scp_tty.c sim_defs.h
term_curses.o: term_curses.c pro_defs.h pro_version.h pro_config.h \
  pro_lk201.h term_fonthash_curses.c
term_overlay_x11.o: term_overlay_x11.c pro_defs.h pro_version.h \
  pro_config.h pro_font.h
term_x11.o: term_x11.c \
  pro_defs.h pro_version.h \
  pro_config.h pro_lk201.h
