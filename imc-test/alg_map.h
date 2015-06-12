#ifndef ALG_MAP_H
#define ALG_MAP_H


class AlgorithmInfo {
private:
    std::string name;
    int socket_fd;

    static pthread_rwlock_t algMapLock;
    static std::map<std::string, AlgorithmInfo> algMap;

public:
    AlgorithmInfo()
        : name(""), socket_fd(-1) {
        // empty
    }

    AlgorithmInfo(const std::string& alg_name, int socket_fd)
        : name(alg_name), socket_fd(socket_fd) {
        // empty
    }

    AlgorithmInfo(const AlgorithmInfo& copy_from)
        : name(copy_from.name), socket_fd(copy_from.socket_fd) {
        // empty
    }

    virtual ~AlgorithmInfo() {
        // empty
    }

    AlgorithmInfo& operator=(const AlgorithmInfo& rhs) {
        this->name = rhs.name;
        this->socket_fd = rhs.socket_fd;
        return *this;
    }

    int getSocket() const { return socket_fd; }
    const std::string& getName() const { return name; }

    static void addAlgorithm(const AlgorithmInfo& alg_info);
    static const AlgorithmInfo* getAlgorithm(const std::string& alg_name);
};


#endif // ALG_MAP_H
