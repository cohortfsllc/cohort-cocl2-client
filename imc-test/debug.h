#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>


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


#endif // DEBUG_H
