default: default1

include ../platforms.make

CFLAGS += -g -Wall $(CMDLINE_CFLAGS)
# -std=c99 
CPPFLAGS += -I../../platform/$(ARCH)/include -I../jpeg-7
CPPFLAGS += -MMD -MP -MF $(OBJDIR)/$(subst .c,.dep,$<)
LDFLAGS += -L../../platform/$(ARCH)/lib -ljpeg -lpthread
ifeq ($(OS),Linux)
LDFLAGS += -lasound -ldl -lrt
endif

# "-static" is a problem for alsa, and other things...

OBJDIR ?= .

#default1: $(OBJDIR)/avcap$(EXEEXT) $(OBJDIR)/avidemux$(EXEEXT) $(OBJDIR)/cjbench$(EXEEXT) $(OBJDIR)/dvdgen$(EXEEXT) $(OBJDIR)/avtest$(EXEEXT) $(OBJDIR)/demux$(EXEEXT) $(OBJDIR)/alsacaptest$(EXEEXT) $(OBJDIR)/mjxtomp2$(EXEEXT) $(OBJDIR)/cti$(EXEEXT)

default1:  $(OBJDIR)/cti$(EXEEXT)

#	@echo wd=$(shell pwd)
#	@echo VPATH=$(VPATH)

# For SDL.

# Another app.
OBJS= \
	$(OBJDIR)/Template.o \
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
	$(OBJDIR)/MotionDetect.o \
	$(OBJDIR)/MjpegMux.o \
	$(OBJDIR)/MjpegDemux.o \
	$(OBJDIR)/SourceSink.o \
	$(OBJDIR)/String.o \
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
	$(OBJDIR)/HalfWidth.o \
	$(OBJDIR)/Mp2Enc.o \
	$(OBJDIR)/Mpeg2Enc.o \
	$(OBJDIR)/VFilter.o \
	$(OBJDIR)/AudioLimiter.o \
	$(OBJDIR)/DVDgen2.o \
	$(OBJDIR)/Cryptor.o \
	$(OBJDIR)/MpegTSMux.o \
	$(OBJDIR)/MpegTSDemux.o \
	$(OBJDIR)/Tap.o \
	$(OBJDIR)/Keycodes.o \
	$(OBJDIR)/ResourceMonitor.o \
	$(OBJDIR)/TV.o \
	$(OBJDIR)/ChannelMaps.o \
	$(OBJDIR)/main.o \
	../../platform/$(ARCH)/jpeg-7/transupp.o

#	$(OBJDIR)/ScriptSession.o \


ifeq ($(OS),Linux)
OBJS+=\
	$(OBJDIR)/Uvc.o \
	$(OBJDIR)/V4L2Capture.o \
	$(OBJDIR)/ALSAio.o \
	$(OBJDIR)/ALSAMixer.o \
	$(OBJDIR)/FFmpegEncode.o \
	$(OBJDIR)/SonyPTZ.o \
	../../platform/$(ARCH)/jpeg-7/libjpeg.la
LDFLAGS+=-lvisca
endif

# SDL
ifeq ($(ARCH),armeb)
SDLLIBS=
else
OBJS+=$(OBJDIR)/SDLstuff.o
CPPFLAGS+=$$(sdl-config --cflags) -I/usr/include/GL
LDFLAGS+=$$(sdl-config --libs) $$(pkg-config glu --libs)
endif

# Signals
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/Signals.o
endif

# Lirc
ifneq ($(ARCH),armeb)
OBJS+= $(OBJDIR)/Lirc.o
LDFLAGS+=-llirc_client
endif


# Cairo
ifneq ($(ARCH),armeb)
OBJS+=	$(OBJDIR)/CairoContext.o
CPPFLAGS+=$$(pkg-config cairo --cflags)
LDFLAGS+=$$(pkg-config cairo --libs)
endif

# libdv
ifneq ($(ARCH),armeb)
OBJS+=	$(OBJDIR)/LibDV.o
CPPFLAGS+=$$(pkg-config libdv --cflags)
LDFLAGS+=$$(pkg-config libdv --libs)
endif

# Quicktime
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/LibQuickTimeOutput.o
CPPFLAGS+=$$(lqt-config --cflags)
LDFLAGS+=$$(lqt-config --libs)
endif

# Ogg
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/OggOutput.o
CPPFLAGS+=$$(pkg-config vorbisenc theoraenc --cflags)
LDFLAGS+=$$(pkg-config vorbisenc theoraenc --libs)
endif

# H264
ifneq ($(ARCH),armeb)
OBJS+=$(OBJDIR)/H264.o
CPPFLAGS+=$$(pkg-config x264 --cflags)
LDFLAGS+=$$(pkg-config x264 --libs)
endif


$(OBJDIR)/cti$(EXEEXT): \
	$(OBJS) \
	$(OBJDIR)/cti.o
	@echo LINK
	$(CC) $(filter %.o, $^) -o $@ $(LDFLAGS)
#	$(STRIP) $@


$(OBJDIR)/ctest$(EXEEXT): \
	$(OBJDIR)/Collection.o \
	$(OBJDIR)/ctest.o
	$(CC) $(filter %.o, $^) -o $@


$(OBJDIR)/%.o: %.c Makefile
	@echo CC $@
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@


clean:
	rm -vf $(OBJDIR)/*.o  $(OBJDIR)/*.dep

DEPS=$(wildcard $(OBJDIR)/*.dep)
ifneq ($(DEPS),)
include $(DEPS)
endif
