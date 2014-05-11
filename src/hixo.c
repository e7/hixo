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
#include "app_module.h"


#define HIXO_MAX_PSS            1024


hixo_sysconf_t  g_sysconf = {};
hixo_rt_context_t g_rt_ctx = {};

// 模块数组
typedef enum {
    HIXO_PHASE_MASTER = 0xe78f8a,
    HIXO_PHASE_WORKER,
    HIXO_PHASE_THREAD,
} hixo_stage_type_t;

hixo_module_t *gap_modules[] = {
    &g_main_core_module,
    &g_event_core_module,
    &g_epoll_module,
    &g_echo_module,
    &g_simple_http_module,
    NULL,
};


hixo_ps_info_t ga_pss_info[HIXO_MAX_PSS];
hixo_ps_info_t *gp_ps_info = NULL;
hixo_status_t g_status = HIXO_STATUS_RUNNING;
int g_child_count = 0;

static int hixo_get_sysconf(void)
{
    int rslt = 0;
    int tmp_err = 0;
    struct rlimit rlmt;
    int *p_file_no = (int *)&g_sysconf.M_MAX_FILE_NO;
    int *p_page_size = (int *)&g_sysconf.M_PAGE_SIZE;
    int *p_ncpus = (int *)&g_sysconf.M_NCPUS;

    // MAX_FILE_NO
    errno = 0;
    rslt = getrlimit(RLIMIT_NOFILE, &rlmt);
    tmp_err = (-1 == rslt) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] getrlimit() failed: %d\n", tmp_err);

        return HIXO_ERROR;
    }
    *p_file_no = rlmt.rlim_cur;

    // PAGE_SIZE
    *p_page_size = PAGE_SIZE;

    // NCPUS
    *p_ncpus = (int)sysconf(_SC_NPROCESSORS_ONLN);

    return HIXO_OK;
}

static void fall_into_daemon(void)
{
    int fd;

    if (0 != fork()) {
        exit(0);
    }
    // signal(SIGHUP, SIG_IGN);

    (void)setsid();

    if (0 != fork()) {
        exit(0);
    }
    // signal(SIGHUP, SIG_IGN);

    (void)umask(0);
    if (-1 == chdir("/")) {
        (void)fprintf(stderr, "[WARNING] chdir to / failed\n");
    }
    fd = open("/dev/null", O_RDWR);
    if (-1 != fd) {
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            (void)close(fd);
        }
    }

    return;
}

void sig_child_handler(int signo)
{
    g_status = HIXO_STATUS_CHILD;
}

void sig_int_handler(int signo)
{
    printf("sigint\n");
}

static int master_main(void)
{
    sigset_t old_mask;
    sigset_t filled_mask;
    sigset_t cmd_mask;
    struct sigaction sa;

    (void)sigemptyset(&old_mask);
    (void)sigfillset(&filled_mask);
    (void)sigfillset(&cmd_mask);

    (void)sigprocmask(SIG_SETMASK, &filled_mask, &old_mask);
    sa.sa_handler = &sig_child_handler;
    sa.sa_mask = filled_mask;
    sa.sa_flags = 0;
    if (-1 == sigaction(SIGCHLD, &sa, NULL)) {
        return HIXO_ERROR;
    }

    sa.sa_handler = &sig_int_handler;
    sa.sa_mask = filled_mask;
    sa.sa_flags = 0;
    if (-1 == sigaction(SIGINT, &sa, NULL)) {
        return HIXO_ERROR;
    }

    (void)sigdelset(&cmd_mask, SIGCHLD);
    (void)sigdelset(&cmd_mask, SIGQUIT);
    (void)sigdelset(&cmd_mask, SIGINT);
    (void)sigdelset(&cmd_mask, SIGHUP);

    while (g_child_count) {
        (void)sigsuspend(&cmd_mask);
        switch (g_status) {
        case HIXO_STATUS_RUNNING:
            break;
        case HIXO_STATUS_CHILD:
            {
                pid_t cpid;
                int status;

                cpid = waitpid(-1, &status, WNOHANG);
                if (!WIFEXITED(status)) {
                    (void)fprintf(stderr,
                                  "[WARNING] worker process %d "
                                      "exit unnormally\n",
                                  cpid);
                }
                --g_child_count;
                break;
            }
        case HIXO_STATUS_QUIT:
            {
            }
        default:
            assert(0);
            break;
        }
    }
    (void)sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return HIXO_OK;
}

static int worker_main_core(void)
{
    int rslt;

#define INIT_WORKER(module_type)    \
    rslt = HIXO_OK;\
    for (int i = 0; NULL != gap_modules[i]; ++i) {\
        if (module_type != gap_modules[i]->m_type) {\
            continue;\
        }\
        if (NULL == gap_modules[i]->mpf_init_worker) {\
            continue;\
        }\
        rslt = (*gap_modules[i]->mpf_init_worker)();\
    }

    INIT_WORKER(HIXO_MODULE_CORE);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_CORE;
    }

    INIT_WORKER(HIXO_MODULE_EVENT);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_EVENT;
    }

    INIT_WORKER(HIXO_MODULE_APP);
    if (HIXO_ERROR == rslt) {
        goto ERR_INIT_WORKER_APP;
    }

#undef INIT_WORKER

    for (int i = 0; NULL != gap_modules[i]; ++i) {
        hixo_core_module_ctx_t *p_ctx = NULL;

        if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
            continue;
        }

        p_ctx = (hixo_core_module_ctx_t *)gap_modules[i]->mp_ctx;
        if (NULL == p_ctx->mpf_runloop) {
            continue;
        }
        rslt = (*p_ctx->mpf_runloop)();
        if (-1 == rslt) {
            break;
        }
    }

    do {
#define EXIT_WORKER(module_type)    \
        for (int i = 0; NULL != gap_modules[i]; ++i) {\
            if (module_type != gap_modules[i]->m_type) {\
                continue;\
            }\
            if (NULL == gap_modules[i]->mpf_exit_master) {\
                continue;\
            }\
            (*gap_modules[i]->mpf_exit_master)();\
        }

        EXIT_WORKER(HIXO_MODULE_APP);
ERR_INIT_WORKER_APP:
        EXIT_WORKER(HIXO_MODULE_EVENT);
ERR_INIT_WORKER_EVENT:
        EXIT_WORKER(HIXO_MODULE_CORE);
ERR_INIT_WORKER_CORE:
        break;

#undef EXIT_WORKER
    } while (0);

    return rslt;
}

void sig_int_handler_worker(int signo)
{
}

void sig_alarm_handler_worker(int signo)
{
}

static int worker_main(int cpu_id)
{
    int rslt;
    cpu_set_t cpuset;

    // 绑定CPU
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    if (-1 == sched_setaffinity(0, sizeof(cpu_set_t), &cpuset)) {
        (void)fprintf(stderr, "[WARNING] sched_setaffinity failed\n");
    }

    while (TRUE) {
        rslt = worker_main_core();
    }

    return rslt;
}

pid_t hixo_spawn_workers(intptr_t n_workers)
{
    pid_t cpid = -1; // 子进程id
    sigset_t filled_mask;
    sigset_t old_mask;

    // 屏蔽所有信号
    (void)sigfillset(&filled_mask);
    (void)sigprocmask(SIG_SETMASK, &filled_mask, &old_mask);

    for (intptr_t i = 0; i < n_workers; ++i) {
        cpid = fork();

        if (-1 == cpid) {
            (void)fprintf(stderr, "[ERROR] fork() failed\n");
            break;
        }

        if (cpid > 0) {
            ga_pss_info[i].m_cpuid = i % g_sysconf.M_NCPUS;
            ga_pss_info[i].m_pid = cpid;
            gp_ps_info = &ga_pss_info[i];
            break;
        }
    }

    // 恢复master进程信号屏蔽字
    if (0 == cpid) {
        (void)sigprocmask(SIG_SETMASK, &old_mask, NULL);
    }

    return cpid;
}

int hixo_main(void)
{
    int rslt = HIXO_OK;
    hixo_conf_t *p_conf;
    intptr_t i = 0;
    pid_t cpid;

    for (i = 0; NULL != gap_modules[i]; ++i) {
        if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
            continue;
        }

        if (NULL == gap_modules[i]->mpf_init_master) {
            continue;
        }

        rslt = (*gap_modules[i]->mpf_init_master)();
        if (HIXO_ERROR == rslt) {
            break;
        }
    }
    if (HIXO_ERROR == rslt) {
        goto EXIT;
    }

    // 获取配置
    p_conf = g_rt_ctx.mp_conf;

    // 守护进程
    if (p_conf->m_daemon) {
        fall_into_daemon();
    }

    // 分裂进程
    assert(p_conf->m_worker_processes < HIXO_MAX_PSS);
    cpid = hixo_spawn_workers(p_conf->m_worker_processes);
    if (-1 == cpid) {
        goto EXIT;
    } else if (0 == cpid) {
        assert(NULL == gp_ps_info);

        rslt = master_main();
    } else {
        gp_ps_info->m_power = p_conf->m_max_connections
                                  - (p_conf->m_max_connections / 8);
        rslt = worker_main(gp_ps_info->m_cpuid);
    }

EXIT:
    for (i -= 1; i >= 0; --i) {
        if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
            continue;
        }
        if (NULL == gap_modules[i]->mpf_exit_master) {
            continue;
        }
        (*gap_modules[i]->mpf_exit_master)();
    }
    return rslt;
}

#define RUN 0
#if 1 == RUN
int main(int argc, char *argv[])
{
    int rslt;

    if (HIXO_ERROR == hixo_get_sysconf()) {
        rslt = HIXO_ERROR;
        goto ERR_SYSCONF;
    }

    rslt = (HIXO_OK == hixo_main()) ? EXIT_SUCCESS : EXIT_FAILURE;

ERR_SYSCONF:
    return rslt;
}
#elif 2 == RUN
int main(int argc, char *argv[])
{
    int rslt;

    // init framework

    // load application
    for (int i = 0; NULL != gap_modules[i]; ++i) {
    }

    // framework runloop

    // unload application
    for (int i = 0; NULL != gap_modules[i]; ++i) {
    }

    // eixt framework

    return rslt;
}
#elif 3 == RUN
static void test_pool_interface(void *obj, intptr_t offset)
{
    hixo_call_pool_new(obj, offset, ITFC_INDEX_MEMPOOL_POOL, 0, 0);
    hixo_call_pool_del(obj, offset, ITFC_INDEX_MEMPOOL_POOL);
}

int main(int argc, char *argv[])
{
    hixo_mempool_t mempool;

    mempool_init(&mempool);
    test_pool_interface(&mempool, VFTS_OFFSET(hixo_mempool_t));
    mempool_exit(&mempool);

    printf("%ld\n", count_places(1));
    printf("%ld\n", count_places(12));
    printf("%ld\n", count_places(123));
    printf("%ld\n", count_places(1234));
    printf("%ld\n", count_places(12345));
    printf("%ld\n", count_places(-1234567890));
    printf("%ld\n", count_places(9223372036854775807));
    printf("%ld\n", count_places(-9223372036854775807));
    printf("=================================\n");

    {
        intptr_t xs = INTEGER_MAX;
        intptr_t ys = INTEGER_MIN;
        intptr_t zs = 0;
        ssize_t len = count_places(xs);

        char *buf = (char *)malloc(len + 1);
        printf("%s\n", wtoa(xs, len, buf, len + 1));
        printf("%s\n", wtoa(ys, len, buf, len + 1));
        printf("%s\n", wtoa(zs, len, buf, len + 1));
        free(buf);
    }

    return 0;
}

#elif 4 == RUN

// mem_shift_left test
int main(int argc, char *argv[])
{
    intptr_t x[4] = {1 ,0, 0, 0};

    for (intptr_t i = 1; i < 150; ++i) {
        mem_shift_left(&x, sizeof(x) - 1, i);
        for (intptr_t i = 0; i < ARRAY_COUNT(x); ++i) {
            fprintf(stderr, "%d,", x[i]);
        }
        fprintf(stderr, "\n", x[i]);
        x[0] = 1;
        x[1] = 0;
        x[2] = 0;
        x[3] = 0;
    }

    return 0;
}

#else

int main(int argc, char *argv[])
{
#define MEMSIZE 4096 * 3
    void *p;
    struct mydata_t {
        int a;
        int b;
        int c;
    } *dt;

    p = malloc(MEMSIZE);
    slub_format(p, MEMSIZE);
    dt = slub_alloc(p, sizeof(struct mydata_t));
    fprintf(stderr, "dt: %p\n", dt);
    dt->a = 97;
    dt->b = 98;
    dt->c = 99;
    dump_slub(p);
    free(p);

    return 0;
#undef MEMSIZE
}

#endif
