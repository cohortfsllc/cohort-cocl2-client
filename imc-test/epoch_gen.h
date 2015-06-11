#ifndef EPOCH_GEN_H
#define EPOCH_GEN_H


#include <pthread.h>
#include <stdint.h>


class EpochGenerator {
private:
    uint32_t currentEpoch;
    pthread_mutex_t mutex;

public:
    EpochGenerator();
    virtual ~EpochGenerator();
    uint32_t nextEpoch();
};


#endif // EPOCH_GEN_H
