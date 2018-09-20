#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

#include "CTI.h"
#include "Lirc.h"
#include "String.h"


int app_code(int argc, char *argv[])
{
  int rc = 0;
  Config_buffer *cb;

  Instance *lirc = Instantiate("Lirc");



  return rc;
}

int main(int argc, char *argv[])
{
  Lirc_init();
  return app_code(argc, argv);

}
