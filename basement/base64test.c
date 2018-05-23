#include <stdio.h>
#include "../File.h"
#include "../String.h"

int main(int argc, char * argv[])
{
  if (argc != 2) {
    printf("Usage: %s path\n", argv[0]);
    return 1;
  }

  String * b64 = File_load_base64(S(argv[1]));
  printf("%s\n", s(b64));
  
  return 0;
}
