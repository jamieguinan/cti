/*
 * Unpack data from Mpeg2 Transport Streams.
 * See earlier code "modc/MpegTS.h" for starters.
 */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */
#include <unistd.h>		/* sleep */
#include <inttypes.h>

#include "CTI.h"
#include "MpegTSDemux.h"
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
  printf("new pid %d\n", pid);
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

  uint64_t offset;

  //int use_feedback;
  //int pending_feedback;
  //int feedback_threshold;

  int retry;

  Stream *streams;

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
  // { "use_feedback", set_use_feedback, 0L, 0L },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void Streams_add(MpegTSDemux_private *priv, uint8_t *packet)
{
  int payloadLen = 188 - 4;
  int payloadOffset = 188 - payloadLen;
  int pid = MpegTS_PID(packet);
  int afLen = 0;
  int isPUS = 0;

  int save_verbosity = cfg.verbosity;

  if (priv->filter_pid && priv->filter_pid != pid) {
    cfg.verbosity = 0;
  }

  if (cfg.verbosity) { 
    printf("packet %d, pid %d\n", packetCounter, pid);
  }
  
  if (priv->streams == NULL) {
    priv->streams = Stream_new(pid);
  }
  
  Stream *s = priv->streams;

  while (s) {
    if (pid == s->pid) {
      /* Append data. */

      if (MpegTS_sync_byte(packet) != 0x47) {
	printf("invalid packet at offset %" PRIu64 "\n", priv->offset);
	return;
      }
    
      if (MpegTS_TEI(packet)) {
	printf("transport error at offset %" PRIu64 "\n", priv->offset);
	return;
      }

      if (MpegTS_PUS(packet)) {
	isPUS = 1;
	/* Flush, free, reallocate. */
	if (s->data) {
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

	    if (cfg.verbosity) { 
	      /* NOTE: This is peeking down into the PES layer, and is only 
		 written to handle one specific example case. */

	      if (pid > 256) { 
		printf("    [PES] previous payload PTS: %d%d%d%d (%d) %d",
		       (s->data->data[9] >> 7) & 1,
		       (s->data->data[9] >> 6) & 1,
		       (s->data->data[9] >> 5) & 1,
		       (s->data->data[9] >> 4) & 1,
		       (s->data->data[9] >> 1) & 7,
		       (s->data->data[9] >> 0) & 1);

		printf(" %" PRIu64 "", MpegTS_PTS(s->data->data + 9));

		printf("\n");
	      }

	      if (pid == 258) { 
		printf("    [PES] previous payload DTS: %d%d%d%d (%d) %d",
		       (s->data->data[14] >> 7) & 1,
		       (s->data->data[14] >> 6) & 1,
		       (s->data->data[14] >> 5) & 1,
		       (s->data->data[14] >> 4) & 1,
		       (s->data->data[14] >> 1) & 7,
		       (s->data->data[14] >> 0) & 1);

		printf(" %" PRIu64, MpegTS_PTS(s->data->data + 14));

		printf("\n");
	      }
	    }


	  } /* if (s->data->len) */
	  s->seq += 1;
	  ArrayU8_cleanup(&s->data);
	} /* if (s->data) */

	if (cfg.verbosity) printf("  payload unit start\n");
	s->data = ArrayU8_new();
      }

      if (MpegTS_TP(packet)) {
	if (cfg.verbosity) printf("  transport priority bit set\n");
      }

      if (MpegTS_AFE(packet)) {
	afLen = MpegTS_AF_len(packet);
	// int optionOffset = 2;
	payloadLen = 188 - 4 - 1 - afLen;
	payloadOffset = 188 - payloadLen;
	if (cfg.verbosity) printf("  AF present, length: (1+) %d\n", afLen);

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
	  if (cfg.verbosity) printf("  discontinuity indicated\n");
	}
	if (MpegTS_AF_RAI(packet)) {
	  if (cfg.verbosity) printf("  random access indicator (new video/audio sequence)\n");
	}
	if (MpegTS_AF_ESPI(packet)) {
	  if (cfg.verbosity) printf("  high priority \n");
	}
	if (MpegTS_AF_PCRI(packet)) {
	  if (0 && cfg.verbosity) printf("  PCR present:  %02x %02x %02x %02x %01x  %02x  %03x\n",
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

	  if (cfg.verbosity) {
	    printf("  PCR present:  [%02x] 90kHz:%d",
		   /* 33 bits, 90KHz */
		   packet[6],
		   (packet[6] << 25) | (packet[7] << 17) | (packet[8] << 9) | (packet[9] << 1) | (packet[10] >> 7));
	    /* 6 bits "reserved", see iso13818-1.pdf Table 2-6 */
	    printf(" res:%d", (packet[10] >> 1) & 0x3f);
	    /* 9 bits, 27MHz */
	    printf(" [%02x, %02x] 27MHz:%d\n", packet[10], packet[11], ((packet[10] & 1) << 8) | packet[11]);
	  }
	}
	if (MpegTS_AF_OPCRI(packet)) {
	  if (cfg.verbosity) printf("  OPCR present\n");
	}
	if (MpegTS_AF_SP(packet)) {
	  if (cfg.verbosity) printf("  splice point\n");
	}
	if (MpegTS_AF_TP(packet)) {
	  if (cfg.verbosity) printf("  transport private data\n");
	}
	if (MpegTS_AF_AFEXT(packet)) {
	  if (cfg.verbosity) printf("  adapdation field extension\n");
	}

      xx:

	if (cfg.verbosity) printf("  remaining payload length: %d\n", payloadLen);
      }

      if (MpegTS_PDE(packet)) {
	if (!s->data) {
	  s->data = ArrayU8_new();
	}
	ArrayU8_append(s->data, ArrayU8_temp_const(packet+payloadOffset, payloadLen));
      }
      else {
	if (cfg.verbosity) printf("  no payload data\n");
      }
      
      break;
    }

    if (!s->next) {
      s->next = Stream_new(pid);
      /* loop will roll around and break above on pid match */
    }
    s = s->next;
  }


  {
    char filename[256];
    sprintf(filename, "tmp/%05d-ts%04d-%03d%s%s", packetCounter, pid, afLen, afLen ? "-AF" : "", isPUS ? "-PUS" : "");
    FILE *f = fopen(filename, "wb");
    if (f) {
      int n = fwrite(packet, 188, 1, f);
      if (!n) perror("fwrite");
      fclose(f);
    }
  }


  cfg.verbosity = save_verbosity;
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
	priv->source = Source_new(sl(priv->input));
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
