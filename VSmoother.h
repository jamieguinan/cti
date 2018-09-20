#ifndef _VSMOOTHER_H_
#define _VSMOOTHER_H_

extern void VSmoother_init(void);

typedef struct _VSmoother VSmoother; /* Incomplete type. */

extern void VSmoother_smooth(VSmoother *priv,
                             double frame_timestamp,
                             int pending_messages);

#endif
