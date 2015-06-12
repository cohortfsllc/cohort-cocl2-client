#include <map>
#include <pthread.h>

#include "alg_map.h"


pthread_rwlock_t AlgorithmInfo::algMapLock = PTHREAD_RWLOCK_INITIALIZER;
std::map<std::string, AlgorithmInfo> AlgorithmInfo::algMap;


void AlgorithmInfo::addAlgorithm(const AlgorithmInfo& alg_info) {
    pthread_rwlock_wrlock(&AlgorithmInfo::algMapLock);
    AlgorithmInfo::algMap[alg_info.name] = alg_info;
    pthread_rwlock_unlock(&AlgorithmInfo::algMapLock);
}


const AlgorithmInfo* AlgorithmInfo::getAlgorithm(const std::string& alg_name) {
    pthread_rwlock_rdlock(&AlgorithmInfo::algMapLock);
    auto it = AlgorithmInfo::algMap.find(alg_name);
    pthread_rwlock_unlock(&AlgorithmInfo::algMapLock);

    if (it == AlgorithmInfo::algMap.end()) {
        return NULL;
    } else {
        return &it->second;
    }
}
