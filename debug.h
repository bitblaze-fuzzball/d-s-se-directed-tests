#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern int DEBUG_LEVEL;
extern int ASSERT_LEVEL;
extern FILE *DEBUG_FILE;

#define __DEBUG_FILE__ DEBUG_FILE

#define warn(...) \
    { fprintf(__DEBUG_FILE__, __VA_ARGS__); fflush(__DEBUG_FILE__); }

#define __debug__(...) \
    { fprintf(__DEBUG_FILE__, __VA_ARGS__); fflush(__DEBUG_FILE__); }

#define debug(...)				\
    if (DEBUG_LEVEL)				\
	__debug__(__VA_ARGS__);

#define debug1(...)				\
    if (DEBUG_LEVEL >= 1)			\
	__debug__(__VA_ARGS__);

#define debug2(...)				\
    if (DEBUG_LEVEL >= 2)			\
	__debug__(__VA_ARGS__);

#define debug3(...)				\
  if (DEBUG_LEVEL >= 3)				\
	__debug__(__VA_ARGS__);

#define debug4(...)				\
  if (DEBUG_LEVEL >= 4)				\
	__debug__(__VA_ARGS__);

#ifndef NDEBUG
#define __assert__(cond) \
    if (!(cond)) {							\
	fprintf(stderr, "%s:%d: %s:  Assertion `%s' failed.\n",		\
	    __FILE__, __LINE__, __FUNCTION__, #cond);			\
	abort();							\
    }
#else
#define __assert__(cond)
#endif

#ifndef NDEBUG

#undef assert
#define assert(cond)				\
    __assert__(cond)

#define assert1(cond)				\
    if (ASSERT_LEVEL >= 1) __assert__(cond)

#define assert2(cond)				\
    if (ASSERT_LEVEL >= 2) __assert__(cond)

#define assert3(cond)				\
    if (ASSERT_LEVEL >= 3) __assert__(cond)

#define assert4(cond)				\
    if (ASSERT_LEVEL >= 4) __assert__(cond)

#ifndef NDEBUG
#define __assert_msg__(cond, ...)				\
    if (!(cond)) {						\
	fprintf(stderr, "%s:%d: %s:  Assertion `%s' failed.\n",	\
		__FILE__, __LINE__, __FUNCTION__, #cond);	\
	fprintf(stderr, __VA_ARGS__);				\
	fprintf(stderr, "\n");					\
	abort();						\
    }
#else
#define __assert_msg__(cond, ...)
#endif

#define assert_msg(cond, ...)			\
    __assert_msg__(cond, __VA_ARGS__)

#define assert1_msg(cond, ...)					\
    if (ASSERT_LEVEL >= 1) __assert_msg__(cond, __VA_ARGS__)

#define assert2_msg(cond, ...)					\
    if (ASSERT_LEVEL >= 2) __assert_msg__(cond, __VA_ARGS__)

#define assert3_msg(cond, ...)					\
    if (ASSERT_LEVEL >= 3) __assert_msg__(cond, __VA_ARGS__)

#define assert4_msg(cond, ...)					\
    if (ASSERT_LEVEL >= 4) __assert_msg__(cond, __VA_ARGS__)

#else

#undef assert
#define assert(...)
#define assert1(...)
#define assert2(...)
#define assert3(...)
#define assert4(...)
#define assert_msg(...)
#define assert1_msg(...)
#define assert2_msg(...)
#define assert3_msg(...)
#define assert4_msg(...)

#endif

#ifndef DISABLE_DISASM

extern "C" {
#include "xed-interface.h"
}

static const char *disasm(unsigned char *addr) {
    xed_state_t dstate;
    xed_decoded_inst_t xedd;
    static char buf[1024];

    xed_tables_init();

    xed_state_zero(&dstate);
    xed_state_init(&dstate,
                   XED_MACHINE_MODE_LEGACY_32, 
                   XED_ADDRESS_WIDTH_32b, 
                   XED_ADDRESS_WIDTH_32b);

    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
    xed_decode(&xedd, (const xed_uint8_t*) addr, 16);
    xed_decoded_inst_dump_att_format(&xedd, buf, sizeof(buf), 0);

    return buf;
}
#endif

static const char *rawbytes(void *a, size_t len) {
    static char buf[1024];
    unsigned char *ptr = (unsigned char *) a;

    assert(len * 4 + 1 < sizeof(buf));

    buf[0] = '\0';
    for (size_t i = 0; i < len; i++) {
	char tmp[5];
	sprintf(tmp, "\\x%.2x", ptr[i]);
	strcat(buf, tmp);
    }

    return buf;
}

#endif // __DEBUG_H__

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
