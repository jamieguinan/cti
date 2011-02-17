/*
 * Unpack data from Mpeg2 Transport Streams. 
 * See earlier code "modc/MpegTS.h" for starters.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "MpegTS.h"
#include "SourceSink.h"
#include "Cfg.h"

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
 * ut I don't understand the layers that well yet.
 */
typedef struct _Stream {
  int pid;
  ArrayU8 *data;
  struct _Stream *next;
} Stream;

static Stream * Stream_new(pid)
{
  Stream *s = Mem_calloc(1, sizeof(*s));
  s->pid = pid;
  printf("new pid %d\n", pid);
  return s;
}

static void Streams_add(Stream **ps, uint8_t *packet)
{
  int payloadLen = 188 - 4;
  int payloadOffset = 188 - payloadLen;
  int pid = MpegTS_PID(packet);
  
  if (*ps == NULL) {
    *ps = Stream_new(pid);
  }
  
  Stream *s = *ps;

  while (s) {
    if (pid == s->pid) {
      /* Append data. */
      break;
    }

    if (!s->next) {
      s->next = Stream_new(pid);
      /* loop will roll around and break above on pid match */
    }
    s = s->next;
  }
}



typedef struct {
  char *input;
  Source *source;
  ArrayU8 *chunk;
  String *boundary;
  int state;
  int needData;
  int max_chunk_size;

  int enable;			/* Set this to start processing. */

  uint64_t offset;

  //int use_feedback;
  //int pending_feedback;
  //int feedback_threshold;

  int retry;

  Stream *streams;

} MpegTSDemux_private;


static int set_input(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = pi->data;
  if (priv->input) {
    free(priv->input);
  }
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
  
  priv->input = strdup(value);
  priv->source = Source_new(priv->input);

  if (priv->chunk) {
    ArrayU8_cleanup(&priv->chunk);
  }
  priv->chunk = ArrayU8_new();

  priv->needData = 1;

  return 0;
}


static int set_enable(Instance *pi, const char *value)
{
  MpegTSDemux_private *priv = pi->data;

  priv->enable = atoi(value);

  if (priv->enable && !priv->source) {
    fprintf(stderr, "MpegTSDemux: cannot enable because source not set!\n");
    priv->enable = 0;
  }
  
  printf("MpegTSDemux enable set to %d\n", priv->enable);

  return 0;
}


static Config config_table[] = {
  { "input", set_input, 0L, 0L },
  { "enable", set_enable, 0L, 0L },
  // { "use_feedback", set_use_feedback, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Config_buffer *cb_in = data;
  int i;

  /* Walk the config table. */
  for (i=0; i < table_size(config_table); i++) {
    if (streq(config_table[i].label, cb_in->label->bytes)) {
      int rc;		/* FIXME: What to do with this? */
      rc = config_table[i].set(pi, cb_in->value->bytes);
      break;
    }
  }
  
  Config_buffer_discard(&cb_in);
}

static void analyze_psi(uint8_t *packet)
{

}


static int packetCounter = 0;

static void parse_chunk(MpegTSDemux_private *priv)
{
  uint8_t *packet;

  while (priv->offset < (priv->chunk->len - 188)) {
    packet = priv->chunk->data + priv->offset;
    int payloadLen = 188 - 4;
    int payloadOffset = 188 - payloadLen;

    int pid = MpegTS_PID(packet);

    printf("packet %d, pid %d\n", packetCounter, pid);

    Streams_add(&priv->streams, pid);

    if (MpegTS_sync_byte(packet) != 0x47) {
      printf("invalid packet at offset %ld\n", priv->offset);
      return;
    }
    
    if (MpegTS_TEI(packet)) {
      printf("transport error at offset %ld\n", priv->offset);
      goto end;
    }

    if (MpegTS_PUS(packet)) {
      printf("  payload unit start\n");      
    }

    if (MpegTS_TP(packet)) {
      // printf("  transport priority bit set\n");
    }

    if (pid == 0) {
      analyze_psi(packet);
    }

    if (MpegTS_AFE(packet)) {
      int afLen = MpegTS_AF_len(packet);
      // int optionOffset = 2;
      payloadLen = 188 - 4 - 1 - afLen;
      payloadOffset = 188 - payloadLen;
      printf("  AF length: (1+) %d\n", afLen);
      printf("  payload length: %d\n", payloadLen);
      if (MpegTS_AF_DI(packet)) {
	printf("  discontinuity indicated\n");
      }
      if (MpegTS_AF_RAI(packet)) {
	printf("  random access indicator (new video/audio sequence)\n");
      }
      if (MpegTS_AF_ESPI(packet)) {
	printf("  high priority \n");
      }
      if (MpegTS_AF_PCRI(packet)) {
	printf("  PCR present.  PID %d.", pid);
	printf("   %02x %02x %02x %02x %01x  %02x  %03x\n",
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

	/* FIXME: Add 42 bits, or 6 bytes??? */
      }
      if (MpegTS_AF_OPCRI(packet)) {
	printf("  OPCR present\n");
      }
      if (MpegTS_AF_SP(packet)) {
	printf("  splice point\n");
      }
      if (MpegTS_AF_TP(packet)) {
	printf("  transport private data\n");
      }
      if (MpegTS_AF_AFEXT(packet)) {
	printf("  adapdation field extension\n");
      }

      if (MpegTS_AF_RAI(packet)) {
#if 0
	if (pid == 5521 && !vidOut) {
	  vidOut = File.open_write(String.sprintf_z("%d.out", pid));
	  Mem.simple_ref(&vidOut->mo);
	}
	if (!allOut) {
	  allOut = File.open_write(String.new("all.out"));
	  Mem.simple_ref(&allOut->mo);
	}
	if (!mpegOut) {
	  mpegOut = File.open_write(String.new("mpeg.out"));
	  Mem.simple_ref(&mpegOut->mo);
	}
#endif
      }
    }

    if (MpegTS_PDE(packet)) {
#if 0      
      if (mpegOut && pid == 5521) {
	File.write_data(mpegOut, packet+payloadOffset, payloadLen);
      }
#endif
    }
    else {
      printf("  no payload data\n");
    }

#if 0
    if (pid == 5521 && vidOut) {
      // File.write_data(vidOut, packet, 188);
    }

    if (allOut) {
      File.write_data(allOut, packet, 188);
    }
#endif

  end:
    packet += 1;
    priv->offset += 188;
    packetCounter += 1;
  }

}


static void MpegTSDemux_tick(Instance *pi)
{
  MpegTSDemux_private *priv = pi->data;
  Handler_message *hm;
  int sleep_and_return = 0;

  hm = GetData(pi, 0);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
  }

  if (!priv->enable) {
    sleep_and_return = 1;
  }
  else { 
    if (!priv->source) {
      fprintf(stderr, "MpegTSDemux enabled, but no source set!\n");
      priv->enable = 0;
      sleep_and_return = 1;
    }
  }

  if (sleep_and_return) {
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = (999999999+1)/100}, NULL);
    return;
  }

  if (priv->needData) {
    ArrayU8 *newChunk;
    /* Network reads should return short numbers if not much data is
       available, so using a size relatively large comparted to an
       audio buffer or Jpeg frame should not cause real-time playback
       to suffer here. */
    newChunk = Source_read(priv->source, 32768);

    //int vsave = cfg.verbosity; cfg.verbosity = 1;
    if (newChunk && cfg.verbosity) { 
      fprintf(stderr, "got %d bytes\n", newChunk->len);
    }
    //cfg.verbosity = vsave;

    if (!newChunk) {
      /* FIXME: EOF on a local file should be restartable.  Maybe
	 socket sources should be restartable, too. */
      Source_close_current(priv->source);
      fprintf(stderr, "%s: source finished.\n", __func__);
      if (priv->retry) {
	fprintf(stderr, "%s: retrying.\n", __func__);
	sleep(1);
	Source_free(&priv->source);
	priv->source = Source_new(priv->input);
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
  MpegTSDemux_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
}


static Template MpegTSDemux_template = {
  .label = "MpegTSDemux",
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
