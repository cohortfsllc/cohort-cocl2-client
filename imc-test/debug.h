#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>


extern bool debug_flag;


#define DEBUG(format, ...) \
    do {                                                                \
        if (debug_flag) {                                                 \
            fprintf(stderr, format, ##__VA_ARGS__ );                    \
        }                                                               \
    } while(0)

#define INFO(format, ...) DEBUG("INFO: " format "\n", ##__VA_ARGS__ )
#define ERROR(format, ...) DEBUG("ERROR: " format "\n", ##__VA_ARGS__ )

#endif // DEBUG_H
