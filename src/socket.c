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


#include "hixo.h"


int hixo_create_socket(hixo_socket_t *p_sock,
                       int fd,
                       hixo_sock_type_t type,
                       void (*pf_read_handler)(hixo_socket_t *),
                       void (*pf_write_handler)(hixo_socket_t *),
                       void (*pf_disconnect_handler)(hixo_socket_t *))
{
    p_sock->mpf_read_handler = pf_read_handler;
    if (HIXO_LISTEN_SOCKET == type) {
        p_sock->mpf_write_handler = NULL;
        p_sock->mpf_disconnect_handler = NULL;
        p_sock->m_event_types = HIXO_EVENT_IN | HIXO_EVENT_FLAGS;
    } else if (HIXO_CMNCT_SOCKET == type) {
        p_sock->mpf_write_handler = pf_write_handler;
        p_sock->mpf_disconnect_handler = pf_disconnect_handler;
        p_sock->m_event_types = (
            HIXO_EVENT_IN | HIXO_EVENT_OUT | HIXO_EVENT_FLAGS
        );
    } else {
        assert(0);
    }

    p_sock->m_fd = fd;
    dlist_init(&p_sock->m_readbuf_queue);
    dlist_init(&p_sock->m_writebuf_queue);
    p_sock->m_stale = !p_sock->m_stale;
    p_sock->m_readable = 0U;
    p_sock->m_writable = 0U;
    p_sock->m_close = 0U;

    return HIXO_OK;
}

void hixo_socket_nodelay(hixo_socket_t *p_sock)
{
    int tmp_err;
    int tcp_nodelay = 1;

    errno = 0;
    (void)setsockopt(p_sock->m_fd,
                     IPPROTO_TCP,
                     TCP_NODELAY,
                     &tcp_nodelay,
                     sizeof(tcp_nodelay));
    tmp_err = errno;
    if (tmp_err) {
        (void)fprintf(stderr, "[WARNING] tcp_nodelay failed: %d\n", tmp_err);
    }

    return;
}

void hixo_socket_unblock(hixo_socket_t *p_sock)
{
    int tmp_err;

    errno = 0;
    (void)unblocking_fd(p_sock->m_fd);
    tmp_err = errno;
    if (tmp_err) {
        fprintf(stderr,
                "[WARNING] fcntl(%d) failed: %d\n",
                p_sock->m_fd,
                tmp_err);
    }

    return;
}

void hixo_destroy_socket(hixo_socket_t *p_sock)
{
    (void)close(p_sock->m_fd);
}
