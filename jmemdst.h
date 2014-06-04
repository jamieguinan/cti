#ifndef _JMEMDST_H_
#define _JMEMDST_H_

GLOBAL(void)
jpeg_mem_dest (j_compress_ptr cinfo, JOCTET * buffer, int buffer_length, int *final_size);

#endif
