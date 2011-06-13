/* This is the generic main() for builds other than OSX/Cocoa. */
extern int cti_main(int argc, char *argv[]);

char *argv0;

int main(int argc, char *argv[])
{
  argv0 = argv[0];

  return cti_main(argc, argv);
}
