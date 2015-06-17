/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#include <pthread.h>
#include <string.h>

#include "call_return.h"
#include "debug.h"


CallReturnRec::CallReturnRec(int epoch,
                             char* resultsBlock,
                             size_t resultsBlockSize)
    : epoch(epoch),
      resultsBlock(resultsBlock),
      resultsBlockSize(resultsBlockSize),
      errorCode(-1) {
    mutex = PTHREAD_MUTEX_INITIALIZER;
    cond = PTHREAD_COND_INITIALIZER;
}


CallReturnRec::~CallReturnRec() {
    assert(0 == pthread_cond_destroy(&cond));
    assert(0 == pthread_mutex_destroy(&mutex));
}


int CallReturnRec::waitForReturn() {
    RETURN_IF_ERROR(pthread_mutex_lock(&mutex));
    RETURN_IF_ERROR(pthread_cond_wait(&cond, &mutex));
    RETURN_IF_ERROR(pthread_mutex_unlock(&mutex));
}


int CallReturnRec::submitReturn(char* providedBlock,
                                size_t providedBlockSize) {
    RETURN_IF_ERROR(pthread_mutex_lock(&mutex));

    if (providedBlockSize <= this->resultsBlockSize) {
        bcopy(providedBlock, this->resultsBlock, providedBlockSize);
    } else {
        bcopy(providedBlock, this->resultsBlock, this->resultsBlockSize);
    }
    
    this->resultsBlockProvided = providedBlockSize;

    RETURN_IF_ERROR(pthread_cond_signal(&cond));
    RETURN_IF_ERROR(pthread_mutex_unlock(&mutex));

    return 0;
}


int CallReturnRec::submitError(int errorCode,
                               char* errorMessage,
                               size_t errorMessageSize) {
    RETURN_IF_ERROR(pthread_mutex_lock(&mutex));

    this->errorCode = errorCode || -1; // guarantee non-zero code

    ERROR("Error %d: %s",
          errorCode, errorMessage);

    resultsBlockProvided = 0;

    RETURN_IF_ERROR(pthread_cond_signal(&cond));
    RETURN_IF_ERROR(pthread_mutex_unlock(&mutex));

    return 0;
}


CallReturnHandler::CallReturnHandler()
    : currentEpoch(111) {
    mutex = PTHREAD_MUTEX_INITIALIZER;
}


CallReturnHandler::~CallReturnHandler() {
    assert(0 == pthread_mutex_destroy(&mutex));
}


CallReturnRec& CallReturnHandler::create(char* resultsBlock,
                                         size_t resultsBlockSize) {
    assert(0 == pthread_mutex_lock(&mutex));

    CallReturnRec* rec =
        new CallReturnRec(currentEpoch++, resultsBlock, resultsBlockSize);

    queue.push_back(rec);

    assert(0 == pthread_mutex_unlock(&mutex));

    return *rec;
}


int CallReturnHandler::submitReturn(int epoch,
                                    char* resultsBlock,
                                    size_t resultsBlockSize) {
    RETURN_IF_ERROR(pthread_mutex_lock(&mutex));

    auto it = findByEpoch(epoch);

    if (it != queue.end()) {
        queue.erase(it);

        RETURN_IF_ERROR(pthread_mutex_unlock(&mutex));

        int rv = (*it)->submitReturn(resultsBlock, resultsBlockSize);
        delete *it;
        return rv;
    } else {
        RETURN_IF_ERROR(pthread_mutex_unlock(&mutex));

        ERROR("Could not find return record with epoch %d", epoch);
        return -1;
    }
}


int CallReturnHandler::submitError(int epoch,
                                   int errorCode,
                                   char* errorMessage,
                                   size_t errorMessageSize) {
    RETURN_IF_ERROR(pthread_mutex_lock(&mutex));

    auto it = findByEpoch(epoch);
    
    if (it != queue.end()) {
        queue.erase(it);

        RETURN_IF_ERROR(pthread_mutex_unlock(&mutex));

        int rv = (*it)->submitError(errorCode, errorMessage, errorMessageSize);
        delete *it;
        return rv;
    } else {
        RETURN_IF_ERROR(pthread_mutex_unlock(&mutex));

        ERROR("Could not find return record with epoch %d", epoch);
        return -1;
    }
}


std::list<CallReturnRec*>::iterator CallReturnHandler::findByEpoch(int epoch) {
    // mutex must be held by caller
    auto it = queue.begin();
    for ( ; it != queue.end(); ++it) {
        if ((*it)->epoch == epoch) {
            break;
        }
    }
    return it;
}
