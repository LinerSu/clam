#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

// RUN: %clam -O0 --inline --crab-dom=zones --crab-dom-param="region.is_dereferenceable=true" --crab-track=mem --crab-heap-analysis=cs-sea-dsa-types --crab-lower-unsigned-icmp --crab-check=assert --crab-sanity-checks "%s" 2>&1 | OutputCheck %s
// CHECK: ^1  Number of total safe checks$
// CHECK: ^0  Number of total warning checks$

/* Externalize Helper Function */
extern uint8_t uint8_t_nd(void);
extern void __CRAB_assert(int);
extern void __CRAB_assume(int);
extern int int_nd(void);
extern bool bool_nd(void);
extern void memhavoc(void *, size_t);
extern bool sea_is_dereferenceable(const void *ptr, intptr_t offset);

struct aws_byte_buf {
    size_t length;
    size_t capacity;
    uint8_t *buffer;
};

size_t size_t_nd(void) {
    int res = int_nd();
    __CRAB_assume(res >= 0);
    return res;
}

void initialize_byte_buf(struct aws_byte_buf *const buf, size_t cap) {
    size_t len = size_t_nd();
    __CRAB_assume(len <= cap);
    buf->length = len;
    buf->capacity = cap;
    buf->buffer = malloc(cap * sizeof(*(buf->buffer)));
}

extern void __CRAB_intrinsic_do_reduction(void *, bool);

int main(void) {
  struct aws_byte_buf *b1 = (
        struct aws_byte_buf*) malloc(sizeof(struct aws_byte_buf));
  initialize_byte_buf(b1, 10);
  
  struct aws_byte_buf *b2 = 
        (struct aws_byte_buf*) malloc(sizeof(struct aws_byte_buf));
  initialize_byte_buf(b2, 20);

  struct aws_byte_buf *buf = bool_nd() ? b1 : b2;
  size_t len = buf->length;
  size_t cap = buf->capacity;
  uint8_t *buffer = buf->buffer;
  __CRAB_assert(len <= cap);
  __CRAB_assert(sea_is_dereferenceable(buffer, len));
  __CRAB_assert(sea_is_dereferenceable(buffer, cap));
  
  return 0;
}