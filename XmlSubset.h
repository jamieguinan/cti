#ifndef _XML_SUBSET_H_
#define _XML_SUBSET_H_

#include "nodetree.h"
#include "String.h"

extern Node * xml_string_to_nodetree(String *str);
extern Node * xml_file_to_nodetree(String * filename);

extern int xml_simple_node(Node *node);
extern String * xml_simple_node_value(Node *node);

#endif
