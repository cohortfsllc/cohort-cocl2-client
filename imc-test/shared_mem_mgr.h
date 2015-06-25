#ifndef SHARED_MEM_MGR_H
#define SHARED_MEM_MGR_H

#include <pthread.h>

#include <map>


class SharedMemMgr;


class SharedMemObj {

    friend SharedMemMgr;

private:
    uint32_t id;
    int fd;
    std::string name;
    size_t size;
    void* addr;

protected:

    SharedMemObj(uint32_t id, size_t size);

public:
        
    virtual ~SharedMemObj();

    uint32_t getId() const { return id; }
    int getFd() const { return fd; }
    size_t getSize() const { return size; }
    void* getAddr() const { return addr; }
};


class SharedMemMgr {
private:
    std::map<int,SharedMemObj*> map;
    pthread_rwlock_t lock;

protected:
    std::map<int,SharedMemObj*>::iterator find(int id);
    void realDestroy(std::map<int,SharedMemObj*>::iterator obj);

public:
    typedef std::map<int,SharedMemObj*>::const_iterator const_iterator;

    SharedMemMgr();
    virtual ~SharedMemMgr();

    const SharedMemObj* create(uint32_t id, size_t size);
    const SharedMemObj* get(uint32_t id);
    int destroy(uint32_t id);

    int size() const { return map.size(); }
    const_iterator cbegin() const { return map.cbegin(); }
    const_iterator cend() const { return map.cend(); }
};


#endif // SHARED_MEM_MGR_H
