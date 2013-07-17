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
    int m_epfd;
    struct epoll_event *mp_epevs;
    int m_hold_lock;
} s_epoll_private = {
    -1,
    NULL,
    FALSE,
};


static int epoll_init(void);
static void epoll_add_event(hixo_socket_t *p_ev);
static int epoll_mod_event(void);
static void epoll_del_event(hixo_socket_t *p_ev);
static int epoll_process_events(int timer);
static void epoll_exit(void);

static hixo_event_module_ctx_t s_epoll_module_ctx = {
    NULL,
    &epoll_add_event,
    &epoll_mod_event,
    &epoll_del_event,
    &epoll_process_events,
    NULL,
    FALSE,
    &s_epoll_private,
};

hixo_module_t g_epoll_module = {
    HIXO_MODULE_EVENT,
    NULL,
    &epoll_init,
    NULL,
    NULL,
    &epoll_exit,
    NULL,
    &s_epoll_module_ctx,
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
        s_epoll_private.m_epfd = -1;

        goto ERR_EPOLL_CREATE;
    }
    s_epoll_private.m_epfd = ret;

    s_epoll_private.mp_epevs
        = (struct epoll_event *)calloc(p_conf->m_max_connections,
                                       sizeof(struct epoll_event));
    if (NULL == s_epoll_private.mp_epevs) {
        goto ERR_EPOLL_EVENTS;
    }

    do {
        rslt = HIXO_OK;
        break;

ERR_EPOLL_EVENTS:
        (void)close(s_epoll_private.m_epfd);

ERR_EPOLL_CREATE:
        break;
    } while (0);

    return rslt;
}

void epoll_add_event(hixo_socket_t *p_sock)
{
    int tmp_err = 0;
    struct epoll_event epev;

    epev.events = p_sock->m_event_types;
    epev.data.ptr = (void *)(((uintptr_t)p_sock) | (!!p_sock->m_stale));
    errno = 0;
    (void)epoll_ctl(s_epoll_private.m_epfd,
                    EPOLL_CTL_ADD,
                    p_sock->m_fd,
                    &epev);
    tmp_err = errno;
    if ((tmp_err) && (EEXIST != tmp_err)) {
        (void)fprintf(stderr, "[ERROR] epoll_ctl failed: %d\n", tmp_err);
    }

    return;
}

int epoll_mod_event(void)
{
    return HIXO_OK;
}

void epoll_del_event(hixo_socket_t *p_sock)
{
    int tmp_err = 0;
    struct epoll_event epev;

    errno = 0;
    (void)epoll_ctl(s_epoll_private.m_epfd,
                    EPOLL_CTL_DEL,
                    p_sock->m_fd,
                    &epev);
    tmp_err = errno;
    tmp_err = errno;
    if ((tmp_err) && (ENOENT != tmp_err)) {
        (void)fprintf(stderr, "[ERROR] epoll_ctl failed: %d\n", tmp_err);
    }

    return;
}

int epoll_process_events(int timer)
{
    int rslt = HIXO_OK;
    int nevents = 0;
    int tmp_err = 0;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    if ((g_ps_status.m_power > 0) && spinlock_try(g_rt_ctx.mp_accept_lock)) {
        s_epoll_private.m_hold_lock = TRUE;
    } else {
        s_epoll_private.m_hold_lock = FALSE;
    }

    for (int i = 0; i < p_conf->m_nservers; ++i) {
        g_rt_ctx.mpp_listeners[i]->m_active
            = s_epoll_private.m_hold_lock ? 1U : 0U;
    }

    if (s_epoll_private.m_hold_lock) {
        for (list_t *p_iter = g_rt_ctx.mp_connections;
             NULL != p_iter;
             p_iter = *(list_t **)p_iter)
        {
            hixo_socket_t *p_sock
                = CONTAINER_OF(p_iter, hixo_socket_t, m_node);

            if (p_sock->m_active) {
                epoll_add_event(p_sock);
            }
        }
    } else {
        for (list_t *p_iter = g_rt_ctx.mp_connections;
             NULL != p_iter;
             p_iter = *(list_t **)p_iter)
        {
            hixo_socket_t *p_sock
                = CONTAINER_OF(p_iter, hixo_socket_t, m_node);

            if (p_sock->m_active) {
                continue;
            }
            epoll_del_event(p_sock);
        }
    }

    errno = 0;
    nevents = epoll_wait(s_epoll_private.m_epfd,
                         s_epoll_private.mp_epevs,
                         p_conf->m_max_connections,
                         timer);
    tmp_err = (-1 == nevents) ? errno : 0;
    if (tmp_err) {
        if (EINTR == tmp_err) {
            goto EXIT;
        } else {
            rslt = HIXO_ERROR;
            fprintf(stderr, "[ERROR] epoll_wait failed: %d\n", tmp_err);
            goto EXIT;
        }
    }

    if (0 == nevents) { // timeout
        goto EXIT;
    }

    // 处理事件
    g_rt_ctx.mp_ctx = &s_epoll_module_ctx;
    for (int i = 0; i < nevents; ++i) {
        struct epoll_event *p_epev = &s_epoll_private.mp_epevs[i];
        uintptr_t stale = ((uintptr_t)p_epev->data.ptr) & 1;
        hixo_socket_t *p_sock
            = (hixo_socket_t *)((uintptr_t)p_epev->data.ptr & (~1));

        assert(p_sock->m_active);
        if ((-1 == p_sock->m_fd) || (stale != p_sock->m_stale)) {
            continue;
        }

        if ((HIXO_EVENT_ERR | HIXO_EVENT_HUP) & p_epev->events) {
            p_epev->events |= HIXO_EVENT_IN | HIXO_EVENT_OUT;
        }

        if (HIXO_EVENT_IN & p_epev->events) {
            assert(NULL != p_sock->mpf_read_handler);
            (*p_sock->mpf_read_handler)(p_sock);
        }

        if (HIXO_EVENT_OUT & p_epev->events) {
            assert(NULL != p_sock->mpf_write_handler);
            (*p_sock->mpf_write_handler)(p_sock);
        }
    }

EXIT:
    if (s_epoll_private.m_hold_lock) { // 只有拿到锁的进程才能解锁
        (void)spinlock_unlock(g_rt_ctx.mp_accept_lock);
        s_epoll_private.m_hold_lock = FALSE;
    }

    return rslt;
}

void epoll_exit(void)
{
    (void)close(s_epoll_private.m_epfd);

    if (NULL != s_epoll_module_ctx.mp_private) {
        free(s_epoll_module_ctx.mp_private);
        s_epoll_module_ctx.mp_private = NULL;
    }

    return;
}
