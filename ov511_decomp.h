#ifndef _OV511_DECOMP_H_
#define _OV511_DECOMP_H_

int
ov511_decomp_400(unsigned char *pIn,
		 unsigned char *pOut,
		 unsigned char *pTmp,
		 int		w,
		 int		h,
		 int		inSize);

int
ov511_decomp_420(unsigned char *pIn,
		 unsigned char *pOut,
		 unsigned char *pTmp,
		 int		w,
		 int		h,
		 int		inSize);

#endif
