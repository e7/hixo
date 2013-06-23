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


#include "list.h"
#include "bitmap.h"
#include "event_module.h"


// sysconf {{
struct {
    int const MAX_FILE_NO;
} g_sysconf;
// }} sysconf

// hixo_listen_t {{
hixo_socket_t ga_hixo_listenings[SRV_ADDRS_COUNT] = {};

static int hixo_handle_accept(hixo_event_t *p_ev)
{
    return HIXO_OK;
}

int hixo_init_listenings(void)
{
    int tmp_err = 0;
    int arrived_count = 0;
    struct sockaddr_in srv_addr;

    // 初始化监听套接字事件体
    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        ga_hixo_listenings[i].m_event.m_ev_flags = EPOLLET | EPOLLIN;
        ga_hixo_listenings[i].m_event.m_overdue = FALSE;
        ga_hixo_listenings[i].m_event.mpf_read = &hixo_handle_accept;
        ga_hixo_listenings[i].m_event.mpf_write = NULL;
    }

    // 初始化监听套接字
    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        int sock_fd = 0;

        errno = 0;
        sock_fd = socket(PF_INET, SOCK_STREAM, 0);
        tmp_err = (-1 == sock_fd) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_event.m_fd = sock_fd;
            ga_hixo_listenings[i].m_status = OPENED;
        } else {
            ga_hixo_listenings[i].m_status = CLOSED;
        }
    }

    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        int ret = 0;

        if (OPENED != ga_hixo_listenings[i].m_status) {
            continue;
        }

        errno = 0;
        ret = unblocking_fd(ga_hixo_listenings[i].m_event.m_fd);
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_status = CONFIGURED;
        } else {
            (void)close(ga_hixo_listenings[i].m_event.m_fd);
            ga_hixo_listenings[i].m_status = CLOSED;
        }
    }

    (void)memset(&srv_addr, 0, sizeof(srv_addr));
    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        int ret = 0;

        if (CONFIGURED != ga_hixo_listenings[i].m_status) {
            continue;
        }

        srv_addr.sin_family = AF_INET;
        srv_addr.sin_addr.s_addr = htonl(SRV_ADDRS[i].m_ip);
        srv_addr.sin_port = htons(SRV_ADDRS[i].m_port);

        errno = 0;
        ret = bind(ga_hixo_listenings[i].m_event.m_fd,
                   (struct sockaddr *)&srv_addr,
                   sizeof(srv_addr));
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_status = BOUND;
        } else {
            fprintf(stderr, "[ERROR] bind() failed: %d\n", tmp_err);

            (void)close(ga_hixo_listenings[i].m_event.m_fd);
            ga_hixo_listenings[i].m_status = CLOSED;
        }
    }

    arrived_count = 0;
    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        int ret = 0;

        if (BOUND != ga_hixo_listenings[i].m_status) {
            continue;
        }

        errno = 0;
        ret = listen(ga_hixo_listenings[i].m_event.m_fd,
                     (SRV_ADDRS[i].m_backlog > 0)
                         ? SRV_ADDRS[i].m_backlog
                         : SOMAXCONN);
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_status = LISTENING;
            ++arrived_count;
        } else {
            (void)close(ga_hixo_listenings[i].m_event.m_fd);
            ga_hixo_listenings[i].m_status = CLOSED;
        }
    }

    return arrived_count ? HIXO_OK : HIXO_ERROR;
}

void hixo_connection_handler(void)
{
}

void hixo_uninit_listenings(void)
{
    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        if (ga_hixo_listenings[i].m_status > CLOSED) {
            (void)close(ga_hixo_listenings[i].m_event.m_fd);
            ga_hixo_listenings[i].m_status = CLOSED;
        }
    }
}
// }} hixo_listen_t


// 模块数组
hixo_module_t *gap_modules[] = {
    &g_main_core_module,
    &g_epoll_module,
};


bitmap_t g_lsn_sockets_bm = {NULL, 0};
int g_master = TRUE;

static int master_main(void)
{
    sleep(-1);

    return HIXO_OK;
}

static int worker_main(void)
{
    int rslt = 0;
    hixo_event_module_ctx_t const *pc_ev_ctx = NULL;

    for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
        if (HIXO_MODULE_EVENT != gap_modules[i]->m_type) {
            continue;
        }

        pc_ev_ctx = (hixo_event_module_ctx_t *)gap_modules[i]->mp_ctx;
        rslt = (*pc_ev_ctx->mpf_init)();
        if (-1 == rslt) {
            break;
        }
    }


    while (TRUE) {
        rslt = (*pc_ev_ctx->mpf_process_events)();

        if (-1 == rslt) {
            break;
        }
    }

    return rslt;
}

static int hixo_main(void)
{
    int rslt = 0;

    if (DAEMON) {
    }

    // 分裂进程
    for (int i = 0; i < WORKER_PROCESSES; ++i) {
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
#if 0
    int rslt = EXIT_FAILURE;

    if (HIXO_ERROR == hixo_init_listenings()) {
        goto ERR_INIT_LISTENINGS;
    }

    if (HIXO_ERROR == hixo_init_sysconf()) {
        goto ERR_INIT_SYSCONF;
    }

    if (HIXO_ERROR == create_bitmap(&g_lsn_sockets_bm,
                                    g_sysconf.MAX_FILE_NO))
    {
        goto ERR_CREATE_BITMAP;
    }
    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        if (LISTENING == ga_hixo_listenings[i].m_status) {
            assert(HIXO_OK == bitmap_set(&g_lsn_sockets_bm,
                                         ga_hixo_listenings[i].m_event.m_fd));
        }
    }

    rslt = (HIXO_ERROR == hixo_main()) ? EXIT_SUCCESS : EXIT_FAILURE;

ERR_CREATE_BITMAP:
    if (g_master) {
        destroy_bitmap(&g_lsn_sockets_bm);
    }
ERR_INIT_SYSCONF:
ERR_INIT_LISTENINGS:
    if (g_master) {
        hixo_uninit_listenings();
    }

    return rslt;

#else

    int rslt = EXIT_FAILURE;
    int core_module_max = 0;
    int fatal_err = FALSE;

    if (-1 == hixo_init_sysconf()) {
        goto ERR_INIT_SYSCONF;
    }

    for (int i = 0; i < ARRAY_COUNT(gap_modules); ++i) {
        core_module_max = i;

        if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
            continue;
        }

        if (HIXO_ERROR == (*gap_modules[i]->mpf_init_master)()) {
            fatal_err = TRUE;

            break;
        }
    }

    if (fatal_err) {
        goto ERR_INIT_MASTER;
    }

ERR_INIT_MASTER:
    if (g_master) {
        for (int i = 0; i < core_module_max; ++i) {
            if (HIXO_MODULE_CORE != gap_modules[i]->m_type) {
                continue;
            }
            
            (*gap_modules[i]->mpf_exit_master)();
        }
    }
ERR_INIT_SYSCONF:

    return rslt;
#endif
}
