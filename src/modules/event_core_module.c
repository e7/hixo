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
#include "event_module.h"
#include "app_module.h"


#define HIXO_SYS_SCHED_INTERVAL_MS      20
#define HIXO_ITIMER_RESOLUTION_S        0
#define HIXO_ITIMER_RESOLUTION_MS       (100 * 1000)


static int event_loop(void);
static void hixo_handle_accept(hixo_socket_t *p_sock);
static void hixo_handle_close(hixo_socket_t *p_sock);

static struct {
    int m_ev_timer_ms;
    int m_shmid;
    hixo_event_module_ctx_t *mp_ev_ctx;
    dlist_t m_app_ctx_list;
} s_event_core_private = {
    -1,
    -1,
    NULL,
    INIT_DLIST(s_event_core_private, m_app_ctx_list),
};

static hixo_core_module_ctx_t s_event_core_ctx = {
    &event_loop,
    &s_event_core_private,
};

int event_loop(void)
{
    int rslt;
    hixo_event_module_ctx_t *p_ev_ctx = s_event_core_private.mp_ev_ctx;

    while (TRUE) {
        list_t *p_iter, *p_next_iter;

        rslt = (*p_ev_ctx->mpf_process_events)(
                   s_event_core_private.m_ev_timer_ms
        );
        if (-1 == rslt) {
            break;
        }

        p_iter = g_rt_ctx.mp_posted_events;
        while (NULL != p_iter) {
            p_next_iter = *(list_t **)p_iter;
            hixo_socket_t *p_sock = CONTAINER_OF(p_iter,
                                                 hixo_socket_t,
                                                 m_posted_node);

            if ((p_sock->m_readable) && (p_sock->mpf_read_handler)) {
                (*p_sock->mpf_read_handler)(p_sock);
            }
            if (p_sock->m_close) {
                if (p_sock->mpf_disconnect_handler) {
                    (*p_sock->mpf_disconnect_handler)(p_sock);
                }
                hixo_handle_close(p_sock);
            }
            if ((p_sock->m_writable) && (p_sock->mpf_write_handler)) {
                (*p_sock->mpf_write_handler)(p_sock);
            }
            assert(rm_node(&g_rt_ctx.mp_posted_events,
                           &p_sock->m_posted_node));

            p_iter = p_next_iter;
        }
    }

    return rslt;
}

void hixo_handle_accept(hixo_socket_t *p_sock)
{
    int fd = 0;
    int tmp_err = 0;
    struct sockaddr_in client_addr;
    socklen_t len = 0;
    hixo_event_module_ctx_t *p_ev_ctx = s_event_core_private.mp_ev_ctx;
    hixo_app_module_ctx_t *p_app_ctx = NULL;

    assert(NULL != p_ev_ctx);
    while (TRUE) {
        hixo_socket_t *p_cmnct = NULL;

        p_cmnct = alloc_resource(&g_rt_ctx.m_sockets_cache);
        if (NULL == p_cmnct) {
            (void)fprintf(stderr, "[WARNING] no more power\n");
            break;
        }

        errno = 0;
        fd = accept(p_sock->m_fd, (struct sockaddr *)&client_addr, &len);
        tmp_err = errno;
        if (tmp_err) {
            free_resource(&g_rt_ctx.m_sockets_cache, p_cmnct);
        }
        if ((EAGAIN == tmp_err) || (EWOULDBLOCK == tmp_err)) {
            break;
        } else if (ECONNABORTED == tmp_err) {
            continue;
        } else if (tmp_err) {
            (void)fprintf(stderr, "[ERROR] accept() failed: %d\n", tmp_err);
            break;
        } else {
            do_nothing();
        }

        assert(fd >= 0);
        dlist_for_each_f (p_pos_node, &s_event_core_private.m_app_ctx_list) {
            hixo_app_module_ctx_t *p_iter;
            int found = FALSE;

            p_iter = CONTAINER_OF(p_pos_node, hixo_app_module_ctx_t, m_node);
            for (int i = 0; i < p_iter->m_nservers; ++i) {
                struct sockaddr_in sockname;
                socklen_t sn_len = sizeof(sockname);

                assert(0 == getsockname(fd,
                                        (struct sockaddr *)&sockname,
                                        &sn_len));
                if (sockname.sin_port != p_iter->mpa_servers[i].m_port) {
                    continue;
                }
                found = TRUE;
                break;
            }

            if (found) {
                p_app_ctx = p_iter;
                break;
            }
        }
        assert(NULL != p_app_ctx);
        if (HIXO_ERROR == hixo_create_socket(p_cmnct,
                                             fd,
                                             HIXO_CMNCT_SOCKET,
                                             p_app_ctx->mpf_handle_read,
                                             p_app_ctx->mpf_handle_write,
                                             p_app_ctx->mpf_handle_disconnect))
        {
            free_resource(&g_rt_ctx.m_sockets_cache, p_cmnct);
            continue;
        }

        hixo_socket_unblock(p_cmnct);
        hixo_socket_nodelay(p_cmnct);

        if (HIXO_ERROR == (*p_ev_ctx->mpf_add_event)(p_cmnct)) {
            hixo_destroy_socket(p_cmnct);
            free_resource(&g_rt_ctx.m_sockets_cache, p_cmnct);
            continue;
        }

        add_node(&g_rt_ctx.mp_connections, &p_cmnct->m_node);
        --gp_ps_info->m_power;
        p_app_ctx->mpf_handle_connect(p_cmnct);
    }

    return;
}

void hixo_handle_close(hixo_socket_t *p_sock)
{
    hixo_event_module_ctx_t *p_ev_ctx = s_event_core_private.mp_ev_ctx;

    p_sock->m_readable = 0U;
    p_sock->m_writable = 0U;

    (*p_ev_ctx->mpf_del_event)(p_sock);
    hixo_destroy_socket(p_sock);
    assert(rm_node(&g_rt_ctx.mp_connections, &p_sock->m_node));
    free_resource(&g_rt_ctx.m_sockets_cache, p_sock);

    ++gp_ps_info->m_power;
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

    reuseaddr = 1;
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
    int nservers;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    errno = 0;
    s_event_core_private.m_shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    tmp_err = errno;
    if (tmp_err) {
        goto ERR_SHMGET;
    }

    // 统计应用服务数
    nservers = 0;
    for (int i = 0; NULL != gap_modules[i]; ++i) {
        hixo_app_module_ctx_t *p_app_ctx;

        if (HIXO_MODULE_APP != gap_modules[i]->m_type) {
            continue;
        }

        p_app_ctx = (hixo_app_module_ctx_t *)gap_modules[i]->mp_ctx;
        nservers += (p_app_ctx->m_nservers > 0) ? p_app_ctx->m_nservers : 0;
    }

    g_rt_ctx.mpp_servers = (hixo_socket_t **)calloc(nservers,
                                                    sizeof(hixo_socket_t *));
    g_rt_ctx.m_nservers = nservers;
    if (NULL == g_rt_ctx.mpp_servers) {
        goto ERR_LISTENERS_CACHE;
    }

    if (HIXO_ERROR == create_resource(&g_rt_ctx.m_sockets_cache,
                                      p_conf->m_max_connections + nservers,
                                      sizeof(hixo_socket_t),
                                      OFFSET_OF(hixo_socket_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create sockets cache failed\n");
        goto ERR_SOCKETS_CACHE;
    }

    valid_sockets = 0;
    for (int i = 0; NULL != gap_modules[i]; ++i) {
        hixo_app_module_ctx_t *p_app_ctx;

        if (HIXO_MODULE_APP != gap_modules[i]->m_type) {
            continue;
        }

        p_app_ctx = (hixo_app_module_ctx_t *)gap_modules[i]->mp_ctx;
        for (int j = 0; j < p_app_ctx->m_nservers; ++j) {
            int fd;
            int backlog;
            struct sockaddr_in srv_addr;
            hixo_socket_t *p_listener = NULL;
            hixo_listen_conf_t *p_server = &p_app_ctx->mpa_servers[j];

            backlog = (p_server->m_backlog > 0)
                          ? p_server->m_backlog
                          : SOMAXCONN;
            (void)memset(&srv_addr, 0, sizeof(srv_addr));
            srv_addr.sin_family = AF_INET;
            if (!inet_aton(p_server->mp_ip, &srv_addr.sin_addr)) {
                // 无效ip
                continue;
            }
            srv_addr.sin_port = htons(p_server->m_port);
            p_listener
                = (hixo_socket_t *)alloc_resource(&g_rt_ctx.m_sockets_cache);
            assert(NULL != p_listener);
            fd = event_core_create_listener((struct sockaddr *)&srv_addr,
                                            backlog);
            if (INVALID_FD == fd) {
                goto ERR_CREATE_LISTENER__;
            }

            (void)hixo_create_socket(p_listener,
                                     fd,
                                     HIXO_LISTEN_SOCKET,
                                     &hixo_handle_accept,
                                     NULL,
                                     NULL);

            g_rt_ctx.mpp_servers[valid_sockets] = p_listener;
            add_node(&g_rt_ctx.mp_connections, &p_listener->m_node);

            ++valid_sockets;
            continue;

        ERR_CREATE_LISTENER__:
            free_resource(&g_rt_ctx.m_sockets_cache, p_listener);
        }

    }
    if (0 == valid_sockets) {
        goto ERR_CREATE_LISTENERS;
    }

    g_rt_ctx.m_nservers = valid_sockets;

    do {
        fprintf(stderr, "[INFO] count of listeners: %d\n", valid_sockets);
        rslt = HIXO_OK;
        break;

ERR_CREATE_LISTENERS:
        destroy_resource(&g_rt_ctx.m_sockets_cache);
ERR_SOCKETS_CACHE:
        free(g_rt_ctx.mpp_servers);
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
    for (int i = 0; i < g_rt_ctx.m_nservers; ++i) {
        hixo_socket_t * p_listener = g_rt_ctx.mpp_servers[i];

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
    int rslt;
    int tmp_err;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;
    struct itimerval timer = {
        {HIXO_ITIMER_RESOLUTION_S, HIXO_ITIMER_RESOLUTION_MS},
        {HIXO_ITIMER_RESOLUTION_S, HIXO_ITIMER_RESOLUTION_MS},
    };

    // 系统定时器
    if (p_conf->m_timer_resolution > HIXO_SYS_SCHED_INTERVAL_MS) {
        s_event_core_private.m_ev_timer_ms = p_conf->m_timer_resolution;
    } else {
        s_event_core_private.m_ev_timer_ms = -1;
        (void)setitimer(ITIMER_REAL, &timer, NULL);
    }

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

    // 查找事件模块
    for (int i = 0; NULL != gap_modules[i]; ++i) {
        hixo_module_t *p_mod = gap_modules[i];

        if (HIXO_MODULE_EVENT != p_mod->m_type) {
            continue;
        }

        s_event_core_private.mp_ev_ctx = (hixo_event_module_ctx_t *)(
                p_mod->mp_ctx
        );
        if (NULL != s_event_core_private.mp_ev_ctx) {
            break;
        }
    }
    if (NULL == s_event_core_private.mp_ev_ctx) {
        fprintf(stderr, "[ERROR] event module not found\n");
        goto ERR_NECESSARY_MODULE_NOT_FOUND;
    }

    // 查找应用模块
    for (int i = 0; NULL != gap_modules[i]; ++i) {
        hixo_module_t *p_mod = gap_modules[i];
        hixo_app_module_ctx_t *p_app_ctx;

        if (HIXO_MODULE_APP != p_mod->m_type) {
            continue;
        }

        p_app_ctx = (hixo_app_module_ctx_t *)p_mod->mp_ctx;
        dlist_add_head(&s_event_core_private.m_app_ctx_list,
                       &p_app_ctx->m_node);
    }
    if (dlist_empty(&s_event_core_private.m_app_ctx_list)) {
        fprintf(stderr, "[ERROR] app module not found\n");
        goto ERR_NECESSARY_MODULE_NOT_FOUND;
    }

    do {
        rslt = HIXO_OK;
        break;

ERR_NECESSARY_MODULE_NOT_FOUND:
        (void)shmdt((void const *)g_rt_ctx.mp_accept_lock);
        g_rt_ctx.mp_accept_lock = NULL;

ERR_SHMAT:
        rslt = HIXO_ERROR;
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
    &event_core_init_master,
    &event_core_init_worker,
    NULL,
    NULL,
    &event_core_exit_worker,
    &event_core_exit_master,

    HIXO_MODULE_CORE,
    &s_event_core_ctx,
};
