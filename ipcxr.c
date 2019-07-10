#include <stdio.h>
#include "SourceSink.h"
#include "File.h"
#include "cti_ipc.h"

int main(int argc, char * argv[])
{
  int argn;
  int timeout_ms = 1000;
  int waitreply = 1;
  int seconds;

  if (argc < 3) {
    printf("Usage:\n"
           "  %s file host:port [nowaitreply] [ timeout_seconds ]\n"
           "  %s '{json text}' host:port [nowaitreply] [ timeout_seconds ]\n"
           , argv[0]
           , argv[0]
           );
    return 1;
  }

  for (argn = 3; argn < argc; argn++) {
    if (streq(argv[argn], "nowaitreply")) {
      waitreply = 0;
    }
    else if (sscanf(argv[argn], "%d", &seconds) == 1) {
      timeout_ms = seconds * 1000;
    }
  }

  ArrayU8 * msg= NULL;

  if (strstr(argv[1], "{") && strstr(argv[1], "}")) {
    msg = ArrayU8_new();
    ArrayU8_append(msg, ArrayU8_temp_string(argv[1]));
  }
  else {
    msg = File_load_data(S(argv[1]));
  }

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

  rc = cti_ipc_send(c->io.s, msg->data, msg->len, timeout_ms);
  if (rc != 0) {
    printf("cti_ipc_send failed\n");
    return 1;
  }

  if (waitreply) {
    uint8_t * response = NULL;
    uint32_t response_length = 0;
    rc = cti_ipc_recv(c->io.s, &response, &response_length, timeout_ms);
    if (rc != 0) {
      printf("cti_ipc_recv failed\n");
      return 1;
    }

    int i;
    for (i=0; i < response_length; i++) {
      putchar((char)response[i]);
    }
  }

  return 0;
}
