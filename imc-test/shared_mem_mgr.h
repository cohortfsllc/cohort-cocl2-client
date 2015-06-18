#ifndef SHARED_MEM_MGR_H
#define SHARED_MEM_MGR_H

#include <map>

class SharedMemObj {
private:
    uint32_t id;
    int fd;
    std::string name;
    size_t size;
    void* addr;

public:
    SharedMemObj(uint32_t id, size_t size);
    virtual ~SharedMemObj();

    int getId() const { return id; }
    int getFd() const { return fd; }
    int getSize() const { return size; }
    void* getAddr() const { return addr; }
};


#endif // SHARED_MEM_MGR_H
