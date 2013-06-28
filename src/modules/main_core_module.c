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


#include "conf.h"
#include "hixo.h"
#include "core_module.h"


static hixo_core_module_ctx_t s_main_core_ctx = {};

static int main_core_init(void)
{
    int rslt = HIXO_OK;
    int tmp_err = 0;
    int valid_sockets = 0;
    struct sockaddr_in srv_addr;

    fprintf(stderr, "[INFO] i'm main_core_init\n");
    rslt = create_conf(&g_conf);
    if (HIXO_ERROR == rslt) {
        goto ERR_CREATE_CONF;
    }

    g_rt_ctx.mp_listeners
        = (hixo_socket_t *)calloc(g_conf.m_nservers, sizeof(hixo_socket_t));
    if (NULL == g_rt_ctx.mp_listeners) {
        goto ERR_ALLOC_LISTEN_SOCKETS;
    }

    valid_sockets = 0;
    for (int i = 0; i < g_conf.m_nservers; ++i) {
        int sock_fd = 0;

        errno = 0;
        sock_fd = socket(PF_INET, SOCK_STREAM, 0);
        tmp_err = (-1 == sock_fd) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            g_rt_ctx.mp_listeners[i].m_fd = sock_fd;
            ++valid_sockets;
        } else {
            g_rt_ctx.mp_listeners[i].m_fd = INVALID_FD;

            break;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }
    for (int i = 0; i < g_conf.m_nservers; ++i) {
        int ret = 0;

        if (INVALID_FD ==  g_rt_ctx.mp_listeners[i].m_fd) {
            continue;
        }

        errno = 0;
        ret = unblocking_fd(g_rt_ctx.mp_listeners[i].m_fd);
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS != tmp_err) {
            (void)close(g_rt_ctx.mp_listeners[i].m_fd);
            g_rt_ctx.mp_listeners[i].m_fd = INVALID_FD;
            --valid_sockets;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }
    (void)memset(&srv_addr, 0, sizeof(srv_addr));
    for (int i = 0; i < g_conf.m_nservers; ++i) {
        int ret = 0;

        if (INVALID_FD ==  g_rt_ctx.mp_listeners[i].m_fd) {
            continue;
        }

        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = htonl(g_conf.mppc_srv_addrs[i]->m_ip);
        srv_addr.sin_port = htons(g_conf.mppc_srv_addrs[i]->m_port);

        errno = 0;
        ret = bind(g_rt_ctx.mp_listeners[i].m_fd,
                   (struct sockaddr *)&srv_addr,
                   sizeof(srv_addr));
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS != tmp_err) {
            fprintf(stderr, "[ERROR] bind() failed: %d\n", tmp_err);

            (void)close(g_rt_ctx.mp_listeners[i].m_fd);
            g_rt_ctx.mp_listeners[i].m_fd = INVALID_FD;
            --valid_sockets;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }
    for (int i = 0; i < g_conf.m_nservers; ++i) {
        int ret = 0;

        if (INVALID_FD ==  g_rt_ctx.mp_listeners[i].m_fd) {
            continue;
        }

        errno = 0;
        ret = listen(g_rt_ctx.mp_listeners[i].m_fd,
                     (g_conf.mppc_srv_addrs[i]->m_backlog > 0)
                         ? g_conf.mppc_srv_addrs[i]->m_backlog
                         : SOMAXCONN);
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS != tmp_err) {
            (void)close(g_rt_ctx.mp_listeners[i].m_fd);
            g_rt_ctx.mp_listeners[i].m_fd = INVALID_FD;
            --valid_sockets;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }

    do {
        break;
ERR_CREATE_SOCKETS:
ERR_ALLOC_LISTEN_SOCKETS:
    destroy_conf(&g_conf);
ERR_CREATE_CONF:
        break;
    } while (0);

    return rslt;
}

static void main_core_exit(void)
{
    fprintf(stderr, "[INFO] i'm main_core_exit\n");

    for (int i = 0; i < g_conf.m_nservers; ++i) {
        (void)close(g_rt_ctx.mp_listeners[i].m_fd);
        g_rt_ctx.mp_listeners[i].m_fd = INVALID_FD;
    }
    destroy_conf(&g_conf);

    return;
}

hixo_module_t g_main_core_module = {
    HIXO_MODULE_CORE,
    &s_main_core_ctx,
    &main_core_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &main_core_exit,
};

