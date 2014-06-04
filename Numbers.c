#include "Numbers.h"
#include "Mem.h"
#include <stdio.h>
#include <stdlib.h>

RunningAverageDouble * RAD_new(int count)
{
  RunningAverageDouble *rad = Mem_calloc(1, sizeof(*rad));
  if (count <= 0) {
    fprintf(stderr, "%s: count must be > 0\n", __func__);
    exit(1);
  }
  rad->num_values = count;
  rad->values = Mem_calloc(rad->num_values, sizeof(rad->values[0]));
  rad->avg = 0.0;
  return rad;
}


void RAD_add(RunningAverageDouble *rad, double value)
{
  if (rad->num_added < rad->num_values) {
    rad->num_added += 1;
  }
  rad->values[rad->index] = value;
  rad->index = (rad->index + 1) % rad->num_values;
  rad->avg_set = 0;		/* For for recalculation. */
}


double RAD_get(RunningAverageDouble *rad)
{
  if (!rad->avg_set) {
    int i;
    double sum = 0.0;
    if (rad->num_added) {
      for (i=0; i < rad->num_added; i++) {
	sum += rad->values[i];
      }
      rad->avg = sum/rad->num_added;
      rad->avg_set = 1;
    }
  }
  return rad->avg;
}
