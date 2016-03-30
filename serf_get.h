#ifndef _CTI_SERF_GET_H
#define _CTI_SERF_GET_H

#include "String.h"

extern int serf_get(int argc, const char **argv, String * output_string);
extern int serf_get_main(int argc, const char **argv);
extern int serf_get_command(String * command, String * output_string);

#endif
