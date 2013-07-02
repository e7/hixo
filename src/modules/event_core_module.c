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
#include "event_module.h"


static struct {
    int m_shmid;
    atomic_t *mp_accept_lock;
    hixo_socket_t *mp_listeners;
    hixo_event_t *mp_listeners_evs;
    hixo_resource_t m_sockets;
    hixo_resource_t m_events;
} s_event_core_private = {
    -1,
};

static hixo_core_module_ctx_t s_event_core_ctx = {
    &s_event_core_private,
};


static int event_core_init_master(void)
{
    int rslt = HIXO_ERROR;
    int valid_sockets = 0;
    int tmp_err = 0;
    hixo_socket_t *p_listeners = NULL;
    hixo_event_t *p_listeners_evs = NULL;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;
    struct sockaddr_in srv_addr;

    errno = 0;
    s_event_core_private.m_shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    tmp_err = errno;
    if (-1 == s_event_core_private.m_shmid) {
        goto ERR_SHMGET;
    }

    p_listeners_evs = (hixo_event_t *)calloc(p_conf->m_nservers,
                                             sizeof(hixo_socket_t));
    if (NULL == p_listeners_evs) {
        goto ERR_CREATE_LISTENERS_EVS;
    }
    s_event_core_private.mp_listeners_evs = p_listeners_evs;
    g_rt_ctx.mp_listeners_evs = p_listeners_evs;

    p_listeners = (hixo_socket_t *)calloc(p_conf->m_nservers,
                                          sizeof(hixo_socket_t));
    if (NULL == p_listeners) {
        goto ERR_CREATE_LISTENERS;
    }

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

    s_event_core_private.mp_listeners = p_listeners;
    g_rt_ctx.mp_listeners = p_listeners;

    do {
        fprintf(stderr, "[INFO] count of listeners: %d\n", valid_sockets);
        rslt = HIXO_OK;
        break;

ERR_CREATE_SOCKETS:
        free(p_listeners);
        s_event_core_private.mp_listeners = NULL;
        g_rt_ctx.mp_listeners = NULL;

ERR_CREATE_LISTENERS:
        free(p_listeners_evs);
        s_event_core_private.mp_listeners_evs = NULL;
        g_rt_ctx.mp_listeners_evs = NULL;

ERR_CREATE_LISTENERS_EVS:
        (void)shmctl(s_event_core_private.m_shmid, IPC_RMID, 0);
        fprintf(stderr, "[ERROR] create sockets failed: %d\n", tmp_err);

ERR_SHMGET:
        fprintf(stderr, "[ERROR] shmget() failed: %d\n", tmp_err);
        break;
    } while (0);

    return rslt;
}

static void event_core_exit_master(void)
{
    hixo_conf_t *p_conf;
    hixo_socket_t *p_listeners;

    if (NULL == g_rt_ctx.mp_conf) {
        return;
    }
    if (NULL == s_event_core_ctx.mp_private) {
        return;
    }

    (void)shmctl(s_event_core_private.m_shmid, IPC_RMID, 0);
    s_event_core_private.m_shmid = -1;

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

static int event_core_init_worker(void)
{
    int rslt = HIXO_ERROR;
    int tmp_err = 0;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    // 映射共享内存
    assert(-1 != s_event_core_private.m_shmid);
    errno = 0;
    s_event_core_private.mp_accept_lock
        = (atomic_t *)shmat(s_event_core_private.m_shmid, 0, 0);
    tmp_err = errno;
    if ((atomic_t *)-1 == s_event_core_private.mp_accept_lock) {
        fprintf(stderr, "[ERROR] shmat() failed: %d\n", tmp_err);
        goto ERR_SHMAT;
    }
    g_rt_ctx.mp_accept_lock = s_event_core_private.mp_accept_lock;

    // 创建套接字和事件资源
    if (HIXO_ERROR == create_resource(&s_event_core_private.m_sockets,
                                      p_conf->m_max_connections,
                                      sizeof(hixo_socket_t),
                                      OFFSET_OF(hixo_socket_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create sockets cache failed\n");
        goto ERR_SOCKETS_CACHE;
    }
    if (HIXO_ERROR == create_resource(&s_event_core_private.m_events,
                                      p_conf->m_max_events,
                                      sizeof(hixo_event_t),
                                      OFFSET_OF(hixo_event_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create events cache failed\n");
        goto ERR_EVENTS_CACHE;
    }

    g_rt_ctx.mp_rs_sockets = &s_event_core_private.m_sockets;
    g_rt_ctx.mp_rs_events = &s_event_core_private.m_events;

    do {
        rslt = HIXO_OK;
        break;

ERR_EVENTS_CACHE:
        destroy_resource(&s_event_core_private.m_sockets);

ERR_SOCKETS_CACHE:
        (void)shmdt(s_event_core_private.mp_accept_lock);
        s_event_core_private.mp_accept_lock = NULL;

ERR_SHMAT:
        break;
    } while (0);

    return rslt;
}

static void event_core_exit_worker(void)
{
    destroy_resource(&s_event_core_private.m_events);
    g_rt_ctx.mp_rs_events = NULL;
    destroy_resource(&s_event_core_private.m_sockets);
    g_rt_ctx.mp_rs_sockets = NULL;
    
    (void)shmdt(s_event_core_private.mp_accept_lock);
    s_event_core_private.mp_accept_lock = NULL;
    g_rt_ctx.mp_accept_lock = NULL;

    return;
}


hixo_module_t g_event_core_module = {
    HIXO_MODULE_CORE,
    UNINITIALIZED,
    &event_core_init_master,
    &event_core_init_worker,
    NULL,
    NULL,
    &event_core_exit_worker,
    &event_core_exit_master,
    &s_event_core_ctx,
};
