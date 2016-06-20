#ifndef _CTI_SERF_GET_H
#define _CTI_SERF_GET_H

#include "String.h"

extern int serf_ssl_verbose;

extern int serf_get_post(int argc, const char **argv, String * output_string);
extern int serf_main(int argc, const char **argv);
extern int serf_command_get(String * command, String * url, String * output_string);
extern int serf_command_post_data_string(String * command, String * url, String * post_data, String * output_string);
extern int serf_command_post_data_file(String * command, String * url, String * file_path, String * output_string);

#endif
