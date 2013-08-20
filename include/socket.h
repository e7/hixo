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
#if DEBUG_FLAG
    hixo_resource_t *mp_belong;
#endif // DEBUG_FLAG

    int m_fd;
    void (*mpf_read_handler)(hixo_socket_t *);
    void (*mpf_write_handler)(hixo_socket_t *);
    void (*mpf_disconnect_handler)(hixo_socket_t *);
    list_t m_node;
    list_t m_posted_node;
    int m_event_types;
    dlist_t m_readbuf_queue;
    dlist_t m_writebuf_queue;
    unsigned int m_stale : 1;
    unsigned int m_readable : 1;
    unsigned int m_writable : 1;
    unsigned int m_close : 1;
};

typedef enum {
    HIXO_LISTEN_SOCKET = 1,
    HIXO_CMNCT_SOCKET,
} hixo_sock_type_t;

extern int hixo_create_socket(hixo_socket_t *p_sock,
                              int fd,
                              hixo_sock_type_t type,
                              void (*pf_read_handler)(hixo_socket_t *),
                              void (*pf_write_handler)(hixo_socket_t *),
                              void (*pf_disconnect_handler)(hixo_socket_t *));
extern void hixo_socket_nodelay(hixo_socket_t *p_sock);
extern void hixo_socket_unblock(hixo_socket_t *p_sock);
#define hixo_socket_shutdown(p_sock) (void)shutdown(p_sock->m_fd, SHUT_WR)
static inline void hixo_socket_close(hixo_socket_t *p_sock)
{
    p_sock->m_close = 1U;
}
extern void hixo_destroy_socket(hixo_socket_t *p_sock);
#endif // __SOCKET_H__
