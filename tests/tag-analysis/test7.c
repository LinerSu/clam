// RUN: %clam -O0 --crab-inter --crab-dom=int --crab-track=mem --crab-heap-analysis=cs-sea-dsa --crab-check=assert --crab-sanity-checks "%s" 2>&1 | OutputCheck %s
// RUN: %clam -O0 --crab-inter --crab-dom=int --crab-track=mem --crab-heap-analysis=ci-sea-dsa --crab-check=assert --crab-sanity-checks "%s" 2>&1 | OutputCheck %s
// CHECK: ^1  Number of total safe checks$
// CHECK: ^1  Number of total warning checks$

#include <stdlib.h>
#include <string.h>
#include "clam/clam.h"

/* seadsa */
extern void sea_dsa_set_read(const void *p);
extern void sea_dsa_set_modified(const void *p);

/* Tag analysis */
typedef uint64_t tag_t;
extern void  __CRAB_intrinsic_add_tag(void *, tag_t);
extern void  __CRAB_intrinsic_check_does_not_have_tag(void *, tag_t);
#define ADD_TAG(PTR, TAG)	      \
  __CRAB_intrinsic_add_tag(PTR, TAG); \
  sea_dsa_set_modified(PTR);

#define CHECK_NOT_TAG(PTR, TAG) \
  __CRAB_intrinsic_check_does_not_have_tag(PTR ,TAG); \
  sea_dsa_set_read(PTR);

/* Program starts here */

/* No load or store touches the array field: only memcpy writes it (the
   length is symbolic, so the copy stays a memcpy).
   - The tag that fill moves into p->data must survive the return: the
     field's cell has no accessed type, but it is a region of fill's
     interface.
   - The copy must not move the tag into p->len, which lies before the
     copied field (nothing overwrites len after the copy). */
struct pkt {
  int len;
  char data[16];
};

void fill(struct pkt *p, char *src, int n) {
  p->len = 16; /* a constant: n comes from nd_int, whose tags are unknown */
  memcpy(p->data, src, n);
}

int main(int argc, char **argv) {
  struct pkt *p = (struct pkt *)malloc(sizeof(struct pkt));
  char in[16];
  int n = nd_int();
  __CRAB_assume(n > 0);
  __CRAB_assume(n <= 16);
  in[0] = 'a';
  ADD_TAG(in, 1);
  fill(p, in, n);
  CHECK_NOT_TAG(p->data, 1); // FAIL: the copied bytes carry tag 1
  CHECK_NOT_TAG(&p->len, 1); // OK
  return 0;
}
