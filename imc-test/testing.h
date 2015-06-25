/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#ifndef TESTING_H
#define TESTING_H


struct TestConnectionArgs {
    int placer_fd;
};


extern int createTestCallingThreads(const std::string& algorithmName,
                                    int threadCount,
                                    int callsPerThead,
                                    int secsBetweenCalls,
                                    int32_t secsVariance);

extern int createTestConnectionThread(int placement_fd);


extern int testSharedMemObj();


extern void makeTestingSharedMemObj(uint32_t id,
                                    size_t size,
                                    size_t unit_size,
                                    long value);


#endif // TESTING_H
