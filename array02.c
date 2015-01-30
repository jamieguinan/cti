#include <stdlib.h>
#include <stdio.h>

#include "Array.c"
#include "Mem.c"

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
  Array_grow((void*)&x1.rates.items, sizeof(int), &x1.rates.available, &x1.rates.count); x1.rates.items[x1.rates.count-1] = 4;

  xyz x2 = {};
  /* macro call */
  Array_append(x2.rates, 5);

  /* pointer declaration */
  Array(int) * x3 = malloc(sizeof(*x3));
  Array_ptr_append(6, x3);
  
  printf("%d\n", Array_get(x1.rates, 0));
  printf("%d\n", Array_get(x2.rates, 0));
  printf("%d\n", Array_ptr_get(x3, 0));

  return 0;
}
