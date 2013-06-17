#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"


// config {{
#define DAEMON                  FALSE
#define SRV_PORT                8888
#define WORKER_PROCESSES        4
#define MAX_CONNECTIONS         64
// }} config

#define HIXO_OK             (0)
#define HIXO_ERROR          (-1)
#define unblocking_fd(fd)   fcntl(fd, \
                                  F_SETFL, \
                                  fcntl(fd, F_GETFL) | O_NONBLOCK)

typedef struct {
    void (*mpf_handler)(void *);

} hixo_event_t;

typedef enum {
    HIXO_CORE,
    HIXO_EVENT,
} hixo_module_type_t;

typedef struct {
    int m_fd;
    int (*mpf_init)(void);
    int (*mpf_add_event)(hixo_event_t *);
    int (*mpf_mod_event)(void);
    int (*mpf_del_event)(void);
    int (*mpf_process_events)(void);
    void (*mpf_uninit)(void);
    void *mp_misc;
} hixo_event_module_ctx_t;

typedef struct {
    hixo_module_type_t m_type;
    void *mp_ctx;
} hixo_module_t;

// epoll模块
static int epoll_init(void);
static int epoll_add_event(hixo_event_t *p_ev);
static int epoll_mod_event(void);
static int epoll_del_event(void);
static int epoll_process_events(void);
static void epoll_uninit(void);

static hixo_event_module_ctx_t s_epoll_module_ctx = {
    -1,
    &epoll_init,
    &epoll_add_event,
    &epoll_mod_event,
    &epoll_del_event,
    &epoll_process_events,
    &epoll_uninit,
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

int epoll_add_event(hixo_event_t *p_ev)
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
    int ready = 0;
    int tmp_err = 0;
    struct epoll_event *p_evs = NULL;

    p_evs = (struct epoll_event *)s_epoll_module_ctx.mp_misc;

    errno = 0;
    ready = epoll_wait(s_epoll_module_ctx.m_fd, p_evs, MAX_CONNECTIONS, -1);
    tmp_err = (-1 == ready) ? errno : 0;
    if (tmp_err) {
        if (EINTR == tmp_err) {
            return HIXO_OK; 
        } else {
            fprintf(stderr, "[ERROR] epoll_wait failed: %d\n", tmp_err);

            return HIXO_ERROR;
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


int g_master = TRUE;

static int master_main(void)
{
    return HIXO_OK;
}

static int worker_main(int sock)
{
    int rslt = 0;
    
    for (int i = 0; i < sizeof(g_modules); ++i) {
        hixo_event_module_ctx_t const *pc_ev_ctx = NULL;

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
    }

    return rslt;
}

static int hixo_main(int sock)
{
    if (DAEMON) {
    }

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
        return master_main();
    } else {
        return worker_main(sock);
    }
}

int main(int argc, char *argv[])
{
    int rslt = EXIT_FAILURE;
    int lsn_fd = 0;
    int tmp_err = 0;
    struct sockaddr_in srv_addr;

    // 创建套接字
    errno = 0;
    lsn_fd = socket(PF_INET, SOCK_STREAM, 0);
    tmp_err = (-1 == lsn_fd) ? errno : 0;
    if (tmp_err) {
        fprintf(stderr, "[ERROR] socket() failed: %d\n", tmp_err);

        goto ERR_SOCKET;
    }

    // 非阻塞
    errno = 0;
    if (-1 == unblocking_fd(lsn_fd)) {
        tmp_err = errno;
        fprintf(stderr, "[ERROR] fcntl() failed: %d\n", tmp_err);

        goto ERR_FCNTL;
    }

    // 绑定
    (void)memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port = htons(SRV_PORT);
    errno = 0;
    if (-1 == bind(lsn_fd,
                   (struct sockaddr *)&srv_addr,
                   sizeof(srv_addr)))
    {
        tmp_err = errno;
        fprintf(stderr, "[ERROR] bind() failed: %d\n", tmp_err);

        goto ERR_BIND;
    }

    // 监听
    errno = 0;
    if (-1 == listen(lsn_fd, SOMAXCONN)) {
        tmp_err = errno;
        fprintf(stderr, "[ERROR] listen failed: %d\n", tmp_err);

        goto ERR_LISTEN;
    }

    rslt = (0 == hixo_main(lsn_fd)) ? EXIT_SUCCESS : EXIT_FAILURE;

    // 错误处理
ERR_LISTEN:

ERR_BIND:

ERR_FCNTL:
    (void)close(lsn_fd);

ERR_SOCKET:

    return rslt;
}
