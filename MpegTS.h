#ifndef _MPEGTS_H_
#define _MPEGTS_H_

extern void MpegTS_init(void);
extern void MpegTSDemux_init(void);

/* Alternate API. */
#define MpegTS_sync_byte(ptr) (ptr[0])
#define MpegTS_TEI(ptr) ((ptr[1] >> 7) & 1)
#define MpegTS_PUS(ptr) ((ptr[1] >> 6) & 1)
#define MpegTS_TP(ptr)  ((ptr[1] >> 5) & 1)
#define MpegTS_PID(ptr) (((ptr[1] & 0x1f) << 8) | (ptr[2]))

#define MpegTS_SC(ptr)  ((ptr[3] >> 6) & 3)
#define MpegTS_AFE(ptr) ((ptr[3] >> 5) & 1)
#define MpegTS_PDE(ptr) ((ptr[3] >> 4) & 1)
#define MpegTS_CC(ptr)  ((ptr[3] & 0xf))

#define MpegTS_AF_len(ptr)   (ptr[4])
#define MpegTS_AF_DI(ptr)    ((ptr[5] >> 7) & 1)
#define MpegTS_AF_RAI(ptr)   ((ptr[5] >> 6) & 1)
#define MpegTS_AF_ESPI(ptr)  ((ptr[5] >> 5) & 1)
#define MpegTS_AF_PCRI(ptr)  ((ptr[5] >> 4) & 1)
#define MpegTS_AF_OPCRI(ptr) ((ptr[5] >> 3) & 1)
#define MpegTS_AF_SP(ptr)    ((ptr[5] >> 2) & 1)
#define MpegTS_AF_TP(ptr)    ((ptr[5] >> 1) & 1)
#define MpegTS_AF_AFEXT(ptr) ((ptr[5] >> 0) & 1)

#define MpegTS_PCR_90K(pcr)  (uint64_t) ( \
					 ((pcr)[0]) | ((pcr)[1] << 8) |	\
					((pcr)[2] << 16) | (((pcr)[3]) << 24))
#define MpegTS_PCR_27M(ptr)


#endif
