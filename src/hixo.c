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


#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "list.h"


typedef struct {
    uint32_t m_ip;
    uint16_t m_port;
    int m_backlog;
} addr_t;


// config {{
#define DAEMON                  FALSE
#define WORKER_PROCESSES        4
#define MAX_CONNECTIONS         64
#define CONNECTION_TIME_OUT     60
addr_t const SRV_ADDRS[] = {
    {INADDR_ANY, 80,   0},
    {INADDR_ANY, 8888, 0},
    {INADDR_ANY, 8889, 0},
    {INADDR_ANY, 8890, 0},
};
#define SRV_ADDRS_COUNT         ARRAY_COUNT(SRV_ADDRS)
// }} config

// sysconf {{
struct {
    int const MAX_FILE_NO;
} g_sysconf;
// }} sysconf


#ifndef ESUCCESS
    #define ESUCCESS        (0)
#endif
#define HIXO_OK             (0)
#define HIXO_ERROR          (-1)
#define unblocking_fd(fd)   fcntl(fd, \
                                  F_SETFL, \
                                  fcntl(fd, F_GETFL) | O_NONBLOCK)

// bitmap {{
typedef struct {
    uint8_t *mp_data;
    uint32_t m_size;
} bitmap_t;

int create_bitmap(bitmap_t *p_bm, uint32_t nbits)
{
    if (0 == nbits) {
        return HIXO_ERROR;
    }

    p_bm->m_size = (nbits - 1) / 8 + 1;
    p_bm->mp_data = (uint8_t *)calloc(1, p_bm->m_size);
    if (NULL == p_bm->mp_data) {
        return HIXO_ERROR;
    }
    (void)memset(p_bm->mp_data, 0, p_bm->m_size);

    return HIXO_OK;
}

int bitmap_set(bitmap_t *p_bm, uint32_t bit_offset)
{
    uint32_t byte_offset = bit_offset / 8;
    uint32_t byte_bit_offset = bit_offset % 8;

    if (bit_offset > (8 * p_bm->m_size - 1)) {
        fprintf(stderr, "[ERROR] bitmap bit_offset out of range\n");

        return HIXO_ERROR;
    }

    p_bm->mp_data[byte_offset] |= 1 << byte_bit_offset;

    return HIXO_OK;
}

int bitmap_is_set(bitmap_t *p_bm, uint32_t bit_offset)
{
    uint32_t byte_offset = bit_offset / 8;
    uint32_t byte_bit_offset = bit_offset % 8;

    if (bit_offset > (8 * p_bm->m_size - 1)) {
        fprintf(stderr, "[ERROR] bitmap bit_offset out of range\n");

        return FALSE;
    }

    if (p_bm->mp_data[byte_offset] & (1 << byte_bit_offset))
    {
        return TRUE;
    } else {
        return FALSE;
    }

}

void destroy_bitmap(bitmap_t *p_bm)
{
    if (NULL != p_bm->mp_data) {
        free(p_bm->mp_data);
        p_bm->mp_data = NULL;
    }
    p_bm->m_size = 0;
}
// }} bitmap


typedef enum {
    HIXO_CORE,
    HIXO_EVENT,
} hixo_module_type_t;

typedef struct s_hixo_event_t hixo_event_t;
struct s_hixo_event_t {
    int m_fd;
    uint32_t m_ev_flags;
    int m_overdue;
    int (*mpf_read)(hixo_event_t *);
    int (*mpf_write)(hixo_event_t *);
};


// hixo_listen_t {{
typedef enum {
    CLOSED = 0x00000001,
    OPENED = (CLOSED << 1) + 1,
    CONFIGURED = OPENED + 1,
    BOUND = CONFIGURED + 1,
    LISTENING = BOUND + 1,
    CONNECTED = (CLOSED << 2) + 1,
    BROKEN = (CLOSED << 3) + 1,
} socket_status_t;

typedef struct s_hixo_socket_t hixo_socket_t;
struct s_hixo_socket_t {
    socket_status_t m_status;
    hixo_event_t m_event;
};

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


typedef struct {
    int (*mpf_init)(void);
    void (*mpf_add_event)(hixo_event_t *);
    int (*mpf_mod_event)(void);
    int (*mpf_del_event)(void);
    int (*mpf_process_events)(void);
    void (*mpf_uninit)(void);

    int m_fd;
    list_t *mp_shut_read_list;
    list_t *mp_shut_write_list;
    void *mp_misc;
} hixo_event_module_ctx_t;

typedef struct {
    hixo_module_type_t m_type;
    void *mp_ctx;
} hixo_module_t;

// epoll模块
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

hixo_module_t g_epoll_module = {
    HIXO_EVENT,
    &s_epoll_module_ctx,
};


// 模块数组
hixo_module_t *g_modules[] = {
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

    for (int i = 0; i < ARRAY_COUNT(g_modules); ++i) {
        if (HIXO_EVENT != g_modules[i]->m_type) {
            continue;
        }

        pc_ev_ctx = (hixo_event_module_ctx_t *)g_modules[i]->mp_ctx;
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
}
