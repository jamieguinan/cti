#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

#include "CTI.h"
#include "ALSACapRec.h"
#include "ALSAMixer.h"
#include "CJpeg.h"
#include "DJpeg.h"
#include "DO511.h"
#include "Images.h"
#include "HalfWidth.h"
#include "JpegFiler.h"
#include "JpegTran.h"
#include "PGMFiler.h"
#include "MotionDetect.h"
#include "V4L1Capture.h"
#include "V4L2Capture.h"
#include "MjpegMux.h"
#include "MjpegDemux.h"
#include "MjpegMux.h"
#include "MjpegLocalBuffer.h"
#include "MjpegStreamBuffer.h"
#include "Mp2Enc.h"
#include "Mpeg2Enc.h"
#include "SDLstuff.h"
#include "String.h"
#include "VFilter.h"
#include "AudioLimiter.h"
#include "AudioFilter.h"
#include "ScriptV00.h"
#include "Null.h"
#include "Effects01.h"
#include "SocketServer.h"
#include "SocketRelay.h"
#include "VSmoother.h"
#include "Y4MInput.h"
#include "Y4MOutput.h"
#include "CairoContext.h"
#include "Signals.h"
#include "JpegSource.h"
#include "RGB3Source.h"
#include "LibQuickTimeOutput.h"
#include "OggOutput.h"
#include "SonyPTZ.h"
#include "MpegTSMux.h"
#include "MpegTSDemux.h"
#include "H264.h"
#include "AAC.h"
#include "Tap.h"
#include "ResourceMonitor.h"
#include "Lirc.h"
#include "TV.h"
#include "DTV.h"
#include "LibDV.h"
#include "Spawn.h"
#include "FaceTracker.h"
#include "UI001.h"
#include "WavOutput.h"
#include "MjxRepair.h"
#include "GdkCapture.h"
#include "ImageLoader.h"
#include "Y4MSource.h"
#include "Splitter.h"
#include "NScale.h"
#include "AVIDemux.h"
#include "RawSource.h"
#include "ImageOutput.h"
#include "SubProc.h"
#include "ExecProc.h"
#include "XMLMessageServer.h"
#include "XMLMessageRelay.h"
#include "Mpeg2Dec.h"
#include "HTTPServer.h"
#include "VirtualStorage.h"
#include "ATSCTuner.h"
#include "Y4MOverlay.h"
#include "LinuxEvent.h"
#include "VMixer.h"
#include "M3U8.h"
#include "TunerControl.h"
#include "RPiH264Enc.h"
#include "BinaryFiler.h"
#include "PushQueue.h"
#include "Global.h"
#include "Curl.h"
//#include "PCMSource.h"

extern int app_code(int argc, char *argv[]);

char *argv0;

InstanceGroup *gig;

int cti_main(int argc, char *argv[])
{
  argv0 = argv[0];

  gig = InstanceGroup_new();
  instance_key_init();

#ifdef HAVE_V4L1
  V4L1Capture_init();
#endif

#ifdef __linux__
  ALSACapRec_init();
  ALSAMixer_init();
  V4L2Capture_init();
  Signals_init();
  Spawn_init();
#endif

#ifdef HAVE_VISCA
  SonyPTZ_init();
#endif

#ifdef HAVE_SDL
  SDLstuff_init();
#endif

#ifdef HAVE_CAIRO
  CairoContext_init();
#endif

#ifdef HAVE_AAC
  AAC_init();
#endif

#ifdef HAVE_H264
  H264_init();
#endif

#ifdef HAVE_OGGOUTPUT
  OggOutput_init();
#endif

#ifdef HAVE_LIBDV
  LibDV_init();
#endif

#ifdef HAVE_LIBMPEG2
  Mpeg2Dec_init();
#endif

#ifdef HAVE_LIBQUICKTIME
  // LibQuickTimeOutput_init();
#endif

#ifdef HAVE_LIRC
  Lirc_init();
#endif

#ifdef HAVE_RPIH264ENC
  RPiH264Enc_init();
#endif

#ifdef HAVE_CURL
  Curl_init();
#endif

  CJpeg_init();
  DJpeg_init();
  JpegFiler_init();
  JpegTran_init();
  JpegSource_init();
  RGB3Source_init();
  PGMFiler_init();
  MjpegMux_init();
  MjpegDemux_init();
  MjpegLocalBuffer_init();
  MjpegStreamBuffer_init();
  ScriptV00_init();
  Null_init();
  DO511_init();
  Effects01_init();
  SocketServer_init();
  SocketRelay_init();
  VSmoother_init();
  Y4MInput_init();
  Y4MOutput_init();
  MotionDetect_init();
  HalfWidth_init();
  Mp2Enc_init();
  Mpeg2Enc_init();
  MpegTSMux_init();
  MpegTSDemux_init();
  VFilter_init();
  AudioLimiter_init();
  AudioFilter_init();
  Tap_init();
  ResourceMonitor_init();
  TV_init();
  DTV_init();
  FaceTracker_init();
  UI001_init();
  WavOutput_init();
  MjxRepair_init();
  GdkCapture_init();
  ImageLoader_init();
  Y4MSource_init();
  Splitter_init();
  NScale_init();
  AVIDemux_init();
  RawSource_init();
  ImageOutput_init();
  SubProc_init();
  ExecProc_init();
  XMLMessageServer_init();
  XMLMessageRelay_init();
  HTTPServer_init();
  VirtualStorage_init();
  ATSCTuner_init();
  Y4MOverlay_init();
  LinuxEvent_init();
  VMixer_init();
  M3U8_init();
  TunerControl_init();
  BinaryFiler_init();
  PushQueue_init();
  //PCMSource_init();
  Global_init();

  return app_code(argc, argv);
}
