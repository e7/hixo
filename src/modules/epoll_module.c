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
#include "event.h"

static int epoll_init(void);
static void epoll_add_event(hixo_event_t *p_ev);
static int epoll_mod_event(void);
static int epoll_del_event(void);
static int epoll_process_events(void);
static void epoll_uninit(void);

static hixo_event_module_ctx_t s_epoll_module_ctx = {
    &epoll_init,
    &epoll_add_event,
    &epoll_mod_event,
    &epoll_del_event,
    &epoll_process_events,
    &epoll_uninit,
    -1,
    NULL,
    NULL,
    NULL,
};

hixo_module_t g_epoll_module = {
    HIXO_EVENT,
    &s_epoll_module_ctx,
};

int epoll_init(void)
{
    int rslt = 0;
    int tmp_err = 0;

    #define epev_size sizeof(struct epoll_event)
    s_epoll_module_ctx.mp_misc = calloc(MAX_CONNECTIONS, epev_size);
    #undef epev_size
    if (NULL == s_epoll_module_ctx.mp_misc) {
        fprintf(stderr, "[ERROR] out of memory\n");

        return HIXO_ERROR;
    }

    errno = 0;
    rslt = epoll_create(MAX_CONNECTIONS);
    tmp_err = (-1 == rslt) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] epoll_create failed: %d\n", tmp_err);

        return HIXO_ERROR;
    }
    s_epoll_module_ctx.m_fd = rslt;

    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
extern hixo_socket_t ga_hixo_listenings[];
        epoll_add_event(&ga_hixo_listenings[i].m_event);
    }

    return HIXO_OK;
}

void epoll_add_event(hixo_event_t *p_ev)
{
    struct epoll_event epev;

    epev.events = p_ev->m_ev_flags;
    epev.data.ptr = p_ev;
    (void)epoll_ctl(s_epoll_module_ctx.m_fd,
                    EPOLL_CTL_ADD,
                    p_ev->m_fd,
                    &epev);
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

    p_epevs = (struct epoll_event *)s_epoll_module_ctx.mp_misc;

    errno = 0;
    nevents = epoll_wait(s_epoll_module_ctx.m_fd,
                         p_epevs,
                         MAX_CONNECTIONS,
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
        hixo_event_t *p_ev = (hixo_event_t *)p_epevs[i].data.ptr;

        if (p_ev->m_overdue) { // 过期事件
            continue;
        }

        if (p_epevs[i].events & (EPOLLERR | EPOLLHUP)) {
        }

        if ((p_epevs[i].events & EPOLLIN) && (!p_ev->m_overdue)) {
        }

        if ((p_epevs[i].events & EPOLLOUT) && (!p_ev->m_overdue)) {
        }
    }

    return HIXO_OK;
}

void epoll_uninit(void)
{
    (void)close(s_epoll_module_ctx.m_fd);

    if (NULL != s_epoll_module_ctx.mp_misc) {
        free(s_epoll_module_ctx.mp_misc);
        s_epoll_module_ctx.mp_misc = NULL;
    }

    return;
}
