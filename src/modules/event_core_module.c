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
#include "core_module.h"


static hixo_core_module_ctx_t s_event_core_ctx = {};


static int event_core_init(void)
{
    int rslt = HIXO_ERROR;
    int valid_sockets = 0;
    int tmp_err = 0;
    hixo_socket_t *p_listeners = NULL;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;
    struct sockaddr_in srv_addr;

    s_event_core_ctx.mp_private = calloc(p_conf->m_nservers,
                                         sizeof(hixo_socket_t));
    if (NULL == s_event_core_ctx.mp_private) {
        goto ERR_OUT_OF_MEM;
    }
    p_listeners = (hixo_socket_t *)s_event_core_ctx.mp_private;

    for (int i = 0; i < p_conf->m_nservers; ++i) {
        errno = 0;
        p_listeners[i].m_fd = socket(PF_INET, SOCK_STREAM, 0);
        if (-1 == p_listeners[i].m_fd) {
            tmp_err = errno;
        } else {
            tmp_err = 0;
            ++valid_sockets;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }

    for (int i = 0; i < p_conf->m_nservers; ++i) {
        int ret = 0;

        if (-1 == p_listeners[i].m_fd) {
            continue;
        }

        errno = 0;
        ret = unblocking_fd(p_listeners[i].m_fd);
        if (-1 == ret) {
            tmp_err = errno;

            (void)close(p_listeners[i].m_fd);
            p_listeners[i].m_fd = -1;
            --valid_sockets;
        } else {
            tmp_err = 0;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }

    (void)memset(&srv_addr, 0, sizeof(srv_addr));
    for (int i = 0; i < p_conf->m_nservers; ++i) {
        int ret = 0;

        if (-1 == p_listeners[i].m_fd) {
            continue;
        }

        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = htonl(p_conf->mppc_srv_addrs[i]->m_ip);
        srv_addr.sin_port = htons(p_conf->mppc_srv_addrs[i]->m_port);
        errno = 0;
        ret = bind(p_listeners[i].m_fd,
                   (struct sockaddr *)&srv_addr,
                   sizeof(srv_addr));
        if (-1 == ret) {
            tmp_err = errno;

            (void)close(p_listeners[i].m_fd);
            p_listeners[i].m_fd = -1;
            --valid_sockets;

            fprintf(stderr, "[WARNING] bind() failed: %d\n", tmp_err);
        } else {
            tmp_err = 0;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }

    for (int i = 0; i < p_conf->m_nservers; ++i) {
        int ret = 0;

        if (-1 == p_listeners[i].m_fd) {
            continue;
        }

        errno = 0;
        ret = listen(p_listeners[i].m_fd,
                     (p_conf->mppc_srv_addrs[i]->m_backlog > 0)
                         ? p_conf->mppc_srv_addrs[i]->m_backlog
                         : SOMAXCONN);
        if (-1 == ret) {
            tmp_err = errno;

            (void)close(p_listeners[i].m_fd);
            p_listeners[i].m_fd = -1;
            --valid_sockets;
        } else {
            tmp_err = 0;
        }
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_SOCKETS;
    }

    g_rt_ctx.mp_listeners = p_listeners; // 挂载到运行时上下文

    do {
        fprintf(stderr, "[INFO] count of listeners: %d\n", valid_sockets);
        rslt = HIXO_OK;
        break;

ERR_CREATE_SOCKETS:
        free(s_event_core_ctx.mp_private);
        s_event_core_ctx.mp_private = NULL;

ERR_OUT_OF_MEM:
        fprintf(stderr, "[ERROR] create sockets failed: %d\n", tmp_err);
        break;
    } while (0);

    return rslt;
}

static void event_core_exit(void)
{
    hixo_conf_t *p_conf;
    hixo_socket_t *p_listeners;

    if (NULL == g_rt_ctx.mp_conf) {
        return;
    }
    if (NULL == s_event_core_ctx.mp_private) {
        return;
    }

    p_conf = g_rt_ctx.mp_conf;
    p_listeners = (hixo_socket_t *)s_event_core_ctx.mp_private;
    for (int i = 0; i < p_conf->m_nservers; ++i) {
        if (-1 == p_listeners[i].m_fd) {
            continue;
        }
        (void)close(p_listeners[i].m_fd);
    }

    free(s_event_core_ctx.mp_private);
    s_event_core_ctx.mp_private = NULL;

    return;
}


hixo_module_t g_event_core_module = {
    HIXO_MODULE_CORE,
    &s_event_core_ctx,
    &event_core_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &event_core_exit,
};
