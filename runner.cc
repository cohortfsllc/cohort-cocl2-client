#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>


#if !SHM_TEST && !STREAM_TEST && !DGRAM_TEST && !SRPC_TEST
#error must define either SHM_TEST, STREAM_TEST, DGRAM_TEST, or SRPC_TEST
#endif

// define one of STREAM_TEST, DGRAM_TEST, SRPC_TEST
// #if !defined(STREAM_TEST) && !defined(DGRAM_TEST) &&  \
//     !defined(SRPC_TEST) && !defined(SHM_TEST)
// #error must define either STREAM_TEST, DGRAM_TEST, or SRPC_TEST
// #endif

#if GDB
#warning GDB turned on
#endif


const char* socketTypeDesc(int socket_type) {
    switch(socket_type) {
    case SOCK_SEQPACKET:
        return "sequential packet";
    case SOCK_STREAM:
        return "stream";
    case SOCK_DGRAM:
        return "datagram";
    default:
        return "UNKNOWN";
    }
}

char* const cmd_vec[] = {
    "/home/ivancich/CoCl2/nacl2/native_client/scons-out/dbg-linux-x86-64/staging/sel_ldr",
#if GDB
    "-c", // debug_mode_ignore_validator
    // "-c", // repeating skips entirely
    "-d", // debug_mode_startup_signal
    "-g", // enable_debug_stub
#endif
    "-Q", // skip_qualification
    // "-v", // verbosity
    "-B",
    "/home/ivancich/CoCl2/nacl/native_client/scons-out/nacl_irt-x86-64/"
    "staging/irt_core.nexe",
#if STREAM_TEST || DGRAM_TEST
    "-h",
    "5:3",
#elif SHM_TEST
    "-i",
    "5:3", // nacl_desc = 5, u.handle = 3, tag = IMC_DESC
#elif SRPC_TEST
    "-i",
    "5:3", // nacl_desc = 5, u.handle = 3, tag = IMC_DESC
    "-X",
    "3", // export_addr_to = 3
#endif
    "-Eann=arbor",
    "--",
    "/home/ivancich/CoCl2/my-tests/message_test_client_x86_64.nexe",
    "It's a",
    "Wonderful",
    "Life!",
    NULL
};


void displayCommandLine() {
    for (char* const* cursor = cmd_vec; NULL != *cursor; ++cursor) {
        if (cursor != cmd_vec) printf("    ");
        printf("%s\n", *cursor);
    }
}


void sendDgram(const int fd, const char* msg) {
    struct msghdr header;

    struct iovec message[1];
    message[0].iov_base = (void*) msg;
    message[0].iov_len = 1+strlen(msg);
    
    header.msg_name = NULL;
    header.msg_namelen = 0;
    header.msg_iov = message;
    header.msg_iovlen = 1;
    header.msg_control = NULL;
    header.msg_controllen = 0;
    header.msg_flags = 0;

    int len_out = sendmsg(fd, &header, 0);

    std::cout << "server sent message of length " << len_out << std::endl;
}


int receiveMessage(const int fd,
                   void* buff, int& buff_len,
                   void* control, int& control_len) {
    struct msghdr header;

    struct iovec message[1];
    message[0].iov_base = buff;
    message[0].iov_len = buff_len;
    
    header.msg_name = NULL;
    header.msg_namelen = 0;
    header.msg_iov = message;
    header.msg_iovlen = 1;
    header.msg_control = control;
    header.msg_controllen = control_len;
    header.msg_flags = 0;

    int len_in = recvmsg(fd, &header, 0);
    if (len_in < 0) {
        perror("server recvmsg error");
        return len_in;
    }

    buff_len = message[0].iov_len;
    control_len = header.msg_controllen;

    std::cout << "server received message of length " << len_in <<
        ", first buffer has length of " << buff_len << 
        ", control has length of " << control_len << std::endl;

    return len_in;
}


void exchangeMessages(const int fd) {
    struct msghdr header_out;

    char* message_content = "hello world\n";

    struct iovec message[1];
    message[0].iov_base = (void*) message_content;
    message[0].iov_len = 1+strlen(message_content);
    
    header_out.msg_name = NULL;
    header_out.msg_namelen = 0;
    header_out.msg_iov = message;
    header_out.msg_iovlen = 1;
    header_out.msg_control = NULL;
    header_out.msg_controllen = 0;
    header_out.msg_flags = 0;

    int len_out = sendmsg(fd, &header_out, 0);

    std::cout << "runner sent message of length " << len_out << std::endl;

    struct msghdr header_in;

    int len_in = recvmsg(fd, &header_in, 0);

    std::cout << "runner received message of length " << len_in << std::endl;
}


void serverReadAllChars() {
    int count = 0;
    std::cout << "Reading..." << std::endl;
    int c;
    while ((c = getchar())) {
        if (EOF == c) {
            std::cout << "got EOF" << std::endl;
            break;
        }
        std::cout << count++ << " : " << (char) c << std::endl;
    }
    std::cout << "Done reading." << std::endl;
}


int setUpFork(int socket_type, pid_t& child, int& server_fd) {
    int fds[2];
    assert(0 == socketpair(AF_UNIX, socket_type, 0, fds));
    std::cout << "socket pair is type " << socketTypeDesc(socket_type) <<
        "; fds: " << fds[0] << " / " << fds[1] << std::endl;

    if (!(child = fork())) {
        // child code
        assert(0 == close(fds[1]));
        std::cout << "client closed " << fds[1] << std::endl;
        displayCommandLine();
        execvp(cmd_vec[0], cmd_vec);
        return 0; // never executed
    }

    std::cout << "child pid is " << child << std::endl;

    assert(0 == close(fds[0]));
    std::cout << "server closed " << fds[0] << std::endl;

    server_fd = fds[1];
    std::cout << "server will use " << server_fd << std::endl;

    return 0;
}


int setUpSrpc(pid_t& child, int& bound_fd) {
    int server_fd = -1;
    int socket_type = SOCK_SEQPACKET;

    setUpFork(socket_type, child, server_fd);

    // NOTE: this is a hacky code, it does not check for errors,
    // so it should not be used in production.

    int control_buff_size = 100;
    int msg_buff_size = 100;


    struct iovec iov;
    iov.iov_base = calloc(msg_buff_size, 1);
    iov.iov_len = msg_buff_size;

    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    int* control = (int*) calloc(control_buff_size, sizeof(int));
    msg.msg_control = control;
    msg.msg_controllen = control_buff_size * sizeof(int);

    msg.msg_flags = 0;

    assert(iov.iov_base);
    assert(msg.msg_control);

    std::cout << "server to receive " << socketTypeDesc(socket_type) <<
        " on " << server_fd << std::endl;
    ssize_t len = recvmsg(server_fd, &msg, 0);
    std::cout << "server received message of size " << len << std::endl;

    bound_fd = control[4]; // This is the socket we wanted to receive
    std::cout << "server received bound fd " << bound_fd << std::endl;
    
    return 0;
}


void srpcTest(pid_t &child_pid) {
    int bound_fd;
    int result = setUpSrpc(child_pid, bound_fd);
    std::cout << "server setUpSrpc returned " << result << std::endl;
}


void sharedMemoryTest(pid_t &child_pid) {
    int socket_fd;
    int result = setUpFork(SOCK_SEQPACKET, child_pid, socket_fd);
    std::cout << "server setUpFork returned " << result <<
        ", socket fd is " << socket_fd << std::endl;

    int buffer_len = 100;
    void* buffer = calloc(buffer_len, 1);
    int control_len = 100 * sizeof(int);
    void* control = calloc(control_len, 1);
    int in_len = receiveMessage(socket_fd,
                                buffer, buffer_len,
                                control, control_len);

    std::cout << "receiveMessage got " << in_len << std::endl;
}


void streamTest(pid_t &child_pid) {
    int socket_fd;
    int result = setUpFork(SOCK_STREAM, child_pid, socket_fd);
    std::cout << "server setUpFork returned " << result <<
        ", socket fd is " << socket_fd << std::endl;

    const char* message = "Hello world!\n";

    assert(strlen(message) == write(socket_fd, message, strlen(message)));
    std::cout << "server done writing to " << socket_fd << std::endl;
    assert(0 == shutdown(socket_fd, SHUT_RDWR));
    assert(0 == close(socket_fd));
    std::cout << "server closed " << socket_fd << std::endl;
    
    // exchangeMessages(fds[0], fds[1]);
}


void dgramTest(pid_t &child_pid) {
    int socket_fd;
    int result = setUpFork(SOCK_DGRAM, child_pid, socket_fd);
    std::cout << "server setUpFork returned " << result <<
        ", socket fd is " << socket_fd << std::endl;

    sendDgram(socket_fd, "Hello world!\n");

    assert(0 == shutdown(socket_fd, SHUT_RDWR));
    assert(0 == close(socket_fd));
    std::cout << "server closed " << socket_fd << std::endl;
    
    // exchangeMessages(fds[0], fds[1]);
}


int main(int argc, char* argv[]) {
    pid_t child_pid;
#if SRPC_TEST
    srpcTest(child_pid);
#elif SHM_TEST
    sharedMemoryTest(child_pid);
#elif STREAM_TEST
    streamTest(child_pid);
#elif DGRAM_TEST
    dgramTest(child_pid);
#else
#error Something not defined.
#endif

    std::cout << "server is waiting..." << std::endl;
    int status;
    waitpid(child_pid, &status, 0);

    std::cout << "Child exiting with status " << status << std::endl;

    return 0;
}
