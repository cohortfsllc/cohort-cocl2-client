/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <assert.h>


extern bool debug_flag;
extern char* debug_header;


#define DEBUG(format, ...) \
    do {                                                                \
        if (debug_flag) {                                               \
            fprintf(stderr, format, debug_header, ##__VA_ARGS__ );      \
        }                                                               \
    } while(0)

#define INFO(format, ...) DEBUG("INFO %s: " format "\n", ##__VA_ARGS__ )
#define ERROR(format, ...) DEBUG("ERROR %s: " format "\n", ##__VA_ARGS__ )


extern void printBuffer(void* buffer, int buffer_len, int skip = 0);


// runs for side effects and propogates an error code up if one is received
#define RETURN_IF_ERROR(statement)              \
    do {                                        \
        int rv = statement;                     \
        if (debug_flag) assert(!rv); \
        else if (rv) return rv;      \
    } while(0)


#endif // DEBUG_H
