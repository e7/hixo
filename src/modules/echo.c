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


#include "adv_string.h"
#include "app_module.h"


// 16k
static int const NBUFS = 4;
static int const BUF_SIZE = 4096;

static int echo_init_worker(void);

static void echo_handle_connect(hixo_socket_t *p_sock);
static void echo_handle_read(hixo_socket_t *p_sock);
static void echo_handle_write(hixo_socket_t *p_sock);
static void echo_handle_disconnect(hixo_socket_t *p_sock);

static hixo_listen_conf_t sa_echo_srvs[] = {
    {"0.0.0.0", 8001, 0},
};

static hixo_app_module_ctx_t s_echo_ctx = {
    &echo_handle_connect,
    &echo_handle_read,
    &echo_handle_write,
    &echo_handle_disconnect,
    sa_echo_srvs,
    ARRAY_COUNT(sa_echo_srvs),
    INIT_DLIST(s_echo_ctx.m_node),
};

hixo_module_t g_echo_module = {
    NULL,
    &echo_init_worker,
    NULL,
    NULL,
    NULL,
    NULL,

    HIXO_MODULE_APP,
    &s_echo_ctx,
};


int echo_init_worker(void)
{
    for (int i = 0; i < ARRAY_COUNT(sa_echo_srvs); ++i) {
        sa_echo_srvs[i].m_port = htons(sa_echo_srvs[i].m_port);
    }

    return HIXO_OK;
}


// ***** app module interface *****
static void test_syn_send(hixo_socket_t *p_sock);

void echo_handle_connect(hixo_socket_t *p_sock)
{
    return;
}

void echo_handle_read(hixo_socket_t *p_sock)
{
    DEFINE_DLIST(old_queue);
    struct iovec *p_vecs = alloca(NBUFS * sizeof(struct iovec));

    // 清空旧缓冲
    dlist_merge(&old_queue, &p_sock->m_readbuf_queue);
    if (!dlist_empty(&old_queue)) {
        dlist_for_each_f (p_pos_node, &old_queue) {
                hixo_buffer_t *p_buf;

                p_buf = CONTAINER_OF(p_pos_node, hixo_buffer_t, m_node);
                hixo_buffer_set_size(p_buf, 0);
        }
    }

    while (p_sock->m_readable) {
        int tmp_err;
        ssize_t recved_size;
        DEFINE_DLIST(tmp_queue);

        // 开迭代缓冲
        for (int i = 0; i < NBUFS; ++i) {
            hixo_buffer_t *p_buf = NULL;

            if (!dlist_empty(&old_queue)) {
                dlist_t *p_node;

                p_node = dlist_get_head(&old_queue);
                assert(NULL != p_node);
                dlist_del(p_node);
                p_buf = CONTAINER_OF(p_node, hixo_buffer_t, m_node);
            } else {
                p_buf = calloc(1, sizeof(hixo_buffer_t));
                hixo_create_buffer(p_buf, BUF_SIZE);
            }
            dlist_add_tail(&tmp_queue, &p_buf->m_node);
            p_vecs[i].iov_base = hixo_buffer_get_data(p_buf);
            p_vecs[i].iov_len = hixo_buffer_get_capacity(p_buf);
        }

        errno = 0;
        recved_size = readv(p_sock->m_fd, p_vecs, NBUFS);
        tmp_err = errno;

        if (recved_size > 0) {
            dlist_for_each_f (p_pos_node, &tmp_queue) {
                hixo_buffer_t *p_buf;

                p_buf = CONTAINER_OF(p_pos_node, hixo_buffer_t, m_node);
                if (0 == recved_size) {
                    break;
                }
                if (recved_size < BUF_SIZE) {
                    hixo_buffer_set_size(p_buf, recved_size);
                    recved_size = 0;
                } else {
                    hixo_buffer_set_size(p_buf, BUF_SIZE);
                    recved_size -= BUF_SIZE;
                }
            }

            // 合并迭代缓冲
            dlist_merge(&p_sock->m_readbuf_queue, &tmp_queue);
            continue;
        }

        // 释放迭代缓冲
        dlist_for_each_f_safe (p_pos_node, p_cur_next, &tmp_queue) {
            hixo_buffer_t *p_buf;

            dlist_del(p_pos_node);
            p_buf = CONTAINER_OF(p_pos_node, hixo_buffer_t, m_node);
            hixo_destroy_buffer(p_buf);
            free(p_buf);
        }

        if (0 == recved_size) {
            hixo_socket_close(p_sock);
            break;
        } else {
            if (ECONNRESET == tmp_err) {
                hixo_socket_close(p_sock);
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

    // 重拾剩余旧缓冲
    dlist_merge(&p_sock->m_readbuf_queue, &old_queue);

    /*void *buf = alloca(1024);
    while (recv(p_sock->m_fd, buf, 1024, 0) > 0) {
    }
    test_syn_send(p_sock);*/

    return;
}

void echo_handle_write(hixo_socket_t *p_sock)
{
    return;
}

void echo_handle_disconnect(hixo_socket_t *p_sock)
{
    if (!dlist_empty(&p_sock->m_readbuf_queue)) {
        dlist_for_each_f_safe (p_pos_node,
                               p_cur_next,
                               &p_sock->m_readbuf_queue)
        {
            hixo_buffer_t *p_buf;

            dlist_del(p_pos_node);
            p_buf = CONTAINER_OF(p_pos_node, hixo_buffer_t, m_node);
            hixo_destroy_buffer(p_buf);
            free(p_buf);
        }
    }
    assert(dlist_empty(&p_sock->m_readbuf_queue));

    return;
}

void test_syn_send(hixo_socket_t *p_sock)
{
/*    static uint8_t data_head[] = "HTTP/1.1 200 OK\r\n"
                                 "Server: hixo\r\n"
                                 "Content-Length: %s\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Connection: keep-alive\r\n\r\n";

    intptr_t tmp_err;
    ssize_t sent_size;
    struct iovec iovs[2];
    char len[32] = {};
    uint8_t *p_data_head = alloca(256);

    (void)snprintf(len, 32, "%s", hixo_buffer_get_size(CONTAINER_OF(p_sock->m_readbuf_queue.mp_next, hixo_buffer_t, m_node)));
    (void)snprintf(p_data_head, 256, data_head, len);
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
    }*/

    hixo_socket_shutdown(p_sock);
    hixo_socket_close(p_sock);

    return;
}
