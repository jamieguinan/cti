#ifndef _CTI_IPC_H_
#define _CTI_IPC_H_

#include <stdint.h>

extern int cti_ipc_send(int fd, uint8_t * message, uint32_t message_length, int timeout_ms);
extern int cti_ipc_recv(int fd, uint8_t ** message, uint32_t * message_length, int timeout_ms);
extern void cti_ipc_free(uint8_t ** message, uint32_t * message_length);

extern int cti_ipc_debug_recv_checksum;

#define CTI_IPC_ERR_NONE 0
#define CTI_IPC_ERR_OTHER 1

#endif
