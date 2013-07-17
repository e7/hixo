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

static void hixo_handle_read(hixo_socket_t *p_sock)
{
    fprintf(stderr, "[INFO] handle read\n");
}

static void hixo_handle_write(hixo_socket_t *p_sock)
{
    fprintf(stderr, "[INFO] handle write\n");
}

static void hixo_handle_accept(hixo_socket_t *p_sock)
{
    int fd = 0;
    int tmp_err = 0;
    struct sockaddr client_addr;
    socklen_t len = 0;
    hixo_event_module_ctx_t *p_ctx = g_rt_ctx.mp_ctx;

    assert(NULL != p_ctx);
    while (TRUE) {
        hixo_socket_t *p_cmnct = NULL;

        errno = 0;
        fd = accept(p_sock->m_fd, &client_addr, &len);
        tmp_err = errno;
        if ((EAGAIN == tmp_err) || (EWOULDBLOCK == tmp_err)) {
            break;
        }
        if (ECONNABORTED == tmp_err) {
            continue;
        }

        errno = 0;
        (void)unblocking_fd(fd);
        tmp_err = errno;
        if (tmp_err) {
            (void)close(fd);
            fprintf(stderr, "[ERROR] fcntl() failed: %d", tmp_err);
            continue;
        }

        p_cmnct = alloc_resource(&g_rt_ctx.m_sockets_cache);
        assert(NULL != p_cmnct);
        if (HIXO_ERROR == hixo_create_socket(p_cmnct,
                                             fd,
                                             HIXO_CMNCT_SOCKET,
                                             &hixo_handle_read,
                                             &hixo_handle_write))
        {
            free_resource(&g_rt_ctx.m_sockets_cache, p_cmnct);
            (void)close(fd);
            continue;
        }

        if (HIXO_ERROR == (*p_ctx->mpf_add_event)(p_cmnct)) {
            hixo_destroy_socket(p_cmnct);
            free_resource(&g_rt_ctx.m_sockets_cache, p_cmnct);
            (void)close(fd);
            continue;
        }

        add_node(&g_rt_ctx.mp_connections, &p_cmnct->m_node);

        --g_ps_status.m_power;
    }

    return;
}

static int event_core_create_listener(struct sockaddr *p_srv_addr,
                                      int backlog)
{
    int fd;
    int tmp_err;
    int reuseaddr;

    errno = 0;
    fd = socket(PF_INET, SOCK_STREAM, 0);
    tmp_err = errno;
    if (tmp_err) {
        goto ERR_SOCKET;
    }

    errno = 0;
    (void)unblocking_fd(fd);
    tmp_err = errno;
    if (tmp_err) {
        goto ERR_INIT;
    }

    errno = 0;
    (void)setsockopt(fd,
                     SOL_SOCKET,
                     SO_REUSEADDR,
                     &reuseaddr,
                     sizeof(reuseaddr));
    tmp_err = errno;
    if (tmp_err) {
        goto ERR_INIT;
    }

    errno = 0;
    (void)bind(fd, p_srv_addr, sizeof(*p_srv_addr));
    tmp_err = errno;
    if (tmp_err) {
        goto ERR_INIT;
    }

    errno = 0;
    (void)listen(fd, backlog);
    tmp_err = errno;
    if (tmp_err) {
        goto ERR_INIT;
    }

    do {
        break;
ERR_INIT:
        (void)close(fd);
ERR_SOCKET:
        fd = INVALID_FD;
        (void)fprintf(stderr, "[ERROR] create socket failed: %d\n", tmp_err);
        break;
    } while (0);

    return fd;
}

static void event_core_destroy_listener(int fd)
{
    (void)close(fd);
}

static int event_core_init_master(void)
{
    int rslt = HIXO_ERROR;
    int valid_sockets = 0;
    int tmp_err = 0;
    hixo_socket_t *p_listener = NULL;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    errno = 0;
    s_event_core_private.m_shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    tmp_err = errno;
    if (tmp_err) {
        goto ERR_SHMGET;
    }

    g_rt_ctx.mpp_listeners = (hixo_socket_t **)calloc(p_conf->m_nservers,
                                                      sizeof(hixo_socket_t *));
    if (NULL == g_rt_ctx.mpp_listeners) {
        goto ERR_LISTENERS_CACHE;
    }

    if (HIXO_ERROR == create_resource(&g_rt_ctx.m_sockets_cache,
                                      p_conf->m_max_connections,
                                      sizeof(hixo_socket_t),
                                      OFFSET_OF(hixo_socket_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create sockets cache failed\n");
        goto ERR_SOCKETS_CACHE;
    }

    valid_sockets = 0;
    for (int i = 0; i < p_conf->m_nservers; ++i) {
        int fd = 0;
        int backlog = (p_conf->mppc_srv_addrs[i]->m_backlog > 0)
                          ? p_conf->mppc_srv_addrs[i]->m_backlog
                          : SOMAXCONN;
        struct sockaddr_in srv_addr;

        p_listener
            = (hixo_socket_t *)alloc_resource(&g_rt_ctx.m_sockets_cache);
        assert(NULL != p_listener);

        (void)memset(&srv_addr, 0, sizeof(srv_addr));
        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = htonl(p_conf->mppc_srv_addrs[i]->m_ip);
        srv_addr.sin_port = htons(p_conf->mppc_srv_addrs[i]->m_port);
        fd = event_core_create_listener((struct sockaddr *)&srv_addr, backlog);
        if (INVALID_FD == fd) {
            free_resource(&g_rt_ctx.m_sockets_cache, p_listener);
            continue;
        }

        (void)hixo_create_socket(p_listener,
                                 fd,
                                 HIXO_LISTEN_SOCKET,
                                 &hixo_handle_accept,
                                 NULL);

        g_rt_ctx.mpp_listeners[i] = p_listener;
        add_node(&g_rt_ctx.mp_connections, &p_listener->m_node);

        ++valid_sockets;
    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_LISTENERS;
    }

    g_ps_status.m_power = p_conf->m_max_connections
                                 - (p_conf->m_max_connections / 8);

    do {
        fprintf(stderr, "[INFO] count of listeners: %d\n", valid_sockets);
        rslt = HIXO_OK;
        break;

ERR_CREATE_LISTENERS:
        destroy_resource(&g_rt_ctx.m_sockets_cache);
ERR_SOCKETS_CACHE:
        free(g_rt_ctx.mpp_listeners);
ERR_LISTENERS_CACHE:
        (void)shmctl(s_event_core_private.m_shmid, IPC_RMID, 0);
ERR_SHMGET:
        rslt = HIXO_ERROR;
        break;
    } while (0);

    return rslt;
}

static void event_core_exit_master(void)
{
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    for (int i = 0; i < p_conf->m_nservers; ++i) {
        hixo_socket_t * p_listener = g_rt_ctx.mpp_listeners[i];

        if (NULL == p_listener) {
            continue;
        }

        assert(rm_node(&g_rt_ctx.mp_connections, &p_listener->m_node));
        event_core_destroy_listener(p_listener->m_fd);
        free_resource(&g_rt_ctx.m_sockets_cache, p_listener);
    }

    destroy_resource(&g_rt_ctx.m_sockets_cache);

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
    &event_core_init_master,
    &event_core_init_worker,
    NULL,
    NULL,
    &event_core_exit_worker,
    &event_core_exit_master,
    &s_event_core_ctx,
};
