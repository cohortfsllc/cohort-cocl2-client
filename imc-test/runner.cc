#include <iostream>
// #include <string>
// #include <vector>

#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

// #include "native_client/src/trusted/desc/nrd_desc_imc.h"
// #include "native_client/src/trusted/desc/nrd_xfer.h"


const int IMC_HANDLE = 12;


const int TOKEN_MAX_LEN = 128;
char* SEL_LDR_PATH_TOKEN = "SEL_LDR_PATH_TOKEN";
char* IRT_PATH_TOKEN = "IRT_PATH_TOKEN";
char* NEXE_PATH_TOKEN = "NEXE_PATH_TOKEN";
char* BOOTSTRAP_SOCKET_ADDR_TOKEN = "BOOTSTRAP_SOCKET_ADDR_TOKEN";
char* IMC_FD_TOKEN = "IMC_FD_TOKEN";


#if GDB
#warning GDB hooks turned on
#endif


char* cmd_vec[] = {
    SEL_LDR_PATH_TOKEN,
#if GDB
    "-c", // debug_mode_ignore_validator
    // "-c", // repeating skips entirely
    "-d", // debug_mode_startup_signal
    "-g", // enable_debug_stub
#endif
    // "-Q", // skip_qualification
    "-i",
    IMC_FD_TOKEN, // IMC handle : socket_addr
    // "-v", // verbosity
    "-B",
    IRT_PATH_TOKEN,
    "-Ecity=ann_arbor", // example of setting environment variable
    "--",
    NEXE_PATH_TOKEN,
    BOOTSTRAP_SOCKET_ADDR_TOKEN,
    NULL,
};


void replaceToken(char** token_list,
                  const char* token,
                  char* value) {
    char** cursor = token_list;
    while (NULL != *cursor) {
        if (0 == strncmp(token, *cursor, TOKEN_MAX_LEN)) {
            *cursor = value;
        }
        ++cursor;
    }
}


void displayCommandLine() {
    for (char const* const* cursor = cmd_vec; NULL != *cursor; ++cursor) {
        if (cursor != cmd_vec) printf("    ");
        printf("%s\n", *cursor);
    }
}


#if 0
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
#endif


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


#if 0
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
#endif


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


#if 0
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
#endif


struct ConnectionArgs {
    int socket_fd;
};


void* handleConnection(void* arg) {
    std::cout << "In thread to receive messages." << std::endl;

    int socket_fd = ((struct ConnectionArgs*) arg)->socket_fd;
    free(arg);

#define CONN_BUFF_LEN 64
    char buffer[CONN_BUFF_LEN];
    int control[32];

    int buffer_len = CONN_BUFF_LEN;
    int control_len = 32 * sizeof(int);

    for (int count = 0; count < 2; count++) {
        int length = receiveMessage(socket_fd,
                                    buffer, buffer_len,
                                    control, control_len);

        std::cout << "handleConnection received message of length " <<
            length << std::endl;

        for (int i = 16; i < length; ++i) {
            printf("%d: %hhu %c\n", i, (unsigned char) buffer[i], buffer[i]);
        }
    }

    return NULL; // thread exits
}


int acceptConnections(const int socket_fd, pthread_t& thread_id) {
    int fd = accept(socket_fd, NULL, NULL);
    assert(fd >= 0);

    struct ConnectionArgs* conn_args =
        (struct ConnectionArgs *) malloc(sizeof(struct ConnectionArgs));
    conn_args->socket_fd = fd;

    int rv = pthread_create(&thread_id, NULL, handleConnection, conn_args);
    assert(0 == rv);

    return 0;
}


int receiveMessages(const int socket_fd, pthread_t& thread_id) {
    struct ConnectionArgs* conn_args =
        (struct ConnectionArgs *) malloc(sizeof(struct ConnectionArgs));
    conn_args->socket_fd = socket_fd;

    int rv = pthread_create(&thread_id, NULL, handleConnection, conn_args);
    assert(0 == rv);

    std::cout << "Thread created to receive messages." << std::endl;

    return 0;
}


// ++++++
int forkWithBoundSocket(pthread_t& thread_id, pid_t& child) {
    int rv;
    int bound_pair[2];

    rv = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, bound_pair);
    assert(rv == 0);

    const int socket_fd = bound_pair[0];
    const int socket_addr = bound_pair[1];

    // rv = acceptConnections(socket_fd, thread_id);
    rv = receiveMessages(socket_fd, thread_id);
    assert(rv == 0);

    char imc_fd_str[128];
    assert(snprintf(imc_fd_str, 128, "%d:%d", IMC_HANDLE, socket_addr) > 0);

    char socket_addr_str[128];
    assert(snprintf(socket_addr_str, 128, "%d", IMC_HANDLE) > 0);

    if (!(child = fork())) {
        // child code
        replaceToken(cmd_vec, IMC_FD_TOKEN, imc_fd_str);
        replaceToken(cmd_vec, BOOTSTRAP_SOCKET_ADDR_TOKEN, socket_addr_str);
        displayCommandLine();
        int rv = execvp(cmd_vec[0], cmd_vec);
        perror("execvp returned when it should not have");
        return -1; // never executed
    }

    // server code
    std::cout << "child pid is " << child << std::endl;

    return 0;
}


#if 0
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
#endif


int main(int argc, char* argv[]) {
    if (4 != argc) {
        std::cerr << "Usage: " << argv[0] <<
            " sel_ldr-path irt-path nexe-path" << std::endl;
        exit(1);
    }

    int rv;

    char* sel_ldr_path = argv[1];
    char* irt_path = argv[2];
    // const char const * nexe_path = argv[3];
    char* nexe_path = "imc_test_client_x86_64.nexe";

    replaceToken(cmd_vec, SEL_LDR_PATH_TOKEN, sel_ldr_path);
    replaceToken(cmd_vec, IRT_PATH_TOKEN, irt_path);
    replaceToken(cmd_vec, NEXE_PATH_TOKEN, nexe_path);

    pthread_t thread_id;
    pid_t child_pid;

    rv = forkWithBoundSocket(thread_id, child_pid);
    assert(0 == rv);

    std::cout << "server is waiting for thread to join..." << std::endl;
    rv = pthread_join(thread_id, NULL);
    std::cout << "...thread joined" << std::endl;
    assert(0 == rv);

    std::cout << "server is waiting..." << std::endl;
    int status;
    pid_t pid_rv = waitpid(child_pid, &status, 0);
    assert(pid_rv == child_pid);

    std::cout << "Child exiting with status " << status << std::endl;

    return 0;
}
