/* Mpeg TS/PES muxer.  This might also handle generating sequential files and indexes. */
#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* calloc */
#include <string.h>		/* memcpy */

#include "Template.h"
#include "MpegTSMux.h"
#include "MpegTS.h"
#include "Images.h"
#include "Audio.h"
#include "Array.h"

#define _min(a, b)  ((a) < (b)?  (a):(b))

static void Config_handler(Instance *pi, void *msg);
static void H264_handler(Instance *pi, void *msg);
static void AAC_handler(Instance *pi, void *msg);

enum { INPUT_CONFIG, INPUT_H264, INPUT_AAC };
static Input MpegTSMux_inputs[] = {
  [ INPUT_CONFIG ] = { .type_label = "Config_msg", .handler = Config_handler },
  [ INPUT_H264 ] = { .type_label = "H264_buffer", .handler = H264_handler },
  [ INPUT_AAC ] = { .type_label = "AAC_buffer", .handler = AAC_handler },
};

//enum { /* OUTPUT_... */ };
static Output MpegTSMux_outputs[] = {
  //[ OUTPUT_... ] = { .type_label = "", .destination = 0L },
};


typedef struct _ts_packet {
  uint8_t data[188];
  struct _ts_packet *next;
} TSPacket;


typedef struct {
  // TS file format string.
  String *chunk_fmt;
  // TS sequence duration in seconds.
  int chunk_duration;

  FILE *chunk_file;

  // Index file format string.
  String *index_fmt;

  TSPacket *packets;
  TSPacket *last_packet;

  uint32_t continuity_counter;

  time_t tv_sec_offset;
} MpegTSMux_private;


static Config config_table[] = {
  // { "...",    set_..., get_..., get_..._range },
};


static void Config_handler(Instance *pi, void *data)
{
  Generic_config_handler(pi, data, config_table, table_size(config_table));
}


static void H264_handler(Instance *pi, void *msg)
{
  MpegTSMux_private *priv = pi->data;
  H264_buffer *h264 = msg;
  MpegTimeStamp pts = {};
  MpegTimeStamp dts = {};
  uint16_t pid = 258;
  uint64_t tv90k;

  /* Map timestamp to 33-bit 90KHz units. */
  tv90k = ((h264->tv.tv_sec - priv->tv_sec_offset) * 90000) + (h264->tv.tv_usec * 9 / 100);

  /* Assign to PTS, and same to DTS. */
  pts.set = 1;
  pts.hi_bit = ((tv90k/2) >> 31 & 1);
  pts.value = (tv90k & 0xFFFFFFFF);

  dts.set = 1;
  dts.hi_bit = pts.hi_bit;
  dts.value = pts.value;

  if (1) {
    /* Debug testing... */
    static FILE *f;
    if (!f) {
      f = fopen("out.264", "wb");
    }
    if (fwrite(h264->data, h264->data_length, 1, f) != 1) {
      perror("fwrite");
    }
    else {
      printf("wrote %d bytes\n", h264->data_length);
    }
  }

  /* Wrap the data in a PES packet (prepend a chunk of header data),
     then divide into TS packets, and either write out, or save in a
     list so they can be smoothly interleaved with audio packets.  See
     "demo-ts-analysis.txt".  */
  ArrayU8 *pes = ArrayU8_new();
  uint8_t stream_id = 0xe0;
  ArrayU8_append_bytes(pes, 
		       0x00, 0x00, 0x01, /* PES packet start code prefix. */
		       stream_id,	 /* Stream id */
		       0x00, 0x00 /* PES packet length (unset) */
		       );

  if (1 /* extension present, depending on Stream id */) {
    ArrayU8_append_bytes(pes,
			 0x84,	/* '10', PES priority bit set, various flags unset. */
			 0xc0,	/* '11' = PTS+DTS present, various flags unset */
			 0x0a	/* PES remaining header data length (5+5) */
			 );
  }

  if (pts.set) {
    /* Pack PTS. */
    ArrayU8_append_bytes(pes, 
			 0x21 | ( pts.hi_bit << 3) | (( pts.value & 0xC0000000) >> 30),
			 (( pts.value & 0x3fc00000 ) >> 22), /* bits 29:22 */
			 (( pts.value & 0x003f8000 ) >> 14) | 1, /* bits 21:15 */
			 (( pts.value & 0x00007f80 ) >> 8),	   /* bits 14:7 */
			 (( pts.value & 0x0000007f ) << 1) | 1   /* bits 6:0 */
			 );
  }

  if (dts.set) {
    /* Pack DTS, same format as PTS. */
    ArrayU8_append_bytes(pes,
			 0x21 | ( dts.hi_bit << 3) | (( dts.value & 0xC0000000) >> 30),
			 (( dts.value & 0x3fc00000 ) >> 22),
			 (( dts.value & 0x003f8000 ) >> 14) | 1,
			 (( dts.value & 0x00007f80 ) >> 8),
			 (( dts.value & 0x0000007f ) << 1) | 1
			 );
  }


  /* Copy then discard h264 data. */
  ArrayU8_append(pes, ArrayU8_temp_const(h264->data, h264->encoded_length));
  H264_buffer_discard(h264); h264 = 0L;
  
  /* Now pack into TS packets... */
  unsigned int pes_offset = 0;
  unsigned int pes_remaining;
  unsigned int payload_size;
  unsigned int payload_index = 0;

  while (pes_offset < pes->len) {
    TSPacket *packet = Mem_calloc(1, sizeof(*packet));
    unsigned int afLen = 0;
    unsigned int afAdjust = 1;

    pes_remaining = pes->len - pes_offset;

    packet->data[0] = 0x47;			/* Sync byte */

    if (payload_index == 0) {
      packet->data[1] = 
	(1 << 6) |		      /* PUS */
	(1 << 5) |		      /* priority */
	((pid & 0x1f00) >> 8);	      /* pid[13:8] */

      packet->data[2] = (pid & 0xff); /* pid[7:0] */
    }
    
    // packet->data[3] = (0x0 << 6) /* not scrambled */

    /* Always pack in a payload. */
    packet->data[3] |= (1 << 4);    

    /* Increment continuity counter if payload is present. */
    packet->data[3] |= (priv->continuity_counter & 0x0F);
    priv->continuity_counter += 1;

    if (payload_index == 0) {
      packet->data[3] |= (1 << 5); /* adaptation field */
      afLen = 7; 

      /* Using timestamp value stored in pts to fill in PCR.
	 FIXME: Verify that PCR is formatted correctly. */
      ArrayU8_append_bytes(pes,
			   (1<<4), /* Flags: PCR */
			   (pts.hi_bit << 7) | ((pts.value & 0xfe000000) >> 25),
			   ((pts.value & 0x01fe0000) >> 17),
			   ((pts.value & 0x0001fe00) >> 9),
			   ((pts.value & 0x000001fe) >> 1),
			   ((pts.value & 0x00000001) << 7) /* low bit of 33 bits */
			      | (0x3f << 1)  /* 6 bits reserved/padding */
			      | (0<<0),	/* 9th bit of extension */
			   (0x00)	/* bits 8:0 of exntension */
			   );
      
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
    else if (pes_remaining < 183) {
      packet->data[3] |= (1 << 5); /* adaptation field */
      afLen = (188 - 4 - 1 - pes_remaining);
      packet->data[4] = afLen;
      packet->data[5]  = 0x00;	/* no flags set */
      memset(packet->data+4+1+1, 0xFF, afLen-1);  /* pad with FFs... */
      payload_size = pes_remaining;
    }
    else if (pes_remaining == 183) {
      packet->data[3] |= (1 << 5); /* adaptation field */
      packet->data[4] = 0;	   /* but zero length... */
      /* leave data[5] alone */
      payload_size = pes_remaining;
    }
    else {
      /* No adaptation field. */
      payload_size = 188 - 4;
      afAdjust = 0;
    }

    memcpy(packet->data+4+afLen+afAdjust, 
	   pes->data+pes_offset, 
	   payload_size);

    pes_offset += payload_size;
    payload_index += 1;

    /* Add to list. */
    if (!priv->packets) {
      priv->packets = packet;
      priv->last_packet = packet;
    }
    else {
      priv->last_packet->next = packet;
      priv->last_packet = packet;
    }

  }
}


static void AAC_handler(Instance *pi, void *msg)
{
  // MpegTSMux_private *priv = pi->data;
  AAC_buffer *aac = msg;

  /* Assemble TS packets, and either write out, or save in a list so
     they can be smoothly interleaved with audio packets. */
  
  /* Discard AAC data. */
  AAC_buffer_discard(&aac);
}

static void flush(Instance *pi)
{
  MpegTSMux_private *priv = pi->data;

  if (!priv->chunk_file) {
    priv->chunk_file = fopen("muxout.ts", "wb");
    if (!priv->chunk_file) {
      return;
    }
  }

  while (priv->packets) {
    TSPacket *tmp = priv->packets;
    int n = fwrite(tmp->data, sizeof(tmp->data), 1, priv->chunk_file);
    if (n != 1) perror("fwrite");
    priv->packets = priv->packets->next;
    Mem_free(tmp);
  }
}


static void MpegTSMux_tick(Instance *pi)
{
  Handler_message *hm;

  hm = GetData(pi, 1);
  if (hm) {
    hm->handler(pi, hm->data);
    ReleaseMessage(&hm);
    flush(pi);    
  }

  pi->counter++;
}

static void MpegTSMux_instance_init(Instance *pi)
{
  MpegTSMux_private *priv = Mem_calloc(1, sizeof(*priv));
  pi->data = priv;
  /* FIXME: This is arbitrary, based on an early 2011 epoch date.  The
     real problem is I don't know to handle PTS wraps.  Should fix
     that sometime... */
  priv->tv_sec_offset = 1300000000;  
}


static Template MpegTSMux_template = {
  .label = "MpegTSMux",
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
