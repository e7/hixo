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

static int simple_http_init_worker(void);

static void simple_http_handle_connect(hixo_socket_t *p_sock);
static void simple_http_handle_read(hixo_socket_t *p_sock);
static void simple_http_handle_write(hixo_socket_t *p_sock);
static void simple_http_handle_disconnect(hixo_socket_t *p_sock);

static hixo_listen_conf_t sa_simple_http_srvs[] = {
    {"0.0.0.0", 8002, 0},
    {"0.0.0.0", 8003, 0},
};

static hixo_app_module_ctx_t s_simple_http_ctx = {
    &simple_http_handle_connect,
    &simple_http_handle_read,
    &simple_http_handle_write,
    &simple_http_handle_disconnect,
    sa_simple_http_srvs,
    ARRAY_COUNT(sa_simple_http_srvs),
    INIT_DLIST(s_simple_http_ctx.m_node),
};

hixo_module_t g_simple_http_module = {
    NULL,
    &simple_http_init_worker,
    NULL,
    NULL,
    NULL,
    NULL,

    HIXO_MODULE_APP,
    &s_simple_http_ctx,
};


int simple_http_init_worker(void)
{
    for (int i = 0; i < ARRAY_COUNT(sa_simple_http_srvs); ++i) {
        sa_simple_http_srvs[i].m_port = htons(sa_simple_http_srvs[i].m_port);
    }

    return HIXO_OK;
}


// ***** app module interface *****
static void test_syn_send(hixo_socket_t *p_sock);

void simple_http_handle_connect(hixo_socket_t *p_sock)
{
    return;
}

void simple_http_handle_read(hixo_socket_t *p_sock)
{
    test_syn_send(p_sock);
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

    hixo_socket_shutdown(p_sock);
    hixo_socket_close(p_sock);

    return;
}
