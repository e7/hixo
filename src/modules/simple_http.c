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


#include "app_module.h"


static void simple_http_handle_connect(hixo_socket_t *p_sock);
static void simple_http_handle_read(hixo_socket_t *p_sock);
static void simple_http_handle_write(hixo_socket_t *p_sock);
static void simple_http_handle_disconnect(hixo_socket_t *p_sock);

static hixo_app_module_ctx_t s_simple_http_ctx = {
    &simple_http_handle_connect,
    &simple_http_handle_read,
    &simple_http_handle_write,
    &simple_http_handle_disconnect,
};

hixo_module_t g_simple_http_module = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    HIXO_MODULE_APP,
    &s_simple_http_ctx,
};


// ***** app module interface *****
static void test_syn_send(hixo_socket_t *p_sock);

void simple_http_handle_connect(hixo_socket_t *p_sock)
{
    return;
}

void simple_http_handle_read(hixo_socket_t *p_sock)
{
    while (p_sock->m_readable) {
        int tmp_err;
        ssize_t left_size;
        uint8_t *p_buf;
        ssize_t recved_size;

        hixo_buffer_clean(&p_sock->m_readbuf);
        if (hixo_buffer_full(&p_sock->m_readbuf)) {
            if (HIXO_ERROR == hixo_expand_buffer(&p_sock->m_readbuf)) {
                break;
            }
        }

        assert(!hixo_buffer_full(&p_sock->m_readbuf));
        left_size = hixo_get_buffer_capacity(&p_sock->m_readbuf)
                        - p_sock->m_readbuf.m_size;
        p_buf = hixo_get_buffer_data(&p_sock->m_readbuf);

        errno = 0;
        recved_size = recv(p_sock->m_fd,
                           &p_buf[p_sock->m_readbuf.m_size],
                           left_size,
                           0);
        tmp_err = errno;

        if (recved_size > 0) {
            continue;
        } else if (0 == recved_size) {
            p_sock->m_close = 1U;
            break;
        } else {
            if (ECONNRESET == tmp_err) {
                p_sock->m_close = 1U;
            } else {
                if (EAGAIN != tmp_err) {
                    (void)fprintf(stderr,
                                  "[ERROR] recv failed: %d\n",
                                  tmp_err);
                }
                p_sock->m_readable = 0U;
                test_syn_send(p_sock);
            }
            break;
        }
    }

    return;
}

void simple_http_handle_write(hixo_socket_t *p_sock)
{
    return;
}

void simple_http_handle_disconnect(hixo_socket_t *p_sock)
{
    return;
}

void test_syn_send(hixo_socket_t *p_sock)
{
    intptr_t tmp_err;
    ssize_t sent_size;
    struct iovec iovs[2];
    static uint8_t data_head[] = "HTTP/1.1 200 OK\r\n"
                                 "Server: hixo\r\n"
                                 "Content-Length: 174\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Connection: keep-alive\r\n\r\n";
    static uint8_t data_body[] = "<!DOCTYPE html>\r\n"
                                 "<html>\r\n"
                                 "<head>\r\n"
                                 "<title>welcome to hixo</title>\r\n"
                                 "</head>\r\n"
                                 "<body bgcolor=\"white\" text=\"black\">\r\n"
                                 "<center><h1>welcome to hixo!</h1></center>\r\n"
                                 "</body>\r\n"
                                 "</html>\r\n";

    sent_size = 0;
    iovs[0].iov_base = data_head;
    iovs[0].iov_len = sizeof(data_head);
    iovs[1].iov_base = data_body;
    iovs[1].iov_len = sizeof(data_body);
    while (sent_size < sizeof(data_head) + sizeof(data_body)) {
        ssize_t tmp_sent;

        errno = 0;
        tmp_sent = writev(p_sock->m_fd, iovs, 2);
        tmp_err = errno;
        if (tmp_err) {
            return;
        } else {
            sent_size += tmp_sent;
        }
    }

    (void)shutdown(p_sock->m_fd, SHUT_WR);
    p_sock->m_close = 1U;

    return;
}
