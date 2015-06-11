#include <string>
#include <iostream>

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
    rv = srandom_r(args-> randomSeed, &rdata);
    assert(0 == rv);

    uuid_t uuid;
    uuid_generate_random(uuid);

    char* objectName = "Bob";

    uint32_t osdsReturned[16];
    
    for (int i = 0; i < args->callCount; ++i) {
        // sleep for randomized time
        int32_t variance = 0;
        rv = random_r(&rdata, &variance);
        assert(0 == rv);
        variance %= (1 + args->secsVariance);
        sleep(variance + args->secsBetweenCalls);

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

        for (int i = 0; i < osdsRequested; ++i) {
            std::cout << i << ": " << osdsReturned[i] << std::endl;
        }
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

    for (int i = 0; i < threadCount; ++i) {
        auto args = new TestCallingArgs();
        args->algorithmName = algorithmName;
        args->callCount = callsPerThread;
        args->secsBetweenCalls = secsBetweenCalls;
        args->secsVariance = secsVariance;
        args->randomSeed = time(NULL) + i;

        int rv = pthread_create(&threads[i],
                                NULL,
                                testCallingThread,
                                &args);
        assert(0 == rv);
    }

    for (int i = 0; i < threadCount; ++i) {
        int rv = pthread_join(threads[i], NULL);
        if (0 != rv) {
            std::cerr << "Got error from pthread_join " << rv << std::endl;
        }
    }

    delete[] threads;
    std::cout << "All calling testing threads complete." << std::endl;
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

        sleep(5); // REMOVE ME
        std::cout << "About to send message." << std::endl;

        int sent_len = sendMessage(socket_fd,
                                   true,
                                   NULL, 0,
                                   message_buff, message_len,
                                   NULL);
        std::cout << "sendCoCl2Message sent message of length " << sent_len <<
            " expecting a value of " << message_len << std::endl;


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
            
        std::cout << "receiveCoCl2Message sent message of length " <<
            recv_len <<
            " with control length of " << control_len << std::endl;

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

    std::cout << "Thread created to test placer." << std::endl;

    return 0;
}