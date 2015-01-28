#include <stdlib.h>
#include <stdio.h>

/* After 20+ years of programming, I think I've come up with a generic
   list function in C that I am "happy enough" with.  The key is to
   separate the list growth, which can be kept generic by using
   pointers to the individual list structure members, from the item
   assignment, which is type-specific but can be done with a simple
   assignment in a enclosing macro that also calls the growth
   function.
*/

void _list_grow(void ** items,
	  int itemsize,
	  int * available,
	  int * count) 
{
  *count += 1;
  printf("*available=%d\n", *available);
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
  printf("*available=%d\n", *available);
}

#define list_append(item, c) { _list_grow((void*)&(c.items), sizeof(c.items[0]), &(c.available), &(c.count)); \
  c.items[c.count-1] = item; }

#define List(t) struct { t * items; int itemsize; int available; int count; }


/* Example type definition. */
typedef struct {
  struct { 
    int  * items; 
    int itemsize; 
    int available; 
    int count; 
  } rates;
} abc;


/* Same type definition, using macros. */

typedef struct {
  List(int) rates;
} xyz;


#define get(c, i) (c.items[i])

int main()
{
  abc x1 = {};
  _list_grow((void*)&x1.rates.items, sizeof(int), &x1.rates.available, &x1.rates.count); x1.rates.items[x1.rates.count-1] = 4;

  xyz x2 = {};
  list_append(5, x2.rates);

  //List(int) * x3 = malloc(sizeof(*x3));
  //list_append(6, x3);
  
  printf("%d\n", get(x1.rates, 0));
  printf("%d\n", get(x2.rates, 0));
  //printf("%d\n", get(x3, 0));

  return 0;
}


