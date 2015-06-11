#ifndef MESSAGING_H
#define MESSAGING_H

extern int receiveMessage(const int fd,
                          void* buff, int buff_len,
                          void* control, int& control_len);

extern int receiveNaClMessage(const int fd,
                              void* buff, int buff_len,
                              void* control, int& control_len,
                              int& bytes_to_skip);

extern int receiveCoCl2Message(const int fd,
                               void* buff, int buff_len,
                               void* control, int& control_len,
                               int& bytes_to_skip);

extern int sendMessage(const int fd,
                       void* control, int control_len,
                       bool include_cocl2_header,
                       ...);

#endif // MESSAGING_H
