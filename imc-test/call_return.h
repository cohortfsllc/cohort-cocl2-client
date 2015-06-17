/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#ifndef CALL_RETURN_H
#define CALL_RETURN_H

#include <list>

#include <pthread.h>


class CallReturnHandler;


class CallReturnRec {
private:
    int             epoch;
    int             errorCode;

    pthread_mutex_t mutex;
    pthread_cond_t  cond;

    char*           resultsBlock;
    size_t          resultsBlockSize;

    // Amount the return wanted to use. If less than resultsBlockSize
    // then only a portion of the return block was used. If greater
    // then the results were truncated.
    size_t          resultsBlockProvided;

protected:

    CallReturnRec(int epoch, char* resultsBlock, size_t resultsBlockSize);

    friend CallReturnHandler;

public:
    virtual ~CallReturnRec();

    int getEpoch() const { return epoch; }
    int waitForReturn();
    int submitReturn(char* resultsBlock, size_t resultsBlockSize);
    int submitError(int errorCode, char* errorMessage, size_t errorMessageSize);
};


class CallReturnHandler {
private:
    std::list<CallReturnRec*> queue;
    int currentEpoch;

    pthread_mutex_t mutex;

    // must hold mutex before calling
    std::list<CallReturnRec*>::iterator findByEpoch(int epoch);

public:
    CallReturnHandler();
    virtual ~CallReturnHandler();

    const CallReturnRec& create(char* resultsBlock, size_t resultsBlockSize);
    int submitReturn(int epoch, char* resultsBlock, size_t resultsBlockSize);
    int submitError(int epoch,
                    int errorCode, char* errorMessage, size_t errorMessageSize);
};


#endif // CALL_RETURN_H
