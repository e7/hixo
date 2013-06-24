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
    HIXO_MODULE_EVENT,
    &s_epoll_module_ctx,
};

int epoll_init(void)
{
    int rslt = 0;
    int tmp_err = 0;

    #define epev_size sizeof(struct epoll_event)
    s_epoll_module_ctx.mp_misc = calloc(g_conf.m_max_events, epev_size);
    #undef epev_size
    if (NULL == s_epoll_module_ctx.mp_misc) {
        fprintf(stderr, "[ERROR] out of memory\n");

        return HIXO_ERROR;
    }

    errno = 0;
    rslt = epoll_create(g_conf.m_max_connections);
    tmp_err = (-1 == rslt) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] epoll_create failed: %d\n", tmp_err);

        return HIXO_ERROR;
    }
    s_epoll_module_ctx.m_fd = rslt;

    for (int i = 0; i < g_conf.m_nservers; ++i) {
    }

    return HIXO_OK;
}

void epoll_add_event(hixo_event_t *p_ev)
{
    struct epoll_event epev;

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
                         g_conf.m_max_events,
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

void epoll_uninit(void)
{
    (void)close(s_epoll_module_ctx.m_fd);

    if (NULL != s_epoll_module_ctx.mp_misc) {
        free(s_epoll_module_ctx.mp_misc);
        s_epoll_module_ctx.mp_misc = NULL;
    }

    return;
}
