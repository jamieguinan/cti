#include <stdlib.h>
#include <stdio.h>

/* After 20+ years of programming, I think I've come up with a generic
   list function in C that I am "happy enough" with.  The key is to
   separate the list growth, which can be kept generic by using
   pointers to the individual list structure members, from the item
   assignment, which is type-specific but can be done with a simple
   assignment in a enclosing macro that also calls the growth
   function.  My instinct had always been to include the growth
   and assignment in the same macro, which C++ templates solve
   nicely, but can be really messy in C, ending up with lots of
   duplicate code.

   Haha, turns out I had written very similar code in XArray.c some
   time ago.
*/

void _array_grow(void ** items,
	  int itemsize,
	  int * available,
	  int * count) 
{
  *count += 1;
  //printf("*available=%d\n", *available);
  if (*available == 0) {
    *available = 2;
    void *tmp = malloc(itemsize*(*available));
    /* FIXME: if (!tmp) { error_handler(); return; } */
    (*items) = tmp;
  }
  else if (*count == *available) {
    *available *= 2;
    void *tmp = realloc((*items), itemsize*(*available));
    /* FIXME: if (!tmp) { error_handler(); return; } */
    (*items) = tmp;
  }
  // printf("*available=%d\n", *available);
}

#define array_append(item, c) { _array_grow((void*)&(c.items), sizeof(c.items[0]), &(c.available), &(c.count)); \
  c.items[c.count-1] = item; }

#define array_get(c, i) (c.items[i])

#define array_ptr_append(item, c) { _array_grow((void*)&(c->items), sizeof(c->items[0]), &(c->available), &(c->count)); \
  c->items[c->count-1] = item; }

#define array_ptr_get(c, i) (c->items[i])

/* This Array type declaration can be used anywhere: local, pointer, struct member, parameter */
#define Array(t) struct { t * items; int itemsize; int available; int count; }

/* Example type definition. */
typedef struct {
  struct { 
    int  * items; 
    int itemsize; 
    int available; 
    int count; 
  } rates;
} abc;


/* Same type definition, using macro. */
typedef struct {
  Array(int) rates;
} xyz;


int main()
{
  abc x1 = {};
  /* explicit call */
  _array_grow((void*)&x1.rates.items, sizeof(int), &x1.rates.available, &x1.rates.count); x1.rates.items[x1.rates.count-1] = 4;

  xyz x2 = {};
  /* macro call */
  array_append(5, x2.rates);

  /* pointer declaration */
  Array(int) * x3 = malloc(sizeof(*x3));
  array_ptr_append(6, x3);
  
  printf("%d\n", array_get(x1.rates, 0));
  printf("%d\n", array_get(x2.rates, 0));
  printf("%d\n", array_ptr_get(x3, 0));

  return 0;
}
