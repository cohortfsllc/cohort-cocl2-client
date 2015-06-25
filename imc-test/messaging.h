/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


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
                       int fds[], int fds_len,
                       bool include_cocl2_header,
                       ...);

extern int getFdFromControl(void* control,
                            size_t control_len,
                            int control_index,
                            int fd_index);

#endif // MESSAGING_H
