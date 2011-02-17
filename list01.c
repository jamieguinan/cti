

  /* TESTING... */
  struct {
    void (*handler)(void *data);
    void *data;
    unsigned int next;
    unsigned char set;
  } *messages;
  int messages_count;
  int messages_max_count;
} Instance;


/* I admit C++ would do a better job at keeping code size down here.  On the
   other hand, data sets are typically huge by comparison, compilers might
   be able to find and coalesce common code, and C++ much other baggage
   that I'm happy to avoid. */
#define ISet_add(iset, pitem) {  \
  if (iset.items == 0L) { \
    iset.avail = 2; \
    iset.items = Mem_calloc(iset.avail, sizeof(pitem)); \
  } \
  else if (iset.avail == iset.count) { \
    iset.avail *= 2; \
    iset.items = Mem_realloc(iset.items, iset.avail * sizeof(pitem)); \
  } \
  iset.items[iset.count] = pitem; \
  iset.count += 1; \
}

#define ISet_add_keyed(iset, key, pitem) {	\
  ISet_add(iset, pitem); \
  Index_update(&iset.index, key, pitem); \
}

#define ISet_pop(iset, pitem) {  \
  pitem = iset.items[0]; \
  iset.count -= 1; \
  memmove(iset.items, &iset.items[1], iset.count * sizeof(iset.items[0])); \
  iset.items[iset.count] = 0; \
}




Post:
  Lock.
  If free slot, insert and return.
  If no free positions, and room for expansion,
    expand ring, insert, return.
  Else if block flag, block.
  Else if discard-on-full flag, discard.
  Else return error.

Pop:
  Lock.
  Copy from current pointer/index.
  Clear current.
  Increment current.

type **elements,
type *new_element,
int element_size,
int push_index
int pop_index
int free_slots

int push8bit() 
{
  /* Assumes data structures are locked. */
  if (free_slots == 0) {
    return -1;
  }
  elements[push_index] = new_element;
  push_index += 1;
  free_slots -= 1;
}

int pop8bit()
{
  if (available == total) {
    return 0;
  }
  element = elements[pop_index];
  clear(elements[pop_index]);
  pop_index += 1;
  free_slots += 1;
}



