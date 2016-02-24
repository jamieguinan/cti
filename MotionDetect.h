#ifndef _MOTIONDETECT_H_
#define _MOTIONDETECT_H_

typedef struct {
  int sum;
} MotionDetect_result;

MotionDetect_result *MotionDetect_result_new(void);
void MotionDetect_result_release(MotionDetect_result **md);

extern void MotionDetect_init(void);

#endif
