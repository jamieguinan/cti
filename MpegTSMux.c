/*
 * Mpeg TS/PES muxer.  
 * See iso13818-1.pdf around p.32 for bit packing.
 * This also supports generating files for HTTP Live Streaming,
 *   http://tools.ietf.org/html/draft-pantos-http-live-streaming-12
 *   https://developer.apple.com/library/ios/technotes/tn2288/_index.html
 *   https://developer.apple.com/library/ios/technotes/tn2224/_index.html
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* access */
#include <sys/stat.h>		/* mkdir */
#include <sys/types.h>		/* mkdir */
#include <inttypes.h>		/* PRIu64 */
#include <math.h>		/* fmod */

#include "CTI.h"
#include "MpegTSMux.h"
#include "MpegTS.h"
#include "Images.h"
#include "Audio.h"
#include "Array.h"
#include "SourceSink.h"

/* From libavformat/mpegtsenc.c */
static const uint32_t crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

uint32_t mpegts_crc32(const uint8_t *data, int len)
{
    register int i;
    uint32_t crc = 0xffffffff;
    
    for (i=0; i<len; i++)
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];
    
    return crc;
}


static void Config_handler(Instance *pi, void *msg);
static void H264_handler(Instance *pi, void *msg);
static void AAC_handler(Instance *pi, void *msg);
static void MP3_handler(Instance *pi, void *msg);
static void flush(Instance *pi, uint64_t flush_timestamp);

enum { INPUT_CONFIG, INPUT_H264, INPUT_AAC, INPUT_MP3 };
static Input MpegTSMux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_H264 ] = { .type_label = "H264_buffer", .handler = H264_handler },
  [ INPUT_AAC ] = { .type_label = "AAC_buffer", .handler = AAC_handler },
  [ INPUT_MP3 ] = { .type_label = "MP3_buffer", .handler = MP3_handler },
};

enum { OUTPUT_RAWDATA };
static Output MpegTSMux_outputs[] = {
  [ OUTPUT_RAWDATA ] = { .type_label = "RawData_buffer", .destination = 0L },
};

int v = 0;


typedef struct _ts_packet {
  uint8_t data[188];
  struct _ts_packet *next;
  int af;
  int pus;
  uint64_t estimated_timestamp;	/* in 90KHz units */
} TSPacket;

#define ESSD_MAX 2

typedef struct {
  TSPacket * packets;
  TSPacket * last_packet;
  int packet_count;
  uint64_t pts_value;
  uint64_t es_duration;		/* nominal elementary packet duration in 90KHz units */
  uint16_t pid;
  uint16_t continuity_counter;
  uint16_t es_id;
  uint pts:1;
  uint dts:1;
  uint pcr:1;
  const char *typecode;
} Stream;

/* Maximum PAT interval is 100ms */
#define MAXIMUM_PAT_INTERVAL_90KHZ (90000/10)

typedef struct {
  Instance i;
  /* TS name format string, using '%s' will generate sequential
     names. */
  Sink *output_sink;

  /* TS sequence duration in seconds. */
  int duration;

  /* Number of output files to maintain */
  int output_count;

  /* PTS timestamp of beginnig of current sequence. */
  uint64_t m3u8_pts_start;

  /* PCR should be "behind" PTS so that the decoder has time to decompress and possibly
     even reorder video frames.  200ms seems to work well, but it is a config item for
     experimentation. */
  unsigned int pcr_lag_ms;

  /* Names of ts files to go into .m3u8 list. */
  String index_dir;
  String_list * m3u8_ts_files;
  int media_sequence;

#define MAX_STREAMS 2
  Stream streams[MAX_STREAMS];

  struct {
    uint8_t continuity_counter;
  } PAT;

  struct {
    uint16_t PCR_PID;
    /* Elementary stream specific data: */
    struct {
      uint8_t streamType;
      uint16_t PID;
    } ESSD[ESSD_MAX];
    uint8_t continuity_counter;
  } PMT;

  int seen_audio;
  int debug_outpackets;
  int debug_pktseq; 

  int verbose;			/* For certain printfs. */

} MpegTSMux_private;


static int set_pmt_essd(Instance *pi, const char *value)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  String_list * parts = String_split_s(value, ":");
  
  if (String_list_len(parts) != 3) {
    fprintf(stderr, "%s expected essd as index:streamtype:pid\n", __func__);
    return 1;
  }

  int i, streamtype, pid;
  String_parse_int(String_list_get(parts, 0), 0, &i);
  String_parse_int(String_list_get(parts, 1), 0, &streamtype);
  String_parse_int(String_list_get(parts, 2), 0, &pid);

  if (i < 0 || i >= (ESSD_MAX)) {
    fprintf(stderr, "%s: essd index out of range\n",  __func__);
    return 1;
  }

  if (pid < 0 || pid > 8191) {
    fprintf(stderr, "%s: essd pid out of range\n",  __func__);
    return 1;
  }

  if (streamtype < 0 || streamtype > 255) {
    fprintf(stderr, "%s: essd streamtype out of range\n",  __func__);
    return 1;
  }

  priv->PMT.ESSD[i].PID = pid;
  priv->PMT.ESSD[i].streamType = streamtype;

  String_list_free(&parts);
  return 0;
}


static int set_pmt_pcrpid(Instance *pi, const char *value)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  int pcrpid = atoi(value);
  if (pcrpid < 0 || pcrpid > 8191) {
    fprintf(stderr, "%s: pmt pcrpid out of range\n",  __func__);
    return 1;
  }
  priv->PMT.PCR_PID = pcrpid;
  return 0;
}


static int set_output(Instance *pi, const char *value)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  if (value[0] == '$') {
    value = getenv(value+1);
    if (!value) {
      fprintf(stderr, "env var %s is not set\n", value+1);
      return 1;
    }
  }
  
  if (priv->output_sink) {
    Sink_free(&priv->output_sink);
  }

  priv->output_sink = Sink_allocate(value);

  return 0;
}


static int set_index_dir(Instance *pi, const char *value)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;

  String_set_local(&priv->index_dir, value);

  return 0;
}


static Config config_table[] = {
  { "pmt_essd", set_pmt_essd, 0L, 0L },
  { "pmt_pcrpid", set_pmt_pcrpid, 0L, 0L },
  { "debug_outpackets", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSMux_private, debug_outpackets) },
  { "output", set_output },
  { "index_dir", set_index_dir },
  { "duration",  0L, 0L, 0L, cti_set_int, offsetof(MpegTSMux_private, duration) },
  { "pcr_lag_ms",  0L, 0L, 0L, cti_set_uint, offsetof(MpegTSMux_private, pcr_lag_ms) },
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void m3u8_files_update(MpegTSMux_private * priv)
{
  if (String_list_len(priv->m3u8_ts_files) <= 1) {
    /* The last file is "being generated" so don't make a list if
       only a single file. */
    return;
  }

  String * file_to_delete = String_value_none();
  if (String_list_len(priv->m3u8_ts_files) > 6) {
    file_to_delete = String_list_pull_at(priv->m3u8_ts_files, 0);
  }

  String * tmpname = String_sprintf("%s/prog_index.m3u8.tmp", sl(priv->index_dir));
  FILE * fpi = fopen(s(tmpname), "w");
  if (!fpi) {
    printf("%s: failed to open %s\n", __func__, s(tmpname));
    goto out;
  }

  /* FIXME: Consider,
       #EXTINF:1, no desc
  */

  fprintf(fpi, "#EXTM3U\n");
  fprintf(fpi, "#EXT-X-TARGETDURATION:%d\n", priv->duration);
  fprintf(fpi, "#EXT-X-VERSION:3\n");
  fprintf(fpi, "#EXT-X-MEDIA-SEQUENCE:%d\n", priv->media_sequence);
  priv->media_sequence += 1;

  if (priv->verbose) { printf("========\n"); }
  int i;
  for (i=0; 
       i < String_list_len(priv->m3u8_ts_files)
	 - 1; 			/* Don't include the latest, it is still being generated. */
       i++) {
    String * fstr = String_list_get(priv->m3u8_ts_files, i);
    if (priv->verbose) { printf(":: %s\n", s(fstr)); }
    fprintf(fpi, "#EXTINF:%d,\n", priv->duration);
    String * bname = String_basename(fstr);
    fprintf(fpi, "%s\n", s(bname));
    String_free(&bname);
  }

  //fprintf(fpi, "#EXT-X-ENDLIST\n");  /* ONLY for VODs, not live... */
  fclose(fpi);

  String * m3u8name = String_sprintf("%s/prog_index.m3u8", sl(priv->index_dir));
  rename(s(tmpname), s(m3u8name));

 out:
  if (!String_is_none(file_to_delete)) {
    unlink(s(file_to_delete));
    String_free(&file_to_delete);
  }

  String_free(&m3u8name);
  String_free(&tmpname);
}


static void packetize(MpegTSMux_private * priv, uint16_t pid, ArrayU8 * data)
{
  /* Wrap a unit of encoded data into a elementary stream packet, then
     divide and wrap that into a series of 188-byte transport stream
     packets and leave them in a list for later multiplexing.  Larger
     TS packet types are not currently supported. */
  MpegTimeStamp pts = {};
  MpegTimeStamp dts = {};
  int i;
  Stream *stream = NULL;
  
  for (i=0; i < MAX_STREAMS; i++) {
    if (priv->streams[i].pid == pid) {
      stream = &priv->streams[i];
      break;
    }
  }
  if (i == MAX_STREAMS) {
    printf("pid %d not set up\n", pid);
    return;
  }

  /* Map timestamp to 33-bit 90KHz units. */
  /* Assign to PTS, and same to DTS. */
  uint8_t peslen = 0; /* PES remaining header data length */
  uint8_t flags = 0;
  if (stream->pts) {
    pts.set = 1;
    flags |= (1<<7);
    peslen += 5;
    pts.value = stream->pts_value;
    if (stream->dts) {
      dts.set = 1;
      flags |= (1<<6);
      peslen += 5;
      dts.value = pts.value;
    }
  }

  /* Wrap the data in a PES packet (prepend a chunk of header data),
     then divide into TS packets, and either write out, or save in a
     list so they can be smoothly interleaved with audio packets.  See
     "demo-ts-analysis.txt".  */
  ArrayU8 *pes = ArrayU8_new();
  ArrayU8_append_bytes(pes, 
		       0x00, 0x00, 0x01, /* PES packet start code prefix. */
		       stream->es_id);	 /* Elementary stream id */

  if (stream->pcr) {
    /* PES remaining length UNSET for video. */
    ArrayU8_append_bytes(pes, 0x00, 0x00);
  }
  else {
    /* PES remaining length SET for audio. */
    /* 8 == 0x84, flag, peslen, [5 PTS bytes] */
    int x = 8 + data->len;	
    ArrayU8_append_bytes(pes, (x >> 8 & 0xff), (x & 0xff));
  }

  if (1 /* extension present, depending on Stream id */) {
    ArrayU8_append_bytes(pes,
			 0x84,	/* '10', PES priority bit set, various flags unset. */
			 flags,
			 peslen
			 );
  }

  if (pts.set) {
    /* Pack PTS. */
    dpf("stream %s pts.value=%" PRIu64 "\n", stream->typecode, pts.value);
    ArrayU8_append_bytes(pes, 
			 (pts.set << 5) | (dts.set << 4) | (((pts.value>>30)&0x7) << 1 ) | 1
			 ,((pts.value >> 22)&0xff) /* bits 29:22 */
			 ,((pts.value >> 14)&0xfe) | 1 /* bits 21:15, 1 */
			 ,((pts.value >> 7)&0xff)	   /* bits 14:7 */
			 ,((pts.value << 1)&0xfe) | 1   /* bits 6:0, 1 */
			 );
  }

  if (dts.set) {
    dpf("stream %s dts.value=%" PRIu64 "\n", stream->typecode, dts.value);
    ArrayU8_append_bytes(pes,
			 (1 << 4) | (((dts.value>>30)&0x7) << 1 ) | 1
			 ,((dts.value >> 22)&0xff) /* bits 29:22 */
			 ,((dts.value >> 14)&0xfe) | 1 /* bits 21:15, 1 */
			 ,((dts.value >> 7)&0xff)	   /* bits 14:7 */
			 ,((dts.value << 1)&0xfe) | 1   /* bits 6:0, 1 */
			 );
  }


  /* Copy the data. */
  ArrayU8_append(pes, data);

  if (v) printf("pid=%d pes->len=%ld, data->len=%ld\n", pid, pes->len, data->len);
  
  /* Now pack into TS packets... */
  unsigned int pes_offset = 0;
  unsigned int pes_remaining;
  unsigned int payload_size;
  unsigned int payload_index = 0;

  uint64_t et = stream->pts_value;
  int et_add = stream->es_duration / ((pes->len/188)+1);

  while (pes_offset < pes->len) {
    TSPacket *packet = Mem_calloc(1, sizeof(*packet));
    packet->estimated_timestamp = et;
    et += et_add;

    //printf("stream %u estimated timestamp: %" PRIu64 " (es_duration=%" PRIu64 " et_add=%d)\n", 
    //   stream->pid, packet->estimated_timestamp, stream->es_duration, et_add);

    unsigned int afLen = 0;
    unsigned int afAdjust = 1;	/* space for the afLen byte */

    pes_remaining = pes->len - pes_offset;
    if (v) printf("pes->len=%ld pes_remaining=%d\n", pes->len, pes_remaining);

    packet->data[0] = 0x47;			/* Sync byte */

    packet->data[1] = 
      (0 << 5) |		      /* priority bit, not set */
      ((pid & 0x1f00) >> 8);	      /* pid[13:8] */

    if (payload_index == 0) {
      packet->pus = 1;
      packet->data[1] |= (1 << 6);      /*  Payload Unit Start */
    }
    
    packet->data[2] = (pid & 0xff); /* pid[7:0] */
    packet->data[3] = (0x0 << 6); /* not scrambled */
    packet->data[3] |= (1 << 4); /* contains payload */
    packet->data[3] |= (stream->continuity_counter & 0x0F);
    stream->continuity_counter += 1;

    if (payload_index == 0 && ( (stream->pcr) || (!stream->pcr && pes->len <= 183) ) ) {
      packet->data[3] |= (1 << 5); /* adaptation field */
      packet->af = 1;
      afLen = 1;

      if (stream->pcr) {
	uint64_t pcr_value = pts.value - (priv->pcr_lag_ms * 90);
	afLen += 6;
	packet->data[5] |= (1<<4);	/* PCR */
	/* FIXME: use data[index] if other flag bits are to be set. */
	/* Notice corresponding unpack code in MpegTSDemux.c for same shifts in other direction. */
	packet->data[6] = ((pcr_value >> 25) & 0xff);
	packet->data[7] = ((pcr_value >> 17) & 0xff);
	packet->data[8] = ((pcr_value >> 9) & 0xff);
	packet->data[9] = ((pcr_value >> 1) & 0xff);
	packet->data[10] = ((pcr_value & 1) << 7) /* low bit of 33 bits */
	  | (0x3f << 1)  /* 6 bits reserved/padding */
	  | (0<<0);	/* 9th bit of extension */
	packet->data[11] = (0x00);	/* bits 8:0 of extension */
      }

      int available_payload_size = (188 - 4 - 1 - afLen);
      if (pes_remaining < available_payload_size) {
	/* Everything fits into one packet! */
	int filler_count = (available_payload_size - pes_remaining);
	memset(packet->data+4+1+afLen, 0xFF, filler_count); /* pad with FFs... */
	afLen += filler_count;
	payload_size = pes_remaining;
      }
      else {
	payload_size = available_payload_size;
      }
      packet->data[4] = afLen;
    }
    else if (pes_remaining <= 183) {
      /* Note: when pes_remaining == 183, afLen is 1, and the memset
	 sets 0 bytes. */
      packet->data[3] |= (1 << 5); /* adaptation field */
      afLen = (188 - 4 - 1 - pes_remaining);
      packet->af = 1;
      packet->data[4] = afLen;
      packet->data[5]  = 0x00;	/* no flags set */
      if (afLen > 0) {
	memset(packet->data+4+1+1, 0xFF, afLen-1);  /* pad with FFs... */
      }
      payload_size = pes_remaining;
    }
    else {
      /* No adaptation field. */
      payload_size = 188 - 4;
      afAdjust = 0;
    }

    memcpy(packet->data+4+afAdjust+afLen, 
	   pes->data+pes_offset, 
	   payload_size);

    pes_offset += payload_size;
    payload_index += 1;

    /* Add to list. */
    if (!stream->packets) {
      stream->packets = packet;
      stream->last_packet = packet;
    }
    else {
      stream->last_packet->next = packet;
      stream->last_packet = packet;
    }
    stream->packet_count += 1;
  }

  ArrayU8_cleanup(&pes);
}

static uint64_t timestamp_to_90KHz(double timestamp)
{
  return (uint64_t)(fmod(timestamp, 95000)  // pts wraps at 95000 seconds, so why not
		    * 1000000 // Convert to microseconds.
		    * 9 / 100); // Convert to 90KHz units.
}

static void H264_handler(Instance *pi, void *msg)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  H264_buffer *h264 = msg;

  if (h264->keyframe) {
    /* Note, flush BEFORE packetize, so that next batch begins with keyframe. */
    flush(pi, timestamp_to_90KHz(h264->c.timestamp));
  }

  priv->streams[0].pts_value = timestamp_to_90KHz(h264->c.timestamp);
  priv->streams[0].es_duration = timestamp_to_90KHz(h264->c.nominal_period);
  dpf("h264->encoded_length=%d timestamp=%.6f nominal_period=%.6f es_duration=%" PRIu64" pts_value=%" PRIu64 " keyframe:%s\n",
      h264->encoded_length, 
      h264->c.timestamp,
      h264->c.nominal_period,
      priv->streams[0].es_duration,
      priv->streams[0].pts_value,
      h264->keyframe?"Y":"N"
      );
  packetize(priv, 258, ArrayU8_temp_const(h264->data, h264->encoded_length));

  H264_buffer_discard(h264);
}


static void AAC_handler(Instance *pi, void *msg)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  AAC_buffer *aac = msg;
  if (!priv->seen_audio) { priv->seen_audio = 1; }
  priv->streams[1].pts_value = timestamp_to_90KHz(aac->timestamp);
  priv->streams[1].es_duration = timestamp_to_90KHz(aac->nominal_period);
  dpf("aac->data_length=%d aac->timestamp=%.6f aac->nominal_period=%.6f es_duration=%" PRIu64"\n",
	 aac->data_length, aac->timestamp,  aac->nominal_period, priv->streams[1].es_duration );
  packetize(priv, 257, ArrayU8_temp_const(aac->data, aac->data_length) );
  AAC_buffer_discard(&aac);
}


static void MP3_handler(Instance *pi, void *msg)
{
  // MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  //MP3_buffer *mp3 = msg;

  /* Assemble TS packets, save in a list so they can be smoothly
     interleaved with video packets. */
  
  /* Discard MP3 data. */
  //MP3_buffer_discard(&mp3);
}


static TSPacket * generate_psi(MpegTSMux_private *priv, uint16_t pid, uint8_t table_id,
			       uint8_t *continuity_counter)
{
  TSPacket *packet = Mem_calloc(1, sizeof(*packet));
  int n = 0;

  packet->data[n++] = 0x47;	/* sync byte */
  packet->data[n++] = 
    (1 << 6) |			/* Payload Unit Start, PSI data */
    ((pid & 0x1f00) >> 8);	/* pid[13:8] */

  packet->pus = 1;

  packet->data[n++] = (pid & 0xff); /* pid[7:0] */
  
  packet->data[n] = (0x0 << 6); /* not scrambled */
  /* no adaptation field */
  packet->data[n] |= (1 << 4);
  packet->data[n++] |= (*continuity_counter & 0x0F);
  *continuity_counter += 1;

  packet->data[n++] = 0;	/* pointer filler byte count (0) */

  /* Table header: */
  packet->data[n++] = table_id;
  packet->data[n] = (1 << 7);	/* Section syntax indicator. */
  /* private bit unset */
  packet->data[n] |= (0x3 << 4); /* "11" reserved bits. */
  /* "00" unused bits unset. */
  uint16_t section_length = 0;
  if (table_id == 0) {
    section_length = 13;	/* FIXME: voodoo */
  }
  else if (table_id == 2) {
    section_length = 23;	/* FIXME: should be calculated based on ESSD... */
  }
  packet->data[n++] |= (section_length >> 8) & 0x3;
  packet->data[n++] = (section_length & 0xff);
  
  /* Table syntax section: */
  uint16_t table_id_extension = 1;
  packet->data[n++] = (table_id_extension >> 8) & 0xff;
  packet->data[n++] = (table_id_extension) & 0xff;
  packet->data[n] = (0x3 << 6);	/* Reserved bits. */
  /* version number unset */
  packet->data[n++] |= (1);	/* current/next */
  packet->data[n++] = 0x00;	/* section number */
  packet->data[n++] = 0x00;	/* last section number */
  
  if (table_id == 0) {
    /* PAT data */
    uint16_t program_num = 1;
    packet->data[n++] = (program_num >> 8) & 0xff;
    packet->data[n++] = (program_num & 0xff);
    packet->data[n] = (0x7 << 5); /* reserved bits */
    uint16_t program_map_pid = 256; /* FIXME: voodoo */
    packet->data[n++] |= ((program_map_pid >> 8) & 0x1f);
    packet->data[n++] = (program_map_pid & 0xff);
  }

  else if (table_id == 2) {
    /* PMT data. */
    int i;
    packet->data[n] = (0x7<<5);	/* Reserved bits */
    if (v) printf("priv->PMT.PCR_PID=%d\n", priv->PMT.PCR_PID);
    packet->data[n++] |= (priv->PMT.PCR_PID >> 8) & 0x1f;
    packet->data[n++] = (priv->PMT.PCR_PID) & 0xff;
    packet->data[n] = (0xf<<4); /* reserved bits */
    /* unused bits unset */
    uint16_t program_descriptors_length = 0;
    packet->data[n++] |= (program_descriptors_length >> 8) & 0x3;
    packet->data[n++] = (program_descriptors_length) & 0xff;

    for (i=0; i < ESSD_MAX; i++) {
      if (v) printf("priv->PMT.ESSD[%d].streamType=%d\n", i, priv->PMT.ESSD[i].streamType);
      packet->data[n++] = priv->PMT.ESSD[i].streamType;
      packet->data[n] = (0x7<<5); /* reserved bits */
      packet->data[n++] |= (priv->PMT.ESSD[i].PID >> 8) & 0x1f;
      packet->data[n++] = (priv->PMT.ESSD[i].PID) & 0xff;
      packet->data[n] = (0xf<<4); /* reserved bits */
      /* unused bits unset */
      uint16_t esinfolen = 0;
      packet->data[n++] |= (esinfolen >> 8) & 0x3;
      packet->data[n++] = (esinfolen) & 0xff;
    }
  }

  /* CRC of everything between pointer field and crc. */
  uint32_t crc = mpegts_crc32(&packet->data[5], n-5);

  packet->data[n++] = (crc >> 24) & 0xff;
  packet->data[n++] = (crc >> 16) & 0xff;
  packet->data[n++] = (crc >> 8) & 0xff;
  packet->data[n++] = (crc >> 0) & 0xff;

  /* Padding. */
  memset(packet->data+n, 0xff, 188-n);

  return packet;
}


static void debug_outputpacket_write(TSPacket * pkt, String * fname)
{
  FILE *f = fopen(s(fname), "wb");
  if (f) {
    if (fwrite(pkt->data, sizeof(pkt->data), 1, f) != 1) { perror("fwrite"); }
    fclose(f);
  }
}


static void flush(Instance *pi, uint64_t flush_timestamp)
{
  /* Write out interleaved video and audio packets, adding PAT and PMT
     packets at least every 100ms or N packets. */
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;
  int i;
  uint64_t pts_now = 0;
  uint64_t pat_pts_last = 0;
  
  if (priv->debug_outpackets && access("outpackets", R_OK) != 0) {
    mkdir("outpackets", 0744);
  }

  /* Sum up the pending AV packets. */
  int av_packets = 0;

  for (i=0; i < MAX_STREAMS; i++) {
    av_packets += priv->streams[i].packet_count;
  }

  if (priv->verbose) { printf("av_packets = %d\n", av_packets); }

  if (av_packets == 0) {
    return;
  }

  if (priv->output_sink) {
    /* flush() always starts a new output segment. */
    Sink_close_current(priv->output_sink);
    Sink_reopen(priv->output_sink);
    String *tmp = String_new(s(priv->output_sink->io.generated_path));
    String_list_add(priv->m3u8_ts_files, &tmp);
    m3u8_files_update(priv);
    priv->m3u8_pts_start = pts_now;
  }

  /* Loop as long as both A and V have some number of packets
     prepared.  Push out one packet with each pass through the loop. */
  while (priv->streams[0].packets || priv->streams[1].packets) {
    /* Find stream with oldest packet. */
    Stream * stream = NULL;
    for (i=0; i < MAX_STREAMS; i++) {
      if (!priv->streams[i].packets) {
	continue;
      }
      if (!stream) {
	stream = &priv->streams[i];
      }
      else if (priv->streams[i].packets->estimated_timestamp < stream->packets->estimated_timestamp) {
	stream = &priv->streams[i];
      }
    }

    if (!stream) {
      fprintf(stderr, "%s:%d sanity check fail, no stream\n", __func__, __LINE__);
      return;
    }

    TSPacket *pkt = stream->packets;

    if (pkt->estimated_timestamp > flush_timestamp) {
      if (priv->verbose) { printf("audio packet(s) will wait for next flush\n"); }
      return;
    }

    /* PAT+PMT should get generated once at the start of a flush,
       which should happen because pat_pts_last is initially zero,
       then regularly during the flush loop.  FIXME:  There is still
       the problem of PTS wrap, because the 33-bit counter can only
       hold ~26 hours at 90KHz, and I'm using a modulo value of the
       real timestamp. */
    if ( (pkt->estimated_timestamp - pat_pts_last) >= (MAXIMUM_PAT_INTERVAL_90KHZ - 1450) ) {
      pat_pts_last = pkt->estimated_timestamp;
      TSPacket *pkt;
      String * fname;

      /* PAT, pid 0 */
      pkt = generate_psi(priv, 0, 0, &priv->PAT.continuity_counter);

      if (priv->debug_outpackets) {
	fname = String_sprintf("outpackets/%05d-ts%04d%s%s", priv->debug_pktseq++, 0,
			       pkt->af ? "-AF" : "" , pkt->pus ? "-PUS" : "");
	debug_outputpacket_write(pkt, fname);
	String_free(&fname);
      }

      if (pi->outputs[OUTPUT_RAWDATA].destination) {
	/* FIXME: Could just assign the packet an avoid another allocation. */
	RawData_buffer * rd = RawData_buffer_new(188); Mem_memcpy(rd->data, pkt->data, sizeof(pkt->data));
	PostData(rd, pi->outputs[OUTPUT_RAWDATA].destination);
      }

      if (priv->output_sink) {
	Sink_write(priv->output_sink, pkt->data, sizeof(pkt->data));
      }

      Mem_free(pkt);
    
      /* PMT, pid 256 */
      pkt = generate_psi(priv, 256, 2, &priv->PMT.continuity_counter);

      if (priv->debug_outpackets) {
	fname = String_sprintf("outpackets/%05d-ts%04d%s%s", priv->debug_pktseq++, 256,
			       pkt->af ? "-AF" : "" , pkt->pus ? "-PUS" : "");
	debug_outputpacket_write(pkt, fname);
	String_free(&fname);
      }

      if (pi->outputs[OUTPUT_RAWDATA].destination) {
	RawData_buffer * rd = RawData_buffer_new(188); Mem_memcpy(rd->data, pkt->data, sizeof(pkt->data));
	PostData(rd, pi->outputs[OUTPUT_RAWDATA].destination);
      }

      if (priv->output_sink) {
	Sink_write(priv->output_sink, pkt->data, sizeof(pkt->data));
      }

      Mem_free(pkt);
    } /* end PAT+PMT generation */


    dpf("flush: et=%" PRIu64 " pid=%d\n", pkt->estimated_timestamp, stream->pid);

    if (priv->debug_outpackets) {
      String * fname;
      fname = String_sprintf("outpackets/%05d-ts%04d%s%s", priv->debug_pktseq++, stream->pid,
			       pkt->af ? "-AF" : "" , pkt->pus ? "-PUS" : "");
      debug_outputpacket_write(pkt, fname);
      String_free(&fname);
    }

    if (pi->outputs[OUTPUT_RAWDATA].destination) {
      RawData_buffer * rd = RawData_buffer_new(188); Mem_memcpy(rd->data, pkt->data, sizeof(pkt->data));
      PostData(rd, pi->outputs[OUTPUT_RAWDATA].destination);
    }

    if (priv->output_sink) {
      Sink_write(priv->output_sink, pkt->data, sizeof(pkt->data));
    }

    stream->packets = stream->packets->next;
    stream->packet_count -= 1;

    Mem_free(pkt);
  }
}


static void MpegTSMux_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  pi->counter++;
}

static void MpegTSMux_instance_init(Instance *pi)
{
  MpegTSMux_private *priv = (MpegTSMux_private *)pi;

  priv->streams[0] = (Stream) {
    .pid = 258, 
    .pts = 1, 
    .dts = 1, 
    .pcr = 1, 
    .es_id = 224,
    .pts_value = 900000,
    .typecode = "V",
  };

  priv->streams[1] = (Stream) {
    .pid = 257,
    .pts = 1,
    .dts = 0,
    .pcr = 0,
    .es_id = 192,
    .pts_value = 900000,
    .typecode = "A",
  };

  priv->debug_outpackets = 0;
  priv->output_count = 5;
  priv->duration = 5;
  priv->m3u8_ts_files = String_list_new();
  priv->media_sequence = 0;
  priv->pcr_lag_ms = 200;
  String_set_local(&priv->index_dir, ".");
}


static Template MpegTSMux_template = {
  .label = "MpegTSMux",
  .priv_size = sizeof(MpegTSMux_private),
  .inputs = MpegTSMux_inputs, 
  .num_inputs = table_size(MpegTSMux_inputs),
  .outputs = MpegTSMux_outputs,
  .num_outputs = table_size(MpegTSMux_outputs),
  .tick = MpegTSMux_tick,
  .instance_init = MpegTSMux_instance_init,
};

void MpegTSMux_init(void)
{
  Template_register(&MpegTSMux_template);
}
