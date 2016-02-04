#include <stdio.h>
#include "SourceSink.h"
#include "File.h"
#include "cti_ipc.h"

int main(int argc, char * argv[])
{
  unsigned int timeout_ms = 1000;

  if (argc < 3) {
    printf("Usage: %s file host:port [ timeout_seconds ]\n", argv[0]);
    return 1;
  }
  
  if (argc == 4) {
    int seconds;
    if (sscanf(argv[3], "%d", &seconds) == 1) {
      timeout_ms = seconds * 1000;
    }
  }

  ArrayU8 * msg = File_load_data(S(argv[1]));
  
  if (!msg) {
    printf("Failed to load %s\n", argv[1]);
    return 1;
  }

  Comm * c = Comm_new(argv[2]);
  if (!IO_ok(c)) {
    printf("connection to %s failed\n", argv[2]);
    return 1;
  }
  
  int rc;

  rc =cti_ipc_send(c->io.s, msg->data, msg->len, timeout_ms);
  if (rc != 0) {
    printf("cti_ipc_send failed\n");
    return 1;
  }

  uint8_t * response = NULL;
  uint32_t response_length = 0;
  cti_ipc_recv(c->io.s, &response, &response_length, timeout_ms);
  if (rc != 0) {
    printf("cti_ipc_recv failed\n");
    return 1;
  }

  int i;
  for (i=0; i < response_length; i++) {
    putchar((char)response[i]);
  }

  return 0;
}
