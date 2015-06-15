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
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}


int CallReturnRec::waitForReturn() {
    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}


int CallReturnRec::submitReturn(char* providedBlock,
                                size_t providedBlockSize) {
   pthread_mutex_lock(&mutex);

    if (providedBlockSize <= this->resultsBlockSize) {
        bcopy(providedBlock, this->resultsBlock, providedBlockSize);
    } else {
        bcopy(providedBlock, this->resultsBlock, this->resultsBlockSize);
    }
    
    this->resultsBlockProvided = providedBlockSize;

    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}


int CallReturnRec::submitError(int errorCode,
                               char* errorMessage,
                               size_t errorMessageSize) {
    pthread_mutex_lock(&mutex);

    this->errorCode = errorCode || -1; // guarantee non-zero code

    ERROR("Error %d: %s",
          errorCode, errorMessage);

    resultsBlockProvided = 0;

    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}
