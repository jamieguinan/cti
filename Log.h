#ifndef _LOG_H_
#define _LOG_H_

enum { LOG_YUVYUV422P, LOG_WAV, 
       LOG_NUM_CATEGORIES };

extern void Log(int category, char *fmt, ...);
extern void Log_dump(void);

#endif
