#ifndef _LOG_H_
#define _LOG_H_

enum { LOG_YUV422P, LOG_WAV, LOG_SOCKET,
       LOG_NUM_CATEGORIES };

extern void Log(int category, char *fmt, ...);
extern void Log_dump(void);

#endif
