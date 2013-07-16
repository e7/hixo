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
hixo_module_t *gap_modules[] = {
    &g_main_core_module,
    &g_event_core_module,
    &g_epoll_module,
    NULL,
};


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

    for (int i = 0; NULL != gap_modules[i]; ++i) {
        if (HIXO_MODULE_EVENT != gap_modules[i]->m_type) {
            continue;
        }

        p_ev_ctx = (hixo_event_module_ctx_t *)gap_modules[i]->mp_ctx;
    }

    if (NULL == p_ev_ctx) {
        goto ERR_NO_EVENT_MODULE;
    }

    while (TRUE) {
        rslt = (*p_ev_ctx->mpf_process_events)(p_conf->m_timer_resolution);

        if (-1 == rslt) {
            break;
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



int init_worker(hixo_module_type_t mod_type)
{
    int rslt = HIXO_OK;

    for (int i = 0; NULL != gap_modules[i]; ++i) {
        if (mod_type != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == gap_modules[i]->mpf_init_worker) {
            continue;
        }

        if (HIXO_ERROR == (*gap_modules[i]->mpf_init_worker)()) {
            rslt = HIXO_ERROR;
            break;
        }
    }

    return rslt;
}

void exit_worker(hixo_module_type_t mod_type)
{
    for (int i = 0; NULL != gap_modules[i]; ++i) {
        if (mod_type != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == gap_modules[i]->mpf_exit_worker) {
            continue;
        }

        (*gap_modules[i]->mpf_exit_worker)();
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

    rslt = init_worker(HIXO_MODULE_CORE);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_CORE;
    }
    rslt = init_worker(HIXO_MODULE_EVENT);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_EVENT;
    }
    rslt = init_worker(HIXO_MODULE_APP);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_APP;
    }

    rslt = event_loop();

    do {
        exit_worker(HIXO_MODULE_APP);

ERR_INIT_WORKER_APP:
        exit_worker(HIXO_MODULE_EVENT);

ERR_INIT_WORKER_EVENT:
        exit_worker(HIXO_MODULE_CORE);

ERR_INIT_WORKER_CORE:
        break;
    } while (0);

    return rslt;
}

int init_master(hixo_module_type_t mod_type)
{
    int rslt = HIXO_OK;

    for (int i = 0; NULL != gap_modules[i]; ++i) {
        if (mod_type != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == gap_modules[i]->mpf_init_master) {
            continue;
        }

        if (HIXO_ERROR == (*gap_modules[i]->mpf_init_master)()) {
            rslt = HIXO_ERROR;

            break;
        }
    }

    return rslt;
}

void exit_master(hixo_module_type_t mod_type)
{
    for (int i = 0; NULL != gap_modules[i]; ++i) {
        if (mod_type != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == gap_modules[i]->mpf_exit_master) {
            continue;
        }
        (*gap_modules[i]->mpf_exit_master)();
    }
}

int hixo_main(void)
{
    int rslt;
    hixo_conf_t *p_conf;

    rslt = init_master(HIXO_MODULE_CORE);
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
    exit_master(HIXO_MODULE_CORE);
    return rslt;
}

int main(int argc, char *argv[])
{
    if (HIXO_ERROR == hixo_get_sysconf()) {
        return EXIT_FAILURE;
    }

    return (HIXO_OK == hixo_main()) ? EXIT_SUCCESS : EXIT_FAILURE;
}
