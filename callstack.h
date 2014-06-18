#ifndef __CALLSTACK_H__
#define __CALLSTACK_H__

typedef struct {
  void *stackptr;
  void *funcaddr;
  void *calladdr;
} callstack_t;

#define CALLSTACK_MAX_DEPTH 1024

static callstack_t __callstack[CALLSTACK_MAX_DEPTH];
static int __callstack_depth = 0;

static inline void callstack_push(int tid, void *stackptr, void *funcaddr, void *calladdr) {
  (void)tid;
  assert(__callstack_depth >= 0 && __callstack_depth < CALLSTACK_MAX_DEPTH);

  __callstack_depth++;
  __callstack[__callstack_depth - 1].stackptr = stackptr;
  __callstack[__callstack_depth - 1].funcaddr = funcaddr;
  __callstack[__callstack_depth - 1].calladdr = calladdr;
}

static inline void callstack_pop(int tid, void *stackptr, void **funcaddr, void **calladdr) {
  (void)tid;
  assert(__callstack_depth >= 0 && __callstack_depth < CALLSTACK_MAX_DEPTH);

  while (__callstack_depth > 0 && __callstack[__callstack_depth - 1].stackptr != stackptr) {
    __callstack_depth--;
  }

  assert(__callstack_depth >= 0 && __callstack_depth < CALLSTACK_MAX_DEPTH);

  if (__callstack_depth > 0 && __callstack[__callstack_depth - 1].stackptr == stackptr) {
    *calladdr = __callstack[__callstack_depth - 1].calladdr;
    *funcaddr = __callstack[__callstack_depth - 1].funcaddr;
    __callstack_depth--;
  } else {
    *calladdr = *funcaddr = NULL;
  }
  return;
}

static inline void callstack_top(int tid, void **funcaddr, void **calladdr) {
  (void)tid;
  assert(__callstack_depth >= 0 && __callstack_depth < CALLSTACK_MAX_DEPTH);

  if (__callstack_depth > 0) {
    *calladdr = __callstack[__callstack_depth - 1].calladdr;
    *funcaddr = __callstack[__callstack_depth - 1].funcaddr;
  } else {
    *calladdr = *funcaddr = NULL;
  }
  return;
}

static inline void *callstack_top_funcaddr(int tid) {
  (void)tid;
  assert(__callstack_depth >= 0 && __callstack_depth < CALLSTACK_MAX_DEPTH);

  if (__callstack_depth > 0) {
    return __callstack[__callstack_depth - 1].funcaddr;
  } else {
    return NULL;
  }
}

static inline int callstack_depth(int tid) {
  (void)tid;
  assert(__callstack_depth >= 0 && __callstack_depth < CALLSTACK_MAX_DEPTH);

  return __callstack_depth;
}

#endif
