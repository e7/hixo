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


struct {
    int const MAX_FILE_NO;
} g_sysconf = {};
hixo_rt_context_t g_rt_ctx = {};

// 模块数组
hixo_module_t *gap_modules[] = {
    &g_main_core_module,
    &g_event_core_module,
    &g_epoll_module,
};


bitmap_t g_lsn_sockets_bm = {NULL, 0};
int g_master = TRUE;

static int master_main(void)
{
    sleep(-1);

    return HIXO_OK;
}

static int worker_loop(void)
{
    int rslt = HIXO_ERROR;
    int fatal_err = FALSE;
    hixo_event_module_ctx_t *p_ev_ctx = NULL;

    for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
        p_ev_ctx = (hixo_event_module_ctx_t *)gap_modules[i]->mp_ctx;

        if (HIXO_MODULE_EVENT != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == p_ev_ctx->mpf_init) {
            continue;
        }

        if (p_ev_ctx->m_initialized) {
            continue;
        }

        if (HIXO_ERROR == (*p_ev_ctx->mpf_init)()) {
            fatal_err = TRUE;
            break;
        }
        p_ev_ctx->m_initialized = TRUE;
    }
    if (fatal_err) {
        goto ERR_INIT_CTX;
    }

    while (TRUE) {
        rslt = (*p_ev_ctx->mpf_process_events)();

        if (-1 == rslt) {
            break;
        }
    }

    do {
ERR_INIT_CTX:
        for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
            p_ev_ctx = (hixo_event_module_ctx_t *)gap_modules[i]->mp_ctx;

            if (HIXO_MODULE_EVENT != gap_modules[i]->m_type) {
                continue;
            }

            if (NULL == p_ev_ctx->mpf_exit) {
                continue;
            }

            if (!p_ev_ctx->m_initialized) {
                continue;
            }

            (*p_ev_ctx->mpf_exit)();
            p_ev_ctx->m_initialized = FALSE;
        }
    } while (0);

    return rslt;
}

static int worker_main(void)
{
    int rslt = HIXO_ERROR;
    int fatal_err = FALSE;

    for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
        if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == gap_modules[i]->mpf_init_worker) {
            continue;
        }

        if (MASTER_INITIALIZED != gap_modules[i]->m_status) {
            continue;
        }

        if (HIXO_ERROR == (*gap_modules[i]->mpf_init_worker)()) {
            fatal_err = TRUE;
            break;
        }
        gap_modules[i]->m_status = WORKER_INITIALIZED;
    }
    if (fatal_err) {
        goto ERR_INIT_WORKER;
    }

    rslt = worker_loop();

    do {
ERR_INIT_WORKER:
        for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
            if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
                continue;
            }

            if (NULL == gap_modules[i]->mpf_exit_worker) {
                continue;
            }

            if (WORKER_INITIALIZED != gap_modules[i]->m_status) {
                continue;
            }

            (*gap_modules[i]->mpf_exit_worker)();
            gap_modules[i]->m_status = MASTER_INITIALIZED;
        }
    } while (0);

    return rslt;
}

static int hixo_main(void)
{
    int rslt = 0;
    hixo_conf_t *p_conf = g_rt_ctx.mp_conf;

    if (p_conf->m_daemon) {
    }

    // 分裂进程
    for (int i = 0; i < p_conf->m_worker_processes; ++i) {
        pid_t cpid = fork();
        if (-1 == cpid) {
            return -1;
        } else if (0 == cpid) {
            g_master = FALSE;

            break;
        } else {
            g_master = TRUE;
        }
    }

    if (g_master) {
        rslt = master_main();
    } else {
        rslt = worker_main();
    }

    return rslt;
}

static int hixo_init_sysconf(void)
{
    int rslt = 0;
    int tmp_err = 0;
    struct rlimit rlmt;
    int *p_file_no = (int *)&g_sysconf.MAX_FILE_NO;

    errno = 0;
    rslt = getrlimit(RLIMIT_NOFILE, &rlmt);
    tmp_err = (-1 == rslt) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] getrlimit() failed: %d\n", tmp_err);

        return HIXO_ERROR;
    }
    *p_file_no = rlmt.rlim_cur;

    return HIXO_OK;
}

int main(int argc, char *argv[])
{
    int rslt = EXIT_FAILURE;
    int fatal_err = FALSE;

    if (HIXO_ERROR == hixo_init_sysconf()) {
        goto ERR_INIT_SYSCONF;
    }

    for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
        if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == gap_modules[i]->mpf_init_master) {
            continue;
        }

        if (UNINITIALIZED != gap_modules[i]->m_status) {
            continue;
        }

        if (HIXO_ERROR == (*gap_modules[i]->mpf_init_master)()) {
            fatal_err = TRUE;

            break;
        }
        gap_modules[i]->m_status = MASTER_INITIALIZED;
    }

    if (fatal_err) {
        goto ERR_INIT_MASTER;
    }

    rslt = (HIXO_OK == hixo_main()) ? EXIT_SUCCESS : EXIT_FAILURE;

    do {
ERR_INIT_MASTER:
        if (g_master) {
            for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
                if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
                    continue;
                }

                if (NULL == gap_modules[i]->mpf_exit_master) {
                    continue;
                }

                if (MASTER_INITIALIZED != gap_modules[i]->m_status) {
                    continue;
                }

                (*gap_modules[i]->mpf_exit_master)();
                gap_modules[i]->m_status = UNINITIALIZED;
            }
        }

ERR_INIT_SYSCONF:
        break;
    } while (0);

    return rslt;
}
