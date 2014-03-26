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

#define MpegTS_PTS(ptr)      (uint64_t) ( \
					(((ptr)[0] & 0x7)  << 30) | \
					(((ptr)[1] & 0xff) << 22) | \
					(((ptr)[2] & 0xfe) << 14) | \
					(((ptr)[3] & 0xff) << 7) | \
					(((ptr)[4] & 0xf3) >> 1)  )


#define MpegTS_PSI_PTRFIELD(ptr)  (ptr[4])
#define MpegTS_PSI_TABLEHDR(ptr) (&ptr[5] + (MpegTS_PSI_PTRFIELD(ptr)))

#define MpegTS_PSI_TABLE_ID(hdr)  (hdr[0])
#define MpegTS_PSI_TABLE_SSI(hdr) ((hdr[1]>>7 & 1))
#define MpegTS_PSI_TABLE_PB(hdr) ((hdr[1]>>6 & 1))
#define MpegTS_PSI_TABLE_RBITS(hdr) ((hdr[1]>>4 & 3))
#define MpegTS_PSI_TABLE_SUBITS(hdr) ((hdr[1]>>2 & 3))
#define MpegTS_PSI_TABLE_SLEN(hdr) (((hdr[1] & 3)<<8) | (hdr[2]))

#define MpegTS_PSI_TSS(hdr) &(hdr[3])
#define MpegTS_PSI_TSS_ID_EXT(tss) ((tss[0]<<8)|tss[1])
#define MpegTS_PSI_TSS_RBITS(tss) (tss[2]>>6 & 3)
#define MpegTS_PSI_TSS_VERSION(tss) (tss[2]>>1 & 0x1F)
#define MpegTS_PSI_TSS_CURRNEXT(tss) (tss[2] & 0x1)
#define MpegTS_PSI_TSS_SECNO(tss) (tss[3])
#define MpegTS_PSI_TSS_LASTSECNO(tss) (tss[4])

#define MpegTS_PAT_PASD(tss) &(tss[5])
#define MpegTS_PAT_PASD_PROGNUM(pasd) ((pasd[0]<<8)|pasd[1])
#define MpegTS_PAT_PASD_RBITS(pasd) ((pasd[2]>>5) & 0x7)
#define MpegTS_PAT_PASD_PMTID(pasd) (((pasd[2] & 0x1f)<<8)|pasd[3])

#define MpegTS_PMT_PMSD(tss) &(tss[5])
#define MpegTS_PMT_PMSD_RBITS(pmsd) ((pmsd[0]>>5) & 0x7)
#define MpegTS_PMT_PMSD_PCRPID(pmsd) (((pmsd[0] & 0x1f)<<8)|pmsd[1])
#define MpegTS_PMT_PMSD_RBITS2(pmsd) ((pmsd[2]>>4) & 0xf)
#define MpegTS_PMT_PMSD_UNUSED(pmsd) ((pmsd[2]>>2) & 0x3)
#define MpegTS_PMT_PMSD_PROGINFOLEN(pmsd) ((pmsd[2] & 0x3) | pmsd[3])

#define MpegTS_PMT_ESSD(pmsd) (&(pmsd[4]) + MpegTS_PMT_PMSD_PROGINFOLEN(pmsd))
#define MpegTS_ESSD_STREAMTYPE(essd) (essd[0])
#define MpegTS_ESSD_RBITS1(essd) ((essd[1]>>5) & 0x7)
#define MpegTS_ESSD_ELEMENTARY_PID(essd) ((essd[1] & 0x1f) | essd[2])
#define MpegTS_ESSD_RBITS2(essd) ((essd[3]>>4) & 0xf)
#define MpegTS_ESSD_UNUSED(essd) ((essd[3]>>2) & 0x3)
#define MpegTS_ESSD_DESCRIPTORSLENGTH(essd) (((essd[3]& 0x3)<<8)|essd[4])



typedef struct {
  uint32_t value;
  unsigned int hi_bit:1;
  unsigned int set:1;			/* boolean */
} MpegTimeStamp;

#endif
