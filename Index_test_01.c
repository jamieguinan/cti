/*
 * Test code for Index.c module.  This includes it directly, so can
 * call static functions including _Index_op().
 *
 * gcc Index_test_01.c -o Index_test_01 && ./Index_test_01
 */

#include "Index.c"
#include "Mem.c"
#include "String.c"
#include "Cfg.c"

static void check(int val, int expected)
{
  if (val == expected) {
    printf("good\n");
  }
  else {
    printf("bad\n");
  }
}

FILE *gvf;

void gvout1(Index_node *node)
{
  fprintf(gvf, "node%ld[label = \"<f0> |<f1> %" PRIu32 "|<f2> \"];\n", node->key, node->key);
}

void gvout2(Index_node *node)
{
  if (node->left) {
    fprintf(gvf, "\"node%ld\":f0 -> \"node%ld\":f1;\n", node->key, node->left->key);
  }
  if (node->right) {
    fprintf(gvf, "\"node%ld\":f2 -> \"node%ld\":f1;\n", node->key, node->right->key);
  }
}

void make_gv(Index *idx, const char *filename)
{
  gvf = fopen(filename, "w");
  if (!gvf) {
    perror(filename);
    return;
  }
  fprintf(gvf, "digraph g {\n");
  fprintf(gvf, "node [shape = record,height=.1];\n");

  Index_walk(idx, gvout1);
  Index_walk(idx, gvout2);

  fprintf(gvf, "}\n");
  fclose(gvf);
}

int main()
{
  {
    Index *idx = Index_new();
    int err;

    _Index_op(idx, S("one"), NULL, (void*)1, NULL, INDEX_OP_ADD, 0, &err);
    check(err, INDEX_NO_ERROR);

    _Index_op(idx, S("two"), NULL, (void*)2, NULL, INDEX_OP_ADD, 0, &err);
    check(err, INDEX_NO_ERROR);

    _Index_op(idx, S("three"), NULL, (void*)3, NULL, INDEX_OP_ADD, 0, &err);
    check(err, INDEX_NO_ERROR);

    _Index_op(idx, S("four"), NULL, (void*)4, NULL, INDEX_OP_ADD, 0, &err);
    check(err, INDEX_NO_ERROR);

    _Index_op(idx, S("five"), NULL, (void*)5, NULL, INDEX_OP_ADD, 0, &err);
    check(err, INDEX_NO_ERROR);

    void *result;

    _Index_op(idx, S("three"), NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_ERROR);
    printf("[three]: %ld\n", (long)result);

    _Index_op(idx, S("one"), NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_ERROR);
    printf("[one]: %ld\n", (long)result);

    _Index_op(idx, S("two"), NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_ERROR);
    printf("[two]: %ld\n", (long)result);

    _Index_op(idx, S("four"), NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_ERROR);
    printf("[four]: %ld\n", (long)result);

    _Index_op(idx, S("five"), NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_ERROR);
    printf("[five]: %ld\n", (long)result);

    _Index_op(idx, S("six"), NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_KEY_NOT_FOUND);



    _Index_op(idx, S("two"), NULL, (void*)22, &result, INDEX_OP_ADD, 0, &err);
    check(err, INDEX_DUPLICATE_KEY);

    _Index_op(idx, S("two"), NULL, (void*)22, &result, INDEX_OP_ADD, 1, &err);
    check(err, INDEX_NO_ERROR);
    printf("[old two]: %ld\n", (long)result);

    _Index_op(idx, S("two"), NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_ERROR);
    printf("[two]: %ld\n", (long)result);

    _Index_op(idx, S("two"), NULL, NULL, NULL, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_DEST);

    _Index_op(idx, NULL, NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_KEY);

    _Index_op(NULL, NULL, NULL, NULL, &result, INDEX_OP_FIND, 0, &err);
    check(err, INDEX_NO_IDX);
  }

  {
    const char * text = "\
I am wondering if I can avoid the shift by using different fixed-valuey \
mask operations depending on the current bit offset into a 32 or 64 \
bit source word.  There would be a similar block of code repeated for \
each possible bit offset, with a switch statement at the top.  It \
would be trading code size for a possible reduction in number of \
instructions executed, but I think it should all fit into the \
instruction cache for any processor that I care about. \
Sometimes I wonder about huffman decoding for Jpeg blocks, and whether \
it could be done with a linear worst-case procedure consisting of only \
arithmetic operations (add, subtract, multiply, divide), leaving \
sparse output.  Then that output could be translated to the final \
output values, again using arithmetic operations.  Avoid conditionals \
and lookup tables.  Pre-generate intermediate factors, as long as they \
are constant.  If all this works, then it might be possible to use \
planar hardware operators to decompress an entire frame worth of \
DCT blocks in parallel.  Or, if the DCTs are hard to separate, \
several frames worth in parallel. \
I was considering using Unix epoch times with millisecond resolution, \
stored in a uint64_t.  This would last 6 million times the current \
date value.  But for 30fps video, even 1ms error per frame could \
accumulate and throw off the video by one frame in only a single \
second.  And MpegTS wants 90KHz resolution.  I will stick with timevals \
for now.  Or why not double-precision floats? \
TS playback on iOS is a little choppy, for both birdcam and satellite \
input sources.  I think this might be because of the ordering of the \
TS packets.  If the video and audio input objects (elementary packets) \
have timestamps and known duration, then I could spread the period \
over the packets and assign an estimated timestamp to each packet, \
even those that do not include PTS/DTS.  Then it should be relatively \
easy to sort the packets for writeout, and more accurately insert PAT \
and PMT packets.";

    String_list *strs = String_split_s(text, " ");
    printf("%d strings\n", String_list_len(strs));

    Index *idx = Index_new();

    int i;
    for (i=0; i < String_list_len(strs); i++) {
      void *oldvalue = NULL;
      int err = 0;
      int d = (i%2);
      printf("%d: adding %s with delete=%d ; ", i, s(String_list_get(strs, i)), d);
      _Index_op(idx, String_list_get(strs, i), NULL, (void*)(long)i, &oldvalue, INDEX_OP_ADD, d, &err);
      printf("err=%d oldvalue=%ld\n", err, (long)oldvalue);
    }
    printf("count=%d\n", idx->count);
    Index_analyze(idx);
    make_gv(idx, "one.gv");

    for (i=0; i < String_list_len(strs); i+=2) {
      void *oldvalue = NULL;
      int err = 0;
      int d = 1;
      printf("%d: finding %s with delete=%d ; ", i, s(String_list_get(strs, i)), d);
      _Index_op(idx, String_list_get(strs, i), NULL, NULL, &oldvalue, INDEX_OP_FIND, d, &err);
      printf("err=%d oldvalue=%ld\n", err, (long)oldvalue);
    }
    printf("count=%d\n", idx->count);
    Index_analyze(idx);
    make_gv(idx, "two.gv");

    for (i=1; i < String_list_len(strs); i+=2) {
      void *oldvalue = NULL;
      int err = 0;
      int d = 1;
      printf("%d: finding %s with delete=%d ; ", i, s(String_list_get(strs, i)), d);
      _Index_op(idx, String_list_get(strs, i), NULL, NULL, &oldvalue, INDEX_OP_FIND, d, &err);
      printf("err=%d oldvalue=%ld\n", err, (long)oldvalue);
    }
    printf("count=%d\n", idx->count);
    Index_analyze(idx);

  }

  return 0;
}
