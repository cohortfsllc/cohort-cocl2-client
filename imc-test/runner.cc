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
#define CONN_BUFF_LEN 64
#define RETURN_BUFF_LEN 2048
#define CONTROL_LEN 32


const int TOKEN_MAX_LEN = 128;
char* SEL_LDR_PATH_TOKEN = "SEL_LDR_PATH_TOKEN";
char* IRT_PATH_TOKEN = "IRT_PATH_TOKEN";
char* NEXE_PATH_TOKEN = "NEXE_PATH_TOKEN";
char* BOOTSTRAP_SOCKET_ADDR_TOKEN = "BOOTSTRAP_SOCKET_ADDR_TOKEN";
char* IMC_FD_TOKEN = "IMC_FD_TOKEN";


char* cmd_vec2[] = {
    SEL_LDR_PATH_TOKEN,
    "-a", // allow file access plus some other syscalls
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
        printf("%d: %hhu %c\n", i, (unsigned char) b[i], b[i]);
    }
}


int my_execv(std::string exec_path, std::vector<std::string> cmd_vec) {
    char** cmd_ary = new char*[cmd_vec.size()];
    for (int i = 0; i < cmd_vec.size(); ++i) {
        cmd_ary[i] = new char[1 + cmd_vec[i].length()];
        strcpy(cmd_ary[i], cmd_vec[i].c_str());
    }

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


int sendMessage(const int fd,
                void* buff, int buff_len,
                void* control, int control_len) {
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

    int len_out = sendmsg(fd, &header, 0);
    if (len_out < 0) {
        perror("server sendmsg error");
    }

    return len_out;
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

        int sent_len = sendMessage(socket_fd,
                                   message_buff, message_len,
                                   NULL, 0);
        std::cout << "sendMessage sent message of length " << sent_len <<
            " expecting a value of " << message_len << std::endl;


        char buff[RETURN_BUFF_LEN];
        int control[CONTROL_LEN];

        int buff_len = RETURN_BUFF_LEN;
        int control_len = CONTROL_LEN * sizeof(int);

        int recv_len =
            receiveMessage(socket_fd, buff, buff_len, control, control_len);
        std::cout << "receiveMessage sent message of length " << recv_len <<
            " with control length of " << control_len << std::endl;

        printBuffer(buff, recv_len);
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

    while(1) {
        int length = receiveMessage(socket_fd,
                                    buffer, buffer_len,
                                    control, control_len);
        std::cout << "handleConnection received message of length " <<
            length << std::endl;

        int offset = -1;
        for (int i ; i < length + COCL2_BANNER_LEN; i += 16) {
            if (!memcmp(COCL2_BANNER, &buffer[i], COCL2_BANNER_LEN)) {
                offset = i;
                break;
            }
        }

        if (offset >= 0) {
            handleMessage(buffer + offset + COCL2_BANNER_LEN,
                          length - offset - COCL2_BANNER_LEN,
                          control,
                          control_len);
        } else {
            std::cerr << "No embedded message found." << std::endl;

            printBuffer(buffer, length);

            std::cerr << "handleConnection received " << control_len <<
                " fds" << std::endl;

            if (control_len > 0) {
                printBuffer(control, control_len);
                // createTesterThread(control[0]);
            }
        }
    }

    return NULL; // thread exits
}




int acceptConnectionsNotUsed(const int socket_fd, pthread_t& thread_id) {
    int fd = accept(socket_fd, NULL, NULL);
    assert(fd >= 0);

    struct RegistrationArgs* conn_args =
        (struct RegistrationArgs *) malloc(sizeof(struct RegistrationArgs));
    conn_args->socket_fd = fd;

    int rv = pthread_create(&thread_id, NULL, handleConnection, conn_args);
    assert(0 == rv);

    return 0;
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


// ++++++
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
        displayCommandLine(cmd_line);
        int rv = my_execv(cmd_line[0], cmd_line);
        return -1; // only executed if error
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

    const bool debug = cmdOptionExists(argv, argv + argc, "-d");
    const char* sel_ldr_path = getCmdOption(argv, argv + argc, "-s");
    const char* irt_path = getCmdOption(argv, argv + argc, "-i");
    const char* nexe_path = getCmdOption(argv, argv + argc, "-n");
    // char* nexe_path = "imc_test_client_x86_64.nexe";

    if (!sel_ldr_path || !irt_path || !nexe_path) {
        usage(argv[0]);
        exit(1);
    }

    std::vector<std::string> cmd_line;

    cmd_line.push_back(sel_ldr_path);

    // allow file access plus some other syscalls
    cmd_line.push_back("-a");

    if (debug) {
        cmd_line.push_back("-c");  // debug_mode_ignore_validator
        // cmd_line.push_back("-c");  // repeating skips entirely
        cmd_line.push_back("-d");  // debug_mode_startup_signal
        cmd_line.push_back("-g");  // enable_debug_stub
    }

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
