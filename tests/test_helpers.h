#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(cond, msg) do {                                          \
    if (!(cond)) {                                                      \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
        return 1;                                                       \
    }                                                                   \
} while (0)

#define ASSERT_EQ_LONG(actual, expected, msg) do {                      \
    long _a = (long)(actual);                                           \
    long _e = (long)(expected);                                         \
    if (_a != _e) {                                                     \
        fprintf(stderr, "FAIL %s:%d: %s (got %ld, expected %ld)\n",     \
                __FILE__, __LINE__, (msg), _a, _e);                     \
        return 1;                                                       \
    }                                                                   \
} while (0)

#endif
