/*
 * MpegTSDemux:
 *
 * Unpack data from Mpeg2 Transport Streams, with options to print
 * information to stdout, and to save TS and ES packets in individual
 * files.  Started from earlier code "modc/MpegTS.h".  References,
 * 
 *   https://en.wikipedia.org/wiki/MPEG_transport_stream
 *   https://en.wikipedia.org/wiki/Program_Specific_Information
 *   https://en.wikipedia.org/wiki/Packetized_Elementary_Stream
 *   https://en.wikipedia.org/wiki/Elementary_stream
 *   file:///home/guinan/projects/video/iso13818-1.pdf
 *   
 * This was written as an exercise and testing tool, so that
 * MpegTSMux.c (not Demux) could be developed in parallel doing the
 * inverse of what this module does.  But I think it could be made
 * into a useful TS demuxer by defining some outputs and passing along
 * the elementary stream data chunks.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep, access */
#include <sys/stat.h>		/* mkdir */
#include <sys/types.h>		/* mkdir */
#include <inttypes.h>

#include "CTI.h"
#include "MpegTSDemux.h"
#include "MpegTS.h"
#include "SourceSink.h"
#include "Cfg.h"

static const char *stream_type_strings[256] = {
  [0x0f] = "AAC",
  [0x1b] = "H.264",
};


static const char *stream_type_string(int index)
{
  const char * result = stream_type_strings[index];
  if (result == NULL) {
    result = "unknown";
  }
  return result;
}


static void Config_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG };
static Input MpegTSDemux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  // [ INPUT_FEEDBACK ] = { .type_label = "Feedback_buffer", .handler = Feedback_handler },
};

//enum { /* OUTPUT_... */ };
static Output MpegTSDemux_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};


/*
 * "Stream", maybe a "Packetized elementary stream", 
 *
 *    http://en.wikipedia.org/wiki/Packetized_elementary_stream
 *
 *  but I don't understand the layers that well yet.
 */
typedef struct _Stream {
  int pid;
  ArrayU8 *data;
  int seq;
  struct _Stream *next;
} Stream;

static Stream * Stream_new(pid)
{
  Stream *s = Mem_calloc(1, sizeof(*s));
  s->pid = pid;
  printf("\n[new pid %d]\n", pid);
  return s;
}

static int packetCounter = 0;

typedef struct {
  Instance i;
  String input;
  Source *source;
  ArrayU8 *chunk;
  int needData;

  int enable;			/* Set this to start processing. */
  int filter_pid;
  
  int pmt_id;			/* Program Map Table ID */

  uint64_t offset;

  //int use_feedback;
  //int pending_feedback;
  //int feedback_threshold;

  int retry;
  int exit_on_eof;

  Stream *streams;

  /* Verbose printfs */
  struct {
    int print_packet;
    int show_pes;
    int PUS;
    int AF;
    int payload;
    int cc;
    int PAT;
    int PMT;
    int miscbits;
  } v;

  /* Debug */
  struct {
    int tspackets;
    int espackets;
  } d;
  
} MpegTSDemux_private;


static int set_input(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;
  if (priv->source) {
    Source_free(&priv->source);
  }

  if (value[0] == '$') {
    value = getenv(value+1);
    if (!value) {
      fprintf(stderr, "env var %s is not set\n", value+1);
      return 1;
    }
  }

  String_set(&priv->input, value);
  priv->source = Source_new(sl(priv->input));

  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->needData = 1;

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "MpegTSDemux: cannot enable because source not set!\n");
    priv->enable = 0;
  }
  
  printf("MpegTSDemux enable set to %d\n", priv->enable);

  return 0;
}


static int set_filter_pid(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;

  priv->filter_pid = atoi(value);

  printf("MpegTSDemux filter_pid set to %d\n", priv->filter_pid);

  return 0;
}


static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  { "filter_pid", set_filter_pid, 0L, 0L },
  { "exit_on_eof", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, exit_on_eof) },
  // { "use_feedback", set_use_feedback, 0L, 0L },
  { "v.print_packet", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, v.print_packet) },
  { "v.show_pes", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, v.show_pes) },
  { "v.PUS", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, v.PUS) },
  { "v.AF", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, v.AF) },
  { "v.PAT", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, v.PAT) },
  { "v.PMT", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, v.PMT) },

  { "d.tspackets", 0L, 0L, 0L, cti_set_int, offsetof(MpegTSDemux_private, d.tspackets) },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void print_packet(uint8_t * packet)
{
  int i = 0;
  printf("   ");
  while (i < 188) {
    printf(" %02x", packet[i]);
    i++;
    if (i % 16 == 0) {
      printf("\n   ");
    }
  }

  if (i % 16 != 0) {
    printf("\n");
  }
}


static void handle_pes(MpegTSDemux_private *priv, Stream *s)
{
  uint8_t *edata = s->data->data;
  int i;


  if (priv->v.show_pes) { printf("    [completed PES packet from earlier TS packets]\n"); }
  if (!(edata[0]==0 && edata[1]==0 && edata[2]==1)) {
    printf("    *** bad prefix [%02x %02x %02x]\n",
	   edata[0], edata[1], edata[2]);
    return;
  }

  int ppl = (edata[4]<<8)|(edata[5]);

  if (priv->v.show_pes) {
    printf("    Prefix 000001 OK\n");
    printf("    Final PES unit length %d\n", s->data->len);
    printf("    Stream id %d\n", edata[3]);
    printf("    PES Packet length %d%s\n", ppl, (ppl?"":" (unspecified)"));
  }

  if (ppl) {
    /* Note, ppl can be 0 for video streams */
    if (ppl != s->data->len) {
      // 
    }
  }
  
  if (priv->v.show_pes) {
    printf("    Marker bits: %d\n", ((edata[6]>>6)&0x3));
    printf("    Scramblig control: %d\n", ((edata[6]>>4)&0x3));
    printf("    Priority: %d\n", ((edata[6]>>3)&0x1));
    printf("    Data alignment indicator: %d\n", ((edata[6]>>2)&0x1));
    printf("    Copyright: %d\n", ((edata[6]>>1)&0x1));
    printf("    Original: %d\n", ((edata[6]>>0)&0x1));
  }

  int pts = ((edata[7]>>6)&0x2);
  int dts = ((edata[7]>>6)&0x1);
  if (!pts && dts) {
    printf("    *** bad PTS DTS value 1\n");
  }
  if (pts) {
    if (priv->v.show_pes) { printf("    PTS present\n"); }
  }
  if (dts) {
    if (priv->v.show_pes) { printf("    DTS present\n"); }
  }

  if (priv->v.show_pes) { 
    printf("    ESCR: %d\n", ((edata[7]>>5)&0x1));
    printf("    ES rate: %d\n", ((edata[7]>>4)&0x1));
    printf("    DSM trick: %d\n", ((edata[7]>>3)&0x1));
    printf("    Addl copy info: %d\n", ((edata[7]>>2)&0x1));
    printf("    CRC: %d\n", ((edata[7]>>1)&0x1));
    printf("    extension: %d\n", ((edata[7]>>0)&0x1));
    printf("    remaining header: %d\n", edata[8]);
  }

  int n = 9;
  if (pts) {
    if (priv->v.show_pes) {
      printf("    PTS bytes %02x %02x %02x %02x %02x\n",
	     edata[n], edata[n+1], edata[n+2], edata[n+3],edata[n+4]);
    }
    uint64_t value = 0;
    /* 00[01][01]NNN[1] */
    value = (edata[n]>>1)&0x7;
    value <<= 30;
    value |= ((uint64_t)edata[n+1] << 22);
    value |= ((uint64_t)(edata[n+2]>>1) << 15);
    value |= ((uint64_t)edata[n+3] << 7);
    value |= ((uint64_t)edata[n+4] >> 1);
    if (priv->v.show_pes) { printf("    PTS value: %" PRIu64 "\n", value); }
    n += 5;

    if (dts) {
      if (priv->v.show_pes) {
	printf("    DTS bytes %02x %02x %02x %02x %02x\n",
	       edata[n],edata[n+1], edata[n+2], edata[n+3],edata[n+4]);
      }
      value = (edata[n]>>5)&0x7;
      value <<= 30;
      value |= ((uint64_t)edata[n+1] << 22);
      value |= ((uint64_t)(edata[n+2]>>1) << 15);
      value |= ((uint64_t)edata[n+3] << 7);
      value |= ((uint64_t)edata[n+4] >> 1);
      if (priv->v.show_pes) { printf("    DTS value: %" PRIu64 "\n", value); }
      n += 5;
    }
  }

  int elementary_payload_bytes = (s->data->len - n);
  if (priv->v.show_pes) {
    printf("    remaining bytes: %d\n", elementary_payload_bytes);
    for (i=0; i < 4 && i < elementary_payload_bytes; i++) {
      printf("      [%02x]\n", edata[n+i]);
    }
  }

  if (priv->d.espackets) {
    char name[64];
    sprintf(name, "%05d-es-%04d-%04d", packetCounter, s->pid, s->seq);
    FILE *f = fopen(name, "wb");
    if (f) {
      if (fwrite(edata+n, elementary_payload_bytes, 1, f) != 1) {
	perror("fwrite");
      }
      fclose(f);
    }
  }

  if (priv->v.show_pes) { printf("    [end completed PES packet]\n"); }
}


static void Streams_add(MpegTSDemux_private *priv, uint8_t *packet)
{
  int payloadLen = 188 - 4;
  int payloadOffset = 188 - payloadLen;
  int pid = MpegTS_PID(packet);
  int afLen = 0;
  int isPUS = 0;

  if (priv->filter_pid && priv->filter_pid != pid) {
    // cfg.verbosity = 0;
  }

  if (priv->streams == NULL) {
    priv->streams = Stream_new(pid);
  }
  
  Stream *s = priv->streams;

  while (s) {
    if (pid == s->pid) {
      break;
    }
    if (!s->next) {
      s->next = Stream_new(pid);
      /* loop will roll around and break above on pid match */
    }
    s = s->next;
  }

  if (priv->v.print_packet) {
    printf("\npacket %d, pid %d\n", packetCounter, pid);
    print_packet(packet);
  }

  if (MpegTS_sync_byte(packet) != 0x47) {
    fprintf(stderr, "invalid packet at offset %" PRIu64 "\n", priv->offset);
    return;
  }
    
  if (MpegTS_TEI(packet)) {
    fprintf(stderr, "transport error at offset %" PRIu64 "\n", priv->offset);
    return;
  }

  if (MpegTS_PUS(packet)) {
    if (priv->v.PUS) {
      printf("  payload unit start (PES or PSI)\n");
    }
    isPUS = 1;
    if (s->data) {
      /* Flush, free, reallocate. */
      if (s->data->len) {
	char name[32];
	FILE *f;
	sprintf(name, "%05d-%04d.%04d", packetCounter, s->pid, s->seq);
	f = fopen(name, "wb");
	if (f) {
	  int n = fwrite(s->data->data, s->data->len, 1, f);
	  if (n != 1) { perror(name); }
	  fclose(f);
	  f = NULL;
	}
	
	if (pid != 0 && pid != priv->pmt_id) { 
	  handle_pes(priv, s);
	}
	
      } /* if (s->data->len) */
      s->seq += 1;
      ArrayU8_cleanup(&s->data);
    } /* if (s->data) */
    s->data = ArrayU8_new();
  }
  
  if (MpegTS_TP(packet)) {
    if (priv->v.miscbits) { printf("  transport priority bit set\n"); }
  }
  
  if (MpegTS_SC(packet)) {
    int bits = MpegTS_SC(packet);
    if (priv->v.miscbits) printf("  scrambling control enabled (bits %d%d)\n",
			      (bits>>1), (bits&1));
  }
  
  if (MpegTS_AFE(packet)) {
    afLen = MpegTS_AF_len(packet);
    // int optionOffset = 2;
    payloadLen = 188 - 4 - 1 - afLen;
    payloadOffset = 188 - payloadLen;
    if (priv->v.AF) printf("  AF present, length: (1+) %d\n", afLen);
    
    if (afLen == 0) {
      /* I've observed this while analyzing the Apple TS dumps.  I
	 treat it such that the remaining payload is 183 byte, so
	 the AF length field occupies the one byte, and the
	 additional flags are left out!  Which implies they are
	 optional, at least in this case, which is not indicated by
	 the Wikipedia page. */
      goto xx;
    }

    if (MpegTS_AF_DI(packet)) {
      if (priv->v.AF) printf("  discontinuity indicated\n");
    }
    if (MpegTS_AF_RAI(packet)) {
      if (priv->v.AF) printf("  random access indicator (new video/audio sequence)\n");
    }
    if (MpegTS_AF_ESPI(packet)) {
      if (priv->v.AF) printf("  high priority \n");
    }
    if (MpegTS_AF_PCRI(packet)) {
      if (priv->v.AF) printf("  PCR present:  %02x %02x %02x %02x %01x  %02x  %03x\n",
			     /* 33 bits, 90KHz */
			     packet[6],
			     packet[7],
			     packet[8],
			     packet[9],
			     packet[10] >> 7,
			     /* 6 bits "reserved", see iso13818-1.pdf Table 2-6 */
			     (packet[10] >> 1) & 0x3f,
			     /* 9 bits, 27MHz */
			     ((packet[10] & 1) << 8) | packet[11]);

      if (priv->v.AF) {
	uint64_t pcr = ((uint64_t)packet[6] << 25) \
	  | (packet[7] << 17) | (packet[8] << 9) | (packet[9] << 1) | ((packet[10] >> 7) & 1);
	printf("  PCR present: %" PRIu64 " @90kHz", pcr);
	/* 6 bits "reserved", see iso13818-1.pdf Table 2-6 */
	printf(" res:%d", (packet[10] >> 1) & 0x3f);
	/* 9 bits, 27MHz */
	printf(" 27MHz:%d\n", ((packet[10] & 1) << 8) | packet[11]);
      }
    }
    if (MpegTS_AF_OPCRI(packet)) {
      if (priv->v.AF) printf("  OPCR present\n");
    }
    if (MpegTS_AF_SP(packet)) {
      if (priv->v.AF) printf("  splice point\n");
    }
    if (MpegTS_AF_TP(packet)) {
      if (priv->v.AF) printf("  transport private data\n");
    }
    if (MpegTS_AF_AFEXT(packet)) {
      if (priv->v.AF) printf("  adapdation field extension\n");
    }
    
  xx:;
    
  }	/* AFE... */
  
  
  if (MpegTS_PDE(packet)) {
    if (priv->v.payload) printf("  payload data present, length %d\n", payloadLen);
    if (!s->data) {
      s->data = ArrayU8_new();
    }
    ArrayU8_append(s->data, ArrayU8_temp_const(packet+payloadOffset, payloadLen));
  }
  else {
    if (priv->v.payload) printf("  no payload data\n");
  }
  
  int cc = MpegTS_CC(packet);
  if (priv->v.cc) { printf("  continuity counter=%d\n", cc); }
  
  /* http://en.wikipedia.org/wiki/Program_Specific_Information */
  if (pid == 0) {
    int pointer_filler_bytes = MpegTS_PSI_PTRFIELD(packet);
    uint8_t * table_header = MpegTS_PSI_TABLEHDR(packet);
    int table_id = MpegTS_PSI_TABLE_ID(table_header);
    int slen = MpegTS_PSI_TABLE_SLEN(table_header);
    if (priv->v.PAT) {
      printf("  PSI: PAT (Program Association Table)\n");
      printf("  (%d pointer filler bytes)\n", pointer_filler_bytes);
      printf("  Table ID %d\n", table_id);
      printf("  Section syntax indicator: %d\n", MpegTS_PSI_TABLE_SSI(table_header));
      printf("  Private bit: %d\n", MpegTS_PSI_TABLE_PB(table_header));
      printf("  Reserved bits: %d\n", MpegTS_PSI_TABLE_RBITS(table_header));
      printf("  Section length unused bits: %d\n", MpegTS_PSI_TABLE_SUBITS(table_header));
      printf("  Section length: %d\n", slen);
    }

    if (MpegTS_PSI_TABLE_SSI(table_header)) {
      uint8_t * tss = MpegTS_PSI_TSS(table_header);
      if (priv->v.PAT) {
	printf("    Table ID extension: %d\n", MpegTS_PSI_TSS_ID_EXT(tss));
	printf("    Reserved bits: %d\n", MpegTS_PSI_TSS_RBITS(tss));
	printf("    Version number: %d\n", MpegTS_PSI_TSS_VERSION(tss));
	printf("    current/next: %d\n", MpegTS_PSI_TSS_CURRNEXT(tss));
	printf("    Section number: %d\n", MpegTS_PSI_TSS_SECNO(tss));
	printf("    Last section number: %d\n", MpegTS_PSI_TSS_LASTSECNO(tss));
      }
	  
      uint8_t * pasd = MpegTS_PAT_PASD(tss);
      uint32_t crc;
      int remain = 
	slen 
	- 5 /* PSI_TSS */
	- sizeof(crc);
      while (remain > 0) {
	priv->pmt_id = MpegTS_PAT_PASD_PMTID(pasd);
	if (priv->v.PAT) {
	  printf("      Program number: %d\n", MpegTS_PAT_PASD_PROGNUM(pasd));
	  printf("      Reserved bits: %d\n", MpegTS_PAT_PASD_RBITS(pasd));
	  printf("      PMT ID: %d\n", priv->pmt_id);
	}
	pasd += 4;
	remain -= 4;
      }
      crc = (pasd[0]<<24)|(pasd[1]<<16)|(pasd[2]<<8)|(pasd[3]<<0);
      if (priv->v.PAT) { printf("      crc: (supplied:calculated) 0x%08x:0x????????\n", crc); }
    }
  }

  else if (priv->pmt_id != 0 && pid == priv->pmt_id) {
    int pointer_filler_bytes = MpegTS_PSI_PTRFIELD(packet);
    uint8_t * table_header = MpegTS_PSI_TABLEHDR(packet);
    int table_id = MpegTS_PSI_TABLE_ID(table_header);
    int slen = MpegTS_PSI_TABLE_SLEN(table_header);
    if (priv->v.PMT) {
      printf("  PSI: PMT (Program Map Table)\n");
      printf("  (%d pointer filler bytes)\n", pointer_filler_bytes);
      printf("  Table ID %d\n", table_id);
      printf("  Section syntax indicator: %d\n", MpegTS_PSI_TABLE_SSI(table_header));
      printf("  Private bit: %d\n", MpegTS_PSI_TABLE_PB(table_header));
      printf("  Reserved bits: %d\n", MpegTS_PSI_TABLE_RBITS(table_header));
      printf("  Section length unused bits: %d\n", MpegTS_PSI_TABLE_SUBITS(table_header));
      printf("  Section length: %d\n", slen);
    }

    if (MpegTS_PSI_TABLE_SSI(table_header)) {
      uint8_t * tss = MpegTS_PSI_TSS(table_header);
      uint8_t * pmsd = MpegTS_PMT_PMSD(tss);
      if (priv->v.PMT) {
	printf("    Table ID extension: %d\n", MpegTS_PSI_TSS_ID_EXT(tss));
	printf("    Reserved bits: %d\n", MpegTS_PSI_TSS_RBITS(tss));
	printf("    Version number: %d\n", MpegTS_PSI_TSS_VERSION(tss));
	printf("    current/next: %d\n", MpegTS_PSI_TSS_CURRNEXT(tss));
	printf("    Section number: %d\n", MpegTS_PSI_TSS_SECNO(tss));
	printf("    Last section number: %d\n", MpegTS_PSI_TSS_LASTSECNO(tss));
	
	printf("      [--Program Map Specific data]\n");
	printf("      Reserved bits: 0x%x\n", MpegTS_PMT_PMSD_RBITS(pmsd));
	printf("      PCR PID: %d\n", MpegTS_PMT_PMSD_PCRPID(pmsd));
	printf("      Reserved bits 2: 0x%x\n", MpegTS_PMT_PMSD_RBITS2(pmsd));
	printf("      Unused bits: %d\n", MpegTS_PMT_PMSD_UNUSED(pmsd));
	printf("      Program descriptors length: %d\n", MpegTS_PMT_PMSD_PROGINFOLEN(pmsd));
      }
      
      uint8_t * essd = MpegTS_PMT_ESSD(pmsd);
      uint32_t crc;
      int remain = 
	slen 
	- 5 /* PSI_TSS */
	- 4 /* PMT_PMSD */
	- MpegTS_PMT_PMSD_PROGINFOLEN(pmsd)
	- sizeof(crc);
      while (remain > 0) {
	if (priv->v.PMT) {
	  printf("      [--Elementary stream specific data]\n");
	  printf("      Stream type: 0x%02x (%s)\n", 
		 MpegTS_ESSD_STREAMTYPE(essd),
		 stream_type_string(MpegTS_ESSD_STREAMTYPE(essd)));
	  printf("      Reserved bits: %d\n", MpegTS_ESSD_RBITS1(essd));
	  printf("      PID: %d\n", MpegTS_ESSD_PID(essd));
	  printf("      Reserved bits2: %d\n", MpegTS_ESSD_RBITS2(essd));
	  printf("      Unused: %d\n", MpegTS_ESSD_UNUSED(essd));
	  printf("      Stream descriptors length: %d\n", MpegTS_ESSD_DESCRIPTORSLENGTH(essd));
	}
	int n = 5 + MpegTS_ESSD_DESCRIPTORSLENGTH(essd);
	remain -= n;
	essd += n;
      }
	  
      crc = (essd[0]<<24)|(essd[1]<<16)|(essd[2]<<8)|(essd[3]<<0);
      if (priv->v.PMT) { printf("      crc: (supplied:calculated) 0x%08x:0x????????\n", crc); }
    }
  }


  if (priv->d.tspackets) {
    char filename[256];
    if (access("tspackets", R_OK) != 0) {
      mkdir("tspackets", 0744);
    }
    sprintf(filename, "tspackets/%05d-ts%04d-%03d%s%s", packetCounter, pid, afLen, afLen ? "-AF" : "", isPUS ? "-PUS" : "");
    FILE *f = fopen(filename, "wb");
    if (f) {
      int n = fwrite(packet, 188, 1, f);
      if (!n) perror("fwrite");
      fclose(f);
    }
  }
}


static void parse_chunk(MpegTSDemux_private *priv)
{
  uint8_t *packet;

  while (priv->offset < (priv->chunk->len - 188)) {
    packet = priv->chunk->data + priv->offset;

    Streams_add(priv, packet);

    priv->offset += 188;
    packetCounter += 1;
  }

}


static void MpegTSDemux_tick(Instance *pi)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;
  Handler_message *hm;
  int wait_flag;

  if (!priv->enable || !priv->source) {
    wait_flag = 1;
  }
  else {
    wait_flag = 0;
  }

  hm = GetData(pi, wait_flag);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm,pi);
  }

  if (!priv->enable || !priv->source) {
    return;
  }

  if (priv->needData) {
    ArrayU8 *newChunk;
    newChunk = Source_read(priv->source);

    if (newChunk && cfg.verbosity) { 
      fprintf(stderr, "got %d bytes\n", newChunk->len);
    }

    if (!newChunk) {
      /* FIXME: EOF on a local file should be restartable.  Maybe
	 socket sources should be restartable, too. */
      Source_close_current(priv->source);
      fprintf(stderr, "%s: source finished.\n", __func__);
      if (priv->retry) {
	fprintf(stderr, "%s: retrying.\n", __func__);
	sleep(1);
	Source_free(&priv->source);
	priv->source = Source_new(sl(priv->input));
      }
      else if (priv->exit_on_eof) {
	exit(0);
      }
      else {
	priv->enable = 0;
      }
      return;
    }

    ArrayU8_append(priv->chunk, newChunk);
    ArrayU8_cleanup(&newChunk);
    parse_chunk(priv);
    // if (cfg.verbosity) { fprintf(stderr, "needData = 0\n"); }
    // priv->needData = 0;

  }

  /* trim consumed data from chunk, reset "current" variables. */
  ArrayU8_trim_left(priv->chunk, priv->offset);
  priv->offset = 0;
  pi->counter += 1;
}

static void MpegTSDemux_instance_init(Instance *pi)
{
  MpegTSDemux_private *priv = (MpegTSDemux_private *)pi;
  priv->filter_pid = -1;
  cfg.verbosity = 1;
}


static Template MpegTSDemux_template = {
  .label = "MpegTSDemux",
  .priv_size = sizeof(MpegTSDemux_private),
  .inputs = MpegTSDemux_inputs,
  .num_inputs = table_size(MpegTSDemux_inputs),
  .outputs = MpegTSDemux_outputs,
  .num_outputs = table_size(MpegTSDemux_outputs),
  .tick = MpegTSDemux_tick,
  .instance_init = MpegTSDemux_instance_init,
};

void MpegTSDemux_init(void)
{
  Template_register(&MpegTSDemux_template);
}
