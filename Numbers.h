#ifndef _NUMBERS_H_
#define _NUMBERS_H_

typedef struct {
  double *values;
  int num_values;
  int num_added;
  int index;
  double avg;
  int avg_set;
} RunningAverageDouble;

RunningAverageDouble * RAD_new(int count);
void RAD_add(RunningAverageDouble *rad, double value);
double RAD_get(RunningAverageDouble *rad);

#endif
