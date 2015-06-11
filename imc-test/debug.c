#include <stdio.h>


bool debug_flag = true;


void printBuffer(void* buffer, int buffer_len, int skip = 0) {
    char* b = (char*) buffer;
    for (int i = skip; i < buffer_len; ++i) {
        printf("%d: %3hhu %2hhx %c\n", i, (unsigned char) b[i], b[i], b[i]);
    }
}
