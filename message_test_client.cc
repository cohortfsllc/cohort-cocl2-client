#include <iostream>
#include "string.h"

#include "sys/socket.h"
#include "sys/mman.h"

#include "nacl_io/nacl_io.h"
#include "nacl_io/kernel_intercept.h"

#include "unistd.h"
#include "assert.h"

#if !SHM_TEST && !STREAM_TEST && !DGRAM_TEST && !SRPC_TEST
#error must define either SHM_TEST, STREAM_TEST, DGRAM_TEST, or SRPC_TEST
#endif

const bool print_args = false;
const bool print_env_vars = false;

struct iovec {
    void *iov_base;	/* Pointer to data.  */
    size_t iov_len;	/* Length of data.  */
};

void printArray(char* array[], const char* label) {
    char** cursor = array;
    int count = 0;
    while (*cursor) {
        std::cout << label << " " << count++ << " is " << *cursor << std::endl;
        ++cursor;
    }
    std::cout << "There were " << count << " of " << label << "." << std::endl;
}

void doTransaction(const int fd) {
    struct msghdr header_in;
    int len_in = recvmsg(fd, &header_in, 0);

    if (-1 == len_in) {
        perror("Client trying to receive message");
        return;
    }

    std::cout << "client received message of length " << len_in << std::endl;
    for (int m = 0; m < header_in.msg_iovlen; ++m) {
        std::cout << "[" << m << "]: ";
        char* base = (char* ) header_in.msg_iov[m].iov_base;
        for (int n = 0; n < header_in.msg_iov[m].iov_len; ++n) {
            std::cout << base[n];
        }
        std::cout << std::endl;
    }

    int len_out = sendmsg(fd, &header_in, 0);
    std::cout << "client sent message of length " << len_out << std::endl;
}


int myGetChar(int fd) {
    char c;
    ssize_t size = read(fd, &c, 1);
    if (-1 == size) {
        perror("client reading character");
        return EOF;
    } else if (0 == size) {
        return EOF;
    } else {
        return (int) c;
    }
}


void readAllChars(const int fd) {
    int count = 0;
    std::cout << "Reading from " << fd << "..." << std::endl;
    int c;
    while ((c = myGetChar(fd))) {
        if (EOF == c) {
            std::cout << "got EOF" << std::endl;
            break;
        }
        std::cout << count++ << " : " << (char) c << std::endl;
    }
    std::cout << "Done reading." << std::endl;
}


int recvDgram(const int fd, void* buff, size_t buff_len) {
    std::cout << "client about to call recv using fd " << fd << std::endl;
    int len_in = recv(fd, buff, buff_len, 0);
    if (len_in < 0) {
        perror("client recvmsg error");
        return len_in;
    }

    std::cout << "client received message of length " << len_in <<
        "first buffer has length of " << buff_len << std::endl;

    return len_in;
}


int recvDgram2(const int fd, void* buff, int& buff_len) {
    struct msghdr header_out;

    struct iovec message[1];
    message[0].iov_base = buff;
    message[0].iov_len = buff_len;
    
    header_out.msg_name = NULL;
    header_out.msg_namelen = 0;
    header_out.msg_iov = message;
    header_out.msg_iovlen = 1;
    header_out.msg_control = NULL;
    header_out.msg_controllen = 0;
    header_out.msg_flags = 0;

    int len_in = recvmsg(fd, &header_out, 0);
    if (len_in < 0) {
        perror("client recvmsg error");
        return len_in;
    }

    buff_len = message[0].iov_len;

    std::cout << "client received message of length " << len_in <<
        "first buffer has length of " << buff_len << std::endl;

    return len_in;
}


void destructiveCheckFds(int start, int end) {
    std::cout << "checking fds" << std::endl;
    for (int fd = start; fd <= end; ++fd) {
        if (fd == 1 || fd == 2) {
            std::cout << "skipping stdout/stderr" << std::endl;
            continue;
        }

        errno = 0;
        int bad1 = close(fd);
        bool bad2 = errno == EBADF;

        std::cout << "fd: " << fd << ": ";
        if (bad1 < 0) {
            std::cout << "BAD -- " << strerror(errno) << std::endl;
        } else {
            std::cout << "GOOD" << std::endl;
        }
    }
    std::cout << "done checking fds" << std::endl;
}


void srpcTest() {
    std::cout << "Doing SRPC TEST" << std::endl;
}


void streamTest(const int fd) {
    std::cout << "client will read from " << fd << std::endl;
    readAllChars(fd);

    assert(0 == close(fd));
    std::cout << "client closed " << fd << std::endl;
}

int send_message(int sock_fd, char *data, size_t data_size,
                 int *fds, int fds_size) {
    struct msghdr msg;
    struct iovec iov;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov.iov_base = data;
    iov.iov_len = data_size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = fds;
    msg.msg_controllen = fds_size;

    int result = ki_sendmsg(sock_fd, &msg, 0);

    return result;
}


int receive_message(int sock_fd, void* data, size_t data_size,
                    int* fds, int fds_size, int* fds_got) {
    struct msghdr msg;
    struct iovec iov;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov.iov_base = data;
    iov.iov_len = data_size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = fds;
    msg.msg_controllen = fds_size;

    int received = ki_recvmsg(sock_fd, &msg, 0);
    if (fds_got != NULL) {
        *fds_got = msg.msg_controllen;
    }

    return received;
}


void dgramTest(const int fd) {
    char buffer[200];
    int buffer_len = sizeof(buffer);
    int fds[1];
    int fds_received;
    int read_size = receive_message(fd,
                                    (void*) buffer, buffer_len,
                                    fds, 1, &fds_received);

    if (read_size < 0) {
        perror("client trying to receive datagram");
    } else {
        std::cout << "receive got " << read_size << " chars and "
                  << fds_received << " file descriptors" << std::endl;
    }
}


#if 0
int sharedMemoryTest(const int imc_fd) {
    int myReturn = 0;
    int shm_size = 100;
    char init_value = 0xDA;

    int mem_fd = imc_mem_obj_create(100);
    if (-1 == mem_fd) {
        std::cerr << "could not create shared memory object" << std::endl;
        myReturn = 1;
        goto done;
    }

    {
        void* address = mmap((void *) 0,
                             shm_size,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             mem_fd,
                             0);
        if (NULL == address) {
            std::cerr << "could not map shared memory object" << std::endl;
            myReturn = 1;
            goto done_close;
        }

        {
            char* c = (char *) address;
            for (int i = 0; i < shm_size; ++i) {
                c[i] = init_value;
            }

            int message_len = 100;
            char* message = (char *) calloc(message_len, 1);
            strncpy(message, "hello", message_len);

            int fds_len = 1;
            int* fds = (int *) calloc(fds_len, sizeof(int));
            fds[0] = mem_fd;
            int* fds_received = NULL;

            int rv = send_message(imc_fd, message, strlen(message), fds, 1);

            std::cout << "client send message got " << rv << std::endl;
            if (rv < 0) {
                std::cerr << "was unable to send message from client to server"
                          << std::endl;
                myReturn = 1;
                goto done_free;
            }

            rv = receive_message(imc_fd,
                                 message, message_len,
                                 fds, fds_len, fds_received);
            rv = receive_message(imc_fd,
                                 message, message_len,
                                 fds, fds_len, fds_received);

            std::cout << "client receive message got " << rv << std::endl;
            if (rv < 0) {
                std::cerr << "was unable to send message from client to server"
                          << std::endl;
                myReturn = 1;
                goto done_free;
            }

        done_free:
    
            free(fds);
            free(message);
            munmap(address, shm_size);
        }

    done_close:

        close(mem_fd);
    }

 done:

    return myReturn;
}
#endif


int main(int argc, char* argv[], char* arge[]) {
#ifdef SRPC_TEST
    srpcTest();
    return 0;
#endif
    const int fd = 5;

    if (print_args) {
        std::cout << "argc is " << argc << std::endl;
        printArray(argv, "argument");
    }

    if (print_env_vars) {
        printArray(arge, "env_var");
    }


    // #define NACL_IO 1
    // #define KI 1

#if NACL_IO
    assert(0 == nacl_io_init());
#endif

#if KI
    ki_init(NULL);
#endif

#if SHM_TEST
    sharedMemoryTest(fd);
#elif STREAM_TEST
    streamTest(fd);
#elif DGRAM_TEST
    dgramTest(fd);
#endif

#if KI
    ki_uninit();
#endif

#if NACL_IO
    assert(0 == nacl_io_uninit());
#endif

    return 0;
}
