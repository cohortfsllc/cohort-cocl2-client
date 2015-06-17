/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#include <stdlib.h>
#include "debug.h"
#include "epoch_gen.h"


EpochGenerator::EpochGenerator()
    : currentEpoch(111) {
    int rv = pthread_mutex_init(&mutex, NULL);
    if (rv) {
        ERROR("could not initialize EpochGenerator mutex");
        exit(1);
    }
}

EpochGenerator::~EpochGenerator() {
    pthread_mutex_destroy(&mutex);
}

uint32_t EpochGenerator::nextEpoch() {
    pthread_mutex_lock(&mutex);
    uint32_t rv = currentEpoch++;
    pthread_mutex_unlock(&mutex);
    return rv;
}
