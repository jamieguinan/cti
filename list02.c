#include <stdlib.h>
#include <stdio.h>

void grow(void ** items,
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

#define append(item, c) { grow((void*)&(c.items), sizeof(c.items[0]), &(c.available), &(c.count)); c.items[c.count-1] = item; }

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
#define List(t, name) struct { t * items; int itemsize; int available; int count; } name

typedef struct {
  List(int, rates);
} xyz;


#define get(c, i) (c.items[i])

int main()
{
  abc x1 = {};
  grow((void*)&x1.rates.items, sizeof(int), &x1.rates.available, &x1.rates.count); x1.rates.items[x1.rates.count-1] = 4;

  xyz x2 = {};
  append(5, x2.rates);
  
  printf("%d\n", get(x1.rates, 0));
  printf("%d\n", get(x2.rates, 0));

  return 0;
}


