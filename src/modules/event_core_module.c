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
} s_event_core_private = {
    -1,
};

static hixo_core_module_ctx_t s_event_core_ctx = {
    &s_event_core_private,
};

static void hixo_handle_accept(hixo_socket_t *p_sock)
{
    int fd = 0;
    int tmp_err = 0;
    struct sockaddr client_addr;
    socklen_t len = 0;

    while (TRUE) {
        errno = 0;
        fd = accept(p_sock->m_fd, &client_addr, &len);
        tmp_err = errno;

        if ((EAGAIN == tmp_err) || (EWOULDBLOCK == tmp_err)) {
            break;
        }

        if (ECONNABORTED == tmp_err) {
            continue;
        }
    }

    return;
}

static int event_core_init_master(void)
{
    int rslt = HIXO_ERROR;
    int valid_sockets = 0;
    int tmp_err = 0;
    hixo_socket_t *p_listener = NULL;
    hixo_event_t *p_event = NULL;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;
    struct sockaddr_in srv_addr;

    errno = 0;
    s_event_core_private.m_shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    tmp_err = errno;
    if (-1 == s_event_core_private.m_shmid) {
        goto ERR_SHMGET;
    }

    if (HIXO_ERROR == create_resource(&g_rt_ctx.m_sockets_cache,
                                      p_conf->m_max_connections,
                                      sizeof(hixo_socket_t),
                                      OFFSET_OF(hixo_socket_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create sockets cache failed\n");
        goto ERR_SOCKETS_CACHE;
    }
    if (HIXO_ERROR == create_resource(&g_rt_ctx.m_events_cache,
                                      p_conf->m_max_events,
                                      sizeof(hixo_event_t),
                                      OFFSET_OF(hixo_event_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create events cache failed\n");
        goto ERR_EVENTS_CACHE;
    }

    for (int i = 0; i < p_conf->m_nservers; ++i) {
        p_event = (hixo_event_t *)alloc_resource(&g_rt_ctx.m_events_cache);
        assert(NULL != p_event);
        p_event->mpf_read_handler = &hixo_handle_accept;

        p_listener
            = (hixo_socket_t *)alloc_resource(&g_rt_ctx.m_sockets_cache);
        assert(NULL != p_listener);

        // 创建监听套接字
        errno = 0;
        p_listener->m_fd = socket(PF_INET, SOCK_STREAM, 0);
        tmp_err = errno;
        if (tmp_err) {
            free_resource(&g_rt_ctx.m_events_cache, p_event);
            free_resource(&g_rt_ctx.m_sockets_cache, p_listener);

            (void)fprintf(stderr, "[ERROR] socket() failed: %d\n", tmp_err);
            break;
        } else {
            ++valid_sockets;
        }

        // 设置非阻塞
        errno = 0;
        (void)unblocking_fd(p_listener->m_fd);
        tmp_err = errno;
        if (tmp_err) {
            (void)close(p_listener->m_fd);
            p_listener->m_fd = -1;
            --valid_sockets;

            free_resource(&g_rt_ctx.m_events_cache, p_event);
            free_resource(&g_rt_ctx.m_sockets_cache, p_listener);

            (void)fprintf(stderr, "[ERROR] fcntl() failed: %d\n", tmp_err);
            continue;
        }

        // 绑定端口
        (void)memset(&srv_addr, 0, sizeof(srv_addr));
        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = htonl(p_conf->mppc_srv_addrs[i]->m_ip);
        srv_addr.sin_port = htons(p_conf->mppc_srv_addrs[i]->m_port);

        errno = 0;
        (void)bind(p_listener->m_fd,
                   (struct sockaddr *)&srv_addr,
                   sizeof(srv_addr));
        tmp_err = errno;
        if (tmp_err) {
            (void)close(p_listener->m_fd);
            p_listener->m_fd = -1;
            --valid_sockets;

            free_resource(&g_rt_ctx.m_events_cache, p_event);
            free_resource(&g_rt_ctx.m_sockets_cache, p_listener);

            (void)fprintf(stderr, "[ERROR] bind() failed: %d\n", tmp_err);
            continue;
        }

        // 监听
        errno = 0;
        (void)listen(p_listener->m_fd,
                     (p_conf->mppc_srv_addrs[i]->m_backlog > 0)
                         ? p_conf->mppc_srv_addrs[i]->m_backlog
                         : SOMAXCONN);
        tmp_err = errno;
        if (tmp_err) {
            (void)close(p_listener->m_fd);
            p_listener->m_fd = -1;
            --valid_sockets;

            free_resource(&g_rt_ctx.m_events_cache, p_event);
            free_resource(&g_rt_ctx.m_sockets_cache, p_listener);

            (void)fprintf(stderr, "[ERROR] listen() failed: %d\n", tmp_err);
            continue;
        }

        add_node(&g_rt_ctx.mp_listeners, &p_listener->m_node);
        add_node(&g_rt_ctx.mp_listeners_evs, &p_event->m_node);

        p_event->mp_data = p_listener;
    }

    if (0 == valid_sockets) {
        goto ERR_CREATE_LISTENERS;
    }

    do {
        fprintf(stderr, "[INFO] count of listeners: %d\n", valid_sockets);
        rslt = HIXO_OK;
        break;

ERR_CREATE_LISTENERS:
        destroy_resource(&g_rt_ctx.m_events_cache);

ERR_EVENTS_CACHE:
        destroy_resource(&g_rt_ctx.m_sockets_cache);

ERR_SOCKETS_CACHE:
        (void)shmctl(s_event_core_private.m_shmid, IPC_RMID, 0);

ERR_SHMGET:
        break;
    } while (0);

    return rslt;
}

static void event_core_exit_master(void)
{
    for (list_t *p_iter = g_rt_ctx.mp_listeners_evs;
         NULL != p_iter;
         p_iter = *(list_t **)p_iter)
    {
        hixo_event_t *p_event = CONTAINER_OF(p_iter,
                                             hixo_event_t,
                                             m_node);
        hixo_socket_t *p_listener = (hixo_socket_t *)p_event->mp_data;

        (void)close(p_listener->m_fd);
        rm_node(&g_rt_ctx.mp_listeners_evs, &p_event->m_node);
        rm_node(&g_rt_ctx.mp_listeners, &p_listener->m_node);
        free_resource(&g_rt_ctx.m_events_cache, p_event);
        free_resource(&g_rt_ctx.m_sockets_cache, p_listener);
    }

    destroy_resource(&g_rt_ctx.m_sockets_cache);
    destroy_resource(&g_rt_ctx.m_events_cache);

    (void)shmctl(s_event_core_private.m_shmid, IPC_RMID, 0);
    s_event_core_private.m_shmid = -1;

    return;
}

static int event_core_init_worker(void)
{
    int rslt = HIXO_ERROR;
    int tmp_err = 0;

    // 映射共享内存
    assert(-1 != s_event_core_private.m_shmid);
    errno = 0;
    g_rt_ctx.mp_accept_lock
        = (atomic_t *)shmat(s_event_core_private.m_shmid, 0, 0);
    tmp_err = errno;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] shmat() failed: %d\n", tmp_err);
        goto ERR_SHMAT;
    }

    do {
        rslt = HIXO_OK;
        break;

ERR_SHMAT:
        break;
    } while (0);

    return rslt;
}

static void event_core_exit_worker(void)
{
    (void)shmdt((void const *)g_rt_ctx.mp_accept_lock);
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
