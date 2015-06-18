/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#include <string>
#include <iostream>
#include <sstream>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <uuid/uuid.h>

#include "runner.h"
#include "messaging.h"
#include "debug.h"

#include "testing.h"


#define CONN_BUFF_LEN   1024
#define RETURN_BUFF_LEN 2048
#define CONTROL_LEN        8


struct TestCallingArgs {
    int threadNumber;
    std::string algorithmName;
    int callCount;
    int secsBetweenCalls;
    int32_t secsVariance;
    unsigned int randomSeed;
};


// runs in its own thread
void* testCallingThread(void* args_temp) {
    int rv;
    TestCallingArgs* args = (TestCallingArgs*) args_temp;

    struct random_data rdata;
    char random_buff[64];
    rv = initstate_r(args->randomSeed,
                     random_buff, sizeof(random_buff),
                     &rdata);
    assert(0 == rv);

    uuid_t uuid;
    uuid_generate_random(uuid);

#if 0
    char uuid_text[256];
    uuid_unparse(uuid, uuid_text);
#endif

    char* objectName = "Bob";

    uint32_t osdsReturned[16];

    std::stringstream out;
    int32_t sleepBaseMicroS = 1000000 * args->secsBetweenCalls;
    
    for (int i = 0; i < args->callCount; ++i) {
        // sleep for randomized time
        int32_t variance = 0;
        rv = random_r(&rdata, &variance);
        assert(0 == rv);
        variance %= 1000000 * args->secsVariance;
        rv = usleep(variance + sleepBaseMicroS);
        assert(0 == rv);

        int32_t osdsRequested;
        rv = random_r(&rdata, &osdsRequested);
        assert(0 == rv);
        osdsRequested = 3 + (osdsRequested % 4); // request 3-6

        rv = calculateOsds(args->algorithmName,
                           uuid,
                           objectName,
                           osdsRequested,
                           osdsReturned);
        assert(0 == rv);

        // since this is multi-threaded, put output into a
        // stringstream until it's all assembled and then send to
        // std::cout at once.
        out.str("");
        out << "Thread " << args->threadNumber << ":    ";
        for (int i = 0; i < osdsRequested; ++i) {
            if (i) out << ", ";
            out << osdsReturned[i];
        }
        out << std::endl;
        std::cout << out.str();

    }

    delete args;

    return NULL;
}


int createTestCallingThreads(const std::string& algorithmName,
                             int threadCount,
                             int callsPerThread,
                             int secsBetweenCalls,
                             int32_t secsVariance) {
    pthread_t* threads = new pthread_t[threadCount];
    uint32_t randomSeedBase = time(NULL);

    for (int i = 0; i < threadCount; ++i) {
        auto args = new TestCallingArgs();
        args->threadNumber = i;
        args->algorithmName = algorithmName;
        args->callCount = callsPerThread;
        args->secsBetweenCalls = secsBetweenCalls;
        args->secsVariance = secsVariance;
        args->randomSeed = randomSeedBase + i;

        int rv = pthread_create(&threads[i],
                                NULL,
                                testCallingThread,
                                args);
        assert(0 == rv);
    }

    for (int i = 0; i < threadCount; ++i) {
        int rv = pthread_join(threads[i], NULL);
        if (0 != rv) {
            ERROR("Got error from pthread_join: %d", rv);
        }
    }

    delete[] threads;

    INFO("All Calling testing threads complete.");

    return 0;
}


// runs in its own thread
void* testConnectionThread(void* args_temp) {
    TestConnectionArgs* args = (TestConnectionArgs*) args_temp;
    
    int socket_fd = args->placer_fd;

    delete args;
    
    int rv;
    char message_buff[1024];

    for (int i = 0; i < 4; ++i) {
        snprintf(message_buff, 1024, "Hello Planet %d %d %d!", i, i, i);
        int message_len = 1 + strlen(message_buff);

        int sent_len = sendMessage(socket_fd,
                                   NULL, 0,
                                   true,
                                   message_buff, message_len,
                                   NULL);
        INFO("sendCoCl2Message sent message of length %d expecting a "
             "value of %d",
             sent_len, message_len);

        char buff[RETURN_BUFF_LEN];
        int control[CONTROL_LEN];

        int buff_len = RETURN_BUFF_LEN;
        int control_len = CONTROL_LEN * sizeof(int);

        int bytes_to_skip = 0;
        int recv_len = receiveCoCl2Message(socket_fd,
                                           buff, buff_len,
                                           control, control_len,
                                           bytes_to_skip);

        char* buff_in = buff + bytes_to_skip;

        INFO("receiveCoCl2Message sent message of length %d with "
             "control length of %d",
             recv_len, control_len);

        printBuffer(buff_in, recv_len);
    } // for loop X 4

    return NULL;
}


int createTestConnectionThread(int placement_fd) {
    auto tester_args = new TestConnectionArgs();
    tester_args->placer_fd = placement_fd;

    pthread_t thread_id;

    int rv = pthread_create(&thread_id,
                            NULL,
                            testConnectionThread,
                            tester_args);
    assert(0 == rv);

    INFO("Thread created to test placer.");

    return 0;
}
