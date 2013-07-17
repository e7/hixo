// Copyright [2013] [E7, ryuuzaki.uchiha@gmail.com]

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef __SOCKET_H__
#define __SOCKET_H__


#include "list.h"
#include "buffer.h"


#define HIXO_EVENT_IN           EPOLLIN
#define HIXO_EVENT_OUT          EPOLLOUT
#define HIXO_EVENT_ERR          EPOLLERR
#define HIXO_EVENT_HUP          EPOLLHUP
#define HIXO_EVENT_FLAGS        EPOLLET


typedef struct s_socket_t hixo_socket_t;
struct s_socket_t {
    int m_fd;
    void (*mpf_read_handler)(hixo_socket_t *);
    void (*mpf_write_handler)(hixo_socket_t *);
    list_t m_node;
    list_t m_posted_node;
    int m_event_types;
    hixo_buffer_t m_readbuf;
    hixo_buffer_t m_writebuf;
    unsigned int m_stale : 1;
    unsigned int m_readable : 1;
    unsigned int m_writable : 1;
    unsigned int m_closed : 1;
};

typedef enum {
    HIXO_LISTEN_SOCKET = 1,
    HIXO_CMNCT_SOCKET,
} hixo_sock_type_t;

extern int hixo_create_socket(hixo_socket_t *p_sock,
                              int fd,
                              hixo_sock_type_t type,
                              void (*pf_read_handler)(hixo_socket_t *),
                              void (*pf_write_handler)(hixo_socket_t *));
extern void hixo_destroy_socket(hixo_socket_t *p_sock);
#endif // __SOCKET_H__