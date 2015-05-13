void srpcTest(pid_t &child_pid) {
    int bound_fd;
    int result = setUpSrpc(child_pid, bound_fd);
    std::cout << "server setUpSrpc returned " << result << std::endl;
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

