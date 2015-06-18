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
