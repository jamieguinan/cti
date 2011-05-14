#ifndef _CTI_COLLECTION_H_
#define _CTI_COLLECTION_H_

#include <stdint.h>

typedef struct _Collection
{
  void **datapp;   /* Point to the pointer. */
  unsigned int itemSize;
  unsigned int iFirst;
  unsigned int iNextFree;
  unsigned int count;
  unsigned int available;
  unsigned int available_limit;
  unsigned int alignment;
  unsigned int deleteAt;
  uint32_t flags;   
  // Index *index;
  void (*error_handler)(struct _Collection *);
} Collection;

/* .flags */
#define COLLECTION_LEFTPACK    (1<<0)
#define COLLECTION_EASY        (1<<1)
#define COLLECTION_DELETE_AT   (1<<2)

/* Reduced this to fit 16-bit for MSP. */
#define COLLECTION_EMPTY_ERROR (1<<14)
#define COLLECTION_FULL_ERROR  (1<<15)

void _Collection_init(void **set, int elem_size, Collection *c);
int Collection_prepare_putlast(Collection *c);
int Collection_prepare_takefirst(Collection *c);

#define Collection_init(set, c)  do { _Collection_init((void**)&set, sizeof(set[0]), &c); } while (0)

#define putlast(entry, set, c) \
  do { if (Collection_prepare_putlast(c) == 0) set[(c)->iNextFree++] = entry;  } while (0)

#define takelast(entry, set, c) \
  do { if (Collection_prepare_takelast(c) == 0) entry = set[(c)->iNextFree--];  } while (0)

#define putfirst(entry, set, c) \
  do { if (Collection_prepare_putfirst(c) == 0) set[(c)->iFirst--];  } while (0)

#define takefirst(entry, set, c) \
  do { if (Collection_prepare_takefirst(c) == 0) entry = set[(c)->iFirst++];  } while (0)

#define putat(entry, set, c, n) \
  do { if (Collection_prepare_putat(c, n) == 0) set[n] = entry;  } while (0)

#define takeat(entry, set, c, n) \
  do { if (Collection_prepare_takeat(c) == 0) entry = set[n];  } while (0)


#define clear_flag(flags, bit)  do { flags &= (~bit); } while(0)
#define set_flag(flags, bit)  do { flags |= (bit); } while(0)

#define cti_min(a, b)  ( (a) < (b) ? (a) : (b) )

#endif
