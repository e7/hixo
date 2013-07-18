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


hixo_sysconf_t  g_sysconf = {};
hixo_rt_context_t g_rt_ctx = {};

// 模块数组
typedef enum {
    HIXO_STAGE_MASTER = 0xe78f8a,
    HIXO_STAGE_WORKER,
    HIXO_STAGE_THREAD,
} hixo_stage_type_t;

hixo_module_t *gap_modules[] = {
    &g_main_core_module,
    &g_event_core_module,
    &g_epoll_module,
    NULL,
};
DECLARE_DLIST(g_core_module_loaded_list);
DECLARE_DLIST(g_event_module_loaded_list);
DECLARE_DLIST(g_app_module_loaded_list);


hixo_ps_status_t g_ps_status = {
    TRUE,
};

static int hixo_get_sysconf(void)
{
    int rslt = 0;
    int tmp_err = 0;
    struct rlimit rlmt;
    int *p_file_no = (int *)&g_sysconf.M_MAX_FILE_NO;
    int *p_page_size = (int *)&g_sysconf.M_PAGE_SIZE;

    errno = 0;
    rslt = getrlimit(RLIMIT_NOFILE, &rlmt);
    tmp_err = (-1 == rslt) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] getrlimit() failed: %d\n", tmp_err);

        return HIXO_ERROR;
    }
    *p_file_no = rlmt.rlim_cur;

    *p_page_size = PAGE_SIZE;

    return HIXO_OK;
}

static int event_loop(void)
{
    int rslt;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;
    hixo_event_module_ctx_t *p_ev_ctx = NULL;

    dlist_for_each_f(p_pos_node, &g_event_module_loaded_list) {
        hixo_module_t *p_mod = CONTAINER_OF(p_pos_node, hixo_module_t, m_node);

        p_ev_ctx = (hixo_event_module_ctx_t *)p_mod->mp_ctx;
    }

    if (NULL == p_ev_ctx) {
        goto ERR_NO_EVENT_MODULE;
    }

    g_rt_ctx.mp_ctx = p_ev_ctx;
    while (TRUE) {
        list_t *p_iter, *p_next_iter;

        rslt = (*p_ev_ctx->mpf_process_events)(p_conf->m_timer_resolution);
        if (-1 == rslt) {
            break;
        }

        p_iter = g_rt_ctx.mp_posted_events;
        while (NULL != p_iter) {
            p_next_iter = *(list_t **)p_iter;
            hixo_socket_t *p_sock = CONTAINER_OF(p_iter,
                                                 hixo_socket_t,
                                                 m_posted_node);

            if (p_sock->m_readable) {
                (*p_sock->mpf_read_handler)(p_sock);
            }
            if (p_sock->m_writable) {
                (*p_sock->mpf_write_handler)(p_sock);
            }
            assert(rm_node(&g_rt_ctx.mp_posted_events,
                           &p_sock->m_posted_node));

            p_iter = p_next_iter;
        }
    }

    do {
        break;

ERR_NO_EVENT_MODULE:
        rslt = HIXO_ERROR;
        break;
    } while (0);

    return rslt;
}


int hixo_init(hixo_stage_type_t stage, hixo_module_type_t mod_type)
{
    int rslt;
    dlist_t *p_list = NULL;

    switch (mod_type) {
    case HIXO_MODULE_CORE:
        p_list = &g_core_module_loaded_list;
        break;

    case HIXO_MODULE_EVENT:
        p_list = &g_event_module_loaded_list;
        break;

    case HIXO_MODULE_APP:
        p_list = &g_app_module_loaded_list;
        break;

    default:
        assert(0);
        break;
    }

    rslt = HIXO_OK;
    for (int i = 0; NULL != gap_modules[i]; ++i) {
        int (*pf_init)(void) = NULL;

        if (mod_type != gap_modules[i]->m_type) {
            continue;
        }

        if (HIXO_STAGE_MASTER == stage) {
            pf_init = gap_modules[i]->mpf_init_master;
        } else if (HIXO_STAGE_WORKER == stage) {
            pf_init = gap_modules[i]->mpf_init_worker;
        } else if (HIXO_STAGE_THREAD == stage) {
            pf_init = gap_modules[i]->mpf_init_thread;
        } else {
            assert(0);
        }

        if (NULL == pf_init) {
            continue;
        }

        if (HIXO_ERROR == (pf_init)()) {
            rslt = HIXO_ERROR;

            break;
        }

        dlist_add_head(p_list, &gap_modules[i]->m_node);
    }

    return rslt;
}

void hixo_exit(hixo_stage_type_t stage, hixo_module_type_t mod_type)
{
    dlist_t *p_list = NULL;

    switch (mod_type) {
    case HIXO_MODULE_CORE:
        p_list = &g_core_module_loaded_list;
        break;

    case HIXO_MODULE_EVENT:
        p_list = &g_event_module_loaded_list;
        break;

    case HIXO_MODULE_APP:
        p_list = &g_app_module_loaded_list;
        break;

    default:
        assert(0);
        break;
    }

    dlist_for_each_f_safe(p_pos_node, p_cur_next, p_list) {
        void (*pf_exit)(void) = NULL;
        hixo_module_t *p_mod = CONTAINER_OF(p_pos_node, hixo_module_t, m_node);

        if (HIXO_STAGE_MASTER == stage) {
            pf_exit = p_mod->mpf_exit_master;
        } else if (HIXO_STAGE_WORKER == stage) {
            pf_exit = p_mod->mpf_exit_worker;
        } else if (HIXO_STAGE_THREAD == stage) {
            pf_exit = p_mod->mpf_exit_thread;
        } else {
            assert(0);
        }

        if (NULL == pf_exit) {
            continue;
        }
        (pf_exit)();

        dlist_del(p_pos_node);
    }
}

static int master_main(void)
{
    sleep(-1);

    return HIXO_OK;
}

static int worker_main(void)
{
    int rslt;

    rslt = hixo_init(HIXO_STAGE_WORKER, HIXO_MODULE_CORE);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_CORE;
    }
    rslt = hixo_init(HIXO_STAGE_WORKER, HIXO_MODULE_EVENT);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_EVENT;
    }
    rslt = hixo_init(HIXO_STAGE_WORKER, HIXO_MODULE_APP);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_APP;
    }

    rslt = event_loop();

    do {
        hixo_exit(HIXO_STAGE_WORKER, HIXO_MODULE_APP);

ERR_INIT_WORKER_APP:
        hixo_exit(HIXO_STAGE_WORKER, HIXO_MODULE_EVENT);

ERR_INIT_WORKER_EVENT:
        hixo_exit(HIXO_STAGE_WORKER, HIXO_MODULE_CORE);

ERR_INIT_WORKER_CORE:
        break;
    } while (0);

    return rslt;
}

int hixo_main(void)
{
    int rslt;
    hixo_conf_t *p_conf;

    rslt = hixo_init(HIXO_STAGE_MASTER, HIXO_MODULE_CORE);
    if (HIXO_ERROR == rslt) {
        goto EXIT;
    }

    if (p_conf->m_daemon) {
    }

    // 分裂进程
    p_conf = g_rt_ctx.mp_conf;
    for (int i = 0; i < p_conf->m_worker_processes; ++i) {
        pid_t cpid = fork();
        if (-1 == cpid) {
            return -1;
        } else if (0 == cpid) {
            g_ps_status.m_master = FALSE;
            break;
        } else {
            g_ps_status.m_master = TRUE;
        }
    }

    if (g_ps_status.m_master) {
        rslt = master_main();
    } else {
        rslt = worker_main();
    }

EXIT:
    hixo_exit(HIXO_STAGE_MASTER, HIXO_MODULE_CORE);
    return rslt;
}

int main(int argc, char *argv[])
{
    int rslt;

    if (HIXO_ERROR == hixo_get_sysconf()) {
        rslt = HIXO_ERROR;
        goto ERR_SYSCONF;
    }

    assert(dlist_empty(&g_core_module_loaded_list));
    assert(dlist_empty(&g_event_module_loaded_list));
    assert(dlist_empty(&g_app_module_loaded_list));
    rslt = (HIXO_OK == hixo_main()) ? EXIT_SUCCESS : EXIT_FAILURE;
    assert(dlist_empty(&g_app_module_loaded_list));
    assert(dlist_empty(&g_event_module_loaded_list));
    assert(dlist_empty(&g_core_module_loaded_list));

ERR_SYSCONF:
    return rslt;
}
