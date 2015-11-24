#ifndef _CTI_IPC_H_
#define _CTI_IPC_H_

#include <stdint.h>

extern int ipc_send(int fd, uint8_t * message, uint32_t message_length, int timeout_ms);
extern int ipc_recv(int fd, uint8_t ** message, uint32_t * message_length, int timeout_ms);
extern void ipc_free(uint8_t ** message, uint32_t * message_length);

#define IPC_ERR_NONE 0
#define IPC_ERR_OTHER 1

#endif
