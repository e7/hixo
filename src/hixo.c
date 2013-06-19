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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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
    byte_t *mp_data;
    uint32_t m_size;
} bitmap_t;

int create_bitmap(bitmap_t *p_bm, uint32_t nbits)
{
    if (0 == nbits) {
        return HIXO_ERROR;
    }

    p_bm->m_size = nbits / sizeof(byte_t) + 1;
    p_bm->mp_data = calloc(1, p_bm->m_size);
    if (NULL == p_bm->mp_data) {
        return HIXO_ERROR;
    }
    (void)memset(p_bm->mp_data, 0, p_bm->m_size);
}

int bitmap_set(bitmap_t *p_bm, uint32_t bit_offset)
{
    uint32_t byte_offset = bit_offset / sizeof(byte_t);
    uint32_t byte_bit_offset = bit_offset % sizeof(byte_t);

    if (bit_offset > (sizeof(byte_t) * p_bm->m_size - 1)) {
        fprintf(stderr, "[ERROR] bitmap bit_offset out of range\n");

        return HIXO_ERROR;
    }
    
    p_bm->mp_data[byte_offset] &= 1 << (sizeof(byte_t) - byte_bit_offset);

    return HIXO_OK;
}

int bitmap_is_set(bitmap_t *p_bm, uint32_t bit_offset)
{
    uint32_t byte_offset = bit_offset / sizeof(byte_t);
    uint32_t byte_bit_offset = bit_offset % sizeof(byte_t);

    if (bit_offset > (sizeof(byte_t) * p_bm->m_size - 1)) {
        fprintf(stderr, "[ERROR] bitmap bit_offset out of range\n");

        return HIXO_ERROR;
    }

    if (p_bm->mp_data[byte_offset]
            & (1 << (sizeof(byte_t) - byte_bit_offset)))
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

typedef struct s_hixo_connection_t hixo_connection_t;
struct s_hixo_connection_t {
    void (*mpf_read)(hixo_connection_t *);
    void (*mpf_write)(hixo_connection_t *);

    int m_fd;
    int m_shut_read;
    list_t m_shut_read_node;
    int m_shut_write;
    list_t m_shut_write_node;
};


// hixo_listen_t {{
typedef enum {
    CLOSED = 0,
    OPENED = CLOSED + 1,
    CONFIGURED = OPENED + 1,
    BOUND = CONFIGURED + 1,
    LISTENING = BOUND + 1,
} listening_status_t;

typedef struct s_hixo_listening_t hixo_listening_t;
struct s_hixo_listening_t {
    int m_fd;
    listening_status_t m_status;
    list_t *mp_conn_list;
};

hixo_listening_t ga_hixo_listenings[SRV_ADDRS_COUNT] = {
    {0, CLOSED, NULL},
};

int hixo_init_listenings(void)
{
    int tmp_err = 0;
    int arrived_count = 0;
    struct sockaddr_in srv_addr;

    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        int sock_fd = 0;

        errno = 0;
        sock_fd = socket(PF_INET, SOCK_STREAM, 0);
        tmp_err = (-1 == sock_fd) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_fd = sock_fd;
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
        ret = unblocking_fd(ga_hixo_listenings[i].m_fd);
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_status = CONFIGURED;
        } else {
            (void)close(ga_hixo_listenings[i].m_fd);
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
        ret = bind(ga_hixo_listenings[i].m_fd,
                   (struct sockaddr *)&srv_addr,
                   sizeof(srv_addr));
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_status = BOUND;
        } else {
            (void)close(ga_hixo_listenings[i].m_fd);
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
        ret = listen(ga_hixo_listenings[i].m_fd,
                     (SRV_ADDRS[i].m_backlog > 0)
                         ? SRV_ADDRS[i].m_backlog
                         : SOMAXCONN);
        tmp_err = (-1 == ret) ? errno : 0;
        if (ESUCCESS == tmp_err) {
            ga_hixo_listenings[i].m_status = LISTENING;
            ++arrived_count;
        } else {
            (void)close(ga_hixo_listenings[i].m_fd);
            ga_hixo_listenings[i].m_status = CLOSED;
        }
    }

    return arrived_count ? HIXO_OK : HIXO_ERROR;
}

void hixo_uninit_listenings(void)
{
    for (int i = 0; i < SRV_ADDRS_COUNT; ++i) {
        if (ga_hixo_listenings[i].m_status > CLOSED) {
            (void)close(ga_hixo_listenings[i].m_fd);
            ga_hixo_listenings[i].m_status = CLOSED;
        }
    }
}
// }} hixo_listen_t


typedef struct {
    int (*mpf_init)(void);
    int (*mpf_add_event)(hixo_connection_t *);
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
static int epoll_add_event(hixo_connection_t *p_ev);
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
    int tmp_err = 0;

    #define epev_size sizeof(struct epoll_event)
    s_epoll_module_ctx.mp_misc = calloc(MAX_CONNECTIONS, epev_size);
    #undef epev_size
    if (NULL == s_epoll_module_ctx.mp_misc) {
        fprintf(stderr, "[ERROR] out of memory\n");

        return HIXO_ERROR;
    }

    errno = 0;
    s_epoll_module_ctx.m_fd = epoll_create(MAX_CONNECTIONS);
    tmp_err = (-1 == s_epoll_module_ctx.m_fd) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] epoll_create failed: %d\n", tmp_err);
        return HIXO_ERROR;
    } else {
        return HIXO_OK;
    }
}

int epoll_add_event(hixo_connection_t *p_ev)
{
    return HIXO_OK;
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
    struct epoll_event *p_evs = NULL;

    p_evs = (struct epoll_event *)s_epoll_module_ctx.mp_misc;

    errno = 0;
    nevents = epoll_wait(s_epoll_module_ctx.m_fd,
                         p_evs,
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
        hixo_connection_t *p_conn = (hixo_connection_t *)p_evs[i].data.ptr;

        if (p_evs[i].events & (EPOLLERR | EPOLLHUP)) {
            p_conn->m_shut_read = TRUE;
            p_conn->m_shut_write = TRUE;
            add_node(&s_epoll_module_ctx.mp_shut_read_list,
                     &p_conn->m_shut_read_node);
            add_node(&s_epoll_module_ctx.mp_shut_write_list,
                     &p_conn->m_shut_write_node);
        }

        if ((p_evs[i].events & EPOLLIN) && (!p_conn->m_shut_read)) {
            (*p_conn->mpf_read)(p_conn);
        }

        if ((p_evs[i].events & EPOLLOUT) && (!p_conn->m_shut_write)) {
            (*p_conn->mpf_read)(p_conn);
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


static struct {
    int m_fd;
    int m_valid;
} s_lsn_sockets[ARRAY_COUNT(SRV_ADDRS)] = {};
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
    struct rlimit rlmt;
    int tmp_err = 0;
    int *p_file_no = (int *)&g_sysconf.MAX_FILE_NO;

    errno = 0;
    rslt = getrlimit(RLIMIT_NOFILE, &rlmt);
    tmp_err = (-1 == rslt) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] getrlimit() failed: %d\n", tmp_err);

        return HIXO_ERROR;
    }
    *p_file_no = rlmt.rlim_cur;

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

int main(int argc, char *argv[])
{
    int rslt = EXIT_FAILURE;

    if (HIXO_ERROR == hixo_init_listenings()) {
        return EXIT_FAILURE;
    }

    rslt = (HIXO_ERROR == hixo_main()) ? EXIT_SUCCESS : EXIT_FAILURE;

    if (g_master) {
        hixo_uninit_listenings();
    }

    return rslt;
}
