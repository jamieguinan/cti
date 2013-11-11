#include "XmlSubset.h"
#include "File.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum states {
  IN_TAG,
  WAITING_FOR_TEXT_OR_SUBNODES,
  IN_TEXT,
  INVALID_XML,
};

static int debug = 0;


Node * xml_string_to_nodetree(String *str)
{
  /* Read an XML file and generate a "Node *" tree. */
  int depth = 0;
  int state = WAITING_FOR_TEXT_OR_SUBNODES;
  String * tag_buffer = String_new("");
  String * text_buffer = String_new("");

  if (!str || String_is_none(str)) {
    return NULL;
  }

  Node * top = node_new("document");
  Node * current = top;

  int i=0;

  while (1) {
    int c = String_get_char(str, i++);
    if (c == -1) {
      break;
    }
    char ch = c;

    if (state == IN_TAG) {
      if (ch == '>') {
	state = WAITING_FOR_TEXT_OR_SUBNODES;
	if (debug) printf("tag: %s\n", s(tag_buffer));
	if (String_get_char(tag_buffer, 0) == '/') {
	  /* Compare tags, close existing node, adjust current */
	  if (streq(current->tag, s(tag_buffer)+1)) {
	    depth -=1;
	    if (debug) printf("-depth=%d\n", depth);
	    current = current->parent;
	  }
	  else {
	    fprintf(stderr, "tags do not match: %s /%s\n", current->tag, s(tag_buffer)+1);
	    state = INVALID_XML;
	    break;
	  }
	}
	else {
	  /* Append new node, adjust current */
	  current = node_add_new(current, s(tag_buffer));
	  depth += 1;
	  if (debug) printf("+depth=%d\n", depth);
	  if (String_begins_with(tag_buffer, "?xml")) {
	    /* XML declaration, back up one level. */
	    current = current->parent;
	    depth -= 1;
	  }
	}
	/* Clear the tag buffer. */
	String_set(tag_buffer, "");
      }
      else {
	String_append_bytes(tag_buffer, &ch, 1);
      }
    }
    else if (state == WAITING_FOR_TEXT_OR_SUBNODES) {
      if (ch == '<') {
	state = IN_TAG;
      }
      else if (!isspace(ch)) {
	state = IN_TEXT;
	String_append_bytes(text_buffer, &ch, 1);
      }
      else {
	/* isspace(ch) is true, continue along... */
      }
    }
    else if (state == IN_TEXT) {
      if (ch == '<') {
	node_add(current, Text(s(text_buffer)));
	if (debug) printf("  %s\n", s(text_buffer));
	String_set(text_buffer, "");
	state = IN_TAG;
      }
      else {
	String_append_bytes(text_buffer, &ch, 1);
      }
    }
  }

  if (state != WAITING_FOR_TEXT_OR_SUBNODES || depth != 0) {
    fprintf(stderr, "XML error (state=%d depth=%d)\n", state, depth);
    /* FIXME: Clean up node tree... */
    top = NULL;
  }
  
  return top;
}


Node * xml_file_to_nodetree(String * filename)
{
  String * contents = File_load_text(filename);
  Node * nt = xml_string_to_nodetree(contents);
  String_free(&contents);
  return nt;
}


int xml_simple_node(Node *node)
{
  /* Return 1 if node represents the common case of <tag>value</tag> */
  if (node
      && node->tag
      && node->subnodes_count == 1
      && node->subnodes[0]->text) {
    return 1;
  }
  else {
    return 0;
  }
}


String * xml_simple_node_value(Node *node)
{
  /* Return "value" for the common-case of <tag>value</tag> */
  if (!xml_simple_node(node)) {
    return String_value_none();
  }
  else {
    return String_new(node->subnodes[0]->text);
  }
}


#ifdef TEST
int main(int argc, char *argv[])
{
  debug = 1;
  Node * n;
  int i;
  if (argc > 1) {
     n = xml_file_to_nodetree(S(argv[1]));
  }
  return 0;
}
#endif

