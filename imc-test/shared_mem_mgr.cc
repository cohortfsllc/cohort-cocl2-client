#include <sstream>

#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "shared_mem_mgr.h"


SharedMemObj::SharedMemObj(uint32_t id, size_t size)
    : id(id), fd(-1), size(0), addr(NULL) {
    int rv;

    std::stringstream nameStream;
    pid_t pid = getpid();
    nameStream << "/cohort_" << pid << "_" << id;
    name = nameStream.str();

    fd = shm_open(name.c_str(),
                  O_RDWR | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR);
    assert(fd >= 0);

    rv = ftruncate(fd, size);
    assert(0 == rv);
    this->size = size;

    addr = mmap(NULL,
                size, 
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                fd,
                0);
    assert(MAP_FAILED != addr);
}


SharedMemObj::~SharedMemObj() {
    int rv;

    rv = munmap(addr, size);
    assert(0 == rv);

    rv = close(fd);
    assert(0 == rv);

    rv = shm_unlink(name.c_str());
    assert(0 == rv);
}



// *** SHARED MEMORY MANAGER


SharedMemMgr::SharedMemMgr() {
    lock = PTHREAD_RWLOCK_INITIALIZER;
}


SharedMemMgr::~SharedMemMgr() {
    pthread_rwlock_wrlock(&lock);
    for (auto it = map.begin();
         it != map.end();
         ++it) {
        map.erase(it);
    }
    pthread_rwlock_unlock(&lock);

    pthread_rwlock_destroy(&lock);
}


const SharedMemObj* SharedMemMgr::create(uint32_t id, size_t size) {
    SharedMemObj* obj = new SharedMemObj(id, size);
    pthread_rwlock_wrlock(&lock);
    map[id] = obj;
    pthread_rwlock_unlock(&lock);
    return obj;
}


int SharedMemMgr::destroy(uint32_t id) {
    int rv = 0;
    SharedMemObj* obj = NULL;

    pthread_rwlock_wrlock(&lock);
    auto it = map.find(id);
    if (it != map.end()) {
        map.erase(it);
        obj = it->second;
    } else {
        rv = -1;
    }
    pthread_rwlock_unlock(&lock);

    if (obj) {
        delete obj;
    }

    return rv;
}


const SharedMemObj* SharedMemMgr::get(uint32_t id) {
    SharedMemObj* result = NULL;

    pthread_rwlock_rdlock(&lock);
    auto it = map.find(id);
    if (it != map.end()) {
        result = it->second;
    }
    pthread_rwlock_unlock(&lock);

    return result;
}
