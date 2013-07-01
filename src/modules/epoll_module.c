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


// epoll事件模块
#include "conf.h"
#include "event_module.h"


struct {
    hixo_resource_t m_sockets;
    hixo_resource_t m_events;
} s_epoll_private;


static int epoll_init(void);
static void epoll_add_event(hixo_event_t *p_ev,
                            uint32_t events,
                            uint32_t flags);
static int epoll_mod_event(void);
static int epoll_del_event(void);
static int epoll_process_events(void);
static void epoll_exit(void);

static hixo_event_module_ctx_t s_epoll_module_ctx = {
    &epoll_init,
    &epoll_add_event,
    &epoll_mod_event,
    &epoll_del_event,
    &epoll_process_events,
    &epoll_exit,
    -1,
    NULL,
};

hixo_module_t g_epoll_module = {
    HIXO_MODULE_EVENT,
    &s_epoll_module_ctx,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

int epoll_init(void)
{
    int rslt = HIXO_ERROR;
    int ret = 0;
    int tmp_err = 0;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    errno = 0;
    ret = epoll_create(p_conf->m_max_connections);
    tmp_err = (-1 == ret) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] epoll_create failed: %d\n", tmp_err);

        goto ERR_EPOLL_CREATE;
    }
    s_epoll_module_ctx.m_fd = ret;

    // 创建套接字和事件资源
    if (HIXO_ERROR == create_resource(&s_epoll_private.m_sockets,
                                      p_conf->m_max_connections,
                                      sizeof(hixo_socket_t),
                                      OFFSET_OF(hixo_socket_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create sockets failed\n");

        goto ERR_CREATE_SOCKETS;
    }
    if (HIXO_ERROR == create_resource(&s_epoll_private.m_events,
                                      p_conf->m_max_events,
                                      sizeof(hixo_event_t),
                                      OFFSET_OF(hixo_event_t, m_node)))
    {
        fprintf(stderr, "[ERROR] create sockets failed\n");

        goto ERR_CREATE_EVENTS;
    }
    s_epoll_module_ctx.mp_private = &s_epoll_private;
    g_rt_ctx.mp_sockets = &s_epoll_private.m_sockets;
    g_rt_ctx.mp_events = &s_epoll_private.m_events;

    // 添加监听器事件监视
    for (int i = 0; i < p_conf->m_nservers; ++i) {
        hixo_event_t *p_ev = NULL;
        hixo_socket_t *p_sock = NULL;

        p_ev = (hixo_event_t *)alloc_resource(&s_epoll_private.m_events);
        p_sock = &g_rt_ctx.mp_listeners[i];
        assert(NULL != p_ev);
        assert(NULL != p_sock);
        p_ev->mp_data = p_sock;

        epoll_add_event(p_ev, EPOLLIN, EPOLLET);
    }

    do {
        rslt = HIXO_OK;
        break;

ERR_CREATE_EVENTS:
        destroy_resource(&s_epoll_private.m_sockets);

ERR_CREATE_SOCKETS:
        (void)close(s_epoll_module_ctx.m_fd);
        s_epoll_module_ctx.m_fd = -1;

ERR_EPOLL_CREATE:
        break;
    } while (0);

    return rslt;
}

void epoll_add_event(hixo_event_t *p_ev,
                     uint32_t events,
                     uint32_t flags)
{
    int tmp_err = 0;
    struct epoll_event epev;
    hixo_socket_t *p_sock = (hixo_socket_t *)p_ev->mp_data;

    epev.events = events;
    epev.data.ptr = p_ev;
    errno = 0;
    (void)epoll_ctl(s_epoll_module_ctx.m_fd,
                    EPOLL_CTL_ADD,
                    p_sock->m_fd,
                    &epev);
    tmp_err = errno;
    if (tmp_err) {
        fprintf(stderr,
                "[WARNING][%d] add event failed: %d\n",
                getpid(),
                tmp_err);
    }

    return;
}

int epoll_mod_event(void)
{
    return HIXO_OK;
}

int epoll_del_event(void)
{
    return HIXO_OK;
}

int epoll_process_events(void)
{
    int nevents = 0;
    int tmp_err = 0;
    int timer = -1;
    struct epoll_event *p_epevs = NULL;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    p_epevs = (struct epoll_event *)s_epoll_module_ctx.mp_private;

    errno = 0;
    nevents = epoll_wait(s_epoll_module_ctx.m_fd,
                         p_epevs,
                         p_conf->m_max_events,
                         timer);
    tmp_err = (-1 == nevents) ? errno : 0;
    if (tmp_err) {
        if (EINTR == tmp_err) {
            return HIXO_OK;
        } else {
            fprintf(stderr, "[ERROR] epoll_wait failed: %d\n", tmp_err);

            return HIXO_ERROR;
        }
    }

    if (0 == nevents) { // timeout
        return HIXO_OK;
    }

    for (int i = 0; i < nevents; ++i) {
    }

    return HIXO_OK;
}

void epoll_exit(void)
{
    (void)close(s_epoll_module_ctx.m_fd);

    if (NULL != s_epoll_module_ctx.mp_private) {
        free(s_epoll_module_ctx.mp_private);
        s_epoll_module_ctx.mp_private = NULL;
    }

    return;
}
