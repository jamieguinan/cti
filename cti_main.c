#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

#include "CTI.h"
#include "ALSAio.h"
#include "ALSAMixer.h"
#include "CJpeg.h"
#include "DJpeg.h"
#include "DO511.h"
#include "Images.h"
#include "TCPOutput.h"
#include "HalfWidth.h"
#include "JpegFiler.h"
#include "JpegTran.h"
#include "MotionDetect.h"
#include "V4L2Capture.h"
#include "MjpegMux.h"
#include "MjpegDemux.h"
#include "MjpegMux.h"
#include "Mp2Enc.h"
#include "Mpeg2Enc.h"
#include "ScriptSession.h"
#include "SDLstuff.h"
#include "String.h"
#include "V4L2Capture.h"
#include "VFilter.h"
#include "AudioLimiter.h"
#include "ScriptV00.h"
#include "Null.h"
#include "Effects01.h"
#include "SocketServer.h"
#include "VSmoother.h"
#include "Y4MInput.h"
#include "Y4MOutput.h"
#include "CairoContext.h"
#include "Signals.h"
#include "JpegSource.h"
#include "DVDgen2.h"
#include "LibQuickTimeOutput.h"
#include "OggOutput.h"
#include "SonyPTZ.h"
#include "MpegTSMux.h"
#include "MpegTSDemux.h"
#include "H264.h"
#include "Tap.h"
#include "ResourceMonitor.h"
#include "Lirc.h"
#include "TV.h"
#include "LibDV.h"
#include "Spawn.h"
#include "FaceTracker.h"

extern int app_code(int argc, char *argv[]);

char *argv0;

int cti_main(int argc, char *argv[])
{
  argv0 = argv[0];

#ifdef __linux__
  ALSACapture_init();
  ALSAPlayback_init();
  ALSAMixer_init();
  V4L2Capture_init();
  SonyPTZ_init();
#ifndef __ARMEB__
  SDLstuff_init();
  Signals_init();
  CairoContext_init();
  LibQuickTimeOutput_init();
  OggOutput_init();
  H264_init();
  Lirc_init();
  LibDV_init();
  Spawn_init();
#endif
#endif
  CJpeg_init();
  DJpeg_init();
  JpegFiler_init();
  JpegTran_init();
  JpegSource_init();
  MjpegMux_init();
  MjpegDemux_init();
  ScriptV00_init();
  Null_init();
  DO511_init();
  Effects01_init();
  SocketServer_init();
  VSmoother_init();
  Y4MOutput_init();
  MotionDetect_init();
  DVDgen2_init();
  HalfWidth_init();
  Mp2Enc_init();
  Mpeg2Enc_init();
  MpegTSMux_init();
  MpegTSDemux_init();
  VFilter_init();
  AudioLimiter_init();
  Tap_init();
  ResourceMonitor_init();
  TV_init();
  FaceTracker_init();
  //ScriptSession_init();

  return app_code(argc, argv);
}
