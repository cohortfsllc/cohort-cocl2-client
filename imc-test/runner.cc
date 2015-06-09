#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "cocl2_bridge.h"


struct RegistrationArgs {
    int socket_fd;
};


struct TesterArgs {
    int placer_fd;
};


const int IMC_HANDLE = 12;
#define CONN_BUFF_LEN 1024
#define RETURN_BUFF_LEN 2048
#define CONTROL_LEN 32


const int TOKEN_MAX_LEN = 128;
char* SEL_LDR_PATH_TOKEN = "SEL_LDR_PATH_TOKEN";
char* IRT_PATH_TOKEN = "IRT_PATH_TOKEN";
char* NEXE_PATH_TOKEN = "NEXE_PATH_TOKEN";
char* BOOTSTRAP_SOCKET_ADDR_TOKEN = "BOOTSTRAP_SOCKET_ADDR_TOKEN";
char* IMC_FD_TOKEN = "IMC_FD_TOKEN";

bool debug_trusted = false;


/*
 * The next two functions are for simplified command line argument
 * parsing. See:
 * http://stackoverflow.com/questions/865668/parse-command-line-arguments
 */


char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

/*
 * Used for command line argument parsing; see previous function.
 */

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}


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


void replaceToken(std::vector<std::string> & cmd_line,
                  const std::string & token,
                  const std::string & value) {
    std::replace(cmd_line.begin(),
                 cmd_line.end(),
                 token,
                 value);
}


void displayCommandLine(char const* const* cmd_vec) {
    for (char const* const* cursor = cmd_vec; NULL != *cursor; ++cursor) {
        if (cursor != cmd_vec) printf("    ");
        printf("%s\n", *cursor);
    }
}

void displayCommandLine(std::vector<std::string> cmd_line) {
    for (auto i = cmd_line.begin();
         i != cmd_line.end();
         ++i) {
        if (i != cmd_line.begin()) {
            std::cout << " ";
        }
        std::cout << *i;
    }
    std::cout << std::endl;
}


void printBuffer(void* buffer, int buffer_len, int skip = 0) {
    char* b = (char*) buffer;
    for (int i = skip; i < buffer_len; ++i) {
        printf("%d: %3hhu %2hhx %c\n", i, (unsigned char) b[i], b[i], b[i]);
    }
}


int my_execv(std::string exec_path, std::vector<std::string> cmd_vec) {
    char** cmd_ary = new char*[1 + cmd_vec.size()];
    for (int i = 0; i < cmd_vec.size(); ++i) {
        cmd_ary[i] = new char[1 + cmd_vec[i].length()];
        strcpy(cmd_ary[i], cmd_vec[i].c_str());
    }
    cmd_ary[cmd_vec.size()] = NULL;

    displayCommandLine(cmd_ary);

    int rv = execvp(exec_path.c_str(), cmd_ary);

    // If call succeeds, we won't end up here, and memory will be
    // freed due to ending of this program. If we get here, though,
    // the call failed and we should clean up.
    
    perror("execvp returned when it should not have");
    if (errno == EFAULT) {
        std::cerr << "Error is EFAULT" << std::endl;
    } else {
        std::cerr << "Error is *NOT* EFAULT" << std::endl;
    }

    for (int i = 0; i < cmd_vec.size(); ++i) {
        delete[] cmd_ary[i];
    }
    delete[] cmd_ary;
    
    return rv;
}


int receiveMessage(const int fd,
                   void* buff, int buff_len,
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
        return len_in;
    }

    control_len = header.msg_controllen;

    return len_in;
}


int receiveNaClMessage(const int fd,
                       void* buff, int buff_len,
                       void* control, int& control_len,
                       int& bytes_to_skip) {
    int length = receiveMessage(fd,
                                buff, buff_len,
                                control, control_len);

    std::cout << "receiveMessage returned length of " << length <<
        std::endl;

    // propogate error
    if (length < 0) {
        return length;
    }

    NaClInternalHeaderCoCl2 * header =
        (NaClInternalHeaderCoCl2 *) buff;
    if (NACL_HANDLE_TRANSFER_PROTOCOL_COCL2
        != header->h.xfer_protocol_version) {
        std::cerr << "BAD NACL MESSAGE" << std::endl;
        errno = EPROTONOSUPPORT;
        return -errno;
    }

    bytes_to_skip = sizeof(NaClInternalHeaderCoCl2) +
        header->h.descriptor_data_bytes;

    return length - bytes_to_skip;
}


int receiveCoCl2Message(const int fd,
                        void* buff, int buff_len,
                        void* control, int& control_len,
                        int& bytes_to_skip) {
    int length = receiveNaClMessage(fd,
                                    buff, buff_len,
                                    control, control_len,
                                    bytes_to_skip);
    std::cout << "receiveNaClMessage returned length of " << length <<
        std::endl;

    // propogate error
    if (length < 0) {
        return length;
    }

    char* buffer = ((char*) buff) + bytes_to_skip;

    CoCl2Header * header = (CoCl2Header *) buffer;
    if (COCL2_PROTOCOL
        != header->h.xfer_protocol_version) {
        std::cerr << "BAD COCL2 MESSAGE" << std::endl;
        errno = EPROTONOSUPPORT;
        return -errno;
    }

    bytes_to_skip += sizeof(CoCl2Header);

    return length - sizeof(CoCl2Header);
}


int sendMessage(const int fd,
                void* buff, int buff_len,
                void* control, int control_len,
                bool is_cocl2_message = false) {
    struct msghdr header;

    struct iovec message[3];

    header.msg_iov = message;
    if (!is_cocl2_message) {
        message[0].iov_base = buff;
        message[0].iov_len = buff_len;
        header.msg_iovlen = 1;
    } else {
        message[0].iov_base = getNaClHeader();
        message[0].iov_len = sizeof(NaClInternalHeaderCoCl2);
        message[1].iov_base = getCoCl2Header();
        message[1].iov_len = sizeof(CoCl2Header);
        message[2].iov_base = buff;
        message[2].iov_len = buff_len;
        header.msg_iovlen = 3;
    }
    
    header.msg_name = NULL;
    header.msg_namelen = 0;
    header.msg_control = control;
    header.msg_controllen = control_len;
    header.msg_flags = 0;

    int len_out = sendmsg(fd, &header, 0);
    if (len_out < 0) {
        perror("server sendmsg error");
    }

    return len_out;
}


int sendCoCl2Message(const int fd,
                     void* buff, int buff_len,
                     void* control, int control_len) {
    int rv = sendMessage(fd, buff, buff_len, control, control_len, true);
    return rv - sizeof(CoCl2Header) - sizeof(NaClInternalHeaderCoCl2);
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


// runs in its own thread
void* testConnection(void* args) {
    int rv;
    int socket_fd = ((struct TesterArgs*) args)->placer_fd;
    free(args);

    char message_buff[1024];

    for (int i = 0; i < 4; ++i) {
        snprintf(message_buff, 1024, "Hello Planet %d %d %d!", i, i, i);
        int message_len = 1 + strlen(message_buff);

        sleep(5); // REMOVE ME
        std::cout << "About to send message." << std::endl;

        int sent_len = sendCoCl2Message(socket_fd,
                                        message_buff, message_len,
                                        NULL, 0);
        std::cout << "sendCoCl2Message sent message of length " << sent_len <<
            " expecting a value of " << message_len << std::endl;


        char buff[RETURN_BUFF_LEN];
        int control[CONTROL_LEN];

        int buff_len = RETURN_BUFF_LEN;
        int control_len = CONTROL_LEN * sizeof(int);

        int bytes_to_skip = 0;
        int recv_len = receiveCoCl2Message(socket_fd,
                                           buff, buff_len,
                                           control, control_len,
                                           bytes_to_skip);

        char* buff_in = buff + bytes_to_skip;
            
        std::cout << "receiveCoCl2Message sent message of length " <<
            recv_len <<
            " with control length of " << control_len << std::endl;

        printBuffer(buff_in, recv_len);
    } // for loop X 4

    return NULL;
}


int createTesterThread(int placement_fd) {
    struct TesterArgs* tester_args =
        (struct TesterArgs *) calloc(sizeof(struct TesterArgs), 1);
    tester_args->placer_fd = placement_fd;

    pthread_t thread_id;

    int rv = pthread_create(&thread_id, NULL, testConnection, tester_args);
    assert(0 == rv);

    std::cout << "Thread created to test placer." << std::endl;

    return 0;
}


void handleMessage(char buffer[], int buffer_len,
                   int control[], int control_len) {
    if (!strncmp("REGISTER", buffer, 8)) {
        char* name = buffer + 8 + 1;
        std::cout << "Found placement algorithm with name " <<
            name << std::endl;
        std::cout << "File descriptor number " << control[4] <<
            " found" << std::endl;
        int rv = createTesterThread(control[4]);
        assert(0 == rv);
    } else {
        std::cerr << "No valid message found." << std::endl;
    }
}


// runs in a separate thread
void* handleConnection(void* arg) {
    std::cout << "In thread to receive messages." << std::endl;

    int socket_fd = ((struct RegistrationArgs*) arg)->socket_fd;
    free(arg);

    char buffer[CONN_BUFF_LEN];
    int control[CONTROL_LEN];

    int buffer_len = CONN_BUFF_LEN;
    int control_len = CONTROL_LEN * sizeof(int);

    while (1) {
        int bytes_to_skip = 0;
        int bytes_received = receiveCoCl2Message(socket_fd,
                                                 buffer, buffer_len,
                                                 control, control_len,
                                                 bytes_to_skip);
        if (bytes_received < 0) {
            perror("Illegal connection message received");
            continue;
        }

        char* data = buffer + bytes_to_skip;
        if ((data + bytes_received) > (buffer + buffer_len)) {
            std::cerr << "WARNING: data bytes more than buffer bytes" <<
                std::endl;
            continue;
        }

        handleMessage(data, bytes_received,
                      control, control_len);
    }

    return NULL; // thread exits
}


int createMessagesThread(const int socket_fd, pthread_t& thread_id) {
    struct RegistrationArgs* conn_args =
        (struct RegistrationArgs *) malloc(sizeof(struct RegistrationArgs));
    conn_args->socket_fd = socket_fd;

    int rv = pthread_create(&thread_id, NULL, handleConnection, conn_args);
    assert(0 == rv);

    std::cout << "Thread created to receive messages." << std::endl;

    return 0;
}


int forkWithBoundSocket(std::vector<std::string> cmd_line,
                        pthread_t& thread_id,
                        pid_t& child_pid) {
    int rv;
    int bound_pair[2];

    rv = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, bound_pair);
    assert(rv == 0);

    const int socket_fd = bound_pair[0];
    const int socket_addr = bound_pair[1];

    rv = createMessagesThread(socket_fd, thread_id);
    assert(rv == 0);

    if (!(child_pid = fork())) {
        char imc_fd_str[128];
        assert(snprintf(imc_fd_str, 128, "%d:%d", IMC_HANDLE, socket_addr) > 0);

        char socket_addr_str[128];
        assert(snprintf(socket_addr_str, 128, "%d", IMC_HANDLE) > 0);

        // child code
        replaceToken(cmd_line, IMC_FD_TOKEN, imc_fd_str);
        replaceToken(cmd_line, BOOTSTRAP_SOCKET_ADDR_TOKEN, socket_addr_str);
        // displayCommandLine(cmd_line);

        int rv = my_execv(cmd_line[0], cmd_line);
        return -1; // only executed if error
    }

    if (debug_trusted) {
        std::cout << "sudo gdb " << cmd_line[0] << " " <<
            child_pid << std::endl;
    }

    // server code
    std::cout << "child pid is " << child_pid << std::endl;

    return 0;
}


void usage(const char* command) {
    std::cerr << "Usage: " << command <<
        " [-d] -s sel_ldr-path -i irt-path -n nexe-path" << std::endl;
    std::cerr << "    -d = turn on debugging GDB hooks" << std::endl;
}


int main(int argc, char* argv[]) {
    int rv;

    const bool debug_untrusted = cmdOptionExists(argv, argv + argc, "-d");
    debug_trusted = cmdOptionExists(argv, argv + argc, "-D");
    const char* sel_ldr_path = getCmdOption(argv, argv + argc, "-s");
    const char* irt_path = getCmdOption(argv, argv + argc, "-i");
    const char* nexe_path = getCmdOption(argv, argv + argc, "-n");
    const char* log_path = getCmdOption(argv, argv + argc, "-l");
    const char* verbosity_level = getCmdOption(argv, argv + argc, "-v");

    if (!sel_ldr_path || !irt_path || !nexe_path) {
        usage(argv[0]);
        exit(1);
    }

    std::vector<std::string> cmd_line;

    cmd_line.push_back(sel_ldr_path);

    if (debug_trusted) {
        // allows debugging of sel_ldr
        cmd_line.push_back("--r_debug=0xXXXXXXXXXXXXXXXX");
    }

    if (debug_untrusted) {
        cmd_line.push_back("-c");  // debug_mode_ignore_validator
        // cmd_line.push_back("-c");  // repeating skips entirely
        cmd_line.push_back("-d");  // debug_mode_startup_signal
        cmd_line.push_back("-g");  // enable_debug_stub
    }

    if (log_path) {
        cmd_line.push_back("-l");
        cmd_line.push_back(log_path);
    }

    if (verbosity_level) {
        int level = atoi(verbosity_level);
        for (int i = 0; i < level; ++i) {
            cmd_line.push_back("-v");
        }
    }

    // allow file access plus some other syscalls
    cmd_line.push_back("-a");

    // cmd_line.push_back("-Q");  // skip_qualification

    // IMC handle
    cmd_line.push_back("-i");
    cmd_line.push_back(IMC_FD_TOKEN);  // to be replaced

    // cmd_line.push_back("-v");  // verbosity

    // add additional ELF file
    cmd_line.push_back("-B");
    cmd_line.push_back(irt_path);
    
    // cmd_line.push_back("-Ecity=ann_arbor");  // example env var

    cmd_line.push_back("--");
    cmd_line.push_back(nexe_path);
    cmd_line.push_back(BOOTSTRAP_SOCKET_ADDR_TOKEN); // to be replaced

    pthread_t thread_id;
    pid_t child_pid;

    rv = forkWithBoundSocket(cmd_line, thread_id, child_pid);
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
