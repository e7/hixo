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
static void epoll_add_event(hixo_event_t *p_ev,
                            uint32_t events,
                            uint32_t flags);
static int epoll_mod_event(void);
static void epoll_del_event(hixo_event_t *p_ev,
                            uint32_t events,
                            uint32_t flags);
static int epoll_process_events(void);
static void epoll_exit(void);

static hixo_event_module_ctx_t s_epoll_module_ctx = {
    &epoll_init,
    &epoll_add_event,
    &epoll_mod_event,
    &epoll_del_event,
    &epoll_process_events,
    &epoll_exit,
    FALSE,
    &s_epoll_private,
};

hixo_module_t g_epoll_module = {
    HIXO_MODULE_EVENT,
    UNINITIALIZED,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
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
        = (struct epoll_event *)calloc(p_conf->m_max_events,
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

void epoll_add_event(hixo_event_t *p_ev,
                     uint32_t events,
                     uint32_t flags)
{
    int tmp_err = 0;
    struct epoll_event epev;
    hixo_socket_t *p_sock = (hixo_socket_t *)p_ev->mp_data;

    if (p_ev->m_active) {
        return;
    }

    epev.events = events | flags;
    epev.data.ptr = p_ev;
    errno = 0;
    (void)epoll_ctl(s_epoll_private.m_epfd,
                    EPOLL_CTL_ADD,
                    p_sock->m_fd,
                    &epev);
    tmp_err = errno;
    if (tmp_err) {
        fprintf(stderr,
                "[WARNING][%d] add event %d failed: %d\n",
                getpid(),
                p_sock->m_fd,
                tmp_err);
    } else {
        p_ev->m_active = TRUE;
    }

    return;
}

int epoll_mod_event(void)
{
    return HIXO_OK;
}

void epoll_del_event(hixo_event_t *p_ev,
                     uint32_t events,
                     uint32_t flags)
{
    int tmp_err = 0;
    struct epoll_event epev;
    hixo_socket_t *p_sock = (hixo_socket_t *)p_ev->mp_data;

    if (!p_ev->m_active) {
        return;
    }

    epev.events = events | flags;
    epev.data.ptr = p_ev;
    errno = 0;
    (void)epoll_ctl(s_epoll_private.m_epfd,
                    EPOLL_CTL_DEL,
                    p_sock->m_fd,
                    &epev);
    tmp_err = errno;
    if (tmp_err) {
        fprintf(stderr,
                "[WARNING][%d] del event failed: %d\n",
                getpid(),
                tmp_err);
    } else {
        p_ev->m_active = FALSE;
    }

    return;
}

int epoll_process_events(void)
{
    int nevents = 0;
    int tmp_err = 0;
    int timer = 20;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    if (spinlock_try(g_rt_ctx.mp_accept_lock)) {
        s_epoll_private.m_hold_lock = TRUE;
    } else {
        s_epoll_private.m_hold_lock = FALSE;
    }

    if (s_epoll_private.m_hold_lock) {
        for (int i = 0; i < p_conf->m_nservers; ++i) {
            hixo_event_t *p_ev = &g_rt_ctx.mp_listeners_evs[i];

            epoll_add_event(p_ev, EPOLLIN, EPOLLET);
        }
    } else {
        for (int i = 0; i < p_conf->m_nservers; ++i) {
            hixo_event_t *p_ev = &g_rt_ctx.mp_listeners_evs[i];

            epoll_del_event(p_ev, EPOLLIN, EPOLLET);
        }
    }

    errno = 0;
    nevents = epoll_wait(s_epoll_private.m_epfd,
                         s_epoll_private.mp_epevs,
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

    if (s_epoll_private.m_hold_lock) {
        (void)spinlock_unlock(g_rt_ctx.mp_accept_lock);
        s_epoll_private.m_hold_lock = FALSE;
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
    (void)close(s_epoll_private.m_epfd);

    if (NULL != s_epoll_module_ctx.mp_private) {
        free(s_epoll_module_ctx.mp_private);
        s_epoll_module_ctx.mp_private = NULL;
    }

    return;
}
