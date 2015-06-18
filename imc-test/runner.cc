/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include "irt_cocl2.h"
#include "cocl2_bridge.h"

#include "messaging.h"
#include "call_return.h"
#include "alg_info.h"
#include "debug.h"
#include "testing.h"

#include "runner.h"


#define CONN_BUFF_LEN   1024
#define RETURN_BUFF_LEN 2048
#define CONTROL_LEN        8

const int IMC_HANDLE = 12;

const int TOKEN_MAX_LEN = 128;
char* SEL_LDR_PATH_TOKEN = "SEL_LDR_PATH_TOKEN";
char* IRT_PATH_TOKEN = "IRT_PATH_TOKEN";
char* NEXE_PATH_TOKEN = "NEXE_PATH_TOKEN";
char* BOOTSTRAP_SOCKET_ADDR_TOKEN = "BOOTSTRAP_SOCKET_ADDR_TOKEN";
char* IMC_FD_TOKEN = "IMC_FD_TOKEN";

bool debug_trusted = false;


static CallReturnHandler callReturnHandler;


typedef struct {
    int socket_fd;
} RegistrationArgs;


typedef struct {
    int socket_fd;
} HandleReturnsArgs;


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


int calculateOsds(const std::string& alg_name,
                  const uuid_t& uuid,
                  const char* object_name,
                  const uint32_t osds_requested,
                  uint32_t osd_list[]) {
    int rv = 0;
    int errorCode = 0;

    const AlgorithmInfo* alg = AlgorithmInfo::getAlgorithm(alg_name);
    if (NULL == alg) {
        ERROR("unknown algorithm %s", alg_name.c_str());
        return -1;
    }

    CallReturnRec& callReturn =
        callReturnHandler.create((char*) osd_list,
                                 osds_requested * sizeof(osd_list[0]));

    OpPlacementCallParams params;
    params.call_params.epoch = callReturn.getEpoch();
    params.call_params.part = -1;
    params.call_params.func_id = FUNC_PLACEMENT;
    params.osds_requested = osds_requested;
    assert(sizeof(uuid_opaque) == sizeof(uuid_t));
    bcopy(&uuid, &params.uuid, sizeof(uuid_opaque));

    rv = sendMessage(alg->getSocket(),
                     NULL, 0,
                     true,
                     OP_CALL, OP_SIZE,
                     &params, sizeof(params),
                     object_name, 1 + strlen(object_name),
                     NULL);
    
    if (rv < 0) {
        rv = -rv;
        perror("Error calling sendMessage");
        goto error;
    }

    rv = callReturn.waitForReturn();
    if (rv) {
        perror("Error waiting for return.");
        goto error;
    }

    errorCode = callReturn.getErrorCode();
    if (errorCode) {
        ERROR("got error code %d back from sandbox", errorCode);
        goto error;
    }

    delete &callReturn;
    return 0;

error:

    delete &callReturn;
    return rv;
}


void* getReturnsThread(void* args_temp) {
    HandleReturnsArgs* args = (HandleReturnsArgs*) args_temp;

    char buff[RETURN_BUFF_LEN];
    int control[CONTROL_LEN];

    int buff_len = RETURN_BUFF_LEN;
    int control_len = CONTROL_LEN * sizeof(int);

    int bytes_to_skip;

    INFO("Thread waiting for returns and errors.");
    while (1) {
        bytes_to_skip = 0;
        int recv_len = receiveCoCl2Message(args->socket_fd,
                                           buff, buff_len,
                                           control, control_len,
                                           bytes_to_skip);
        if (recv_len < 0) {
            ERROR("error receiving message from sandbox");
            INFO("SLEEPING for 5");
            sleep(5);
            continue;
        }

        char* buff_in = buff + bytes_to_skip;
        char* buff_end = buff_in + recv_len;

        if (OPS_EQUAL(buff_in, OP_RETURN)) {
            char* buff2 = buff_in + OP_SIZE;
            OpReturnParams* return_params = (OpReturnParams*) buff2;
            char* buff3 = buff2 + sizeof(OpReturnParams);

            int rv = callReturnHandler.submitReturn(return_params->epoch,
                                                    buff3,
                                                    buff_end - buff3);
            if (rv) {
                ERROR("error calling submitReturn %d", rv);
            }
        } else if (OPS_EQUAL(buff_in, OP_ERROR)) {
            char* buff2 = buff + OP_SIZE;
            OpErrorParams* error_params = (OpErrorParams*) buff2;
            char* error_message = buff2 + sizeof(OpErrorParams);
            ERROR("Got error for epoch %d, error code %d, error message: %s",
                  error_params->ret_params.epoch,
                  error_params->error_code,
                  error_message);
            int rv =
                callReturnHandler.submitError(
                    error_params->ret_params.epoch,
                    error_params->error_code,
                    error_message,
                    buff_end - error_message);
            if (rv) {
                ERROR("error calling submitError %d", rv);
            }
        } else {
            ERROR("return thread got unknown message; here's the buffer");
            printBuffer(buff_in, recv_len);
        }
    } // while (1)

    free(args_temp);
    return NULL;
}


int createHandleReturnsThread(int placement_fd) {
   HandleReturnsArgs* args =
        (HandleReturnsArgs *) calloc(sizeof(HandleReturnsArgs), 1);
    args->socket_fd = placement_fd;

    pthread_t thread_id;

    int rv = pthread_create(&thread_id, NULL, getReturnsThread, args);
    assert(0 == rv);

    return 0;
}


void handleMessage(char buffer[], int buffer_len,
                   int control[], int control_len) {
    if (OPS_EQUAL(OP_REGISTER, buffer)) {
        char* name = buffer + OP_SIZE;
        std::string algorithmName = name;

        INFO("Registered placement algorithm \"%s\".",
             name);

        // socket_fd is in the fifth position
        assert(control_len > 5 * sizeof(int));
        int socket_fd = control[4];

        AlgorithmInfo::addAlgorithm(name, socket_fd);

        int rv;

        rv = createHandleReturnsThread(socket_fd);
        assert(0 == rv);

        rv = createTestCallingThreads(algorithmName, 25, 10, 500, 1000);
        assert(0 == rv);
    } else {
        ERROR("Received invalid message.");
    }
}


// runs in a separate thread
void* handleConnection(void* arg) {
    std::cout << "In thread to receive messages." << std::endl;

    int socket_fd = ((RegistrationArgs*) arg)->socket_fd;
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
    RegistrationArgs* conn_args =
        (RegistrationArgs *) malloc(sizeof(RegistrationArgs));
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
