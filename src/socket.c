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
                       void (*pf_write_handler)(hixo_socket_t *))
{
    int rslt;

    if (HIXO_LISTEN_SOCKET == type) {
        (void)hixo_create_buffer(&p_sock->m_readbuf, 0);
        (void)hixo_create_buffer(&p_sock->m_writebuf, 0);
        p_sock->m_event_types = HIXO_EVENT_IN | HIXO_EVENT_FLAGS;
    } else if (HIXO_CMNCT_SOCKET == type) {
        if (HIXO_ERROR == hixo_create_buffer(&p_sock->m_readbuf,
                                             g_sysconf.M_PAGE_SIZE))
        {
            goto ERR_READBUF;
        }
        if (HIXO_ERROR == hixo_create_buffer(&p_sock->m_writebuf,
                                             g_sysconf.M_PAGE_SIZE))
        {
            goto ERR_WRITEBUF;
        }
        p_sock->m_event_types = (
            HIXO_EVENT_IN | HIXO_EVENT_OUT | HIXO_EVENT_FLAGS
        );
    } else {
        assert(0);
    }

    p_sock->m_fd = fd;
    p_sock->mpf_read_handler = pf_read_handler;
    p_sock->mpf_write_handler = pf_write_handler;
    p_sock->m_stale = !p_sock->m_stale;
    p_sock->m_readable = 0U;
    p_sock->m_writable = 0U;
    p_sock->m_closed = 0U;

    do {
        rslt = HIXO_OK;
        break;

ERR_WRITEBUF:
        hixo_destroy_buffer(&p_sock->m_readbuf);
ERR_READBUF:
        (void)close(fd);
        rslt = HIXO_ERROR;
        break;
    } while (0);

    return rslt;
}

void hixo_destroy_socket(hixo_socket_t *p_sock)
{
    (void)shutdown(p_sock->m_fd, SHUT_RDWR);
    (void)close(p_sock->m_fd);
    hixo_destroy_buffer(&p_sock->m_readbuf);
    hixo_destroy_buffer(&p_sock->m_writebuf);
}
