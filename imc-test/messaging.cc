#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <iostream>

#include "irt_cocl2.h"
#include "cocl2_bridge.h"
#include "debug.h"

#include "messaging.h"


#define MAX_SEND_IOVEC     8


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
    // propogate error
    if (length < 0) {
        return length;
    }

    NaClInternalHeaderCoCl2 * header =
        (NaClInternalHeaderCoCl2 *) buff;
    if (NACL_HANDLE_TRANSFER_PROTOCOL_COCL2
        != header->h.xfer_protocol_version) {
        ERROR("BAD NACL MESSAGE 0x%04x", header->h.xfer_protocol_version);
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
    // propogate error
    if (length < 0) {
        return length;
    }

    char* buffer = ((char*) buff) + bytes_to_skip;

    CoCl2Header * header = (CoCl2Header *) buffer;
    if (COCL2_PROTOCOL
        != header->h.xfer_protocol_version) {
        ERROR("Bad CoCl2 message 0x%04x", header->h.xfer_protocol_version);
        errno = EPROTONOSUPPORT;
        return -errno;
    }

    bytes_to_skip += sizeof(CoCl2Header);

    return length - sizeof(CoCl2Header);
}


// bool parameter is b/w control and data buffers to help clarity difference
int sendMessage(const int fd,
                void* control, int control_len,
                bool include_cocl2_header,
                ...) {

    struct msghdr header;
    // need 2 extra for NaCL header and potential CoCl2 header
    struct iovec message[2 + MAX_SEND_IOVEC];
    header.msg_iov = message;

    int cursor = 0;

    message[cursor].iov_base = getNaClHeader();
    message[cursor].iov_len = sizeof(NaClInternalHeaderCoCl2);
    ++cursor;

    if (include_cocl2_header) {
        message[cursor].iov_base = getCoCl2Header();
        message[cursor].iov_len = sizeof(CoCl2Header);
        ++cursor;
    }

    va_list access;
    va_start(access, include_cocl2_header);
    for (int count = 0; ; ++count) {
        char* buffer = va_arg(access, char*);
        if (!buffer) {
            break;
        }

        if (count >= MAX_SEND_IOVEC) {
            ERROR("tried to send too many parts (%d) to an iovec", count);
            break;
        }

        int buffer_len = va_arg(access, int);

        message[cursor].iov_base = buffer;
        message[cursor].iov_len = buffer_len;
        ++cursor;
    }
    va_end(access);

    header.msg_iovlen = cursor;
    header.msg_name = NULL;
    header.msg_namelen = 0;
    header.msg_control = control;
    header.msg_controllen = control_len;
    header.msg_flags = 0;

    int len_out = sendmsg(fd, &header, 0);

    return len_out;
}


#if 0
int sendCoCl2Message(const int fd,
                     void* buff, int buff_len,
                     void* control, int control_len) {
    int rv = sendMessage(fd, buff, buff_len, control, control_len, true);
    return rv - sizeof(CoCl2Header) - sizeof(NaClInternalHeaderCoCl2);
}
#endif


