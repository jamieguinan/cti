default: default1

include ../platforms.make

MAIN=main.o

PKGCONFIGDIR ?= /usr/lib/pkgconfig

HOSTARCH=$(shell uname -m)-$(shell uname -s)

ifeq ($(ARCH),$(HOSTARCH))
LDFLAGS+=-Wl,-rpath,$(shell pwd)/../../platform/$(ARCH)/lib
endif

CFLAGS += -Os -Wall $(CMDLINE_CFLAGS)
#CFLAGS += -Werror
#CFLAGS += -O0 -ggdb
#CFLAGS += -Os -ggdb
ifneq ($(ARCH),armeb)
ifneq ($(ARCH),lpd)
CFLAGS += -Wno-unused-result
endif
endif
# -std=c99 
CPPFLAGS += -I../../platform/$(ARCH)/include -I../jpeg-7
CPPFLAGS += -MMD -MP -MF $(OBJDIR)/$(subst .c,.dep,$<)
LDFLAGS += -L../../platform/$(ARCH)/lib -ljpeg -lpthread
ifeq ($(OS),Linux)
LDFLAGS += -ldl -lrt
endif

#INSTRUMENT = -finstrument-functions
CPPFLAGS += -DUSE_STACKDEBUG

# "-static" is a problem for alsa, and other things...

OBJDIR ?= .

default1:  $(OBJDIR)/cti$(EXEEXT)

#	@echo wd=$(shell pwd)
#	@echo VPATH=$(VPATH)

# Another app.
OBJS= \
	$(OBJDIR)/CTI.o \
	$(OBJDIR)/locks.o \
	$(OBJDIR)/Mem.o \
	$(OBJDIR)/Index.o \
	$(OBJDIR)/Cfg.o \
	$(OBJDIR)/Log.o \
	$(OBJDIR)/Array.o \
	$(OBJDIR)/Range.o \
	$(OBJDIR)/File.o \
	$(OBJDIR)/jmemsrc.o \
	$(OBJDIR)/jmemdst.o \
	$(OBJDIR)/jpeghufftables.o \
	$(OBJDIR)/wrmem.o \
	$(OBJDIR)/CJpeg.o \
	$(OBJDIR)/DJpeg.o \
	$(OBJDIR)/DO511.o \
	$(OBJDIR)/ov511_decomp.o \
	$(OBJDIR)/Images.o \
	$(OBJDIR)/JpegFiler.o \
	$(OBJDIR)/JpegTran.o \
	$(OBJDIR)/PGMFiler.o \
	$(OBJDIR)/MotionDetect.o \
	$(OBJDIR)/MjpegMux.o \
	$(OBJDIR)/MjpegDemux.o \
	$(OBJDIR)/SourceSink.o \
	$(OBJDIR)/String.o \
	$(OBJDIR)/CSV.o \
	$(OBJDIR)/ScriptV00.o \
	$(OBJDIR)/Wav.o \
	$(OBJDIR)/Audio.o \
	$(OBJDIR)/Numbers.o \
	$(OBJDIR)/Null.o \
	$(OBJDIR)/Control.o \
	$(OBJDIR)/Effects01.o \
	$(OBJDIR)/Collection.o \
	$(OBJDIR)/SocketServer.o \
	$(OBJDIR)/VSmoother.o \
	$(OBJDIR)/Y4MInput.o \
	$(OBJDIR)/Y4MOutput.o \
	$(OBJDIR)/XArray.o \
	$(OBJDIR)/JpegSource.o \
	$(OBJDIR)/RGB3Source.o \
	$(OBJDIR)/HalfWidth.o \
	$(OBJDIR)/Mp2Enc.o \
	$(OBJDIR)/Mpeg2Enc.o \
	$(OBJDIR)/VFilter.o \
	$(OBJDIR)/AudioLimiter.o \
	$(OBJDIR)/Cryptor.o \
	$(OBJDIR)/MpegTSMux.o \
	$(OBJDIR)/MpegTSDemux.o \
	$(OBJDIR)/Tap.o \
	$(OBJDIR)/Keycodes.o \
	$(OBJDIR)/Pointer.o \
	$(OBJDIR)/ResourceMonitor.o \
	$(OBJDIR)/TV.o \
	$(OBJDIR)/DTV.o \
	$(OBJDIR)/ChannelMaps.o \
	$(OBJDIR)/Spawn.o \
	$(OBJDIR)/XPlaneControl.o \
	$(OBJDIR)/FaceTracker.o \
	$(OBJDIR)/Position.o \
	$(OBJDIR)/UI001.o \
	$(OBJDIR)/WavOutput.o \
	$(OBJDIR)/MjxRepair.o \
	$(OBJDIR)/GdkCapture.o \
	$(OBJDIR)/MjpegLocalBuffer.o \
	$(OBJDIR)/MjpegStreamBuffer.o \
	$(OBJDIR)/ImageLoader.o \
	$(OBJDIR)/Images.o \
	$(OBJDIR)/FPS.o \
	$(OBJDIR)/NScale.o \
	$(OBJDIR)/Y4MSource.o \
	$(OBJDIR)/Splitter.o \
	$(OBJDIR)/cti_main.o \
	$(OBJDIR)/AVIDemux.o \
	$(OBJDIR)/RawSource.o \
	$(OBJDIR)/ImageOutput.o \
	$(OBJDIR)/SubProc.o \
	$(OBJDIR)/ExecProc.o \
	$(OBJDIR)/dpf.o \
	$(OBJDIR)/XMLMessageServer.o \
	$(OBJDIR)/socket_common.o \
	$(OBJDIR)/XmlSubset.o \
	$(OBJDIR)/nodetree.o \
	$(OBJDIR)/Stats.o \
	$(OBJDIR)/StackDebug.o \
	$(OBJDIR)/crc.o \
	$(OBJDIR)/$(MAIN) \
	../../platform/$(ARCH)/jpeg-7/transupp.o

#	$(OBJDIR)/ScriptSession.o \


ifeq ($(OS),Linux)
OBJS+=\
	$(OBJDIR)/Uvc.o \
	$(OBJDIR)/FFmpegEncode.o \
	../../platform/$(ARCH)/jpeg-7/libjpeg.la
ifneq ($(ARCH),lpd)
OBJS+=\
	$(OBJDIR)/V4L2Capture.o \
	$(OBJDIR)/ALSAio.o \
	$(OBJDIR)/ALSAMixer.o
LDFLAGS+=-lasound 
CPPFLAGS+=-DHAVE_PRCTL
endif

#OBJS+=$(OBJDIR)/SonyPTZ.o
#CPPFLAGS+=-DHAVE_VISCA
#LDFLAGS+=-lvisca

ifeq ($(ARCH),lpd)
OBJS+=$(OBJDIR)/V4L1Capture.o
LDFLAGS+=-lm
endif

ifeq ($(ARCH),armeb)
CPPFLAGS+=-DHAVE_V4L1
OBJS+=$(OBJDIR)/V4L1Capture.o
endif

endif


# SDL on OSX
ifeq ($(ARCH),i386-Darwin)
MAIN=SDLMain.o
OBJS+=$(OBJDIR)/sdl_main.o
endif

# Signals
#ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/Signals.o
#endif

# Lirc
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/../liblirc_client.so))
OBJS+= $(OBJDIR)/Lirc.o
CPPFLAGS+=-DHAVE_LIRC
LDFLAGS+=-llirc_client
endif

#SDL
ifneq ($(ARCH),armeb)
ifneq ($(ARCH),lpd)
ifeq (0,$(shell (sdl-config --cflags > /dev/null 2> /dev/null; echo $$?)))
OBJS+=$(OBJDIR)/SDLstuff.o
CPPFLAGS+=$$(sdl-config --cflags) -I/usr/include/GL
LDFLAGS+=$$(sdl-config --libs) $$(pkg-config glu --libs)
CPPFLAGS+=-DHAVE_SDL
endif
endif
endif

# Cairo
ifneq ($(ARCH),armeb)
ifneq ($(ARCH),lpd)
ifeq (0,$(shell (pkg-config cairo --cflags > /dev/null 2> /dev/null; echo $$?)))
OBJS+=	$(OBJDIR)/CairoContext.o
CPPFLAGS+=$$(pkg-config cairo --cflags)
LDFLAGS+=$$(pkg-config cairo --libs)
# This is a new requirement to resolve undefined reference to `pixman_*' errors
LDFLAGS+=$$(pkg-config pixman-1 --libs)
CPPFLAGS+=-DHAVE_CAIRO
endif
endif
endif

# libdv
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/libdv.pc))
OBJS+=	$(OBJDIR)/LibDV.o
CPPFLAGS+=$$(pkg-config libdv --cflags)
LDFLAGS+=$$(pkg-config libdv --libs)
CPPFLAGS+=-DHAVE_LIBDV
endif

# libmpeg2
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/libmpeg2.pc))
OBJS+=	$(OBJDIR)/Mpeg2Dec.o
CPPFLAGS+=$$(pkg-config libmpeg2 --cflags)
LDFLAGS+=$$(pkg-config libmpeg2 --libs)
CPPFLAGS+=-DHAVE_LIBMPEG2
endif

# Quicktime
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/libquicktime.pc))
OBJS+=$(OBJDIR)/LibQuickTimeOutput.o
CPPFLAGS+=$$(pkg-config libquicktime --cflags)
LDFLAGS+=$$(pkg-config libquicktime --libs) -lpixman-1
CPPFLAGS+=-DHAVE_LIBQUICKTIME
endif

# Ogg
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/vorbisenc.pc))
OBJS+=$(OBJDIR)/OggOutput.o
CPPFLAGS+=$$(pkg-config vorbisenc theoraenc --cflags)
LDFLAGS+=$$(pkg-config vorbisenc theoraenc --libs)
CPPFLAGS+=-DHAVE_OGGOUTPUT
endif

# H264
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/x264.pc))
OBJS+=$(OBJDIR)/H264.o
CPPFLAGS+=$$(pkg-config x264 --cflags)
LDFLAGS+=$$(pkg-config x264 --libs)
CPPFLAGS+=-DHAVE_H264
endif

# AAC
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/../../include/faac.h))
OBJS+=$(OBJDIR)/AAC.o
LDFLAGS+=-lfaac
CPPFLAGS+=-DHAVE_AAC
endif

# SQLite
ifneq (,$(shell /bin/ls $(PKGCONFIGDIR)/sqlite3.pc))
OBJS+=$(OBJDIR)/SQLite.o
LDFLAGS += -lsqlite3
CPPFLAGS+=-DHAVE_SQLITE3
endif

# CUDA
ifeq ($(ARCH),x86_64-Linux)
ifneq (,$(shell /bin/ls /opt/cuda/bin/nvcc))
OBJS+=$(OBJDIR)/NVidiaCUDA.o
LDFLAGS += 
CPPFLAGS+=
endif
endif


$(OBJDIR)/cti$(EXEEXT): \
	$(OBJS) \
	$(OBJDIR)/cti_app.o
	@echo LINK
#	@echo $(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
	@$(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
ifeq ($(ARCH),x86_64-Linux)
# Sigh, some libs bump their version numbers all the fucking time.  And I like to keep
# cti binaries around for later use, without always having to rebuild.  So, keep a
# cache of libraries which frequently change.
#	@echo Copying required libaries that frequently change:
#	@cp -Lvu $$(ldd $@ | grep -E '264|png' | sed -e 's,.*/usr,/usr,g' -e 's, .*$$,,') $(HOME)/lib/
# Or ../../platform/$(ARCH)/lib/ ?
endif
	@echo Generating map
	$(NM) $@ | sort > $@.map
#	@echo STRIP
#	@$(STRIP) $@


SHARED_OBJS=$(subst .o,.so,$(OBJS))

$(OBJDIR)/ctis$(EXEEXT) : \
	$(SHARED_OBJS)
	@echo All shared objects are built.


$(OBJDIR)/mjplay$(EXEEXT): \
	$(OBJS) \
	$(OBJDIR)/mjplay.o
	@echo LINK
	$(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
#	$(STRIP) $@


$(OBJDIR)/ctest$(EXEEXT): \
	$(OBJDIR)/Collection.o \
	$(OBJDIR)/ctest.o
	$(CC) $(filter %.o, $^) -o $@



$(OBJDIR)/%.o: %.c Makefile
	@echo CC $<
#	@echo $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	@$(CC) $(CPPFLAGS) $(INSTRUMENT) $(CFLAGS) -c $< -o $@

$(OBJDIR)/StackDebug.o: StackDebug.c Makefile
	@echo CC $<
#	@echo $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.so: %.c Makefile
	@echo 'CC (dll)' $<
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SHARED_FLAGS) -DCTI_SHARED -c $< -o $@

$(OBJDIR)/%.o: %.m Makefile
	@echo CC $<
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -vf $(OBJDIR)/*.o  $(OBJDIR)/*.dep

DEPS=$(wildcard $(OBJDIR)/*.dep)
ifneq ($(DEPS),)
include $(DEPS)
endif
