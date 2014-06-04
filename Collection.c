#include "CTI.h" 		/* this indirectly includes Collection.h */

#include <string.h>
#include <stdio.h>

#include <stdlib.h>
#define Calloc calloc
#define Realloc realloc

void _Collection_init(void **set, int elem_size, Collection *c)
{
  memset(c, 0, sizeof(*c));
  c->datapp = set;
  c->itemSize = elem_size;
}

int Collection_prepare_putlast(Collection *c)
{
  /* The easy case should be as short as possible. */
  if (c->flags & COLLECTION_EASY) {
    if ((c->count+1) == c->available ||
        (c->iNextFree+1) == c->available) {
      /* The next add will be tricky, prepare for it. */
      clear_flag(c->flags, COLLECTION_EASY);
    }
    c->count += 1;
    return 0;
  }

  /* The fallthrough code below handles all the trickier cases. */

  if (!c->available) {  /* First time called. */
    if (c->available_limit) {
      c->available = cti_min(8, c->available_limit);
    }
    else {
      c->available = 8;
    }

    *(c->datapp) = Calloc(1, c->available * c->itemSize);
    
    set_flag(c->flags, COLLECTION_EASY);
    c->count = 1;
    return 0;
  }

  if (c->count == c->available) {
     /* Expand, or error. */
     if (c->available == c->available_limit) {
       /* Now we're fucked... */
       set_flag(c->flags, COLLECTION_FULL_ERROR);
       if (c->error_handler) {
         c->error_handler(c);
       }
       return 1;
     }
     
     int available_old = c->available;  /* Need this later.. */

     /* Bump up collection size */
     if (c->available_limit) {
       c->available = cti_min(c->available*2, c->available_limit);
     }
     else {
       c->available*=2;
     }

     printf("c->available=%d\n", c->available);

     *(c->datapp) = Realloc(*(c->datapp), c->available * c->itemSize);

     if (c->iNextFree <= c->iFirst) {
       /* Have just expanded the data buffer, but the pointers in 
          the "original" part of the buffer are in a backwards order.
	  Options:
            1: A few memmoves to align the elements at the bottom
               of the buffer, set iFirst=0.
	    2: Set a flag and try to manage.  This gets complicated
               if the buffer keeps expanding, then need another flag
               everytime it expands, ugh.
            3: memmove [0..iNextFree] into the newly allocated
               part of the buffer, then clear that same range in
	       the original part of the bufer. iNextFree = oldTop + iNextFree.
          Using option 3...
       */
       uint8_t *p = *(c->datapp);
       uint8_t *pNewPart = p + (available_old * c->itemSize);

       memmove(pNewPart, p, c->iNextFree * c->itemSize);
       memset(p, 0, c->iNextFree * c->itemSize);
       c->iNextFree += available_old;
     }
  }

  else if (c->iNextFree == c->available) {
    c->iNextFree = 0;
  }
  else if ((c->count+1) == c->available ||
	   (c->iNextFree+1) == c->available) {
    /* Leave COLLECTOIN_EASY unset. */
  }
  else  {
    set_flag(c->flags, COLLECTION_EASY);
  }

  c->count += 1;
  return 0;
}


int Collection_prepare_takefirst(Collection *c)
{
  if (0 == c->count) {
    set_flag(c->flags, COLLECTION_EMPTY_ERROR);
    if (c->error_handler) {
      c->error_handler(c);
    }
    return 1;
  }

  if (c->iFirst == c->available) {
    c->iFirst = 0;
  }

  c->count -= 1;

  return 0;
}
